#include "TestFramework.h"
#include "../ReferencePedals/Generic/Chorus.h"
#include "../ReferencePedals/Generic/Phaser.h"
#include "../ReferencePedals/Generic/Tremolo.h"
#include "../ReferencePedals/Generic/Wah.h"
#include "../ReferencePedals/Generic/Delay.h"
#include "../ReferencePedals/Generic/Reverb.h"
#include "../ReferencePedals/Models/boss_ce2.h"
#include "../ReferencePedals/Models/boss_ce2w.h"
#include "../ReferencePedals/Models/boss_dd3t.h"
#include "../ReferencePedals/Models/mxr_phase90_script.h"
#include "../ReferencePedals/PedalRegistry.h"
using namespace luthier::reference;
namespace
{
template<class P>void checkModel(luthier::tests::TestContext& ctx)
{
 for(double sr:{8000.,44100.,48000.,48000.25,88200.,96000.,192000.,384000.})for(float setting:{0.f,1.f})
 {
  std::array<float,kMaxParams> params{};params.fill(setting);const auto estimate=P::prepareMemory(sr,2048,2,params.data());
  CHECK(estimate.ownedBytes!=kUnknownPreparedMemoryBytes);CHECK(estimate.ownedBytes>=sizeof(P));
  P pedal;CHECK(pedal.getPreparedMemoryBytes()==sizeof(P));
  for(int i=0;i<pedal.desc().numParams;++i)pedal.setParam(i,setting);
  pedal.prepare(sr,2048,2);const auto actual=pedal.getPreparedMemoryBytes();
  CHECK(actual<=estimate.ownedBytes);CHECK(actual>=sizeof(P));
  pedal.reset();CHECK(pedal.getPreparedMemoryBytes()==actual);
  pedal.prepare(8000,1,0);CHECK(pedal.getPreparedMemoryBytes()>=sizeof(P));
 }
}
template<class P>void invalidRate(luthier::tests::TestContext& ctx)
{
 for(double sr:{-1.,0.,1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),1e300})
  CHECK(P::prepareMemory(sr,512,1,nullptr).ownedBytes==kUnknownPreparedMemoryBytes);
}
}
LUTHIER_TEST(RefPedalTimeFxMemory, freshAndRetainedPreparedCapacityReports)
{
 checkModel<generic::Chorus>(ctx);checkModel<generic::Phaser>(ctx);checkModel<generic::Tremolo>(ctx);checkModel<generic::Wah>(ctx);checkModel<generic::Delay>(ctx);checkModel<generic::Reverb>(ctx);
 checkModel<models::BossCe2>(ctx);checkModel<models::BossCe2w>(ctx);checkModel<models::BossDd3t>(ctx);checkModel<models::MxrPhase90Script>(ctx);
 invalidRate<generic::Chorus>(ctx);invalidRate<generic::Delay>(ctx);invalidRate<generic::Reverb>(ctx);invalidRate<models::BossCe2>(ctx);invalidRate<models::BossCe2w>(ctx);invalidRate<models::BossDd3t>(ctx);
}
LUTHIER_TEST(RefPedalTimeFxMemory, acceptedRegistryEntriesHaveConstructorFreeEstimates)
{
 PedalRegistry::registerAll();
 for(const char* id:{"generic.chorus","generic.phaser","generic.tremolo","generic.wah","generic.delay","generic.reverb"})
 {
  const auto estimate=PedalRegistry::memoryRequirement(id,192000,2048,2,nullptr);CHECK(estimate.ownedBytes!=kUnknownPreparedMemoryBytes);
  auto pedal=PedalRegistry::make(id);CHECK(pedal!=nullptr);if(pedal){pedal->prepare(192000,2048,2);CHECK(pedal->getPreparedMemoryBytes()<=estimate.ownedBytes);}
 }
}
