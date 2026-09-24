#pragma once

/*  Playing Technique Engine (engine spec 4).

    Decides what the player just did. A MIDI note on its own carries almost none of
    that information, so the technique is inferred from context: which string is
    still ringing, how long since the last note on it, which direction the pitch
    moved, how hard it was struck, and which technique controllers are held.

    The detection order in engine spec 4 is followed exactly, because it is a
    priority list: an explicitly-triggered technique always beats an inferred one.
*/

#include "PlayingEvents.h"
#include <array>

namespace luthier
{

class TechniqueEngine
{
public:
    void prepare (double sampleRate, int numStrings) noexcept;
    void reset() noexcept;

    void setNumStrings (int n) noexcept { numStrings = juce::jlimit (1, kMaxStrings, n); }

    //==========================================================================
    // Controller state, pushed in from the MIDI interpreter.

    void setPalmMuteAmount (double amount) noexcept { palmMute = juce::jlimit (0.0, 1.0, amount); }
    double getPalmMuteAmount() const noexcept { return palmMute; }

    void setMutedPickAmount (double amount) noexcept { mutedPick = juce::jlimit (0.0, 1.0, amount); }

    void setPinchHarmonicTrigger (bool on) noexcept { pinchTrigger = on; }
    void setNaturalHarmonicTrigger (bool on) noexcept { harmonicTrigger = on; }
    void setTapTrigger (bool on) noexcept { tapTrigger = on; }
    void setSlideMode (bool on) noexcept { slideMode = on; }
    void setSlideGuitarMode (bool on) noexcept { slideGuitarMode = on; }
    void setFretlessMode (bool on) noexcept { fretless = on; }

    bool isSlideGuitarMode() const noexcept { return slideGuitarMode; }
    bool isFretlessMode() const noexcept { return fretless; }

    /** Velocity above which a note is read as a pinch harmonic when the harmonic
        velocity trigger is enabled. */
    void setHarmonicVelocityThreshold (double v) noexcept { harmonicVelocity = juce::jlimit (0.0, 1.0, v); }
    void setHarmonicVelocityTriggerEnabled (bool e) noexcept { harmonicVelocityTrigger = e; }

    /** Legato window: notes closer together than this on the same string are read
        as a slide rather than two separate articulations. */
    void setLegatoWindowMs (double ms) noexcept { legatoWindowMs = juce::jlimit (5.0, 400.0, ms); }
    double getLegatoWindowMs() const noexcept { return legatoWindowMs; }

    /** Velocity below which a legato transition is a hammer-on or pull-off rather
        than a re-pluck. Engine spec uses MIDI 80, i.e. 0.63. */
    void setLegatoVelocityThreshold (double v) noexcept { legatoVelocity = juce::jlimit (0.0, 1.0, v); }

    void setHammerOnEnabled (bool e) noexcept { hammerOnEnabled = e; }
    void setSlideEnabled (bool e) noexcept { slideEnabled = e; }

    //==========================================================================
    /** The core decision. Called once per incoming note-on.

        @param stringIndex     which string the note was assigned to
        @param newFret         the fret position of the new note
        @param velocity        0 to 1
        @param timestampSamples absolute sample position of the event
        @param [out] harmonicPartial which partial to isolate, 0 for none
        @param [out] slideFromFret   the fret to glide from, or -1
    */
    Technique decide (int stringIndex,
                      double newFret,
                      double velocity,
                      int64_t timestampSamples,
                      int& harmonicPartial,
                      double& slideFromFret) noexcept;

    /** auto-articulation.md 4.2 (FEAT-ASSIST): the same decision, and whether a
        controller trigger chose it (the explicit branches: palm mute, pinch,
        harmonic trigger or velocity, tap, slide guitar, muted pick, Slide
        Mode). */
    Technique decide (int stringIndex, double newFret, double velocity, int64_t timestampSamples,
                      int& harmonicPartial, double& slideFromFret, bool& explicitOut) noexcept;

    /** FEAT-ASSIST: false while Performance Assist owns legato (its rule 1 or 2
        on); decide() then skips its own hammer-on / slide inference. */
    void setLegatoInferenceEnabled (bool e) noexcept { legatoInference = e; }
    bool isLegatoInferenceEnabled() const noexcept { return legatoInference; }
    bool isSlideMode() const noexcept { return slideMode; }

    /** Records that a note ended, so the next note on that string is not treated
        as legato. */
    void noteEnded (int stringIndex, int64_t timestampSamples) noexcept;

    /** Whether a string currently has a note held on it. */
    bool isStringActive (int stringIndex) const noexcept;
    double getStringFret (int stringIndex) const noexcept;

    /** Damping state the string engine should apply for the current controllers. */
    int getDampingStateFor (Technique t) const noexcept;

    /** How long a slide to the given interval should take, in seconds. */
    double slideDurationFor (double semitoneDistance) const noexcept;

    /** The partial a natural harmonic at a given fret produces. Returns 0 if that
        fret is not a node. */
    static int harmonicPartialForFret (double fret) noexcept;

private:
    double sr = 44100.0;
    int numStrings = 6;

    struct StringState
    {
        bool active = false;
        double fret = 0.0;
        double velocity = 0.0;
        int64_t lastNoteSample = -1000000;
        Technique lastTechnique = Technique::Pluck;
    };

    std::array<StringState, kMaxStrings> strings {};

    double palmMute = 0.0;
    double mutedPick = 0.0;
    bool pinchTrigger = false;
    bool harmonicTrigger = false;
    bool tapTrigger = false;
    bool slideMode = false;
    bool slideGuitarMode = false;
    bool fretless = false;

    bool harmonicVelocityTrigger = false;
    double harmonicVelocity = 0.95;

    double legatoWindowMs = 40.0;
    double legatoVelocity = 0.63;

    bool hammerOnEnabled = true;
    bool legatoInference = true;   // FEAT-ASSIST
    bool slideEnabled = true;
};

} // namespace luthier
