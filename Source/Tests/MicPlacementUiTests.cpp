/*  Continuous mic placement, the GUI (mic-placement.md 13: MP-26 to MP-32,
    MP-34 and MP-37), under xvfb in the style of EditorTests.cpp.

    Mouse gestures are synthesised as the component would receive them, so a
    drag runs the same code path - gesture, snap, undo entry - a hand does.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/Overlays.h"
#include "../UI/UiPreferences.h"
#include "../UI/MicPlacementEditor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"
#include "../Presets/MicPlacementMigration.h"
#include "../Support/IrLibrary.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    template <typename T>
    T* findOne (juce::Component& root)
    {
        for (auto* c : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (c))
                return match;

            if (auto* deeper = findOne<T> (*c))
                return deeper;
        }

        return nullptr;
    }

    juce::MouseEvent event (juce::Component& c, juce::Point<float> at, juce::ModifierKeys mods = {})
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        return juce::MouseEvent (source, at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c,
                                 juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false);
    }

    /** Visible all the way up to the editor, with a size: what a user sees.
        (isShowing() also wants a desktop window, which these tests do not open.) */
    bool onScreen (juce::Component& c)
    {
        if (c.getWidth() <= 0 || c.getHeight() <= 0)
            return false;

        for (auto* p = &c; p != nullptr; p = p->getParentComponent())
            if (! p->isVisible())
                return false;

        return true;
    }

    /** The Advanced panel reopens on the persisted last-used workspace tab, and WORKSHOP
        takes over the columns holding the cab section (so the mic view is zero-sized).
        The rig pins a tab that does not, and puts the user's setting back afterwards. */
    struct TabPreference
    {
        juce::String name = UiPreferences::get().getString (AdvancedPanel::workspaceTabNamePreferenceKey, {});
        int index = UiPreferences::get().getInt (AdvancedPanel::workspaceTabPreferenceKey, 0);

        ~TabPreference()
        {
            UiPreferences::get().setString (AdvancedPanel::workspaceTabNamePreferenceKey, name);
            UiPreferences::get().setInt (AdvancedPanel::workspaceTabPreferenceKey, index);
        }
    };

    struct Rig
    {
        TabPreference restoreTab;   // first member: restored after the editor is gone
        std::unique_ptr<LuthierAudioProcessor> processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor;

        Rig (bool advanced = true, int width = 1600, int height = 1000)
        {
            processor = std::make_unique<LuthierAudioProcessor>();
            processor->prepareToPlay (48000.0, 256);
            processor->getParameterBridge().applyAllNow();
            editor.reset (processor->createEditor());
            editor->setVisible (true);
            editor->setSize (width, height);

            if (advanced)
            {
                editor->keyPressed (AccessibilitySettings::get().findShortcut ("toggleAdvanced")->key);
                this->advanced().setWorkspaceTabNamed ("MOD");
            }
        }

        LuthierAudioProcessor& p() { return *processor; }
        AdvancedPanel& advanced() { return *findOne<AdvancedPanel> (*editor); }
        MicPlacementView& view() { return *advanced().getMicPlacementView(); }

        float get (const char* id) { return MicUi::get (p(), id); }
        void set (const char* id, float v) { MicUi::set (p(), id, v); }

        void setGuitar (const juce::String& contains)
        {
            auto* c = dynamic_cast<juce::AudioParameterChoice*> (p().getState().getParameter (ParamIDs::guitarType));

            for (int i = 0; i < c->choices.size(); ++i)
                if (c->choices[i].containsIgnoreCase (contains))
                {
                    c->setValueNotifyingHost (c->convertTo0to1 ((float) i));
                    break;
                }

            p().getParameterBridge().applyAllNow();
            view().resized();
        }

        /** Drags a handle so its centre lands on `target` in the face's pixels. */
        void drag (MicHandle& handle, juce::Point<float> target, juce::ModifierKeys mods = {}, std::function<void()> midway = {})
        {
            auto& face = *dynamic_cast<MicFace*> (handle.getParentComponent());
            const auto centreLocal = handle.getLocalBounds().toFloat().getCentre();
            handle.mouseDown (event (handle, centreLocal, mods));

            const auto startFace = face.handleCentre (handle.getMic());

            for (int step = 1; step <= 8; ++step)
            {
                const auto atFace = startFace + (target - startFace) * ((float) step / 8.0f);
                handle.mouseDrag (event (handle, atFace - handle.getPosition().toFloat(), mods));

                if (step == 4 && midway)
                    midway();
            }

            handle.mouseUp (event (handle, target - handle.getPosition().toFloat(), mods));
        }
    };

    double radiusOf (Rig& rig, int mic)
    {
        const auto& ids = MicPlacementMigration::idsFor (mic);
        return std::hypot ((double) rig.get (ids.x), (double) rig.get (ids.y));
    }
}

