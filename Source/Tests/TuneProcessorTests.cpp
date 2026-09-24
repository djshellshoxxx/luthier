/*  The TUNE tab wired into the plugin (tune-builder.md 3.6, 8 and 15;
    practice-tools 0.2): what the processor does with the player's MIDI and
    clicks, and what it keeps in its state. TunePlayerTests and TunePanelTests
    cover the player and the panel on their own. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/PracticePanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    ChordCell chord (const char* symbol, double beats = 4.0)
    {
        ChordCell c;
        ProgressionError error = ProgressionError::none;
        parseChordSymbol (symbol, c, error);
        c.durationBeats = beats;
        return c;
    }

    /** Two bars of Am and F, strummed, with a held melody note in each bar. */
    Tune strummedTuneWithAMelody()
    {
        Tune t;
        t.meta.title = "Wired";
        t.meta.tempoBpm = 120.0;

        TuneSection verse;
        verse.name = "Verse";
        verse.lengthBars = 2;
        verse.chords = { chord ("Am"), chord ("F") };
        verse.rhythmPatternId = "Folk Down Up";
        verse.genreKitId = "Folk Fingerstyle";

        MelodyTrack melody;
        melody.notes.push_back (MelodyNote::make (0.0, 3.0, 76, 120));
        melody.notes.push_back (MelodyNote::make (4.0, 3.0, 77, 120));
        verse.melody = melody;

        t.addSection (verse);
        return t;
    }

    struct Played
    {
        std::vector<float> left;
        juce::MidiBuffer midiOut;
        bool rhythmDrove = false;
    };

    /** Runs the processor for `seconds` with no host play head (the tune's
        own clock), servicing the tune at the timer's rate. */
    Played play (LuthierAudioProcessor& processor, double seconds)
    {
        Played played;
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), kBlock);
        const int blocks = (int) (seconds * kSr / kBlock);

        for (int b = 0; b < blocks; ++b)
        {
            if (b % 6 == 0)
                processor.serviceTune();

            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            played.rhythmDrove = played.rhythmDrove || processor.getEngine().getRhythmEngine().isDriving();
            played.midiOut.addEvents (midi, 0, kBlock, b * kBlock);

            for (int i = 0; i < kBlock; ++i)
                played.left.push_back (buffer.getSample (0, i));
        }

        return played;
    }

    std::unique_ptr<LuthierAudioProcessor> processorWith (const Tune& tune)
    {
        auto processor = std::make_unique<LuthierAudioProcessor>();
        processor->prepareToPlay (kSr, kBlock);

        if (auto* humanise = processor->getState().getParameter (ParamIDs::macroHumanize))
            humanise->setValueNotifyingHost (0.0f);

        processor->getTuneSession().newTune (tune);
        return processor;
    }

    double rmsDb (const std::vector<float>& signal, size_t from = 0)
    {
        double sum = 0.0;

        for (size_t i = from; i < signal.size(); ++i)
            sum += (double) signal[i] * signal[i];

        return juce::Decibels::gainToDecibels (std::sqrt (sum / (double) juce::jmax ((size_t) 1, signal.size() - from)), -400.0);
    }

    double differenceDb (const std::vector<float>& a, const std::vector<float>& b)
    {
        std::vector<float> d (juce::jmin (a.size(), b.size()));

        for (size_t i = 0; i < d.size(); ++i)
            d[i] = a[i] - b[i];

        return rmsDb (d);
    }
}

//==============================================================================
/*  8 and TuneMidi.h's wiring note: while the rhythm engine strums the chord
    channel, the melody still sounds - it reaches the engine as direct notes,
    which the rhythm engine's stream does not replace. */
LUTHIER_TEST (TuneProcessor, theMelodySoundsWhileTheRhythmEngineStrums)
{
    auto withMelody = processorWith (strummedTuneWithAMelody());
    auto without = processorWith (strummedTuneWithAMelody());

    without->getTuneSession().getMidiOptions().includeMelody = false;
    without->getTuneSession().rebuildTimeline();

    withMelody->getTunePlayer().play();
    without->getTunePlayer().play();

    const auto a = play (*withMelody, 3.0);
    const auto b = play (*without, 3.0);

    CHECK_MSG (a.rhythmDrove && b.rhythmDrove, "the rhythm engine never strummed the tune's chords");
    CHECK_MSG (rmsDb (b.left) > -60.0, "the strummed chords were silent: " + juce::String (rmsDb (b.left), 1) + " dBFS");

    // The same strum either way, so what differs is the melody.
    const double melody = differenceDb (a.left, b.left);
    CHECK_MSG (melody > -40.0, "the melody added only " + juce::String (melody, 1) + " dBFS to the strum");
}

