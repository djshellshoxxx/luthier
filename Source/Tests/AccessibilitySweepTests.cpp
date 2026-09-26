/*  SPEC-SWEEP: accessibility.md 1-4 on the painted widgets, the knobs and the
    overlays (A11Y-7, A11Y-9, A11Y-10, A11Y-11, A11Y-13, A11Y-14, A11Y-48).
    Groups: ScreenReader, Keyboard, Editor. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/LiveStrip.h"
#include "../UI/Overlays.h"
#include "../UI/Widgets.h"

#include <set>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    template <typename T>
    void collectAll (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);

            collectAll<T> (*child, found);
        }
    }

    bool onScreen (juce::Component* c, juce::Component* root)
    {
        if (c->getWidth() <= 0 || c->getHeight() <= 0)
            return false;

        for (auto* p = c; p != nullptr; p = p->getParentComponent())
        {
            if (! p->isVisible())
                return false;

            if (p == root)
                return true;
        }

        return false;
    }
}

//==============================================================================
/*  A11Y-7: the output meter is a read-only value in dBFS. */
LUTHIER_TEST (ScreenReader, theMeterReportsItsPeakInDbfs)
{
    LuthierAudioProcessor processor;
    LevelMeter meter;
    meter.setSource (&processor);
    meter.setSize (20, 100);

    CHECK (meter.getTitle() == "Output level");

    auto* handler = meter.getAccessibilityHandler();
    CHECK (handler != nullptr);

    if (handler == nullptr)
        return;

    CHECK (handler->getRole() == juce::AccessibilityRole::progressBar);

    auto* value = handler->getValueInterface();
    CHECK (value != nullptr && value->isReadOnly());

    if (value != nullptr)
    {
        const auto text = value->getCurrentValueAsString();
        CHECK_MSG (text == "silent" || text.endsWith ("dBFS"), "the meter read \"" + text + "\"");
    }
}

/*  A11Y-9: the snapshot strip names each of its eight pads, and pressing one
    through the accessibility action recalls it. */
LUTHIER_TEST (ScreenReader, theSnapshotStripNamesEachSnapshot)
{
    LuthierAudioProcessor processor;
    processor.captureSnapshot (0, "Verse");
    processor.captureSnapshot (1, "Chorus");
    processor.getSnapshots().setCrossfadeMs (0.0);

    SnapshotStrip strip (processor);
    strip.setSize (800, 44);
    strip.refresh();

    for (int slot = 0; slot < SnapshotStrip::kButtonsShown; ++slot)
    {
        auto* accessor = strip.getSlotAccessor (slot);
        CHECK (accessor != nullptr);

        if (accessor == nullptr)
            continue;

        CHECK_MSG (accessor->getTitle().startsWith ("Snapshot " + juce::String (slot + 1)),
                   "slot " + juce::String (slot) + " is titled \"" + accessor->getTitle() + "\"");
        CHECK (accessor->getWantsKeyboardFocus());
        CHECK (accessor->getBounds().getWidth() > 0);
    }

    CHECK (strip.getSlotAccessor (1)->getTitle().contains ("Chorus"));
    CHECK (strip.getSlotAccessor (5)->getTitle().contains ("empty"));

    auto* handler = strip.getSlotAccessor (1)->getAccessibilityHandler();
    CHECK (handler != nullptr);

    if (handler != nullptr)
    {
        CHECK (handler->getActions().invoke (juce::AccessibilityActionType::press));
        processor.getSnapshots().advancePending();
        CHECK (processor.getSnapshots().getCurrentSnapshot() == 1);
    }
}

//==============================================================================
/*  A11Y-13 / A11Y-14: a knob takes focus, and the arrows step 1%, Shift 0.1%,
    Ctrl/Cmd 10%. */
LUTHIER_TEST (Accessibility, arrowKeysStepFineAndCoarse)
{
    LuthierAudioProcessor processor;

    LuthierKnob knob ("Gain");
    knob.attachTo (processor, ParamIDs::ampGain);
    knob.setSize (60, 80);

    auto& slider = knob.getSlider();
    CHECK (slider.getWantsKeyboardFocus());

    const auto range = slider.getRange();
    slider.setValue (range.getStart() + range.getLength() * 0.5, juce::sendNotificationSync);

    auto delta = [&slider] (const juce::KeyPress& key)
    {
        const double before = slider.getValue();
        CHECK (slider.keyPressed (key));
        return slider.getValue() - before;
    };

    const double normal = delta (juce::KeyPress (juce::KeyPress::upKey));
    const double fine   = delta (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0));
    const double coarse = delta (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::commandModifier, 0));

    CHECK_MSG (fine > 0.0 && normal > fine && coarse > normal,
               "fine " + juce::String (fine) + ", normal " + juce::String (normal)
                 + ", coarse " + juce::String (coarse));
    CHECK_NEAR (normal, range.getLength() * 0.01, range.getLength() * 0.002);

    CHECK (delta (juce::KeyPress (juce::KeyPress::downKey)) < 0.0);

    CHECK (slider.keyPressed (juce::KeyPress (juce::KeyPress::endKey)));
    CHECK_NEAR (slider.getValue(), range.getEnd(), 1.0e-6);

    // And the parameter followed.
    CHECK (processor.getState().getParameter (ParamIDs::ampGain)->getValue() > 0.99f);
}

