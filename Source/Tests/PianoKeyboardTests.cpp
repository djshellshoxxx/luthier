/*  The piano keyboard (PianoKeyboard.h): a click plays the pitch on the guitar,
    the strings light their keys, the range follows the guitar, and the
    Advanced strip offers it as KEYS beside FRETS and ROLL. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/PianoKeyboard.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/UiPreferences.h"
#include "../Accessibility/Accessibility.h"
#include "../Model/Playing/TuningEngine.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    juce::MouseEvent eventAt (juce::Component& target, juce::Point<float> p, juce::ModifierKeys mods)
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();

        return juce::MouseEvent (source, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, juce::Time::getCurrentTime(),
                                 p, juce::Time::getCurrentTime(), 1, false);
    }

    void runBlocks (LuthierAudioProcessor& processor, int count, juce::MidiBuffer* first = nullptr)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);

        for (int i = 0; i < count; ++i)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (i == 0 && first != nullptr)
                midi = *first;

            processor.processBlock (buffer, midi);
        }
    }

    /** The string sounding `note`, or -1. */
    int stringSounding (LuthierAudioProcessor& processor, int note)
    {
        auto& engine = processor.getEngine();

        for (int s = 0; s < engine.getNumStrings(); ++s)
            if (engine.getStringMidiNote (s) == note)
                return s;

        return -1;
    }

    /** The middle of a key, a little below halfway down it. */
    juce::Point<float> centreOfKey (juce::MidiKeyboardComponent& keys, int note)
    {
        const float x = keys.getKeyStartPosition (note) + keys.getKeyWidth() * 0.5f;
        return { x, (float) keys.getHeight() * 0.85f };   // low on a white key: clear of the black ones
    }

    void quietHumanise (LuthierAudioProcessor& processor)
    {
        if (auto* humanise = processor.getState().getParameter (ParamIDs::macroHumanize))
            humanise->setValueNotifyingHost (0.0f);
    }
}

//==============================================================================
/*  A click on a key is a real note-on for that pitch: after a few blocks some
    string is sounding it, the key shows as held, and letting go of the mouse
    sends the note-off. */
LUTHIER_TEST (PianoKeyboard, clickingAKeyPlaysThatPitchOnTheGuitar)
{
    LuthierAudioProcessor processor;
    quietHumanise (processor);
    processor.prepareToPlay (kSr, kBlock);
    runBlocks (processor, 2);

    PianoKeyboardComponent piano (processor);
    piano.setVisible (true);
    piano.setSize (900, 90);

    auto& keys = piano.getKeys();
    constexpr int note = 57;   // A3: second string, open, or up the neck elsewhere
    CHECK (note >= piano.getLowestKey() && note <= piano.getHighestKey());

    const auto at = centreOfKey (keys, note);

    keys.mouseDown (eventAt (keys, at, juce::ModifierKeys::leftButtonModifier));
    CHECK (piano.isKeyHeld (note));

    runBlocks (processor, 8);   // the Poly chord window can hold a note a block or two
    const int s = stringSounding (processor, note);
    CHECK_MSG (s >= 0, "no string sounds MIDI " + juce::String (note) + " after clicking its key");

    // Held is drawn apart from sounding, but the key is both now.
    piano.refresh();
    CHECK (piano.getSoundingString (note) == s);
    CHECK (piano.getKeyLight (note) > 0.0f);

    keys.mouseUp (eventAt (keys, at, juce::ModifierKeys()));
    CHECK (! piano.isKeyHeld (note));

    runBlocks (processor, 4);
    CHECK_MSG (stringSounding (processor, note) < 0, "the note is still held after the mouse came up");
}

//==============================================================================
/*  A drag from one key to the next is a glissando: the first is released as
    the second starts. And the keyboard alone plays it (Right, then Space). */
