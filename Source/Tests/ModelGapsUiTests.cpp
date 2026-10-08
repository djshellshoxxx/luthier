/*  MODEL-GAPS workstream, the parts that need the plugin or its panels (TODO
    2k, 2d): the FeedbackLed's drain rate, VP-7-04 through the real panels, the
    doubler's defaults, notation export on a worker thread, the migration
    backup on load, the Aux 1 pre/post-circuit toggle, the sustain controls
    as modulation destinations and the capture fed by the engine itself.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../DSP/Effects/PedalsMod.h"
#include "../UI/Widgets.h"
#include "../UI/AmpFacePanel.h"
#include "../UI/PedalRack.h"
#include "../UI/NotationPanel.h"
#include "../UI/RoutingPanel.h"
#include "../UI/FretboardComponent.h"
#include "../UI/Faces/AmpFace.h"
#include "../UI/Faces/PedalFace.h"
#include "../Modulation/ModMatrix.h"
#include "../Routing/TapBuffers.h"
#include "../Export/LuthierMidiEvents.h"


using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    void setPlain (LuthierAudioProcessor& processor, const juce::String& id, float plain)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    juce::Image snapshot (juce::Component& c)
    {
        juce::Image image (juce::Image::ARGB, juce::jmax (1, c.getWidth()), juce::jmax (1, c.getHeight()), true,
                           juce::SoftwareImageType());
        juce::Graphics g (image);
        c.paintEntireComponent (g, true);
        return image;
    }

    /** Plays a short phrase through the whole plugin. */
    void playPhrase (LuthierAudioProcessor& processor, std::initializer_list<int> notes)
    {
        processor.prepareToPlay (kSr, kBlock);
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);
        const std::vector<int> list (notes);

        for (int b = 0; b < 40 * (int) list.size() + 20; ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (b % 40 == 0 && b / 40 < (int) list.size())
                midi.addEvent (juce::MidiMessage::noteOn (1, list[(size_t) (b / 40)], (juce::uint8) 100), 10);

            if (b % 40 == 30 && b / 40 < (int) list.size())
                midi.addEvent (juce::MidiMessage::noteOff (1, list[(size_t) (b / 40)]), 0);

            processor.processBlock (buffer, midi);
        }

        processor.drainPerformanceCapture();
    }
}

//==============================================================================
/*  gui-engine-dataflow.md 22: "UI drain: 30 Hz". */
LUTHIER_TEST (ModelGapsUi, theFeedbackLedDrainsAtThirtyHertz)
{
    LuthierAudioProcessor processor;
    FeedbackLed led (processor);

    CHECK (FeedbackLed::kRefreshHz == 30);
    CHECK_MSG (led.getRefreshIntervalMs() == 1000 / 30, "interval " + juce::String (led.getRefreshIntervalMs()) + " ms");
}

/*  ambiguity-resolutions 3: the doubler's defaults, all of them. */
LUTHIER_TEST (ModelGapsUi, theDoublerDefaultsAreTheClassicAdt)
{
    DoublerPedal pedal;
    enum { kDelay = 0, kPitch, kPan, kWidth, kMix, kHp, kLp };

    CHECK_NEAR (pedal.getParameterDescriptor (kDelay).defaultValue, 22.0, 1.0e-9);
    CHECK_NEAR (pedal.getParameterDescriptor (kPitch).defaultValue, -8.0, 1.0e-9);
    CHECK_NEAR (pedal.getParameterDescriptor (kPan).defaultValue, -0.7, 1.0e-9);
    CHECK_NEAR (pedal.getParameterDescriptor (kWidth).defaultValue, 1.0, 1.0e-9);   // stereo
    CHECK_NEAR (pedal.getParameterDescriptor (kMix).defaultValue, 40.0, 1.0e-9);
    CHECK_NEAR (pedal.getParameterDescriptor (kHp).defaultValue, 100.0, 1.0e-9);
    CHECK_NEAR (pedal.getParameterDescriptor (kLp).defaultValue, 8000.0, 1.0e-9);

    // The ranges the spec gives.
    CHECK (pedal.getParameterDescriptor (kPitch).minValue == -25.0 && pedal.getParameterDescriptor (kPitch).maxValue == 25.0);
    CHECK (pedal.getParameterDescriptor (kHp).minValue == 20.0 && pedal.getParameterDescriptor (kHp).maxValue == 500.0);
    CHECK (pedal.getParameterDescriptor (kLp).minValue == 2000.0 && pedal.getParameterDescriptor (kLp).maxValue == 20000.0);

    // A fresh pedal is at its defaults.
    pedal.prepare (kSr, kBlock);
    CHECK_NEAR (pedal.getParameterValue (kPitch), -8.0, 1.0e-6);
    CHECK_NEAR (pedal.getParameterValue (kHp), 100.0, 1.0e-3);
    CHECK_NEAR (pedal.getParameterValue (kLp), 8000.0, 1.0e-2);
}