//==============================================================================
// MP-26
LUTHIER_TEST (MicPlacementUi, advancedCabSectionHasTheViewAndOneEntryDrags)
{
    Rig rig;
    auto& view = rig.view();

    CHECK (onScreen (view));
    CHECK (onScreen (view.getFace()));

    // The Quick choices are the legacy parameters' controls, attached.
    CHECK (view.getQuickPosition (0).getLearnParameterId() == ParamIDs::micPosition);
    CHECK (view.getQuickDistance (0).getLearnParameterId() == ParamIDs::micDistance);
    CHECK (view.getQuickPosition (1).getLearnParameterId() == ParamIDs::micPosition2);
    CHECK (view.getQuickDistance (1).getLearnParameterId() == ParamIDs::micDistance2);

    // A drag from the cap to the edge.
    rig.set (ParamIDs::micX, 0.0f);
    rig.set (ParamIDs::micY, 0.0f);
    view.getFace().refresh();

    const float x0 = rig.get (ParamIDs::micX), y0 = rig.get (ParamIDs::micY);
    const int stepsBefore = rig.p().getNumUndoSteps();
    auto& face = view.getFace();
    const auto centre = face.handleCentre (0);
    rig.drag (face.getHandle (0), centre + juce::Point<float> (face.ringRadiusPx (0.9) * 0.7071f, -face.ringRadiusPx (0.9) * 0.7071f));

    CHECK_MSG (rig.get (ParamIDs::micX) != x0 && rig.get (ParamIDs::micY) != y0, "the drag did not move both X and Y");
    CHECK_NEAR (radiusOf (rig, 0), 0.9, 0.03);
    CHECK_MSG (rig.p().getNumUndoSteps() == stepsBefore + 1,
               "a drag pushed " + juce::String (rig.p().getNumUndoSteps() - stepsBefore) + " undo entries");

    rig.p().undo();
    CHECK (rig.get (ParamIDs::micX) == x0);
    CHECK (rig.get (ParamIDs::micY) == y0);

    // A Quick pick writes the mapped placement in the same one entry.
    const int beforeQuick = rig.p().getNumUndoSteps();
    view.getQuickPosition (0).getComboBox().setSelectedItemIndex (3, juce::sendNotificationSync);   // Cone Edge
    CHECK_NEAR (rig.get (ParamIDs::micX), 0.9, 1.0e-4);
    CHECK (rig.p().getNumUndoSteps() == beforeQuick + 1);
    rig.p().undo();
    CHECK_NEAR (rig.get (ParamIDs::micX), x0, 1.0e-6);

    // A preset load that moves the Quick box remaps nothing.
    rig.set (ParamIDs::micX, -0.7f);
    rig.set (ParamIDs::micPosition, 0.0f);   // no gesture: a host or a load
    CHECK_NEAR (rig.get (ParamIDs::micX), -0.7, 1.0e-4);
}

