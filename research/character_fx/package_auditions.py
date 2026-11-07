#!/usr/bin/env python3
"""Write RMS-matched audition WAVs from the probe's original musical input.

This is not a blind listening verdict: each candidate keeps its native .5 dial.
"""
import argparse,csv,json,wave
from pathlib import Path
import numpy as np

def main():
 ap=argparse.ArgumentParser();ap.add_argument('sources',type=Path);ap.add_argument('results',type=Path);ap.add_argument('output',type=Path);a=ap.parse_args();a.output.mkdir(parents=True,exist_ok=True)
 source=np.fromfile(a.sources/'musical_source.f32',dtype='<f4').reshape(-1,2);n=len(source)
 rows=[]
 for name,latency in [('dry',0),('airwindows',0),('dspark',71),('patina',0),('daisy',0),('0xfx',0)]:
  x=source.copy() if name=='dry' else np.fromfile(a.results/(name+'_musical.f32'),dtype='<f4').reshape(-1,2)[latency:latency+n].astype(float)
  assert len(x)==n and np.isfinite(x).all()
  rms=float(np.sqrt(np.mean(x*x)));peak=float(np.max(abs(x)));gain=min(10**(-20/20)/max(rms,1e-15),.95/max(peak,1e-15));x*=gain
  filename=name+'_RMS_matched.wav'
  with wave.open(str(a.output/filename),'wb') as f:
   f.setparams((2,3,48000,0,'NONE','not compressed'))
   pcm=np.clip(np.rint(x.reshape(-1)*8388607),-8388608,8388607).astype('<i4');u=pcm.view('<u4');raw=np.stack([u&255,(u>>8)&255,(u>>16)&255],axis=1).astype('u1');f.writeframes(raw.tobytes())
  rows.append(dict(candidate=name,file=filename,latency_trimmed_samples=latency,applied_gain_db=float(20*np.log10(gain)),final_rms_dbfs=float(20*np.log10(np.sqrt(np.mean(x*x)))),final_peak_dbfs=float(20*np.log10(max(abs(x).reshape(-1))))))
 with (a.output/'levels.csv').open('w') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
 (a.output/'README.txt').write_text('Fuente original renderizada por Prophec-y; no es audio del Prophet fisico.\nPrograma000 con FX y distorsion desactivados en clon privado; acorde MIDI48/52/55, velocity0.8.\nMismo audio de entrada para todos. Dial nativo0.5; no equivale a igual cantidad de distorsion.\nArchivos24-bit stereo48kHz, RMS objetivo-20dBFS; no normalizacion LUFS.\nDSPark:1etapa,drive+12dB,sag0.3,2x interno; se recortan71muestras de latencia declarada.\nLos otros kernels no reciben oversampling externo en esta prueba.\nLos modelos no estan incorporados al plugin. Ver levels.csv y el informe para limites.\n')
 print(json.dumps(rows,indent=2))
if __name__=='__main__':main()
