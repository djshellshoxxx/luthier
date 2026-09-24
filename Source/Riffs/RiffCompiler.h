#pragma once

/*  Compiling a riff for audition (riff-library.md 5.1).

    compile() is pure and deterministic, and runs on the message thread (or a
    worker): the same riff, settings and guitar give an equal CompiledRiff,
    on any thread. It transposes and re-frets (RiffTransposer), then lays the
    notes out as a sorted, immutable array of POD events positioned in beats,
    which is all RiffPlayer reads on the audio thread.

    Technique mapping (5.1.3):
      hammer -> HammerOn, pull -> PullOff, tap -> Tap, pm -> PalmMute
      (palmMuteDepth from the pm value, -1 for the player's own), dead ->
      MutedPick, natural / artificial / tapharm -> NaturalHarmonic with the
      partial from the node, pinch -> PinchHarmonic, the arrival of a legato
      slide and slideup / slidedown -> Slide with slideFromFret, trill ->
      alternating HammerOn / PullOff at 12 Hz, ghost x0.45 velocity, accent
      x1.2, staccato half length. Anything else plucks.

      Two decisions (docs/coverage/FEAT-RIFFS.md): the engine's Slide is by
      definition unpicked (it re-excites at 0.35), so the *picked* slides -
      slidein, a shift slide's arrival - are a Pluck with slideFromFret set,
      which the engine glides just the same; slideout / slideup are a
      no-pluck Slide over the last quarter of the note.

    Bends, bend releases, prebends and whammy become breakpoint curves in
    cents per note; vibrato is a sine at its rate and depth, evaluated by the
    player. Strums are staggered 1/cv seconds apart in stroke order; that
    spacing is held in seconds (delaySeconds) and converted at play time.
*/

#include "RiffTransposer.h"
#include "../Model/Playing/PlayingEvents.h"

#include <memory>

namespace luthier
{

//==============================================================================
struct RiffEvent
{
    enum class Kind : int { noteOn = 0, noteOff = 1 };

    double beat = 0.0;
    double delaySeconds = 0.0;       ///< strum stagger, added after the beat
    Kind kind = Kind::noteOn;
    int stringIndex = 0;
    double fret = 0.0;
    int midiNote = 60;
    double velocity = 0.8;
    Technique technique = Technique::Pluck;
    int harmonicPartial = 0;
    double slideFromFret = -1.0;
    double slideBeats = 0.0;         ///< converted to seconds at the playing tempo
    double palmMuteDepth = -1.0;
    int bassTechnique = -1;
    bool letRing = false;
    int bendSegment = -1;            ///< the note's pitch curve, or -1
    int noteIndex = -1;              ///< the placed note it plays

    bool operator== (const RiffEvent& o) const noexcept;
};

/** One note's pitch movement: breakpoints in cents, plus a vibrato. */
struct RiffBendSegment
{
    int stringIndex = 0;
    double startBeat = 0.0, endBeat = 0.0;
    int firstPoint = 0, numPoints = 0;     ///< into CompiledRiff::bendPoints, beats absolute
    double vibratoRateHz = 0.0, vibratoDepthCents = 0.0;
    double vibratoDelayBeats = 0.0;

    bool operator== (const RiffBendSegment& o) const noexcept;
};

struct CompiledRiff
{
    juce::String id, name;

    std::vector<RiffEvent> events;               ///< sorted by beat, then delay, offs first
    std::vector<RiffBendSegment> bends;          ///< sorted by start
    std::vector<std::pair<double, double>> bendPoints;   ///< (beat, cents)

    double lengthBeats = 4.0;
    double loopBeats = 4.0;                      ///< whole bars
    double beatsPerBar = 4.0;
    double tempoBpm = 120.0;                     ///< the riff's own tempo
    int numStrings = 6;

    RiffPlacement placement;                     ///< what the tab view draws and the destinations use
    GuitarSpecSummary guitar;
    RiffPlaySettings settings;
    juce::StringArray notices;

    /** Every field that plays equal: RL-04's purity check. */
    bool operator== (const CompiledRiff& o) const noexcept;
    bool operator!= (const CompiledRiff& o) const noexcept { return ! (*this == o); }

    /** The placed notes as a one-track score (the key and tempo it compiled at). */
    PerformanceScore toScore (const Riff& source) const;

    /** The placed notes back as a riff (string, fret and tuning of the target
        guitar), for the destinations: drag-out, Add to Tune, Learn It. */
    Riff toPlacedRiff (const Riff& source) const;
};

namespace RiffCompiler
{
    std::shared_ptr<const CompiledRiff> compile (const Riff& riff, const RiffPlaySettings& settings,
                                                 const GuitarSpecSummary& guitar);

    /** The BassStepType index a BASS_TECH token plays as (slap -> thumb,
        pop -> pop, thump -> thumb, lhslap -> dead); -1 for none. */
    int bassTechniqueIndex (const juce::String& token);

    /** The partial a harmonic at a node fret (or node offset) sounds; 0 off a node. */
    int harmonicPartialFor (double fretOrOffset);
}

} // namespace luthier
