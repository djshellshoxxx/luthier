/*  RESET & STOP and Panic (live-performance.md 9; the user's "a loop that would
    not stop even after hitting reset and panic a bunch of times").

    Panic ends what is sounding. Reset & Stop also ends what would start it
    again - the tune player looping, the looper, the rhythm engine free-running,
    the kill switch - and puts every setting back. Both are carried out on the
    audio thread's own block, so these drive the processor the way a host does
    and look at what comes out. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/HeaderBar.h"
#include "../UI/HelpContent.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"

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

    /** Two bars of strummed Am and F with a melody, looping by default. */
    Tune loopingTune()
    {
        Tune t;
        t.meta.title = "Runaway";
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

    /** Down-strums on the quarter notes of a sixteenth-note grid. */
    RhythmPattern quarterNoteDowns()
    {
        RhythmPattern p;
        p.setName ("Test Quarters");
        p.setKind (RhythmPattern::Kind::strum);
        p.setSubdivision (Subdivision::sixteenth);
        p.setLength (16);
        p.setSwing (0.5);

        for (int i = 0; i < 16; ++i)
        {
            StrumStep step;
            step.type = (i % 4 == 0) ? StrumType::down : StrumType::rest;
            step.dynamic = 1.0;
            step.stringMask = 0x0FFF;
            p.setStrumStep (i, step);
        }

        return p;
    }

    /** Renders `blocks` blocks with no host play head, servicing the tune the
        way the timer does, and returns the peak of the main output. `midi`
        goes into the first block only. */
    double run (LuthierAudioProcessor& processor, int blocks, juce::MidiBuffer midi = {})
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), kBlock);
        double peak = 0.0;

        for (int b = 0; b < blocks; ++b)
        {
            if (b % 6 == 0)
                processor.serviceTune();

            buffer.clear();
            processor.processBlock (buffer, midi);
            midi.clear();

            for (int ch = 0; ch < 2; ++ch)
                peak = juce::jmax (peak, (double) buffer.getMagnitude (ch, 0, kBlock));
        }

        return peak;
    }

    /*  "Silent" here means back at the plugin's idle noise floor: amp hiss and
        mains hum are deliberate (Engine.silenceInSilenceOut), so a stopped
        plugin is not all zeros. The floor is measured on the processor with
        nothing played, after a short warm-up (the engine's filters settling
        from zero), and what follows a stop may not rise above a small multiple
        of it once it has had the same warm-up. */
    double idleFloor (LuthierAudioProcessor& processor)
    {
        run (processor, (int) (0.2 * kSr / kBlock));
        return run (processor, (int) (0.5 * kSr / kBlock));
    }

    double silenceCeiling (double floor)
    {
        return juce::jmax (3.0 * floor, 1.0e-4);
    }

    /** A processor with a tune looping, a loop playing in the looper, the rhythm
        engine free-running on a held chord, the kill switch on, and a couple of
        parameters moved off their defaults: everything Reset & Stop has to end. */
    struct Runaway
    {
        Runaway()
        {
            processor.prepareToPlay (kSr, kBlock);

            if (auto* humanise = processor.getState().getParameter (ParamIDs::macroHumanize))
                humanise->setValueNotifyingHost (0.0f);

            // The tune, looping.
            processor.getTuneSession().newTune (loopingTune());
            processor.getTunePlayer().setLoop (true);
            processor.getTunePlayer().play();

            // The rhythm engine, free-running on a chord that stays held.
            auto& rhythm = processor.getEngine().getRhythmEngine();
            rhythm.setPattern (quarterNoteDowns());
            rhythm.setEnabled (true);
            rhythm.setFreeRun (true);

            juce::MidiBuffer chordMidi;

            for (int note : { 40, 47, 52, 56, 59, 64 })
                chordMidi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

            // The looper, recording a second of it and then playing it back.
            processor.setPracticePanelOpen (true);
            processor.getLooper().press();
            run (processor, (int) (1.2 * kSr / kBlock), chordMidi);
            processor.getLooper().press();
            run (processor, 4);

            processor.getKillSwitch().setActive (true);
            processor.getMetronome().setEnabled (true);

            for (const char* id : { ParamIDs::masterGain, ParamIDs::strumSpeed })
                if (auto* p = processor.getState().getParameter (id))
                    p->setValueNotifyingHost (p->getDefaultValue() < 0.5f ? 0.9f : 0.1f);

            run (processor, 4);
        }

        LuthierAudioProcessor processor;
    };

    int parametersOffDefault (LuthierAudioProcessor& processor, juce::String& firstOffender)
    {
        int off = 0;

        for (auto* p : processor.getParameters())
        {
            if (std::abs (p->getValue() - p->getDefaultValue()) > 1.0e-5f)
            {
                if (off == 0)
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                        firstOffender = ranged->paramID;

                ++off;
            }
        }

        return off;
    }

    template <typename T>
    T* findOne (juce::Component& root)
    {
        if (auto* found = dynamic_cast<T*> (&root))
            return found;

        for (auto* child : root.getChildren())
            if (auto* found = findOne<T> (*child))
                return found;

        return nullptr;
    }

    juce::TextButton* findButtonTitled (juce::Component& root, const juce::String& title)
    {
        if (auto* button = dynamic_cast<juce::TextButton*> (&root))
            if (button->getTitle() == title || button->getButtonText() == title)
                return button;

        for (auto* child : root.getChildren())
            if (auto* found = findButtonTitled (*child, title))
                return found;

        return nullptr;
    }
}

