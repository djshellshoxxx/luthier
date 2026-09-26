/*  performance-budget.md 8: the CPU relief ladder, on synthetic load. */

#include "TestFramework.h"

#include "../Support/CpuRelief.h"
#include "../LuthierEngine.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 128;

    /** Feeds `load` for `seconds`; returns the highest step seen. */
    int feed (CpuRelief& relief, double load, double seconds)
    {
        int highest = relief.getStep();

        for (int b = 0; b < (int) (seconds * kSr / kBlock); ++b)
            highest = juce::jmax (highest, relief.update (load, kBlock));

        return highest;
    }
}

LUTHIER_TEST (CpuRelief, laddersUpAtEightyFivePercentAndBackDown)
{
    CpuRelief relief;
    relief.prepare (kSr);

    // Normal load: nothing happens, however long.
    feed (relief, 0.60, 10.0);
    CHECK (relief.getStep() == CpuRelief::none);

    // Just under the threshold: still nothing.
    feed (relief, 0.84, 5.0);
    CHECK (relief.getStep() == CpuRelief::none);

    // Over it: the 200 ms average has to cross first, then one step per 200 ms.
    feed (relief, 0.95, 0.25);
    CHECK_MSG (relief.getStep() <= 1, "climbed " + juce::String (relief.getStep()) + " steps in 250 ms");

    feed (relief, 0.95, 0.60);
    CHECK_MSG (relief.getStep() >= 2 && relief.getStep() <= 4, "at step " + juce::String (relief.getStep()) + " after 850 ms");

    feed (relief, 0.95, 3.0);
    CHECK (relief.getStep() == CpuRelief::dropStrings);
    CHECK (relief.isCpuLimitBannerDue());

    // Hovering between 70% and 85% holds the step (hysteresis).
    feed (relief, 0.78, 5.0);
    CHECK (relief.getStep() == CpuRelief::dropStrings);

    // Relief: one step down per second below 70%.
    feed (relief, 0.30, 1.5);
    CHECK_MSG (relief.getStep() == 6, "at step " + juce::String (relief.getStep()) + " 1.5 s into relief");
    CHECK (! relief.isCpuLimitBannerDue());

    feed (relief, 0.30, 8.0);
    CHECK (relief.getStep() == CpuRelief::none);

    // A single overloaded block does not trip it: the average absorbs a spike.
    relief.update (3.0, kBlock);
    feed (relief, 0.30, 1.0);
    CHECK (relief.getStep() == CpuRelief::none);

    // Garbage in is no load.
    relief.update (std::numeric_limits<double>::quiet_NaN(), kBlock);
    CHECK (std::isfinite (relief.getAverageLoad()));
}

LUTHIER_TEST (CpuRelief, stepSevenIsOptOut)
{
    CpuRelief relief;
    relief.prepare (kSr);
    CHECK (relief.isStringDropAllowed());           // default on

    relief.setStringDropAllowed (false);
    CHECK (feed (relief, 1.2, 10.0) == CpuRelief::freezeAudition);
    CHECK (! relief.isCpuLimitBannerDue());

    relief.setStringDropAllowed (true);
    feed (relief, 1.2, 1.0);
    CHECK (relief.getStep() == CpuRelief::dropStrings);

    // Opting out while dropping strings stops at once.
    relief.setStringDropAllowed (false);
    relief.update (1.2, kBlock);
    CHECK (relief.getStep() == CpuRelief::freezeAudition);
}

/*  The engine's hook: step 4 halves the NoiseEngine pools, and a normal load
    leaves them alone. */
LUTHIER_TEST (CpuRelief, theEngineHalvesTheNoisePoolsOnlyUnderLoad)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);

    const auto fullSqueak = engine.getNoisePool().getPoolLimit (NoiseClass::squeak);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;

    for (int b = 0; b < 200; ++b)
    {
        buffer.clear();
        engine.processBlock (buffer, midi);
    }

    // An offline render runs far faster than real time: no relief.
    CHECK (engine.getCpuRelief().getStep() == CpuRelief::none);
    CHECK (engine.getNoisePool().getPoolLimit (NoiseClass::squeak) == fullSqueak);

    // Drive the ladder to step 4 as if overloaded, then let the engine apply it.
    auto& relief = engine.getCpuRelief();
    while (relief.getStep() < CpuRelief::halveNoisePools)
        relief.update (1.5, kBlock);

    // The engine applies the step it computes on its next block; overload it
    // by feeding the same verdict through its own ladder first.
    for (int b = 0; b < 3; ++b)
    {
        buffer.clear();
        engine.processBlock (buffer, midi);
    }

    // Offline, the engine's own measurement pulls the step back down only
    // after a second; three blocks later it is still halved.
    CHECK (engine.getCpuRelief().getStep() >= CpuRelief::halveNoisePools);
    CHECK (engine.getNoisePool().getPoolLimit (NoiseClass::squeak) < fullSqueak);

    // Prepare puts it back.
    engine.prepare (kSr, kBlock);
    CHECK (engine.getNoisePool().getPoolLimit (NoiseClass::squeak) == fullSqueak);
}
