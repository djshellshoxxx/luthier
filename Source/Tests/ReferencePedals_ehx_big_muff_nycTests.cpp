// OWNER: WP13
#include "../ReferencePedals/Models/ehx_big_muff_nyc.h"
#include "TestFramework.h"
LUTHIER_TEST(RefPedalMuffNyc, descriptorAndSilence)
{
    luthier::reference::models::EhxBigMuffNyc p;
    CHECK(p.desc().numParams == 8);
    int lanes = 0;
    for (int i = 0; i < 8; ++i)
        lanes += !p.desc().params[i].isLaneExcluded();
    CHECK(lanes == 4);
    p.prepare(48000, 1, 1);
    CHECK(p.getLatencySamples() == 136);
    for (int i = 0; i < 4096; ++i)
    {
        double l = 0, r = 0;
        p.process(&l, &r, 1);
        CHECK(l == 0);
        CHECK(r == 0);
    }
}
LUTHIER_TEST(RefPedalMuffNyc, originalDefaultLevelCalibrationGate)
{
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        luthier::reference::models::EhxBigMuffNyc p;
        p.prepare(sr, 1, 1);
        double energy = 0;
        for (int i = 0; i < (int)(2 * sr); ++i)
        {
            double l = .1 * std::sin(2 * luthier::reference::common::pi * 1000 * i / sr), r = l;
            p.process(&l, &r, 1);
            if (i >= (int)sr)
                energy += l * l;
        }
        const double db = 10 * std::log10(energy / sr / .005);
        CHECK(db >= -6 && db <= 6);
    }
}
