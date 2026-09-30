/*  Performance Assist's UI (auto-articulation.md 14, AA-37 - AA-41).

    Run under xvfb like EditorTests.cpp: the editor is built and laid out but
    never put on the desktop, so what is asked is state - parameter values, the
    undo stack, the labels the overlay would draw at a given time - rather than
    pixels. The clocks of the pill and the decision log are injected so "within
    two frames" and "gone by 700 ms" are exact.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/EasyPanel.h"
#include "../UI/RhythmPanel.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/FretboardComponent.h"
#include "../UI/GuitarBodyComponent.h"
#include "../UI/OptionsPages.h"
#include "../UI/PerformanceAssistUi.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

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

    struct Ui
    {
        std::unique_ptr<LuthierAudioProcessor> processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor;

        explicit Ui (bool advanced = false)
        {
            processor = std::make_unique<LuthierAudioProcessor>();
            processor->prepareToPlay (kSr, kBlock);
            processor->getUiState().advancedMode = advanced;
            editor.reset (processor->createEditor());
            editor->setSize (1400, 900);
        }

        ~Ui()
        {
            editor.reset();
        }

        LuthierAudioProcessorEditor& ed() { return *dynamic_cast<LuthierAudioProcessorEditor*> (editor.get()); }

        float plain (const char* id)
        {
            auto* p = processor->getState().getParameter (id);
            return p->convertFrom0to1 (p->getValue());
        }

        void set (const char* id, float plainValue)
        {
            auto* p = processor->getState().getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
            processor->getParameterBridge().applyAllNow();
        }

        /** Plays MIDI through the processor: the decisions reach the feed. */
        void play (std::vector<std::pair<int, juce::MidiMessage>> events, int blocks)
        {
            juce::AudioBuffer<float> buffer (2, kBlock);

            for (int b = 0; b < blocks; ++b)
            {
                juce::MidiBuffer midi;

                for (const auto& [at, m] : events)
                    if (at >= b * kBlock && at < (b + 1) * kBlock)
                        midi.addEvent (m, at - b * kBlock);

                buffer.clear();
                processor->processBlock (buffer, midi);
            }
        }
    };

    juce::MouseEvent mouseAt (juce::Component& c)
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        const auto now = juce::Time::getCurrentTime();
        const juce::Point<float> centre (c.getWidth() * 0.5f, c.getHeight() * 0.5f);

        return juce::MouseEvent (source, centre, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &c, &c, now, centre, now, 1, false);
    }

    /** A legato pair in Mono: C4, then D4 150 ms later with a 10 ms overlap. */
    std::vector<std::pair<int, juce::MidiMessage>> hammerOnPhrase()
    {
        auto s = [] (double ms) { return (int) (ms * 0.001 * kSr); };
        return { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) },
                 { s (150), juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100) },
                 { s (160), juce::MidiMessage::noteOff (1, 60) },
                 { s (400), juce::MidiMessage::noteOff (1, 62) } };
    }
}

