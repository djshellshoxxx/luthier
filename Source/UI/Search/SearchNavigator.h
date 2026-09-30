#pragma once

/*  global-search.md 4.2, 3.3, 6.1: going there, and the editor's half of search.

    SearchNavigator is what one editor owns for search: the index, the action
    registry, the palette, the highlighter, and the navigation itself. It is
    the SearchServices the built-in providers act through and the
    SearchContext a feature's provider is handed on activation, so neither
    has to know the editor.

    It is a friend of LuthierAudioProcessorEditor rather than a set of new
    public editor methods: the editor keeps its one new public entry point,
    performAction, and the reaching-in is in this file, where search owns it.

    Navigation derives the route to a control by walking up its parents and
    running each tagged ancestor's opener (SearchAnchors), outermost first -
    Easy/Advanced, a workspace tab, an Options page, a drawer tab, an overlay.
    Controls that exist only in a popover come from ParameterLocations.

    Message thread only.
*/

#include "SearchIndex.h"
#include "SearchProviders.h"
#include "ActionRegistry.h"
#include "InlineValue.h"

namespace luthier
{
class LuthierAudioProcessor;
class LuthierAudioProcessorEditor;
class OverlayPanel;
}

namespace luthier::search
{

class CommandPalette;

//==============================================================================
/*  4.2 step 8: a mouse-transparent ring over the control, pulsing three times
    over 900 ms, or held still for 1500 ms under reduced motion. It follows the
    control by SafePointer, so a control deleted mid-flash ends the flash. */
class SearchHighlighter : public juce::Component,
                          private juce::Timer
{
public:
    SearchHighlighter();
    ~SearchHighlighter() override;

    /** Rings `target` (or `area` inside it, when given). */
    void flash (juce::Component* target, juce::Rectangle<int> area = {});
    void stop();

    bool isFlashing() const noexcept { return flashing; }
    juce::Component* getTarget() const noexcept { return target.getComponent(); }

    /** The ring, in this component's coordinates. */
    juce::Rectangle<int> getRingBounds() const;
    float getAlpha() const noexcept { return alpha; }

    /** Every alpha a frame was drawn with since the last flash (GS-37). */
    const std::vector<float>& getFrameAlphas() const noexcept { return frameAlphas; }

    /** Advances the animation by hand (tests, which have no timer thread). */
    void advance (double elapsedMs);

    static constexpr double kPulseMs = 900.0;
    static constexpr double kStaticMs = 1500.0;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    juce::Component::SafePointer<juce::Component> target;
    juce::Rectangle<int> targetArea;
    bool flashing = false, reduced = false;
    double elapsed = 0.0;
    double lastTickMs = 0.0;
    float alpha = 0.0f;
    std::vector<float> frameAlphas;
};

//==============================================================================
class SearchNavigator : public SearchServices,
                        public SearchContext,
                        private juce::Timer,
                        private juce::ChangeListener
{
public:
    SearchNavigator (LuthierAudioProcessorEditor& editor, LuthierAudioProcessor& processor);
    ~SearchNavigator() override;

    /** Tags the editor's surfaces, registers the commands and the default
        providers. Called once, at the end of the editor's constructor. */
    void initialise();

    SearchIndex& getIndex() noexcept             { return index; }
    ActionRegistry& getActions() noexcept        { return actions; }
    CommandPalette& getPalette() noexcept        { return *palette; }
    SearchHighlighter& getHighlighter() noexcept { return highlighter; }
    LuthierAudioProcessor& getProcessor() noexcept { return processor; }

    //==========================================================================
    // 6.1: opening and closing.
    void openPalette (const juce::String& initialText = {});
    void closePalette() override;
    void togglePalette();
    bool isPaletteOpen() const;

    /** The editor's layout pass. */
    void layout (juce::Rectangle<int> editorBounds, int headerHeight);

    //==========================================================================
    /** What an activation did, for the palette's row and footer. */
    struct Outcome
    {
        enum class Status { done, keptOpen, needsConfirm, refused, failed };

        Status status = Status::failed;
        juce::String message;       ///< footer / row subtitle
        bool warning = false;
        bool allowInline = false;   ///< modeUnavailable: the inline adjuster stays usable

        bool closesPalette() const noexcept { return status == Status::done; }
    };

    /** Enter on a row (4.2-4.3, 5). `confirmed` is the second Enter on a row
        that asked (auto-switch off, Slide Mode off, a missing part). */
    Outcome activate (const SearchItem& item, ActivationKind kind, bool confirmed, int secondaryIndex = -1);

    /** 4.4: applies a value reading, as a mouse edit. */
    Outcome applyValue (const ValueReading& reading);

    /** Alt+Left / Alt+Right: one arrow-key step; nudges within 200 ms merge
        into one gesture and so one undo entry. */
    bool nudge (const juce::String& parameterId, int direction, bool fine);

    /** Closes a nudge gesture now (tests; the 200 ms timer otherwise). */
    void endNudgeGesture();

    ValueReading readValue (const juce::String& rawQuery);

    /** The palette's value column for a parameter row. */
    juce::String valueTextFor (const SearchItem& item);

    /** "Opens in Advanced", "Needs Slide Mode", "Available in Luthier Pro"... */
    juce::String subtitleFor (const SearchItem& item, Availability availability);

    /** The empty state's suggestions (6.2): three from the view in use, then
        Load a preset, Open Workshop, Tuning and Show all shortcuts. */
    std::vector<const SearchItem*> getSuggestions();

    bool isLiveMode() const;

    //==========================================================================
    // The navigation itself (4.2).

    /** Goes to a parameter's control. */
    Outcome goToParameterControl (const SearchItem& item, bool confirmed);

    /** Opens a place (steps 5-9). */
    Outcome goToPlace (const SearchItem& item, bool confirmed);

    struct LastNavigation
    {
        juce::String itemId;
        bool ok = false;
        bool pending = false;
        juce::Component::SafePointer<juce::Component> focused;   ///< the inner control
        juce::Component::SafePointer<juce::Component> control;   ///< the LearnTarget
        juce::String message;
    };

    const LastNavigation& getLastNavigation() const noexcept { return last; }

    /** The control navigation would choose for a parameter right now, or
        nullptr (GS-02 checks it against the editor). */
    juce::Component* chooseControl (const juce::String& parameterId, bool& needsModeSwitch);

    /** Runs pending navigation retries now (tests pump this instead of waiting). */
    void pumpPending();

    //==========================================================================
    // Test and edition hooks.

    /** 10: which items are Pro-only in this build. Unset in a Pro build. */
    std::function<bool (const SearchItem&)> proLockPredicate;

    /** Replaces the MIDI OUT export's file chooser, so tests do not open one. */
    std::function<void()> onExportMidiPressed;

    /** Gives a component keyboard focus, and remembers that it was asked
        for: a window with no desktop peer (the tests, under xvfb) cannot take
        focus, so the request is what they check. */
    void requestFocus (juce::Component* c);
    juce::Component* getLastFocusRequest() const noexcept { return lastFocusRequest.getComponent(); }

    /** Every announcement the palette or navigator made (14, GS-36). */
    std::function<void (const juce::String&)> onAnnouncement;
    void announce (const juce::String& text);

    /** The overlays this editor owns, tagged "overlay:<name>" (GS-03). */
    std::vector<juce::Component*> getOwnedOverlays() const;

    /** The last notice search put in the window's notice strip. */
    const juce::String& getLastNotice() const noexcept { return lastNotice; }

    /** Commands with no key, run by LuthierAudioProcessorEditor::performAction. */
    bool performExtendedAction (const juce::String& actionId);

    /** The Easy strip a control sits in ("Tone"), or empty. */
    juce::String easyStripFor (const juce::Component& c) const;

    //==========================================================================
    // SearchServices
    void describeParameter (const juce::String& parameterId, juce::String& breadcrumb,
                            bool& inEasy, bool& inAdvanced) override;
    juce::uint32 getUiGeneration() override;
    bool isAdvancedModeAvailable() override;
    bool isAdvancedMode() override;
    bool isProLocked (const SearchItem&) override;
    bool goToParameter (const SearchItem& item, ActivationKind kind) override;
    bool openPlace (const SearchItem& item) override;
    bool openHelpTopic (const juce::String& topicId, bool pinnedInTab) override;
    bool openWorkshopOn (PartType type, const juce::String& partName) override;
    bool openShortcutRow (const juce::String& actionId) override;
    bool openSetting (const SearchItem& item) override;
    bool showPresetInBrowser (int presetIndex) override;
    std::vector<Setting> getSettings() override;

    // SearchContext
    bool openLocation (const UiLocation& location, const juce::String& announceAs) override;
    bool performAction (const juce::String& actionId) override;
    void showFooterMessage (const juce::String& text, bool warning) override;
    void postNotice (const juce::String& text) override;

private:
    void timerCallback() override;

    /** 12: a preset load (host, program change) re-runs the open palette's query. */
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void tagSurfaces();
    void registerActions();

    bool belongsToEditor (juce::Component& c) const;
    bool canShowPopovers() const;
    bool isWorkspacePanel (const juce::Component* c) const;
    bool isOnScreen (juce::Component& c) const;
    OverlayPanel* overlayContaining (juce::Component& c) const;
    OverlayPanel* overlayNamed (const juce::String& name) const;

    enum class Mode { easy, advanced, either };
    Mode modeOf (juce::Component& c) const;

    /** Switches mode by the 4.2 rules; false (with the message) if it may not. */
    bool switchModeFor (Mode wanted, const juce::String& name, bool confirmed, Outcome& outcome);

    bool runStep (const LocationStep& step);
    bool finishOnControl (juce::Component& control, const SearchItem& item);
    void scrollIntoView (juce::Component& c);
    void focusAndHighlight (juce::Component& control, juce::Rectangle<int> area = {});
    juce::String breadcrumbFor (juce::Component& control);
    void rebuildControlMap();

    juce::Component* findPracticeTab (const juce::String& label) const;
    juce::Component* workshopPanelShown() const;

    LuthierAudioProcessorEditor& editor;
    LuthierAudioProcessor& processor;

    SearchIndex index;
    ActionRegistry actions;
    SearchHighlighter highlighter;
    std::unique_ptr<CommandPalette> palette;

    LastNavigation last;
    juce::Component::SafePointer<juce::Component> lastFocusRequest;
    juce::String footerMessage;
    juce::String lastNotice;   ///< the last showFooterMessage, for activate()'s outcome

    /** A navigation waiting for a popover or a group to appear. */
    struct Pending
    {
        juce::String parameterId, itemId, name;
        int attempts = 0;
    };

    std::optional<Pending> pending;

    /** parameter id -> its controls, rebuilt when LiveControls changes. */
    std::map<juce::String, std::vector<juce::Component*>> controlMap;
    int controlMapChangeCount = -1;

    // Alt+Arrow nudges (4.4).
    juce::String nudgeParameter;
    double lastNudgeMs = 0.0;
    bool nudgeGestureOpen = false;
};

} // namespace luthier::search