//==============================================================================
/*  The fixture really is a runaway before the button is pressed: otherwise a
    silent result would prove nothing. */
LUTHIER_TEST (ResetStop, theRunawayFixtureMakesSoundOnItsOwn)
{
    Runaway r;

    r.processor.getKillSwitch().setActive (false);
    const double peak = run (r.processor, (int) (0.5 * kSr / kBlock));

    CHECK_MSG (peak > 0.02, "the runaway fixture is silent, peak " + juce::String (peak, 6));
    CHECK (r.processor.getTunePlayer().isPlaying());
    CHECK (r.processor.getLooper().getState() == Looper::State::playing);
    CHECK (r.processor.getEngine().getRhythmEngine().isFreeRunning());
    CHECK (r.processor.getEngine().getRhythmEngine().isDriving());
}

/*  Reset & Stop: silence, and every setting back where it started. */
LUTHIER_TEST (ResetStop, resetAndStopSilencesEverythingAndRestoresEveryDefault)
{
    Runaway r;
    auto& processor = r.processor;

    juce::String offender;
    CHECK_MSG (parametersOffDefault (processor, offender) > 0, "the fixture moved no parameter");

    // What an untouched plugin at the defaults sounds like when nothing plays.
    double floor = 0.0;

    {
        LuthierAudioProcessor fresh;
        fresh.prepareToPlay (kSr, kBlock);
        floor = idleFloor (fresh);
    }

    processor.resetAndStop();

    // A short fade for whatever was mid-tail, then only the idle floor.
    run (processor, (int) (0.1 * kSr / kBlock));
    const double peak = run (processor, (int) (0.5 * kSr / kBlock));

    CHECK_MSG (peak < silenceCeiling (floor),
               "still sounding after Reset & Stop, peak " + juce::String (peak, 6)
                 + " against an idle floor of " + juce::String (floor, 6));

    CHECK_MSG (! processor.getTunePlayer().isPlaying(), "the tune is still playing");
    CHECK_MSG (processor.getLooper().getState() == Looper::State::stopped, "the looper is still going");
    CHECK_MSG (! processor.getBackingTrack().isPlaying(), "the backing track is still playing");
    CHECK_MSG (! processor.getMetronome().isEnabled(), "the metronome is still on");
    CHECK_MSG (! processor.getSessionRecorder().isEnabled(), "the session recorder is still on");

    auto& rhythm = processor.getEngine().getRhythmEngine();
    CHECK_MSG (! rhythm.isEnabled(), "the rhythm engine is still enabled");
    CHECK_MSG (! rhythm.isFreeRunning(), "the rhythm engine is still free-running");
    CHECK_MSG (! rhythm.isDriving(), "the rhythm engine is still driving");

    CHECK_MSG (! processor.getKillSwitch().isActive(), "the kill switch is still on");
    CHECK (! processor.isAuditioning());

    const int off = parametersOffDefault (processor, offender);
    CHECK_MSG (off == 0, juce::String (off) + " parameters are off their default, first " + offender);

    for (int s = 0; s < processor.getEngine().getNumStrings(); ++s)
        CHECK_MSG (processor.getEngine().getStringMidiNote (s) < 0,
                   "string " + juce::String (s) + " still holds a note");

    CHECK (processor.getEngine().getMidiInterpreter().getActiveNoteCount() == 0);
    CHECK (processor.getPresetManager().getCurrentPresetName() == "Init");
}

