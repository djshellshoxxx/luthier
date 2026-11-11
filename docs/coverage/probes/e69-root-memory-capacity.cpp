// Independent logical allocation-payload oracle. Estimator formulas are never
// used to derive measured owned or peak bytes. No allocator metadata/RSS claim.
#include "Source/ReferencePedals/PedalRegistry.h"
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <cstddef>
using namespace luthier::reference;
static thread_local bool capture = false;
static thread_local uint64_t owned = 0, peak = 0, calls = 0;
struct alignas(std::max_align_t) Header { size_t size; bool counted; };
void* operator new(std::size_t n) { auto* h=static_cast<Header*>(std::malloc(sizeof(Header)+(n?n:1))); if(!h)throw std::bad_alloc(); h->size=n;h->counted=capture;if(capture){owned+=n;peak=std::max(peak,owned);++calls;}return h+1; }
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {if(p){auto* h=static_cast<Header*>(p)-1;if(h->counted)owned-=h->size;std::free(h);}}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
void* operator new(std::size_t n,const std::nothrow_t&) noexcept {try{return ::operator new(n);}catch(...){return nullptr;}}
void* operator new[](std::size_t n,const std::nothrow_t&) noexcept {try{return ::operator new[](n);}catch(...){return nullptr;}}
void operator delete(void* p,const std::nothrow_t&) noexcept {::operator delete(p);}
void operator delete[](void* p,const std::nothrow_t&) noexcept {::operator delete[](p);}
int main()
{
 PedalRegistry::registerAll(); int failures=0,cases=0;
 for(const auto* desc:PedalRegistry::all())
  for(double rate:{8000.,44100.,48000.,48000.25,88200.,96000.,192000.,384000.})
   for(int policy:{0,1,2})
    for(int setting:{-1,0,1})
    {
     std::array<float,kMaxParams> params{};
     for(int i=0;i<desc->numParams;++i)params[i]=setting<0?float(desc->params[i].defaultNormalised()):float(setting);
     peak=calls=0;capture=true;auto estimate=PedalRegistry::memoryRequirement(desc->modelId,rate,2048,policy,params.data());capture=false;
     const auto preflightCalls=calls;
     capture=true;auto pedal=PedalRegistry::make(desc->modelId);
     for(int i=0;i<desc->numParams;++i)pedal->setParam(i,params[i]);
     pedal->prepare(rate,2048,policy);const auto beforeReport=calls;auto actual=pedal->getPreparedMemoryBytes();const auto reportCalls=calls-beforeReport;capture=false;
     const auto measured=owned,measuredPeak=peak;uint64_t estimatedPeak=0;
     bool pass=preflightCalls==0 && reportCalls==0 && actual==measured && actual<=estimate.ownedBytes && addPreparedBytes(estimate.ownedBytes,estimate.temporaryBytes,estimatedPeak) && measuredPeak<=estimatedPeak;
     capture=true;pedal->prepare(8000,1,0);const auto retained=pedal->getPreparedMemoryBytes();capture=false;
     pass=pass && retained==owned;pedal.reset();pass=pass && owned==0;
     ++cases;if(!pass)++failures;
     std::printf("%s rate%.2f policy%d setting%d estimate%llu temporary%llu reported%llu measured%llu peak%llu retained%llu preflight_calls%llu report_calls%llu pass%d\n",desc->modelId,rate,policy,setting,(unsigned long long)estimate.ownedBytes,(unsigned long long)estimate.temporaryBytes,(unsigned long long)actual,(unsigned long long)measured,(unsigned long long)measuredPeak,(unsigned long long)retained,(unsigned long long)preflightCalls,(unsigned long long)reportCalls,pass);
    }
 std::printf("models%zu fresh_cases%d retained_cases%d failures%d\n",PedalRegistry::all().size(),cases,cases,failures);
 return failures?1:0;
}
