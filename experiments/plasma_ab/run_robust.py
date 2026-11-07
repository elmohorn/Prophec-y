#!/usr/bin/env python3
"""Additional offline Hybrid tests; source transforms remain experiment-only."""
import ast, csv, subprocess, sys, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path('/tmp/prophec-hybrid-robust')
core=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else root/'build/libp6core.a'
if not core.is_file(): raise SystemExit('Build p6core first and pass the library path.')
out.mkdir(parents=True,exist_ok=True)
values={}
for node in ast.parse((root/'experiments/plasma_ab/run.py').read_text()).body:
 if isinstance(node,ast.Assign) and len(node.targets)==1 and isinstance(node.targets[0],ast.Name) and node.targets[0].id in ('needle','soft','original','regulated'):
  values[node.targets[0].id]=ast.literal_eval(node.value)
source=(root/'plugin/CustomCharacter.cpp').read_text()
if source.count(values['needle'])!=1 or source.count(values['original'])!=2: raise SystemExit('Production source changed; review the experiment.')
hybrid=source.replace(values['needle'],values['soft']).replace(values['original'],values['regulated'],1)
# Precompute fixed 2x Plasma envelope coefficients during prepare().
fields='float springDecay{},springDamping{},plasmaAttack{},ferroAttack{};'
initial='    p.delay=std::max(0,latency-p.os.getLatency());'
rates='(1.f-std::exp(-1.f/(.002f*static_cast<float>(p.highRate)))) : (1.f-std::exp(-1.f/(.030f*static_cast<float>(p.highRate))))'
if hybrid.count(fields)!=1 or hybrid.count(initial)!=1 or hybrid.count(rates)!=1: raise SystemExit('Hybrid source changed; review coefficient caching.')
cached=hybrid.replace(fields,'float springDecay{},springDamping{},plasmaAttack{},ferroAttack{},hybridGateAttack{},hybridGateRelease{};')
cached=cached.replace(initial,'    p.hybridGateAttack=1.f-std::exp(-1.f/(.002f*static_cast<float>(p.highRate)));p.hybridGateRelease=1.f-std::exp(-1.f/(.030f*static_cast<float>(p.highRate)));\n'+initial).replace(rates,'p.hybridGateAttack : p.hybridGateRelease')
common=['plugin/CharacterFx.cpp','third_party/DaisySP/Source/Effects/wavefolder.cpp','third_party/DaisySP/Source/Filters/svf.cpp','third_party/DaisySP/Source/Utility/dcblock.cpp']
with tempfile.TemporaryDirectory(prefix='p6-hybrid-') as tmp:
 temp=Path(tmp); candidate=temp/'CustomCharacter_hybrid.cpp';candidate.write_text(hybrid); cached_candidate=temp/'CustomCharacter_cached.cpp';cached_candidate.write_text(cached)
 for probe in ('automation','cpu','alias'):
  for variant in (('hybrid','hybrid_cached') if probe=='automation' else ('legacy','hybrid','hybrid_cached')):
   src=cached_candidate if variant=='hybrid_cached' else candidate if variant=='hybrid' else root/'plugin/CustomCharacter.cpp';exe=temp/(probe+'_'+variant)
   cmd=['g++','-std=c++20','-O3','-DNDEBUG','-pthread','-Iinclude','-Iplugin','-Ithird_party/DSPark','-Ithird_party/DaisySP','-Ithird_party/DaisySP/Source',f'experiments/plasma_ab/{probe}_probe.cpp',*common,str(src),str(core),'-o',str(exe)]
   subprocess.run(cmd,cwd=root,check=True)
   if probe=='alias': subprocess.run([str(exe),str(out/(probe+'_'+variant+'.f64'))],check=True)
   else:
    p=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
    (out/(probe+'_'+variant+'.csv')).write_text(p.stdout)
   print(probe,variant,'done',flush=True)
print('Results:',out)