LUTHIER_TEST (PianoKeyboard, dragIsAGlissandoAndTheKeyboardPlaysIt)
{
    LuthierAudioProcessor processor;
    quietHumanise (processor);
    processor.prepareToPlay (kSr, kBlock);
    runBlocks (processor, 2);

    PianoKeyboardComponent piano (processor);
    piano.setVisible (true);
    piano.setSize (900, 90);
    auto& keys = piano.getKeys();

    keys.mouseDown (eventAt (keys, centreOfKey (keys, 55), juce::ModifierKeys::leftButtonModifier));
    CHECK (piano.isKeyHeld (55));
    keys.mouseDrag (eventAt (keys, centreOfKey (keys, 57), juce::ModifierKeys::leftButtonModifier));
    CHECK (piano.isKeyHeld (57));
    CHECK_MSG (! piano.isKeyHeld (55), "the key dragged off is still held");
    keys.mouseUp (eventAt (keys, centreOfKey (keys, 57), juce::ModifierKeys()));
    CHECK (! piano.isKeyHeld (57));

    // Keyboard: the cursor follows the last key clicked; Right steps it, Space holds it.
    const int first = 57;
    CHECK (keys.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK (keys.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK_MSG (piano.isKeyHeld (first + 1), "Space did not hold the key under the cursor");
    keys.keyStateChanged (false);
    CHECK (! piano.isKeyHeld (first + 1));
}

//==============================================================================
/*  Notes from anywhere light their keys: here the host's MIDI, which never
    shows as held - held is the user's hand on this keyboard only. */
LUTHIER_TEST (PianoKeyboard, hostMidiLightsTheMatchingKey)
{
    LuthierAudioProcessor processor;
    quietHumanise (processor);
    processor.prepareToPlay (kSr, kBlock);
    runBlocks (processor, 2);

    PianoKeyboardComponent piano (processor);
    piano.setVisible (true);
    piano.setSize (900, 90);

    constexpr int note = 52;   // E3
    piano.refresh();
    CHECK (piano.getSoundingString (note) < 0);
    CHECK (piano.getSoundingDescription() == "Nothing sounding");

    juce::MidiBuffer host;
    host.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);
    runBlocks (processor, 8, &host);

    const int s = stringSounding (processor, note);
    CHECK_MSG (s >= 0, "the host's note did not reach a string");

    piano.refresh();
    CHECK_MSG (piano.getSoundingString (note) == s, "key " + juce::String (note) + " lit by string "
                                                     + juce::String (piano.getSoundingString (note)));
    CHECK (piano.getKeyLight (note) > 0.0f);
    CHECK (! piano.isKeyHeld (note));
    CHECK_MSG (piano.getSoundingDescription().contains ("E3 on string " + juce::String (s + 1)),
               "description: " + piano.getSoundingDescription());

    // Each string has its own colour.
    CHECK (PianoKeyboardComponent::stringColour (0, 6) != PianoKeyboardComponent::stringColour (1, 6));

    // Reduced motion: lit is fully lit, no partial fade.
    auto& settings = AccessibilitySettings::get();
    const bool was = settings.isReducedMotion();
    settings.setReducedMotion (true);
    piano.refresh();
    CHECK_NEAR (piano.getKeyLight (note), 1.0, 1.0e-6);
    settings.setReducedMotion (was);
}

//==============================================================================
/*  The keys cover the guitar: a standard six-string from its low E to the top
    fret of the high E, a four-string bass from E1 to the top of its G string. */
LUTHIER_TEST (PianoKeyboard, theRangeCoversTheGuitar)
{
    {
        TuningEngine guitar;
        guitar.prepare (kSr);
        guitar.setTuningPreset (TuningPreset::Standard);

        for (int s = 0; s < 6; ++s)
            guitar.setMaxFrets (s, 22);

        const auto range = PianoKeyboardComponent::computeGuitarRange (guitar, 6);
        CHECK_MSG (range.lowest == 40, "six-string lowest " + juce::String (range.lowest));
        CHECK_MSG (range.highest == 64 + 22, "six-string highest " + juce::String (range.highest));
    }

    {
        TuningEngine bass;
        bass.prepare (kSr);
        bass.setTuningPreset (TuningPreset::BassStandard);

        for (int s = 0; s < 4; ++s)
            bass.setMaxFrets (s, 20);

        const auto range = PianoKeyboardComponent::computeGuitarRange (bass, 4);
        CHECK_MSG (range.lowest == 28, "bass lowest " + juce::String (range.lowest));
        CHECK_MSG (range.highest == 43 + 20, "bass highest " + juce::String (range.highest));
    }

    // The component follows the processor's guitar, padded to white keys.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    PianoKeyboardComponent piano (processor);
    piano.setSize (900, 90);

    const auto range = piano.getGuitarRange();
    CHECK (piano.getKeys().getRangeStart() <= range.lowest);
    CHECK (piano.getKeys().getRangeEnd() >= range.highest);
    CHECK (! juce::MidiMessage::isMidiNoteBlack (piano.getKeys().getRangeStart()));
    CHECK (! juce::MidiMessage::isMidiNoteBlack (piano.getKeys().getRangeEnd()));
    CHECK_MSG (range.lowest == 40, "the default guitar starts at " + juce::String (range.lowest));

    // Zoomed in, the keys are wider than the width allows, and it scrolls.
    const float fitted = piano.getKeys().getKeyWidth();
    piano.setZoom (2.0f);
    CHECK (piano.getKeys().getKeyWidth() > fitted * 1.9f);
}

//==============================================================================
/*  QWERTY playing is off by default (A, S, D, T, L and P are shortcuts), and
    the toggle turns it on and is remembered. */
LUTHIER_TEST (PianoKeyboard, qwertyIsOptIn)
{
    UiPreferences::get().setBool ("pianoKeyboard.qwerty", false);

    LuthierAudioProcessor processor;

    {
        PianoKeyboardComponent piano (processor);
        piano.setSize (900, 90);

        CHECK (! piano.isQwertyPlayingEnabled());
        CHECK (! piano.getKeys().keyPressed (juce::KeyPress ('p', 0, 0)));   // left for Panic

        piano.getQwertyButton().setToggleState (true, juce::sendNotificationSync);
        CHECK (piano.isQwertyPlayingEnabled());
        CHECK (piano.getKeys().keyPressed (juce::KeyPress ('a', 0, 0)));
    }

    PianoKeyboardComponent again (processor);
    CHECK (again.isQwertyPlayingEnabled());
    again.setQwertyPlayingEnabled (false);
}

//==============================================================================
/*  The Advanced strip: FRETS | ROLL | KEYS, one view at a time, remembered. */
LUTHIER_TEST (PianoKeyboard, theStripCyclesFretsRollKeysAndRemembers)
{
    UiPreferences::get().reset();

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    using View = AdvancedPanel::StripView;

    {
        AdvancedPanel panel (processor);
        panel.setVisible (true);
        panel.setSize (1600, 900);

        CHECK (panel.getStripView() == View::frets);
        CHECK (panel.getFretboard().isVisible());
        CHECK (! panel.getPianoKeyboard().isVisible());

        panel.getStripButton (View::roll).onClick();   // what a click does
        CHECK (panel.getStripView() == View::roll);
        CHECK (panel.getStringRoll().isVisible());
        CHECK (! panel.getPianoKeyboard().isVisible());

        panel.setStripView (View::keys);
        CHECK (panel.getStripView() == View::keys);
        CHECK (! panel.getFretboard().isVisible());
        CHECK (! panel.getStringRoll().isVisible());
        CHECK (panel.getPianoKeyboard().isVisible());
        CHECK (panel.getPianoKeyboard().getWidth() > 200);
        CHECK (panel.getPianoKeyboard().getHeight() > 60);
        CHECK (panel.getStripButton (View::keys).getToggleState());
        CHECK (! panel.getStripButton (View::frets).getToggleState());
    }

    // Remembered.
    {
        AdvancedPanel again (processor);
        again.setVisible (true);
        again.setSize (1600, 900);
        CHECK (again.getStripView() == View::keys);
        CHECK (again.getPianoKeyboard().isVisible());

        again.setStripView (View::frets);
        CHECK (again.getFretboard().isVisible());
        CHECK (! again.getPianoKeyboard().isVisible());
    }

    // An older session's "roll" bool still opens the roll.
    UiPreferences::get().reset();
    UiPreferences::get().setBool ("advanced.stripShowsRoll", true);
    {
        AdvancedPanel old (processor);
        old.setVisible (true);
        old.setSize (1600, 900);
        CHECK (old.getStripView() == View::roll);
    }

    UiPreferences::get().reset();
}

//==============================================================================
/*  Easy mode: a "Keys" toggle opens the keyboard along the bottom of the
    guitar area, and the guitar gives up the room. */
LUTHIER_TEST (PianoKeyboard, easyModeKeysToggleOpensItUnderTheGuitar)
{
    UiPreferences::get().setBool ("easy.showKeys", false);

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    EasyPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1200, 660);

    auto& drawer = panel.getKeysDrawer();
    CHECK (! drawer.isOpen());
    CHECK (! drawer.getKeyboard().isVisible());
    CHECK (drawer.getToggle().isVisible() && drawer.getToggle().getWidth() > 0);
    const int guitarHeight = panel.getGuitar().getHeight();

    drawer.getToggle().setToggleState (true, juce::sendNotificationSync);
    CHECK (drawer.isOpen());
    CHECK (drawer.getKeyboard().isVisible());
    CHECK (drawer.getKeyboard().getHeight() >= 40);
    CHECK (panel.getGuitar().getHeight() < guitarHeight);
    CHECK_MSG (drawer.getKeyboard().getY() >= panel.getGuitar().getBottom(), "the keyboard overlaps the guitar");
    CHECK_MSG (drawer.getKeyboard().getBottom() <= panel.getPlayingArea().getY(), "the keyboard overlaps the playing strip");

    drawer.setOpen (false);
    CHECK (panel.getGuitar().getHeight() == guitarHeight);
}
