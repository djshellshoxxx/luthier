#pragma once

/*  The phase-5b technique modules the engine owns (engine-technique-layer.md
    1-2), in one place so LuthierEngine carries one member for them.

    ScrapeEngine, SlapEngine and SlideEngine predate this and stay where they
    are; this adds the mute, the tap, the bend stack, the controllers they
    read and the cascade resolver. The engine's glue (where each is called in
    the block, what it does to the strings) is LuthierEngineTechniques.cpp.
*/

#include "MuteEngine.h"
#include "TapEngine.h"
#include "BendEngine.h"
#include "TechniqueControls.h"
#include "CascadeResolver.h"

namespace luthier
{

struct TechniqueLayer
{
    MuteEngine mute;
    TapEngine tap;
    BendEngine bend;
    TechniqueControls controls;
    CascadeResolver cascade;

    /** Pipeline-order instrumentation (engine-technique-layer 10): a counter
        per stage, bumped as each runs, so a test can see the order. */
    struct StageLog
    {
        static constexpr int kMax = 16;
        std::array<int, kMax> order {};
        int count = 0;
        void clear() noexcept { count = 0; }
        void add (int stage) noexcept { if (count < kMax) order[(size_t) count++] = stage; }
    };

    enum Stage { stageTriggers = 0, stageCascade, stageTechniques, stageRhythm, stageStrings };
    StageLog stages;

    void prepare (double sampleRate) noexcept
    {
        mute.prepare (sampleRate);
        tap.prepare (sampleRate);
        bend.prepare (sampleRate);
        cascade.prepare (sampleRate);
        controls.reset();
    }

    void reset() noexcept
    {
        mute.reset();
        tap.reset();
        bend.reset();
        cascade.reset();
        controls.reset();
    }

    /** Any technique armed (the six arm switches; slide's is Slide Mode). */
    bool anyArmed() const noexcept
    {
        return mute.getSettings().armed || tap.getSettings().armed || bend.isArmed();
    }

    //==========================================================================
    /** Structural technique state a preset carries (engine-technique-layer 7):
        the live mute grid, the custom scale, the drawn bend curve. */
    juce::var toVar() const
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("mute_grid", mute.toVar());
        o->setProperty ("bend_scale", bend.getCustomScale().toVar());

        juce::Array<juce::var> curve;

        for (int i = 0; i < BendEngine::kCurvePoints; ++i)
            curve.add (bend.getDrawnCurvePoint (i));

        o->setProperty ("bend_curve", curve);
        return juce::var (o);
    }

    /** A missing block resets to defaults (6: pre-delta presets load disarmed and default). */
    void fromVar (const juce::var& state)
    {
        auto* o = state.getDynamicObject();

        mute.fromVar (o != nullptr ? o->getProperty ("mute_grid") : juce::var());

        MicrotonalScale scale;
        scale.fromVar (o != nullptr ? o->getProperty ("bend_scale") : juce::var());
        bend.setCustomScale (scale);

        const auto* curve = o != nullptr ? o->getProperty ("bend_curve").getArray() : nullptr;

        for (int i = 0; i < BendEngine::kCurvePoints; ++i)
        {
            const double x = (double) i / (double) (BendEngine::kCurvePoints - 1);
            bend.setDrawnCurvePoint (i, curve != nullptr && i < curve->size() ? (double) curve->getReference (i) : x * x);
        }
    }
};

} // namespace luthier