//==============================================================================
// AA-37
LUTHIER_TEST (AutoArticulationUi, easyPillStyleAndPopover)
{
    Ui ui (false);
    auto* pill = findOne<AssistPill> (*ui.editor);
    auto* style = findOne<AssistStyleBox> (*ui.editor);

    CHECK (pill != nullptr && style != nullptr);

    if (pill == nullptr || style == nullptr)
        return;

    CHECK (pill->getWidth() == AssistPill::kWidth && pill->getHeight() == AssistPill::kHeight);
    CHECK (style->getWidth() == 70);

    double now = 1000.0;
    pill->setClockForTests ([&now] { return now; });

    // Tap toggles.
    CHECK (ui.plain (ParamIDs::aaEnabled) < 0.5f);
    pill->mouseDown (mouseAt (*pill));
    now += 80.0;
    pill->mouseUp (mouseAt (*pill));
    CHECK (ui.plain (ParamIDs::aaEnabled) > 0.5f);

    // The combo sets aa_style.
    style->getComboBox().setSelectedItemIndex ((int) AssistStyle::rock, juce::sendNotificationSync);
    CHECK ((int) std::lround (ui.plain (ParamIDs::aaStyle)) == (int) AssistStyle::rock);

    // Hold opens the popover and does not toggle.
    pill->mouseDown (mouseAt (*pill));
    now += AssistPill::kHoldMs + 10.0;
    pill->refresh();
    CHECK (pill->isPopoverOpen());
    pill->mouseUp (mouseAt (*pill));
    CHECK (ui.plain (ParamIDs::aaEnabled) > 0.5f);
    pill->closePopover();
    CHECK (! pill->isPopoverOpen());

    // Down opens it from the keyboard too.
    CHECK (pill->keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK (pill->isPopoverOpen());
    pill->closePopover();

    // The dot flashes within two UI frames of a decision.
    ui.set (ParamIDs::playingMode, (float) PlayingMode::Mono);
    ui.set (ParamIDs::aaEnabled, 1.0f);
    CHECK (! pill->isDotFlashing());
    ui.play (hammerOnPhrase(), 40);

    bool flashed = false;

    for (int frame = 0; frame < 2 && ! flashed; ++frame)
    {
        now += 34.0;
        AssistUi::drain (*ui.processor, true);
        pill->refresh();
        flashed = pill->isDotFlashing();
    }

    CHECK (flashed);
    now += AssistPill::kFlashMs + 1.0;
    CHECK (! pill->isDotFlashing());
}

// AA-38
LUTHIER_TEST (AutoArticulationUi, advancedPlayingGroupComesFirstAndWorks)
{
    Ui ui (true);
    if (auto* advanced = findOne<AdvancedPanel> (*ui.editor))
        advanced->setWorkspaceTabNamed ("RHYTHM");
    auto* rhythm = findOne<RhythmPanel> (*ui.editor);
    CHECK (rhythm != nullptr);

    if (rhythm == nullptr)
        return;

    rhythm->setSize (380, rhythm->preferredHeight());
    auto* group = rhythm->getPlayingGroup();
    CHECK (group != nullptr);

    if (group == nullptr)
        return;

    // First: nothing in the tab sits above it.
    for (auto* child : rhythm->getChildren())
        if (child != group && child->isVisible() && child->getHeight() > 0)
            CHECK_MSG (child->getY() >= group->getBottom(), "something sits above the PLAYING group");

    // Each rule switch flips exactly its bit.
    for (int bit = 0; bit < (int) AssistRule::numRules; ++bit)
    {
        const int before = (int) std::lround (ui.plain (ParamIDs::aaRules));
        auto& sw = group->getRuleSwitch (bit);
        sw.setToggleState (! sw.getToggleState(), juce::sendNotificationSync);
        const int after = (int) std::lround (ui.plain (ParamIDs::aaRules));
        CHECK_MSG ((before ^ after) == (1 << bit), "bit " + juce::String (bit) + ": " + juce::String (before) + " -> " + juce::String (after));
        sw.setToggleState (! sw.getToggleState(), juce::sendNotificationSync);
    }

    // The status line.
    ui.set (ParamIDs::aaEnabled, 1.0f);
    ui.set (ParamIDs::aaStyle, (float) AssistStyle::rock);
    ui.set (ParamIDs::aaAmount, 60.0f);
    group->refresh();
    CHECK_MSG (group->getStatusText() == juce::String (juce::CharPointer_UTF8 ("ON \xc2\xb7 Rock \xc2\xb7 60%")), group->getStatusText());

    // The notices.
    CHECK (group->getNoticeText().isEmpty());
    ui.set (ParamIDs::playingMode, (float) PlayingMode::GuitarController);
    group->refresh();
    CHECK (group->getNoticeText().contains ("Guitar Controller / MPE"));
    ui.set (ParamIDs::playingMode, (float) PlayingMode::Poly);
    ui.set (ParamIDs::mpeEnabled, 1.0f);
    group->refresh();
    CHECK (group->getNoticeText().contains ("Guitar Controller / MPE"));
    ui.set (ParamIDs::mpeEnabled, 0.0f);

    auto& engineRhythm = ui.processor->getEngine().getRhythmEngine();
    engineRhythm.setEnabled (true);
    engineRhythm.setFreeRun (true);
    ui.play ({ { 0, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 90) },
               { 0, juce::MidiMessage::noteOn (1, 52, (juce::uint8) 90) },
               { 0, juce::MidiMessage::noteOn (1, 55, (juce::uint8) 90) } }, 8);
    group->refresh();

    if (engineRhythm.isDriving())
        CHECK (group->getNoticeText().contains ("rhythm engine"));

    // Collapsing keeps the status line.
    const int open = group->preferredHeight();
    group->setCollapsed (true);
    CHECK (group->preferredHeight() < open);
    CHECK (group->getStatusText().isNotEmpty());
    CHECK (ui.processor->getUiState().playingGroupCollapsed);
    group->setCollapsed (false);
}