/*  Reset alone used to leave the tune looping; it is the same call now. */
LUTHIER_TEST (ResetStop, resetEverythingStopsTheTuneAndTheLoopsToo)
{
    Runaway r;

    double floor = 0.0;

    {
        LuthierAudioProcessor fresh;
        fresh.prepareToPlay (kSr, kBlock);
        floor = idleFloor (fresh);
    }

    r.processor.resetEverything();
    run (r.processor, (int) (0.1 * kSr / kBlock));
    const double peak = run (r.processor, (int) (0.3 * kSr / kBlock));

    CHECK_MSG (peak < silenceCeiling (floor),
               "still sounding after Reset, peak " + juce::String (peak, 6)
                 + " against an idle floor of " + juce::String (floor, 6));
    CHECK (! r.processor.getTunePlayer().isPlaying());
    CHECK (r.processor.getLooper().getState() == Looper::State::stopped);
    CHECK (! r.processor.getEngine().getRhythmEngine().isFreeRunning());
}

/*  Reset & Stop is one undo step, and undo brings the settings back (not the
    transport: undo is for the parameters). */
LUTHIER_TEST (ResetStop, resetAndStopIsOneUndoStep)
{
    Runaway r;
    auto& processor = r.processor;

    auto* gain = processor.getState().getParameter (ParamIDs::masterGain);
    CHECK (gain != nullptr);

    if (gain == nullptr)
        return;

    const float moved = gain->getValue();
    processor.resetAndStop();
    CHECK_NEAR (gain->getValue(), gain->getDefaultValue(), 1.0e-5);

    processor.undo();
    CHECK_NEAR (gain->getValue(), moved, 1.0e-5);
}

//==============================================================================
/*  live-performance 9: Panic silences a held note and a running rhythm strum
    within one block, and leaves every setting alone. */
