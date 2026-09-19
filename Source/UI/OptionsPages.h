#pragma once

/*  The Options pages the extension specs add
    (controllers 3 and 5, live-performance 8, accessibility 9, updates-telemetry 6).

    Each spec asks for its own "Options -> Something" tab, and there are six of
    them. Rather than six overlays, they are pages inside the one Options overlay,
    which is where a user already goes looking.

    Each page is a plain component with no knowledge of the others, so adding the
    next one is adding a file rather than editing a switch.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "../Controllers/ControllerProfile.h"
#include "../Updates/Telemetry.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** A page of the Options overlay. */
class OptionsPage : public juce::Component
{
public:
    explicit OptionsPage (LuthierAudioProcessor& p) : processor (p) {}

    /** Called when the page becomes visible, and periodically while it is. */
    virtual void refresh() {}

protected:
    LuthierAudioProcessor& processor;
};

//==============================================================================
/** Options -> Controllers (controllers.md sections 3 and 5). */
class ControllersPage final : public OptionsPage
{
public:
    explicit ControllersPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void applySelectedProfile();
    void runWizardStep();

    ControllerProfileLibrary library;
    LatencyWizard wizard;

    juce::ComboBox profileBox;
    juce::Label profileNotes, routingLabel;

    juce::Slider latencySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider deadZoneSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider minimumNoteSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    juce::ToggleButton guitarModeToggle { "LinnStrument guitar mode: rows map to strings" };

    // The latency wizard (controllers 3).
    juce::TextButton wizardButton { "Measure latency" };
    juce::Label wizardLabel;

    juce::TextButton saveProfileButton { "Save as my profile" };

    bool updatingControls = false;
};

//==============================================================================
/** Options -> Expression (live-performance.md section 8). */
class ExpressionPage final : public OptionsPage
{
public:
    explicit ExpressionPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void advanceWizard();

    juce::ComboBox ccBox, curveBox;
    juce::Label promptLabel, rangeLabel;

    juce::TextButton calibrateButton { "Calibrate" }, cancelButton { "Cancel" };

    juce::Slider heelDeadZone { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider toeDeadZone { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    juce::ListBox calibratedList;
    juce::Array<int> calibratedCcs;

    class ListModel : public juce::ListBoxModel
    {
    public:
        explicit ListModel (ExpressionPage& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;

    private:
        ExpressionPage& owner;
    };

    ListModel listModel { *this };

    bool updatingControls = false;
};

//==============================================================================
/** Options -> Accessibility and Options -> Localization (accessibility.md 9). */
class AccessibilityPage final : public OptionsPage
{
public:
    explicit AccessibilityPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuildShortcutList();

    juce::ComboBox verbosityBox, paletteBox, scaleBox, fontBox;
    juce::ToggleButton reducedMotionToggle { "Reduced motion" };

    juce::ComboBox localeBox, fallbackBox;
    juce::TextButton catalogButton { "Custom catalog..." };
    juce::Label contrastLabel, localeNote;

    // The rebind table (accessibility 2).
    juce::ListBox shortcutList;
    juce::TextEditor searchBox;
    juce::TextButton resetAllButton { "Reset all shortcuts" };
    juce::Label rebindHint;

    juce::Array<int> visibleShortcuts;

    class ShortcutModel : public juce::ListBoxModel
    {
    public:
        explicit ShortcutModel (AccessibilityPage& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    private:
        AccessibilityPage& owner;
    };

    ShortcutModel shortcutModel { *this };

    /** Which row is waiting for a key, or -1. */
    int capturingRow = -1;

    std::unique_ptr<juce::FileChooser> chooser;

    bool updatingControls = false;

    /** Catches the key press for a rebind. */
    bool keyPressed (const juce::KeyPress& key) override;
};

//==============================================================================
/** Options -> Updates and Options -> Privacy (updates-telemetry.md 1 and 6). */
class PrivacyPage final : public OptionsPage
{
public:
    explicit PrivacyPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Telemetry& telemetry();

    void checkForUpdate();

    // Updates.
    juce::ToggleButton updateCheckToggle { "Check for updates automatically" };
    juce::ToggleButton betaToggle { "Include beta releases" };
    juce::TextButton checkNowButton { "Check now" };
    juce::Label updateStatus;

    // Telemetry, each explained in plain English as the spec asks.
    juce::ToggleButton usageToggle { "Usage telemetry" };
    juce::ToggleButton diagnosticsToggle { "Diagnostics telemetry" };
    juce::ToggleButton crashToggle { "Crash reports" };

    juce::Label usageExplanation, diagnosticsExplanation, crashExplanation;

    // The log, and the buttons that act on it.
    juce::TextButton viewLogButton { "View last upload" };
    juce::TextButton clearLogsButton { "Clear all local logs" };
    juce::TextButton paranoiaButton { "Turn everything off and delete all diagnostic files" };

    juce::TextEditor logView;

    // Endpoints, for an enterprise proxy.
    juce::TextEditor manifestUrlBox, telemetryUrlBox, crashUrlBox;
    juce::Label endpointsHeading, policyLabel;

    bool updatingControls = false;
};

} // namespace luthier
