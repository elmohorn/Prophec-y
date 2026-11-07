#!/usr/bin/env python3
"""Post-coarse diagnostic refinement, only saw reset0 and reset0.5.

This is an explicitly adaptive exploration, not new independent validation.
"""
import csv,json,math,subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import numpy as np
from compare_online_suite import decode
from analyze_sync_stages import power_harmonics,write_csv
from compare_sync_recordings import error


def main():
 out=Path('reference_private/sync_comparison');rows=list(csv.DictReader((out/'window_comparisons.csv').open()));prov=json.loads((out/'provenance.json').read_text());f0=prov['common_f0_hz'];offset=12*math.log2(f0/(440*2**((51-69)/12)))
 chosen=[x for x in rows if float(x['shape'])==.5 and float(x['reset_phase']) in [0,.5]]
 specs=sorted(set((float(x['reset_phase']),round(float(x['ratio'])+d/16,8)) for x in chosen for d in range(-4,5) if 1<=float(x['ratio'])+d/16<=12))
 Path('calibration/v019_sync_comparison/refinement_protocol.json').write_text(json.dumps({'adaptive_after_coarse_results':True,'profiles':'saw shape0.5, reset0 and0.5 only','ratio_step':1/16,'radius':.25,'bounds':[1,12],'per_window_fit_harmonics':list(range(1,9)),'unused_in_ratio_fit':list(range(9,17)),'new_renders':len(specs),'oversampling':2,'duration_s':.25,'preroll_s':.1,'reason':'Check whether coarse ratio quantization explains the saw reset-phase comparison. High-harmonic scores had already been observed; no blind validation claim.'},indent=2)+'\n')
 def job(spec):
  phase,ratio=spec;name=f'refined_p{phase:g}_r{ratio:g}';wav=out/'renders'/(name+'.wav');cmd=['build-check/p6_sync_probe',str(wav),'--note','51','--master-semis',str(offset),'--slave-semis',str(offset+12*math.log2(ratio)),'--shape1','.5','--reset-phase',str(phase),'--oversampling','2','--seconds','.25','--preroll','.1'];r=subprocess.run(cmd,check=True,capture_output=True,text=True);wav.with_suffix('.log').write_text(r.stdout+r.stderr);m,h=power_harmonics(decode(wav),f0,16);return (phase,ratio,str(wav)),h-h[0]
 with ThreadPoolExecutor(max_workers=4) as pool:models=list(pool.map(job,specs))
 result=[]
 for old in chosen:
  ref=np.array([float(old[f'reference_h{k}_dbc']) for k in range(1,17)]);phase=float(old['reset_phase']);ratio=float(old['ratio']);options=[x for x in models if x[0][0]==phase and abs(x[0][1]-ratio)<=.250001];info,h=min(options,key=lambda x:error(x[1],ref,range(1,9)))
  row=dict(start_s=float(old['start_s']),reset_phase=phase,coarse_ratio=ratio,refined_ratio=info[1],fit_rmse_db=error(h,ref,range(1,9)),unused_high_rmse_db=error(h,ref,range(9,17)),all_rmse_db=error(h,ref,range(1,17)),wav=info[2])
  for k in range(16):row[f'reference_h{k+1}_dbc']=float(ref[k]);row[f'candidate_h{k+1}_dbc']=float(h[k])
  result.append(row)
 write_csv(out/'refined_saw_comparisons.csv',result)
 print(json.dumps({'new_renders':len(specs),'profiles':[{'reset_phase':phase,**{key:float(np.nanmedian([x[key] for x in result if x['reset_phase']==phase])) for key in ['fit_rmse_db','unused_high_rmse_db','all_rmse_db']}} for phase in [0,.5]]}))


if __name__=='__main__':main()
