#!/usr/bin/env python3
"""Independent continuous-time Fourier oracle for a reset piecewise-linear VCO.

This tests the declared mathematical oscillator family, NOT Prophet-6 fidelity.
No BLEP, BLAMP, decimator or C++ synthesis implementation is used by the oracle.
See docs/V0_19_RESEARCH_REVIEW.md for the hard-sync/Fourier literature.
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
import numpy as np
from scipy.io import wavfile

def vertices(shape: float, duty: float=.5, edge: float=1e-6):
    """Geometry contract only: triangle->saw->pulse; no antialias algorithm."""
    if not 0<=shape<=1 or not .04<=duty<=.96 or not 0<edge<.01:
        raise ValueError('invalid geometry')
    if shape<.5:
        up=.5+shape*(1-2*edge);plateau=0.;down=1-up
    else:
        t=2*shape-1;up=(1-edge)-t*(1-2*edge)
        plateau=t*duty*(1-2*edge);down=edge
    return [(0.,-1.),(up,1.),(up+plateau,1.),(up+plateau+down,-1.),(1.,-1.)]

def segments(ratio: float,shape: float,duty: float=.5,edge:float=1e-6):
    if not math.isfinite(ratio) or ratio<=0 or ratio>128:
        raise ValueError('ratio must be finite in (0,128]')
    # Cut the master period wherever the slave crosses a waveform vertex/wrap.
    points=vertices(shape,duty,edge);result=[]
    for cycle in range(math.ceil(ratio)):
        for (a,ya),(b,yb) in zip(points[:-1],points[1:]):
            if b-a<1e-14:continue
            left=max(0.,(cycle+a)/ratio);right=min(1.,(cycle+b)/ratio)
            if right<=left:continue
            slope=(yb-ya)/(b-a)*ratio
            mid=(left+right)/2
            ym=ya+(ratio*mid-cycle-a)*(yb-ya)/(b-a)
            result.append((left,right,ym,slope))
    return result

def coefficients(ratio:float,shape:float,duty:float=.5,harmonics:int=20,edge:float=1e-6):
    """Exact segment integrals; stable series when narrow edges make w*L small."""
    if harmonics<1:raise ValueError('positive harmonic count required')
    h=np.arange(1,harmonics+1);w=2*np.pi*h;c=np.zeros(harmonics,dtype=complex)
    for left,right,ym,slope in segments(ratio,shape,duty,edge):
        length=right-left;mid=(left+right)/2;q=w*length/2
        i0=length*np.sinc(q/np.pi)
        term=np.empty_like(q);small=abs(q)<1e-3
        term[small]=-q[small]/3+q[small]**3/30-q[small]**5/840
        term[~small]=(q[~small]*np.cos(q[~small])-np.sin(q[~small]))/q[~small]**2
        i1=1j*length**2/2*term
        c+=np.exp(-1j*w*mid)*(ym*i0+slope*i1)
    return c

def amplitudes(x,sr,f0,count=20):
    # Hann projection at exact frequencies; independent of the C++ probe's code.
    t=np.arange(len(x))/sr;window=np.hanning(len(x));x=x-x.mean()
    return np.array([2*abs(np.sum(x*window*np.exp(-2j*np.pi*f0*h*t)))/window.sum()
                     for h in range(1,count+1)])

def relative_db(x):
    return 20*np.log10(np.maximum(x/max(float(np.max(x)),1e-30),1e-12))

def run(args):
    root=args.output;root.mkdir(parents=True,exist_ok=True);probe=args.probe.resolve()
    semis=[19.,24.,28.,31.,36.,40.];shapes=[0.,.25,.5,.75,1.]
    notes=[int(n) for n in args.notes.split(',')]
    if not notes or any(n<0 or n>65 for n in notes):
        raise ValueError('choose MIDI notes 0..65 so all 20 measured harmonics stay in band')
    jobs=[(note,s,w,o,m) for note in notes for s in semis for w in shapes for o in (1,2,4,8) for m in ('legacy','step-slope')]
    oracle={}
    for s in semis:
        for shape in shapes:oracle[(s,shape)]=abs(coefficients(2**(s/12),shape))
    def one(job):
        note,s,shape,o,mode=job;f0=440*2**((note-69)/12)
        wav=root/'renders'/f'n{note}_s{s:g}_w{shape:g}_os{o}_{mode}.wav'
        wav.parent.mkdir(exist_ok=True)
        cmd=[str(probe),str(wav),'--note',str(note),'--slave-semis',str(s),'--shape1',str(shape),
             '--oversampling',str(o),'--seconds','1','--preroll','0.5','--correction',mode]
        log=subprocess.run(cmd,check=True,capture_output=True,text=True)
        sr,x=wavfile.read(wav);assert sr==48000
        candidate=amplitudes(x.mean(axis=1).astype(float),sr,f0)
        target=oracle[(s,shape)];a=relative_db(target);b=relative_db(candidate)
        mask=a>=-50
        err=b[mask]-a[mask]
        row=dict(note=note,slave_semitones=s,shape=shape,oversampling=o,mode=mode,
            included_harmonics=int(mask.sum()),harmonic_rmse_db=float(np.sqrt(np.mean(err**2))),
            harmonic_max_abs_error_db=float(max(abs(err))),
            oracle_h5_minus_h4_db=float(a[4]-a[3]),candidate_h5_minus_h4_db=float(b[4]-b[3]))
        for i in range(20):row[f'oracle_h{i+1}_db']=float(a[i]);row[f'candidate_h{i+1}_db']=float(b[i])
        wav.with_suffix('.log').write_text(log.stdout+log.stderr)
        return row
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:rows=list(pool.map(one,jobs))
    with (root/'harmonics.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=rows[0]);writer.writeheader();writer.writerows(rows)
    (root/'provenance.json').write_text(json.dumps(dict(
        source_sha=args.source_sha,probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),
        oracle='analytic continuous-time piecewise-linear integrals',edge_fraction=1e-6,
        notes=notes,sample_rate=48000,harmonics=20,inclusion_floor_db=-50,
        limits=['validates this waveform family only; not hardware truth',
                'static ratios only, no filter/Poly Mod/Slop/FX',
                'does not measure out-of-harmonic alias energy or all transient behavior'],renders=len(rows)),indent=2)+'\n')
    print(json.dumps(dict(renders=len(rows),output=str(root))))

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--probe',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--jobs',type=int,default=4);ap.add_argument('--source-sha',default='unknown')
    ap.add_argument('--notes',default='41,65',help='comma-separated MIDI notes, at most 65')
    run(ap.parse_args())
if __name__=='__main__':main()
