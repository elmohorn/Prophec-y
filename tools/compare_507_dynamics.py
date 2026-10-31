#!/usr/bin/env python3
"""Provisional performance reconstruction and filter/PolyMod ablation for507.

Inferred gates and a presumed factory patch are nuisance assumptions. No DSP
changes or calibrated hardware parameter estimates are produced by this tool.
"""
import csv,json,subprocess,hashlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import numpy as np
from compare_online_suite import decode,compare,SR,envelope
from analyze_sync_stages import write_csv


def main():
 out=Path('reference_private/507_dynamics');out.mkdir(exist_ok=True,parents=True);data=Path('calibration/v019_507_dynamics')
 cases=[dict(id='amazona',file='507-SyncLead-Uni.mp3',on=12.91,note=79,gate=14.10,end=16.8),dict(id='audiofanzine',file='Audiofanzine_507.mp3',on=13.49,note=57,gate=14.10,end=15.4)]
 profiles={'v09':[], 'filter_bypass':['--filter-bypass','--preserve-output-controls'],'pm_bypass':['--poly-mod-bypass','--preserve-output-controls'],'pm_pitch0':['--pm-pitch-octaves','0'],'pm_shape0':['--pm-shape-range','0']}
 for maximum in [2.5,5,10]:
  for exponent in [2,3,4]:profiles[f'time{maximum:g}_curve{exponent}']=['--filter-env-max-seconds',str(maximum),'--filter-env-exponent',str(exponent)]
 for span in [1,2,6]:profiles[f'pm_pitch{span}']=['--pm-pitch-octaves',str(span)]
 protocol=dict(cases=cases,profiles=profiles,gate_offsets_s=[-.10,0,.10],onset_offsets_s=[-.04,0,.04],note='Gate offsets and onset offsets are paired (-.04,-.10),(0,0),(.04,.10); this is a sensitivity check, not all nine combinations.',frozen_before_dynamics_model_scores=True,fit_source='audiofanzine',cross_source='amazona',limits='Previous comparisons of both sources were already seen. Last-note isolation omits previous-note state. Full phrase reconstructions preserve uncertainty and are not exact MIDI.',selection='rank median MR spectral error on Audiofanzine; report band error and envelope independently; crosscheck fixed candidate on Amazona; no per-source retuning')
 (data/'protocol.json').write_text(json.dumps(protocol,indent=2)+'\n')
 refs={}
 for c in cases:
  x=decode(Path('reference_private/sync_search')/c['file']);refs[c['id']]=x
 jobs=[(c,p,shift,gate) for c in cases for p in profiles for shift,gate in [(-.04,-.1),(0,0),(.04,.1)]]
 def job(spec):
  c,p,shift,gate=spec;name=f"{c['id']}_{p}_shift{shift:g}";events=out/(name+'.csv');wav=out/(name+'.wav');on=c['on']+shift
  # Start the note at local0; shift only changes reference alignment and gate.
  duration=c['gate']+gate-on
  events.write_text(f'time_s,midi_note,velocity_0_to_1,duration_s\n0,{c["note"]},1,{duration}\n')
  cmd=['build-check/p6_render_events_cal','reference_private/007.syx',str(events),str(wav),'--reference-a-v09','--tail-seconds',str(c['end']-on-duration),'--pre-roll-seconds','.5']+profiles[p]
  rr=subprocess.run(cmd,check=True,capture_output=True,text=True);wav.with_suffix('.log').write_text(rr.stdout+rr.stderr)
  ref=refs[c['id']][round(on*SR):round(c['end']*SR)];cand=decode(wav);n=min(len(ref),len(cand));ref=ref[:n];cand=cand[:n];n=n//240*240;ref=ref[:n];cand=cand[:n]
  # Complete note+release metrics, no automatic envelope lag fitting.
  m=compare(ref,cand,kind='steady');windows=[]
  for age in np.arange(.1,min(duration,1.3)-.2,.1):
   a=round(age*SR);b=a+9600;z=compare(ref[a:b],cand[a:b],kind='steady');windows.append(z['mr_log_spectral_mae_db'])
  return dict(source=c['id'],profile=p,onset_shift_s=shift,gate_shift_s=gate,**{k:m[k] for k in ['mr_log_spectral_mae_db','log_band_shape_rmse_db','envelope_mae']},median_200ms_error_db=float(np.median(windows)),wav=str(wav))
 with ThreadPoolExecutor(max_workers=4) as pool:rows=list(pool.map(job,jobs))
 write_csv(data/'isolated_comparisons.csv',rows)
 summary=[]
 for c in cases:
  for p in profiles:
   rr=[x for x in rows if x['source']==c['id'] and x['profile']==p]
   summary.append(dict(source=c['id'],profile=p,**{k:float(np.median([x[k] for x in rr])) for k in ['mr_log_spectral_mae_db','log_band_shape_rmse_db','envelope_mae','median_200ms_error_db']}))
 write_csv(data/'isolated_summary.csv',summary)
 eligible=[x for x in summary if x['source']=='audiofanzine' and x['profile'].startswith(('time','pm_pitch')) and x['profile']!='pm_pitch0'];best=min(eligible,key=lambda x:x['mr_log_spectral_mae_db'])['profile']
 # Full phrase: keep inferred note runs, with a separate fixed gate policy.
 phrasejobs=[]
 for c in cases:
  raw=list(csv.DictReader((data/f"{c['id']}_events.csv").open()))
  # End the inferred run at the assumed physical final gate, never turn the
  # changing release spectrum into new octave-alias note-ons.
  raw=[x for x in raw if float(x['time_s'])<c['on']-.08]
  raw.append(dict(time_s=c['on'],midi_note=c['note'],velocity_0_to_1=1,duration_s=c['gate']-c['on']))
  for policy in ['detached','connected']:
   ev=out/f"{c['id']}_phrase_{policy}.csv"
   with ev.open('w',newline='') as f:
    w=csv.writer(f,lineterminator='\n');w.writerow(['time_s','midi_note','velocity_0_to_1','duration_s'])
    for i,row in enumerate(raw):
     start=float(row['time_s']);duration=float(row['duration_s'])
     if i+1<len(raw):duration=max(.02,float(raw[i+1]['time_s'])-start+(.02 if policy=='connected' else -.02))
     w.writerow([start,int(row['midi_note']),1,duration])
   # Fixed best candidate carried across both sources and policies.
   for p in dict.fromkeys(['v09',best]):phrasejobs.append((c,policy,ev,p))
 def phrase(spec):
  c,policy,ev,p=spec;wav=out/f"{c['id']}_phrase_{policy}_{p}.wav";cmd=['build-check/p6_render_events_cal','reference_private/007.syx',str(ev),str(wav),'--reference-a-v09','--tail-seconds',str(c['end']-c['gate']),'--pre-roll-seconds','.5']+profiles[p];subprocess.run(cmd,check=True,capture_output=True);ref=refs[c['id']][:round(c['end']*SR)];cand=decode(wav);m=compare(ref,cand,kind='steady');return dict(source=c['id'],policy=policy,profile=p,**{k:m[k] for k in ['mr_log_spectral_mae_db','log_band_shape_rmse_db','envelope_mae']},wav=str(wav))
 with ThreadPoolExecutor(max_workers=4) as pool:phrases=list(pool.map(phrase,phrasejobs))
 write_csv(data/'phrase_comparisons.csv',phrases)
 (data/'provenance.json').write_text(json.dumps(dict(renders=len(jobs)+len(phrasejobs),isolated_renders=len(jobs),phrase_renders=len(phrasejobs),candidate_selected_on_audiofanzine=best,production_changes=False,source_hashes={c['file']:hashlib.sha256((Path('reference_private/sync_search')/c['file']).read_bytes()).hexdigest() for c in cases},renderer_sha256=hashlib.sha256(Path('build-check/p6_render_events_cal').read_bytes()).hexdigest()),indent=2)+'\n')
 print(json.dumps(dict(best=best,summary=summary,phrases=phrases)))


if __name__=='__main__':main()
