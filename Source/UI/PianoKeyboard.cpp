#include "PianoKeyboard.h"
#include "Widgets.h"
#include "UiPreferences.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"
#include "../DSP/Common/DspCommon.h"
#include "../Model/Playing/TuningEngine.h"

#include <map>

namespace luthier
{

namespace
{
    constexpr int kFastHz = 30;          ///< gui-engine-dataflow 2: the per-string strips' rate
    constexpr int kSlowHz = 10;          ///< accessibility 5: reduced motion
    constexpr int kSideColumn = 48;      ///< the QWERTY and zoom buttons
    constexpr float kHeldFloor = 0.35f;  ///< a held note stays visibly lit however quiet it gets
    constexpr float kLitThreshold = 0.02f;
    constexpr float kMaxZoom = 4.0f;
    constexpr const char* kQwertyPreference = "pianoKeyboard.qwerty";
    constexpr const char* kEasyPreference = "easy.showKeys";

    /*  The keys take their colours from the palette by role, not by value: the
        white keys are whichever of the text and background colours is lighter,
        the black keys the darker, which is right for the dark palettes (cream
        on walnut), the light one (paper and ink) and High contrast alike. */
    juce::Colour whiteKeyColour()
    {
        return Palette::textPrimary.getPerceivedBrightness() >= Palette::background.getPerceivedBrightness()
                 ? Palette::textPrimary : Palette::background;
    }

    juce::Colour blackKeyColour()
    {
        return Palette::textPrimary.getPerceivedBrightness() >= Palette::background.getPerceivedBrightness()
                 ? Palette::backgroundDeep : Palette::textPrimary;
    }

    /** Text on a fill: whichever key colour stands out from it more. */
    juce::Colour inkOn (juce::Colour fill)
    {
        const float b = fill.getPerceivedBrightness();
        const auto light = whiteKeyColour(), dark = blackKeyColour();

        return std::abs (light.getPerceivedBrightness() - b) > std::abs (dark.getPerceivedBrightness() - b)
                 ? light : dark;
    }

    bool isBlack (int note) noexcept { return juce::MidiMessage::isMidiNoteBlack (note); }

    juce::String defaultQwertyKeys() { return "awsedftgyhujkolp;"; }
}

//==============================================================================
/*  The keys themselves: JUCE's keyboard for the mouse, touch, drag and QWERTY
    handling, drawn from the palette and lit from the owner's per-note state. */
class PianoKeyboardComponent::Keys : public juce::MidiKeyboardComponent,
                                     public juce::SettableTooltipClient
{
public:
    Keys (PianoKeyboardComponent& o, juce::MidiKeyboardState& state)
        : juce::MidiKeyboardComponent (state, juce::KeyboardComponentBase::horizontalKeyboard),
          owner (o)
    {
        setOctaveForMiddleC (4);                     // middle C = C4, low E = E2
        setMidiChannel (LuthierAudioProcessor::kPreviewKeyboardChannel);
        setScrollButtonWidth (14);
        setBlackNoteLengthProportion (0.62f);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        clearKeyMappings();                          // QWERTY is opt-in (see the header)
    }

    int cursorNote = 60;

    //==========================================================================
    void drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour, juce::Colour) override
    {
        const auto base = whiteKeyColour();
        auto face = isDown ? base.interpolatedWith (Palette::accentDim, 0.35f) : base;
        g.setColour (face);
        g.fillRect (area);

        const int s = owner.getSoundingString (note);
        const float light = owner.getKeyLight (note);
        juce::Colour lit;

        if (s >= 0 && light > 0.0f)
        {
            lit = stringColour (s, owner.numStrings);
            face = face.overlaidWith (lit.withAlpha (0.25f + 0.75f * light));
            g.setColour (face);
            g.fillRect (area.reduced (1.0f, 0.0f));
        }

        if (isOver && ! isDown)
        {
            g.setColour (Palette::accent.withAlpha (0.14f));
            g.fillRect (area);
        }

        // Separators, and a closing edge after the last key.
        g.setColour (Palette::edge);
        g.fillRect (area.withWidth (1.0f));

        if (note == getRangeEnd())
            g.fillRect (area.withX (area.getRight() - 1.0f).withWidth (1.0f));

        if (isDown)
        {
            g.setColour (Palette::accent);
            g.drawRect (area.reduced (1.5f, 1.0f), 2.0f);
        }

        drawCursor (note, g, area);

        // Labels at the bottom: the string number on a lit key, the octave on C.
        auto labels = area.reduced (1.0f, 2.0f);
        const float fontHeight = juce::jlimit (7.0f, 11.0f, getKeyWidth() * 0.62f);
        g.setFont (Fonts::ui (fontHeight));

        const auto text = getWhiteNoteText (note);

        if (text.isNotEmpty() && labels.getHeight() > fontHeight * 2.0f)
        {
            g.setColour (inkOn (face).withAlpha (0.75f));
            g.drawText (text, labels.removeFromBottom (fontHeight + 1.0f), juce::Justification::centredBottom, false);
        }

        if (s >= 0 && light > 0.0f && labels.getHeight() > fontHeight * 2.0f)
        {
            g.setColour (inkOn (face));
            g.setFont (Fonts::ui (fontHeight, true));
            g.drawText (juce::String (s + 1), labels.removeFromBottom (fontHeight + 2.0f),
                        juce::Justification::centredBottom, false);
        }
    }

