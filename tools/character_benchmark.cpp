#include "CharacterFx.h"
#include "p6/P6Engine.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

static p6::StereoSample insert(void* ctx,p6::StereoSample sample) noexcept
{ return static_cast<p6plugin::CharacterFx*>(ctx)->process(sample); }

int main()
{
    using Clock=std::chrono::steady_clock;
    std::cout << "device,sample_rate,block_size,mean_us,p99_us,max_us,budget_us,p99_budget_percent,checksum\n";
    for(double sr:{48000.,96000.})for(int blockSize:{32,64,128})for(int type=0;type<6;++type) {
        p6::P6Engine engine;engine.prepare(sr,2);
        p6plugin::CharacterFx fx;fx.prepare(sr);p6plugin::CharacterState state;state.type=type;
        if(type>=2){auto& v=state.custom[static_cast<std::size_t>(type-2)];v[0]=.8f;v[1]=.8f;v[5]=1.f;}
        fx.setState(state);engine.setCharacterInsert(&fx,insert,false);
        for(int note:{48,52,55,60,64,67})engine.noteOn(note,.8);
        constexpr int blocks=250;std::vector<double> durations;durations.reserve(blocks);double checksum=0;
        for(int n=0;n<static_cast<int>(sr*.1);++n){fx.advanceRoute();(void)engine.process();}
        for(int block=0;block<blocks;++block){
            const auto start=Clock::now();
            for(int n=0;n<blockSize;++n){fx.advanceRoute();const auto s=engine.process();checksum+=s.left+s.right;}
            durations.push_back(std::chrono::duration<double,std::micro>(Clock::now()-start).count());
        }
        double total=0;for(double x:durations)total+=x;
        std::sort(durations.begin(),durations.end());const double p99=durations[static_cast<std::size_t>(blocks*99/100)];
        const double budget=1.e6*blockSize/sr;
        if(!std::isfinite(checksum))return 1;
        std::cout << p6plugin::kCharacterNames[static_cast<std::size_t>(type)] << ',' << sr << ',' << blockSize << ','
                  << std::fixed << std::setprecision(3) << total/blocks << ',' << p99 << ',' << durations.back() << ','
                  << budget << ',' << 100.*p99/budget << ',' << checksum << '\n';
    }
}
