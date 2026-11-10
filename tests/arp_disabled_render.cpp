#include "p6/P6Engine.h"
#include "p6/P6PerformanceScheduler.h"
#include <bit>
#include <cstdint>
#include <cstdio>
int main(){
 p6::P6Engine engine;engine.prepare(48000,2);
 p6::VoiceParams params;params.osc1.level=.7;params.osc1.shape=.5;params.ampSustain=.8;
 engine.setVoiceParams(params);
 p6::P6PerformanceScheduler scheduler(engine);scheduler.prepare(48000);
 scheduler.setClockBpm(120);scheduler.setClockDivision(p6::ClockDivision::Sixteenth);
 scheduler.setArpeggiatorEnabled(true);
 scheduler.noteOn(60,.8);scheduler.noteOn(64,.7);scheduler.noteOn(67,.9);
 std::uint64_t hash=1469598103934665603ull;
 for(int i=0;i<30000;i++){
  if(i==14000)scheduler.noteOff(64);
  if(i==20000)scheduler.noteOn(69,.6);
  auto x=scheduler.process();
  hash=(hash^std::bit_cast<std::uint64_t>(x.left))*1099511628211ull;
  hash=(hash^std::bit_cast<std::uint64_t>(x.right))*1099511628211ull;
 }
 std::printf("%016llx\n",static_cast<unsigned long long>(hash));
}
