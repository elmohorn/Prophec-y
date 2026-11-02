"""Exercise the experimental sync path; return success while recording failures."""
import csv,itertools,subprocess
from pathlib import Path
root=Path(__file__).resolve().parent
cases=itertools.product((0.,.25,.5,.75,1.),(.04,.5,.96),(110.,997.,8000.),(.5,1.,2.1,4.))
rows=[]
for shape,duty,hz,ratio in cases:
 if hz*ratio>48000*.45:continue
 for mode in ('current','elliptic'):
  p=subprocess.run([str(root/'static_probe'),'/dev/null',mode,'48000',str(hz),str(shape),str(duty),str(ratio)],capture_output=True,text=True)
  rows.append((mode,shape,duty,hz,ratio,p.returncode,p.stderr.strip()))
with (root/'results/sync_stress.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['method','shape','duty','hz','ratio','exit_code','diagnostic']);w.writerows(rows)
for mode in ('current','elliptic'):
 subset=[r for r in rows if r[0]==mode];failed=[r for r in subset if r[5]]
 print(f'{mode}: {len(subset)} cases; {len(failed)} failed the abs(output) <= 100 check')
 for r in failed[:3]:print(r)
