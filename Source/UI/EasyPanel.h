#pragma once

/*  Easy mode (gui-integration.md 3).

    +-------------------------------------------------------+--------------+
    |   GUITAR ILLUSTRATION (interactive, 3.1)              |  RIG STRIP   |
    +-------------------------------------------------------+  (3.2, 280)  |
    |  PLAYING STRIP (3.3)                                  |  circuit     |
    +-------------------------------------------------------+  pre rack    |
    |  TONE STRIP (3.4)                                     |  amp         |
    +-------------------------------------------------------+  post rack   |
    |  RHYTHM STRIP (3.5)                                   |  cab, room   |
    +-------------------------------------------------------+--------------+

    Every control here is bound to the same parameter as its twin in Advanced
    mode; nothing in Easy mode is a separate copy of the state.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "MuteGroup.h"
#include "FretboardComponent.h"
#include "GuitarBodyComponent.h"
#include "CircuitPanel.h"
#include "AmpFacePanel.h"
#include "PianoKeyboard.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/*  3.2's compact rack: eight slots as small cards; a click opens the pedal's
    full controls as a popover (the same PedalSlotComponent Advanced uses). */
class CompactRack : public juce::Component,
                    private juce::Timer
{
public:
    CompactRack (LuthierAudioProcessor& processor, bool postChain);
    ~CompactRack() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    /** The slot under a point, or -1. */
    int slotAt (juce::Point<int>) const;

    /** Opens slot `index`'s popover (tests call it directly). Returns the component shown. */
    juce::Component* openSlot (int index);

    static constexpr int kSlots = 8;

    /** Where slot `index` is drawn (two rows of four), for the layout test. */
    juce::Rectangle<int> slotBounds (int index) const;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    bool postChain;
    juce::StringArray shownNames;
    std::unique_ptr<juce::Component> unshownPopover;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompactRack)
};

