#!/usr/bin/env python3
"""Build a scratch-only comparison against pinned upstream MIT DSP sources.

Requires the source directories listed by the acquisition manifest; never adds
upstream DSP to the production plugin. Adapters normalize API, not tone/drive.
"""
import argparse,hashlib,json,subprocess
from pathlib import Path

def function(text,name):
    a=text.index('static void '+name);brace=text.index('{',a);depth=1;i=brace+1
    while depth:
        depth+=(text[i]=='{')-(text[i]=='}');i+=1
    return text[a:i]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('sources',type=Path);ap.add_argument('--sanitize',action='store_true');a=ap.parse_args();r=a.sources.resolve()
    studio=(r/'0xFX/src/engine/internal/studio.c').read_text()
    end=studio.index('} valve_color_state_t;')+len('} valve_color_state_t;');start=studio.rfind('typedef struct',0,end)
    extract=studio[start:end]+'\n'+function(studio,'valve_color_process')
    wrapper='''// Scratch adapter of MIT-licensed source; upstream notices remain beside original files.
#include <algorithm>
#include <cmath>
#include <cstdint>
using VstInt32=int32_t;
#define __Tube2_H
class Tube2 { public:
 float A=1,B=.5f;double sr=48000;
 double previousSampleA=0,previousSampleB=0,previousSampleC=0,previousSampleD=0,previousSampleE=0,previousSampleF=0;
 uint32_t fpdL=1234567,fpdR=7654321;
 double getSampleRate() const{return sr;}
 void processReplacing(float**,float**,VstInt32);
 void processDoubleReplacing(double**,double**,VstInt32);
};
'''
    wrapper+='\n#include "'+str(r/'airwindows/plugins/WinVST/Tube2/Tube2Proc.cpp')+'"\n'
    wrapper+='namespace oxfx { struct fx_studio_instance_t {void* state;float params[4];};\n'+extract+'\n}\n'
    (r/'upstream_adapters.h').write_text(wrapper)
    src=Path(__file__).with_name('fx_probe.cpp').resolve()
    cmd=['c++','-std=c++20','-O2','-Wall','-Wextra','-I'+str(r),'-I'+str(r/'DSPark'),'-I'+str(r/'DaisySP/Source/Utility'),str(src),str(r/'DaisySP/Source/Effects/overdrive.cpp'),'-o',str(r/'fx_probe')]
    if a.sanitize:
        cmd[cmd.index('-O2')]='-O1';cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer'];cmd[-1]=str(r/'fx_probe_asan')
    subprocess.run(cmd,check=True)
    (r/('build_asan.json' if a.sanitize else 'build.json')).write_text(json.dumps(dict(command=cmd,binary_sha256=hashlib.sha256(Path(cmd[-1]).read_bytes()).hexdigest()),indent=2)+'\n')
if __name__=='__main__':main()
