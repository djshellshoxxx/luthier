#pragma once

/*  The fixed reference phrase output normalization measures
    (output-normalization.md 4.3).

    A sibling of AuditionPhrase: a timed MIDI sequence, built the same way for
    every sound, so the measured loudness depends only on the sound and never
    on what the player happened to play. It is fed to the engine as direct MIDI
    (LuthierEngine::setDirectMidi), so it is played as written whatever the
    rhythm engine, voicer or playing mode.

      Guitar families: 0-2 s  four down-strummed chords I-IV-V-I (20 ms spread,
                              velocity 90, 0.5 s each)
                       2-4 s  eight eighth notes at 120 BPM on the middle
                              strings, velocities 70 / 100 alternating
                       4-5 s  one chord at velocity 100, let ring
      Bass family:     0-4 s  root-fifth-octave eighth notes, velocities 80 / 100
                       4-5 s  a held root and octave double stop

    The measurement window runs from the phrase start to 6.0 s. The root is
    the instrument's lowest open string after tuning and capo.
*/

#include <juce_audio_basics/juce_audio_basics.h>

namespace luthier
{

class LuthierEngine;

class NormalizationPhrase
{
public:
    /** 4.3: the measurement window, from the phrase start. */
    static constexpr double kWindowSeconds = 6.0;

    /** 4.3: silence rendered first so the engine settles; discarded. */
    static constexpr double kSettleSeconds = 0.5;

    /** The phrase as a timed sequence (timestamps in seconds from the phrase
        start). Every note is released by kWindowSeconds. `numStrings` limits
        the chord size, so a four-string bass is never asked for six notes. */
    static juce::MidiMessageSequence build (bool bassFamily, int rootNote, int numStrings = 6);

    /** The root 4.3 asks for: the lowest open string after tuning and capo. */
    static int rootNoteFor (LuthierEngine& engine);

    /** The phrase in sample time at `sampleRate`, one MidiBuffer event per
        message. Convenience for renderers that slice it per block. */
    static juce::MidiBuffer buildBuffer (bool bassFamily, int rootNote, int numStrings, double sampleRate);
};

} // namespace luthier
