/*  RT audit F-A1: the rhythm engine's strokes reach the plugin's MIDI output
    exactly once, at the sample they are played, whatever the host's block
    size; and switching the rhythm engine off ends them, releasing every note
    the host was left holding.

    Before the fix the strokes had two writers into MIDI out's rhythm source
    (LuthierEngine's deferred writer and a second copy in processSlice), so a
    host recording the plugin's MIDI got every stroke twice; and the engine's
    per-block stroke list was only cleared while the rhythm engine was on, so
    after it was switched off the second writer re-sent the last block's
    strokes every block.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Rhythm/Patterns.h"

#include <map>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr double kBpm = 120.0;
    constexpr int kSamplesPerBeat = 24000;   // 120 bpm at 48 kHz

    // Where the rhythm engine is switched off: a multiple of every block size
    // tested (37 * 2048 * 2), so each render switches it at the same sample.
    // Mid-way between beats 6 and 7, with the beat-6 strum still ringing.
    constexpr int64_t kSwitchOffAt = 151552;
    constexpr int64_t kRenderEnd = kSwitchOffAt + 2 * kSamplesPerBeat;
    constexpr int kChord[] = { 40, 47, 52, 56, 59, 64 };

    struct TransportHead : juce::AudioPlayHead
    {
        int64_t position = 0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (true);
            info.setBpm (kBpm);
            info.setPpqPosition ((double) position / (double) kSamplesPerBeat);
            info.setTimeInSamples (position);
            info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
            return info;
        }
    };

    struct OutEvent
    {
        int64_t time = 0;
        int status = 0, data1 = 0, data2 = 0;

        bool operator== (const OutEvent& o) const noexcept
        {
            return time == o.time && status == o.status && data1 == o.data1 && data2 == o.data2;
        }

        bool isNoteOn() const noexcept  { return (status & 0xf0) == 0x90 && data2 > 0; }
        bool isNoteOff() const noexcept { return (status & 0xf0) == 0x80 || ((status & 0xf0) == 0x90 && data2 == 0); }
        int channel() const noexcept    { return (status & 0x0f) + 1; }

        juce::String describe() const
        {
            return juce::String (time) + (isNoteOn() ? " on " : isNoteOff() ? " off " : " ? ")
                   + "ch" + juce::String (channel()) + " n" + juce::String (data1);
        }
    };

    RhythmPattern quarterDownStrums()
    {
        RhythmPattern pattern;
        pattern.setName ("Continuity Quarter Downs");
        pattern.setKind (RhythmPattern::Kind::strum);
        pattern.setSubdivision (Subdivision::sixteenth);
        pattern.setLength (16);
        pattern.setSwing (0.5);

        for (int i = 0; i < 16; ++i)
        {
            StrumStep step;
            step.type = (i % 4 == 0) ? StrumType::down : StrumType::rest;
            step.dynamic = 1.0;
            step.stringMask = 0x0FFF;
            pattern.setStrumStep (i, step);
        }

        return pattern;
    }

    /** The plugin's MIDI output, on the host's timeline, for a held E major
        chord strummed in quarters at 120 bpm with only the rhythm source on;
        the rhythm engine is switched off at kSwitchOffAt. */
    std::vector<OutEvent> renderRhythmMidiOut (int blockSize)
    {
        LuthierAudioProcessor processor;
        TransportHead head;
        processor.setPlayHead (&head);

        if (auto* h = processor.getState().getParameter (ParamIDs::macroHumanize))
            h->setValueNotifyingHost (0.0f);

        processor.prepareToPlay (kSr, blockSize);

        MidiOutConfig cfg;
        cfg.enabled = true;
        cfg.passThrough = false;
        cfg.rhythmEngine = true;
        cfg.stringActivity = false;
        cfg.ccBroadcast = false;
        processor.getRouting().setMidiOutConfig (cfg);

        auto& rhythm = processor.getEngine().getRhythmEngine();
        RhythmHumanise exact;
        exact.timingMs = 0.0;
        exact.velocityPercent = 0.0;
        exact.amount = 0.0;
        rhythm.setHumanise (exact);
        rhythm.setSeed (1);
        rhythm.setPattern (quarterDownStrums());
        rhythm.setEnabled (true);

        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                     processor.getTotalNumOutputChannels()), blockSize);
        juce::MidiBuffer midi;
        processor.reserveMidiOutputBuffer (midi);

        std::vector<OutEvent> out;

        for (int64_t position = 0; position < kRenderEnd; position += blockSize)
        {
            if (position == kSwitchOffAt)
                rhythm.setEnabled (false);

            midi.clear();

            if (position == 0)
                for (int note : kChord)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            head.position = position;
            buffer.clear();
            processor.processBlock (buffer, midi);

            for (const auto m : midi)
            {
                OutEvent e;
                e.time = position + m.samplePosition;
                e.status = m.numBytes > 0 ? m.data[0] : 0;
                e.data1 = m.numBytes > 1 ? m.data[1] : 0;
                e.data2 = m.numBytes > 2 ? m.data[2] : 0;
                out.push_back (e);
            }
        }

        return out;
    }

    /** Every check the stream must pass on its own. */
    void checkStream (const std::vector<OutEvent>& out, int blockSize)
    {
        const auto tag = "[block " + juce::String (blockSize) + "] ";

        // Per (channel, note): on, off, on, off ... never a second on before
        // its off, never an off with nothing on, and nothing left on at the end.
        std::map<std::pair<int, int>, int> held;
        int doubledOns = 0, orphanOffs = 0, onsAfterSwitchOff = 0, lateOffs = 0;
        juce::String firstProblem;

        auto note = [&firstProblem] (const OutEvent& e, const char* what)
        {
            if (firstProblem.isEmpty())
                firstProblem = juce::String (what) + " at " + e.describe();
        };

        for (const auto& e : out)
        {
            if (! e.isNoteOn() && ! e.isNoteOff())
                continue;

            auto& h = held[{ e.channel(), e.data1 }];

            if (e.isNoteOn())
            {
                if (e.time >= kSwitchOffAt) { ++onsAfterSwitchOff; note (e, "note-on after switch-off"); }
                if (h > 0)                  { ++doubledOns; note (e, "second note-on"); }
                ++h;
            }
            else
            {
                if (h == 0) { ++orphanOffs; note (e, "note-off with nothing on"); }
                else        --h;

                // The switch-off releases at once: nothing trails in later.
                if (e.time > kSwitchOffAt) { ++lateOffs; note (e, "note-off after the switch-off sample"); }
            }
        }

        int stuck = 0;

        for (const auto& [key, count] : held)
            stuck += count;

        CHECK_MSG (doubledOns == 0, tag + juce::String (doubledOns) + " doubled note-ons; first problem: " + firstProblem);
        CHECK_MSG (orphanOffs == 0, tag + juce::String (orphanOffs) + " note-offs with nothing on; first problem: " + firstProblem);
        CHECK_MSG (onsAfterSwitchOff == 0, tag + juce::String (onsAfterSwitchOff) + " note-ons after the rhythm engine was switched off");
        CHECK_MSG (lateOffs == 0, tag + juce::String (lateOffs) + " note-offs later than the switch-off");
        CHECK_MSG (stuck == 0, tag + juce::String (stuck) + " notes left on at the host");

        // Seven quarter strums before the switch-off (beats 0..6), each one
        // starting exactly on its beat and touching each sounding string once.
        std::vector<int64_t> onTimes;
        std::map<int, int> onsPerChannel;

        for (const auto& e : out)
            if (e.isNoteOn())
            {
                onTimes.push_back (e.time);
                ++onsPerChannel[e.channel()];
            }

        const int strings = (int) onsPerChannel.size();
        CHECK_MSG (strings >= 4, tag + juce::String (strings) + " string channels carried the strum");
        CHECK_MSG ((int) onTimes.size() == 7 * strings,
                   tag + juce::String ((int) onTimes.size()) + " note-ons for 7 strums on " + juce::String (strings) + " strings");

        for (const auto& [channel, count] : onsPerChannel)
            CHECK_MSG (count == 7, tag + "channel " + juce::String (channel) + " carried " + juce::String (count) + " note-ons, not 7");

        for (int beat = 0; beat < 7; ++beat)
        {
            const int64_t start = (int64_t) beat * kSamplesPerBeat;
            int64_t first = -1;
            int inWindow = 0;

            for (auto t : onTimes)
                if (t >= start && t < start + kSamplesPerBeat)
                {
                    if (first < 0 || t < first) first = t;
                    ++inWindow;
                }

            CHECK_MSG (first == start, tag + "beat " + juce::String (beat) + " strum starts at " + juce::String (first));
            CHECK_MSG (inWindow == strings, tag + "beat " + juce::String (beat) + " has " + juce::String (inWindow) + " note-ons");
        }

        // Time order: the stream a host records is sorted.
        for (size_t i = 1; i < out.size(); ++i)
            if (out[i].time < out[i - 1].time)
            {
                CHECK_MSG (false, tag + "out of order: " + out[i - 1].describe() + " then " + out[i].describe());
                break;
            }
    }
}