/*  A11Y-48: every visible knob in the default view is on the Tab walk. */
LUTHIER_TEST (Keyboard, aTabWalkReachesEveryKnob)
{
    LuthierAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
        return;

    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    auto traverser = editor->createKeyboardFocusTraverser();
    CHECK (traverser != nullptr);

    if (traverser == nullptr)
        return;

    // Walk the traverser the way Tab does, container by container.
    std::set<juce::Component*> reachable;
    std::function<void (juce::Component*)> walk = [&] (juce::Component* container)
    {
        auto t = container->createKeyboardFocusTraverser();

        if (t == nullptr)
            return;

        for (auto* c : t->getAllComponents (container))
        {
            if (! reachable.insert (c).second)
                continue;

            if (c->isKeyboardFocusContainer())
                walk (c);
        }
    };

    walk (editor.get());

    juce::Array<LuthierKnob*> knobs;
    collectAll<LuthierKnob> (*editor, knobs);

    int visible = 0, missing = 0;
    juce::String firstMissing;

    for (auto* knob : knobs)
    {
        if (! onScreen (knob, editor.get()))
            continue;

        ++visible;

        if (reachable.count (&knob->getSlider()) == 0)
        {
            if (missing++ == 0)
                firstMissing = knob->getParameterId();
        }
    }

    CHECK_MSG (visible > 0, "no knobs on screen");
    CHECK_MSG (missing == 0, juce::String (missing) + " of " + juce::String (visible)
                               + " knobs are not on the Tab walk, first " + firstMissing);
}

//==============================================================================
/*  A11Y-10 / A11Y-11: an overlay takes focus on its first control and gives it
    back to its launcher on dismiss. Needs a window, so it runs on the desktop
    (xvfb in CI). */
LUTHIER_TEST (Editor, anOverlayFocusesItsFirstControlAndReturnsFocus)
{
    LuthierAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
        return;

    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);
    editor->addToDesktop (juce::ComponentPeer::windowIsTemporary);
    editor->setVisible (true);

    if (! editor->isShowing())
    {
        editor->removeFromDesktop();
        return;   // no display: nothing can hold focus
    }

    juce::Array<LuthierKnob*> knobs;
    collectAll<LuthierKnob> (*editor, knobs);

    juce::Slider* launcher = nullptr;
    for (auto* knob : knobs)
        if (knob->isShowing())
        {
            launcher = &knob->getSlider();
            break;
        }

    CHECK (launcher != nullptr);

    auto* host = [&]() -> OverlayHost*
    {
        juce::Array<OverlayHost*> hosts;
        collectAll<OverlayHost> (*editor, hosts);
        return hosts.getFirst();
    }();

    CHECK (host != nullptr);

    if (launcher != nullptr && host != nullptr)
    {
        launcher->grabKeyboardFocus();

        if (launcher->hasKeyboardFocus (false))
        {
            const auto* binding = AccessibilitySettings::get().findShortcut ("options");
            CHECK (binding != nullptr && editor->keyPressed (binding->key));
            CHECK (host->isShowingOverlay());

            auto* focused = juce::Component::getCurrentlyFocusedComponent();
            CHECK_MSG (focused != nullptr && host->isParentOf (focused),
                       "focus did not move into the overlay");
            CHECK (host->getLauncher() == launcher);

            host->dismiss();
            CHECK_MSG (launcher->hasKeyboardFocus (false), "focus did not return to the knob");
        }
    }

    editor->removeFromDesktop();
}

/*  A11Y-43: the verbosity setting gates what is spoken. */
LUTHIER_TEST (Accessibility, verbosityGatesAnnouncements)
{
    using V = AccessibilitySettings::Verbosity;
    using A = AccessibleSetup::Announcement;

    CHECK (AccessibleSetup::shouldAnnounce (A::error, V::minimal));
    CHECK (! AccessibleSetup::shouldAnnounce (A::standard, V::minimal));
    CHECK (! AccessibleSetup::shouldAnnounce (A::valueChange, V::minimal));

    CHECK (AccessibleSetup::shouldAnnounce (A::error, V::standard));
    CHECK (AccessibleSetup::shouldAnnounce (A::standard, V::standard));
    CHECK (! AccessibleSetup::shouldAnnounce (A::valueChange, V::standard));

    CHECK (AccessibleSetup::shouldAnnounce (A::error, V::verbose));
    CHECK (AccessibleSetup::shouldAnnounce (A::standard, V::verbose));
    CHECK (AccessibleSetup::shouldAnnounce (A::valueChange, V::verbose));
}
