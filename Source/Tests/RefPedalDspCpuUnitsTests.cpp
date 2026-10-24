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

LUTHIER_TEST(RefPedalDspCpuUnits, explicitMilliPercentBudgets)
{
    struct Entry
    {
        const PedalDesc *desc;
        uint32_t units;
    };
    const Entry entries[] = {
        {&generic::Boost::descriptor(), 300},           {&generic::Overdrive::descriptor(), 1000},
        {&generic::Fuzz::descriptor(), 1200},           {&generic::Compressor::descriptor(), 300},
        {&models::IbanezTs808::descriptor(), 400},      {&models::BossDs1::descriptor(), 350},
        {&models::BossSd1::descriptor(), 1200},         {&models::MxrDistortionPlus::descriptor(), 800},
        {&models::ProcoRat::descriptor(), 1200},        {&models::MxrDynaComp::descriptor(), 500},
        {&models::EhxBigMuffNyc::descriptor(), 2000},   {&models::EhxMuffWicker::descriptor(), 2500},
        {&models::EhxGreenRussian::descriptor(), 1500}, {&models::EhxDeluxeMuff::descriptor(), 2500}};
    for (const auto &entry : entries)
    {
        CHECK(entry.desc->cpuUnits == entry.units);
        CHECK(entry.desc->cpuUnitsPerPercent == 1000);
    }
}
