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
 c.setMotion(NAN);for(int i=0;i<8;i++)assert(c.next().velocityScale==1.0);
}
