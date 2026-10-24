// Independent allocation-payload accounting: no estimator formula is used to
// compute the measured owned or peak bytes. Factory object size is added once.
#include "Source/ReferencePedals/Generic/Chorus.h"
#include "Source/ReferencePedals/Generic/Phaser.h"
#include "Source/ReferencePedals/Generic/Tremolo.h"
#include "Source/ReferencePedals/Generic/Wah.h"
#include "Source/ReferencePedals/Generic/Delay.h"
#include "Source/ReferencePedals/Generic/Reverb.h"
#include "Source/ReferencePedals/Models/boss_ce2.h"
#include "Source/ReferencePedals/Models/boss_ce2w.h"
#include "Source/ReferencePedals/Models/boss_dd3t.h"
#include "Source/ReferencePedals/Models/mxr_phase90_script.h"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <cstddef>
using namespace luthier::reference;
static thread_local bool capture=false;static thread_local uint64_t owned=0,peak=0,calls=0;
struct alignas(std::max_align_t) Header{size_t size;bool counted;};
void* operator new(std::size_t n){auto* h=static_cast<Header*>(std::malloc(sizeof(Header)+(n?n:1)));if(!h)throw std::bad_alloc();h->size=n;h->counted=capture;if(capture){owned+=n;peak=std::max(peak,owned);++calls;}return h+1;}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void* p)noexcept{if(p){auto* h=static_cast<Header*>(p)-1;if(h->counted)owned-=h->size;std::free(h);}}void operator delete[](void* p)noexcept{::operator delete(p);}void operator delete(void* p,std::size_t)noexcept{::operator delete(p);}void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}
static int failures=0;
template<class P>void run(const char* name)
{
 for(double sr:{8000.,44100.,48000.,48000.25,88200.,96000.,192000.,384000.})
 {
  peak=calls=0;
  {
   P p;capture=true;const auto estimate=P::prepareMemory(sr,2048,2,nullptr);capture=false;const auto preflightCalls=calls;
   capture=true;p.prepare(sr,2048,2);const auto reported=p.getPreparedMemoryBytes();capture=false;
   const auto actual=sizeof(P)+owned,actualPeak=sizeof(P)+peak;
   const bool pass=preflightCalls==0 && reported==actual && actual<=estimate.ownedBytes && actualPeak<=estimate.ownedBytes+estimate.temporaryBytes;
   if(!pass)++failures;
   std::printf("%s sr%.2f object%zu estimate_owned%llu temporary%llu reported%llu measured_owned%llu measured_peak%llu preflight_allocations%llu pass%d\n",name,sr,sizeof(P),(unsigned long long)estimate.ownedBytes,(unsigned long long)estimate.temporaryBytes,(unsigned long long)reported,(unsigned long long)actual,(unsigned long long)actualPeak,(unsigned long long)preflightCalls,pass);
   capture=true;p.prepare(8000,1,0);const auto retained=p.getPreparedMemoryBytes();capture=false;
   if(retained!=sizeof(P)+owned)++failures;
   std::printf("%s reprepare8000_from%.2f reported%llu measured_owned%llu\n",name,sr,(unsigned long long)retained,(unsigned long long)(sizeof(P)+owned));
  }
  if(owned!=0)++failures;
 }
}
int main(){run<generic::Chorus>("Chorus");run<generic::Phaser>("Phaser");run<generic::Tremolo>("Tremolo");run<generic::Wah>("Wah");run<generic::Delay>("Delay");run<generic::Reverb>("Reverb");run<models::BossCe2>("CE2");run<models::BossCe2w>("CE2W");run<models::BossDd3t>("DD3T");run<models::MxrPhase90Script>("Phase90");std::printf("failures%d\n",failures);return failures?1:0;}
