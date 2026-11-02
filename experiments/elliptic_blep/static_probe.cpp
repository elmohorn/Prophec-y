// EXPERIMENT ONLY: static trapezoid geometry, hard-sync prototype.
// Not feature-equivalent under moving shape/PW; see README.
#include "p6/PolyBLEPOscillator.h"
#include "elliptic-blep.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

struct EllipticOsc {
 double sr,phase=.25,frequency,shape,duty,held=0; signalsmith::blep::EllipticBlep<double> blep;
 EllipticOsc(double rate,double hz,double sh,double pw):sr(rate),frequency(hz),shape(sh),duty(pw),blep(rate){}
 double value(double p)const{return p6::PolyBLEPOscillator::nominalWaveform(p,shape,duty,1);}
 static double wrap(double p){return p-std::floor(p);}
 double slope(double p)const {
  constexpr double e=1e-6;double rise,high,fall;
  if(shape<.5){rise=.5+shape*(1.-2*e);high=0;fall=1-rise;}
  else {double b=(shape-.5)*2, span=1-2*e;rise=(1-e)-b*span;high=b*duty*span;fall=e;}
  double mean=high-(1-rise-high-fall),scale=1/(1+std::abs(mean));
  if(p<rise)return 2/rise*scale;if(p<rise+high)return 0;if(p<rise+high+fall)return -2/fall*scale;return 0;
 }
 void corners(double start,double span,double t0,double t1,double dt){
  if(span<=0)return;constexpr double e=1e-6;double rise,high,fall;
  if(shape<.5){rise=.5+shape*(1.-2*e);high=0;fall=1-rise;}
  else{double b=(shape-.5)*2,span2=1-2*e;rise=(1-e)-b*span2;high=b*duty*span2;fall=e;}
  const std::array<double,4> edges{0.,rise,rise+high,wrap(rise+high+fall)};
  for(size_t i=0;i<edges.size();++i){bool repeated=false;for(size_t j=0;j<i;++j)if(std::abs(edges[i]-edges[j])<1e-12)repeated=true;if(repeated)continue;
   const double x=edges[i],reached=wrap(x-start);if(reached>=span || (t0>0 && reached<1e-12))continue;
   const double u=t0+(reached/span)*(t1-t0),eps=1e-10;
   const double delta=(slope(wrap(x+eps))-slope(wrap(x-eps)))*dt;
   blep.add(delta,2,std::clamp(1-u,0.,.999999999));
  }
 }
 double process(std::optional<double> sync){
  double dt=frequency/sr;blep.step();double end;
  if(sync){double u=*sync;corners(phase,dt*u,0,u,dt);double p=wrap(phase+dt*u);
   blep.add(value(0)-value(p),1,std::clamp(1-u,0.,.999999999));
   blep.add((slope(0)-slope(p))*dt,2,std::clamp(1-u,0.,.999999999));
   corners(0,dt*(1-u),u,1,dt);end=wrap(dt*(1-u));
  }else{corners(phase,dt,0,1,dt);end=wrap(phase+dt);}
  double result=held;held=value(end)+blep.get();phase=end;return result;
 }
};
int main(int argc,char** argv){if(argc!=8){std::cerr<<"file mode sr hz shape duty sync_ratio\n";return 2;}
 std::string file=argv[1],mode=argv[2];double sr=std::stod(argv[3]),hz=std::stod(argv[4]),shape=std::stod(argv[5]),duty=std::stod(argv[6]),ratio=std::stod(argv[7]);
 p6::PolyBLEPOscillator current;current.prepare(sr);current.setFrequency(hz);current.setShape(shape);current.setPulseWidth(duty);current.reset(.25);
 EllipticOsc alt(sr,hz,shape,duty);double master=0;std::ofstream f(file,std::ios::binary);if(!f)return 1;auto start=std::chrono::steady_clock::now();
 for(int i=0;i<static_cast<int>(sr*1.5);++i){std::optional<double> sync;double md=hz*ratio/sr;if(ratio>0 && master+md>=1)sync=(1-master)/md;master=EllipticOsc::wrap(master+md);double y;
 if(mode=="current")y=current.process(sync);else if(mode=="elliptic")y=alt.process(sync);else {double dt=hz/sr;if(sync)alt.phase=EllipticOsc::wrap(dt*(1-*sync));else alt.phase=EllipticOsc::wrap(alt.phase+dt);y=alt.value(alt.phase);}
 if(!std::isfinite(y)||std::abs(y)>100){std::cerr<<"unstable sample "<<i<<" value "<<y<<"\n";return 1;}f.write(reinterpret_cast<const char*>(&y),sizeof y);}
 std::cerr<<mode<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\n";return !f;
}
