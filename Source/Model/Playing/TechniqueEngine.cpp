#include "TechniqueEngine.h"
#include "../../DSP/String/StringEngine.h"
#include "../../DSP/String/Harmonics.h"

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
    // harmonic-realism.md 4.1: the analytic node search replaces the table.
    // An integer fret is read as tab first, so <4>, <9> and <16> are the 5th
    // partial's nodes as a player would touch them.
    return harmonics::partialForFret (harmonics::tabTouchFret (fret));
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
    lastTappedHarmonic = false;

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
        // harmonic-realism.md 3: the partial follows from where the thumb
        // grazes (the pick position), found by the engine; not from velocity.
        result = Technique::PinchHarmonic;
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
    else if (artificialTrigger)
    {
        // harmonic-realism.md 6: fretted note, touched at fret + offset.
        result = Technique::ArtificialHarmonic;
    }
    else if (tapTrigger)
    {
        result = Technique::Tap;
    }
    else if (tappedHarmonicTrigger)
    {
        // 3: a tap at fret + offset on the fretted string.
        result = Technique::Tap;
        lastTappedHarmonic = true;
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
    else if (state.active && (legatoInference || slideMode))   // FEAT-ASSIST: Assist may own legato
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

Technique TechniqueEngine::decide (int stringIndex, double newFret, double velocity, int64_t timestampSamples,
                                   int& harmonicPartial, double& slideFromFret, bool& explicitOut) noexcept
{
    // auto-articulation.md 4.2 (FEAT-ASSIST): read before decide() moves the state on.
    const bool active = juce::isPositiveAndBelow (stringIndex, kMaxStrings) && strings[(size_t) stringIndex].active;

    explicitOut = palmMute > 0.05 || pinchTrigger || harmonicTrigger
                  || (harmonicVelocityTrigger && velocity >= harmonicVelocity)
                  || tapTrigger || slideGuitarMode || mutedPick > 0.05
                  || (slideMode && slideEnabled && active);

    return decide (stringIndex, newFret, velocity, timestampSamples, harmonicPartial, slideFromFret);
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
