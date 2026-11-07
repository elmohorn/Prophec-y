#!/usr/bin/env python3
"""Build isolated variations of the actual Character code; no repository mutation."""
from pathlib import Path
import subprocess,sys,tempfile,csv,math,hashlib
root=Path(__file__).resolve().parents[2]
out=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path('/tmp/prophec-plasma-ab')
out.mkdir(parents=True,exist_ok=True)
source=(root/'plugin/CustomCharacter.cpp').read_text()
needle='const float target=channel.lit?1.f:0.f;channel.conduction+=p.plasmaAttack*(target-channel.conduction);'
assert source.count(needle)==1,'review source change before adapting experiment'
soft='''const float envelopeRate = std::abs(voltage)>channel.envelope ? (1.f-std::exp(-1.f/(.002f*static_cast<float>(p.highRate)))) : (1.f-std::exp(-1.f/(.030f*static_cast<float>(p.highRate))));
                channel.envelope+=envelopeRate*(std::abs(voltage)-channel.envelope);
                const float knee=std::clamp((channel.envelope-p.threshold*.5f)/std::max(p.threshold,1.e-6f),0.f,1.f);
                const float target=knee*knee*(3.f-2.f*knee);channel.conduction+=p.plasmaAttack*(target-channel.conduction);'''
original='channel.tone+=p.toneAlpha*(y-channel.tone);y=channel.dc.Process(channel.tone)*p.output;'
assert source.count(original)==2
# Compensate the nominal small-signal slope from drive, saturation and output.
regulated='channel.tone+=p.toneAlpha*(y-channel.tone);y=channel.dc.Process(channel.tone)*p.output/std::max(1.f,p.drive*(2.f+8.f*character)*p.output);'
variants={'legacy':source,'soft_knee':source.replace(needle,soft),'gain_comp':source.replace(original,regulated,1),'new_defaults':source}
cpp=['plugin/CharacterFx.cpp','third_party/DaisySP/Source/Effects/wavefolder.cpp','third_party/DaisySP/Source/Filters/svf.cpp','third_party/DaisySP/Source/Utility/dcblock.cpp']
core=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else root/'build/libp6core.a'
if not core.is_file(): raise SystemExit('Build the core library and pass its path as the second argument; see experiment README.')
with tempfile.TemporaryDirectory(prefix='p6-plasma-') as temp:
 rows=[]
 for name,text in variants.items():
  f=Path(temp)/(name+'.cpp');f.write_text(text);exe=Path(temp)/name
  cmd=['g++','-std=c++20','-O2','-pthread','-Iinclude','-Iplugin','-Ithird_party/DSPark','-Ithird_party/DaisySP','-Ithird_party/DaisySP/Source','experiments/plasma_ab/probe.cpp',*cpp,str(f),str(core),'-o',str(exe)]
  subprocess.run(cmd,cwd=root,check=True)
  for sr in (44100,48000,96000):
   p=subprocess.run([str(exe),name,str(sr),str(out)],capture_output=True,text=True,check=True)
   rows+=list(csv.reader(p.stdout.splitlines()))
  print(name,'done',flush=True)
 with (out/'metrics.csv').open('w',newline='') as f:
  w=csv.writer(f);w.writerow(['variant','sample_rate','scenario','input_rms','output_rms','output_peak','max_sample_step']);w.writerows(rows)
print('Results:',out)
