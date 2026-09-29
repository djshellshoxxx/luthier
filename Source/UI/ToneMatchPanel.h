#pragma once

/*  The TONE MATCH panel (tone-match.md section 6).

    Three IR slot editors, the two match wizards, and the capture utility.

    The wizards are the part with a shape worth stating: tone-match 2 and 3 are
    both multi-step procedures, and a wizard that opened a modal dialog per step
    would be unusable while the user is meant to be playing. They run in place
    instead - the panel shows one step at a time with a single button to advance,
    and the panel stays interactive throughout.
*/

#include "AnimationPolicy.h"   // cpu-quality-modes 6
#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "../ToneMatch/ToneMatch.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** One IR slot's card (tone-match 1). */
class IrSlotEditor : public juce::Component,
                     public juce::FileDragAndDropTarget
{
public:
    enum class Slot { body = 0, cab1, cab2 };

    IrSlotEditor (LuthierAudioProcessor& processor, Slot slot);
    ~IrSlotEditor() override;

    void refresh();

    std::function<void()> onChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    static constexpr int preferredHeight = 176;

private:
    IrSlot& slot();
    void pushIrEdit (const char* what, bool groups);   // action-and-undo.md

    void load (const juce::File& file);

    LuthierAudioProcessor& processor;
    Slot which;

    std::unique_ptr<LuthierToggle> engageToggle;
    juce::Label nameLabel, infoLabel;
    juce::TextButton loadButton { "Load..." }, clearButton { "Clear" };

    juce::ComboBox channelBox;
    juce::Slider gainTrim { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider predelay { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider startTrim { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider endTrim { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider mix { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton reverseButton { "Reverse" };

    std::unique_ptr<juce::FileChooser> chooser;

    bool dragging = false;
    bool updatingControls = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IrSlotEditor)
};

//==============================================================================
/** The cab-match and EQ-match wizards, and the capture utility
    (tone-match 2, 3 and 4). */
class MatchWizard : public juce::Component,
                    private juce::Timer
{
public:
    enum class Kind { cabMatch = 0, eqMatch, capture };

    MatchWizard (LuthierAudioProcessor& processor, Kind kind);
    ~MatchWizard() override;

    std::function<void()> onFinished;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int preferredHeight = 124;

private:
    void timerCallback() override;

    void advance();
    void restart();

    juce::String getStepText() const;

    LuthierAudioProcessor& processor;
    Kind kind;

    /** Where the wizard is. Step zero is "not started". */
    int step = 0;

    juce::Label stepLabel, resultLabel;
    juce::TextButton actionButton { "Start" }, cancelButton { "Cancel" };

    juce::ComboBox signalBox, lengthBox;
    juce::Slider aggressiveness { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton preserveDynamics { "Preserve dynamics" };

    /** The reference and the plugin's own output, recorded in turn. */
    std::vector<float> reference, current;

    double nullResultDb = 0.0;
    bool haveResult = false;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatchWizard)

private:
    // cpu-quality-modes 6: the motion switch.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "MatchWizard" };
};

//==============================================================================
class ToneMatchPanel : public juce::Component
{
public:
    explicit ToneMatchPanel (LuthierAudioProcessor& processor);
    ~ToneMatchPanel() override;

    int preferredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshLibrary();
    void loadSelectedFromLibrary();

    LuthierAudioProcessor& processor;

    std::unique_ptr<IrSlotEditor> bodySlot, cabSlot1, cabSlot2;
    std::unique_ptr<MatchWizard> cabMatch, eqMatch, capture;

    // --- library browser (tone-match 6) -------------------------------------------
    juce::ComboBox tagFilter;
    juce::ListBox libraryList;
    juce::TextButton refreshButton { "Rescan" };
    juce::Label libraryHint;

    juce::Array<juce::File> libraryFiles;
    juce::Array<juce::File> visibleFiles;

    class LibraryModel : public juce::ListBoxModel
    {
    public:
        explicit LibraryModel (ToneMatchPanel& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    private:
        ToneMatchPanel& owner;
    };

    LibraryModel listModel { *this };

    juce::Label slotsHeading, wizardsHeading, libraryHeading;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneMatchPanel)
};

} // namespace luthier
