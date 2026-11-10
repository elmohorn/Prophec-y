#include "p6/P6ArpCharacter.h"
#include "p6/P6Engine.h"
#include "p6/P6PerformanceScheduler.h"
#include <cassert>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>
static std::atomic<bool> tracking{false};
static std::atomic<int> allocations{0};
void* operator new(std::size_t n){if(tracking)++allocations;if(auto*p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void*p) noexcept {std::free(p);}
void operator delete(void*p,std::size_t) noexcept {std::free(p);}
int main() {
 p6::P6ArpCharacter c;c.reset();
 for(int i=0;i<64;i++){auto s=c.next();assert(s.play&&s.velocityScale==1.0&&s.index==i%8);}
 for(int count=0;count<=8;count++){const auto mask=p6::P6ArpCharacter::rhythmMask(count);int n=0;for(int i=0;i<8;i++)n+=(mask>>i)&1;assert(n==count);}
 c.reset();c.setEnabled(true);c.setMask(0x55);c.setMotion(1.0);c.setFreeze(true);
 double reference[8]{};
 tracking=true;
 for(int n=0;n<8000;n++){auto s=c.next();assert(s.play==((s.index%2)==0));if(n<8)reference[s.index]=s.velocityScale;else assert(reference[s.index]==s.velocityScale);}
 tracking=false;assert(allocations==0);
 c.setFreeze(false);bool changed=false;for(int i=0;i<16;i++){auto s=c.next();if(s.velocityScale!=reference[s.index])changed=true;}assert(changed);
 const auto savedSeed=c.variationSeed();
 p6::P6ArpCharacter restored;restored.reset();restored.setEnabled(true);restored.setMotion(1.0);restored.setFreeze(true);restored.restoreVariationSeed(savedSeed);
 c.setFreeze(true);
 for(int i=0;i<8;i++){const auto a=c.next(),b=restored.next();assert(a.index==b.index&&a.velocityScale==b.velocityScale);}
 c.setMotion(NAN);for(int i=0;i<8;i++)assert(c.next().velocityScale==1.0);
 p6::P6Engine engine;engine.prepare(48000,1);
 p6::VoiceParams p;p.osc1.level=.7;p.ampAttack=.001;p.ampRelease=.001;p.ampSustain=1.;engine.setVoiceParams(p);
 p6::P6PerformanceScheduler scheduler(engine);scheduler.prepare(48000);
 scheduler.setClockBpm(120);scheduler.setClockDivision(p6::ClockDivision::Sixteenth);
 scheduler.setArpCharacterEnabled(true);scheduler.setArpCharacterMask(0);
 scheduler.setArpeggiatorEnabled(true);scheduler.noteOn(60,.8);
 tracking=true;
 for(int i=0;i<7000;i++)(void)scheduler.process();
 tracking=false;assert(engine.activeVoiceCount()==0&&allocations==0);
 scheduler.setArpCharacterMask(255);
 for(int i=0;i<7000;i++)(void)scheduler.process();
 assert(engine.activeVoiceCount()>0);
 scheduler.noteOff(60);for(int i=0;i<7000;i++)(void)scheduler.process();
 assert(engine.activeVoiceCount()==0);
}
