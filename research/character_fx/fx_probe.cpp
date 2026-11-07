// Prophec-y research harness. No upstream DSP is compiled into the plugin.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include "upstream_adapters.h"
#include "DSPark/Effects/TubePreamp.h"
#include "Patina/dsp/circuits/saturation/TubePreamp.h"
#include "DaisySP/Source/Effects/overdrive.h"

thread_local bool countAlloc=false;
thread_local size_t allocations=0;
void* operator new(std::size_t n){if(countAlloc)++allocations;if(void* p=std::malloc(std::max(n,size_t(1))))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
struct Adapter {
 virtual ~Adapter()=default; virtual void drive(float)=0;virtual void reset()=0;
 virtual void process(float**,int)=0;virtual int latency()const{return 0;}
};
struct Air:Adapter{Tube2 p;explicit Air(double sr){p.sr=sr;}void drive(float d)override{p.B=d;}void reset()override{float b=p.B;double sr=p.sr;p=Tube2{};p.sr=sr;p.B=b;}void process(float** x,int n)override{p.processReplacing(x,x,n);}};
struct Daisy:Adapter{daisysp::Overdrive p[2];Daisy(){for(auto& z:p)z.Init();}void drive(float d)override{for(auto& z:p)z.SetDrive(d);}void reset()override{}void process(float** x,int n)override{for(int c=0;c<2;++c)for(int i=0;i<n;++i)x[c][i]=p[c].Process(x[c][i]);}};
struct Patina:Adapter{TubePreamp p;TubePreamp::Params v;explicit Patina(double sr){p.prepare(2,sr);v.outputLevel=1;v.bias=.5f;}void drive(float d)override{v.drive=d;}void reset()override{p.reset();}void process(float** x,int n)override{p.processBlock(x,2,n,v);}};
struct Ox:Adapter{oxfx::valve_color_state_t s[2]{};oxfx::fx_studio_instance_t p[2];float sr;explicit Ox(double f):sr(f){for(int c=0;c<2;++c)p[c]={&s[c],{.5f,.5f,1,1}};}void drive(float d)override{for(auto& z:p)z.params[0]=d;}void reset()override{for(auto& z:s)z={};}void process(float** x,int n)override{for(int c=0;c<2;++c)oxfx::valve_color_process(&p[c],x[c],n,sr);}};
struct DS:Adapter{dspark::TubePreamp<float> p;explicit DS(double sr){p.setStages(1);p.setMix(1);p.setSag(.3f);p.prepare({sr,1024,2});}void drive(float d)override{p.setDrive(-12+48*d);}void reset()override{p.reset();}void process(float** x,int n)override{p.processBlock(dspark::AudioBufferView<float>(x,2,n));}int latency()const override{return p.getLatency();}};
std::unique_ptr<Adapter> create(const std::string& id,double sr){if(id=="airwindows")return std::make_unique<Air>(sr);if(id=="dspark")return std::make_unique<DS>(sr);if(id=="patina")return std::make_unique<Patina>(sr);if(id=="daisy")return std::make_unique<Daisy>();if(id=="0xfx")return std::make_unique<Ox>(sr);throw std::runtime_error("unknown candidate");}
std::array<std::vector<float>,2> signal(const std::string& kind,double sr,int n,int offset=0){
 std::array<std::vector<float>,2> x;for(auto& c:x)c.resize(n);
 const int bin=2*int(std::round((kind=="hf"?15000.:1000.)*32768/sr/2))+1;
 const double f=double(bin)*sr/32768,amp=kind=="tone_low"?.12589254:.50118723;
 for(int i=0;i<n;++i){double y=0,t=double(i+offset)/sr;
  if(kind=="impulse")y=i+offset==128?.5:0;
  else if(kind!="silence"&&kind!="chord")y=amp*std::sin(2*M_PI*f*t);
  else if(kind=="chord"){for(double h:{130.8128,164.8138,195.9977})for(int k=1;k<=8;++k)y+=std::sin(2*M_PI*h*k*t)/k;y*=.075;}
  x[0][i]=float(y);x[1][i]=kind=="left_only"?0:float(y);
 }return x;
}
struct Timing{double totalUs=0,worstUs=0;size_t alloc=0;};
Timing render(Adapter& a,std::array<std::vector<float>,2>& x,int block){Timing r;const int n=int(x[0].size());
 for(int pos=0;pos<n;pos+=block){int count=std::min(block,n-pos);float* ptr[2]={x[0].data()+pos,x[1].data()+pos};allocations=0;countAlloc=true;
 auto t=std::chrono::steady_clock::now();a.process(ptr,count);auto end=std::chrono::steady_clock::now();countAlloc=false;
 double us=std::chrono::duration<double,std::micro>(end-t).count();r.totalUs+=us;r.worstUs=std::max(r.worstUs,us);r.alloc+=allocations;
 }return r;
}
void write(const std::filesystem::path& p,const std::array<std::vector<float>,2>& x){std::ofstream f(p,std::ios::binary);for(size_t i=0;i<x[0].size();++i){f.write(reinterpret_cast<const char*>(&x[0][i]),4);f.write(reinterpret_cast<const char*>(&x[1][i]),4);}}
int main(int argc,char**argv){try{
 if(argc<2)throw std::runtime_error("output directory required");std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 std::ofstream csv(out/"runs.csv");csv<<"candidate,sample_rate,dial,signal,frames,block,latency_samples,prepare_ms,cpu_percent_one_core,worst_block_us,new_allocations,nonfinite,peak,file\n";
 constexpr int N=32768;for(std::string id:{"airwindows","dspark","patina","daisy","0xfx"})for(double sr:{44100.,48000.,96000.}){
 auto start=std::chrono::steady_clock::now();auto a=create(id,sr);double prep=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 for(float d:{.2f,.5f,.8f})for(std::string kind:{"silence","tone_low","tone_hot","hf","impulse","left_only","chord"}){
 a->drive(d);a->reset();auto warm=signal(kind=="impulse"?"silence":kind,sr,int(sr*.25),-int(sr*.25));render(*a,warm,128);
 auto x=signal(kind,sr,N);auto t=render(*a,x,128);size_t bad=0;double peak=0;for(auto& c:x)for(float v:c){bad+=!std::isfinite(v);if(std::isfinite(v))peak=std::max(peak,std::abs(double(v)));}
 std::string name=id+"_"+std::to_string(int(sr))+"_d"+std::to_string(int(std::round(d*10)))+"_"+kind+".f32";write(out/name,x);
 csv<<id<<','<<sr<<','<<d<<','<<kind<<','<<N<<",128,"<<a->latency()<<','<<prep<<','<<(t.totalUs/1e6/(N/sr)*100)<<','<<t.worstUs<<','<<t.alloc<<','<<bad<<','<<peak<<','<<name<<'\n';csv.flush();
 }
 std::cout<<id<<' '<<sr<<" complete; prepare "<<prep<<" ms\n"<<std::flush;
 }
 // Paired block-size experiment with fresh instances and identical input.
 std::ofstream edge(out/"block_size.csv");edge<<"candidate,block_a,block_b,max_absolute_difference,rms_difference\n";
 for(std::string id:{"airwindows","dspark","patina","daisy","0xfx"}){
  std::array<std::array<std::vector<float>,2>,2> yy;
  for(int b=0;b<2;++b){auto a=create(id,48000);a->drive(.5f);a->reset();auto warm=signal("chord",48000,12000,-12000);render(*a,warm,b?1024:16);yy[b]=signal("chord",48000,32768);render(*a,yy[b],b?1024:16);}
  double peak=0,energy=0;for(int c=0;c<2;++c)for(int i=0;i<32768;++i){double d=double(yy[0][c][i])-yy[1][c][i];peak=std::max(peak,std::abs(d));energy+=d*d;}
  edge<<id<<",16,1024,"<<peak<<','<<std::sqrt(energy/65536)<<'\n';
 }
 // Parameter transition recorded separately; native smoothing is not added by the adapter.
 for(std::string id:{"airwindows","dspark","patina","daisy","0xfx"}){
  auto a=create(id,48000);a->drive(.2f);a->reset();auto warm=signal("tone_hot",48000,24000,-24000);render(*a,warm,128);
  auto x=signal("tone_hot",48000,48000);for(int pos=0;pos<48000;pos+=128){if(pos==24064)a->drive(.8f);int n=std::min(128,48000-pos);float* ptr[]={x[0].data()+pos,x[1].data()+pos};a->process(ptr,n);}write(out/(id+"_automation.f32"),x);
 }
 if(argc>2){
  std::ifstream file(argv[2],std::ios::binary);std::vector<float> interleaved((std::filesystem::file_size(argv[2]))/4);file.read(reinterpret_cast<char*>(interleaved.data()),interleaved.size()*4);
  for(std::string id:{"airwindows","dspark","patina","daisy","0xfx"}){
   auto a=create(id,48000);a->drive(.5f);a->reset();auto warm=signal("silence",48000,12000);render(*a,warm,128);
   std::array<std::vector<float>,2> x;const size_t n=interleaved.size()/2;for(auto& ch:x)ch.resize(n+512);
   for(size_t i=0;i<n;++i)for(int c=0;c<2;++c)x[c][i]=interleaved[i*2+c];render(*a,x,128);
   write(out/(id+"_musical.f32"),x);
  }
 }
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
