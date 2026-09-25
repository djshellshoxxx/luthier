#include "StringDetune.h"
#include "../Parameters.h"

namespace luthier
{

namespace StringDetune
{
    namespace
    {
        juce::RangedAudioParameter* offsetParameter (juce::AudioProcessorValueTreeState& state, int stringIndex)
        {
            return state.getParameter (ParamIDs::stringDetune (stringIndex + 1));
        }
    }

    Offsets pattern (uint64_t seed) noexcept
    {
        // Salted, so the pattern is not the same stream as the character
        // engine's own rolls when it is handed the instrument's seed.
        RtRandom rng { seed ^ 0x5D37D1E7A11E7ull };

        Offsets out {};

        for (auto& value : out)
            value = rng.nextBipolar();

        return out;
    }

    Offsets randomOffsets (double amount, uint64_t seed) noexcept
    {
        const double range = juce::jlimit (0.0, 1.0, std::isfinite (amount) ? amount : 0.0) * kMaxCents;

        auto out = pattern (seed);

        for (auto& value : out)
            value = juce::jlimit (-range, range, value * range);

        return out;
    }

    Offsets read (juce::AudioProcessorValueTreeState& state)
    {
        Offsets out {};

        for (int s = 0; s < kMaxStrings; ++s)
            if (auto* p = offsetParameter (state, s))
                out[(size_t) s] = (double) p->convertFrom0to1 (p->getValue());

        return out;
    }

    void write (juce::AudioProcessorValueTreeState& state, const Offsets& cents, bool asGestures)
    {
        for (int s = 0; s < kMaxStrings; ++s)
        {
            auto* p = offsetParameter (state, s);

            if (p == nullptr)
                continue;

            const double value = std::isfinite (cents[(size_t) s]) ? cents[(size_t) s] : 0.0;
            const float normalised = p->convertTo0to1 ((float) juce::jlimit (-kMaxCents, kMaxCents, value));

            if (asGestures)
                p->beginChangeGesture();

            p->setValueNotifyingHost (normalised);

            if (asGestures)
                p->endChangeGesture();
        }
    }

    double getAmount (juce::AudioProcessorValueTreeState& state)
    {
        if (auto* p = state.getParameter (ParamIDs::outOfTune))
            return (double) p->convertFrom0to1 (p->getValue());

        return 0.0;
    }

    void randomise (juce::AudioProcessorValueTreeState& state, uint64_t seed)
    {
        write (state, randomOffsets (getAmount (state), seed));
    }

    void reset (juce::AudioProcessorValueTreeState& state)
    {
        write (state, Offsets {});
    }

    //==========================================================================
    void AmountDrag::begin (juce::AudioProcessorValueTreeState& state, uint64_t fallbackSeed)
    {
        start = read (state);
        startAmount = getAmount (state);
        seed = fallbackSeed;
        active = true;
    }

    void AmountDrag::update (juce::AudioProcessorValueTreeState& state, double newAmount)
    {
        if (! active)
            return;

        newAmount = juce::jlimit (0.0, 1.0, newAmount);

        double largest = 0.0;

        for (auto value : start)
            largest = juce::jmax (largest, std::abs (value));

        Offsets next {};

        if (startAmount > 1.0e-3 && largest > 0.05)
        {
            const double scale = newAmount / startAmount;

            for (size_t s = 0; s < next.size(); ++s)
                next[s] = start[s] * scale;
        }
        else
        {
            next = randomOffsets (newAmount, seed);
        }

        // One gesture: the knob's own drag is the undo step and the host's.
        write (state, next, false);
    }

    //==========================================================================
    juce::String noteNameFor (const TuningEngine& tuning, int stringIndex)
    {
        const double hz = tuning.getStringTuning (stringIndex).openFrequencyHz;

        if (! (hz > 0.0))
            return "?";

        const int midi = (int) std::round (hzToMidi (hz, tuning.getConcertA()));
        return TuningEngine::noteName (midi).trimCharactersAtEnd ("-0123456789");
    }
}

} // namespace luthier
