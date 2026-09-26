/*  SPEC-SWEEP routing-io / MIDI-out checks: the rhythm engine's strokes reach
    MIDI out's rhythm source (rhythm-engine RE-41), and the plugin declares the
    MIDI output it produces to the host (PROGRESS PR-13).
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Rhythm/Patterns.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    /** Renders @p blocks with a held E major chord and counts note-ons that come
        out of the plugin, and the distinct channels they are on. */
    int countRhythmNoteOnsOut (bool rhythmSourceOn, juce::SortedSet<int>* channels = nullptr,
                               int* outOfBlock = nullptr)
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, kBlock);

        MidiOutConfig cfg;
        cfg.enabled = true;
        cfg.passThrough = false;
        cfg.rhythmEngine = rhythmSourceOn;
        cfg.stringActivity = false;
        cfg.ccBroadcast = false;
        processor.getRouting().setMidiOutConfig (cfg);

        auto& rhythm = processor.getEngine().getRhythmEngine();
        PatternLibrary patterns;
        const auto strums = patterns.findByKind (RhythmPattern::Kind::strum);

        if (strums.isEmpty())
            return -1;

        rhythm.setPattern (patterns.getPattern (strums[0]));
        rhythm.setFreeRun (true);
        rhythm.setEnabled (true);

        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                     processor.getTotalNumOutputChannels()), kBlock);
        int noteOns = 0;

        for (int block = 0; block < 200; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                for (int note : { 40, 47, 52, 56, 59, 64 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            for (const auto m : midi)
            {
                const auto message = m.getMessage();

                if (message.isNoteOn())
                {
                    ++noteOns;

                    if (channels != nullptr)
                        channels->add (message.getChannel());

                    if (outOfBlock != nullptr && (m.samplePosition < 0 || m.samplePosition >= kBlock))
                        ++*outOfBlock;
                }
            }
        }

        return noteOns;
    }
}

/*  RE-41 / routing-io 6: MIDI out's "rhythm engine" source carries the strokes
    the rhythm engine plays, one channel per string; switching the source off
    removes them. The buffer it reads was never filled. */
LUTHIER_TEST (Routing, rhythmSourceCarriesTheStrum)
{
    juce::SortedSet<int> channels;
    int outOfBlock = 0;
    const int withRhythm = countRhythmNoteOnsOut (true, &channels, &outOfBlock);
    CHECK (outOfBlock == 0);
    const int without = countRhythmNoteOnsOut (false);

    CHECK_MSG (withRhythm > 12, "only " + juce::String (withRhythm) + " rhythm note-ons reached MIDI out");
    CHECK_MSG (channels.size() >= 3, juce::String (channels.size()) + " distinct string channels");
    CHECK_MSG (without == 0, juce::String (without) + " note-ons with the rhythm source (and pass-through) off");
}

/*  PR-13: producesMidi() is true, so the plugin wrappers must be built with
    NEEDS_MIDI_OUTPUT TRUE (JucePlugin_ProducesMidiOutput). Otherwise JUCE's VST3
    wrapper declares no event output bus and the AU no MIDI output callback, and
    MIDI out never leaves the plugin in a host. The test target is a console app
    without the plugin's JucePlugin_* definitions, so it reads the build file. */
LUTHIER_TEST (PluginBuses, midiOutputIsDeclaredToTheHost)
{
    LuthierAudioProcessor processor;
    CHECK (processor.producesMidi());
    CHECK (processor.acceptsMidi());

    const auto cmake = juce::File (__FILE__).getParentDirectory().getParentDirectory()
                                            .getParentDirectory().getChildFile ("CMakeLists.txt");

    if (! cmake.existsAsFile())
    {
        CHECK_MSG (false, "cannot find " + cmake.getFullPathName());
        return;
    }

    const auto text = cmake.loadFileAsString();

    auto flag = [&text] (const char* name)
    {
        for (const auto& line : juce::StringArray::fromLines (text))
        {
            const auto trimmed = line.trim();

            if (trimmed.startsWith (name))
                return trimmed.fromFirstOccurrenceOf (name, false, false).trim()
                              .upToFirstOccurrenceOf (" ", false, false).trim();
        }

        return juce::String();
    };

    CHECK_MSG (flag ("NEEDS_MIDI_OUTPUT") == "TRUE",
               "NEEDS_MIDI_OUTPUT is '" + flag ("NEEDS_MIDI_OUTPUT") + "' but producesMidi() is true");
    CHECK_MSG (flag ("NEEDS_MIDI_INPUT") == "TRUE", "NEEDS_MIDI_INPUT does not match acceptsMidi()");
}