//==============================================================================
class EasyPanel : public juce::Component,
                  private juce::Timer,
                  private juce::ScrollBar::Listener
{
public:
    explicit EasyPanel (LuthierAudioProcessor& processor);
    ~EasyPanel() override;

    std::function<void()> onOpenExport;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** The rig strip's width (3.2). */
    static constexpr int kRigWidth = 280;

    /** Where each strip landed, for the layout test. */
    juce::Rectangle<int> getRigArea() const noexcept { return rigArea; }
    juce::Rectangle<int> getPlayingArea() const noexcept { return playingArea; }
    juce::Rectangle<int> getToneArea() const noexcept { return toneArea; }
    juce::Rectangle<int> getRhythmArea() const noexcept { return rhythmArea; }
    GuitarBodyComponent& getGuitar() noexcept { return guitarBody; }

    /** The "Keys" toggle and the piano keyboard it opens under the guitar. */
    PianoKeyboardDrawer& getKeysDrawer() noexcept { return *keysDrawer; }

    /** 3.5's readout: the chord and the next strum's arrow. */
    juce::String getRhythmReadout() const { return rhythmReadout.getText(); }

    /** For tests: the rhythm strip's Feel knob. */
    juce::Slider& getRhythmFeelSlider() noexcept { return rhythmFeelSlider; }

    /** 3.5's dice: a random genre kit. */
    void rollRhythmDice();

    /** The rig strip's card heights for a strip `total` points tall (TODO 2h). */
    struct CardHeights { int circuit = 0, rack = 0, amp = 0, cab = 0, room = 0; };
    static CardHeights cardHeights (int total) noexcept;
    static CardHeights floorHeights() noexcept;

    /** The floors a short strip keeps: a rack's two rows of 20-point slots,
        and the guitar card's Small knobs with a body to turn. */
    static constexpr int kRackMinHeight = 64;
    static constexpr int kCircuitMinHeight = 76;

    /** Every card at its floor, together: a rig strip shorter than this
        (Live Mode with the practice drawer open, say) scrolls rather than
        squash its cards any further. */
    static int rigFloorHeight() noexcept;

    /** True when the rig strip is scrolling (it is shorter than rigFloorHeight). */
    bool isRigScrollable() const noexcept { return rigScrollBar.isVisible(); }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** The pre- and post-effects racks, for the layout test. */
    CompactRack& getPreRack() noexcept { return preRack; }
    CompactRack& getPostRack() noexcept { return postRack; }

    /** The guitar card's knobs and response view, for the layout test. */
    LuthierKnob& getGuitarVolumeKnob() noexcept { return guitarVolumeKnob; }
    LuthierKnob& getGuitarToneKnob() noexcept { return guitarToneKnob; }
    juce::Component* getCircuitView() noexcept { return circuitView.get(); }

    /** The amp card's title row, which also holds the model choice. */
    static constexpr int ampTitleRow = 24;

    /** The amp's face on the card, for the layout test. */
    AmpFacePanel& getAmpFace() noexcept { return ampFace; }

    /** The ROOM card's light (visual-polish.md 4): warm pool, reach from the
        size, strength from the wet level. Public so a test can draw it. */
    static void paintRoomLight (juce::Graphics&, juce::Rectangle<float> card, float size, float wet);
    float getRoomLightSize() const noexcept { return roomLightSize; }
    float getRoomLightWet() const noexcept { return roomLightWet; }

private:
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void layoutRigStrip();
    void refreshStyleList();
    void buildRhythmStrip();
    void buildRigStrip();
    void refreshRhythmStrip();
    void applyStylePreset (int presetIndex);

    LuthierAudioProcessor& processor;

    GuitarBodyComponent guitarBody;
    std::unique_ptr<PianoKeyboardDrawer> keysDrawer;

    // ---- playing strip (3.3): mode, the macros, whammy -------------------------------
    LuthierKnob attackKnob    { "Attack",    LuthierKnob::Size::Small };
    LuthierKnob bodyKnob      { "Body",      LuthierKnob::Size::Small };
    LuthierKnob driveKnob     { "Drive",     LuthierKnob::Size::Small };
    LuthierKnob toneKnob      { "Tone",      LuthierKnob::Size::Small };
    LuthierKnob spaceKnob     { "Space",     LuthierKnob::Size::Small };
    LuthierKnob humanizeKnob  { "Humanize",  LuthierKnob::Size::Small };
    LuthierKnob characterKnob { "Character", LuthierKnob::Size::Small };
    LuthierKnob whammyKnob    { "Whammy",    LuthierKnob::Size::Small };

    LuthierChoice playingModeSelector { "Mode" };

    // Fingers or a pick: the one right-hand choice a player reaches for by
    // the song, so it sits beside the mode rather than only in Advanced.
    LuthierToggle fingersToggle { "Fingers" };

    /** muting-rhythm 7: the 4-way Mute button (Off, Light, Heavy, Extreme). */
    std::unique_ptr<EasyMuteButton> muteButton;

    // ---- tone strip (3.4) --------------------------------------------------------------
    LuthierKnob inputKnob  { "Input",   LuthierKnob::Size::Small };
    LuthierKnob outputKnob { "Output",  LuthierKnob::Size::Small };
    LuthierKnob mixKnob    { "Wet/Dry", LuthierKnob::Size::Small };
    LuthierKnob widthKnob  { "Width",   LuthierKnob::Size::Small };

    juce::ComboBox styleBox;
    juce::Label styleLabel { {}, "Style" };

    juce::TextButton auditionButton { "Audition" };
    juce::ComboBox auditionPhraseBox;
    juce::TextButton exportButton { "Export" };
    juce::TextButton randomiseButton { "Randomise" };
    juce::TextButton resetButton { "Reset" };

    LevelMeter meter;
    juce::Label chordLabel;

    // ---- rhythm strip (3.5) ------------------------------------------------------------
    juce::Label rhythmLabel { {}, "Rhythm" };
    juce::ComboBox rhythmGenreBox;
    juce::TextButton rhythmDice { "Dice" };
    juce::Slider rhythmFeelSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton rhythmEnableButton { "OFF" };
    juce::Label rhythmHintLabel, rhythmReadout;

    // ---- rig strip (3.2) ---------------------------------------------------------------
    LuthierKnob guitarVolumeKnob { "Volume", LuthierKnob::Size::Small };
    LuthierKnob guitarToneKnob   { "Tone",   LuthierKnob::Size::Small };
    std::unique_ptr<CircuitResponseView> circuitView;

    CompactRack preRack, postRack;

    // 3.2's amp card: the model, and the amp's face with its six knobs on it (visual-polish.md 2).
    LuthierChoice ampModel { "Amp" };
    AmpFacePanel ampFace { processor, AmpFacePanel::Style::card };

    LuthierChoice cabModel { "Cab" }, mic1 { "Mic 1" }, mic2 { "Mic 2" };
    LuthierKnob micBlend { "Blend", LuthierKnob::Size::Small };

    LuthierChoice roomSize { "Room" };
    LuthierKnob roomMix { "Wet/Dry", LuthierKnob::Size::Small };

    juce::Rectangle<int> rigArea, playingArea, toneArea, rhythmArea, roomArea;

    // The rig strip scrolls when it is shorter than its cards' floors.
    juce::ScrollBar rigScrollBar { true };
    int rigScroll = 0;
    juce::Array<std::pair<juce::Rectangle<int>, juce::String>> rigCards;

    float roomLightSize = 0.5f, roomLightWet = 0.0f;

    juce::Array<int> stylePresetIndices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EasyPanel)
};

} // namespace luthier
