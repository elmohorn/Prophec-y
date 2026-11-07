#include "CharacterFx.h"
#include <cmath>
#include <fstream>
int main(int argc,char**argv){if(argc!=2)return 2;constexpr int sr=48000;const double f=10099.;p6plugin::CharacterFx fx;fx.prepare(sr);p6plugin::CharacterState st;st.type=2;st.custom[0][5]=.5f;fx.setState(st);std::ofstream out(argv[1],std::ios::binary);for(int i=0;i<sr*2;++i){double x=.1*std::sin(6.283185307179586*f*i/sr);fx.advanceRoute();auto y=fx.process({x,x});if(i>=sr)out.write(reinterpret_cast<const char*>(&y.left),sizeof(double));}return !out;}