    void drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour) override
    {
        const auto base = blackKeyColour();
        auto face = isDown ? base.interpolatedWith (Palette::accentDim, 0.5f) : base;

        const int s = owner.getSoundingString (note);
        const float light = owner.getKeyLight (note);

        if (s >= 0 && light > 0.0f)
            face = face.overlaidWith (stringColour (s, owner.numStrings).withAlpha (0.25f + 0.75f * light));

        if (isOver && ! isDown)
            face = face.overlaidWith (Palette::accent.withAlpha (0.2f));

        g.setColour (face);
        g.fillRoundedRectangle (area.withTrimmedTop (-2.0f), 2.0f);

        // A sheen on an unpressed key, where a pressed one sinks.
        if (! isDown && Palette::textured)
        {
            g.setColour (face.brighter (0.25f).withAlpha (0.5f));
            g.fillRect (area.reduced (area.getWidth() * 0.18f, 0.0f).removeFromTop (area.getHeight() * 0.85f)
                            .withTrimmedTop (1.0f).withWidth (1.0f));
        }

        if (isDown)
        {
            g.setColour (Palette::accent);
            g.drawRect (area.reduced (1.0f), 1.5f);
        }

        drawCursor (note, g, area);

        if (s >= 0 && light > 0.0f && area.getHeight() > 18.0f)
        {
            const float fontHeight = juce::jlimit (7.0f, 10.0f, area.getWidth() * 0.8f);
            g.setColour (inkOn (face));
            g.setFont (Fonts::ui (fontHeight, true));
            g.drawText (juce::String (s + 1), area.reduced (0.0f, 2.0f), juce::Justification::centredBottom, false);
        }
    }

    juce::String getWhiteNoteText (int note) override
    {
        // Every C, and the lowest key so the range's start is named too.
        if (note % 12 == 0 || note == getRangeStart())
            return TuningEngine::noteName (note);

        return {};
    }

    void drawUpDownButton (juce::Graphics& g, int w, int h, bool isOver, bool isDown, bool movesUp) override
    {
        g.fillAll (isDown ? Palette::accentDim : (isOver ? Palette::panelRaised : Palette::panel));
        LuthierLookAndFeel::drawChevron (g, { (float) w * 0.5f, (float) h * 0.5f }, juce::jmin (4.0f, (float) w * 0.3f),
                                         movesUp ? 1 : 3, Palette::textMuted);   // right / left
    }

    //==========================================================================
    /*  Clicks are handled here rather than by JUCE's keyboard, whose hit test
        asks the window (reallyContains) and so cannot be driven without one;
        the geometry is the keyboard's own. One note per finger: a drag onto
        another key releases the last one (a glissando), and velocity grows
        towards the near end of the key, as a pianist's does. */
    int noteAt (juce::Point<float> p) const
    {
        if (! getLocalBounds().toFloat().contains (p))
            return -1;

        for (const bool black : { true, false })
            for (int n = getRangeStart(); n <= getRangeEnd(); ++n)
                if (isBlack (n) == black && getRectangleForKey (n).contains (p))
                    return n;

        return -1;
    }

    float velocityAt (int note, juce::Point<float> p) const
    {
        const auto key = getRectangleForKey (note);
        const float t = (p.y - key.getY()) / juce::jmax (1.0f, key.getHeight());
        return juce::jlimit (0.15f, 1.0f, 0.3f + 0.7f * t);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        juce::MidiKeyboardComponent::mouseMove (e);   // the hover highlight

        if (! e.mods.isPopupMenu())
            playUnderFinger (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        juce::MidiKeyboardComponent::mouseMove (e);

        if (! e.mods.isPopupMenu())
            playUnderFinger (e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        juce::MidiKeyboardComponent::mouseMove (e);
        releaseFinger (e.source.getIndex());
    }

    void playUnderFinger (const juce::MouseEvent& e)
    {
        const int finger = e.source.getIndex();
        const int note = noteAt (e.position);
        const auto found = fingerNotes.find (finger);
        const int previous = found != fingerNotes.end() ? found->second : -1;

        if (note == previous)
            return;

        releaseFinger (finger);

        if (note >= 0)
        {
            fingerNotes[finger] = note;
            cursorNote = note;

            if (! owner.isKeyHeld (note))
                owner.pressKey (note, velocityAt (note, e.position));
        }
    }

    void releaseFinger (int finger)
    {
        const auto found = fingerNotes.find (finger);

        if (found == fingerNotes.end())
            return;

        const int note = found->second;
        fingerNotes.erase (found);

        // Another finger (or the keyboard) may still hold the same key.
        for (const auto& other : fingerNotes)
            if (other.second == note)
                return;

        if (note != heldByKey)
            owner.releaseKey (note);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCommandDown())
        {
            owner.setZoom (owner.getZoom() * (wheel.deltaY > 0.0f ? 1.15f : 1.0f / 1.15f));
            return;
        }

        juce::MidiKeyboardComponent::mouseWheelMove (e, wheel);
    }

    // accessibility 2: the keyboard alone can play it. Left / Right move a
    // cursor by a semitone, Up / Down by an octave; Space or Enter holds it.
    bool keyPressed (const juce::KeyPress& key) override
    {
        const int step = key == juce::KeyPress::leftKey ? -1 : key == juce::KeyPress::rightKey ? 1
                       : key == juce::KeyPress::downKey ? -12 : key == juce::KeyPress::upKey ? 12 : 0;

        if (step != 0)
        {
            moveCursor (step);
            return true;
        }

        if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
        {
            if (heldByKey < 0)
            {
                heldByKey = cursorNote;
                owner.pressKey (cursorNote, 0.8f);
            }

            return true;
        }

        return juce::MidiKeyboardComponent::keyPressed (key);
    }

    bool keyStateChanged (bool isKeyDown) override
    {
        bool used = juce::MidiKeyboardComponent::keyStateChanged (isKeyDown);

        if (heldByKey >= 0 && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey)
                           && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::returnKey))
        {
            owner.releaseKey (heldByKey);
            heldByKey = -1;
            used = true;
        }

        return used;
    }

    void focusGained (FocusChangeType) override { repaint(); }

    void focusLost (FocusChangeType type) override
    {
        if (heldByKey >= 0)
        {
            owner.releaseKey (heldByKey);
            heldByKey = -1;
        }

        juce::MidiKeyboardComponent::focusLost (type);
        repaint();
    }

    // accessibility 1: a named group whose description says what sounds, and
    // whose press plays the cursor's key and lets it ring (a screen reader has
    // no key-up to release on).
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (
            *this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this]
            {
                const int note = cursorNote;
                owner.pressKey (note, 0.8f);

                juce::Component::SafePointer<Keys> safe (this);
                juce::Timer::callAfterDelay (400, [safe, note]
                {
                    if (safe != nullptr)
                        safe->owner.releaseKey (note);
                });
            }));
    }

    void moveCursor (int step)
    {
        cursorNote = juce::jlimit (getRangeStart(), getRangeEnd(), cursorNote + step);

        // Keep it in view.
        if (getKeyStartPosition (cursorNote) < 0.0f || getKeyStartPosition (cursorNote) + getKeyWidth() > (float) getWidth())
            setLowestVisibleKey (juce::jmax (getRangeStart(), cursorNote - 6));

        setDescription (TuningEngine::noteName (cursorNote));
        repaint();
    }

