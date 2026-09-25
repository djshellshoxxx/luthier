/*  The TUNE tab's export and the tune in the plugin's state (tune-builder.md
    8, 9, 12, 15): an offline render that nulls against live playback (15-07),
    the tune coming back with its file on relaunch (15-10), and the section
    state boundary reaching the processor (8). TunePanelTests drives the
    dialog; this is the machinery under it. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Tune/TuneExport.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    ChordCell chord (const char* symbol, double beats = 4.0)
    {
        ChordCell c;
        ProgressionError error = ProgressionError::none;
        parseChordSymbol (symbol, c, error);
        c.durationBeats = beats;
        return c;
    }

    /** Two sections at 240 bpm: a bar of Am and F with a melody, then a bar
        of G that asks for a state boundary. Four seconds in all. */
    Tune twoSectionTune (bool boundary)
    {
        Tune t;
        t.meta.title = "Rendered";
        t.meta.tempoBpm = 240.0;

        TuneSection verse;
        verse.name = "Verse";
        verse.lengthBars = 2;
        verse.chords = { chord ("Am"), chord ("F") };
        verse.rhythmPatternId = "Folk Down Up";
        verse.genreKitId = "Folk Fingerstyle";

        MelodyTrack melody;
        melody.notes.push_back (MelodyNote::make (0.0, 2.0, 76, 110));
        melody.notes.push_back (MelodyNote::make (4.0, 2.0, 77, 110));
        verse.melody = melody;

        TuneSection chorus;
        chorus.name = "Chorus";
        chorus.lengthBars = 2;
        chorus.chords = { chord ("G") };
        chorus.rhythmPatternId = "Classic Strum";
        chorus.genreKitId = "Folk Fingerstyle";
        chorus.stateBoundary = boundary;

        t.addSection (verse);
        t.addSection (chorus);
        return t;
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

    /** Runs a processor for `seconds` with no host play head, servicing the
        tune the way the processor's timer does. */
    std::vector<float> play (LuthierAudioProcessor& processor, double seconds, bool service = true)
    {
        std::vector<float> left;
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), kBlock);
        const int blocks = (int) (seconds * kSr / kBlock);

        for (int b = 0; b < blocks; ++b)
        {
            if (service)
                processor.serviceTune();

            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            for (int i = 0; i < kBlock; ++i)
                left.push_back (buffer.getSample (0, i));
        }

        return left;
    }

    double rmsDb (const std::vector<float>& signal)
    {
        double sum = 0.0;

        for (float v : signal)
            sum += (double) v * v;

        return juce::Decibels::gainToDecibels (std::sqrt (sum / (double) juce::jmax ((size_t) 1, signal.size())), -400.0);
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
/*  15-07: "Export audio: offline render matches live render within -80 dBFS
    RMS null." The offline instance is a second processor loaded with the
    live one's state, which carries the tune and the render intent
    (TuneSession): it plays the tune itself, once, from the top, servicing
    its rhythm changes from the render (TunePlayer's offline hook). */
LUTHIER_TEST (TuneExport, anOfflineRenderNullsAgainstLivePlayback)
{
    auto source = processorWith (twoSectionTune (false));

    // The state a render is made from, with and without the intent.
    source->getTuneSession().setRenderIntent (true);
    const auto state = source->captureStateBlock();
    source->getTuneSession().setRenderIntent (false);
    const auto plain = source->captureStateBlock();

    // Live: a fresh instance with that state, Play pressed, the tune serviced
    // as it goes - what a user hears after loading the session.
    auto live = std::make_unique<LuthierAudioProcessor>();
    live->setStateInformation (plain.getData(), (int) plain.getSize());
    live->setPlayConfigDetails (0, 2, kSr, kBlock);
    live->prepareToPlay (kSr, kBlock);
    CHECK (! live->getTunePlayer().isPlaying());
    live->getTunePlayer().setLoop (false);
    live->getTunePlayer().setCountInBars (0);
    live->getTunePlayer().play();
    const auto heard = play (*live, 4.5);
    CHECK_MSG (rmsDb (heard) > -60.0, "the live tune was silent: " + juce::String (rmsDb (heard), 1) + " dBFS");

    // Offline: the same state, with the intent, into a fresh instance.
    auto offline = std::make_unique<LuthierAudioProcessor>();
    offline->setStateInformation (state.getData(), (int) state.getSize());
    CHECK (offline->getTuneSession().isRendering());
    CHECK (offline->getTuneSession().getTune() == live->getTuneSession().getTune());
    CHECK (offline->getTunePlayer().isPlaying());
    CHECK (! offline->getTunePlayer().isLooping());

    offline->setPlayConfigDetails (0, 2, kSr, kBlock);
    offline->prepareToPlay (kSr, kBlock);
    const auto rendered = play (*offline, 4.5, false);   // nobody services it but the render itself

    CHECK_MSG (rmsDb (rendered) > -60.0, "the offline render was silent: " + juce::String (rmsDb (rendered), 1) + " dBFS");

    const double null = differenceDb (heard, rendered);
    CHECK_MSG (null < -80.0, "offline vs live null: " + juce::String (null, 1) + " dBFS RMS");

    // Both stopped at the end: no loop, no hanging note.
    CHECK (! offline->getTunePlayer().isPlaying());
    CHECK (offline->getTunePlayer().getNumSoundingNotes() == 0);

    // A state captured without the intent starts nothing (checked on `live` above).
    CHECK (! live->getTuneSession().isRendering());
}

//==============================================================================
/*  9.1's "every routing bus as its own file": the bus wrapper renders an
    aux bus as the stereo pair the exporter records. The main bus through
    the wrapper is the instance's own output. */
LUTHIER_TEST (TuneExport, theBusWrapperRendersOneBusAsStereo)
{
    auto live = processorWith (twoSectionTune (false));
    live->getTuneSession().setRenderIntent (true);
    const auto state = live->captureStateBlock();
    live->getTuneSession().setRenderIntent (false);

    CHECK (TuneExport::getStemNames (TuneStemChoice::mainStereo).size() == 1);
    CHECK (TuneExport::getStemNames (TuneStemChoice::everyBus).size() == 1 + kNumAuxStrips);
    CHECK (TuneExport::getStemBusIndex (0) == 0);
    CHECK (TuneExport::getStemBusIndex (1) == 1);
    CHECK (TuneExport::getStemBusIndex (kNumAuxStrips) == 1 + kNumAuxBuses + kNumPerStringBuses);

    // Bus 0 is the instance itself.
    auto direct = TuneExport::wrapForBus (LuthierAudioProcessor::createOfflineInstance(), 0);
    CHECK (dynamic_cast<LuthierAudioProcessor*> (direct.get()) != nullptr);

    // An aux bus renders through the wrapper without a fuss.
    auto wrapped = TuneExport::wrapForBus (LuthierAudioProcessor::createOfflineInstance(), TuneExport::getStemBusIndex (1));
    auto* wrapper = dynamic_cast<TuneBusRenderProcessor*> (wrapped.get());
    CHECK (wrapper != nullptr);

    if (wrapper == nullptr)
        return;

    wrapped->setStateInformation (state.getData(), (int) state.getSize());
    wrapped->setPlayConfigDetails (0, 2, kSr, kBlock);
    wrapped->prepareToPlay (kSr, kBlock);
    CHECK (wrapped->getTotalNumOutputChannels() == 2);

    juce::AudioBuffer<float> buffer (2, kBlock);
    bool finite = true;

    for (int b = 0; b < 40; ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        wrapped->processBlock (buffer, midi);

        for (int i = 0; i < kBlock; ++i)
            finite = finite && std::isfinite (buffer.getSample (0, i)) && std::isfinite (buffer.getSample (1, i));
    }

    CHECK (finite);
    wrapped->releaseResources();

    // The sequence that sets a render's length ends at the tune's end.
    const auto tune = twoSectionTune (false);
    const auto sequence = TuneExport::makeRenderSequence (tune);
    CHECK (sequence.getNumEvents() == 1);
    CHECK_NEAR (sequence.getEndTime(), TuneExport::getTuneLengthSeconds (tune), 1.0e-9);
    CHECK_NEAR (TuneExport::getTuneLengthSeconds (tune), 4.0, 1.0e-9);
}

//==============================================================================
/*  15-10: "Standalone reload: launch standalone, save a tune, close, relaunch,
    the last tune loads and plays." The standalone keeps the plugin's state
    between runs, and the state carries the tune and its file. */
LUTHIER_TEST (TuneExport, aRelaunchRestoresTheSavedTuneAndItPlays)
{
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuthierTuneRelaunch");
    folder.deleteRecursively();
    folder.createDirectory();
    const auto file = folder.getChildFile ("Last Tune.luthiertune");

    juce::MemoryBlock state;

    {
        auto first = processorWith (twoSectionTune (false));
        juce::String error;
        CHECK_MSG (first->getTuneSession().saveAs (file, error), error);
        CHECK (! first->getTuneSession().isDirty());
        first->getStateInformation (state);
    }

    auto second = std::make_unique<LuthierAudioProcessor>();
    second->setStateInformation (state.getData(), (int) state.getSize());
    second->prepareToPlay (kSr, kBlock);

    if (auto* humanise = second->getState().getParameter (ParamIDs::macroHumanize))
        humanise->setValueNotifyingHost (0.0f);

    auto& session = second->getTuneSession();
    CHECK (session.getTune().meta.title == "Rendered");
    CHECK (session.getFile() == file);
    CHECK (! session.isDirty());
    CHECK (! session.isRendering());
    CHECK (! second->getTunePlayer().isPlaying());   // nothing plays until asked

    second->getTunePlayer().play();
    const auto heard = play (*second, 1.5);
    CHECK_MSG (rmsDb (heard) > -60.0, "the restored tune did not play: " + juce::String (rmsDb (heard), 1) + " dBFS");

    // Save goes to the file it came from.
    session.edit (TuneEditClass::other, "Tempo", [] (Tune& t) { return t.setTempo (100.0); });
    juce::String error;
    CHECK_MSG (session.save (error), error);
    Tune back;
    CHECK (TuneFile::load (file, back).ok());
    CHECK_NEAR (back.meta.tempoBpm, 100.0, 1.0e-9);

    folder.deleteRecursively();
}

//==============================================================================
/*  8: "Section boundaries write a state boundary so mod matrix envelopes and
    rhythm engine phase reset if the user wants (per-section toggle)". The
    processor reads the player's flag in the block the section starts in. */
LUTHIER_TEST (TuneExport, aSectionStateBoundaryReachesTheProcessorOnTheSectionsFirstBlock)
{
    for (bool boundary : { true, false })
    {
        auto processor = processorWith (twoSectionTune (boundary));
        processor->getTunePlayer().setLoop (false);
        processor->getTunePlayer().play();

        // The chorus starts 8 beats in: 2 s at 240 bpm, sample 96000.
        const int expectedBlock = (int) (2.0 * kSr) / kBlock;
        std::vector<int> crossings;

        juce::AudioBuffer<float> buffer (juce::jmax (2, processor->getTotalNumOutputChannels()), kBlock);

        for (int b = 0; b < (int) (4.5 * kSr) / kBlock; ++b)
        {
            if (b % 3 == 0)
                processor->serviceTune();

            buffer.clear();
            juce::MidiBuffer midi;
            processor->processBlock (buffer, midi);

            if (processor->getTunePlayer().crossedStateBoundary())
                crossings.push_back (b);
        }

        if (boundary)
        {
            CHECK_MSG (crossings.size() == 1, "boundary crossings: " + juce::String ((int) crossings.size()));

            if (crossings.size() == 1)
                CHECK_MSG (std::abs (crossings[0] - expectedBlock) <= 1,
                           "crossed in block " + juce::String (crossings[0]) + ", expected " + juce::String (expectedBlock));
        }
        else
        {
            CHECK (crossings.empty());
        }
    }
}
