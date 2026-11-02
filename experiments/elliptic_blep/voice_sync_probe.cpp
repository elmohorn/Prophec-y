#include "p6/P6Voice.h"
#include <algorithm>
#include <cmath>
#include <iostream>
int main(){for(int os:{1,2})for(int note:{45,119})for(int semi:{0,-12}){int failures=0;double peak=0,pre=0;for(unsigned seed=1;seed<=20;++seed){double seedPeak=0,seedPre=0;p6::P6Voice v;v.prepare(48000,seed,os);p6::VoiceParams p;p.sync=true;p.osc1.shape=1;p.osc1.level=1;p.osc2.level=0;p.osc2.semitones=semi;v.setParams(p);p6::VoiceDiagnostics d;d.mix=p6::VoiceDiagnosticMix::Osc1Only;d.bypassFilters=true;v.setDiagnostics(d);v.noteOn(note);for(int i=0;i<12000;++i){auto x=v.process(0,0);seedPeak=std::max(seedPeak,std::abs(x.dry));seedPre=std::max(seedPre,std::abs(x.preFilter));}peak=std::max(peak,seedPeak);pre=std::max(pre,seedPre);if(seedPeak>10||seedPre>100)++failures;}std::cout<<"os "<<os<<" note "<<note<<" semi "<<semi<<" failures "<<failures<<" peak "<<peak<<" pre "<<pre<<'\n';}}
