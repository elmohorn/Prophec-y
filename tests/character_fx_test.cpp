#include "CharacterFx.h"
#include "CustomCharacter.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

static thread_local bool counting=false;
static thread_local int allocations=0;
void* operator new(std::size_t n) { if(counting)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
#define CHECK(x) do{if(!(x)){std::cerr<<"failed: " #x <<" line "<<__LINE__<<'\n';return 1;}}while(false)

static p6::StereoSample insert(void* p,p6::StereoSample x) noexcept
{ return static_cast<p6plugin::CharacterFx*>(p)->process(x); }
static p6::StereoSample scale(void*,p6::StereoSample x) noexcept {return {x.left*2,x.right*2};}

int main()
{
    for(double sr:{44100.,48000.,96000.}) {
        p6plugin::CharacterFx fx;fx.prepare(sr);p6plugin::CharacterState state;
        fx.setState(state);CHECK(fx.latency()==71);CHECK(!fx.beforeEffects());
        // A pure 71-sample delay in None, including exact zeros and stereo.
        for(int n=0;n<200;++n) {
            const auto out=fx.process({n==0?.5:0.,n==0?-.25:0.});
            CHECK(out.left==(n==71?.5:0.));CHECK(out.right==(n==71?-.25:0.));
        }
        state.type=1;fx.setState(state);
        double energy=0;
        counting=true;
        bool finite=true;
        for(int n=0;n<24000;++n) {
            if(n%1000==0) {state.tube[0]=static_cast<float>(n%7000)/7000.f;state.position=(n/1000)%2;fx.setState(state);}
            fx.advanceRoute();
            const auto out=fx.process({.15*std::sin(n*.043),0.});
            finite=finite&&std::isfinite(out.left)&&std::isfinite(out.right)&&std::abs(out.right)<1e-8;
            energy+=out.left*out.left;
        }
        counting=false;
        CHECK(finite);CHECK(energy>0);CHECK(allocations==0);
        state.type=0;state.position=0;fx.setState(state);
        for(int n=0;n<10000;++n){fx.advanceRoute();fx.process({});}
        CHECK(!fx.beforeEffects());CHECK(fx.routeGain()==1.f);CHECK(fx.excitation()==0.f);
        CHECK(!fx.telemetry().active);
        // Metering uses sample-rate-derived RMS and latency-aligned dry, so
        // None cannot report a waveform change merely because of its delay.
        fx.prepare(sr);state={};fx.setState(state);
        for(int n=0;n<static_cast<int>(sr);++n) {
            const double x=.2*std::sin(2.*3.141592653589793*1000.*n/sr);
            fx.process({x,x});
        }
        const auto dryMeter=fx.telemetry();
        CHECK(std::abs(dryMeter.inputRms-.141421f)<.003f);
        CHECK(std::abs(dryMeter.outputRms-.141421f)<.003f);
        CHECK(dryMeter.changeRms==0.f);
        state.type=1;state.tube[0]=.8f;fx.setState(state);
        for(int n=0;n<static_cast<int>(sr*.5);++n) {
            const double x=.2*std::sin(2.*3.141592653589793*1000.*n/sr);
            fx.advanceRoute();fx.process({x,x});
        }
        CHECK(fx.telemetry().active);CHECK(fx.telemetry().changeRms>1e-5f);
        state.bypass=true;fx.setState(state);CHECK(!fx.telemetry().active);
        for(int n=0;n<static_cast<int>(sr*.5);++n){fx.advanceRoute();fx.process({});}
        CHECK(fx.telemetry().inputRms<1e-5f);CHECK(fx.telemetry().outputRms<1e-5f);
        CHECK(fx.telemetry().changeRms<1e-5f);
        fx.prepare(sr);CHECK(fx.telemetry().inputRms==0.f);CHECK(fx.telemetry().changeRms==0.f);
    }
    // All new devices: deterministic audio, hot-input stability, independent
    // stereo (spring is an intentional coupled tank), zero realtime allocation.
    for(double sr:{44100.,48000.,96000.,192000.})for(int type=2;type<6;++type) {
        p6plugin::CharacterFx a,b;a.prepare(sr);b.prepare(sr);
        p6plugin::CharacterState state;state.type=type;state.custom[static_cast<std::size_t>(type-2)][5]=1;
        a.setState(state);b.setState(state);double energy=0;bool valid=true;
        counting=true;
        for(int n=0;n<12000;++n){
            const double x=n<8000?.2*std::sin(n*.11):0.;
            a.advanceRoute();b.advanceRoute();const auto y=a.process({x,0}),z=b.process({x,0});
            valid=valid&&y.left==z.left&&y.right==z.right&&std::isfinite(y.left)&&std::abs(y.left)<32;
            if(type!=3)valid=valid&&std::abs(y.right)<1.e-8;
            energy+=y.left*y.left;
        }
        counting=false;CHECK(valid);CHECK(allocations==0);CHECK(energy>1.e-5);CHECK(a.telemetry().type==type);
        // Bypass and Mix=0 must null the exact latency-aligned input, no tail.
        for(int mode=0;mode<2;++mode){
            state.bypass=mode==0;state.custom[static_cast<std::size_t>(type-2)][5]=mode==1?0.f:1.f;a.setState(state);
            for(int n=0;n<static_cast<int>(sr*.04);++n){a.advanceRoute();a.process({});}
            for(int n=0;n<180;++n){a.advanceRoute();const auto y=a.process({n==0?.7:0.,n==0?-.3:0.});CHECK(y.left==(n==71?.7:0.));CHECK(y.right==(n==71?-.3:0.));}
        }
        state.bypass=false;state.custom[static_cast<std::size_t>(type-2)][5]=1;
        counting=true;
        for(int n=0;n<16000;++n){
            if(n%257==0){state.type=2+(n/257)%4;state.position=(n/257)%2;for(auto& device:state.custom)for(auto& v:device)v=static_cast<float>((n/257)%3)*.5f;a.setState(state);}
            a.advanceRoute();const auto y=a.process({8.*std::sin(n*.073),-7.*std::sin(n*.17)});
            valid=valid&&std::isfinite(y.left)&&std::isfinite(y.right)&&std::abs(y.left)<64&&std::abs(y.right)<64;
        }
        counting=false;CHECK(valid);CHECK(allocations==0);
    }
    // Spring pickups must carry a decaying impulse tail rather than dry audio.
    {
        p6plugin::CharacterFx spring;spring.prepare(48000);p6plugin::CharacterState state;state.type=3;
        state.custom[1][1]=.15f;state.custom[1][5]=1;spring.setState(state);
        for(int n=0;n<1000;++n){spring.advanceRoute();spring.process({});}
        double early=0,late=0;
        for(int n=0;n<144000;++n){const auto y=spring.process({n==0?1.:0.,0});if(n>1000&&n<20000)early+=y.left*y.left;if(n>120000)late+=y.left*y.left;}
        CHECK(early>1.e-5);CHECK(late<early*.001);
    }
    // Bias and asymmetric gain must remain continuous around zero.
    for(float drive:{0.f,.42f,1.f})for(float folds:{0.f,.42f,1.f})
        for(float symmetry:{0.f,.42f,1.f})for(float bias:{0.f,.42f,1.f}) {
            auto v=p6plugin::kCustomDefaults[3];v[0]=drive;v[1]=folds;v[2]=symmetry;v[3]=bias;
            const auto a=p6plugin::CustomCharacter::neonTransfer(-1.e-7f,v);
            const auto b=p6plugin::CustomCharacter::neonTransfer(1.e-7f,v);
            CHECK(std::abs(a-b)<.0001f);
        }
    // The unchanged engine and the None wrapper must null after exactly the
    // declared delay; do not compare only RMS/spectrum.
    p6::P6Engine a,b;a.prepare(48000,1);b.prepare(48000,1);
    a.setDistortion(.4);b.setDistortion(.4);
    p6plugin::CharacterFx fx;fx.prepare(48000);fx.setState({});
    b.setCharacterInsert(&fx,insert,false);
    a.noteOn(60,.6);b.noteOn(60,.6);
    std::vector<p6::StereoSample> reference(2048);
    for(int n=0;n<2048;++n) {
        reference[static_cast<std::size_t>(n)]=a.process();const auto out=b.process();
        const auto expected=n<71?p6::StereoSample{}:reference[static_cast<std::size_t>(n-71)];
        CHECK(out.left==expected.left&&out.right==expected.right);
    }
    // Nonlinear distortion makes pre/post order observable, even with a very
    // simple external insert. No DSPark-specific property can mask this test.
    p6::P6Engine pre,post;pre.prepare(48000,1);post.prepare(48000,1);
    pre.setDistortion(.8);post.setDistortion(.8);
    pre.setCharacterInsert(nullptr,scale,true);post.setCharacterInsert(nullptr,scale,false);
    pre.noteOn(60,.8);post.noteOn(60,.8);
    double difference=0;
    for(int n=0;n<2048;++n) difference+=std::abs(pre.process().left-post.process().left);
    CHECK(difference>1e-4);
    std::cout<<"Character FX delay, stereo, routing, transitions and ordinary-new checks passed\n";
}

