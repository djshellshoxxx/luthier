/*  The editor's half of onboarding.md 2-4 (TUNE-HELP-ONBOARDING workstream):
    the welcome banner, the tour's targets, and the first-week hints' targets.
    Kept out of PluginEditor.cpp so the hub file carries only the calls. The
    pieces themselves are in UI/Onboarding.h and know nothing of the editor.
*/

#include "PluginEditor.h"
#include "Tune/TuneExamples.h"

namespace luthier
{

void LuthierAudioProcessorEditor::setupOnboarding()
{
    // --- the welcome banner (2, 13) -----------------------------------------------------
    addChildComponent (welcomeBanner);
    welcomeBanner.onVisibilityChanged = [this] { resized(); };
    welcomeBanner.onStartTour = [this] { startTour(); };
    welcomeBanner.onWhatsNew = [this] { openHelp ("whats-new"); };

    // --- Help -> Take the tour (2) ------------------------------------------------------
    helpPanel.getView().onTakeTour = [this]
    {
        overlayHost.dismiss();
        startTour();
    };

    if (auto* helpTab = advancedPanel.getHelpTab())
        helpTab->onTakeTour = [this] { startTour(); };

    // gui-integration 20: the Easy strips' ? icons.
    easyPanel.onOpenHelp = [this] (const juce::String& topic) { openHelp (topic); };

    // --- the tour (3) --------------------------------------------------------------------
    addChildComponent (tour);
    tour.findTarget = [this] (const juce::String& id) { return findTourTarget (id); };
    tour.prepareStep = [this] (const juce::String& id) { prepareTourStep (id); };
    tour.onEnded = [this] (bool) { grabKeyboardFocus(); };

    // --- the first-week hints (4), under the overlays -------------------------------------
    addAndMakeVisible (discovery, getIndexOfChildComponent (&overlayHost));

    using Target = DiscoveryLayer::Target;

    discovery.addTargets (DiscoveryLayer::Kind::pulse, [this]
    {
        std::vector<Target> targets
        {
            { "workshop_wrench", header.getTourTarget ("workshop") },   // "Workshop wrench pulses on first sight."
            { "slide_glyph",     header.getTourTarget ("slide") },      // "Slide glyph pulses on first sight."
            { "tune_tab",        advancedPanel.getWorkspaceTabButton ("TUNE") }   // "TUNE tab pulses on first sight."
        };

        // "Every panel with a ? icon shows a subtle pulse on first sight."
        for (auto* help : advancedPanel.getHelpButtons())
            targets.push_back ({ help->getDiscoveryKey(), help });

        for (auto* help : easyPanel.getHelpButtons())
            targets.push_back ({ help->getDiscoveryKey(), help });

        return targets;
    });

    // "Every unused Column 4 tab shows a soft accent dot until first opened."
    discovery.addTargets (DiscoveryLayer::Kind::tabDot, [this]
    {
        std::vector<Target> targets;

        for (int i = 0; i < advancedPanel.getNumWorkspaceTabs(); ++i)
            if (i != advancedPanel.getWorkspaceTab())
            {
                const auto name = advancedPanel.getWorkspaceTabName (i);
                targets.push_back ({ name, advancedPanel.getWorkspaceTabButton (name) });
            }

        return targets;
    });

    // gui-integration 20 / onboarding 13: NEW on the entry point of a feature the
    // running version introduced. "tab_<NAME>" is a workspace tab, "header_<id>" a
    // header control.
    discovery.addTargets (DiscoveryLayer::Kind::newDot, [this]
    {
        std::vector<Target> targets;

        for (const auto& feature : Onboarding::getNewFeatures())
        {
            const juce::String key (feature.key);
            juce::Component* c = nullptr;

            if (key.startsWith ("tab_"))
                c = advancedPanel.getWorkspaceTabButton (key.fromFirstOccurrenceOf ("tab_", false, false).replaceCharacter ('_', ' '));
            else if (key.startsWith ("header_"))
                c = header.getTourTarget (key.fromFirstOccurrenceOf ("header_", false, false));

            targets.push_back ({ key, c });
        }

        return targets;
    });

    // "The randomize dice shows a tooltip on first Randomize hover."
    randomiseTooltip.attachTo (&easyPanel.getRandomiseButton());

    if (Onboarding::isAutomatic())
        runWelcome();
}

void LuthierAudioProcessorEditor::runWelcome()
{
    const auto version = Onboarding::getCurrentVersion();
    Onboarding::recordLaunch (version);

    // One banner per window: a second editor in the same process shows nothing.
    const auto due = Onboarding::getWelcomeDue (version);

    if (due == Onboarding::Welcome::none || welcomeBanner.isVisible())
        return;

    welcomeBanner.showFor (due, version);
    Onboarding::noteWelcomeShown (due, version);
}

void LuthierAudioProcessorEditor::startTour()
{
    // The optional TECHNIQUES stop joins when that tab is in the build.
    const bool techniques = advancedPanel.getWorkspaceTabButton ("TECHNIQUES") != nullptr;

    tour.setBounds (getLocalBounds());
    tour.start (Onboarding::getTourSteps (techniques));

    if (isShowing())
        tour.grabKeyboardFocus();
}

juce::Rectangle<int> LuthierAudioProcessorEditor::findTourTarget (const juce::String& id)
{
    auto areaOf = [this] (juce::Component* c) -> juce::Rectangle<int>
    {
        if (c == nullptr || ! Onboarding::isVisibleWithin (*c, this))
            return {};

        return getLocalArea (c, c->getLocalBounds());
    };

    if (auto* inHeader = header.getTourTarget (id))
        return areaOf (inHeader);

    if (id == "guitar")
        return advancedMode ? areaOf (advancedPanel.getColumnViewport (0)) : areaOf (&easyPanel.getGuitar());

    if (id == "rig")
    {
        if (! advancedMode)
            return getLocalArea (&easyPanel, easyPanel.getRigArea());

        auto* amp = advancedPanel.getColumnViewport (2);
        const auto area = areaOf (amp);
        return ! area.isEmpty() ? area : areaOf (advancedPanel.getColumnViewport (1));
    }

    if (id == "workspace")
        return advancedMode ? getLocalArea (&advancedPanel, advancedPanel.getWorkspaceTabStripBounds()) : juce::Rectangle<int>();

    if (id == "tune")
        return advancedMode ? areaOf (advancedPanel.getWorkspaceTabButton ("TUNE")) : juce::Rectangle<int>();

    if (id == "techniques")
        return advancedMode ? areaOf (advancedPanel.getWorkspaceTabButton ("TECHNIQUES")) : juce::Rectangle<int>();

    if (id == "snapshots")
        return areaOf (Onboarding::findComponent (*this, [this] (juce::Component& c)
                       {
                           return (dynamic_cast<SnapshotStrip*> (&c) != nullptr || dynamic_cast<SnapshotGrid*> (&c) != nullptr)
                                    && Onboarding::isVisibleWithin (c, this);
                       }));

    if (id == "practice")
        return areaOf (Onboarding::findButton (practicePanel, "PRACTICE"));

    return {};
}

void LuthierAudioProcessorEditor::prepareTourStep (const juce::String& id)
{
    // The stops after "Switch to Advanced" are about Advanced's columns and tabs.
    static const juce::StringArray advancedStops { "guitar", "rig", "workspace", "techniques", "tune", "snapshots" };

    if (advancedStops.contains (id) && ! advancedMode && isAdvancedModeAvailable())
    {
        setAdvancedMode (true);
        header.setAdvancedMode (advancedMode);
    }

    // The Workshop takes over columns 3 and 4, which hides the rig: show it.
    if ((id == "rig" || id == "workspace") && advancedMode && advancedPanel.isWorkshopShowing())
        advancedPanel.setWorkspaceTabNamed ("TUNE");

    // "Save a moment, recall with one press": the snapshot strip is on the LIVE tab.
    if (id == "snapshots" && advancedMode)
        advancedPanel.setWorkspaceTabNamed ("LIVE");
}

} // namespace luthier

namespace luthier
{

void LuthierAudioProcessorEditor::applyFirstRunPreset()
{
    auto& presets = processor.getPresetManager();
    const int index = presets.indexOfPreset (Onboarding::kFirstRunPreset);

    if (index < 0)
        return;

    presets.loadPreset (index);
    processor.getParameterBridge().applyAllNow();

    // onboarding 6: the example setlists, over this machine's factory bank.
    TuneExamples::installExampleSetlists (presets);
}

} // namespace luthier
