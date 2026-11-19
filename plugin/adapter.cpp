#include "adapter.h"
#include <algorithm>
#include <cmath>
extern "C" {
#include "../third_party/setBfree/b_whirl/whirl.h"
void useMIDIControlFunction(void*,const char*,void(*)(void*,unsigned char),void*){}
void notifyControlChangeByName(void*,const char*,unsigned char){}
int getConfigParameter_d(const char*,ConfigContext*,double*){return 0;}
int getConfigParameter_i(const char*,ConfigContext*,int*){return 0;}
int getConfigParameter_dr(const char*,ConfigContext*,double*,double,double){return 0;}
int getConfigParameter_ir(const char*,ConfigContext*,int*,int,int){return 0;}
}
namespace {float norm(float x){return std::isfinite(x)?std::clamp(x,0.f,1.f):0.f;}}
RotarySetBfree::RotarySetBfree()=default;
RotarySetBfree::~RotarySetBfree(){if(w_)freeWhirl(w_);}
bool RotarySetBfree::prepare(double sr){if(!(sr>=8000&&sr<=192000))return false;if(w_)freeWhirl(w_);w_=allocWhirl();if(!w_)return false;initWhirl(w_,nullptr,sr);phase_=phaseDrum_=smoothWheel_=smoothMotion_=0;setSpeed(Slow);return true;}
void RotarySetBfree::setSpeed(Speed s)noexcept{speed_=s;if(w_)setRevSelect(w_,s==Fast?WHIRL_FAST:s==Stop?WHIRL_STOP:WHIRL_SLOW);}
void RotarySetBfree::setMotion(float x)noexcept{motion_=norm(x);}
void RotarySetBfree::setModWheel(float x)noexcept{wheel_=norm(x);}
void RotarySetBfree::setWheelDepth(float x)noexcept{depth_=norm(x);}
void RotarySetBfree::updateMotion(std::size_t n)noexcept{
 if (motion_==0.f && depth_==0.f && smoothMotion_==0. && smoothWheel_==0.) return;
 const double dt=double(n)/w_->SampleRateD;
 const double a=1-std::exp(-dt/.06);smoothWheel_+=(wheel_*depth_-smoothWheel_)*a;smoothMotion_+=(motion_-smoothMotion_)*a;
 phase_+=dt*.21*6.283185307179586;if(phase_>=6.283185307179586)phase_-=6.283185307179586;
 phaseDrum_+=dt*.1533*6.283185307179586;if(phaseDrum_>=6.283185307179586)phaseDrum_-=6.283185307179586;
 if(speed_==Stop){w_->hornTarget=w_->drumTarget=0;}
 else {
  const double blend=speed_==Fast?1.:std::clamp(smoothWheel_,0.,1.);
  const double horn=(w_->hornRPMslow+(w_->hornRPMfast-w_->hornRPMslow)*blend)/(60.*w_->SampleRateD);
  const double drum=(w_->drumRPMslow+(w_->drumRPMfast-w_->drumRPMslow)*blend)/(60.*w_->SampleRateD);
  w_->hornTarget=horn*(1.+.08*smoothMotion_*std::sin(phase_));
  w_->drumTarget=drum*(1.+.05*smoothMotion_*std::sin(phaseDrum_+1.3));
 }
 // Re-enable motor updates even if setBfree reached its previous target.
 w_->hornAcDc=w_->hornTarget>w_->hornIncr?1:-1;
 w_->drumAcDc=w_->drumTarget>w_->drumIncr?1:-1;
}
void RotarySetBfree::process(const float* in,float* l,float* r,std::size_t n)noexcept{if(!w_)return;while(n){auto k=std::min<std::size_t>(n,64);updateMotion(k);whirlProc(w_,in,l,r,k);in+=k;l+=k;r+=k;n-=k;}}
double RotarySetBfree::hornHz()const noexcept{return w_?w_->hornIncr*w_->SampleRateD:0;}
double RotarySetBfree::drumHz()const noexcept{return w_?w_->drumIncr*w_->SampleRateD:0;}

double RotarySetBfree::hornAngle()const noexcept{return w_?w_->hornAngleGRD:0;}
double RotarySetBfree::drumAngle()const noexcept{return w_?w_->drumAngleGRD:0;}
