#pragma once

/*  Overlays: help, options, debug, export, the preset browser, the chord library,
    the tab display and the hidden effect.

    Build spec: "Every overlay dismissible by Escape, click-outside, and visible
    close button. Never stuck. Only one overlay visible at a time."

    That is enforced structurally rather than by convention. OverlayHost owns the
    scrim and shows exactly one panel; every panel derives from OverlayPanel, which
    provides the title bar, the close button and the Escape handler. A panel cannot
    be shown without them.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "OptionsPages.h"
#include "HelpTab.h"
#include "../DSP/Common/DspCommon.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class OverlayPanel : public juce::Component
{
public:
    explicit OverlayPanel (const juce::String& title);
    ~OverlayPanel() override;

    /** Called by the host when the panel is about to be shown. */
    virtual void overlayShown() {}

    /** Called when it is dismissed, however that happened. */
    virtual void overlayHidden() {}

    /** Preferred size; the host centres the panel at this size where it fits. */
    virtual juce::Point<int> getPreferredSize() const { return { 720, 520 }; }

    std::function<void()> onDismiss;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    /** The area inside the title bar and padding, where subclasses put content. */
    juce::Rectangle<int> getContentBounds() const;

protected:
    /** Subclasses lay their content out here rather than in resized(). */
    virtual void layoutContent (juce::Rectangle<int> content) { juce::ignoreUnused (content); }

    juce::String title;

private:
    juce::TextButton closeButton { "Close" };

    static constexpr int titleBarHeight = 40;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OverlayPanel)
};

//==============================================================================
/*  The MIDI-learn arm layer.

    While MIDI Learn is armed, this sits invisibly over the whole editor and takes
    the next click. It finds what was underneath, walks up to the nearest
    LearnTarget, and starts learning for that parameter.

    Taking the click rather than merely observing it is the point: a global mouse
    listener would see the press but the control would still act on it, so arming
    and then clicking a knob would move the knob. Here the click is consumed and
    the only thing that happens is the arm landing.
*/
class MidiLearnArmLayer : public juce::Component
{
public:
    MidiLearnArmLayer();

    /** Called with the parameter the user picked, or empty if they clicked
        somewhere that is not a control. */
    std::function<void (juce::String)> onTargetPicked;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnArmLayer)
};

//==============================================================================
/** Owns the scrim and guarantees that at most one overlay is visible. */
class OverlayHost : public juce::Component
{
public:
    OverlayHost();
    ~OverlayHost() override;

    /** Shows a panel. Any panel already up is dismissed first. The host does not
        take ownership; the caller keeps the panel alive. */
    void show (OverlayPanel* panel);

    void dismiss();

    bool isShowingOverlay() const noexcept { return current != nullptr; }
    OverlayPanel* getCurrentOverlay() const noexcept { return current; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    OverlayPanel* current = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OverlayHost)
};

//==============================================================================
/** Help: what everything does, how to use it, licence, links and troubleshooting. */
class HelpPanel : public OverlayPanel
{
public:
    explicit HelpPanel (LuthierAudioProcessor& processor);

    juce::Point<int> getPreferredSize() const override { return { 860, 600 }; }

    HelpTab& getView() noexcept { return view; }

    /** Pins the help to a panel's topic; false (and unchanged) for a name no
        topic answers to. */
    bool showTopicFor (const juce::String& name) { return view.showTopicFor (name); }

protected:
    void layoutContent (juce::Rectangle<int> content) override { view.setBounds (content); }

private:
    HelpTab view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpPanel)
};

//==============================================================================
/** The debug window: live internals, crash logging and the destructive reset. */
class DebugPanel : public OverlayPanel,
                   private juce::Timer
{
public:
    explicit DebugPanel (LuthierAudioProcessor& processor);
    ~DebugPanel() override;

    juce::Point<int> getPreferredSize() const override { return { 900, 620 }; }

    void overlayShown() override;
    void overlayHidden() override;

protected:
    void layoutContent (juce::Rectangle<int> content) override;

private:
    void timerCallback() override;
    void refreshState();

    LuthierAudioProcessor& processor;

    juce::TextEditor stateView, streamView;
    juce::ToggleButton crashLogToggle { "Create log file on crash" };
    juce::TextButton troubleshootButton { "Export troubleshooting file" };
    juce::TextButton openFolderButton { "Open diagnostics folder" };
    juce::TextButton hardResetButton { "Reset all settings and clear caches" };
    juce::TextButton clearButton { "Clear stream" };
    juce::Label explanation;

    int lastStreamCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DebugPanel)
};