//==============================================================================
// MP-27
LUTHIER_TEST (MicPlacementUi, keyboardMovesTheFocusedHandle)
{
    Rig rig;
    auto& handle = rig.view().getFace().getHandle (0);
    handle.grabKeyboardFocus();

    const float x0 = rig.get (ParamIDs::micX);

    for (int i = 0; i < 10; ++i)
        CHECK (handle.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));

    CHECK_NEAR (rig.get (ParamIDs::micX), x0 + 0.10, 1.0e-6);

    const float x1 = rig.get (ParamIDs::micX);
    CHECK (handle.keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys::shiftModifier, 0)));
    CHECK_NEAR (rig.get (ParamIDs::micX), x1 + 0.002, 1.0e-6);

    const float d0 = rig.get (ParamIDs::micDist);
    CHECK (handle.keyPressed (juce::KeyPress (juce::KeyPress::pageUpKey)));
    CHECK_NEAR (rig.get (ParamIDs::micDist), d0 + 1.0, 1.0e-4);

    const float a0 = rig.get (ParamIDs::micAngle);
    CHECK (handle.keyPressed (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::altModifier, 0)));
    CHECK_NEAR (rig.get (ParamIDs::micAngle), a0 + 1.0, 1.0e-4);

    // N snaps outward to the next ring.
    rig.set (ParamIDs::micX, 0.4f);
    rig.set (ParamIDs::micY, 0.0f);
    CHECK (handle.keyPressed (juce::KeyPress ('n', {}, 'n')));
    CHECK_NEAR (radiusOf (rig, 0), 0.62, 1.0e-5);
    CHECK (handle.keyPressed (juce::KeyPress ('n', juce::ModifierKeys::shiftModifier, 'N')));
    CHECK_NEAR (radiusOf (rig, 0), 0.35, 1.0e-5);

    // Home resets this mic.
    rig.set (ParamIDs::micDist, 40.0f);
    CHECK (handle.keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    CHECK_NEAR (rig.get (ParamIDs::micX), 0.35, 1.0e-5);
    CHECK_NEAR (rig.get (ParamIDs::micDist), 2.5, 1.0e-4);

    // Nudges within 200 ms are one undo entry.
    handle.focusLost (juce::Component::focusChangedDirectly);
    const int before = rig.p().getNumUndoSteps();

    for (int i = 0; i < 5; ++i)
        handle.keyPressed (juce::KeyPress (juce::KeyPress::leftKey));

    CHECK_MSG (rig.p().getNumUndoSteps() == before + 1,
               "five quick nudges made " + juce::String (rig.p().getNumUndoSteps() - before) + " entries");
}

//==============================================================================
// MP-28
LUTHIER_TEST (MicPlacementUi, handlesAreAccessibleGroupsOfFourSliders)
{
    Rig rig;
    auto& handle = rig.view().getFace().getHandle (0);

    // The handler a screen reader gets (built directly: the tests open no
    // desktop window for JUCE to hand one out through).
    auto a11y = handle.createAccessibilityHandler();
    CHECK (a11y != nullptr);

    if (a11y != nullptr)
    {
        CHECK (a11y->getRole() == juce::AccessibilityRole::group);
        CHECK (a11y->getTitle() == "Mic 1 placement");

        if (auto* value = a11y->getValueInterface())
            CHECK (value->getCurrentValueAsString() == "Mic 1, Classic Dynamic, Cap Edge, 2.5 centimetres, 0 degrees, speaker 1 of 4");
    }

    // Its accessible children: four sliders, bound to the placement.
    int sliders = 0;

    for (auto* child : handle.getChildren())
        if (dynamic_cast<juce::Slider*> (child) != nullptr && child->isVisible())
            ++sliders;

    CHECK_MSG (sliders == 4, "the handle has " + juce::String (sliders) + " slider children");

    const juce::StringArray titles { "Mic 1 X", "Mic 1 Y", "Mic 1 Distance", "Mic 1 Angle" };

    for (int i = 0; i < 4; ++i)
        CHECK (handle.getAccessibleSlider (i).getTitle() == titles[i]);

    // Tab order in the expanded editor: mic 1, mic 2, the side view, the cards.
    rig.advanced().openMicEditor();
    auto& ed = *rig.advanced().getMicPlacementEditor();
    CHECK (ed.getFront().getHandle (0).getExplicitFocusOrder() == 1);
    CHECK (ed.getFront().getHandle (1).getExplicitFocusOrder() == 2);
    CHECK (ed.getSideView().getExplicitFocusOrder() == 3);

    // Section 8's keys are the handle's while it has focus: none falls
    // through to a global shortcut.
    handle.grabKeyboardFocus();

    for (const auto& key : { juce::KeyPress (juce::KeyPress::leftKey), juce::KeyPress (juce::KeyPress::pageDownKey),
                              juce::KeyPress (juce::KeyPress::downKey, juce::ModifierKeys::altModifier, 0),
                              juce::KeyPress ('n', {}, 'n'), juce::KeyPress ('r', {}, 'r'),
                              juce::KeyPress (juce::KeyPress::homeKey) })
        CHECK_MSG (handle.keyPressed (key), "the handle let " + key.getTextDescription() + " through");
}

