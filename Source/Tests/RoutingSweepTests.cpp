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

    // The project root: the first folder above the working directory or the
    // test binary that holds the plugin's CMakeLists.txt.
    auto findCmake = []
    {
        for (auto start : { juce::File::getCurrentWorkingDirectory(),
                            juce::File::getSpecialLocation (juce::File::currentExecutableFile) })
            for (auto dir = start; dir.getFullPathName().length() > 1; dir = dir.getParentDirectory())
            {
                const auto candidate = dir.getChildFile ("CMakeLists.txt");

                if (candidate.existsAsFile() && candidate.loadFileAsString().contains ("juce_add_plugin(Luthier"))
                    return candidate;

                if (dir == dir.getParentDirectory())
                    break;
            }

        return juce::File();
    };

    const auto cmake = findCmake();

    if (! cmake.existsAsFile())
    {
        CHECK_MSG (false, "cannot find the project's CMakeLists.txt");
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

/*  IR-26 (input-routing 6): the sidechain reaches the main output only through
    a consumer (sidechain-to-amp, the monitor mix, a keyed effect). With every
    consumer off, a loud sidechain leaves the main out exactly as it was. */
LUTHIER_TEST (Routing, sidechainDoesNotReachTheMainOutUnconsumed)
{
    auto render = [] (float sidechainLevel)
    {
        LuthierAudioProcessor processor;

        auto layout = processor.getBusesLayout();

        if (layout.inputBuses.size() > 0)
            layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();

        processor.setBusesLayout (layout);
        processor.setRateAndBufferSizeDetails (kSr, kBlock);
        processor.prepareToPlay (kSr, kBlock);

        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                     processor.getTotalNumInputChannels()), kBlock);
        std::vector<float> mainOut;

        for (int block = 0; block < 40; ++block)
        {
            buffer.clear();

            auto sidechain = processor.getBusBuffer (buffer, true, 0);

            for (int ch = 0; ch < sidechain.getNumChannels(); ++ch)
                for (int i = 0; i < kBlock; ++i)
                    sidechain.setSample (ch, i, sidechainLevel * (float) std::sin (0.05 * (block * kBlock + i)));

            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 45, 0.8f), 0);

            processor.processBlock (buffer, midi);

            auto main = processor.getBusBuffer (buffer, false, 0);

            for (int i = 0; i < kBlock; ++i)
                mainOut.push_back (main.getSample (0, i));
        }

        return std::make_pair (mainOut, processor.getTotalNumInputChannels());
    };

    const auto silentA = render (0.0f);
    const auto silentB = render (0.0f);
    const auto loud = render (0.9f);

    CHECK_MSG (loud.second > 0, "the processor declined a sidechain input");

    double baseline = 0.0, leak = 0.0, level = 0.0;

    for (size_t i = 0; i < silentA.first.size(); ++i)
    {
        baseline = juce::jmax (baseline, (double) std::abs (silentA.first[i] - silentB.first[i]));
        leak = juce::jmax (leak, (double) std::abs (loud.first[i] - silentA.first[i]));
        level = juce::jmax (level, (double) std::abs (silentA.first[i]));
    }

    CHECK (level > 1.0e-3);   // there is a note to compare
    CHECK_MSG (leak <= baseline + 1.0e-6,
               "the sidechain moved the main out by " + juce::String (leak, 6)
                 + " (run-to-run difference " + juce::String (baseline, 6) + ")");
}
