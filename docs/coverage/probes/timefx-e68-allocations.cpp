#include "Source/ReferencePedals/Generic/Delay.h"
#include "Source/ReferencePedals/Generic/Reverb.h"
#include "Source/ReferencePedals/Models/boss_dd3t.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
using namespace luthier::reference;
static thread_local uint64_t count=0;
void* operator new(std::size_t n){++count;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
template<class P>void run(const char* name)
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})
 {
  P p;p.prepare(sr,2048,2);std::array<double,2048> l{},r{},wl{},wr{};const auto before=count;int failures=0;
  for(int block=0;block<64;++block)
  {
   for(int k=0;k<p.desc().numParams;++k)p.setParam(k,float((block+k)%3)*.5f);
   for(int i=0;i<2048;++i)l[i]=32*std::sin(i*.13),r[i]=-32*std::cos(i*.17);
   if(!p.processWithWetResidual(l.data(),r.data(),wl.data(),wr.data(),2048))++failures;
   for(int i=0;i<2048;++i)if(!std::isfinite(wl[i])||!std::isfinite(wr[i]))++failures;
   p.onFoot({bool(block%2),.5f,0,0});p.setExpression(0,.5f);p.setEnabled(block%2);
  }
  std::printf("%s sr%.0f frames131072 callback_allocations%llu finite_or_hook_failures%d\n",name,sr,(unsigned long long)(count-before),failures);
 }
}
int main(){run<generic::Delay>("Delay");run<generic::Reverb>("Reverb");run<models::BossDd3t>("DD3T");}
