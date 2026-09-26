/*  output-normalization.md 15, the GUI tests (xvfb): ON-34 Options, ON-35 the
    badge, ON-36 accessibility.

    The switch writes the user's choice to UiPreferences (section 6), which is
    a real file; every test here restores the keys it touches, so a later test
    (or the user's own plugin) does not start with normalization on.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/EasyPanel.h"
#include "../UI/HeaderBar.h"
#include "../UI/Notifications.h"
#include "../UI/NormalizationOptions.h"
#include "../UI/NormalizationBadge.h"
#include "../UI/OptionsPages.h"
#include "../UI/Overlays.h"
#include "../UI/UiPreferences.h"
#include "../UI/RoutingPanel.h"
#include "../UI/WorkshopPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** Saves and restores the normalization preference keys. */
    struct PreferenceGuard
    {
        bool enabled, suppressed;
        int target;

        PreferenceGuard()
        {
            auto& p = UiPreferences::get();
            enabled = p.getBool (OutputNormalization::kPrefDefaultEnabled, false);
            suppressed = p.getBool (OutputNormalization::kPrefBannerSuppressed, false);
            target = p.getInt (OutputNormalization::kPrefDefaultTarget, -18);
            p.setBool (OutputNormalization::kPrefDefaultEnabled, false);
            p.setBool (OutputNormalization::kPrefBannerSuppressed, false);
        }

        ~PreferenceGuard()
        {
            auto& p = UiPreferences::get();
            p.setBool (OutputNormalization::kPrefDefaultEnabled, enabled);
            p.setBool (OutputNormalization::kPrefBannerSuppressed, suppressed);
            p.setInt (OutputNormalization::kPrefDefaultTarget, target);
            p.save();
        }
    };

    template <typename T>
    void collect (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);

            collect<T> (*child, found);
        }
    }

    template <typename T>
    T* findOne (juce::Component& root)
    {
        juce::Array<T*> found;
        collect<T> (root, found);
        return found.isEmpty() ? nullptr : found.getFirst();
    }

    bool visibleUpTo (juce::Component* c, juce::Component* root)
    {
        for (; c != nullptr && c != root; c = c->getParentComponent())
            if (! c->isVisible())
                return false;

        return c == root;
    }

    /** A measurement, as the calibrator would hand it back. */
    void measured (OutputNormalization& n, double lufs)
    {
        NormalizationCalibrator::Measurement m;
        m.ok = true;
        m.measuredLufs = lufs;
        m.source = NormalizationCalibrator::Source::render;
        m.hash = juce::String::repeatedString ("0", 64);
        n.handleMeasurement (1, m);
    }

    /** A click, synchronously: triggerClick posts a message, and this runner
        has no dispatch loop to deliver it. */
    void click (juce::ToggleButton& b)
    {
        b.setToggleState (! b.getToggleState(), juce::dontSendNotification);

        if (b.onClick)
            b.onClick();
    }

    struct Editor
    {
        LuthierAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor;

        Editor (int width = 1280, int height = 800)
        {
            processor.prepareToPlay (48000.0, 256);
            editor.reset (processor.createEditor());
            editor->setSize (width, height);
        }

        LuthierAudioProcessorEditor& e() { return *dynamic_cast<LuthierAudioProcessorEditor*> (editor.get()); }
    };
}

