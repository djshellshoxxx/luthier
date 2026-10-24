#include "TestFramework.h"
#include "../ReferencePedals/Generic/Boost.h"
#include "../ReferencePedals/Generic/Overdrive.h"
#include "../ReferencePedals/Generic/Fuzz.h"
#include "../ReferencePedals/Generic/Compressor.h"
#include <array>
#include <limits>
using namespace luthier::reference;
namespace {
template<class P> void exerciseGenericA(luthier::tests::TestContext& ctx)
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})for(int size:{1,16,37,64,481,2048}){
  P a,b;a.prepare(sr,2048,2);b.prepare(sr,2048,2);
  std::array<double,4096> l{},r{},x{},y{};
  for(size_t i=0;i<l.size();++i){l[i]=x[i]=std::sin(i*.19)*.2;r[i]=y[i]=std::cos(i*.13)*.1;}
  a.setParam(0,.9f);b.setParam(0,.9f);
  for(int i=0;i<4096;i+=size)a.process(l.data()+i,r.data()+i,std::min(size,4096-i));
  for(int i=0;i<4096;++i)b.process(x.data()+i,y.data()+i,1);
  for(size_t i=0;i<l.size();++i){CHECK(l[i]==x[i]);CHECK(r[i]==y[i]);CHECK(std::isfinite(l[i]));CHECK(std::abs(l[i])<=16);}
  a.reset();double bad[]={std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),1e6,-1e6,1e-310,0};double rr[6]{};a.process(bad,rr,6);for(double v:bad){CHECK(std::isfinite(v));CHECK(std::abs(v)<=16);}
 }
}
}
LUTHIER_TEST(RefPedalGenericA, boostBlockAndFinite){exerciseGenericA<generic::Boost>(ctx);}
LUTHIER_TEST(RefPedalGenericA, overdriveBlockAndFinite){exerciseGenericA<generic::Overdrive>(ctx);}
LUTHIER_TEST(RefPedalGenericA, fuzzBlockAndFinite){exerciseGenericA<generic::Fuzz>(ctx);}
LUTHIER_TEST(RefPedalGenericA, compressorBlockAndFinite){exerciseGenericA<generic::Compressor>(ctx);}
LUTHIER_TEST(RefPedalGenericA, compressorStaticCurve)
{
 CHECK_NEAR(generic::Compressor::gainReductionDb(-12,-24,4,0),-9,1e-12);
 CHECK_NEAR(generic::Compressor::gainReductionDb(-30,-24,4,0),0,1e-12);
 CHECK_NEAR(generic::Compressor::gainReductionDb(-24,-24,4,6),-.5625,1e-12);
 generic::Compressor p;p.setParam(4,0);p.prepare(48000,64,1);
 double l[]={.1,.2,-.3,.4},r[]={.4,.3,.2,.1};p.process(l,r,4);CHECK(l[0]==.1);CHECK(l[2]==-.3);CHECK(r[0]==.4);
}
LUTHIER_TEST(RefPedalGenericA, externalSidechainRequiresFreshBuffers)
{
 generic::Compressor p;p.setParam(10,1);p.prepare(48000,64,1);
 double l[64],r[64],sc[64];std::fill_n(l,64,.2);std::fill_n(r,64,.1);std::fill_n(sc,64,1.0);
 p.process(l,r,64);CHECK(l[63]==.2);CHECK(r[63]==.1);CHECK(!p.sidechainAvailable());
 p.setSidechain(sc,sc,64);p.process(l,r,64);CHECK(p.sidechainAvailable());CHECK(l[63]!=.2);
 std::fill_n(l,64,.2);std::fill_n(r,64,.1);p.process(l,r,64);CHECK(!p.sidechainAvailable());CHECK(l[63]==.2);
}
LUTHIER_TEST(RefPedalGenericA, latencyStableAcrossEnableAndDescriptorRoundTrips)
{
 std::array<std::unique_ptr<IPedal>,4> pedals{std::make_unique<generic::Boost>(),std::make_unique<generic::Overdrive>(),std::make_unique<generic::Fuzz>(),std::make_unique<generic::Compressor>()};
 for(auto& p:pedals){p->prepare(48000,64,2);const int delay=p->getLatencySamples();p->setEnabled(false);CHECK(p->getLatencySamples()==delay);p->setEnabled(true);CHECK(p->getLatencySamples()==delay);
 for(int j=0;j<p->desc().numParams;++j){const auto& d=p->desc().params[j];if(d.kind==ParamKind::Continuous)for(double x:{0.,.25,.5,.75,1.})CHECK_NEAR(d.toNormalised(d.fromNormalised(x)),x,1e-6);}
 }
}
#ifdef LUTHIER_ALLOCATION_COUNTER
namespace luthier::tests {long allocationsOnThisThread() noexcept;}
LUTHIER_TEST(RefPedalGenericA, realtimeCallsDoNotAllocate)
{
 generic::Boost a;generic::Overdrive b;generic::Fuzz c;generic::Compressor d;IPedal* pedals[]={&a,&b,&c,&d};
 for(auto* p:pedals){p->prepare(192000,2048,2);std::array<double,2048> l{},r{};const auto before=luthier::tests::allocationsOnThisThread();
 for(int i=0;i<32;++i){for(int j=0;j<p->desc().numParams;++j)p->setParam(j,(float)(i%5)*.25f);p->setEnabled((i%3)!=0);p->setSidechain(l.data(),r.data(),2048);p->process(l.data(),r.data(),2048);p->onMidi({});p->onFoot({});p->reset();}
 const auto after=luthier::tests::allocationsOnThisThread();CHECK(after==before);}
}
#endif
