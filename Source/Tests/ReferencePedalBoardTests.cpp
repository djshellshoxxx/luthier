// The reference board is a separate, default-off processor, not PedalType.
#include "TestFramework.h"
#include "../ReferencePedals/Board.h"
#include <array>
#include <cmath>
#include <limits>
using namespace luthier::reference;

LUTHIER_TEST (ReferencePedals, initiallyDisabledDoesNotChangeAudio)
{
    Board b;
    b.prepare (48000.0);
    b.setModel (0, Model::ds1Prototype);
    double l[] {0.0, 0.01, -0.12, 0.54, 0.0}, r[] {0.02, 0.02, -0.2, 0.01, 0.0};
    const std::array<double, 5> beforeL {0.0, 0.01, -0.12, 0.54, 0.0};
    const std::array<double, 5> beforeR {0.02, 0.02, -0.2, 0.01, 0.0};
    b.processStereo (l, r, 5, Placement::beforeExistingPre);
    for (int i = 0; i < 5; ++i) { CHECK (l[i] == beforeL[(size_t)i]); CHECK (r[i] == beforeR[(size_t)i]); }
}

LUTHIER_TEST (ReferencePedals, routingOrderIsAtomicAndStable)
{
    Board b;
    CHECK (b.getIdAtPosition (0) == 0);
    b.move (0, 2);
    CHECK (b.getIdAtPosition (0) == 1);
    CHECK (b.getIdAtPosition (1) == 2);
    CHECK (b.getIdAtPosition (2) == 0);
    bool seen[Board::slotCount] {};
    for (int p = 0; p < Board::slotCount; ++p)
    {
        const auto id = b.getIdAtPosition (p);
        CHECK (id >= 0 && id < Board::slotCount);
        CHECK (!seen[id]);
        seen[id] = true;
    }
}

LUTHIER_TEST (ReferencePedals, selectedEffectsModifyInputAndStayFinite)
{
    for (const auto model : {Model::ts808Prototype, Model::ds1Prototype, Model::phase90Prototype})
    {
        Board b;
        b.prepare (48000.0);
        b.setBoardEnabled (true);
        b.setModel (0, model);
        std::array<double, 2048> l {}, r {};
        for (int i = 0; i < 2048; ++i) l[(size_t)i] = r[(size_t)i] = 0.15 * std::sin (i * 0.1);
        double first = l[1024];
        b.processStereo (l.data(), r.data(), 2048, Placement::beforeExistingPre);
        CHECK (std::abs (l[1024] - first) > 1.0e-6);
        for (int i = 0; i < 2048; ++i) { CHECK (std::isfinite (l[(size_t)i])); CHECK (l[(size_t)i] == r[(size_t)i]); }
    }
}

LUTHIER_TEST (ReferencePedals, placementCanBypassOneStage)
{
    Board b;
    b.prepare (48000.0);
    b.setBoardEnabled (true);
    b.setModel (0, Model::ds1Prototype);
    b.setPlacement (0, Placement::afterExistingPost);
    double l[512], r[512];
    for (int i=0; i<512; ++i) l[i] = r[i] = 0.3;
    b.processStereo (l, r, 512, Placement::beforeExistingPre);
    CHECK (l[511] == 0.3);
    b.processStereo (l, r, 512, Placement::afterExistingPost);
    CHECK (std::abs (l[511] - 0.3) > 0.0001);
}

LUTHIER_TEST (ReferencePedals, smallBlocksAndNullInputAreSafe)
{
    Board b;
    b.prepare (44100.0);
    b.setModel (0, Model::ts808Prototype);
    b.setBoardEnabled (true);
    for (int size : {1, 2, 4, 5, 7, 16, 128})
    {
        double l[128] {}, r[128] {};
        b.processStereo (l, r, size, Placement::beforeExistingPre);
        for (int i=0;i<size;++i) { CHECK (std::isfinite (l[i])); CHECK (std::isfinite (r[i])); }
    }
}

LUTHIER_TEST (ReferencePedals, invalidPreparationRatesRemainFinite)
{
    // The MIDI router and prototype DSP must both receive a validated rate.
    for (double rate : {0.0, -1.0, 7999.0,
                        std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity()})
    {
        Board b;
        b.prepare (rate, 128);
        b.setBoardEnabled (true);
        b.setModel (0, Model::ts808Prototype);
        double left[128] {}, right[128] {};
        left[0] = right[0] = 0.2;
        b.processStereo (left, right, 128, Placement::beforeExistingPre);
        for (int i = 0; i < 128; ++i)
        {
            CHECK (std::isfinite (left[i]));
            CHECK (std::isfinite (right[i]));
        }
    }
}

LUTHIER_TEST (ReferencePedals, invalidPlacementDoesNotOverwriteValidRouting)
{
    Board b;
    b.setPlacement (0, Placement::afterExistingPost);
    b.setPlacement (0, static_cast<Placement> (-1));
    CHECK (b.getPlacement (0) == Placement::afterExistingPost);
    b.setPlacement (0, static_cast<Placement> (99));
    CHECK (b.getPlacement (0) == Placement::afterExistingPost);
}