/*  VP-7-04 at panel level: the plugin's own Standby and bypass change the
    pilot light and the pedal LED on the real panels. */
LUTHIER_TEST (ModelGapsUi, standbyAndBypassReachTheFacesOnThePanels)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    {
        AmpFacePanel amp (processor, AmpFacePanel::Style::section);
        amp.setSize (234, AmpFacePanel::sectionHeight);

        setPlain (processor, ParamIDs::ampStandby, 0.0f);
        amp.refresh();
        const auto on = snapshot (amp);
        const bool stateOn = amp.getFaceState().standby;

        setPlain (processor, ParamIDs::ampStandby, 1.0f);
        amp.refresh();
        const auto standby = snapshot (amp);
        const bool stateStandby = amp.getFaceState().standby;

        CHECK (! stateOn && stateStandby);

        const auto pilot = amp.getFaceLayout().pilot.getCentre().roundToInt();
        CHECK_MSG (on.getPixelAt (pilot.x, pilot.y) != standby.getPixelAt (pilot.x, pilot.y),
                   "the pilot light does not follow the plugin's Standby");
    }

    {
        processor.getEngine().getPostEffects().setSlotType (0, PedalType::Doubler);
        setPlain (processor, ParamIDs::slotType (true, 0), (float) (int) PedalType::Doubler);

        PedalRack rack (processor, true);
        rack.setSize (PedalSlotComponent::nominalWidth, 100);
        rack.refresh();
        rack.setSize (PedalSlotComponent::nominalWidth, rack.getPreferredHeight());
        rack.resized();

        auto* slot = rack.getSlot (0);
        CHECK (slot != nullptr && slot->getShownType() == PedalType::Doubler);

        if (slot != nullptr)
        {
            setPlain (processor, ParamIDs::slotBypass (true, 0), 0.0f);
            slot->refresh();
            const auto active = snapshot (*slot);

            setPlain (processor, ParamIDs::slotBypass (true, 0), 1.0f);
            slot->refresh();
            const auto bypassed = snapshot (*slot);

            const auto led = slot->getFaceLayout().led.getCentre().roundToInt();
            CHECK_MSG (active.getPixelAt (led.x, led.y) != bypassed.getPixelAt (led.x, led.y),
                       "the doubler's LED does not follow the plugin's bypass");
        }
    }
}

