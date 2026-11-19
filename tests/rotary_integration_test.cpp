#include "CharacterFx.h"
#include "PluginStateCodec.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
int main(){p6plugin::WrapperState s,d;s.character.type=6;s.character.rotary={.4f,.7f,0.f,1.f,.5f};auto bytes=p6plugin::encodeState(s);assert(p6plugin::decodeState(bytes,d));assert(d.character.type==6&&std::abs(d.character.rotary[0]-.4f)<.001f);s.character.type=1;auto prior=p6plugin::encodeState(s);prior[4]=7;prior.resize(p6plugin::kV7StateBytes);assert(p6plugin::decodeState(prior,d));assert(d.character.type==1&&d.character.rotary==p6plugin::kRotaryDefaults);
for(int rate:{44100,48000,96000,192000,384000}){p6plugin::CharacterFx f;f.prepare(rate);p6plugin::CharacterState v;v.type=6;v.rotary={.5f,1.f,0.f,1.f,.5f};f.setState(v);double e=0,side=0;for(int i=0;i<rate*3;i++){if(i==rate)f.setModWheel(1.f);f.advanceRoute();double x=.1*std::sin(i*6.283185307*440/rate);auto o=f.process({x,x});assert(std::isfinite(o.left)&&std::isfinite(o.right));e+=o.left*o.left+o.right*o.right;side+=std::abs(o.left-o.right);}assert(e>1&&side>1);printf("%d: rotary output finite and stereo\n",rate);
 p6plugin::CharacterFx width;width.prepare(rate);width.setState(v);double recovered=0;for(int i=0;i<rate/2;i++){width.advanceRoute();auto y=width.process({.1,-.1});if(i>rate/4)recovered+=std::abs(y.left-y.right);}assert(recovered>rate*.02);
}
// Side-only stereo must follow Rotary Output, including at 100% wet.
{ double levels[2]{};int k=0;
 for(float gain:{0.f,1.f}) {p6plugin::CharacterFx fx;fx.prepare(48000);p6plugin::CharacterState v;v.type=6;v.rotary={0,0,0,1,gain};fx.setState(v);
  for(int i=0;i<48000;i++){fx.advanceRoute();double x=.1*std::sin(i*.07);auto y=fx.process({x,-x});if(i>=24000)levels[k]+=y.left*y.left;}++k;}
 assert(levels[1]>levels[0]*200.);
}
// A bypassed rotor must not replay frozen delay/filter contents on silence.
{ p6plugin::CharacterFx fx;fx.prepare(48000);p6plugin::CharacterState v;v.type=6;v.rotary={0,0,0,1,.5};fx.setState(v);
 for(int i=0;i<24000;i++){fx.advanceRoute();double x=.3*std::sin(i*.05);fx.process({x,x});}
 v.bypass=true;fx.setState(v);for(int i=0;i<96000;i++){fx.advanceRoute();fx.process({0,0});}
 v.bypass=false;fx.setState(v);double peak=0;for(int i=0;i<24000;i++){fx.advanceRoute();auto x=fx.process({0,0});peak=std::max(peak,std::abs(x.left));}assert(peak<1e-7);
}
}
