/*  SPEC-SWEEP polish rows: accessibility.md 1, 2, 4, 8 (A11Y-8, A11Y-10, A11Y-29,
    A11Y-40), qa-polish.md 4 and spec ui 105 (QA-40, SP-105) - overlays dismiss by
    every route and only one is up at a time.
    Groups: Fretboard, Overlay, Fonts, UiScale, ValidatorNotices. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/FretboardComponent.h"
#include "../UI/Overlays.h"
#include "../UI/OptionsPages.h"
#include "../Accessibility/Localisation.h"
#include "../UI/Theme.h"
#include "../UI/ValidatorNotices.h"
#include "../Validator.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    juce::MouseEvent clickAt (juce::Component& target, juce::Point<float> p)
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        return juce::MouseEvent (source, p, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, juce::Time::getCurrentTime(), p,
                                 juce::Time::getCurrentTime(), 1, false);
    }

    template <typename T>
    T* findChild (juce::Component& root)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                return match;

            if (auto* deeper = findChild<T> (*child))
                return deeper;
        }

        return nullptr;
    }

    struct PlainPanel : OverlayPanel
    {
        explicit PlainPanel (const juce::String& t) : OverlayPanel (t) {}
        juce::Point<int> getPreferredSize() const override { return { 300, 200 }; }
    };
}

//==============================================================================
/*  A11Y-8: the fretboard is a keyboard-operable grid that says where it is. */
LUTHIER_TEST (Fretboard, arrowKeysMoveTheCursorAndEachCellIsDescribed)
{
    LuthierAudioProcessor processor;
    FretboardComponent board (processor);
    board.setSize (900, 240);

    CHECK (board.getWantsKeyboardFocus());

    board.setCursor (2, 5);
    CHECK (board.getCursorString() == 2 && board.getCursorFret() == 5);

    const auto text = board.describeCell (2, 5);
    CHECK_MSG (text.startsWith ("String 3, fret 5, current note: "), text);

    CHECK (board.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK (board.getCursorString() == 3);
    CHECK (board.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK (board.getCursorFret() == 6);
    CHECK (board.keyPressed (juce::KeyPress (juce::KeyPress::leftKey, juce::ModifierKeys::ctrlModifier, 0)));
    CHECK (board.getCursorFret() == 1);

    // The ends clamp rather than wrap or crash.
    board.keyPressed (juce::KeyPress (juce::KeyPress::homeKey));
    CHECK (board.getCursorFret() == 0);
    board.keyPressed (juce::KeyPress (juce::KeyPress::leftKey));
    CHECK (board.getCursorFret() == 0);
    board.keyPressed (juce::KeyPress (juce::KeyPress::endKey));
    CHECK (board.getCursorFret() == board.getNumFrets());
    for (int i = 0; i < 20; ++i)
        board.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    CHECK (board.getCursorString() == 0);

    // An open string and the same note up the neck read differently by fret, not by note name.
    CHECK (board.describeCell (0, 0).contains ("fret 0"));
}

LUTHIER_TEST (Fretboard, everyCellNamesAKnownNoteAndTheHandlerIsAGroupWithAValue)
{
    LuthierAudioProcessor processor;
    FretboardComponent board (processor);
    board.setSize (900, 240);

    for (int s = 0; s < board.getNumStrings(); ++s)
        for (int f = 0; f <= board.getNumFrets(); ++f)
            CHECK_MSG (board.describeCell (s, f).contains ("current note: "), "empty description");

    auto handler = board.createAccessibilityHandler();
    CHECK (handler != nullptr);
    CHECK (handler->getRole() == juce::AccessibilityRole::group);
    CHECK (board.getTitle() == "Fretboard");
    CHECK (board.getDescription().isNotEmpty());
    CHECK (handler->getValueInterface() != nullptr);
    CHECK (handler->getValueInterface()->getCurrentValueAsString().startsWith ("String "));
}

LUTHIER_TEST (Fretboard, mKeyMutesAndAMutedStringRefusesToPlay)
{
    LuthierAudioProcessor processor;
    FretboardComponent board (processor);
    board.setSize (900, 240);
    board.setCursor (1, 3);

    CHECK (board.playCursor());
    CHECK (board.keyPressed (juce::KeyPress ('m', juce::ModifierKeys(), 'm')));
    CHECK (board.isStringMuted (1));
    CHECK (board.describeCell (1, 3).endsWith ("muted"));
    CHECK (! board.playCursor());
    CHECK (board.keyPressed (juce::KeyPress ('m', juce::ModifierKeys(), 'm')));
    CHECK (! board.isStringMuted (1));

    // A key the board does not own travels on to the editor.
    CHECK (! board.keyPressed (juce::KeyPress ('q', juce::ModifierKeys(), 'q')));
}

//==============================================================================
/*  A11Y-10 / QA-40: an overlay is announced by its own name, as a dialog. */
LUTHIER_TEST (Overlay, aPanelIsNamedAndIsADialogToAScreenReader)
{
    PlainPanel panel ("Preferences");
    CHECK (panel.getName() == "Preferences");
    CHECK (panel.getTitle() == "Preferences");

    auto handler = panel.createAccessibilityHandler();
    CHECK (handler != nullptr);
    CHECK (handler->getRole() == juce::AccessibilityRole::dialogWindow);
}

/*  SP-105 / QA-40: Escape, click-outside and the Close button each dismiss, and
    showing a second overlay removes the first. */
LUTHIER_TEST (Overlay, everyRouteDismissesAndOnlyOneIsUp)
{
    juce::Component root;
    root.setSize (900, 600);

    OverlayHost host;
    root.addChildComponent (host);
    host.setBounds (root.getLocalBounds());

    PlainPanel first ("First"), second ("Second");

    // Escape
    host.show (&first);
    CHECK (host.isShowingOverlay() && host.isVisible());
    CHECK (first.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (! host.isShowingOverlay() && ! host.isVisible());

    // Click outside the panel, on the scrim
    host.show (&first);
    CHECK (host.isShowingOverlay());
    host.mouseDown (clickAt (host, { 2.0f, 2.0f }));
    CHECK (! host.isShowingOverlay());

    // A click inside the panel does not dismiss it
    host.show (&first);
    const auto inside = first.getBounds().getCentre().toFloat();
    host.mouseDown (clickAt (host, inside));
    CHECK (host.isShowingOverlay());

    // The visible Close button
    auto* close = findChild<juce::TextButton> (first);
    CHECK (close != nullptr);

    if (close != nullptr)
    {
        CHECK (close->isVisible());
        close->onClick();
        CHECK (! host.isShowingOverlay());
    }

    // One at a time
    host.show (&first);
    host.show (&second);
    CHECK (host.getCurrentOverlay() == &second);
    CHECK (first.getParentComponent() == nullptr);
    CHECK (second.getParentComponent() == &host);
    host.dismiss();
    CHECK (! host.isShowingOverlay());
}

//==============================================================================
/*  A11Y-40: the font override reaches the fonts the UI actually draws with. */
LUTHIER_TEST (Fonts, theOverrideReplacesTheBundledFamilyAndClearsBack)
{
    auto& settings = AccessibilitySettings::get();
    const auto before = settings.getFontOverride();

    const auto bundled = Fonts::ui (13.0f).getTypefaceName();

    settings.setFontOverride ("Courier New");
    CHECK (Fonts::ui (13.0f).getTypefaceName() == "Courier New");
    CHECK (Fonts::ui (13.0f, true).getTypefaceName() == "Courier New");
    CHECK_NEAR (Fonts::ui (13.0f).getHeight(), 13.0f, 0.01f);

    settings.setFontOverride ({});
    CHECK (Fonts::ui (13.0f).getTypefaceName() == bundled);

    settings.setFontOverride (before);
}

//==============================================================================
/*  A11Y-29: a scale the screen cannot hold steps down to one that fits. */
LUTHIER_TEST (UiScale, aTooSmallScreenPicksTheLargestScaleThatFits)
{
    const int minW = 940, minH = 560;

    // A 1366x768 laptop cannot hold 940x560 at 150 % (1410 wide) or 200 %.
    const juce::Rectangle<int> laptop (0, 0, 1366, 738);
    CHECK_NEAR (AccessibilitySettings::largestScaleThatFits (2.0, laptop, minW, minH), 1.25, 1.0e-9);
    CHECK_NEAR (AccessibilitySettings::largestScaleThatFits (1.5, laptop, minW, minH), 1.25, 1.0e-9);

    // A request that already fits is left alone.
    CHECK_NEAR (AccessibilitySettings::largestScaleThatFits (1.0, laptop, minW, minH), 1.0, 1.0e-9);
    const juce::Rectangle<int> big (0, 0, 3840, 2160);
    CHECK_NEAR (AccessibilitySettings::largestScaleThatFits (2.0, big, minW, minH), 2.0, 1.0e-9);

    // No display known: trust the request.
    CHECK_NEAR (AccessibilitySettings::largestScaleThatFits (1.5, {}, minW, minH), 1.5, 1.0e-9);

    // Never below the smallest offered step, however small the screen.
    const auto smallest = AccessibilitySettings::kScales.front();
    CHECK_NEAR (AccessibilitySettings::largestScaleThatFits (2.0, { 0, 0, 200, 100 }, minW, minH), smallest, 1.0e-9);

    // Never above what was asked.
    CHECK (AccessibilitySettings::largestScaleThatFits (0.75, big, minW, minH) <= 0.75 + 1.0e-9);
}

//==============================================================================
/*  SP-114: every pickup off is said out loud, once per episode, and a blip is
    not news. */
LUTHIER_TEST (ValidatorNotices, allPickupsOffIsAnnouncedOncePerEpisode)
{
    Validator validator;
    ValidatorNotices notices;

    CHECK (notices.poll (validator, 0).empty());            // priming read

    validator.checkPickupOutput (false, 0.0, 0);
    CHECK (notices.poll (validator, 100).empty());           // one report: a blip while a guitar loads

    validator.checkPickupOutput (false, 0.0, 48000);
    const auto first = notices.poll (validator, 1100);       // a second of silence: real
    CHECK (first.size() == 1);

    if (first.size() == 1)
    {
        CHECK (first[0].id == ValidatorNotices::pickupsOffId);
        CHECK (first[0].message == ValidatorNotices::pickupsOffMessage());
        CHECK (first[0].level == Notification::Level::warning);
    }

    validator.checkPickupOutput (false, 0.0, 96000);
    CHECK (notices.poll (validator, 2100).empty());          // same episode: not said again

    CHECK (notices.poll (validator, 6000).empty());          // quiet gap: the episode ends

    validator.checkPickupOutput (false, 0.0, 144000);
    validator.checkPickupOutput (false, 0.0, 192000);
    CHECK (notices.poll (validator, 7000).size() == 1);      // a new episode is announced again
}

/*  SP-111: a corrected tension is said, not just clamped. */
LUTHIER_TEST (ValidatorNotices, aCorrectedTensionRaisesOneInfoNotice)
{
    Validator validator;
    ValidatorNotices notices;
    CHECK (notices.poll (validator, 0).empty());

    bool accepted = true;
    validator.checkTension (0, 100000.0, 650.0, 0, accepted);   // far beyond any string

    const auto out = notices.poll (validator, 500);
    CHECK (out.size() == 1);

    if (out.size() == 1)
    {
        CHECK (out[0].id == ValidatorNotices::tensionCorrectedId);
        CHECK (out[0].level == Notification::Level::info);
    }

    validator.checkTension (1, 100000.0, 650.0, 100, accepted);
    CHECK (notices.poll (validator, 900).empty());               // same episode

    // History from before the editor opened is not news.
    ValidatorNotices late;
    CHECK (late.poll (validator, 10000).empty());
}

//==============================================================================
/*  A11Y-41: numeric readouts are tabular - every digit the same width - whatever
    the locale, so a value changing does not shuffle the text beside it. */
LUTHIER_TEST (Fonts, numericReadoutsAreTabular)
{
    const auto mono = Fonts::mono (12.0f);
    const float one = mono.getStringWidthFloat ("1");

    for (const char* digit : { "0", "2", "3", "4", "5", "6", "7", "8", "9", "." })
        CHECK_NEAR (mono.getStringWidthFloat (digit), one, 0.01f);
}

/*  A11Y-46: Options > Localization - the language and its fallback are chosen
    there and reach the localisation state. */
LUTHIER_TEST (Localisation, theOptionsPageChoosesLocaleAndFallback)
{
    LuthierAudioProcessor processor;
    LocalizationPage page (processor);
    page.setSize (600, 300);

    juce::Array<juce::ComboBox*> boxes;

    for (auto* child : page.getChildren())
        if (auto* box = dynamic_cast<juce::ComboBox*> (child))
            boxes.add (box);

    CHECK (boxes.size() == 2);

    if (boxes.size() != 2)
        return;

    const auto& locales = Localisation::getShipLocales();
    CHECK (locales.size() >= 2);

    auto& loc = Localisation::get();
    const auto oldLocale = loc.getLocale();
    const auto oldFallback = loc.getFallbackLocale();

    // The boxes show the current state.
    CHECK (boxes[0]->getSelectedId() >= 1 && boxes[1]->getSelectedId() >= 1);

    boxes[1]->setSelectedId (2, juce::sendNotificationSync);
    CHECK (loc.getFallbackLocale() == locales[1].code);

    boxes[1]->setSelectedId (1, juce::sendNotificationSync);
    CHECK (loc.getFallbackLocale() == locales[0].code);

    boxes[0]->setSelectedId (1, juce::sendNotificationSync);
    CHECK (loc.getLocale() == locales[0].code);

    // Leave the machine as it was found.
    loc.setLocale (oldLocale);
    loc.setFallbackLocale (oldFallback);
    AccessibilitySettings::get().save();
}

//==============================================================================
/*  A11Y-47: a screen-reader smoke over the editor. Every button, combo box,
    slider and text field on screen in Easy and in Advanced has an accessible
    name (title, or a button's own text) and a role: what NVDA, VoiceOver and
    Orca read for it. */
namespace
{
    // isShowing() needs a desktop peer, which a headless editor has not got.
    bool visibleUnder (juce::Component& root, juce::Component& c)
    {
        for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        {
            if (! p->isVisible())
                return false;

            if (p == &root)
                return true;
        }

        return false;
    }

    struct A11yScan
    {
        int checked = 0;
        juce::StringArray unnamed;

        void walk (juce::Component& root, juce::Component& c, const juce::String& where)
        {
            for (auto* child : c.getChildren())
            {
                const bool interactive = dynamic_cast<juce::Button*> (child) != nullptr
                                         || dynamic_cast<juce::ComboBox*> (child) != nullptr
                                         || dynamic_cast<juce::Slider*> (child) != nullptr
                                         || dynamic_cast<juce::TextEditor*> (child) != nullptr;

                if (interactive && visibleUnder (root, *child) && child->getWidth() > 0 && child->getHeight() > 0
                    && child->isAccessible())
                {
                    ++checked;

                    // A fresh handler answers the same questions the platform would ask.
                    if (auto handler = child->createAccessibilityHandler())
                    {
                        if (handler->getTitle().isEmpty())
                            {
                            juce::String chain;
                            for (auto* p = child->getParentComponent(); p != nullptr && p != &root; p = p->getParentComponent())
                                chain << " < " << juce::String (typeid (*p).name()) << "(" << p->getName() << ")";

                            unnamed.add (where + ": " + juce::String (typeid (*child).name()) + " \"" + child->getName()
                                         + "\" at " + child->getBounds().toString() + chain);
                        }
                    }
                }

                walk (root, *child, where);
            }
        }
    };
}

LUTHIER_TEST (ScreenReader, everyInteractiveControlOnScreenHasAName)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr);

    if (editor == nullptr)
        return;

    editor->setVisible (true);
    editor->setSize (1600, 900);

    A11yScan easy;
    easy.walk (*editor, *editor, "Easy");

    editor->keyPressed (AccessibilitySettings::get().findShortcut ("toggleAdvanced")->key);
    A11yScan advanced;
    advanced.walk (*editor, *editor, "Advanced");

    CHECK_MSG (easy.checked > 10, "the scan found almost nothing in Easy");
    CHECK_MSG (advanced.checked > easy.checked, "Advanced should show more controls than Easy");

    juce::StringArray all;
    all.addArray (easy.unnamed);
    all.addArray (advanced.unnamed);
    all.removeDuplicates (false);

    CHECK_MSG (all.isEmpty(), juce::String (all.size()) + " unnamed control(s) of "
                                + juce::String (easy.checked + advanced.checked) + ":\n  "
                                + all.joinIntoString ("\n  "));
}
