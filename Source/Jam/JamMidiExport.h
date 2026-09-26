#pragma once

/*  The band's capture as a MIDI file (jam-mode.md 9): drag-out and Export.

    Type 1, two tracks: "Jam Drums" (GM channel 10 by default, carrying the
    tempo map and time signature) and "Jam Bass" (channel 11). Positions come
    from the band's own musical time, so the file sits on the grid the band
    played, not on the latency-shifted samples. The Generic profile is plain
    notes; the Luthier profile adds a `LUTHIER: JAM style= variation=
    intensity= kit=` text meta at each change. Message thread.
*/

#include "JamCapture.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace luthier
{

struct JamMidiExportOptions
{
    int bars = 8;                ///< the last N bars; 0 = everything captured
    bool luthierProfile = false; ///< Generic is the default (9)
    int ticksPerQuarter = 960;
    int drumChannel = 10, bassChannel = 11;
};

class JamMidiExport
{
public:
    /** The drag-out's choices: 4, 8, 16, 32 bars, or all (0). */
    static constexpr int kDragChoices[] = { 4, 8, 16, 32, 0 };

    static juce::MidiFile build (const std::vector<JamCaptureEvent>& events, const JamMidiExportOptions& options);

    /** Builds from the capture and writes a temporary file for a drag, or to
        `destination`. Returns the file, or an empty File if there was nothing
        to write. */
    static juce::File write (const JamCapture& capture, const JamMidiExportOptions& options,
                             const juce::File& destination = {});

    /** The file's name: "Jam - last 8 bars.mid". */
    static juce::String suggestedName (const JamMidiExportOptions& options);
};

} // namespace luthier
