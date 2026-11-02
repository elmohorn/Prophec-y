#define main probe_main_do_not_call
#include "static_probe.cpp"
#undef main
int main(){
 constexpr int n=1000000;double sink=0;
 std::cout<<"shape,method,ns_per_sample,checksum\n";
 for(double sh:{0.,.5,1.})for(int mode=0;mode<2;++mode){
  p6::PolyBLEPOscillator osc;osc.prepare(48000);osc.reset(.25);osc.setFrequency(997);osc.setShape(sh);osc.setPulseWidth(.5);
  EllipticOsc alt(48000,997,sh,.5);double sum=0;
  for(int i=0;i<10000;++i)sum+=mode?alt.process({}):osc.process();
  auto start=std::chrono::steady_clock::now();
  for(int i=0;i<n;++i)sum+=mode?alt.process({}):osc.process();
  double ns=1e9*std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/n;
  sink+=sum;std::cout<<sh<<','<<(mode?"elliptic":"current")<<','<<ns<<','<<sum<<'\n';
 }
 return !std::isfinite(sink);
}
