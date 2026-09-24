#pragma once

/*  Muting's controls (muting-rhythm.md 3 and 7, gui-techniques-updates.md 1).

    Three pieces:

      - MuteGridEditor, a row of cells each holding a mute type, painted with
        a brush. It edits whatever it is pointed at through three callbacks,
        so the same component is the live mute grid (muting-rhythm 2) and the
        RHYTHM tab's Mute Row under the strum grid (7, gui-techniques 6).
      - MuteGroup, the TECHNIQUES tab's MUTE sub-tab (gui-techniques 1): arm,
        master mode, the live grid with its presets, palm position and
        pressure, fretting-hand style, chuka source, humanise and ghost
        velocity.
      - EasyMuteButton, Easy mode's compact 4-way Mute button (7): Off, Light,
        Heavy, Extreme, applying the master mute mode.

    The grid cells are not parameters (a groove is a composition, like the
    strum grid); everything else is, and attaches like any other control.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../Rhythm/Muting.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class MuteGridEditor : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    MuteGridEditor();

    std::function<int()> getNumCells;
    std::function<MuteType (int)> getCell;
    std::function<void (int, MuteType)> setCell;

    /** The type a click or drag paints. */
    void setBrush (MuteType type) noexcept { brush = type; }
    MuteType getBrush() const noexcept { return brush; }

    /** Lights the cell being played, or none for -1. */
    void setPlayingCell (int cell);

    /** What a left click on `cell` does: paints the brush there. For tests too. */
    void paintCell (int cell);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    static constexpr int preferredHeight = 24;

private:
    int cellAt (juce::Point<int> position) const;
    juce::Rectangle<int> cellBounds (int cell) const;
    int numCells() const;
    void showCellMenu (int cell);

    MuteType brush = MuteType::palmHeavy;
    int playingCell = -1;
    int lastPainted = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuteGridEditor)
};

//==============================================================================
class MuteGroup : public juce::Component,
                  private juce::Timer
{
public:
    explicit MuteGroup (LuthierAudioProcessor& processor);
    ~MuteGroup() override;

    static constexpr int headingHeight = 20;
    static constexpr int rowHeight = 22;
    static constexpr int choiceHeight = 36;
    static constexpr int gap = 2;

    static constexpr int preferredHeight = headingHeight + gap
                                         + (Metrics::buttonHeight + gap)          // arm
                                         + 2 * (choiceHeight + gap)               // master, fretting style
                                         + (rowHeight + gap)                      // brush + preset
                                         + (MuteGridEditor::preferredHeight + gap)
                                         + 4 * (rowHeight + gap)                  // position, pressure, humanise, ghost
                                         + (choiceHeight + gap)                   // chuka source
                                         + 4;

    /** Re-reads the live grid. */
    void refresh();

    /** 6: writes a preset's sixteen cells into the live grid. */
    void applyPreset (int index);

    void resized() override;

    // For tests.
    MuteGridEditor& getLiveGrid() noexcept      { return liveGrid; }
    juce::ComboBox& getBrushBox() noexcept      { return brushBox; }
    juce::ComboBox& getPresetBox() noexcept     { return presetBox; }
    LuthierToggle& getArmToggle() noexcept      { return armToggle; }
    LuthierChoice& getMasterModeControl() noexcept { return masterMode; }

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;

    juce::Label heading;
    LuthierToggle armToggle { "MUTE ARMED" };
    LuthierChoice masterMode { "Master mode" }, frettingStyle { "Fretting hand" },
                  chukaSource { "Chuka source" };
    juce::ComboBox brushBox, presetBox;
    MuteGridEditor liveGrid;
    LuthierSlider palmPosition { "Palm position" }, palmPressure { "Palm pressure" },
                  humanise { "Humanise" }, ghostVelocity { "Ghost level" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuteGroup)
};

//==============================================================================
/*  muting-rhythm 7: Easy mode's Mute button. Each click moves Off -> Light ->
    Heavy -> Extreme -> Off; Off disarms muting, the others arm it with that
    master mode. It reads the parameters back, so a preset or the Advanced
    controls moving them show here. */
class EasyMuteButton : public juce::TextButton,
                       private juce::Timer
{
public:
    explicit EasyMuteButton (LuthierAudioProcessor& processor);
    ~EasyMuteButton() override;

    /** 0 Off, 1 Light, 2 Heavy, 3 Extreme; anything else the parameters hold reads as its nearest. */
    int getState() const;
    void refresh();

private:
    void timerCallback() override;
    void write (int state);

    LuthierAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EasyMuteButton)
};

} // namespace luthier
