#!/usr/bin/env python3
"""Adaptive pulse-duty counterexample using existing controls, never a new default."""
import csv,json,math,subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import numpy as np
from scipy.io import wavfile
from compare_online_suite import decode,SR
from compare_sync_controls import chord_harmonics
from analyze_sync_stages import write_csv


def main():
 out=Path('reference_private/sync_comparison');cases=[x for x in csv.DictReader((out/'chord_controls.csv').open()) if x['wave']=='Pulse'];x=decode(Path('reference_private/sync_search/OB6-P6-Vergleich-Waveform-Pulse_P6_right.wav'))
 refs={}
 for c in cases:
  start=float(c['start_s']);refs[start]=chord_harmonics(x[round(start*SR):round((start+1)*SR)],json.loads(c['pitches_hz']))
 # Ideal pulse magnitudes choose ONE global equivalent width from the first two
 # chord windows. The later nominal endpoint scores were already seen.
 def cost(duty):
  h=np.arange(1,17);db=20*np.log10(np.maximum(abs(np.sin(np.pi*h*duty)/(h*np.sin(np.pi*duty))),1e-12));model=np.tile(db,3);err=[]
  for start in [1.5,6.5]:
   ref,mask=refs[start];mask=mask & (ref>=-40) & np.tile(h<=8,3);err.extend((np.maximum(model[mask],-60)-np.maximum(ref[mask],-60))**2)
  return np.mean(err)
 grid=np.arange(.05,.50001,.001);duty=float(grid[np.argmin([cost(d) for d in grid])])
 protocol={'adaptive_after_nominal_scores':True,'fit_windows_s':[1.5,6.5],'fit_harmonics':list(range(1,9)),'grid':[.05,.5,.001],'chosen_existing_control_duty':duty,'evaluation_windows_s':[1.5,6.5,10.5,15.5],'evaluation_oversampling':[2,8],'limits':'Magnitude-equivalent width only; mirrored duty equivalent, exact hardware knob/filter/voice controls unknown. Later nominal scores already seen, not blind. Analytic selection checked using actual C++ renders.'}
 Path('calibration/v019_sync_comparison/pulse_width_protocol.json').write_text(json.dumps(protocol,indent=2)+'\n')
 def job(c):
  start=float(c['start_s']);os=int(c['oversampling']);pitches=json.loads(c['pitches_hz']);notes=[48,51,54] if start in [1.5,10.5] else [50,53,56];parts=[]
  for i,(note,f0) in enumerate(zip(notes,pitches)):
   wav=out/'controls'/f'PulseDuty_{start:g}_os{os}_voice{i}.wav';offset=12*math.log2(f0/(440*2**((note-69)/12)));cmd=['build-check/p6_sync_probe',str(wav),'--free-running','--note',str(note),'--slave-semis',str(offset),'--shape1','1','--pw1',str(duty),'--oversampling',str(os),'--seconds','1','--preroll','.5','--seed',str(982+i)];subprocess.run(cmd,check=True,capture_output=True);parts.append(decode(wav))
  y=sum(parts)/3;dest=out/'controls'/f'PulseDuty_{start:g}_os{os}.wav';wavfile.write(dest,SR,y.astype('float32'));rh,mask=refs[start];ch,_=chord_harmonics(y,pitches);mask &= rh>=-40
  return dict(start_s=start,oversampling=os,duty=duty,included_partials=int(mask.sum()),harmonic_rmse_floor60_db=float(np.sqrt(np.mean((np.maximum(rh[mask],-60)-np.maximum(ch[mask],-60))**2))),nominal_square_error_db=float(c['harmonic_rmse_floor60_db']),wav=str(dest))
 with ThreadPoolExecutor(max_workers=4) as pool:rows=list(pool.map(job,cases))
 write_csv(out/'pulse_width_comparisons.csv',rows);print(json.dumps({'renders':24,'duty':duty,'comparisons':rows}))


if __name__=='__main__':main()
