#pragma once

/*  preset-browser-previews.md 3.1: the phrases a preset preview plays.

    A separate enum from AuditionPhrase (Support/AudioExporter.h) so that the
    audition indices stored in sessions never move. Each phrase is written once
    in standard tuning against a reference root (E2 for guitar, E1 for bass) and
    transposed so that its root sits on the preset's lowest open string after
    tuning and capo - "Drop C Riff" riffs in C, "5-String Low B" grooves on B0.

    Everything here is deterministic: the same context always builds the same
    sequence, which PB-01's bit-identical render depends on.
*/

#include <juce_audio_basics/juce_audio_basics.h>

namespace luthier
{

class PreviewPhrase
{
public:
    enum class Id
    {
        acoustic_strum, fingerstyle, nylon_comp, rasgueado, clean_arp, crunch_riff,
        highgain_riff, lead_lick, jazz_comp, funk_chop, twang_lick, slide_lick,
        ambient_swell, bass_groove, bass_slap, bass_walk,
        NumIds
    };

    /** The facts about a preset the choice and the transposition need. */
    struct Context
    {
        juce::String name, category, previewPhrase;
        juce::StringArray tags;

        bool bassFamily = false;
        bool nylon = false;          ///< nylon strings, or a classical / flamenco body
        bool acousticBody = false;
        bool slide = false;          ///< slide_mode / slide_guitar on
        bool slapArmed = false;
        bool rhythmEngineOn = false;
        double drive = 0.0;          ///< 0-1, preset-browser-previews 6.1

        int lowestOpenMidi = 40;     ///< lowest open string after tuning and capo
        int highestMidi = 88;        ///< the top of the guitar's range
    };

    /** A built phrase: note times in seconds from the phrase start. */
    struct Built
    {
        Id id = Id::clean_arp;
        juce::MidiMessageSequence sequence;
        double noteEndSeconds = 0.0;   ///< the last note-off
        int lowestNote = 127;
        double bpm = 100.0;
    };

    static constexpr double kMaxNoteSeconds = 3.2;   ///< 3.1: notes last 3.2 s or less
    static constexpr double kMaxClipSeconds = 4.0;   ///< phrase plus tail, hard cap
    static constexpr double kFadeSeconds    = 0.25;  ///< the raised-cosine end fade

    static const char* getIdString (Id) noexcept;
    static Id fromString (const juce::String&, Id fallback = Id::NumIds) noexcept;

    /** 3.1's selection order: the preset's own field, then the table's words,
        then the feature fallback. */
    static Id choose (const Context&);

    /** The phrase's reference tempo, from 3.1's table. */
    static double getBpm (Id) noexcept;

    static Built build (Id, const Context&);
};

} // namespace luthier
