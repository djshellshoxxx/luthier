#pragma once

/*  midi-export.md 4.1's range, shared by the MIDI OUT and NOTATION tabs
    (MODEL-GAPS, TODO 9 / 10): "entire capture, current section (tune only),
    last N seconds, marked region".

    The marked region is the performance capture's (mark in, play, mark out);
    both captures count samples on the processor's clock, so one pair of marks
    serves the MIDI capture too. The current section is the TUNE tab's
    selected section, where it first plays: tune beat 0 is the host's quarter
    note 0, which is how the tune player locks to the host. */

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Capture/PerformanceCapture.h"
#include "../Export/MidiPerformance.h"

namespace luthier
{

class LuthierAudioProcessor;

namespace CaptureRanges
{
    enum Id { entire = 1, lastSeconds = 2, markedRegion = 3, currentSection = 4 };

    /** The four choices, in 4.1's words. */
    void addItems (juce::ComboBox& box);

    /** The TUNE tab's selected section as host quarter notes; empty when the tune has none. */
    juce::Range<double> currentSectionPpq (LuthierAudioProcessor& processor);

    /** For the NOTATION tab: the capture options for a range choice. */
    void apply (LuthierAudioProcessor& processor, int id, double seconds, CaptureScoreOptions& options);

    /** For the MIDI OUT tab: the range of the MIDI capture's performance; empty = all of it. */
    juce::Range<juce::int64> midiCaptureRange (LuthierAudioProcessor& processor, const MidiPerformance& performance,
                                               int id, double seconds);

    /** Whether the choice has anything to cover now (a marked region is marked; the tune has the section). */
    bool isAvailable (LuthierAudioProcessor& processor, int id);
}

} // namespace luthier
