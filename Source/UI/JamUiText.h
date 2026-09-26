#pragma once

/*  What the Jam UI says (jam-mode.md 8 and 12), as plain functions of the
    band's published status, so the JAM tab, the Easy strip, the Live pill and
    the tests read the same words.

    Message thread: everything here builds Strings. */

#include <juce_core/juce_core.h>

#include "../Jam/JamStatus.h"

namespace luthier
{

/** The facts the 8.4 messages depend on, beyond the status itself. */
struct JamUiFacts
{
    bool haveStatus = false;          ///< a status was read and is fresh (8.3: 250 ms)
    JamStatus status;
    bool enabled = false;             ///< jam_enabled
    int chordSource = 0;              ///< 0 Auto, 1 Live, 2 Tune
    bool tunePlaying = false;
    bool separateFallback = false;    ///< Separate asked for, no aux (7)
    bool backingTrackPlaying = false; ///< 11
    juce::String styleWarning;        ///< 13's banner, when a style file failed
};

namespace JamUiText
{
    /** The pill: JAM (off), ARMED, COUNT, PLAYING or ENDING. */
    juce::String pillText (JamState state, bool enabled);

    /** "Rock", "Funk"... or "User". */
    juce::String styleName (int style);

    /** "PLAYING . Rock B . 3(+1)" - the header's right-hand status; "-" when stale. */
    juce::String statusLine (const JamUiFacts& facts);

    /** "Am7 -> F (predicted) . 112 bpm host . bar 17.3"; "-" when stale. */
    juce::String chordLine (const JamUiFacts& facts);

    /** Every 8.4 message that applies now, most important first. */
    juce::StringArray messages (const JamUiFacts& facts);

    /** 12: "Band playing, Rock, intensity 3". Empty when there is nothing to say. */
    juce::String stateAnnouncement (const JamStatus& status);

    /** 12: "Kick 1 and 3, snare 2 and 4, hats 8ths; bass A A E G". */
    juce::String laneDescription (const JamStatus& status);

    /** One lane's hits as words: "1 and 3", "8ths", "3 hits". Empty for none. */
    juce::String describeHits (uint32_t hits, int stepsInBar, int stepsPerBeat);

    /** The seven drum lanes' names, then "Bass". */
    const char* laneName (int lane) noexcept;

    //==========================================================================
    // The 8.4 message texts, for tests to look for.
    extern const char* const kArmedNoChord;
    extern const char* const kNoTunePlaying;
    extern const char* const kSeparateNeedsAux;
    extern const char* const kNoPlayHead;
    extern const char* const kBassistResting;
    extern const char* const kBackingTrackToo;
    juce::String genericGroove (const JamStatus& status);
}

} // namespace luthier
