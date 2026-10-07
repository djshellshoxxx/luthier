#pragma once

/*  piano-roll-chord-display.md 1-3: the piano roll strip.

    It mirrors what the strings sound (the SoundingNotes the audio thread
    publishes, drained by a 30 Hz timer) and plays the guitar from its keys
    through the processor's preview MIDI, channel 1, like any other note.

    - Header: ROLL / KEYS, Latch (with Play and Clear), Show fingering; in
      Advanced mode a collapse arrow and a drag handle (40 - 140 px).
    - Keys: click or drag (a glissando), velocity from where on the key (top
      40, bottom 110). With the strip focused, the computer keyboard plays:
      A-L white keys, W-P black keys, Z / X an octave down / up.
    - Latch: clicks toggle keys into a held set; Play (or Enter) sends it as
      one chord - strummed by the interpreter, or by the rhythm engine when it
      is on; Clear (or Escape) empties it.
    - Show fingering: the latched (or held) keys voiced by a RubricVoicer on
      this guitar's tuning, drawn as ghost dots on the fretboard before
      anything sounds; a note below the lowest string is marked on its key.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "PianoRollModel.h"
#include "FretboardComponent.h"
#include "AnimationPolicy.h"   // cpu-quality-modes 6

namespace luthier
{

class LuthierAudioProcessor;

class PianoRollStrip : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
public:
    PianoRollStrip (LuthierAudioProcessor& processor, bool advancedMode);
    ~PianoRollStrip() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType) override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    //==========================================================================
    /** The height it wants now: Advanced opens at 72 (keys 24 + roll 48) and is
        sized 40 - 140 by its handle, or just its header while collapsed; Easy
        is 56. */
    int getPreferredHeight() const;

    /** The option for this mode (Options -> Visual aids -> Show piano roll). */
    bool isWanted() const;

    /** Called when the preferred height or the wish to be shown changed. */
    std::function<void()> onLayoutChanged;

    /** Where "Show fingering" draws (the fretboard in the same window). */
    void setFretboard (FretboardComponent* board) { fretboard = board; updateFingering(); }

    /** Or, where there is no fretboard (Easy), whoever draws the dots instead. */
    std::function<void (const std::vector<FretboardComponent::GhostDot>&)> onGhostDots;

    /** The fingering last worked out (string, fret from the capo). */
    const std::vector<FretboardComponent::GhostDot>& getGhostDots() const noexcept { return ghostDots; }

    //==========================================================================
    // For tests and the accessibility layer.
    const PianoRollModel& getModel() const noexcept { return model; }
    PianoRollModel::Range getRange() const noexcept { return range; }
    juce::Rectangle<float> getKeyBounds (int note) const;
    int noteAt (juce::Point<float> position, float* velocity = nullptr) const;

    void pressKey (int note, float velocity);
    void releaseKey (int note);
    void playLatched();
    void clearLatched();
    const juce::Array<int>& getLatched() const;
    bool isLatchOn() const;
    void setLatch (bool on);
    void setShowFingering (bool on);
    bool isShowingRoll() const;
    bool isCollapsed() const;
    void setCollapsed (bool collapsed);

    /** Notes Show fingering could not place: under the lowest string. */
    const juce::Array<int>& getUnreachable() const noexcept { return unreachable; }

    /** One timer frame with the clock passed in (tests). */
    void tick (double nowMs);

    juce::TextButton& getModeButton() noexcept { return modeButton; }
    juce::TextButton& getLatchButton() noexcept { return latchButton; }
    juce::TextButton& getFingeringButton() noexcept { return fingeringButton; }
    juce::TextButton& getPlayButton() noexcept { return playButton; }
    juce::TextButton& getClearButton() noexcept { return clearButton; }
    juce::TextButton& getCollapseButton() noexcept { return collapseButton; }

    static constexpr int kHeaderWidth = 112;
    static constexpr int kCollapsedHeight = 22;
    static constexpr int kEasyHeight = 56;

private:
    void timerCallback() override;
    void updateFingering();
    void refreshButtons();
    void computerKeyNote (int semitone, bool down);
    static bool isBlack (int note) noexcept;

    LuthierAudioProcessor& processor;
    // cpu-quality-modes 6: the roll's scroll is a live readout (10 Hz, stepped, at Low).
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "PianoRollStrip" };
    const bool advanced;
    FretboardComponent* fretboard = nullptr;

    PianoRollModel model;
    PianoRollModel::Range range;
    std::uint32_t lastSequence = 0;
    double lastPublishMs = 0.0, lastTickMs = 0.0;
    bool lastTickQuiet = false;

    juce::TextButton modeButton { "ROLL" }, latchButton { "LATCH" }, fingeringButton { "FINGERING" },
                     playButton { "PLAY" }, clearButton { "CLEAR" }, collapseButton;

    juce::Rectangle<int> header, keysArea, rollArea, handleArea;

    int mouseNote = -1;                 ///< the key the pointer holds down
    bool draggingHandle = false;
    int dragStartHeight = 0;

    juce::Array<int> held;              ///< keys held by pointer or computer keyboard (unlatched)
    juce::Array<int> sentChord;         ///< the latched chord last played, released on the next Play or Clear
    juce::Array<int> unreachable;
    std::vector<FretboardComponent::GhostDot> ghostDots;

    int computerOctave = 4;             ///< the octave A plays C of (C4 = 60)
    std::array<bool, 17> computerDown {};
    bool wasWanted = false, lastShowsRoll = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollStrip)
};

} // namespace luthier