//==============================================================================
/*  notation-export 0.1: "Notation export is offline. It runs on a worker thread." */
LUTHIER_TEST (ModelGapsUi, notationExportRunsOnAWorkerThread)
{
    LuthierAudioProcessor processor;
    playPhrase (processor, { 52, 55, 57 });

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-model-gaps-export");
    dir.deleteRecursively();
    dir.createDirectory();

    for (auto format : { NotationFormat::musicXml, NotationFormat::asciiTab, NotationFormat::midi })
    {
        const auto file = dir.getChildFile (juce::String ("take") + getNotationFormatExtension (format));
        bool called = false, result = false;
        juce::String error;

        const bool started = NotationTakeExport::writeAsync (processor, format, file, {}, {},
                                                             // Delivered later on the message thread, after this
                                                             // test: it must not touch the test's locals.
                                                             [] (bool, const juce::String&) {}, &error);
        CHECK_MSG (started, error);

        // The report is posted to the message thread; pumping the whole system
        // queue here would dispatch other tests' leftovers, so the test waits
        // for the worker to finish writing instead.
        for (int i = 0; i < 2000 && (NotationTakeExport::isBusy() || ! file.existsAsFile()); ++i)
            juce::Thread::sleep (2);

        called = result = ! NotationTakeExport::isBusy() && file.existsAsFile();

        CHECK_MSG (called && result, getNotationFormatExtension (format) + juce::String (": ") + error);
        CHECK (file.existsAsFile() && file.getSize() > 0);
        CHECK (NotationTakeExport::getLastWorkerThread() != nullptr);
        CHECK (NotationTakeExport::getLastWorkerThread() != juce::Thread::getCurrentThreadId());
        CHECK (! NotationTakeExport::isBusy());
    }

    // Nothing captured: refused at once, nothing called back.
    LuthierAudioProcessor empty;
    empty.prepareToPlay (kSr, kBlock);
    juce::String error;
    CHECK (! NotationTakeExport::writeAsync (empty, NotationFormat::musicXml, dir.getChildFile ("none.musicxml"),
                                             {}, {}, [] (bool, const juce::String&) {}, &error));
    CHECK (error.isNotEmpty());

    dir.deleteRecursively();
}

//==============================================================================
/*  file-formats.md 2: "Backup on migration: original file moved to
    Presets/Backup/<yyyy-mm-dd>/<name>-v<schema>.luthierpreset". */
LUTHIER_TEST (ModelGapsUi, aMigratedPresetKeepsItsOriginal)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    auto& presets = processor.getPresetManager();

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-model-gaps-migrate");
    dir.deleteRecursively();
    dir.createDirectory();

    // A current preset: saved now, it needs no migration and gets no backup.
    auto current = presets.toVar ("Current", "Test", "", {});
    CHECK (! PresetManager::needsMigration (current));

    // The same without its ranges block: a schema 1 (pre-M42) file.
    auto old = juce::JSON::parse (juce::JSON::toString (current));
    old.getDynamicObject()->removeProperty ("ranges");
    CHECK (PresetManager::needsMigration (old));

    const auto file = dir.getChildFile ("Old Sound.luthierpreset");
    CHECK (file.replaceWithText (juce::JSON::toString (old)));
    const auto originalText = file.loadFileAsString();

    CHECK (presets.loadPreset (file));

    const auto backup = presets.getLastMigrationBackup();
    CHECK_MSG (backup.existsAsFile(), "no migration backup was filed");
    CHECK (backup.getFileName() == "Old Sound-v1.luthierpreset");
    CHECK (backup.getParentDirectory().getParentDirectory() == dir.getChildFile ("Backup"));
    CHECK (backup.getParentDirectory().getFileName() == juce::Time::getCurrentTime().formatted ("%Y-%m-%d"));
    CHECK (backup.loadFileAsString() == originalText);

    // Loading it again does not file a second copy.
    CHECK (presets.loadPreset (file));
    int copies = 0;

    for (const auto& entry : juce::RangedDirectoryIterator (dir.getChildFile ("Backup"), true, "*.luthierpreset"))
        copies += entry.getFile().existsAsFile() ? 1 : 0;

    CHECK (copies == 1);

    // A current file gets none.
    const auto fresh = dir.getChildFile ("Fresh.luthierpreset");
    CHECK (fresh.replaceWithText (juce::JSON::toString (current)));
    CHECK (presets.loadPreset (fresh));
    CHECK (presets.getLastMigrationBackup() == juce::File());

    dir.deleteRecursively();
}

//==============================================================================
/*  ambiguity-resolutions 8 / routing-io 2: Aux 1's pre / post-circuit toggle -
    "circuit interaction from section 1 (feedback attenuated by guitar volume)
    is testable via" it. Post-circuit, the volume knob is in the DI; pre, not. */
