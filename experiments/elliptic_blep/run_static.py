"""Exploratory stationary-morph/sync comparison; not a production-voice test."""
import csv, subprocess, time
from pathlib import Path
import numpy as np
from scipy.signal import resample_poly
from scipy.optimize import minimize_scalar
from scipy.io import wavfile

root=Path(__file__).resolve().parent
exe=root/'static_probe'
out=root/'results';out.mkdir(exist_ok=True)
cases=[('tri_low',48000,110,0.,.5,0),('saw_mid',48000,997,.5,.5,0),('pulse_wide',48000,997,1.,.5,0),('pulse_narrow',48000,997,1.,.1,0),('morph_a',48000,997,.25,.5,0),('morph_b',48000,997,.75,.2,0),('saw_high',48000,8000,.5,.5,0),('sync_saw',48000,997,.5,.5,1.5),('sync_pulse',48000,997,1.,.25,2.1),('saw_441',44100,997,.5,.5,0),('saw_96',96000,997,.5,.5,0)]

def render(name, mode, sr, hz, shape, duty, sync):
 path=out/(name+'_'+mode+'.raw')
 p=subprocess.run([str(exe),str(path),mode,str(sr),str(hz),str(shape),str(duty),str(sync)],capture_output=True,text=True)
 if p.returncode:raise RuntimeError(p.stderr)
 return np.fromfile(path,dtype=np.float64),p.stderr.strip()

def compare(x,ref,sr):
 # Isolated 1-s portion, FFT-based fractional circular shift; cosine taper
 # is deliberately absent: integer-Hz periodic signals need coherent windows.
 n=int(sr);start=int(sr*.5);x=x[start:start+n];ref=ref[start:start+n]
 freqs=np.fft.rfftfreq(n);Y=np.fft.rfft(x);R=np.fft.rfft(ref)
 def fit(delay):
  shifted=Y*np.exp(-2j*np.pi*freqs*delay)
  gain=np.vdot(shifted,R).real/max(np.vdot(shifted,shifted).real,1e-30)
  return np.sum(np.abs(R-gain*shifted)**2),gain
 candidates=[(fit(i)[0],i) for i in range(-12,13)];center=min(candidates)[1]
 z=minimize_scalar(lambda d:fit(d)[0],bounds=(center-1,center+1),method='bounded')
 err,gain=fit(z.x);return 10*np.log10(max(err,1e-30)/np.sum(np.abs(R)**2)),gain,z.x

rows=[]
for name,sr,hz,shape,duty,sync in cases:
 current,ct=render(name,'current',sr,hz,shape,duty,sync)
 elliptic,et=render(name,'elliptic',sr,hz,shape,duty,sync)
 ref16,_=render(name,'naive',sr*16,hz,shape,duty,sync)
 ref8,_=render(name,'naive',sr*8,hz,shape,duty,sync)
 r16=resample_poly(ref16,1,16,window=('kaiser',12.0));r8=resample_poly(ref8,1,8,window=('kaiser',12.0))
 conv,_,_=compare(r8,r16,sr)
 for mode,x,timing in [('current',current,ct),('elliptic',elliptic,et)]:
  err,gain,delay=compare(x,r16,sr);peak=float(np.max(np.abs(x)));rms=float(np.sqrt(np.mean(x*x)))
  alias_db = float('nan')
  # For integer-Hz non-sync stationary cases: out-of-harmonic bins only.
  # Alias landing on harmonics is invisible to this metric.
  if sync == 0 and hz == int(hz):
   segment=x[sr//2:sr//2+sr];power=np.abs(np.fft.rfft(segment))**2
   mask=np.ones(len(power),dtype=bool);mask[0]=False
   for harmonic in range(int(hz),sr//2+1,int(hz)):
    mask[max(0,harmonic-1):min(len(power),harmonic+2)]=False
   alias_db=10*np.log10(max(power[mask].sum(),1e-30)/max(power[~mask].sum(),1e-30))
  rows.append([name,sr,hz,shape,duty,sync,mode,err,gain,delay,peak,rms,conv,alias_db,timing])
 # listening: each pair uses equal RMS on the steady-state after fade-in;
 # whole 1.5 s clip, peak constrained by the same factor for both.
 if name in ('saw_mid','pulse_narrow','sync_saw'):
  a=current.copy();b=elliptic.copy(); sl=slice(sr//4,None)
  b*=np.sqrt(np.mean(a[sl]**2)/max(np.mean(b[sl]**2),1e-30))
  g=.8/max(np.max(np.abs(a)),np.max(np.abs(b)),1e-30)
  for label,x in [('actual',a),('elliptic',b)]:wavfile.write(out/(name+'_'+label+'.wav'),sr,(x*g).astype(np.float32))
 print(name,'reference 8x/16x error dB',round(conv,1),flush=True)
with (out/'results.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['case','sr','hz','shape','duty','sync_ratio','method','relative_total_error_db','fit_gain','shift_samples','peak','rms','reference_8x_vs_16x_db','out_of_harmonic_db','render_timing_not_cpu_benchmark']);w.writerows(rows)
