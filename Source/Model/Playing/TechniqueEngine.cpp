#include "TechniqueEngine.h"
#include "../../DSP/String/StringEngine.h"

namespace luthier
{

void TechniqueEngine::prepare (double sampleRate, int strings) noexcept
{
    sr = sampleRate;
    numStrings = juce::jlimit (1, kMaxStrings, strings);
    reset();
}

void TechniqueEngine::reset() noexcept
{
    for (auto& s : strings)
    {
        s.active = false;
        s.fret = 0.0;
        s.velocity = 0.0;
        s.lastNoteSample = -1000000;
        s.lastTechnique = Technique::Pluck;
    }
}

//==============================================================================
int TechniqueEngine::harmonicPartialForFret (double fret) noexcept
{
    // A natural harmonic sounds where a node of some partial lies under the finger.
    // Fret 12 is the halfway point (2nd partial), fret 7 and 19 are thirds (3rd),
    // fret 5 and 24 are quarters (4th), fret 4 and 9 are fifths (5th).
    struct NodeFret { double fret; int partial; };

    static const NodeFret nodes[] =
    {
        { 12.0, 2 }, { 7.0, 3 }, { 19.0, 3 }, { 5.0, 4 }, { 24.0, 4 },
        { 4.0, 5 }, { 9.0, 5 }, { 16.0, 5 }, { 3.2, 6 }, { 2.7, 7 }, { 2.3, 8 }
    };

    for (const auto& n : nodes)
        if (std::abs (fret - n.fret) < 0.35)
            return n.partial;

    return 0;
}

//==============================================================================
Technique TechniqueEngine::decide (int stringIndex,
                                   double newFret,
                                   double velocity,
                                   int64_t timestampSamples,
                                   int& harmonicPartial,
                                   double& slideFromFret) noexcept
{
    harmonicPartial = 0;
    slideFromFret = -1.0;

    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return Technique::Pluck;

    auto& state = strings[(size_t) stringIndex];

    Technique result = Technique::Pluck;

    // ---- explicit triggers, in the spec's priority order --------------------
    if (palmMute > 0.05)
    {
        result = Technique::PalmMute;
    }
    else if (pinchTrigger)
    {
        result = Technique::PinchHarmonic;
        // A pinch usually lands on the 2nd to 5th partial depending on where the
        // thumb grazes; higher velocity tends to catch a higher one.
        harmonicPartial = 2 + (int) (velocity * 3.0);
    }
    else if (harmonicTrigger
             || (harmonicVelocityTrigger && velocity >= harmonicVelocity))
    {
        const int partial = harmonicPartialForFret (newFret);

        if (partial > 0)
        {
            result = Technique::NaturalHarmonic;
            harmonicPartial = partial;
        }
        else
        {
            // Not on a node: the player is touching somewhere that will not chime,
            // so treat it as an artificial harmonic an octave up instead of
            // producing a dead note.
            result = Technique::ArtificialHarmonic;
            harmonicPartial = 2;
        }
    }
    else if (tapTrigger)
    {
        result = Technique::Tap;
    }
    else if (slideGuitarMode)
    {
        result = Technique::SlideGuitar;

        if (state.active)
            slideFromFret = state.fret;
    }
    else if (mutedPick > 0.05)
    {
        result = Technique::MutedPick;
    }
    else if (state.active)
    {
        // ---- legato inference -------------------------------------------------
        const double elapsedMs = (double) (timestampSamples - state.lastNoteSample) * 1000.0 / sr;

        if (slideEnabled && (slideMode || elapsedMs < legatoWindowMs))
        {
            result = Technique::Slide;
            slideFromFret = state.fret;
        }
        else if (hammerOnEnabled && velocity < legatoVelocity)
        {
            result = (newFret > state.fret) ? Technique::HammerOn : Technique::PullOff;

            // Identical pitch with a light touch is a re-articulation, not a
            // hammer-on that goes nowhere.
            if (std::abs (newFret - state.fret) < 0.05)
                result = Technique::Pluck;
        }
        else
        {
            result = Technique::Pluck;
        }
    }

    // A fretless instrument slides between notes rather than stepping, so any
    // legato transition becomes a glide.
    if (fretless && state.active
        && (result == Technique::HammerOn || result == Technique::PullOff))
    {
        result = Technique::Slide;
        slideFromFret = state.fret;
    }

    state.active = true;
    state.fret = newFret;
    state.velocity = velocity;
    state.lastNoteSample = timestampSamples;
    state.lastTechnique = result;

    return result;
}

//==============================================================================
void TechniqueEngine::noteEnded (int stringIndex, int64_t timestampSamples) noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    strings[(size_t) stringIndex].active = false;
    strings[(size_t) stringIndex].lastNoteSample = timestampSamples;
}

bool TechniqueEngine::isStringActive (int stringIndex) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return false;

    return strings[(size_t) stringIndex].active;
}

double TechniqueEngine::getStringFret (int stringIndex) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return 0.0;

    return strings[(size_t) stringIndex].fret;
}

//==============================================================================
int TechniqueEngine::getDampingStateFor (Technique t) const noexcept
{
    switch (t)
    {
        case Technique::PalmMute:  return (int) StringEngine::Damping::PalmMute;
        case Technique::MutedPick: return (int) StringEngine::Damping::LightTouch;
        default:                   return (int) StringEngine::Damping::Open;
    }
}

double TechniqueEngine::slideDurationFor (double semitoneDistance) const noexcept
{
    // A hand moves at a roughly constant speed, so a longer slide takes longer.
    // About 25 ms per fret matches a natural-sounding legato slide, with floors
    // and ceilings so a two-fret slide is not instant and a twelve-fret slide is
    // not a glissando that outlasts the note.
    const double d = std::abs (semitoneDistance);
    return juce::jlimit (0.020, 0.400, 0.018 + d * 0.025);
}

} // namespace luthier
