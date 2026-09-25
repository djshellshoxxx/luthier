#pragma once

/*  The piano keyboard: play the guitar from piano keys, and see which keys the
    strings are sounding.

    It spans the current guitar's playable range - the lowest open string (with
    the capo) to the top fret of the highest string - padded out to white keys
    at each end, and fits that range to its width; when the keys would be too
    narrow it scrolls (the wheel, or the arrow buttons at either end), and
    Ctrl/Cmd + wheel or the - / + buttons zoom. Each C is labelled with its
    octave (C2, C3 ...), in the guitarist's convention of middle C = C4.

    Playing. A click on a key queues a real note-on into the processor's preview
    MIDI (LuthierAudioProcessor::triggerPreviewMidiNote), so the plugin's own
    voicer chooses the string exactly as it does for a host's MIDI; releasing
    the mouse queues the note-off. Velocity comes from where on the key you
    click (nearer the player is louder), and dragging across keys is a
    glissando. It is built on juce::MidiKeyboardComponent over a UI-side
    juce::MidiKeyboardState whose listener forwards to the processor, which
    keeps the host's notes out of the "pressed" state.

    The computer keyboard (QWERTY, A W S E D ...) can play it too, but that
    map collides with the editor's single-key shortcuts (S slide mode, D
    practice, T tap tempo, L live, P panic), so it is off until the QWERTY
    toggle is on (remembered). With the keyboard focused, Left / Right move a
    key cursor and Space or Enter plays that key, so it is usable without a
    mouse either way.

    Mirroring. Thirty times a second (ten under reduced motion) it reads each
    string's sounding note and level from the engine - the same reads the
    fretboard and the string roll make - so notes from the host, the rhythm
    engine's strums, the tune player and clicks on the fretboard all light
    their keys. Each string has its own colour, and a lit key carries its
    string's number, so colour is never the only cue; the light follows the
    string's level, so a ringing note fades. Under reduced motion a key is
    simply lit or not. The keys the user is holding are drawn pressed (an
    outline and a darker face), which is distinct from sounding.

    Colours come only from Palette, so it follows every palette, the light one
    included: the white keys are whichever of the text and background colours
    is the lighter.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "Theme.h"

#include <array>
#include <memory>

namespace luthier
{

class LuthierAudioProcessor;
class TuningEngine;
class LuthierToggle;

//==============================================================================
class PianoKeyboardComponent : public juce::Component,
                               public juce::SettableTooltipClient,
                               private juce::Timer,
                               private juce::ComponentListener,
                               private juce::MidiKeyboardState::Listener
{
public:
    explicit PianoKeyboardComponent (LuthierAudioProcessor& processor);
    ~PianoKeyboardComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    //==========================================================================
    /** The notes a guitar can sound, lowest open string to the top fret of the
        highest string, inclusive. */
    struct NoteRange { int lowest = 40, highest = 88; };
    static NoteRange computeGuitarRange (const TuningEngine& tuning, int numStrings);

    /** The range the guitar has now (as last refreshed). */
    NoteRange getGuitarRange() const noexcept { return guitarRange; }

    /** The keys drawn: the guitar's range widened to white keys at both ends. */
    int getLowestKey() const noexcept;
    int getHighestKey() const noexcept;

    //==========================================================================
    /** What a click does (tests and the accessibility action call these). */
    void pressKey (int midiNote, float velocity);
    void releaseKey (int midiNote);
    bool isKeyHeld (int midiNote) const noexcept;

    /** QWERTY playing: off by default (it shadows single-key shortcuts). */
    void setQwertyPlayingEnabled (bool enabled);
    bool isQwertyPlayingEnabled() const noexcept { return qwertyEnabled; }

    /** Zoom: 1 fits the range to the width; above it the keys scroll. */
    void setZoom (float newZoom);
    float getZoom() const noexcept { return zoom; }

    //==========================================================================
    /** Re-reads the engine (the timer's work; tests call it). */
    void refresh();

    /** The string lighting a key, or -1; and how brightly (0..1). */
    int getSoundingString (int midiNote) const noexcept;
    float getKeyLight (int midiNote) const noexcept;

    /** What a screen reader hears: the notes sounding and on which strings. */
    juce::String getSoundingDescription() const;

    /** The per-string colour, from the palette's accent turned round the hue
        circle so each string of the guitar gets its own. */
    static juce::Colour stringColour (int stringIndex, int numStrings);

    //==========================================================================
    juce::MidiKeyboardComponent& getKeys() noexcept;
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    juce::Button& getQwertyButton() noexcept { return qwertyButton; }

    // The timer runs only while the keyboard can be seen (as the string roll's).
    void visibilityChanged() override;
    void parentHierarchyChanged() override;
    bool isRefreshRunning() const noexcept { return isTimerRunning(); }

    static constexpr int kMinKeyWidth = 12;

private:
    class Keys;
    friend class Keys;

    void timerCallback() override;
    void handleNoteOn (juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int channel, int note, float velocity) override;

    void componentVisibilityChanged (juce::Component&) override;
    void watchAncestors();
    void updateTimerState();
    int wantedRefreshHz() const;
    void applyRange();
    void layoutKeys();
    void applyColours();

    LuthierAudioProcessor& processor;
    juce::MidiKeyboardState keyboardState;
    std::unique_ptr<Keys> keys;

    juce::TextButton qwertyButton, zoomOutButton, zoomInButton;

    NoteRange guitarRange;
    int numStrings = 6;
    bool qwertyEnabled = false;
    float zoom = 1.0f;
    int runningHz = 0;
    bool reducedMotion = false;
    juce::Colour lastBackground;

    static constexpr int kMaxStrings = 12;
    std::array<int, kMaxStrings> lastNote {};
    std::array<float, kMaxStrings> glow {};
    std::array<float, 128> keyLight {};
    std::array<int, 128> keyString {};

    juce::Array<juce::Component::SafePointer<juce::Component>> watchedAncestors;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoKeyboardComponent)
};

//==============================================================================
/*  Easy mode's "Keys" toggle and the keyboard it opens along the bottom of the
    guitar area. The host adds it, then hands its layout the guitar's area and
    a slot for the toggle, and uses what comes back for the guitar. */
class PianoKeyboardDrawer
{
public:
    explicit PianoKeyboardDrawer (LuthierAudioProcessor& processor);
    ~PianoKeyboardDrawer();

    void addTo (juce::Component& host);

    /** Places the toggle in `toggleSlot` and, when open, the keyboard along the
        bottom of `area`; returns what is left of `area`. */
    juce::Rectangle<int> layout (juce::Rectangle<int> area, juce::Rectangle<int> toggleSlot);

    void setOpen (bool shouldBeOpen);
    bool isOpen() const noexcept { return open; }

    juce::Button& getToggle() noexcept;
    PianoKeyboardComponent& getKeyboard() noexcept { return keyboard; }

private:
    PianoKeyboardComponent keyboard;
    std::unique_ptr<LuthierToggle> toggle;
    juce::Component::SafePointer<juce::Component> hostComponent;
    bool open = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoKeyboardDrawer)
};

} // namespace luthier
