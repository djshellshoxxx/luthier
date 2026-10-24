#include "TestFramework.h"
#include "../ReferencePedals/PedalRegistry.h"
using namespace luthier::reference;
LUTHIER_TEST(RefPedalTimeFxCpuMetadata, sixAcceptedModelsChargeOriginalDeclaredBudgets)
{
    PedalRegistry::registerAll();
    struct Expected{const char* id;uint32_t units;double percent;};
    const Expected expected[]{
        {"generic.chorus",800,.8},{"generic.phaser",600,.6},
        {"generic.tremolo",200,.2},{"generic.wah",500,.5},
        {"generic.delay",1400,1.4},{"generic.reverb",3000,3.0}};
    double total=0;
    for(const auto& item:expected)
    {
        const auto* desc=PedalRegistry::find(item.id);CHECK(desc!=nullptr);if(!desc)continue;
        CHECK(desc->cpuUnits==item.units);CHECK(desc->cpuUnitsPerPercent==1000);
        const double percent=double(desc->cpuUnits)/desc->cpuUnitsPerPercent;
        CHECK_NEAR(percent,item.percent,1e-12);total+=percent;
    }
    // One of each costs 6.5%, and therefore cannot be treated as six free slots.
    CHECK_NEAR(total,6.5,1e-12);
}