private:
    void drawCursor (int note, juce::Graphics& g, juce::Rectangle<float> area)
    {
        if (note == cursorNote && hasKeyboardFocus (false))
        {
            g.setColour (Palette::accentBright);
            g.drawRect (area.reduced (3.0f, 3.0f), 1.5f);
        }
    }

    PianoKeyboardComponent& owner;
    int heldByKey = -1;
    std::map<int, int> fingerNotes;   ///< mouse source index -> the key it holds
};

//==============================================================================
PianoKeyboardComponent::PianoKeyboardComponent (LuthierAudioProcessor& p)
    : processor (p)
{
    lastNote.fill (-1);
    glow.fill (0.0f);
    keyLight.fill (0.0f);
    keyString.fill (-1);

    keys = std::make_unique<Keys> (*this, keyboardState);
    addAndMakeVisible (*keys);
    keyboardState.addListener (this);

    setTitle (tr ("piano.title"));
    setTooltip (tr ("piano.tooltip"));
    keys->setTitle (tr ("piano.title"));
    keys->setHelpText (tr ("piano.help"));
    keys->setTooltip (tr ("piano.tooltip"));

    auto setUpSmall = [this] (juce::TextButton& b, const juce::String& key, bool toggles)
    {
        b.setButtonText (tr (key));
        b.setTooltip (tr (key + ".tooltip"));
        b.setClickingTogglesState (toggles);
        AccessibleSetup::configureButton (b, tr (key), tr (key + ".tooltip"));
        addAndMakeVisible (b);
    };

    setUpSmall (qwertyButton, "piano.qwerty", true);
    setUpSmall (zoomOutButton, "piano.zoomOut", false);
    setUpSmall (zoomInButton, "piano.zoomIn", false);
    zoomOutButton.setButtonText ("-");
    zoomInButton.setButtonText ("+");

    qwertyButton.onClick = [this] { setQwertyPlayingEnabled (qwertyButton.getToggleState()); };
    zoomOutButton.onClick = [this] { setZoom (zoom / 1.25f); };
    zoomInButton.onClick  = [this] { setZoom (zoom * 1.25f); };

    applyColours();

    numStrings = juce::jlimit (1, kMaxStrings, processor.getEngine().getNumStrings());
    guitarRange = computeGuitarRange (processor.getEngine().getTuningEngine(), numStrings);
    applyRange();
    keys->cursorNote = juce::jlimit (getLowestKey(), getHighestKey(), guitarRange.lowest);

    setQwertyPlayingEnabled (UiPreferences::get().getBool (kQwertyPreference, false));

    refresh();
    runningHz = wantedRefreshHz();
    updateTimerState();
}

