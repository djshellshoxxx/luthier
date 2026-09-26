#pragma once

/*  piano-roll-chord-display.md 4: the chord or note name over the guitar's
    lower bout - semi-transparent, faded in and out by ChordNameFader, named by
    ChordNaming from what the strings are sounding (the SoundingNotes the audio
    thread publishes). GuitarBodyComponent ticks it from its live-overlay timer
    and paints it in the live pass, never into the cached scene (section 7).

    - Off (Options -> Visual aids): nothing is drawn and no detection runs (CD-04).
    - A strum's strings within 30 ms of its first, by the audio's own sample
      times, are one chord (CD-03).
    - Screen readers: "Announce chord names" posts the name politely, at most
      once every 1.5 s.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "ChordNameFader.h"
#include "ChordNaming.h"
#include "../Support/SoundingNotes.h"

namespace luthier
{

class LuthierAudioProcessor;

class ChordNameOverlay
{
public:
    explicit ChordNameOverlay (LuthierAudioProcessor& processor);

    /** Reads the strings' snapshot and advances the fade (the UI timer). */
    void tick (double nowMs);

    /** The same with a frame passed in (tests). */
    void observe (double nowMs, const SoundingNotes::Frame& frame, double sampleRate);

    /** Draws the name centred in `boutArea`, sized from the illustration's height. */
    void paint (juce::Graphics&, juce::Rectangle<float> boutArea, float illustrationHeight, double nowMs) const;

    /** Where the lower bout is: the quarter of the body at its tail end,
        across the body's width. `body` and `nut` in the same coordinates. */
    static juce::Rectangle<float> lowerBout (juce::Rectangle<float> body, juce::Point<float> nut);

    /** Section 4's size: 12 % of the illustration's height, 28 - 96 px. */
    static float fontHeightFor (float illustrationHeight) noexcept;

    juce::String getText() const { return fader.getText(); }
    float getOpacity (double nowMs) const noexcept { return fader.getOpacity (nowMs); }
    const ChordNameFader& getFader() const noexcept { return fader; }

    /** How many times a name was worked out (CD-04: none while the option is off). */
    int getDetectionCount() const noexcept { return detections; }
    juce::String getLastAnnouncement() const { return lastAnnouncement; }

    static constexpr double kAnnounceIntervalMs = 1500.0;

private:
    ChordNaming::Spelling spelling() const;

    LuthierAudioProcessor& processor;
    ChordNameFader fader;

    std::array<int, SoundingNotes::kMaxStrings> lastNotes {};
    std::array<std::int64_t, SoundingNotes::kMaxStrings> lastStarts {};
    std::int64_t chordFirstOnset = -1;
    juce::String lastName;
    int detections = 0;

    double lastPublishMs = 0.0;
    std::uint32_t lastSequence = 0;

    juce::String lastAnnouncement;
    double lastAnnounceMs = -1.0e9;
};

} // namespace luthier