//==============================================================================
LUTHIER_TEST (NormalizationGui, ON34_OptionsAudioPage)
{
    PreferenceGuard guard;
    Editor ed;
    auto& n = ed.processor.getOutputNormalization();

    ed.e().showOptionsPage ("AUDIO");
    auto* page = findOne<AudioPage> (*ed.editor);
    CHECK (page != nullptr);

    if (page == nullptr)
        return;

    auto& group = page->getNormalizationGroup();
    CHECK (group.getParentComponent() == page);
    CHECK (group.getSwitch().getButtonText() == "Normalize output loudness");

    // Off: caption hidden, target disabled, readout "Off".
    group.refresh();
    CHECK (! group.getSwitch().getToggleState());
    CHECK (! group.getCaption().isVisible());
    CHECK (! group.getTargetBox().isEnabled());
    CHECK (group.getReadout().getText() == "Off. Each sound plays at its natural level.");

    // On from the UI: the banner, with both actions.
    auto& centre = ed.e().getNotifications();
    click (group.getSwitch());

    CHECK (n.isEnabled());
    CHECK (centre.contains ("normalization.on"));

    if (centre.getCurrentId() == "normalization.on")
    {
        CHECK (centre.getCurrentMessage().startsWith ("Output normalization is on. Every sound is brought to the same loudness"));
        CHECK (centre.currentHasAction());
        CHECK (centre.currentHasSecondaryAction());
        CHECK (! centre.isAutoDismissScheduled());
    }

    group.refresh();
    CHECK (group.getCaption().isVisible());
    CHECK (group.getCaption().getText().startsWith ("! Evens out the natural level differences"));
    CHECK (group.getTargetBox().isEnabled());
    CHECK (group.getReadout().getText() == juce::String ("Measuring this sound..."));

    // The readout matches the status within 0.05 dB.
    measured (n, -25.51);
    group.refresh();
    const auto status = ed.processor.getNormalizationStatus();
    CHECK_NEAR (status.gainDb, 7.51, 0.05);
    CHECK (group.getReadout().getText() == "Normalization: +7.5 dB (this sound measures -25.5 LUFS)");

    // A target change from the box.
    group.getTargetBox().setSelectedItemIndex (0, juce::sendNotificationSync);
    CHECK (n.getTargetLufs() == -14.0);

    // "Don't show again" suppresses the next one.
    if (centre.getCurrentId() == "normalization.on")
        centre.performCurrentSecondaryAction();

    CHECK (UiPreferences::get().getBool (OutputNormalization::kPrefBannerSuppressed, false));
    CHECK (! centre.contains ("normalization.on"));

    click (group.getSwitch());   // off
    click (group.getSwitch());   // on again
    CHECK (n.isEnabled());
    CHECK (! centre.contains ("normalization.on"));

    // The preference remembers the last choice (6).
    CHECK (UiPreferences::get().getBool (OutputNormalization::kPrefDefaultEnabled, false));
    CHECK (UiPreferences::get().getInt (OutputNormalization::kPrefDefaultTarget, 0) == -14);

    // A session restored with it on posts no banner.
    UiPreferences::get().setBool (OutputNormalization::kPrefBannerSuppressed, false);
    centre.clear();
    juce::MemoryBlock state;
    ed.processor.getStateInformation (state);
    n.setEnabled (false);
    ed.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (n.isEnabled());
    CHECK (! centre.contains ("normalization.on"));

    n.setEnabled (false);
}

LUTHIER_TEST (NormalizationGui, ON35_Badge)
{
    PreferenceGuard guard;

    for (int width : { 1000, 1280, 1920 })
    {
        for (int advanced = 0; advanced < 2; ++advanced)
        {
            Editor ed (width, 800);
            if (advanced == 1)
                if (const auto* toggle = AccessibilitySettings::get().findShortcut ("toggleAdvanced"))
                    ed.editor->keyPressed (toggle->key);
            auto& n = ed.processor.getOutputNormalization();

            auto* header = findOne<HeaderBar> (*ed.editor);
            auto* easy = findOne<EasyPanel> (*ed.editor);
            CHECK (header != nullptr && easy != nullptr);

            if (header == nullptr || easy == nullptr)
                continue;

            auto& headerBadge = header->getNormalizationBadge();
            auto& easyBadge = easy->getNormalizationBadge();

            headerBadge.refresh();
            easyBadge.refresh();
            CHECK (! headerBadge.isVisible());
            CHECK (! easyBadge.isVisible());

            n.setEnabled (true);
            measured (n, -25.5);
            headerBadge.refresh();
            easyBadge.refresh();

            const juce::String where = juce::String (width) + (advanced ? " Advanced" : " Easy");
            CHECK_MSG (headerBadge.isVisible() && ! headerBadge.getBounds().isEmpty(), "header badge at " + where);
            CHECK_MSG (header->getLocalBounds().contains (headerBadge.getBounds()), "header badge outside the header at " + where);
            CHECK (headerBadge.getBadgeText() == "N +7.5");

            if (! advanced)
            {
                CHECK_MSG (visibleUpTo (&easyBadge, ed.editor.get()) && ! easyBadge.getBounds().isEmpty(), "Easy badge at " + where);
            }

            // Clicking it opens Options -> AUDIO with the switch focused.
            CHECK (headerBadge.onOpenOptions != nullptr);
            CHECK (headerBadge.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));   // Enter (async click)

            if (headerBadge.onOpenOptions)
                headerBadge.onOpenOptions();   // what the click runs

            auto* page = findOne<AudioPage> (*ed.editor);
            CHECK_MSG (page != nullptr && visibleUpTo (page, ed.editor.get()), "AUDIO page not showing after the badge at " + where);

            if (page != nullptr)
            {
                auto& sw = page->getNormalizationGroup().getSwitch();
                CHECK_MSG (sw.hasKeyboardFocus (false) || ! sw.isShowing(), "switch not focused at " + where);
            }

            n.setEnabled (false);
            headerBadge.refresh();
            CHECK (! headerBadge.isVisible());
        }
    }
}