PianoKeyboardComponent::~PianoKeyboardComponent()
{
    stopTimer();

    // Nothing is left sounding because its key went away while held.
    keyboardState.allNotesOff (0);
    keyboardState.removeListener (this);

    for (auto& ancestor : watchedAncestors)
        if (ancestor != nullptr)
            ancestor->removeComponentListener (this);
}

juce::MidiKeyboardComponent& PianoKeyboardComponent::getKeys() noexcept
{
    return *keys;
}

//==============================================================================
PianoKeyboardComponent::NoteRange PianoKeyboardComponent::computeGuitarRange (const TuningEngine& tuning, int strings)
{
    NoteRange range { 127, 0 };
    const double concertA = juce::jmax (1.0, tuning.getConcertA());

    for (int s = 0; s < juce::jlimit (1, kMaxStrings, strings); ++s)
    {
        auto noteAt = [&] (double fret)
        {
            return juce::jlimit (0, 127, juce::roundToInt (hzToMidi (juce::jmax (1.0, tuning.computeFrequency (s, fret)), concertA)));
        };

        range.lowest  = juce::jmin (range.lowest, noteAt (0.0));
        range.highest = juce::jmax (range.highest, noteAt ((double) tuning.getHighestPlayableFret (s)));
    }

    if (range.lowest > range.highest)
        return {};

    return range;
}

int PianoKeyboardComponent::getLowestKey() const noexcept
{
    const int lo = guitarRange.lowest;
    return juce::jmax (0, isBlack (lo) ? lo - 1 : lo);
}

int PianoKeyboardComponent::getHighestKey() const noexcept
{
    const int hi = guitarRange.highest;
    return juce::jmin (127, isBlack (hi) ? hi + 1 : hi);
}