//==============================================================================
// MP-29
LUTHIER_TEST (MicPlacementUi, releaseNearARingSnapsExactly)
{
    Rig rig;
    auto& face = rig.view().getFace();
    auto& handle = face.getHandle (0);
    rig.set (ParamIDs::micX, 0.8f);
    rig.set (ParamIDs::micY, 0.0f);
    face.refresh();

    // Released 4 px outside the Cap Edge ring.
    const auto centre = face.handleCentre (0) - juce::Point<float> (face.ringRadiusPx (0.8), 0.0f);
    rig.drag (handle, centre + juce::Point<float> (face.ringRadiusPx (0.35) + 4.0f, 0.0f));
    CHECK_NEAR (radiusOf (rig, 0), 0.35, 1.0e-6);

    // With Alt held, it stays where it was put.
    rig.set (ParamIDs::micX, 0.8f);
    face.refresh();
    rig.drag (handle, centre + juce::Point<float> (face.ringRadiusPx (0.35) + 4.0f, 0.0f), juce::ModifierKeys::altModifier);
    CHECK (std::abs (radiusOf (rig, 0) - 0.35) > 1.0e-3);
}

//==============================================================================
// MP-30
LUTHIER_TEST (MicPlacementUi, easyPad)
{
    Rig rig (false);
    auto& easy = *findOne<EasyPanel> (*rig.editor);
    auto& pad = easy.getMicPad();

    CHECK (onScreen (pad));
    CHECK (pad.getWidth() <= MicPad::kWidth && pad.getHeight() <= MicPad::kHeight && pad.getHeight() >= 40);

    // Left to right raises u from 0 to 0.9, monotonically.
    double last = -1.0;

    for (int i = 0; i <= 10; ++i)
    {
        pad.setFromPad ({ (float) i / 10.0f, 0.2f });
        const double u = radiusOf (rig, 0);
        CHECK (u > last);
        last = u;
    }

    CHECK_NEAR (last, 0.9, 1.0e-4);

    // Down is 1 to 100 cm, log.
    pad.setFromPad ({ 0.5f, 0.0f });
    CHECK_NEAR (rig.get (ParamIDs::micDist), 1.0, 1.0e-3);
    pad.setFromPad ({ 0.5f, 0.5f });
    CHECK_NEAR (rig.get (ParamIDs::micDist), 10.0, 1.0e-2);
    pad.setFromPad ({ 0.5f, 1.0f });
    CHECK_NEAR (rig.get (ParamIDs::micDist), 100.0, 1.0e-2);

    // The mic 2 ghost with dual mic on.
    rig.set (ParamIDs::dualMic, 1.0f);
    CHECK (pad.isShowingGhost());
    rig.set (ParamIDs::dualMic, 0.0f);
    CHECK (! pad.isShowingGhost());

    // An acoustic: along, and the Pickup <-> Mic knob beside the pad.
    auto* c = dynamic_cast<juce::AudioParameterChoice*> (rig.p().getState().getParameter (ParamIDs::guitarType));

    for (int i = 0; i < c->choices.size(); ++i)
        if (c->choices[i].containsIgnoreCase ("dread"))
            c->setValueNotifyingHost (c->convertTo0to1 ((float) i));

    rig.p().getParameterBridge().applyAllNow();
    easy.resized();
    pad.setFromPad ({ 0.0f, 0.3f });
    CHECK_NEAR (rig.get (ParamIDs::acMicAlong), 4.0, 1.0e-4);
    pad.setFromPad ({ 1.0f, 0.3f });
    CHECK_NEAR (rig.get (ParamIDs::acMicAlong), 1.0, 1.0e-4);
    CHECK (onScreen (easy.getAcousticMicMixKnob()));

    // A double-click opens the editor as an overlay.
    pad.mouseDoubleClick (event (pad, { 10.0f, 10.0f }));
    auto* host = findOne<OverlayHost> (*rig.editor);
    CHECK (host != nullptr && host->isShowingOverlay()
           && dynamic_cast<MicPlacementOverlay*> (host->getCurrentOverlay()) != nullptr);
}

