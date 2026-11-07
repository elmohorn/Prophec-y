#include "CharacterFx.h"
#include <array>
#include <cmath>
#include <chrono>
#include <iostream>
int main(){constexpr int sr=48000,count=480000;std::array<double,sr> input{};for(int i=0;i<sr;++i)input[i]=.08*std::sin(6.283185307179586*440*i/sr);for(int run=0;run<3;++run){p6plugin::CharacterFx fx;fx.prepare(sr);p6plugin::CharacterState state;state.type=2;state.custom[0][5]=.5f;fx.setState(state);double sum=0;auto start=std::chrono::steady_clock::now();for(int i=0;i<count;++i){auto v=input[i%sr];fx.advanceRoute();auto y=fx.process({v,-v});sum+=y.left;}double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();std::cout<<sec<<' '<<(100*sec/(double(count)/sr))<<' '<<sum<<'\n';}}
