#!/usr/bin/env python3
"""Analyze coherent-tone/impulse probes. Scores are diagnostics, not listening ranks."""
import argparse,csv,json,math
from pathlib import Path
import numpy as np

def db(v):return float(20*np.log10(max(float(v),1e-15)))
def metrics(x,kind,sr):
 result=dict(rms_dbfs=db(np.sqrt(np.mean(x*x))),mean_dc=float(x.mean()),channel_difference_rms_dbfs=db(np.sqrt(np.mean((x[:,0]-x[:,1])**2))))
 if kind=='silence':result['noise_rms_dbfs']=result['rms_dbfs']
 if kind=='left_only':result['right_leak_dbfs']=db(np.sqrt(np.mean(x[:,1]**2)))
 if kind=='impulse':result['impulse_peak_index']=int(np.argmax(abs(x[:,0])))
 if kind in ['tone_low','tone_hot','hf']:
  n=len(x);target=15000 if kind=='hf' else 1000;k=2*int(np.floor(target*n/sr/2+.5))+1
  z=np.fft.rfft(x[:,0]);p=abs(z)**2;f=np.fft.rfftfreq(n,1/sr)
  fundamental=float(p[k]);harmonics=[];valid=np.zeros(len(p),bool)
  for h in range(1,int((sr/2)/(k*sr/n))+1):
   b=k*h;valid[max(0,b-1):min(len(p),b+2)]=True
   if h>1:harmonics.append(float(p[b]))
  band=(f>=40)&(f<=min(20000,sr/2-1));residual=p[band&~valid]
  result.update(fundamental_hz=k*sr/n,fundamental_dbfs=db(2*np.sqrt(fundamental)/n),thd_percent=float(100*np.sqrt(sum(harmonics)/max(fundamental,1e-30))),nonharmonic_inband_db_relative_fundamental=db(np.sqrt(float(residual.sum())/max(fundamental,1e-30))))
 return result

def main():
 ap=argparse.ArgumentParser();ap.add_argument('input',type=Path);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 rows=[]
 for row in csv.DictReader((a.input/'runs.csv').open()):
  x=np.fromfile(a.input/row['file'],dtype='<f4').reshape(-1,2).astype(np.float64)
  if not np.isfinite(x).all():raise ValueError('nonfinite signal '+row['file'])
  rows.append({**row,**metrics(x,row['signal'],float(row['sample_rate']))})
 keys=list(dict.fromkeys(k for r in rows for k in r))
 with (a.output/'metrics.csv').open('w') as f:
  w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)
 summary=[]
 for id in sorted({r['candidate'] for r in rows}):
  rr=[r for r in rows if r['candidate']==id];cpu=[float(r['cpu_percent_one_core']) for r in rr]
  mid=[r for r in rr if float(r['sample_rate'])==48000 and float(r['dial'])==.5]
  summary.append(dict(candidate=id,renders=len(rr),nonfinite=sum(int(r['nonfinite']) for r in rr),ordinary_cpp_new_allocations=sum(int(r['new_allocations']) for r in rr),latencies_samples=sorted({int(r['latency_samples']) for r in rr}),prepare_ms_max=max(float(r['prepare_ms']) for r in rr),cpu_percent_median=float(np.median(cpu)),cpu_percent_range=[min(cpu),max(cpu)],max_peak=max(float(r['peak']) for r in rr),at_48k_mid_dial={r['signal']:{k:r[k] for k in ['rms_dbfs','noise_rms_dbfs','thd_percent','nonharmonic_inband_db_relative_fundamental','right_leak_dbfs'] if k in r} for r in mid}))
 report=dict(summary=summary,total_renders=len(rows),limits=['Native drive mappings differ; equal dial is not matched distortion or gain.','DSPark adapter uses one stage, sag .3, 100% wet, default 2x; remaining candidates use native rate without added oversampling.','CPU measured in shared cloud executor, one sweep, not product PC guarantees.','new counter covers ordinary C++ new calls only, not malloc/aligned allocation or all realtime contracts.','Steady tone FFT nonharmonic residual includes noise/transients; not a pure alias-energy measurement.','All audio is finite in recorded cases; this does not establish complete stability or analog fidelity.'])
 (a.output/'summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
