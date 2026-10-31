#!/usr/bin/env python3
"""Re-render a local online-reference corpus and report phase-aware metrics.

Requires NumPy, SciPy and ffmpeg. No third-party audio is redistributed.
All profiles use the SAME pre-roll grid. Report medians/ranges, never choose
each model's best phase. Metrics are diagnostics, not similarity percentages.
"""
from __future__ import annotations
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import numpy as np
from scipy.signal import stft, welch

SR = 48000

def decode(path: Path) -> np.ndarray:
    raw = subprocess.check_output(['ffmpeg','-v','error','-i',str(path),'-ac','2',
        '-ar',str(SR),'-f','f32le','-acodec','pcm_f32le','pipe:1'])
    x = np.frombuffer(raw,dtype='<f4').reshape(-1,2).astype(np.float64)
    if not len(x) or not np.isfinite(x).all():
        raise ValueError(f'empty/nonfinite audio: {path}')
    return x

def rms(x):
    return float(np.sqrt(np.mean(x*x)))

def envelope(x):
    hop=240 # fixed 5 ms; average channel power avoids antiphase cancellation
    n=len(x)//hop
    return np.sqrt(np.mean(x[:n*hop].reshape(n,hop,2)**2,axis=(1,2)))

def aligned_pair(ref,cand,max_lag_s=0.15):
    """Only bounded envelope alignment; no time stretch or waveform phase fit."""
    a,b=envelope(ref),envelope(cand)
    a=a/max(rms(a),1e-12); b=b/max(rms(b),1e-12)
    options=[]
    for lag in range(-round(max_lag_s/.005),round(max_lag_s/.005)+1):
        ia=max(0,-lag); ib=max(0,lag); n=min(len(a)-ia,len(b)-ib)
        if n<10: continue
        # Small penalty prevents arbitrary shifts for essentially flat envelopes.
        loss=float(np.mean((a[ia:ia+n]-b[ib:ib+n])**2))+abs(lag)*1e-6
        options.append((loss,abs(lag),lag))
    if not options: raise ValueError('audio too short for envelope alignment')
    lag=min(options)[2]; ia=max(0,-lag)*240; ib=max(0,lag)*240
    n=min(len(ref)-ia,len(cand)-ib)
    return ref[ia:ia+n],cand[ib:ib+n],lag*.005

def spectrum(x,nfft):
    # Independent L/R powers preserve the spectral energy of stereo voices.
    freqs,_,z=stft(x,fs=SR,window='hann',nperseg=nfft,noverlap=3*nfft//4,
                   axis=0,boundary=None,padded=False)
    power=np.mean(np.abs(z)**2,axis=1)
    return freqs,power

def harmonic_metrics(ref,cand,expected):
    def one(x):
        mono=x.mean(axis=1)
        f,p=welch(mono-mono.mean(),fs=SR,nperseg=min(16384,len(mono)))
        # Search only +/-50 cents around the expected physical master period.
        # Return strongest periodicity peak, not the loudest spectral partial.
        n=min(16384,len(mono));frame=(mono[:n]-mono[:n].mean())*np.hanning(n)
        ac=np.fft.irfft(abs(np.fft.rfft(frame,n=2*n))**2)[:n]
        lo=int(SR/(expected*2**(50/1200)));hi=int(SR/(expected/2**(50/1200)))+1
        k=lo+int(np.argmax(ac[lo:hi]));delta=0.
        if 0<k<n-1:
            den=ac[k-1]-2*ac[k]+ac[k+1]
            if abs(den)>1e-20: delta=float(np.clip(.5*(ac[k-1]-ac[k+1])/den,-.5,.5))
        f0=SR/(k+delta)
        harmonics=[]
        for h in range(1,17):
            hz=f0*h;mask=abs(f-hz)<=max(SR/16384,hz*(2**(35/1200)-1))
            harmonics.append(10*np.log10(max(float(p[mask].sum()),1e-24)))
        harmonics=np.array(harmonics);harmonics-=max(harmonics)
        return f0,harmonics
    fr,hr=one(ref);fc,hc=one(cand)
    return dict(reference_f0_hz=fr,candidate_f0_hz=fc,
        pitch_error_cents=1200*math.log2(fc/fr),
        harmonic_rmse_db=float(np.sqrt(np.mean((hr-hc)**2))),
        reference_h5_minus_h4_db=float(hr[4]-hr[3]),
        candidate_h5_minus_h4_db=float(hc[4]-hc[3]))