LUTHIER_TEST (ModelGapsUi, auxOneTapsBeforeOrAfterTheCircuit)
{
    auto diLevel = [&ctx] (bool preCircuit, float volume)
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, kBlock);
        setPlain (processor, ParamIDs::guitarVolume, volume);
        setPlain (processor, ParamIDs::aux1PreCircuit, preCircuit ? 1.0f : 0.0f);
        processor.getParameterBridge().applyAllNow();

        auto& engine = processor.getEngine();
        engine.getTapBuffers().setAuxWanted ((int) AuxBus::di, true);
        CHECK (engine.isAuxDiPreCircuit() == preCircuit);
        engine.reset();

        NoteOnEvent e;
        e.stringIndex = 1;
        e.velocity = 0.9;
        e.pitchHz = 246.94;
        engine.triggerNoteNow (e);

        double sum = 0.0;
        juce::AudioBuffer<float> block (2, kBlock);

        for (int b = 0; b < 40; ++b)
        {
            block.clear();
            juce::MidiBuffer none;
            engine.processBlock (block, none);

            if (const float* di = engine.getTapBuffers().auxRead ((int) AuxBus::di, 0))
                for (int i = 0; i < kBlock; ++i)
                    sum += (double) di[i] * di[i];
        }

        return std::sqrt (sum);
    };

    const double postFull = diLevel (false, 1.0f), postHalf = diLevel (false, 0.5f);
    const double preFull = diLevel (true, 1.0f), preHalf = diLevel (true, 0.5f);

    CHECK_MSG (postHalf < postFull * 0.9, "post-circuit: the volume knob did not reach Aux 1");
    CHECK_MSG (std::abs (preHalf - preFull) <= preFull * 0.01,
               "pre-circuit: the volume knob reached Aux 1 (" + juce::String (preHalf / preFull, 3) + ")");

    // The toggle is a saved parameter, off (post-circuit) by default.
    LuthierAudioProcessor processor;
    auto* p = processor.getState().getParameter (ParamIDs::aux1PreCircuit);
    CHECK (p != nullptr && p->getDefaultValue() == 0.0f);
    setPlain (processor, ParamIDs::aux1PreCircuit, 1.0f);
    processor.getParameterBridge().applyAllNow();
    CHECK (processor.getEngine().isAuxDiPreCircuit());

    // On the ROUTING tab, attached to that parameter.
    RoutingPanel panel (processor);
    CHECK (panel.getAux1PreCircuitToggle() != nullptr);
    CHECK (panel.getAux1PreCircuitToggle()->getLearnParameterId() == ParamIDs::aux1PreCircuit);
}

//==============================================================================
/*  ambiguity-resolutions 8: "Feedback / freeze / E-Bow parameters are legal
    modulation destinations per modulation-matrix.md 2." A route to each moves
    the engine. */
