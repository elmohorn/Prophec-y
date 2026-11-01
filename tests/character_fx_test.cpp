#include "CharacterFx.h"
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
