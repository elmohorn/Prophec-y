#include "CustomCharacter.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
// Exploratory convergence against a 4x host-rate reference, not an alias-only metric.
int main(int argc,char** argv)
{
    if(argc!=2){std::cerr<<"Usage: p6_neon_rate_probe OUTPUT_DIRECTORY\n";return 1;}
    const std::filesystem::path root(argv[1]);std::filesystem::create_directories(root);
    for(int sr:{48000,192000})for(int freq:{251,997,4001}) {
        p6plugin::CustomCharacter fx;fx.prepare(sr,71);auto v=p6plugin::kCustomDefaults[3];
        v[0]=.8f;v[1]=.8f;v[2]=.42f;v[3]=.42f;v[4]=1.f;
        std::ofstream out(root/("neon_"+std::to_string(sr)+"_"+std::to_string(freq)+".f32"),std::ios::binary);
        if(!out)return 2;
        for(int n=0;n<sr+71;++n){const auto x=.3*std::sin(6.283185307179586*freq*n/sr);const auto y=fx.process({x,x},5,v);if(n>=71){const float value=static_cast<float>(y.left);out.write(reinterpret_cast<const char*>(&value),sizeof(value));}}
        if(!out)return 3;
    }
}