//==============================================================================
/*  Options.

    One overlay, one tab per page, in gui-integration.md section 5's order. The
    pages themselves are in OptionsPages.h, which is also where the two
    departures from section 5's list - no RANGES, an extra CONTROLLERS - are
    explained.
*/
class OptionsPanel : public OverlayPanel
{
public:
    explicit OptionsPanel (LuthierAudioProcessor& processor);

    /*  Wider and taller than the other overlays: eleven tabs need two rows of
        strip, and the Controllers, Accessibility and Privacy pages put a label
        beside every control, which does not fit in 720. */
    juce::Point<int> getPreferredSize() const override { return { 860, 620 }; }

    void overlayShown() override;

    /** Opens the page holding the rebindable shortcut table (accessibility 2's
        "show all shortcuts" surface). */
    void showShortcutTable();

    /*  The Diagnostics page offers the debug window, and an overlay cannot put
        another overlay on screen - only the editor can - so the request comes out
        here and the editor wires it. */
    std::function<void()> onShowDebugWindow;

    /*  Opens a page by its tab name, for callers that want a specific page and
        should not have to know its index. A notification banner sending the user
        to PRIVACY is the case this exists for: section 5 fixes the order, but
        RANGES arriving would still shift every index after it, and a banner that
        quietly opened the wrong page would be worse than one that did nothing.

        Returns false if there is no such tab, which is how a caller finds out
        that the page it wanted has not been built. Case-insensitive. */
    bool showPageNamed (const juce::String& tabName);

    /** The tab names, in order, as the strip shows them. */
    juce::StringArray getPageNames() const;

protected:
    void layoutContent (juce::Rectangle<int> content) override;

private:
    void showPage (int index);

    juce::OwnedArray<juce::TextButton> pageButtons;
    juce::OwnedArray<OptionsPage> pages;
    int currentPage = 0;

    LuthierAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptionsPanel)
};

//==============================================================================
/** Audio export, with format, depth, rate, length, normalise and the source. */
class ExportPanel : public OverlayPanel,
                    private juce::Timer
{
public:
    explicit ExportPanel (LuthierAudioProcessor& processor);
    ~ExportPanel() override;

    juce::Point<int> getPreferredSize() const override { return { 620, 480 }; }

    void overlayShown() override;

protected:
    void layoutContent (juce::Rectangle<int> content) override;

private:
    void timerCallback() override;
    void startExport();
    void updateEstimate();

    LuthierAudioProcessor& processor;

    juce::ComboBox sourceBox, formatBox, bitDepthBox, sampleRateBox, phraseBox;
    juce::Slider tailSlider;
    juce::ToggleButton normaliseToggle { "Normalise" };
    juce::Slider normaliseTarget;
    juce::TextEditor fileNameEditor;
    juce::TextButton chooseFolderButton { "Folder..." };
    juce::TextButton exportButton { "Export" };
    juce::TextButton cancelButton { "Cancel" };
    juce::Label statusLabel, estimateLabel;
    juce::ProgressBar progressBar { progress };

    juce::File destinationFolder;
    juce::File importedMidiFile;
    double progress = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExportPanel)
};