void PianoKeyboardComponent::applyRange()
{
    keys->setAvailableRange (getLowestKey(), getHighestKey());
    keys->cursorNote = juce::jlimit (getLowestKey(), getHighestKey(), keys->cursorNote);

    // Keep the QWERTY map's octave inside the new range.
    if (qwertyEnabled)
        setQwertyPlayingEnabled (true);

    layoutKeys();
}

//==============================================================================
void PianoKeyboardComponent::pressKey (int midiNote, float velocity)
{
    keyboardState.noteOn (LuthierAudioProcessor::kPreviewKeyboardChannel, midiNote, velocity);
}

void PianoKeyboardComponent::releaseKey (int midiNote)
{
    keyboardState.noteOff (LuthierAudioProcessor::kPreviewKeyboardChannel, midiNote, 0.0f);
}

bool PianoKeyboardComponent::isKeyHeld (int midiNote) const noexcept
{
    return keyboardState.isNoteOnForChannels (0xffff, midiNote);
}

void PianoKeyboardComponent::handleNoteOn (juce::MidiKeyboardState*, int, int note, float velocity)
{
    // The state is only ever driven from the message thread (mouse, keys and
    // pressKey), so this runs there; the processor's preview lock does the rest.
    processor.triggerPreviewMidiNote (note, velocity);
}

void PianoKeyboardComponent::handleNoteOff (juce::MidiKeyboardState*, int, int note, float)
{
    processor.releasePreviewMidiNote (note);
}

void PianoKeyboardComponent::setQwertyPlayingEnabled (bool enabled)
{
    qwertyEnabled = enabled;
    qwertyButton.setToggleState (enabled, juce::dontSendNotification);
    UiPreferences::get().setBool (kQwertyPreference, enabled);

    keys->clearKeyMappings();

    if (enabled)
    {
        const auto map = defaultQwertyKeys();

        for (int i = 0; i < map.length(); ++i)
            keys->setKeyPressForNote (juce::KeyPress (map[i], 0, 0), i);

        // The first C at or above the lowest key, so A plays inside the range.
        keys->setKeyPressBaseOctave (juce::jlimit (0, 10, (getLowestKey() + 11) / 12));
    }
}

void PianoKeyboardComponent::setZoom (float newZoom)
{
    zoom = juce::jlimit (1.0f, kMaxZoom, newZoom);
    zoomOutButton.setEnabled (zoom > 1.0f);
    zoomInButton.setEnabled (zoom < kMaxZoom);
    layoutKeys();
}

//==============================================================================
void PianoKeyboardComponent::resized()
{
    auto r = getLocalBounds().reduced (2);

    const bool side = r.getWidth() >= 200;
    qwertyButton.setVisible (side);
    zoomOutButton.setVisible (side && r.getHeight() >= 44);
    zoomInButton.setVisible (side && r.getHeight() >= 44);

    if (side)
    {
        auto column = r.removeFromLeft (kSideColumn);
        r.removeFromLeft (Metrics::gridHalf);
        qwertyButton.setBounds (column.removeFromTop (20));
        column.removeFromTop (Metrics::gridHalf);
        auto zoomRow = column.removeFromTop (20);
        zoomOutButton.setBounds (zoomRow.removeFromLeft (zoomRow.getWidth() / 2).withTrimmedRight (1));
        zoomInButton.setBounds (zoomRow.withTrimmedLeft (1));
    }

    keys->setBounds (r);
    layoutKeys();
}

void PianoKeyboardComponent::layoutKeys()
{
    const int width = keys->getWidth();

    if (width <= 0)
        return;

    int whites = 0;

    for (int n = getLowestKey(); n <= getHighestKey(); ++n)
        if (! isBlack (n))
            ++whites;

    const float fit = (float) width / (float) juce::jmax (1, whites);
    const float keyWidth = juce::jmax ((float) kMinKeyWidth, fit * zoom);

    keys->setKeyWidth (keyWidth);
    keys->setScrollButtonsVisible (keyWidth * (float) whites > (float) width + 0.5f);
}

