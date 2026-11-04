#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_ce2.h"
#include "../ReferencePedals/Models/boss_dd3t.h"
#include "../ReferencePedals/Models/mxr_phase90_script.h"
namespace {
template<class P> void isolation(luthier::tests::TestContext& ctx,int monoIndex)
{
    for(double sr:{44100.,48000.,96000.,192000.})
    {
        P p;p.setEnabled(true);p.prepare(sr,2048,1);double energy=0,rightEnergy=0;
        for(int i=0;i<int(sr);++i){double l=.1*std::sin(i*.1),r=0;p.process(&l,&r,1);energy+=l*l;rightEnergy+=r*r;}
        CHECK(energy>1);CHECK_NEAR(rightEnergy,0,1e-25);
        p.setParam(monoIndex,1);p.reset();double difference=0;
        for(int i=0;i<int(sr*.1);++i){double l=.1*std::sin(i*.1),r=0;p.process(&l,&r,1);difference+=std::abs(l-r);}
        CHECK_NEAR(difference,0,1e-12);
    }
}
}
LUTHIER_TEST(RefPedalReferenceStereo, independentMonoStatesAndExplicitSum)
{
    using namespace luthier::reference::models;
    isolation<BossCe2>(ctx,13);isolation<BossDd3t>(ctx,18);isolation<MxrPhase90Script>(ctx,12);
}
