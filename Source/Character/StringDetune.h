#pragma once

/*  String Detune: the actions behind the CHARACTER panel's STRING DETUNE group
    (spec/DECISIONS.md, "String Detune").

    The offsets themselves are parameters (string_detune_1..12) and the tuning
    engine applies them; what lives here is what the buttons and the Out of
    tune knob do to those parameters, kept out of the component so the tests
    and any other panel (Easy) can drive exactly the same thing.

    Message thread only: everything here writes parameters.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

#include "../Model/Playing/TuningEngine.h"

namespace luthier
{

namespace StringDetune
{
    using Offsets = std::array<double, kMaxStrings>;

    inline constexpr double kMaxCents = TuningEngine::kMaxStringDetuneCents;

    /** A seeded pattern, one value per string in [-1, 1]. The same seed gives
        the same pattern on every platform (RtRandom, not std::rand). Uniform,
        so across a set of strings some are nearly in tune and some are near
        the limit - which is how a guitar that has not been tuned for a week
        actually sounds. */
    Offsets pattern (uint64_t seed) noexcept;

    /** The Randomise action's offsets: the pattern scaled to +/- amount * 25
        cents. amount is clamped to 0..1, so nothing leaves the 25-cent range. */
    Offsets randomOffsets (double amount, uint64_t seed) noexcept;

    /** Reads every string's offset, in cents, from the parameters. */
    Offsets read (juce::AudioProcessorValueTreeState& state);

    /** Writes every string's offset, in cents, each clamped to +/-25.
        `asGestures` wraps each write in a change gesture, for a button press;
        a write made inside another control's drag passes false so the drag
        stays one gesture (and one undo step). */
    void write (juce::AudioProcessorValueTreeState& state, const Offsets& cents, bool asGestures = true);

    /** Randomise: each string to a seeded offset within +/- (Out of tune * 25). */
    void randomise (juce::AudioProcessorValueTreeState& state, uint64_t seed);

    /** Reset: every string back to 0. The Out of tune amount is left alone. */
    void reset (juce::AudioProcessorValueTreeState& state);

    /** The Out of tune amount, 0..1. */
    double getAmount (juce::AudioProcessorValueTreeState& state);

    /*  The Out of tune knob, turned by hand. Captured at the start of the drag:
        the knob scales the offsets that were there (so a hand-set pattern
        keeps its shape and a Randomise result keeps its character), or, when
        every string was in tune or the amount was zero, lays down the seeded
        pattern at the new amount. Automation of the amount alone moves no
        string - it is the range Randomise uses - so a host never rewrites
        twelve parameters behind the user's back. */
    class AmountDrag
    {
    public:
        void begin (juce::AudioProcessorValueTreeState& state, uint64_t fallbackSeed);
        void update (juce::AudioProcessorValueTreeState& state, double newAmount);
        void end() noexcept { active = false; }
        bool isActive() const noexcept { return active; }

    private:
        Offsets start {};
        double startAmount = 0.0;
        uint64_t seed = 0;
        bool active = false;
    };

    /** "E", "A", "F#" - the string's open note name, no octave, from its open
        frequency as tuned (capo and detune left out: it labels the string). */
    juce::String noteNameFor (const TuningEngine& tuning, int stringIndex);
}

} // namespace luthier
