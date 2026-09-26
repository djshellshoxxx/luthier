#pragma once

/*  The RHYTHM tab's bass step grid (bass-techniques.md 9, gui-integration.md
    4.4: "the bass step grid when the current guitar family is bass") and the
    Bass voicing's pattern (ambiguity-resolutions 4.3) - MODEL-GAPS workstream.

    One cell per step. A left-click cycles the step's technique - rest, thumb,
    pop, ghost, finger, dead - which is how bass lines are written; a
    right-click chooses the chord tone (root, fifth, octave) and the level.
    A factory grid menu loads the bass kits' grids; Clear empties it, which
    hands the rhythm back to the strum pattern.

    Like the strum grid, the grid is composition rather than automation: it
    lives in the RhythmEngine and travels with the preset through its state,
    not through parameters. Shown only on a bass (gui-integration 0.7).
*/

#include "AnimationPolicy.h"   // cpu-quality-modes 6
#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "../Rhythm/BassStepGrid.h"

namespace luthier
{

class LuthierAudioProcessor;
class RhythmEngine;

class BassStepGridView : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    explicit BassStepGridView (LuthierAudioProcessor& processor);

    void refresh();
    void setPlayingStep (int step);

    /** The click's edit, for the tests: cycles step `index`'s technique. */
    void cycleStep (int index);

    const BassStepGrid& getGrid() const noexcept { return grid; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int preferredHeight = 44;

private:
    juce::Rectangle<int> cellBounds (int step) const;
    int stepAt (juce::Point<int> position) const;
    void showStepMenu (int step);
    void commit();

    LuthierAudioProcessor& processor;
    BassStepGrid grid;
    int playingStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassStepGridView)
};

class BassGridGroup : public juce::Component,
                      private juce::Timer
{
public:
    explicit BassGridGroup (LuthierAudioProcessor& processor);
    ~BassGridGroup() override;

    /** 0 while the guitar is not a bass. */
    int preferredHeight() const;
    bool isShown() const noexcept { return shown; }

    void resized() override;

    std::function<void()> onShownChanged;

    /** Re-reads the grid and the bass pattern from the engine; follows the family. */
    void refresh();

    juce::ComboBox& getPatternBox() noexcept { return patternBox; }
    juce::ComboBox& getFactoryBox() noexcept { return factoryBox; }
    BassStepGridView& getGridView() noexcept { return gridView; }

private:
    void timerCallback() override;
    RhythmEngine& rhythm();

    LuthierAudioProcessor& processor;

    juce::Label heading, patternLabel, hint;
    juce::ComboBox patternBox, factoryBox, lengthBox;
    juce::TextButton clearButton { "CLEAR" };
    BassStepGridView gridView;

    bool shown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassGridGroup)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "BassGridGroup" };
};

} // namespace luthier