LUTHIER_TEST (ResetStop, panicSilencesAHeldNoteAndARunningStrumWithinOneBlock)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    if (auto* humanise = processor.getState().getParameter (ParamIDs::macroHumanize))
        humanise->setValueNotifyingHost (0.0f);

    auto& rhythm = processor.getEngine().getRhythmEngine();
    rhythm.setPattern (quarterNoteDowns());
    rhythm.setEnabled (true);
    rhythm.setFreeRun (true);

    if (auto* gain = processor.getState().getParameter (ParamIDs::masterGain))
        gain->setValueNotifyingHost (0.9f);

    // The floor at these settings, which Panic leaves alone.
    const double floor = idleFloor (processor);

    juce::MidiBuffer chordMidi;

    for (int note : { 40, 47, 52, 56, 59, 64 })
        chordMidi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

    // Strumming, and audible.
    const double before = run (processor, (int) (0.6 * kSr / kBlock), chordMidi);
    CHECK_MSG (before > silenceCeiling (floor),
               "the rhythm engine never rose above the floor, peak " + juce::String (before, 6));
    CHECK (rhythm.isDriving());

    int held = 0;

    for (int s = 0; s < processor.getEngine().getNumStrings(); ++s)
        if (processor.getEngine().getStringMidiNote (s) >= 0)
            ++held;

    CHECK_MSG (held > 0, "no string holds a note before the panic");

    processor.panic();

    // The very next block is down at the floor: strings choked and reset,
    // every tail cleared.
    const double after = run (processor, 1);
    CHECK_MSG (after < silenceCeiling (floor),
               "the block after Panic still sounds, peak " + juce::String (after, 6)
                 + " against an idle floor of " + juce::String (floor, 6));

    for (int s = 0; s < processor.getEngine().getNumStrings(); ++s)
        CHECK_MSG (processor.getEngine().getStringMidiNote (s) < 0,
                   "string " + juce::String (s) + " still holds a note after Panic");

    CHECK (processor.getEngine().getMidiInterpreter().getActiveNoteCount() == 0);
    CHECK_MSG (! rhythm.isDriving(), "the rhythm engine is still driving after Panic");

    /*  And it stays silent: with its held chord released the free-running
        engine has nothing to strum. The first 100 ms after the block are the
        engine settling from its reset - the amp's DC blockers and the noise
        floor starting again from zero, a bump of about -35 dBFS that a fresh
        instance makes too - so they are given the same warm-up idleFloor gives
        a fresh instance before the floor is held to. */
    run (processor, (int) (0.1 * kSr / kBlock));
    const double later = run (processor, (int) (0.5 * kSr / kBlock));
    CHECK_MSG (later < silenceCeiling (floor),
               "sound came back after Panic, peak " + juce::String (later, 6)
                 + " against an idle floor of " + juce::String (floor, 6));

    // 9.5 / 9.6: settings are not Panic's business.
    CHECK (rhythm.isEnabled());
    CHECK (rhythm.isFreeRunning());

    if (auto* gain = processor.getState().getParameter (ParamIDs::masterGain))
        CHECK_NEAR (gain->getValue(), 0.9, 1.0e-5);
}

/*  Panic also stops the tune: a tune re-feeds its notes every block, so a
    panic that left it playing was undone before it finished. */
LUTHIER_TEST (ResetStop, panicStopsTheTunePlayer)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    processor.getTuneSession().newTune (loopingTune());
    processor.getTunePlayer().play();
    run (processor, 20);

    CHECK (processor.getTunePlayer().isPlaying());
    processor.panic();
    CHECK (! processor.getTunePlayer().isPlaying());
}

//==============================================================================
/*  The header: RESET & STOP sits beside Panic, in the warning colour, at every
    window width, without pushing anything off the row. */