//==============================================================================
/** The preset browser: categories, search, tags, load, delete. */
class PresetBrowserPanel : public OverlayPanel,
                           private juce::ChangeListener
{
public:
    explicit PresetBrowserPanel (LuthierAudioProcessor& processor);
    ~PresetBrowserPanel() override;

    juce::Point<int> getPreferredSize() const override { return { 780, 560 }; }

    /** Raised when the user asks to save the current sound as a new preset. */
    std::function<void()> saveAsPanelRequested;

    void overlayShown() override;

protected:
    void layoutContent (juce::Rectangle<int> content) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuildList();
    void loadSelected();

    LuthierAudioProcessor& processor;

    juce::TextEditor searchBox;
    juce::ComboBox categoryBox;
    juce::ListBox list;
    juce::Label description;
    juce::TextButton loadButton { "Load" };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton saveAsButton { "Save As..." };

    // ambiguity-resolutions.md 5.2: Morph, its two slots and the slider.
    void refreshMorph();
    juce::TextButton morphToggle { "Morph" };
    juce::TextButton slotAButton { "A" }, slotBButton { "B" };
    juce::Slider morphSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;

    juce::Array<int> visibleIndices;

    class PresetListModel : public juce::ListBoxModel
    {
    public:
        explicit PresetListModel (PresetBrowserPanel& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        void selectedRowsChanged (int lastRow) override;

    private:
        PresetBrowserPanel& owner;
    };

    PresetListModel listModel { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserPanel)
};

//==============================================================================
/** Save As: name, category, description and tags. */
class SaveAsPanel : public OverlayPanel
{
public:
    explicit SaveAsPanel (LuthierAudioProcessor& processor);

    juce::Point<int> getPreferredSize() const override { return { 520, 320 }; }

    void overlayShown() override;

protected:
    void layoutContent (juce::Rectangle<int> content) override;

private:
    LuthierAudioProcessor& processor;

    juce::TextEditor nameEditor, descriptionEditor, tagsEditor;
    juce::ComboBox categoryBox;
    juce::TextButton saveButton { "Save" };
    juce::Label status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaveAsPanel)
};

//==============================================================================
/** The chord library and the live tab display. */
class ChordAndTabPanel : public OverlayPanel,
                         private juce::Timer
{
public:
    explicit ChordAndTabPanel (LuthierAudioProcessor& processor);
    ~ChordAndTabPanel() override;

    juce::Point<int> getPreferredSize() const override { return { 800, 560 }; }

    void overlayShown() override;

protected:
    void layoutContent (juce::Rectangle<int> content) override;

private:
    void timerCallback() override;
    void captureTabColumn();

    LuthierAudioProcessor& processor;

    juce::TextEditor searchBox;
    juce::ListBox chordList;
    juce::Component chordDiagram;
    juce::TextEditor tabView;
    juce::TextButton clearTabButton { "Clear tab" };
    juce::TextButton exportTabButton { "Export tab" };
    juce::ToggleButton recordTabToggle { "Capture what I play" };

    juce::Array<int> matches;
    int selectedChord = 0;

    juce::StringArray tabLines;
    std::array<int, kMaxStrings> lastNotes {};

    class ChordListModel : public juce::ListBoxModel
    {
    public:
        explicit ChordListModel (ChordAndTabPanel& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void selectedRowsChanged (int lastRow) override;

    private:
        ChordAndTabPanel& owner;
    };

    class DiagramComponent : public juce::Component
    {
    public:
        explicit DiagramComponent (ChordAndTabPanel& o) : owner (o) {}
        void paint (juce::Graphics&) override;

    private:
        ChordAndTabPanel& owner;
    };

    ChordListModel listModel { *this };
    DiagramComponent diagram { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordAndTabPanel)
};

//==============================================================================
/** The hidden effect, revealed by clicking one specific pixel. */
class SecretPanel : public OverlayPanel
{
public:
    explicit SecretPanel (LuthierAudioProcessor& processor);

    juce::Point<int> getPreferredSize() const override { return { 560, 300 }; }

protected:
    void layoutContent (juce::Rectangle<int> content) override;

private:
    LuthierAudioProcessor& processor;

    LuthierKnob rateKnob { "Warp Rate" };
    LuthierKnob depthKnob { "Warp Depth" };
    LuthierKnob feedbackKnob { "Regeneration" };
    LuthierKnob mixKnob { "Mix" };
    LuthierToggle enableToggle { "Engage" };
    juce::Label blurb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SecretPanel)
};

} // namespace luthier
