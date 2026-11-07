// Offline listening/level experiment, no changes to plug-in state or DSP.
#include "CharacterFx.h"
#include "p6/P6Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
int main(int argc, char** argv) {
 if (argc!=4) return 2;
 const std::string variant=argv[1]; const int sr=std::stoi(argv[2]); const std::string dir=argv[3];
 constexpr double pi=3.14159265358979323846; const int frames=sr*2;
 std::cout<<std::setprecision(9);
 for(int scenario=0;scenario<7;++scenario) {
  p6::P6Engine engine;engine.prepare(sr,2);std::vector<p6::StereoSample> in(frames);
  for(int n=0;n<frames;++n){double t=double(n)/sr;
   if(scenario<5){const double amp=scenario==0?.02:scenario==1?.04:scenario==2?.05:scenario==3?.1:.12*t/2.;const double x=amp*std::sin(2*pi*440*t);in[n]={x,x};}
   else{if(n==0){engine.noteOn(48,.8);if(scenario==6){engine.noteOn(55,.8);engine.noteOn(60,.8);engine.noteOn(64,.8);}}if(n==sr*3/2){engine.noteOff(48);if(scenario==6){engine.noteOff(55);engine.noteOff(60);engine.noteOff(64);}}auto x=engine.process();in[n]={x.left*.1,x.right*.1};}
  }
  p6plugin::CharacterFx fx;fx.prepare(sr);p6plugin::CharacterState state;state.type=2;
  if(variant=="new_defaults"){state.custom[0][1]=.10f;state.custom[0][5]=.5f;state.custom[0][6]=.35f;}
  if(variant=="hybrid")state.custom[0][5]=.5f;
  fx.setState(state);double ip=0,op=0,pk=0,step=0,prev=0;std::ofstream file;
  if(sr==48000&&(scenario==4||scenario==6))file.open(dir+"/"+variant+"_"+(scenario==4?"ramp":"chord")+".f32",std::ios::binary);
  for(int n=0;n<frames;++n){fx.advanceRoute();auto y=fx.process(in[n]);if(!std::isfinite(y.left)||!std::isfinite(y.right))return 1;if(n>=sr/4){ip+=(in[n].left*in[n].left+in[n].right*in[n].right)*.5;op+=(y.left*y.left+y.right*y.right)*.5;pk=std::max({pk,std::abs(y.left),std::abs(y.right)});step=std::max(step,std::abs(y.left-prev));}prev=y.left;if(file){float s[2]{float(y.left),float(y.right)};file.write(reinterpret_cast<char*>(s),sizeof s);}}
  const char* names[]={"sine_002","sine_004","sine_005","sine_010","ramp","single_note","chord"};
  std::cout<<variant<<','<<sr<<','<<names[scenario]<<','<<std::sqrt(ip/(frames-sr/4))<<','<<std::sqrt(op/(frames-sr/4))<<','<<pk<<','<<step<<'\n';
 }
}
