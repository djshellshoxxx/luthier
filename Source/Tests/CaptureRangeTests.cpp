/*  midi-export 4.1's ranges beyond "entire" and "last N seconds" (MODEL-GAPS,
    TODO 9 / 10): the marked region and the current section, on the NOTATION
    and MIDI OUT tabs. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/CaptureRanges.h"
#include "../UI/MidiOutPanel.h"
#include "../UI/NotationPanel.h"
#include "../Tune/TuneTemplates.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /** Plays `notes` one every 40 blocks; `between (i)` runs before note i. */
    template <typename Between>
    void play (LuthierAudioProcessor& processor, std::initializer_list<int> notes, Between&& between)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);
        const std::vector<int> list (notes);

        for (int b = 0; b < 40 * (int) list.size() + 20; ++b)
        {
            if (b % 40 == 0 && b / 40 < (int) list.size())
            {
                processor.drainPerformanceCapture();
                between (b / 40);
            }

            buffer.clear();
            juce::MidiBuffer midi;

            if (b % 40 == 1 && b / 40 < (int) list.size())
                midi.addEvent (juce::MidiMessage::noteOn (1, list[(size_t) (b / 40)], (juce::uint8) 100), 10);

            if (b % 40 == 30 && b / 40 < (int) list.size())
                midi.addEvent (juce::MidiMessage::noteOff (1, list[(size_t) (b / 40)]), 0);

            processor.processBlock (buffer, midi);
        }

        processor.drainPerformanceCapture();
    }
}

LUTHIER_TEST (CaptureRanges, theMarkedRegionIsWhatWasPlayedBetweenTheMarks)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    auto& capture = processor.getPerformanceCapture();

    CHECK (! capture.hasMarkedRegion());

    // Four notes; marks around the second and third.
    play (processor, { 52, 55, 57, 59 }, [&capture] (int i)
    {
        if (i == 1) capture.markIn();
        if (i == 3) capture.markOut();
    });

    CHECK (capture.hasMarkedRegion());

    // NOTATION: the score holds the two notes inside.
    CaptureScoreOptions options;
    CaptureRanges::apply (processor, CaptureRanges::markedRegion, 0.0, options);
    PerformanceScore score;
    capture.toScore (score, options);
    CHECK_MSG (score.getTotalNoteCount() == 2, juce::String (score.getTotalNoteCount()) + " notes in the marked region");

    // Entire: all four.
    CaptureRanges::apply (processor, CaptureRanges::entire, 0.0, options);
    capture.toScore (score, options);
    CHECK (score.getTotalNoteCount() == 4);

    // MIDI OUT: the MIDI capture's performance in the same stretch holds two note-ons.
    const auto performance = MidiTakeExport::capturedPerformance (processor);
    const auto range = CaptureRanges::midiCaptureRange (processor, performance, CaptureRanges::markedRegion, 0.0);
    CHECK (! range.isEmpty());

    int noteOns = 0;

    for (const auto& m : performance.getMessages())
        if (m.message.isNoteOn() && range.contains (m.sample))
            ++noteOns;

    CHECK_MSG (noteOns == 2, juce::String (noteOns) + " note-ons in the marked region of the MIDI capture");

    // The tabs offer it, with the mark buttons beside it.
    MidiOutPanel midiOut (processor);
    NotationPanel notation (processor);
    CHECK (notation.getRangeBox().indexOfItemId (CaptureRanges::markedRegion) >= 0);
    CHECK (notation.getRangeBox().indexOfItemId (CaptureRanges::currentSection) >= 0);
    CHECK (midiOut.getRangeBox().indexOfItemId (CaptureRanges::markedRegion) >= 0);

    midiOut.getRangeBox().setSelectedId (CaptureRanges::markedRegion, juce::sendNotificationSync);
    CHECK (midiOut.getMarkInButton().isVisible() || ! midiOut.isVisible());
    CHECK (midiOut.chosenRange (performance) == range);

    // Clearing the take clears the marks: an unmarked region exports nothing, not everything.
    capture.clearTake();
    CHECK (! capture.hasMarkedRegion());
    CaptureRanges::apply (processor, CaptureRanges::markedRegion, 0.0, options);
    CHECK (options.sampleRange.getEnd() < 0);
}

LUTHIER_TEST (CaptureRanges, theCurrentSectionIsTheTunesSelectedSection)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    // A tune of two four-bar sections, the second selected: beats 16-32.
    auto tune = TuneTemplateLibrary::createBlank();

    for (const char* name : { "A", "B" })
    {
        TuneSection section;
        section.name = name;
        section.lengthBars = 4;
        tune.arrangement.sections.push_back (section);
        tune.arrangement.setlist.push_back ({ name, 1, {} });
    }

    processor.getTuneSession().newTune (tune);
    processor.getTuneSession().setSelectedSection (1);

    const auto ppq = CaptureRanges::currentSectionPpq (processor);
    CHECK_NEAR (ppq.getStart(), 16.0, 1.0e-9);
    CHECK_NEAR (ppq.getEnd(), 32.0, 1.0e-9);

    // A take played in time with the host: one note in each section.
    auto& capture = processor.getPerformanceCapture();
    capture.prepare (kSr);

    for (double beat : { 2.0, 18.0 })
    {
        CaptureClock clock;
        clock.sampleRate = kSr;
        clock.transportPlaying = true;
        clock.blockStartPpq = beat;
        clock.blockStartSample = (juce::int64) (beat * 24000.0);
        capture.beginBlock (clock);
        capture.noteOn (0, 1, 52, 2.0, 0.8f);
        capture.noteOff (1000, 1);
    }

    capture.drain();

    CaptureScoreOptions options;
    CaptureRanges::apply (processor, CaptureRanges::currentSection, 0.0, options);
    PerformanceScore score;
    capture.toScore (score, options);
    CHECK_MSG (score.getTotalNoteCount() == 1, juce::String (score.getTotalNoteCount()) + " notes in the current section");

    const auto samples = capture.sampleRangeForPpq (ppq);
    CHECK (samples.contains ((juce::int64) (18.0 * 24000.0)));
    CHECK (! samples.contains ((juce::int64) (2.0 * 24000.0)));
}
