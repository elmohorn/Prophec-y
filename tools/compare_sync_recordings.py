#!/usr/bin/env python3
"""Exploratory hard-sync spectral fits with unknown capture controls.

Fit ratio on H1..8 only; report unused H9..16 separately. Per-window ratio
fitting is deliberately optimistic and never identifies the recorded controls.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import math
from pathlib import Path
import subprocess
import numpy as np
from analyze_sync_stages import power_harmonics, write_csv
from compare_online_suite import decode, SR


def error(candidate, reference, indices):
    mask = (reference >= -40) & np.isin(np.arange(1, len(reference)+1), indices)
    if not mask.any():
        return float('nan')
    return float(np.sqrt(np.mean((np.maximum(candidate[mask], -60)-np.maximum(reference[mask], -60))**2)))


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--probe',type=Path,default=Path('build-check/p6_sync_probe'))
    ap.add_argument('--protocol',type=Path,default=Path('calibration/v019_sync_comparison/protocol.json'))
    ap.add_argument('--audio',type=Path,default=Path('reference_private/sync_search/OB6-P6-Vergleich-Sync-Sweep-OSC1_P6_right.wav'))
    ap.add_argument('--output',type=Path,default=Path('reference_private/sync_comparison'))
    a=ap.parse_args();p=json.loads(a.protocol.read_text());a.output.mkdir(exist_ok=True,parents=True);(a.output/'renders').mkdir(exist_ok=True)
    source=decode(a.audio);refs=[];quality=[]
    for start in p['windows_s']:
        x=source[round(start*SR):round((start+p['duration_s'])*SR)]
        m,h=power_harmonics(x,p['expected_f0_hz'],16)
        m1,_=power_harmonics(x[:len(x)//2],p['expected_f0_hz'],16)
        m2,_=power_harmonics(x[len(x)//2:],p['expected_f0_hz'],16)
        if m['f0_search_edge']:
            raise ValueError(f'F0 search edge at {start}; review before rendering')
        refs.append(h-h[0]);quality.append(dict(start_s=start,f0_hz=m['power_f0_hz'],half_window_drift_cents=1200*math.log2(m2['power_f0_hz']/m1['power_f0_hz']),rms=float(np.sqrt(np.mean(x*x)))))
    write_csv(a.output/'reference_windows.csv',quality)
    f0=float(np.median([x['f0_hz'] for x in quality]));offset=12*math.log2(f0/(440*2**((51-69)/12)))
    # One common frequency prevents per-window pitch/timbre fitting. Each signal
    # is measured at its own F0 by the common harmonic analysis helper.
    specs=[(shape,phase,ratio) for shape in p['shapes'] for phase in p['reset_phases'] for ratio in p['ratios']]
    def render(spec):
        shape,phase,ratio=spec;name=f's{shape:g}_p{phase:g}_r{ratio:g}';wav=a.output/'renders'/(name+'.wav')
        cmd=[str(a.probe.resolve()),str(wav),'--note','51','--master-semis',str(offset),'--slave-semis',str(offset+12*math.log2(ratio)),'--shape1',str(shape),'--pw1',str(p['pulse_width']),'--reset-phase',str(phase),'--oversampling',str(p['oversampling']),'--seconds',str(p['duration_s']),'--preroll',str(p['preroll_s'])]
        result=subprocess.run(cmd,check=True,capture_output=True,text=True);wav.with_suffix('.log').write_text(result.stdout+result.stderr)
        m,h=power_harmonics(decode(wav),f0,16)
        return dict(name=name,shape=shape,reset_phase=phase,ratio=ratio,f0_hz=m['power_f0_hz'],wav=str(wav)),h-h[0]
    with ThreadPoolExecutor(max_workers=4) as pool:models=list(pool.map(render,specs))
    rows=[]
    for shape in p['shapes']:
        for phase in p['reset_phases']:
            group=[x for x in models if x[0]['shape']==shape and x[0]['reset_phase']==phase]
            for q,ref in zip(quality,refs):
                best=min(group,key=lambda x:error(x[1],ref,p['fit_harmonics']))
                info,h=best
                row=dict(start_s=q['start_s'],**info,fit_rmse_db=error(h,ref,p['fit_harmonics']),unused_high_rmse_db=error(h,ref,p['evaluation_harmonics']),all_rmse_db=error(h,ref,range(1,17)),ratio_grid_edge=info['ratio'] in [p['ratios'][0],p['ratios'][-1]])
                for k in range(16):row[f'reference_h{k+1}_dbc']=float(ref[k]);row[f'candidate_h{k+1}_dbc']=float(h[k])
                rows.append(row)
    write_csv(a.output/'window_comparisons.csv',rows)
    summary=[]
    for shape in p['shapes']:
        for phase in p['reset_phases']:
            subset=[x for x in rows if x['shape']==shape and x['reset_phase']==phase]
            summary.append(dict(shape=shape,reset_phase=phase,windows=len(subset),median_fit_rmse_db=float(np.nanmedian([x['fit_rmse_db'] for x in subset])),median_unused_high_rmse_db=float(np.nanmedian([x['unused_high_rmse_db'] for x in subset])),median_all_rmse_db=float(np.nanmedian([x['all_rmse_db'] for x in subset])),grid_edge_windows=sum(x['ratio_grid_edge'] for x in subset)))
    write_csv(a.output/'profile_summary.csv',summary)
    provenance=dict(source_sha256=hashlib.sha256(a.audio.read_bytes()).hexdigest(),protocol_sha256=hashlib.sha256(a.protocol.read_bytes()).hexdigest(),probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(),common_f0_hz=f0,renders=len(models),reference_windows=len(refs),production_changes=False)
    (a.output/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n');print(json.dumps(provenance));print(json.dumps(summary))


if __name__=='__main__':main()
