from pathlib import Path
import sys,csv,subprocess
sys.path.insert(0,'tools')
from compare_online_suite import decode,compare,SR
from analyze_sync_stages import write_csv
r=Path('reference_private/507_dynamics');src=decode(Path('reference_private/sync_search/507-SyncLead-Uni.mp3'));rows=[]
for gate in [15.1,16.1]:
 for profile in ['v09','time10_curve3']:
  name=f'amazona_late_gate{gate}_{profile}';ev=r/(name+'.csv');ev.write_text(f'time_s,midi_note,velocity_0_to_1,duration_s\n0,79,1,{gate-12.91}\n');wav=r/(name+'.wav');cmd=['build-check/p6_render_events_cal','reference_private/007.syx',str(ev),str(wav),'--reference-a-v09','--pre-roll-seconds','.5','--tail-seconds',str(18.6-gate)]
  if profile!='v09':cmd+=['--filter-env-max-seconds','10','--filter-env-exponent','3']
  subprocess.run(cmd,check=True,capture_output=True);m=compare(src[round(12.91*SR):round(18.6*SR)],decode(wav),kind='steady');rows.append(dict(source='amazona',assumed_gate_s=gate,profile=profile,**{k:m[k] for k in ['mr_log_spectral_mae_db','log_band_shape_rmse_db','envelope_mae']},wav=str(wav)))
write_csv(Path('calibration/v019_507_dynamics/late_gate_sensitivity.csv'),rows);print(rows)
