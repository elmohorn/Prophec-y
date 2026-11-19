#include "adapter.h"
#include <cassert>
#include <initializer_list>
#include <cmath>
#include <cstdio>
int main(){
 for(int rate: {44100,48000,96000,192000}) {
  RotarySetBfree fx; assert(fx.prepare(rate)); fx.setWheelDepth(1.f); fx.setMotion(.5f); fx.setModWheel(0.f);
  float in[64]{},l[64],r[64]; for(int n=0;n<rate*4/64;++n)fx.process(in,l,r,64);
  double slowH=fx.hornHz(),slowD=fx.drumHz();
  fx.setModWheel(1.f); for(int n=0;n<rate*6/64;++n)fx.process(in,l,r,64);
  assert(fx.hornHz()>slowH+3.); assert(fx.drumHz()>slowD+1.);
  fx.setModWheel(0.f); for(int n=0;n<rate*10/64;++n)fx.process(in,l,r,64);
  assert(fx.hornHz()<slowH+1.);
  fx.setSpeed(RotarySetBfree::Stop); for(int n=0;n<rate*15/64;++n)fx.process(in,l,r,64);
  assert(fx.hornHz()<.05 && fx.drumHz()<.05);
  std::printf("%d: wheel slow-fast-slow and stop verified\n",rate);
 }
}