LUTHIER_TEST (ModelGapsUi, theSustainControlsAreModulationDestinations)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    auto& matrix = processor.getModMatrix();

    auto indexOf = [&processor] (const juce::String& id)
    {
        const auto& parameters = processor.getParameters();

        for (int i = 0; i < parameters.size(); ++i)
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]))
                if (withId->paramID == id)
                    return i;

        return -1;
    };

    const char* ids[] = { ParamIDs::feedbackAmount, ParamIDs::feedbackDistance, ParamIDs::feedbackAngle,
                          ParamIDs::feedbackFocus, ParamIDs::feedbackOctaveBias,
                          ParamIDs::freezeEnable, ParamIDs::freezeCaptureMs, ParamIDs::freezeLevel,
                          ParamIDs::freezeAttackMs, ParamIDs::freezeReleaseMs, ParamIDs::freezeLpCutoff,
                          ParamIDs::freezeHpCutoff,
                          ParamIDs::ebowEnable, ParamIDs::ebowIntensity, ParamIDs::ebowHarmonic, ParamIDs::ebowStringMask };

    // Every one takes a route (modulation-matrix 2: "every automatable parameter").
    for (const char* id : ids)
    {
        ModRoute route;
        route.sourceId = "macro1";
        route.destinationId = id;
        route.depth = 0.5f;
        route.enabled = true;
        CHECK_MSG (matrix.addRoute (route), juce::String ("could not route to ") + id);
    }

    const double amountBefore = processor.getEngine().getFeedbackLoop().getSettings().amount;
    const double intensityBefore = processor.getEngine().getEBow().getSettings().intensity;

    matrix.setMacroValue (0, 1.0);
    ModBlockContext context;

    for (int b = 0; b < 64; ++b)
        matrix.processBlock (kBlock, context);

    processor.getParameterBridge().applyToEngine();

    // Each destination carries an offset...
    for (const char* id : ids)
    {
        const int index = indexOf (id);
        CHECK (index >= 0 && matrix.isDestinationModulated (index));
        CHECK_MSG (matrix.getOffsetFor (index) != 0.0f, juce::String (id) + " has no modulation offset");

        // Half the range at depth 0.5 and the macro full (modulation-matrix 3:
        // the sum is in parameter units) - not held to a few units.
        const auto range = processor.getState().getParameterRange (id);
        const bool discrete = dynamic_cast<juce::AudioParameterChoice*> (processor.getParameters()[index]) != nullptr
                           || dynamic_cast<juce::AudioParameterBool*> (processor.getParameters()[index]) != nullptr;

        if (! discrete)
            CHECK_MSG (std::abs (matrix.getOffsetFor (index) - 0.5f * (range.end - range.start)) <= 0.01f * (range.end - range.start),
                       juce::String (id) + ": offset " + juce::String (matrix.getOffsetFor (index)) + " of a "
                         + juce::String (range.end - range.start) + " range");
    }

    // ... and it reaches the engine.
    CHECK_MSG (processor.getEngine().getFeedbackLoop().getSettings().amount > amountBefore,
               "feedback_amount's route did not reach the feedback loop");
    CHECK_MSG (processor.getEngine().getEBow().getSettings().intensity > intensityBefore,
               "ebow_intensity's route did not reach the E-Bow: " + juce::String (intensityBefore) + " -> "
                 + juce::String (processor.getEngine().getEBow().getSettings().intensity) + ", offset "
                 + juce::String (matrix.getOffsetFor (indexOf (ParamIDs::ebowIntensity))));
}

//==============================================================================
/*  notation-export 6.1 / TODO 9: the engine reports techniques and the
    detector's chords to the capture itself, so the NOTATION tab's chord
    history fills in use. */
LUTHIER_TEST (ModelGapsUi, theCaptureHearsTechniquesAndChordsFromTheEngine)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    CHECK (processor.getEngine().getPerformanceCapture() == &processor.getPerformanceCapture());

    // A chord held in Poly mode: the chord track gets its name.
    juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);

    for (int b = 0; b < 40; ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (b == 0)
            for (int n : { 45, 52, 57, 60, 64 })
                midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 90), 4);

        processor.processBlock (buffer, midi);
    }

    processor.drainPerformanceCapture();
    const auto& capture = processor.getPerformanceCapture();

    juce::StringArray chordNames;

    for (const auto& c : capture.getChords())
        chordNames.add (c.name);

    CHECK_MSG (chordNames.contains ("Am"), "chords captured: " + chordNames.joinIntoString (", "));
    CHECK (capture.getNotes().size() >= 5);

    // A technique the engine played reaches the note: a palm mute from the engine's own trigger.
    const auto countBefore = capture.getNotes().size();
    NoteOnEvent e;
    e.stringIndex = 0;
    e.midiNote = 64;
    e.velocity = 0.8;
    e.technique = Technique::PalmMute;
    e.pitchHz = 329.63;

    CaptureClock clock;
    clock.sampleRate = kSr;
    processor.getPerformanceCapture().beginBlock (clock);
    processor.getEngine().triggerNoteNow (e);
    processor.drainPerformanceCapture();

    CHECK (capture.getNotes().size() == countBefore + 1);

    if (capture.getNotes().size() == countBefore + 1)
        CHECK (capture.getNotes().back().technique == Technique::PalmMute);
}

//==============================================================================
/*  midi-export 2.1 / 6 (TODO 10): CHARACTER - "seed changes, environment
    changes (temperature, humidity) as they occur" - on the EVENTS source. */
