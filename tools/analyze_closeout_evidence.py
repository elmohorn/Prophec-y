#!/usr/bin/env python3
"""Describe available hardware demos without inventing cutoff raw values or gates.

Spectral descriptors describe recorded output, not filter cutoff or isolated EG.
No DSP parameter fitting, automatic patch identification or candidate selection.
"""
import argparse,csv,hashlib,json
from pathlib import Path
import numpy as np
from scipy.signal import stft
from compare_online_suite import decode,SR
from inspect_reference_media import inspect

def descriptors(x):
    f,t,z=stft(x,fs=SR,nperseg=4096,noverlap=3616,axis=0,boundary=None,padded=False)
    p=np.mean(abs(z)**2,axis=1)
    band=(f>=40)&(f<=12000); fb=f[band]; pb=p[band]
    power=pb.sum(axis=0); active=power>max(float(power.max())*1e-4,1e-24)
    norm=pb/np.maximum(power,1e-30)
    centroid=(fb[:,None]*norm).sum(axis=0)
    cumulative=np.cumsum(norm,axis=0)
    roll=fb[np.minimum(np.argmax(cumulative>=.95,axis=0),len(fb)-1)]
    high=p[(f>=3000)&(f<=12000)].sum(axis=0)/np.maximum(power,1e-30)
    low=p[(f>=40)&(f<500)].sum(axis=0)/np.maximum(power,1e-30)
    peak=np.argmax(pb,axis=0); jj=np.arange(len(t)); log=np.log(np.maximum(pb,1e-30))
    left=np.maximum(peak-1,0);right=np.minimum(peak+1,len(fb)-1)
    den=log[left,jj]-2*log[peak,jj]+log[right,jj]
    delta=np.clip(.5*(log[left,jj]-log[right,jj])/np.where(abs(den)>1e-15,den,1),-.5,.5)
    tone=(peak+np.flatnonzero(band)[0]+delta)*SR/4096
    share=(pb[left,jj]+pb[peak,jj]+pb[right,jj])/np.maximum(power,1e-30)
    # A dominant line need not be oscillator F0 or self-oscillation frequency.
    strong=active&(share>=.85)&(peak>0)&(peak<len(fb)-1)
    rows=[dict(time_s=float(t[i]),active=bool(active[i]),centroid_hz=float(centroid[i]),
        rolloff95_hz=float(roll[i]),high_3k_12k_power_fraction=float(high[i]),low_40_500_power_fraction=float(low[i]),
        dominant_line_hz=float(tone[i]),dominant_three_bin_share=float(share[i])) for i in range(len(t))]
    result=dict(frame_count=len(t),active_frames=int(active.sum()),dominant_line_frames=int(strong.sum()))
    for name,values,mask in [('centroid_hz',centroid,active),('rolloff95_hz',roll,active),
        ('high_3k_12k_fraction',high,active),('low_40_500_fraction',low,active),('dominant_line_hz',tone,strong)]:
        result[name+'_p05_p50_p95']=[float(a) for a in np.percentile(values[mask],[5,50,95])] if mask.any() else None
    return rows,result

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('manifest',type=Path);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    manifest=json.loads(a.manifest.read_text());a.output.mkdir(parents=True,exist_ok=True);summaries=[]
    for source in manifest['sources']:
        path=Path(source['local_path']);sha=hashlib.sha256(path.read_bytes()).hexdigest()
        if sha!=source['sha256']:raise ValueError('changed source '+str(path))
        integrity=inspect(path);rows,summary=descriptors(decode(path))
        with (a.output/(source['id']+'_frames.csv')).open('w') as file:
            w=csv.DictWriter(file,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
        summaries.append(dict(id=source['id'],sha256=sha,integrity=integrity,**summary,
            known_raw_controls=source.get('known_raw_controls',False),known_note_offs=False,
            cutoff_hz_per_raw_identifiable=False,envelope_raw_to_seconds_identifiable=False))
    report=dict(sources=summaries,protocol=dict(sample_rate=SR,nfft=4096,hop=480,
        analysis_band_hz=[40,12000],active_relative_power_floor_db=-40,dominant_three_bin_share=.85),
        limitations=['Output descriptors are not cutoff estimates, ADSR times or instrument fidelity scores.',
        'Unknown input waveform, resonance, key tracking, gates and raw controls prevent absolute law identification.',
        'Dominant line may be a harmonic or a played note; do not convert it to a cutoff code.',
        'No fitted knob trajectory, model selection or DSP change.'])
    (a.output/'summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