// AA-39
LUTHIER_TEST (AutoArticulationUi, labelsAppearFadeAndObeyTheOption)
{
    for (bool advanced : { true, false })
    {
        Ui ui (advanced);
        AssistLabelOverlay* overlay = nullptr;

        if (advanced)
        {
            if (auto* board = findOne<FretboardComponent> (*ui.editor))
                overlay = board->getAssistLabels();
        }
        else if (auto* guitar = findOne<GuitarBodyComponent> (*ui.editor))
        {
            overlay = guitar->getAssistLabels();
        }

        CHECK_MSG (overlay != nullptr, juce::String ("no overlay in ") + (advanced ? "Advanced" : "Easy"));

        if (overlay == nullptr)
            continue;

        double now = 5000.0;
        ui.processor->getAssistLog().setClockForTests ([&now] { return now; });
        AssistUi::setShowLabels (true);

        ui.set (ParamIDs::playingMode, (float) PlayingMode::Mono);
        ui.set (ParamIDs::aaEnabled, 1.0f);
        ui.set (ParamIDs::macroHumanize, 0.0f);
        ui.play (hammerOnPhrase(), 40);
        AssistUi::drain (*ui.processor, true);

        // Within two frames (it was drained this frame): an H at the note's string and fret.
        now += 33.0;
        const auto labels = overlay->computeLabels (now);
        const AssistLabelOverlay::Drawn* h = nullptr;

        for (const auto& l : labels)
            if (l.glyph == "H")
                h = &l;

        CHECK_MSG (h != nullptr, juce::String (labels.size()) + " labels, none an H");

        if (h != nullptr)
        {
            const int s = h->stringIndex;
            CHECK (s >= 0);
            CHECK (juce::roundToInt (h->fret) == juce::roundToInt (ui.processor->getEngine().getStringFret (s)) || h->fret > 0.0);
            CHECK (h->opacity > 0.0f && h->opacity <= 0.9f);
        }

        // Gone by 700 ms.
        CHECK (overlay->computeLabels (now + 700.0).empty());

        // Reduced motion: no intermediate opacities.
        overlay->setReducedMotionForTests (1);

        for (double t : { 50.0, 250.0, 450.0, 590.0 })
            for (const auto& l : overlay->computeLabels (now - 33.0 + t))
                CHECK_NEAR (l.opacity, 0.9, 1.0e-6);

        overlay->setReducedMotionForTests (-1);

        // With the option off nothing is drawn, and the list still fills.
        AssistUi::setShowLabels (false);
        CHECK (overlay->computeLabels (now).empty());
        CHECK (ui.processor->getAssistLog().getNumListed() > 0);
        AssistUi::setShowLabels (true);

        ui.processor->getAssistLog().setClockForTests ({});
    }

    // The switch lives in Options -> Appearance and writes the preference.
    Ui ui (false);
    AppearancePage page (*ui.processor);
    page.getAssistLabelsToggle().setToggleState (false, juce::sendNotificationSync);
    CHECK (! AssistUi::showLabels());
    page.getAssistLabelsToggle().setToggleState (true, juce::sendNotificationSync);
    CHECK (AssistUi::showLabels());
}