def compare(ref,cand,kind='performance',expected=None):
    original_ref_rms,original_cand_rms=rms(ref),rms(cand)
    if min(original_ref_rms,original_cand_rms)<1e-10:
        raise ValueError('silent reference/candidate cannot be calibrated')
    if kind=='performance': ref,cand,lag=aligned_pair(ref,cand)
    else:
        n=min(len(ref),len(cand));ref=ref[:n];cand=cand[:n];lag=0.
    gain=rms(ref)/max(rms(cand),1e-12);cand=cand*gain
    result=dict(alignment_lag_s=lag,comparison_duration_s=len(ref)/SR,
        candidate_gain_match_db=20*math.log10(gain),
        input_reference_rms=original_ref_rms,input_candidate_rms=original_cand_rms)
    log_errors=[];convergence=[]
    for fft in (512,2048,8192):
        f,a=spectrum(ref,fft);_,b=spectrum(cand,fft)
        mask=(f>=40)&(f<=12000);a=a[mask];b=b[mask]
        floor=max(float(a.max())*1e-6,1e-24)
        active=a>=floor
        la=10*np.log10(np.maximum(a,floor));lb=10*np.log10(np.maximum(b,floor))
        value=float(np.mean(abs(la[active]-lb[active])))
        result[f'log_spectral_mae_{fft}_db']=value;log_errors.append(value)
        convergence.append(float(np.linalg.norm(np.sqrt(a)-np.sqrt(b))/max(np.linalg.norm(np.sqrt(a)),1e-12)))
    result['mr_log_spectral_mae_db']=float(np.mean(log_errors))
    result['mr_spectral_convergence']=float(np.mean(convergence))
    # Broad log-frequency bands are less sensitive to small VCO detuning than
    # bin-wise distances. Keep this independent metric; neither may dominate.
    f,pa=spectrum(ref,8192);_,pb=spectrum(cand,8192)
    pa=pa.mean(axis=1);pb=pb.mean(axis=1)
    edges=np.geomspace(40,12000,33)
    bands=[]
    for p in (pa,pb):
        vals=[]
        for lo,hi in zip(edges[:-1],edges[1:]):
            use=(f>=lo)&(f<hi)
            vals.append(10*np.log10(max(float(p[use].mean()) if use.any() else 0,1e-18)))
        vals=np.array(vals);bands.append(vals-vals.mean())
    result['log_band_shape_rmse_db']=float(np.sqrt(np.mean((bands[0]-bands[1])**2)))
    ea,eb=envelope(ref),envelope(cand)
    result['envelope_mae']=float(np.mean(abs(ea-eb))/max(float(ea.max()),1e-12))
    for label,x in [('reference',ref),('candidate',cand)]:
        result[label+'_side_mid_ratio']=rms((x[:,0]-x[:,1])/2)/max(rms(x.mean(axis=1)),1e-12)
        f,p=spectrum(x,8192);p=p.mean(axis=1);keep=(f>=40)&(f<=12000);f=f[keep];p=p[keep]
        result[label+'_centroid_hz']=float(np.sum(f*p)/max(float(p.sum()),1e-24))
        result[label+'_rolloff95_hz']=float(f[min(np.searchsorted(np.cumsum(p),.95*p.sum()),len(f)-1)])
    if expected: result.update(harmonic_metrics(ref,cand,expected))
    return result

def write_csv(path,rows):
    fields=list(dict.fromkeys(k for r in rows for k in r))
    with path.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)