void PianoKeyboardComponent::applyColours()
{
    lastBackground = Palette::background;

    for (auto* b : { &qwertyButton, &zoomOutButton, &zoomInButton })
    {
        b->setColour (juce::TextButton::buttonColourId, Palette::panel);
        b->setColour (juce::TextButton::buttonOnColourId, Palette::accent.withAlpha (0.25f));
        b->setColour (juce::TextButton::textColourOffId, Palette::textMuted);
        b->setColour (juce::TextButton::textColourOnId, Palette::accentBright);
    }

    // JUCE draws the background from these (it is final): the well behind the
    // keys, the line under them, and no drop shadow.
    keys->setColour (juce::MidiKeyboardComponent::whiteNoteColourId, Palette::panelSunken);
    keys->setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, Palette::edge);
    keys->setColour (juce::MidiKeyboardComponent::shadowColourId, Palette::shadow.withAlpha (0.0f));
    keys->setColour (juce::KeyboardComponentBase::upDownButtonBackgroundColourId, Palette::panel);
    keys->setColour (juce::KeyboardComponentBase::upDownButtonArrowColourId, Palette::textMuted);
    repaint();
}

void PianoKeyboardComponent::paint (juce::Graphics& g)
{
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
}

//==============================================================================
juce::Colour PianoKeyboardComponent::stringColour (int stringIndex, int strings)
{
    const auto base = Palette::accent;
    const float hue = base.getHue() + (float) juce::jmax (0, stringIndex) / (float) juce::jmax (1, strings);

    return juce::Colour::fromHSV (hue - std::floor (hue),
                                  juce::jmax (0.55f, base.getSaturation()),
                                  juce::jlimit (0.6f, 0.95f, base.getBrightness()),
                                  1.0f);
}

int PianoKeyboardComponent::getSoundingString (int midiNote) const noexcept
{
    return juce::isPositiveAndBelow (midiNote, 128) ? keyString[(size_t) midiNote] : -1;
}

float PianoKeyboardComponent::getKeyLight (int midiNote) const noexcept
{
    return juce::isPositiveAndBelow (midiNote, 128) ? keyLight[(size_t) midiNote] : 0.0f;
}

juce::String PianoKeyboardComponent::getSoundingDescription() const
{
    juce::StringArray notes;

    for (int n = 0; n < 128; ++n)
        if (keyString[(size_t) n] >= 0)
            notes.add (tr ("piano.soundingNote", { { "note", TuningEngine::noteName (n) },
                                                   { "n", juce::String (keyString[(size_t) n] + 1) } }));

    return notes.isEmpty() ? tr ("piano.silent")
                           : tr ("piano.sounding", { { "notes", notes.joinIntoString (", ") } });
}

//==============================================================================
void PianoKeyboardComponent::refresh()
{
    auto& engine = processor.getEngine();

    // ---- the range, from the tuning (strings, frets and capo can all change) ----
    const int strings = juce::jlimit (1, kMaxStrings, engine.getNumStrings());
    const auto range = computeGuitarRange (engine.getTuningEngine(), strings);

    if (strings != numStrings || range.lowest != guitarRange.lowest || range.highest != guitarRange.highest)
    {
        numStrings = strings;
        guitarRange = range;
        applyRange();
    }

    if (Palette::background != lastBackground)
        applyColours();

    // ---- what the strings are sounding --------------------------------------
    reducedMotion = AccessibilitySettings::get().isReducedMotion();

    std::array<float, 128> light {};
    std::array<int, 128> owners;
    owners.fill (-1);

    for (int s = 0; s < kMaxStrings; ++s)
    {
        if (s >= numStrings)
        {
            glow[(size_t) s] = 0.0f;
            lastNote[(size_t) s] = -1;
            continue;
        }

        const int note = engine.getStringMidiNote (s);

        // A released string rings on with no note of its own: it is still the
        // note it last played, fading as its level does.
        if (note >= 0)
            lastNote[(size_t) s] = note;

        const int shown = lastNote[(size_t) s];

        // The fretboard's scaling: a level of about 0.055 is fully lit.
        const float level = (float) juce::jlimit (0.0, 1.0, engine.getStringLevel (s) * 18.0);
        const float target = note >= 0 ? juce::jmax (level, kHeldFloor) : level;
        auto& g = glow[(size_t) s];

        if (reducedMotion)
            g = target > kLitThreshold ? 1.0f : 0.0f;   // on or off, no fade (accessibility 5)
        else
            g = g + (target - g) * (target > g ? 0.85f : 0.3f);

        if (g < 0.01f)
            g = 0.0f;

        if (g > 0.0f && juce::isPositiveAndBelow (shown, 128) && g > light[(size_t) shown])
        {
            light[(size_t) shown] = g;
            owners[(size_t) shown] = s;
        }
    }

    bool changed = false;

    for (size_t n = 0; n < 128 && ! changed; ++n)
        changed = owners[n] != keyString[n] || std::abs (light[n] - keyLight[n]) > 0.004f;

    if (changed)
    {
        const bool whoChanged = owners != keyString;
        keyLight = light;
        keyString = owners;
        keys->repaint();

        if (whoChanged)
        {
            const auto description = getSoundingDescription();
            setDescription (description);
            keys->setDescription (description);
        }
    }
}

