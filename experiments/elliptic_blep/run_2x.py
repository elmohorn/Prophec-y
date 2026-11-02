"""Oscillators alone at 2x, followed by P6Voice's 2x FIR, not the voice path."""
import csv, subprocess
from pathlib import Path
import numpy as np
from scipy.signal import lfilter
root=Path(__file__).resolve().parent;out=root/'results';exe=root/'static_probe'
taps=np.array([-0.0045008481327892709,-3.9951677602018973e-06,0.0070884249415108184,2.7819696099880195e-07,-0.012457607023340873,-7.5118250422239079e-06,0.020566233912965352,5.6543610983244625e-06,-0.033089432751335759,-1.1055962468726037e-05,0.054406576637674564,4.3321895465555246e-06,-0.10059227382572718,-9.3020689158754156e-06,0.31758521603022044,0.502030620974806,0.31758521603022044,-9.3020689158754156e-06,-0.10059227382572718,4.3321895465555246e-06,0.054406576637674564,-1.1055962468726037e-05,-0.033089432751335759,5.6543610983244625e-06,0.020566233912965352,-7.5118250422239079e-06,-0.012457607023340873,2.7819696099880195e-07,0.0070884249415108184,-3.9951677602018973e-06,-0.0045008481327892709])
rows=[]
for name,shape,duty in [('saw',.5,.5),('pulse',1.,.1),('intermediate',.75,.2)]:
 for mode in ['current','elliptic']:
  f=out/f'2x_{name}_{mode}.raw';subprocess.run([str(exe),str(f),mode,'96000','997',str(shape),str(duty),'0'],check=True,capture_output=True)
  x=np.fromfile(f,dtype=np.float64);y=lfilter(taps,[1.],x)[1::2];seg=y[24000:72000];power=abs(np.fft.rfft(seg))**2;mask=np.ones(len(power),dtype=bool);mask[0]=False
  for k in range(997,24001,997):mask[max(0,k-1):min(len(power),k+2)]=False
  db=10*np.log10(max(power[mask].sum(),1e-30)/max(power[~mask].sum(),1e-30));rows.append((name,mode,db,float(np.max(abs(y)))))
  print(name,mode,round(db,2))
with (out/'isolated_2x.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['case','method','out_of_harmonic_db','peak']);w.writerows(rows)
