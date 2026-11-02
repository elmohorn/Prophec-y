// Offline, reproducible audition material: identical core performance into each
// Character setting. Raw floats are converted to WAV with one shared gain.
#include "CharacterFx.h"
#include "p6/P6Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if(argc != 2){std::cerr << "usage: tube_plasma_audition OUTPUT_DIRECTORY\n";return 2;}
    constexpr int sr=48000, seconds=8;
    std::vector<p6::StereoSample> input;input.reserve(sr*seconds);
    p6::P6Engine engine;engine.prepare(sr,2);
    constexpr std::array<int,8> notes{48,55,60,63,67,63,55,48};
    for(int n=0;n<sr*seconds;++n){
        const int beat=n/sr, offset=n%sr;
        if(offset==0)engine.noteOn(notes[static_cast<std::size_t>(beat)],.8);
        if(offset==sr*3/4)engine.noteOff(notes[static_cast<std::size_t>(beat)]);
        auto x=engine.process();input.push_back({x.left*.25,x.right*.25});
    }
    const std::array<std::string,5> names{"dry","tube_default","tube_driven","plasma_default","plasma_driven"};
    for(std::size_t scenario=0;scenario<names.size();++scenario){
        p6plugin::CharacterFx fx;fx.prepare(sr);p6plugin::CharacterState s;
        s.type=scenario==0?0:scenario<3?1:2;
        if(scenario==2){s.tube[0]=.65f;s.tube[1]=.6f;}
        if(scenario==4){auto& v=s.custom[0];v[0]=.65f;v[1]=.35f;v[2]=.55f;v[3]=.6f;}
        fx.setState(s);std::ofstream out(std::string(argv[1])+"/"+names[scenario]+".f32",std::ios::binary);
        if(!out){std::cerr << "cannot open output\n";return 1;}
        for(auto x:input){fx.advanceRoute();const auto y=fx.process(x);const float frame[2]{static_cast<float>(y.left),static_cast<float>(y.right)};if(!std::isfinite(frame[0])||!std::isfinite(frame[1]))return 1;out.write(reinterpret_cast<const char*>(frame),sizeof frame);}
        if(!out)return 1;
    }
}