LUTHIER_TEST (ResetStop, theHeaderHasAResetAndStopButtonBesidePanicAtEveryWidth)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        CHECK_MSG (false, "no editor");
        return;
    }

    editor->setVisible (true);

    auto* header = findOne<HeaderBar> (*editor);
    CHECK (header != nullptr);

    if (header == nullptr)
        return;

    for (int width : { LuthierAudioProcessorEditor::minimumWidth,
                       LuthierAudioProcessorEditor::defaultWidth, 1280, 1600 })
    {
        editor->setSize (width, LuthierAudioProcessorEditor::defaultHeight);

        const juce::String where ("at " + juce::String (width) + " px");

        // LUTHIER_DUMP_HEADER=<folder>: a render of the header per width, to look at.
        if (const auto folder = juce::SystemStats::getEnvironmentVariable ("LUTHIER_DUMP_HEADER", {});
            folder.isNotEmpty())
        {
            juce::Image image (juce::Image::ARGB, juce::jmax (1, header->getWidth()),
                               juce::jmax (1, header->getHeight()), true);
            juce::Graphics g (image);
            header->paintEntireComponent (g, true);

            juce::File file (folder);
            file = file.getChildFile ("header-" + juce::String (width) + ".png");
            juce::FileOutputStream out (file);

            if (out.openedOk())
                juce::PNGImageFormat().writeImageToStream (image, out);
        }

        auto* reset = findButtonTitled (*header, tr ("header.resetStop"));
        auto* panic = findButtonTitled (*header, "Panic");

        CHECK_MSG (reset != nullptr, "no RESET & STOP button " + where);
        CHECK_MSG (panic != nullptr, "no Panic button " + where);

        if (reset == nullptr || panic == nullptr)
            continue;

        CHECK_MSG (reset->isVisible() && reset->getWidth() > 0 && reset->getHeight() > 0,
                   "RESET & STOP has no size " + where);
        CHECK_MSG (reset->getRight() <= panic->getX() && reset->getY() == panic->getY(),
                   "RESET & STOP is not beside Panic " + where);
        CHECK (reset->findColour (juce::TextButton::textColourOffId) == Palette::warning);
        CHECK (reset->getTooltip().isNotEmpty());

        // Nothing in the header overlaps it, and - from gui-integration's
        // 1200 px minimum window up - every other button keeps a size, with a
        // preset name still readable in the middle.
        for (auto* child : header->getChildren())
        {
            if (child == reset || ! child->isVisible())
                continue;

            CHECK_MSG (! child->getBounds().intersects (reset->getBounds()),
                       child->getName() + " overlaps RESET & STOP " + where);

            if (auto* button = dynamic_cast<juce::TextButton*> (child);
                button != nullptr && width >= LuthierAudioProcessorEditor::defaultWidth)
                CHECK_MSG (button->getWidth() > 0,
                           "the " + button->getButtonText() + " button was squeezed out " + where);
        }

        if (width >= LuthierAudioProcessorEditor::defaultWidth)
        {
            auto* name = findButtonTitled (*header, processor.getPresetManager().getCurrentPresetName());

            CHECK_MSG (name != nullptr && name->getWidth() >= 60,
                       "the preset name has " + juce::String (name != nullptr ? name->getWidth() : 0)
                         + " px " + where);
        }
    }
}

/*  The shortcut is registered - so the HELP tab lists it - and the editor acts
    on it. */
LUTHIER_TEST (ResetStop, ctrlShiftPIsRegisteredListedAndActedOn)
{
    auto& settings = AccessibilitySettings::get();
    const auto* binding = settings.findShortcut ("resetAndStop");

    CHECK_MSG (binding != nullptr, "resetAndStop is not in the shortcut registry");

    if (binding != nullptr)
    {
        const juce::KeyPress expected ('p', juce::ModifierKeys::commandModifier
                                             | juce::ModifierKeys::shiftModifier, 0);
        CHECK (binding->defaultKey == expected);
        CHECK (tr (binding->descriptionKey) != binding->descriptionKey);
    }

    bool listed = false;

    for (const auto& row : HelpContent::getShortcutRows())
        if (row.actionId == "resetAndStop")
            listed = row.group == "Playing" && row.description.isNotEmpty();

    CHECK_MSG (listed, "the HELP tab's shortcut list does not show Reset & Stop under Playing");

    if (binding == nullptr)
        return;

    Runaway r;
    std::unique_ptr<juce::AudioProcessorEditor> editor (r.processor.createEditor());

    if (editor == nullptr)
    {
        CHECK_MSG (false, "no editor");
        return;
    }

    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    CHECK_MSG (editor->keyPressed (binding->key), "the editor ignored the Reset & Stop key");

    CHECK (! r.processor.getTunePlayer().isPlaying());
    CHECK (r.processor.getLooper().getState() == Looper::State::stopped);
    CHECK (! r.processor.getEngine().getRhythmEngine().isFreeRunning());

    juce::String offender;
    const int off = parametersOffDefault (r.processor, offender);
    CHECK_MSG (off == 0, juce::String (off) + " parameters are off their default, first " + offender);
}



