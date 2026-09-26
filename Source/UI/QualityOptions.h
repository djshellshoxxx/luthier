#pragma once

/*  cpu-quality-modes.md 5: Options -> AUDIO, the QUALITY section.

        QUALITY
        CPU quality   ( Auto | High | Medium | Low )         <- radio pill group
        This instance [ Use global setting (High) v ]         <- override combo
        Now running: Medium (Auto)   [#####-----] 41 %        <- Luthier's own share
        [x] Always render offline at High
        [x] Tell me when Auto changes quality
        Oversampling  [ 4x v ]   Running at 2x while quality is Medium.
        What each level changes >                              <- disclosure

    AudioPage owns the oversampling control (it was there first, GAPS A3) and
    places getOversamplingNote() beside it. Rows stack below 520 px.

    Accessibility (9): the pills are a radio group named "CPU quality" (role
    group, items radioButton); the arrow keys move within it; each item's
    description is its level in one sentence.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "AnimationPolicy.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class QualityOptions final : public juce::Component,
                             private juce::ChangeListener
{
public:
    explicit QualityOptions (LuthierAudioProcessor& processor);
    ~QualityOptions() override;

    /** The height the section wants at `width`. */
    int getPreferredHeight (int width) const;

    /** Called when the disclosure opens or closes, so the page re-lays out. */
    std::function<void()> onLayoutChanged;

    /** Re-reads the settings, the controller and the oversampling note. */
    void refresh();

    /** Puts keyboard focus on the selected pill (the badge's destination). */
    void focusGroup();

    /** AudioPage shows this beside its oversampling control. */
    juce::Label& getOversamplingNote() noexcept { return oversamplingNote; }

    /** The oversampling note's text ("" when not capped). Also the tooltip of
        the Advanced column 3 Master oversampling control. */
    static juce::String oversamplingNoteFor (LuthierAudioProcessor& processor);

    //==========================================================================
    // For the tests (CQ-26, CQ-27).
    juce::Component& getGroup() noexcept { return group; }
    juce::TextButton& getPill (QualityChoice c) noexcept { return *pills[(size_t) pillIndex (c)]; }
    juce::ComboBox& getOverrideBox() noexcept { return overrideBox; }
    juce::ToggleButton& getOfflineToggle() noexcept { return offlineToggle; }
    juce::ToggleButton& getNotifyToggle() noexcept { return notifyToggle; }
    juce::String getStatusText() const;
    bool isDisclosureOpen() const noexcept { return disclosureOpen; }
    void setDisclosureOpen (bool open);

    void resized() override;
    void paint (juce::Graphics&) override;

    static int pillIndex (QualityChoice c) noexcept;   // Auto, High, Medium, Low
    static QualityChoice pillChoice (int index) noexcept;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void choose (QualityChoice c);

    //==========================================================================
    /** The radio group: a container the screen reader announces as a group. */
    class Group : public juce::Component
    {
    public:
        explicit Group (QualityOptions& o) : owner (o) { setTitle ("CPU quality"); }
        bool keyPressed (const juce::KeyPress&) override;
        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    private:
        QualityOptions& owner;
    };

    /** "Now running: ..." with Luthier's own share, 4 Hz, stepped. */
    class Status : public juce::Component, private juce::Timer
    {
    public:
        explicit Status (LuthierAudioProcessor& p);
        void paint (juce::Graphics&) override;
        juce::String getText() const { return text; }
        void update();

    private:
        void timerCallback() override { update(); }

        LuthierAudioProcessor& processor;
        AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "QualityStatus" };
        juce::String text;
        double load = 0.0;
        bool showBar = false;
    };

    LuthierAudioProcessor& processor;

    Group group { *this };
    std::array<std::unique_ptr<juce::TextButton>, 4> pills;
    juce::Label groupLabel, instanceLabel, oversamplingNote, detailsLabel;
    juce::ComboBox overrideBox;
    Status status;
    juce::ToggleButton offlineToggle, notifyToggle;
    juce::TextButton disclosureButton;
    bool disclosureOpen = false;
    bool updating = false;
};

//==============================================================================
/** cpu-quality-modes 5: the debug window's lines - the level, the effective
    oversampling per stage, the IR lengths, the modal count and the sleeping
    strings. */
namespace QualityDiagnostics
{
    juce::String describe (LuthierAudioProcessor& processor);
}

} // namespace luthier