LUTHIER_TEST (NormalizationGui, ON36_Accessibility)
{
    PreferenceGuard guard;
    Editor ed;
    auto& n = ed.processor.getOutputNormalization();
    ed.e().showOptionsPage ("AUDIO");
    auto* page = findOne<AudioPage> (*ed.editor);
    CHECK (page != nullptr);

    if (page == nullptr)
        return;

    auto& group = page->getNormalizationGroup();

    // Names and the description (9).
    CHECK (group.getSwitch().getTitle() == "Normalize output loudness");
    CHECK (group.getSwitch().getDescription().startsWith ("Evens out the natural level differences"));
    CHECK (group.getTargetBox().getTitle() == "Target loudness");
    CHECK (! group.getTargetBox().isEnabled());
    CHECK (group.getTargetBox().getDescription().isNotEmpty());   // says why it is disabled
    CHECK (group.getReadout().isAccessible());

    n.setEnabled (true);
    measured (n, -25.5);

    auto* header = findOne<HeaderBar> (*ed.editor);
    CHECK (header != nullptr);

    if (header != nullptr)
    {
        auto& badge = header->getNormalizationBadge();
        badge.refresh();
        CHECK (badge.getTitle() == "Output normalization, plus 7.5 decibels");
        CHECK (badge.getWantsKeyboardFocus());
        CHECK (badge.getTooltip() == "Output normalization +7.5 dB, target -18 LUFS. Click for options.");
    }

    // Tab order: the switch, then the target.
    {
        group.refresh();
        auto traverser = group.createFocusTraverser();
        const auto order = traverser->getAllComponents (&group);
        auto indexOf = [&order] (juce::Component* c) { const auto it = std::find (order.begin(), order.end(), c); return it == order.end() ? -1 : (int) (it - order.begin()); };
        const int sw = indexOf (&group.getSwitch()), box = indexOf (&group.getTargetBox());
        CHECK (sw >= 0 && box >= 0 && sw < box);
    }

    // Announcements: at most one every 2 s.
    {
        juce::uint32 now = 100000;
        group.clock = [&now] { return now; };
        const int before = group.getNumAnnouncements();

        measured (n, -20.0);                // +2.0: announced
        group.refresh();
        measured (n, -30.0); now += 1000;   // +12.0, 1 s later: held back
        group.refresh();
        measured (n, -25.5); now += 1500;   // +7.5, 2.5 s after the first: announced
        group.refresh();

        CHECK (group.getNumAnnouncements() - before == 2);
    }

    // The rebindable command toggles the switch and posts the banner.
    {
        auto& shortcuts = AccessibilitySettings::get();
        const auto* binding = shortcuts.findShortcut ("toggleNormalization");
        CHECK (binding != nullptr);
        CHECK (binding != nullptr && ! binding->key.isValid());   // unbound by default

        n.setEnabled (false);
        ed.e().getNotifications().clear();

        const juce::KeyPress key ('j', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, 0);
        CHECK (shortcuts.rebind ("toggleNormalization", key));
        CHECK (ed.editor->keyPressed (key));
        CHECK (n.isEnabled());
        CHECK (ed.e().getNotifications().contains ("normalization.on"));
        shortcuts.resetShortcut ("toggleNormalization");
    }

    n.setEnabled (false);
}

LUTHIER_TEST (NormalizationGui, CaptionsAndDiagnostics)
{
    PreferenceGuard guard;
    Editor ed;
    auto& n = ed.processor.getOutputNormalization();

    // 5.4: the ROUTING caption and the Workshop note follow the switch.
    RoutingPanel routingPanel (ed.processor);
    WorkshopPanel workshopPanel (ed.processor);
    auto& routing = routingPanel.getNormalizationCaption();
    auto& workshop = workshopPanel.getNormalizationNote();

    CHECK (! routing.isVisible());
    CHECK (! workshop.isVisible());

    n.setEnabled (true);
    routing.update();
    workshop.update();

    CHECK (routing.isVisible() && routing.getParentComponent() == &routingPanel);
    CHECK (workshop.isVisible() && workshop.getParentComponent() == &workshopPanel);
    CHECK (routing.getText() == "Output normalization applies to the main output only.");
    CHECK (workshop.getText().startsWith ("Normalization is on: level differences between parts are evened out."));

    // Diagnostics: the stage with its gain, the hash, the source, the GR.
    measured (n, -24.0);
    const auto lines = NormalizationUi::diagnosticsLines (ed.processor).joinIntoString ("\n");
    CHECK (lines.contains ("6.00 dB calibrated"));
    CHECK (lines.contains ("hash 0000000000000000"));
    CHECK (lines.contains ("source render"));
    CHECK (lines.contains ("true-peak gain reduction"));

    n.setEnabled (false);
}