//==============================================================================
// MP-31
LUTHIER_TEST (MicPlacementUi, expandedEditorTakesOverColumnsThreeAndFour)
{
    Rig rig;
    auto& adv = rig.advanced();
    auto& view = rig.view();

    view.getExpandButton().onClick();

    CHECK (adv.isMicEditorShowing());
    auto* ed = adv.getMicPlacementEditor();
    CHECK (ed != nullptr);

    if (ed == nullptr)
        return;

    // Over the workspace, below the strip, which stays visible.
    const auto strip = adv.getWorkspaceTabStripBounds();
    CHECK (ed->getBounds().getY() >= strip.getBottom());
    CHECK (ed->getBounds().getRight() >= strip.getRight() - 2);
    CHECK_MSG (ed->getWidth() > strip.getWidth(), "the editor did not take Column 3 as well");

    // INTEGRATE-2: with JAM and RIFFS the strip scrolls (FEAT-RIFFS), so any one
    // tab may be scrolled aside; the selected tab is always whole on screen.
    if (auto* tab = adv.getWorkspaceTabButton (adv.getWorkspaceTabName (adv.getWorkspaceTab())))
        CHECK (onScreen (*tab));

    // Escape closes it and returns focus to the expand button.
    CHECK (ed->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (! adv.isMicEditorShowing());
    CHECK (view.getExpandButton().hasKeyboardFocus (false) || view.didRestoreFocusToExpand());
}

//==============================================================================
// MP-32
LUTHIER_TEST (MicPlacementUi, familyAndCabinetSwitchesLeaveNoStaleHandles)
{
    Rig rig;
    auto& view = rig.view();

    rig.setGuitar ("dread");
    CHECK (view.isShowingAcoustic());
    CHECK (view.getFace().getHandle (0).getAccessibleSlider (0).getTitle() == "Mic 1 Along");

    rig.setGuitar ("double-cut");
    CHECK (! view.isShowingAcoustic());
    CHECK (view.getFace().getHandle (0).getAccessibleSlider (0).getTitle() == "Mic 1 X");

    // A family switch mid-drag commits one entry.
    const int before = rig.p().getNumUndoSteps();
    auto& face = view.getFace();
    rig.drag (face.getHandle (0), face.handleCentre (0) + juce::Point<float> (20.0f, 0.0f), {},
              [&rig] { rig.setGuitar ("dread"); });
    CHECK_MSG (rig.p().getNumUndoSteps() == before + 1,
               "a drag across a family switch made " + juce::String (rig.p().getNumUndoSteps() - before) + " entries");
    rig.setGuitar ("double-cut");

    // 4x12 -> 1x12 with speaker 3: speaker 1 is drawn.
    rig.set (ParamIDs::cabType, (float) (int) CabinetType::Cab4x12);
    rig.set (ParamIDs::micSpeaker, 3.0f);
    rig.set (ParamIDs::cabType, (float) (int) CabinetType::Cab1x12Closed);
    face.refresh();
    const auto on1 = face.handleCentre (0);
    rig.set (ParamIDs::micSpeaker, 1.0f);
    face.refresh();
    CHECK (face.handleCentre (0) == on1);
    rig.set (ParamIDs::micSpeaker, 3.0f);
    CHECK (juce::roundToInt (rig.get (ParamIDs::micSpeaker)) == 3);   // the stored value is kept
    CHECK (MicUi::describe (rig.p(), 0).contains ("speaker 1 of 1"));
}

//==============================================================================
// MP-33's view half: a user IR hides that mic's handle and says why.
LUTHIER_TEST (MicPlacementUi, aUserIrHidesTheHandleAndSaysSo)
{
    Rig rig;
    auto& slot = rig.p().getCabIrSlot (0);
    const auto file = IrLibrary::findCabIr (CabinetConfig {});

    if (! file.existsAsFile() || ! slot.load (file))
        return;

    slot.setEngaged (true);
    rig.view().getFace().refresh();
    CHECK (! rig.view().getFace().getHandle (0).isVisible());
    CHECK (MicUi::statusMessages (rig.p()).contains ("Placement is baked into your IR."));
    slot.setEngaged (false);
}

//==============================================================================
// 6.2's chips: automation, and a mic in its null.
LUTHIER_TEST (MicPlacementUi, chipsSayWhoIsDrivingAndWhenAMicIsInItsNull)
{
    Rig rig;
    auto& view = rig.view();

    CHECK (! view.isDrivenByAutomation (0));
    rig.set (ParamIDs::micX, 0.5f);   // no gesture: a host lane
    CHECK (view.isDrivenByAutomation (0));

    // A gesture (a drag, a knob) is the user, not automation.
    {
        MicEdit edit (rig.p(), "drag", { ParamIDs::micX2 });
        edit.set (ParamIDs::micX2, 0.2f);
    }
    CHECK (! view.isDrivenByAutomation (1));

    rig.set (ParamIDs::micType, (float) (int) MicType::RibbonR121);
    rig.set (ParamIDs::micAngle, 88.0f);
    CHECK (MicUi::isInNull (rig.p(), 0));
    CHECK (MicUi::statusMessages (rig.p()).contains ("Mic in its null"));
    rig.set (ParamIDs::micAngle, 40.0f);
    CHECK (! MicUi::isInNull (rig.p(), 0));

    // 6.4's empty states.
    rig.set (ParamIDs::cabOn, 0.0f);
    CHECK (MicUi::statusMessages (rig.p()).contains ("Cabinet is off. Turn it on to place mics."));
    rig.set (ParamIDs::cabOn, 1.0f);
    rig.set (ParamIDs::cabType, (float) (int) CabinetType::AcousticDI);
    CHECK (MicUi::statusMessages (rig.p()).contains ("Acoustic DI has no speaker to mic. Pick a cabinet."));
}

//==============================================================================
// MP-34
LUTHIER_TEST (MicPlacementUi, reflowsAndRespectsReducedMotion)
{
    const auto fits = [] (juce::Component& parent) -> juce::String
    {
        for (auto* c : parent.getChildren())
            if (c->isVisible() && ! parent.getLocalBounds().contains (c->getBounds()))
                return c->getTitle().isNotEmpty() ? c->getTitle() : c->getName().isNotEmpty() ? c->getName()
                                                                                                : juce::String (typeid (*c).name());
        return {};
    };

    for (int width : { 1280, 1600, 2560 })
        for (float scale : { 1.0f, 1.5f, 2.0f })
        {
            // A scaled window is a smaller logical one.
            const int w = juce::roundToInt ((float) width / scale), h = juce::roundToInt ((float) width * 0.625f / scale);

            if (w < AdvancedPanel::minimumUsableWidth)
                continue;

            Rig rig (true, w, h);
            const auto label = juce::String (width) + " at " + juce::String (juce::roundToInt (scale * 100.0f)) + "%";
            auto clipped = fits (rig.view());
            CHECK_MSG (clipped.isEmpty(), "the view clips " + clipped + " at " + label);

            rig.advanced().openMicEditor();
            clipped = fits (*rig.advanced().getMicPlacementEditor());
            CHECK_MSG (clipped.isEmpty(), "the editor clips " + clipped + " at " + label);
        }

    // Snap eases: on normally, none under reduced motion.
    for (bool reduced : { false, true })
    {
        Rig rig;

        // After the editor exists: building it reads the saved settings.
        auto& settings = AccessibilitySettings::get();
        const bool was = settings.isReducedMotion();
        settings.setReducedMotion (reduced);

        auto& face = rig.view().getFace();
        rig.set (ParamIDs::micX, 0.8f);
        face.refresh();
        const auto centre = face.handleCentre (0) - juce::Point<float> (face.ringRadiusPx (0.8), 0.0f);
        rig.drag (face.getHandle (0), centre + juce::Point<float> (face.ringRadiusPx (0.62) + 3.0f, 0.0f));
        CHECK_MSG (face.getHandle (0).isAnimatingSnap() == ! reduced,
                   reduced ? "a snap animated under reduced motion" : "a snap did not ease");

        settings.setReducedMotion (was);
    }
}

//==============================================================================
// MP-37
LUTHIER_TEST (MicPlacementUi, everyStringIsInTheCatalogAndGeneric)
{
    auto& loc = Localisation::get();
    loc.setLocale ("en");
    int micKeys = 0;

    for (const auto& key : loc.getAllKeys())
    {
        if (! key.startsWith ("mic."))
            continue;

        ++micKeys;
        CHECK_MSG (loc.translate (key) != key, key + " does not resolve");
    }

    CHECK (micKeys >= 20);

    for (const char* key : { "mic.status.cabinetOff", "mic.status.acousticDi", "mic.status.acousticSilent",
                             "mic.status.bakedIr", "mic.chip.null", "mic.chip.automation", "mic.pad.label",
                             "mic.options.snap", "mic.options.plot", "mic.a11y.value" })
        CHECK_MSG (loc.hasKey (key), juce::String (key) + " is missing from the catalog");

    // The silhouettes are generic: no model's name is drawn or read out.
    for (int m = 0; m < (int) MicType::NumMics; ++m)
    {
        const juce::String name = CabinetEngine::getMicName ((MicType) m);

        for (const char* brand : { "SM57", "SM7B", "MD421", "U87", "R121", "C414", "D112", "Shure", "Neumann",
                                   "Royer", "AKG", "Sennheiser" })
            CHECK_MSG (! name.contains (brand), name + " names a brand");
    }
}