LUTHIER_TEST (RhythmMidiOut, strokesReachMidiOutOnceAndStopWithTheEngine)
{
    const auto out = renderRhythmMidiOut (512);
    checkStream (out, 512);
}

/*  The same performance at four block sizes is the same MIDI stream, event for
    event (a block size of 1 is what a host's sample-accurate automation can
    ask for; 37 lands every boundary off the grid). */
LUTHIER_TEST (RhythmMidiOut, streamIsIndependentOfTheBlockSize)
{
    const auto reference = renderRhythmMidiOut (512);

    for (int blockSize : { 1, 37, 2048 })
    {
        const auto out = renderRhythmMidiOut (blockSize);
        checkStream (out, blockSize);

        CHECK_MSG (out.size() == reference.size(),
                   "[block " + juce::String (blockSize) + "] " + juce::String ((int) out.size())
                   + " events, 512 gave " + juce::String ((int) reference.size()));

        for (size_t i = 0; i < juce::jmin (out.size(), reference.size()); ++i)
            if (! (out[i] == reference[i]))
            {
                CHECK_MSG (false, "[block " + juce::String (blockSize) + "] event " + juce::String ((int) i)
                                  + " is " + out[i].describe() + ", 512 gave " + reference[i].describe());
                break;
            }
    }
}