//==============================================================================
void PianoKeyboardComponent::timerCallback()
{
    const int hz = wantedRefreshHz();

    if (hz != runningHz)
    {
        runningHz = hz;
        startTimerHz (hz);
    }

    refresh();
}

int PianoKeyboardComponent::wantedRefreshHz() const
{
    return AccessibilitySettings::get().isReducedMotion() ? kSlowHz : kFastHz;
}

void PianoKeyboardComponent::visibilityChanged()
{
    updateTimerState();
}

void PianoKeyboardComponent::parentHierarchyChanged()
{
    watchAncestors();
    updateTimerState();
}

void PianoKeyboardComponent::componentVisibilityChanged (juce::Component&)
{
    updateTimerState();
}

void PianoKeyboardComponent::watchAncestors()
{
    for (auto& ancestor : watchedAncestors)
        if (ancestor != nullptr)
            ancestor->removeComponentListener (this);

    watchedAncestors.clear();

    for (auto* c = getParentComponent(); c != nullptr; c = c->getParentComponent())
    {
        c->addComponentListener (this);
        watchedAncestors.add (c);
    }
}

void PianoKeyboardComponent::updateTimerState()
{
    bool visible = true;

    for (auto* c = static_cast<const juce::Component*> (this); c != nullptr && visible; c = c->getParentComponent())
        visible = c->isVisible();

    if (visible && ! isTimerRunning())
    {
        runningHz = wantedRefreshHz();
        startTimerHz (runningHz);
        refresh();
    }
    else if (! visible && isTimerRunning())
    {
        stopTimer();

        // Hidden with a key down (the mouse cannot come up on it now): let go.
        keyboardState.allNotesOff (0);
    }
}

//==============================================================================
PianoKeyboardDrawer::PianoKeyboardDrawer (LuthierAudioProcessor& processor)
    : keyboard (processor),
      toggle (std::make_unique<LuthierToggle> (tr ("easy.keys")))
{
    open = UiPreferences::get().getBool (kEasyPreference, false);

    toggle->setTooltip (tr ("easy.keys.tooltip"));
    toggle->getButton().setClickingTogglesState (true);
    toggle->getButton().setToggleState (open, juce::dontSendNotification);
    toggle->getButton().onClick = [this] { setOpen (toggle->getButton().getToggleState()); };
    AccessibleSetup::configureButton (toggle->getButton(), tr ("easy.keys"), tr ("easy.keys.tooltip"));
}

PianoKeyboardDrawer::~PianoKeyboardDrawer() = default;

juce::Button& PianoKeyboardDrawer::getToggle() noexcept
{
    return toggle->getButton();
}

void PianoKeyboardDrawer::addTo (juce::Component& host)
{
    hostComponent = &host;
    host.addAndMakeVisible (*toggle);
    host.addChildComponent (keyboard);
    keyboard.setVisible (open);
}

juce::Rectangle<int> PianoKeyboardDrawer::layout (juce::Rectangle<int> area, juce::Rectangle<int> toggleSlot)
{
    toggle->setBounds (toggleSlot);

    if (open)
    {
        const int height = juce::jlimit (40, 72, area.getHeight() / 4);
        keyboard.setBounds (area.removeFromBottom (height));
        area.removeFromBottom (Metrics::gridHalf);
    }

    keyboard.setVisible (open);
    return area;
}

void PianoKeyboardDrawer::setOpen (bool shouldBeOpen)
{
    open = shouldBeOpen;
    toggle->getButton().setToggleState (open, juce::dontSendNotification);
    UiPreferences::get().setBool (kEasyPreference, open);
    keyboard.setVisible (open);

    if (hostComponent != nullptr)
        hostComponent->resized();
}

} // namespace luthier
