// OWNER: WP7
#include "../ReferencePedals/Generic/Boost.h"
#include "../ReferencePedals/Generic/Compressor.h"
#include "../ReferencePedals/Generic/Fuzz.h"
#include "../ReferencePedals/Generic/Overdrive.h"
#include "../ReferencePedals/Models/boss_ds1.h"
#include "../ReferencePedals/Models/boss_sd1.h"
#include "../ReferencePedals/Models/ehx_big_muff_nyc.h"
#include "../ReferencePedals/Models/ehx_deluxe_muff.h"
#include "../ReferencePedals/Models/ehx_green_russian.h"
#include "../ReferencePedals/Models/ehx_muff_wicker.h"
#include "../ReferencePedals/Models/ibanez_ts808.h"
#include "../ReferencePedals/Models/mxr_distortion_plus.h"
#include "../ReferencePedals/Models/mxr_dynacomp.h"
#include "../ReferencePedals/Models/proco_rat.h"
#include "TestFramework.h"
#include <limits>
using namespace luthier::reference;
#if defined(LUTHIER_ALLOCATION_COUNTER)
namespace luthier::tests
{
long allocationsOnThisThread() noexcept;
}
#endif
namespace
{
template <class Model> void audit(luthier::tests::TestContext &ctx)
{
    const auto &desc = Model::descriptor();
    for (double sr : {8000., 44100., 48000., 88200., 96000., 192000.})
        for (int policy : {0, 1, 2})
        {
            std::array<float, kMaxParams> params{};
            for (int i = 0; i < desc.numParams; ++i)
                params[i] = desc.params[i].defaultNormalised();
            if (policy == 0)
                for (int i = 0; i < desc.numParams; ++i)
                    if (desc.params[i].has(PF_Config))
                        params[i] = 0;
            const int block = policy == 0 ? 1 : policy == 1 ? 2048 : 4096;
#if defined(LUTHIER_ALLOCATION_COUNTER)
            const auto before = luthier::tests::allocationsOnThisThread();
#endif
            // Deliberately query before constructing the model.
            const auto requirement = Model::prepareMemory(sr, block, policy, params.data());
#if defined(LUTHIER_ALLOCATION_COUNTER)
            CHECK(luthier::tests::allocationsOnThisThread() == before);
#endif
            CHECK(requirement.ownedBytes != kUnknownPreparedMemoryBytes);
            CHECK(requirement.temporaryBytes == 0);
            Model model;
            for (int i = 0; i < desc.numParams; ++i)
                model.setParam(i, params[i]);
            model.prepare(sr, block, policy);
#if defined(LUTHIER_ALLOCATION_COUNTER)
            const auto queryBefore = luthier::tests::allocationsOnThisThread();
#endif
            const auto actual = model.getPreparedMemoryBytes();
#if defined(LUTHIER_ALLOCATION_COUNTER)
            CHECK(luthier::tests::allocationsOnThisThread() == queryBefore);
#endif
            CHECK(actual == requirement.ownedBytes);
            CHECK(actual >= sizeof(Model));
            model.reset();
            CHECK(model.getPreparedMemoryBytes() == actual);
        }
}
} // namespace
LUTHIER_TEST(RefPedalDspMemory, embeddedGenericAndNamedModels)
{
    audit<generic::Boost>(ctx);
    audit<generic::Overdrive>(ctx);
    audit<generic::Fuzz>(ctx);
    audit<generic::Compressor>(ctx);
    audit<models::IbanezTs808>(ctx);
    audit<models::BossDs1>(ctx);
    audit<models::BossSd1>(ctx);
    audit<models::MxrDistortionPlus>(ctx);
    audit<models::ProcoRat>(ctx);
    audit<models::EhxGreenRussian>(ctx);
}
LUTHIER_TEST(RefPedalDspMemory, dynamicModelsMatchPreparedCapacities)
{
    audit<models::MxrDynaComp>(ctx);
    audit<models::EhxBigMuffNyc>(ctx);
    audit<models::EhxMuffWicker>(ctx);
    audit<models::EhxDeluxeMuff>(ctx);
}
LUTHIER_TEST(RefPedalDspMemory, retainedCapacityAndInvalidDynamicPreflight)
{
    common::FractionalDelay line;
    CHECK(line.ownedMemoryBytes() == 0);
    line.prepare(100);
    CHECK(line.ownedMemoryBytes() == 116 * sizeof(double));
    line.prepare(1);
    CHECK(line.ownedMemoryBytes() >= 17 * sizeof(double));
    const auto bytes = line.ownedMemoryBytes();
    line.reset();
    CHECK(line.ownedMemoryBytes() == bytes);
    models::EhxMuffWicker p;
    p.prepare(192000, 2048, 1);
    const auto high = p.getPreparedMemoryBytes();
    p.prepare(44100, 2048, 1);
    const auto low = p.getPreparedMemoryBytes();
    CHECK(low >= models::EhxMuffWicker::prepareMemory(44100, 2048, 1, nullptr).ownedBytes);
    CHECK(low <= high);
    for (double sr :
         {0., -1., 1e300, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        CHECK(models::EhxMuffWicker::prepareMemory(sr, 2048, 1, nullptr).ownedBytes ==
              kUnknownPreparedMemoryBytes);
    CHECK(models::EhxMuffWicker::prepareMemory(48000, -1, 1, nullptr).ownedBytes ==
          kUnknownPreparedMemoryBytes);
}

LUTHIER_TEST(RefPedalDspMemory, dynamicBoundsAndDeluxeConfiguration)
{
    using Estimator = PreparedMemoryEstimator;
    for (Estimator estimate : {&models::MxrDynaComp::prepareMemory, &models::EhxBigMuffNyc::prepareMemory,
                               &models::EhxMuffWicker::prepareMemory, &models::EhxDeluxeMuff::prepareMemory})
    {
        for (double rate : {0., -1., 7999., 192001., 1e300, std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::quiet_NaN()})
            CHECK(estimate(rate, 4096, 1, nullptr).ownedBytes == kUnknownPreparedMemoryBytes);
        CHECK(estimate(48000, 0, 1, nullptr).ownedBytes == kUnknownPreparedMemoryBytes);
        CHECK(estimate(48000, -1, 1, nullptr).ownedBytes == kUnknownPreparedMemoryBytes);
    }
    const auto &desc = models::EhxDeluxeMuff::descriptor();
    std::array<float, kMaxParams> params{};
    for (int i = 0; i < desc.numParams; ++i)
        params[i] = desc.params[i].defaultNormalised();
    CHECK(models::EhxDeluxeMuff::prepareMemory(48000, 4096, 1, nullptr).ownedBytes ==
          models::EhxDeluxeMuff::prepareMemory(48000, 4096, 1, params.data()).ownedBytes);
    for (float hq :
         {0.f, 1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
        for (int policy : {0, 1, 2})
        {
            params[13] = hq;
            models::EhxDeluxeMuff model;
            for (int i = 0; i < desc.numParams; ++i)
                model.setParam(i, params[i]);
            model.prepare(48000, 4096, policy);
            CHECK(model.getPreparedMemoryBytes() ==
                  models::EhxDeluxeMuff::prepareMemory(48000, 4096, policy, params.data()).ownedBytes);
        }
}