// AA-40
LUTHIER_TEST (AutoArticulationUi, keyboardAndScreenReader)
{
    Ui ui (true);
    if (auto* advanced = findOne<AdvancedPanel> (*ui.editor))
        advanced->setWorkspaceTabNamed ("RHYTHM");
    auto* rhythm = findOne<RhythmPanel> (*ui.editor);
    CHECK (rhythm != nullptr);

    if (rhythm == nullptr || rhythm->getPlayingGroup() == nullptr)
        return;

    auto& group = *rhythm->getPlayingGroup();
    const auto order = group.getFocusOrder();

    CHECK (order.size() == 4 + (size_t) AssistRule::numRules + 1);

    int last = 0;

    for (auto* c : order)
    {
        CHECK_MSG (c->getExplicitFocusOrder() > last, "focus order out of sequence");
        last = c->getExplicitFocusOrder();

        const auto label = c->getTitle().isNotEmpty() ? c->getTitle()
                         : c->getName().isNotEmpty() ? c->getName() : juce::String();
        const bool learnTarget = dynamic_cast<LearnTarget*> (c) != nullptr;
        CHECK_MSG (label.isNotEmpty() || learnTarget, "a control with no screen-reader label");
    }

    // Each rule switch is labelled with its name and a one-sentence description.
    for (int bit = 0; bit < (int) AssistRule::numRules; ++bit)
    {
        CHECK (group.getRuleSwitch (bit).getTitle() == AssistRule::getName (bit));
        CHECK (group.getRuleSwitch (bit).getDescription().endsWith ("."));
    }

    // A toggles the feature.
    const auto* binding = AccessibilitySettings::get().findShortcut ("toggleAssist");
    CHECK (binding != nullptr && binding->key == juce::KeyPress ('a', 0, 0));

    if (binding != nullptr)
    {
        CHECK (ui.ed().keyPressed (binding->key));
        CHECK (ui.plain (ParamIDs::aaEnabled) > 0.5f);
        CHECK (ui.ed().keyPressed (binding->key));
        CHECK (ui.plain (ParamIDs::aaEnabled) < 0.5f);
    }

    // The pill's announcement.
    Ui easy (false);
    easy.set (ParamIDs::aaEnabled, 1.0f);
    easy.set (ParamIDs::aaStyle, (float) AssistStyle::rock);
    CHECK (AssistUi::accessibleSummary (*easy.processor) == "Performance Assist, on, style Rock, amount 60 percent");

    // The list: empty text, then rows that read as specified.
    group.refresh();
    CHECK (group.getRowText (0) == AssistUi::kEmptyListText);

    AssistDecisionLog::Decision d;
    d.sample = (juce::int64) (12.4 * kSr);
    d.string = 2;
    d.fret = 7.0f;
    d.label = AssistLabel::hammerOn;
    CHECK (ui.processor->getAssistLog().describe (d, true) == "12.4 seconds, string 3 fret 7, hammer-on");
    CHECK (ui.processor->getAssistLog().describe (d, false) == "12.4s  str 3 fr 7  Hammer-on");
}

// AA-41
LUTHIER_TEST (AutoArticulationUi, undoEntries)
{
    Ui ui (true);
    auto& p = *ui.processor;

    auto steps = [&p] { return p.getNumUndoSteps(); };

    // The toggle: one entry, its description, undo restores.
    int before = steps();
    AssistUi::setEnabled (p, true);
    CHECK (steps() == before + 1);
    CHECK_MSG (p.getUndoDescription() == "Turn on Performance Assist", p.getUndoDescription());
    p.undo();
    CHECK (ui.plain (ParamIDs::aaEnabled) < 0.5f);

    // A rule switch.
    before = steps();
    AssistUi::setRule (p, 2, false);
    CHECK (steps() == before + 1);
    CHECK_MSG (p.getUndoDescription() == "Turn off Slides rule", p.getUndoDescription());
    p.undo();
    CHECK ((int) std::lround (ui.plain (ParamIDs::aaRules)) == AssistRule::all);

    // Style and Amount: one entry per gesture, as the attachments make them.
    for (const char* id : { ParamIDs::aaStyle, ParamIDs::aaAmount })
    {
        auto* prm = p.getState().getParameter (id);
        const float was = prm->getValue();
        before = steps();

        prm->beginChangeGesture();
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, was + 0.3f));
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, was + 0.4f));
        prm->endChangeGesture();

        CHECK_MSG (steps() == before + 1, juce::String (id) + ": " + juce::String (steps() - before) + " entries");
        p.undo();
        CHECK_NEAR (prm->getValue(), was, 1.0e-6);
    }

    // Played notes make none.
    AssistUi::setEnabled (p, true);
    before = steps();
    ui.play (hammerOnPhrase(), 40);
    CHECK (steps() == before);
}