/*  midi-export 6 / tune-builder 8: with MIDI out's TUNE source on, the tune's
    parts go out on their own channels; off, they do not. */
LUTHIER_TEST (TuneProcessor, midiOutCarriesTheTuneWhenAskedTo)
{
    for (const bool tuneSource : { true, false })
    {
        auto processor = processorWith (strummedTuneWithAMelody());

        auto config = processor->getRouting().getMidiOutConfig();
        config.enabled = true;
        config.tunePlayback = tuneSource;
        processor->getRouting().setMidiOutConfig (config);

        processor->getTunePlayer().play();
        const auto played = play (*processor, 1.0);

        bool melodyOut = false, chordOut = false;

        for (const auto metadata : played.midiOut)
        {
            const auto m = metadata.getMessage();

            if (m.isNoteOn() && m.getChannel() == 2 && m.getNoteNumber() == 76)
                melodyOut = true;

            if (m.isNoteOn() && m.getChannel() == 1)
                chordOut = true;
        }

        CHECK_MSG (melodyOut == tuneSource && chordOut == tuneSource,
                   juce::String ("TUNE source ") + (tuneSource ? "on" : "off") + ": melody out "
                     + (melodyOut ? "yes" : "no") + ", chords out " + (chordOut ? "yes" : "no"));
    }
}

/*  3.6 and practice-tools 0.2: the count-in clicks are heard. Sent to the main
    out, they are on it; left on the monitor bus of a layout that has one, the
    main out has none of them. */
LUTHIER_TEST (TuneProcessor, theCountInIsHeardOnTheMainOutWhenSentThere)
{
    // No chords and no melody: during the count-in the clicks are all there is.
    Tune silent = strummedTuneWithAMelody();
    silent.arrangement.sections[0].melody = MelodyTrack {};

    auto measure = [&silent] (bool toMain)
    {
        auto processor = processorWith (silent);
        processor->getTuneSession().getMidiOptions().includeChords = false;
        processor->getTuneSession().rebuildTimeline();

        processor->setClickToMain (toMain);
        processor->getTunePlayer().setCountInBars (1);
        processor->getTunePlayer().play();

        // A bar at 120 bpm is 2 s; the first 1.5 s are count-in only.
        return rmsDb (play (*processor, 1.5).left);
    };

    const double onMain = measure (true);
    CHECK_MSG (onMain > -50.0, "the count-in on the main out is " + juce::String (onMain, 1) + " dBFS");

    auto probe = std::make_unique<LuthierAudioProcessor>();
    probe->prepareToPlay (kSr, kBlock);

    if (RoutingMatrix::layoutHasAux (probe->getRouting().getActiveLayout()))
    {
        const double onMonitor = measure (false);
        CHECK_MSG (onMonitor < onMain - 30.0,
                   "left on the monitor bus, the clicks still reach the main out at "
                     + juce::String (onMonitor, 1) + " dBFS");
    }
    else
    {
        // No monitor bus (stereo only): the click goes to the main out anyway.
        CHECK (std::abs (measure (false) - onMain) < 1.0);
    }
}

/*  15: "save a tune, close, relaunch, the last tune loads" - the tune and
    the click route travel in the plugin's state. */
LUTHIER_TEST (TuneProcessor, thePluginStateKeepsTheTuneAndTheClickRoute)
{
    auto source = processorWith (strummedTuneWithAMelody());
    source->setClickToMain (true);

    juce::MemoryBlock state;
    source->getStateInformation (state);

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->setStateInformation (state.getData(), (int) state.getSize());

    CHECK (restored->getTuneSession().getTune() == source->getTuneSession().getTune());
    CHECK (restored->getTuneSession().getTune().meta.title == "Wired");
    CHECK (restored->isClickToMain());
}

/*  practice-tools 0.2's "optionally route to the main out": the metronome
    tab's CLICK TO MAIN switch is the setting. */
LUTHIER_TEST (TuneProcessor, theMetronomeTabSendsTheClickToTheMainOut)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    MetronomeTab tab (*processor);
    tab.setSize (600, 300);
    tab.refresh();

    juce::TextButton* toMain = nullptr;
    std::function<void (juce::Component&)> find = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* b = dynamic_cast<juce::TextButton*> (child); b != nullptr && b->getButtonText() == "CLICK TO MAIN")
                toMain = b;

            find (*child);
        }
    };
    find (tab);

    CHECK (toMain != nullptr);

    if (toMain == nullptr)
        return;

    CHECK (! toMain->getToggleState() && ! processor->isClickToMain());

    toMain->setToggleState (true, juce::dontSendNotification);
    toMain->onClick();
    CHECK (processor->isClickToMain());

    processor->setClickToMain (false);
    tab.refresh();
    CHECK (! toMain->getToggleState());
}