def run(args):
    profiles=json.loads(args.profiles_file.read_text())['profiles']
    selected=args.profiles.split(',');prerolls=[float(v) for v in args.prerolls.split(',')]
    if any(v<0 or not math.isfinite(v) for v in prerolls): raise ValueError('invalid pre-roll')
    for name in selected:
        if name not in profiles: raise ValueError('unknown profile: '+name)
    cases=list(csv.DictReader(args.manifest.open()))
    if args.cases: cases=[c for c in cases if c['case_id'] in args.cases.split(',')]
    if not cases: raise ValueError('no selected cases')
    root=args.manifest.resolve().parent
    for c in cases:
        for key in ('sysex','events','reference'):
            c[key]=str((root/c[key]).resolve())
            if not Path(c[key]).is_file(): raise ValueError('missing '+c[key])
    args.output.mkdir(parents=True,exist_ok=True)
    renderer=args.renderer.resolve();refs={c['case_id']:decode(Path(c['reference'])) for c in cases}
    def one(job):
        c,name,pre=job;out=args.output/'renders'/name/f"{c['case_id']}_pre{pre:g}.wav"
        out.parent.mkdir(parents=True,exist_ok=True)
        command=[str(renderer),c['sysex'],c['events'],str(out),'--tail-seconds','2',
                 '--pre-roll-seconds',str(pre),'--oversampling',str(args.oversampling),
                 *profiles[name]['renderer_args']]
        run=subprocess.run(command,check=True,capture_output=True,text=True)
        ref=refs[c['case_id']];cand=decode(out);start=round(float(c.get('candidate_start_s') or 0)*SR)
        cand=cand[start:start+len(ref)]
        if len(cand)<len(ref): raise ValueError('candidate shorter than reference: '+str(out))
        metrics=compare(ref,cand,c.get('kind','performance'),float(c['expected_f0_hz']) if c.get('expected_f0_hz') else None)
        out.with_suffix('.log').write_text(run.stdout+run.stderr)
        return dict(case_id=c['case_id'],name=c['name'],trust=c['trust'],kind=c['kind'],
                    profile=name,pre_roll_s=pre,**metrics)
    jobs=[(c,p,t) for c in cases for p in selected for t in prerolls]
    with ThreadPoolExecutor(max_workers=args.jobs) as pool: rows=list(pool.map(one,jobs))
    write_csv(args.output/'comparisons.csv',rows)
    numeric=[k for k,v in rows[0].items() if isinstance(v,(int,float)) and k!='pre_roll_s']
    summaries=[]
    for c in cases:
        for p in selected:
            rr=[r for r in rows if r['case_id']==c['case_id'] and r['profile']==p]
            summary=dict(case_id=c['case_id'],name=c['name'],profile=p,trust=c['trust'],realizations=len(rr))
            for key in dict.fromkeys(numeric+[k for r in rr for k in r if k.endswith('_db') or k.endswith('_cents')]):
                vals=[r[key] for r in rr if key in r and isinstance(r[key],(int,float))]
                if vals:
                    summary[key+'_median']=float(np.median(vals));summary[key+'_min']=min(vals);summary[key+'_max']=max(vals)
            summaries.append(summary)
    write_csv(args.output/'summary.csv',summaries)
    provenance=dict(source_branch='v0.19-sound-matching',source_sha=args.source_sha,
        renderer_sha256=hashlib.sha256(renderer.read_bytes()).hexdigest(),
        profiles_file_sha256=hashlib.sha256(args.profiles_file.read_bytes()).hexdigest(),
        prerolls=prerolls,oversampling=args.oversampling,sample_rate=SR,
        analysis_band_hz=[40,12000],fft_sizes=[512,2048,8192],
        limits=['single recording source, not independent hardware replications',
            'VCO pre-roll changes phase and Slop realization together',
            'relative gain only: recovered listening references were RMS-normalized',
            'inferred events, not recorded MIDI; sync envelope intentionally excluded',
            'no engine defaults changed'],
        assets={c['case_id']:{k:hashlib.sha256(Path(c[k]).read_bytes()).hexdigest() for k in ('reference','sysex','events')} for c in cases})
    (args.output/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    print(json.dumps(dict(renders=len(rows),cases=len(cases),profiles=len(selected),output=str(args.output))))

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('manifest',type=Path);ap.add_argument('--renderer',type=Path,required=True)
    ap.add_argument('--profiles-file',type=Path,default=Path('calibration/external_priors.json'))
    ap.add_argument('--profiles',default='current,reference_a_v07,reference_a_v08,reference_a_v09,phosphor_pw_v019,reference_a_v09_plus_phosphor_pw')
    ap.add_argument('--prerolls',default='0,0.5,1.5,3');ap.add_argument('--oversampling',type=int,choices=(1,2,4,8),default=2)
    ap.add_argument('--output',type=Path,required=True);ap.add_argument('--cases',default='');ap.add_argument('--jobs',type=int,default=4)
    ap.add_argument('--source-sha',default='unknown')
    args=ap.parse_args()
    try:run(args)
    except (ValueError,OSError,subprocess.SubprocessError) as exc:
        print(f'error: {exc}',file=sys.stderr);return 2
    return 0
if __name__=='__main__':raise SystemExit(main())
