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
#include "RightHandGroup.h"   // REALISM-B
#include "Widgets.h"
#include "FretboardComponent.h"
#include "GuitarBodyComponent.h"
#include "CircuitPanel.h"
#include "AmpFacePanel.h"
#include "PanelHelpButton.h"

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

private:
    void timerCallback() override;
    juce::Rectangle<int> slotBounds (int index) const;

    LuthierAudioProcessor& processor;
    bool postChain;
    juce::StringArray shownNames;
    std::unique_ptr<juce::Component> unshownPopover;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompactRack)
};

//==============================================================================
class EasyPanel : public juce::Component,
                  private juce::Timer
{
public:
    /** SPEC-SWEEP (GD-9, gui-engine-dataflow 4): the chord readout keeps the
        last chord and dims it once kChordStaleMs pass without a new one. */
    static constexpr double kChordStaleMs = 3000.0;
    void tickChordReadout (double nowMs);
    juce::String getChordReadoutText() const { return chordLabel.getText(); }
    bool isChordReadoutDimmed() const { return chordLabel.findColour (juce::Label::textColourId) != Palette::accent; }

    explicit EasyPanel (LuthierAudioProcessor& processor);
    ~EasyPanel() override;

    std::function<void()> onOpenExport;

    /** gui-integration 20 (TUNE-HELP-ONBOARDING): a strip's ? asks the editor
        for Help pinned to it. */
    std::function<void (const juce::String& topic)> onOpenHelp;
    std::vector<PanelHelpButton*> getHelpButtons() { return { &rigHelp, &playingHelp, &toneHelp, &rhythmHelp }; }

    /** onboarding 4: the Randomise button the first-week tooltip is on. */
    juce::Button& getRandomiseButton() noexcept { return randomiseButton; }

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

    /** 3.5's readout: the chord and the next strum's arrow. */
    juce::String getRhythmReadout() const { return rhythmReadout.getText(); }

    /** For tests: the rhythm strip's Feel knob. */
    juce::Slider& getRhythmFeelSlider() noexcept { return rhythmFeelSlider; }

    /** 3.5's dice: a random genre kit. */
    void rollRhythmDice();

private:
    void timerCallback() override;
    void refreshStyleList();
    void buildRhythmStrip();
    void buildRigStrip();
    void refreshRhythmStrip();
    void applyStylePreset (int presetIndex);

    LuthierAudioProcessor& processor;

    GuitarBodyComponent guitarBody;

    // ---- playing strip (3.3): mode, the macros, whammy -------------------------------
    LuthierKnob attackKnob    { "Attack",    LuthierKnob::Size::Small };
    LuthierKnob bodyKnob      { "Body",      LuthierKnob::Size::Small };
    LuthierKnob driveKnob     { "Drive",     LuthierKnob::Size::Small };
    LuthierKnob toneKnob      { "Tone",      LuthierKnob::Size::Small };
    LuthierKnob spaceKnob     { "Space",     LuthierKnob::Size::Small };
    std::unique_ptr<RightHandToolSelector> toolSelector;   // REALISM-B: fingerstyle-attack.md 7, the Tool selector
    LuthierKnob humanizeKnob  { "Humanize",  LuthierKnob::Size::Small };
    LuthierKnob characterKnob { "Character", LuthierKnob::Size::Small };
    LuthierKnob whammyKnob    { "Whammy",    LuthierKnob::Size::Small };

    LuthierChoice playingModeSelector { "Mode" };

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
    double lastChordMs = -1.0e9;   // SPEC-SWEEP GD-9

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

    juce::Rectangle<int> rigArea, playingArea, toneArea, rhythmArea;
    juce::Array<std::pair<juce::Rectangle<int>, juce::String>> rigCards;

    juce::Array<int> stylePresetIndices;

    // gui-integration 20: one ? per strip.
    PanelHelpButton rigHelp { "Rig" }, playingHelp { "Playing" }, toneHelp { "Tone" }, rhythmHelp { "Rhythm" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EasyPanel)
};

} // namespace luthier