LUTHIER_TEST (ModelGapsUi, characterSeedAndEnvironmentGoOutAsTheyChange)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto run = [&processor] (int blocks)
    {
        juce::Array<LuthierEvent> found;
        juce::AudioBuffer<float> audio (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);

        for (int b = 0; b < blocks; ++b)
        {
            audio.clear();
            juce::MidiBuffer midi;
            processor.processBlock (audio, midi);

            for (const auto metadata : midi)
            {
                const auto message = metadata.getMessage();
                LuthierEvent event;
                juce::int64 correction = 0;
                juce::String error;

                if (message.isSysEx()
                    && LuthierEvents::decodeSysEx (message.getSysExData(), message.getSysExDataSize(), event, correction, error)
                    && event.eventClass == LuthierEventClass::character)
                    found.add (event);
            }
        }

        return found;
    };

    auto cfg = processor.getRouting().getMidiOutConfig();
    cfg.enabled = true;
    cfg.luthierEvents = false;
    processor.getRouting().setMidiOutConfig (cfg);
    CHECK (run (4).isEmpty());

    // On: the seed and the environment are stated once...
    cfg.luthierEvents = true;
    processor.getRouting().setMidiOutConfig (cfg);
    const auto stated = run (4);
    CHECK (stated.size() == 2);

    auto& character = processor.getEngine().getCharacterEngine();

    if (stated.size() == 2)
    {
        CHECK (stated[0].get ("what") == "seed");
        CHECK (stated[0].get ("seed") == juce::String ((juce::uint64) character.getSeed()));
        CHECK (stated[1].get ("what") == "environment");
        // Glue: the environment is env_temperature_c now, not the legacy enum.
        CHECK_NEAR (stated[1].getReal ("temp"), processor.getEngine().getEnvironment().getInputs().temperatureC, 1.0e-6);
    }

    // ... then only when they change.
    CHECK (run (4).isEmpty());

    character.setSeed (4815162342ull);
    const auto seed = run (2);
    CHECK (seed.size() == 1 && seed[0].get ("seed") == "4815162342");

    if (auto* temperature = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::envTemperatureC)))
        temperature->setValueNotifyingHost (temperature->convertTo0to1 (32.0f));

    const auto env = run (2);
    CHECK (env.size() == 1 && std::abs (env[0].getReal ("temp") - 32.0) < 1.0e-6);
}

//==============================================================================
/*  notation-export 3 (TODO 9): "The plugin can also render the current bar to
    the on-plugin fretboard as tablature dots." */
LUTHIER_TEST (ModelGapsUi, theCurrentBarIsDrawnOnTheFretboardAsTabDots)
{
    LuthierAudioProcessor processor;
    playPhrase (processor, { 52, 55, 57 });

    FretboardComponent fretboard (processor);
    fretboard.setSize (600, 120);

    fretboard.refreshTabDots();
    CHECK (fretboard.getTabDots().empty());   // off by default

    // The NOTATION tab's switch turns it on.
    NotationPanel panel (processor);
    auto& button = panel.getFretboardDotsButton();
    button.setToggleState (true, juce::dontSendNotification);
    button.onClick();
    CHECK (processor.isShowingTabDotsOnFretboard());

    fretboard.refreshTabDots();
    const auto& dots = fretboard.getTabDots();
    const auto& notes = processor.getPerformanceCapture().getNotes();

    CHECK (! dots.empty());
    CHECK (dots.size() <= notes.size());

    if (! dots.empty() && ! notes.empty())
    {
        // The newest dot is the newest note, where the engine played it.
        CHECK (dots.back().stringIndex == notes.back().stringIndex);
        CHECK_NEAR (dots.back().fret, notes.back().fret, 1.0e-9);
        CHECK (dots.back().age <= dots.front().age);
    }

    // It draws.
    juce::Image image (juce::Image::ARGB, 600, 120, true, juce::SoftwareImageType());
    juce::Graphics g (image);
    fretboard.paintEntireComponent (g, true);

    button.setToggleState (false, juce::dontSendNotification);
    button.onClick();
    fretboard.refreshTabDots();
    CHECK (fretboard.getTabDots().empty());
}
