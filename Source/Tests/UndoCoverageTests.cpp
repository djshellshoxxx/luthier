/*  action-and-undo.md 3 and 13: each action class makes its entry and the entry
    reverses it, driven through the real UI component where one exists.

    The components keep their controls private, so the controls are found by
    walking the component tree for their tooltip, text or type, and driven as a
    user would: a slider set with a synchronous notification, a button toggled
    and its onClick called, a combo box with a synchronous change.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/CharacterPanel.h"
#include "../UI/GuitarBodyComponent.h"
#include "../UI/LivePanel.h"
#include "../UI/ModMatrixPanel.h"
#include "../UI/OptionsPages.h"
#include "../UI/UiPreferences.h"
#include "../UI/UndoHistoryPanel.h"
#include "../UI/PracticePanel.h"
#include "../UI/RhythmPanel.h"
#include "../UI/RoutingPanel.h"
#include "../UI/ToneMatchPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    template <typename T>
    T* find (juce::Component& root, const std::function<bool (T&)>& matches)
    {
        if (auto* t = dynamic_cast<T*> (&root))
            if (matches (*t))
                return t;

        for (auto* child : root.getChildren())
            if (auto* found = find<T> (*child, matches))
                return found;

        return nullptr;
    }

    template <typename T>
    T* withTooltip (juce::Component& root, const juce::String& start)
    {
        return find<T> (root, [&start] (T& t) { return t.getTooltip().startsWith (start); });
    }

    void click (juce::Button& button)
    {
        if (button.getClickingTogglesState())
            button.setToggleState (! button.getToggleState(), juce::dontSendNotification);

        if (button.onClick != nullptr)
            button.onClick();
    }

    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> p)
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        return juce::MouseEvent (source, p, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, juce::Time::getCurrentTime(), p,
                                 juce::Time::getCurrentTime(), 1, false);
    }

    struct Fixture
    {
        LuthierAudioProcessor processor;
        double now = 100000.0;

        Fixture()
        {
            processor.prepareToPlay (kSr, kBlock);
            processor.setUndoClock ([this] { return now; });
        }

        int steps() const { return processor.getNumUndoSteps(); }
        void later() { now += 1000.0; }

        /*  Merge with SPEC-SWEEP (UW-2 / UW-5): an edit to audio-thread state
            (the capo parameter, a string's detune) lands at the top of the next
            block, so a check of the engine runs one block first. */
        void settle()
        {
            juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumInputChannels(),
                                                         processor.getTotalNumOutputChannels()), kBlock);
            buffer.clear();
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            processor.getParameterBridge().applyAllNow();   // the capo is structural (async)
        }
    };
}

//==============================================================================
LUTHIER_TEST (UndoCoverage, irSlotSettingsAreEntries)
{
    Fixture f;
    IrSlotEditor editor (f.processor, IrSlotEditor::Slot::cab1);

    auto* mix = withTooltip<juce::Slider> (editor, "How much of the impulse response");
    CHECK (mix != nullptr);

    if (mix == nullptr)
        return;

    auto& slot = f.processor.getCabIrSlot (0);
    const double before = slot.getMix();
    const int start = f.steps();

    mix->setValue (37.0, juce::sendNotificationSync);
    f.now += 50.0;
    mix->setValue (41.0, juce::sendNotificationSync);   // one drag, one entry

    CHECK_NEAR (slot.getMix(), 0.41, 1.0e-6);
    CHECK_MSG (f.steps() == start + 1, "the IR mix made " + juce::String (f.steps() - start) + " entries");

    f.processor.undo();
    CHECK_NEAR (slot.getMix(), before, 1.0e-6);
}

//==============================================================================
LUTHIER_TEST (UndoCoverage, routingGainAndMuteAreEntries)
{
    Fixture f;
    auto& routing = f.processor.getRouting();

    AuxStrip strip (f.processor, 2);
    strip.setSize (400, 30);

    auto* gain = find<juce::Slider> (strip, [] (juce::Slider&) { return true; });
    CHECK (gain != nullptr);

    if (gain == nullptr)
        return;

    const double before = routing.getAuxGainDb (2);
    gain->setValue (-9.0, juce::sendNotificationSync);
    CHECK_NEAR (routing.getAuxGainDb (2), -9.0, 1.0e-6);

    f.later();
    const bool wasMuted = routing.isAuxMuted (2);
    strip.mouseDown (mouseAt (strip, { 100.0f, 15.0f }));   // the M box
    CHECK (routing.isAuxMuted (2) != wasMuted);

    f.processor.undo();
    CHECK (routing.isAuxMuted (2) == wasMuted);

    f.processor.undo();
    CHECK_NEAR (routing.getAuxGainDb (2), before, 1.0e-6);
}

//==============================================================================
LUTHIER_TEST (UndoCoverage, rhythmSettingsAndPatternEditsAreEntries)
{
    Fixture f;
    RhythmPanel panel (f.processor);
    panel.setSize (900, 700);

    auto& rhythm = f.processor.getEngine().getRhythmEngine();

    // Free-run (a toggle, its own entry).
    auto* freeRun = withTooltip<juce::Button> (panel, "Free-run");
    CHECK (freeRun != nullptr);

    if (freeRun != nullptr)
    {
        const bool before = rhythm.isFreeRunning();
        click (*freeRun);
        CHECK (rhythm.isFreeRunning() != before);
        CHECK (f.processor.getUndoDescription() == "Toggle rhythm free-run");
        f.processor.undo();
        CHECK (rhythm.isFreeRunning() == before);
    }

    // Capo.
    f.later();
    auto* capoUp = withTooltip<juce::Button> (panel, "Move the capo up");
    CHECK (capoUp != nullptr);

    if (capoUp != nullptr)
    {
        const int before = rhythm.getCapoFret();
        click (*capoUp);
        f.settle();
        CHECK (rhythm.getCapoFret() == before + 1);
        f.processor.undo();
        f.settle();
        CHECK (rhythm.getCapoFret() == before);
    }

    // Humanise: a slider, grouped.
    f.later();
    auto* timing = withTooltip<juce::Slider> (panel, "Timing jitter");
    CHECK (timing != nullptr);

    if (timing != nullptr)
    {
        const double before = rhythm.getHumanise().timingMs;
        const int start = f.steps();
        timing->setValue (before + 5.0, juce::sendNotificationSync);
        f.now += 30.0;
        timing->setValue (before + 7.0, juce::sendNotificationSync);
        CHECK (f.steps() == start + 1);
        f.processor.undo();
        CHECK_NEAR (rhythm.getHumanise().timingMs, before, 1.0e-6);
    }

    // Swing (lives in the pattern).
    f.later();
    auto* swing = withTooltip<juce::Slider> (panel, "Swing");
    CHECK (swing != nullptr);

    if (swing != nullptr)
    {
        const double before = rhythm.getPattern().getSwing();
        swing->setValue (70.0, juce::sendNotificationSync);
        CHECK_NEAR (rhythm.getPattern().getSwing(), 0.70, 1.0e-6);
        f.processor.undo();
        CHECK_NEAR (rhythm.getPattern().getSwing(), before, 1.0e-6);
    }

    // A strum-grid click edits the pattern, one entry.
    f.later();
    StrumGrid grid (f.processor);
    grid.setSize (640, StrumGrid::preferredHeight);
    grid.refresh();

    const auto before = rhythm.getPattern().toVar();
    const int start = f.steps();

    for (int x = 4; x < 640 && f.steps() == start; x += 8)
        grid.mouseDown (mouseAt (grid, { (float) x, 10.0f }));

    if (rhythm.getPattern().getKind() == RhythmPattern::Kind::strum)
    {
        CHECK_MSG (f.steps() == start + 1, "a strum cell click made no entry");
        CHECK (f.processor.getUndoDescription() == "Edit strum pattern");
        f.processor.undo();
        CHECK (juce::JSON::toString (rhythm.getPattern().toVar()) == juce::JSON::toString (before));
    }
}

//==============================================================================
LUTHIER_TEST (UndoCoverage, headstockDetuneIsOneGroupedEntryPerString)
{
    Fixture f;
    TuningPopover popover (f.processor);

    auto* detune = find<juce::Slider> (popover, [] (juce::Slider& s) { return s.getTooltip().startsWith ("Deliberate detune"); });
    CHECK (detune != nullptr);

    if (detune == nullptr)
        return;

    auto& tuning = f.processor.getEngine().getTuningEngine();
    const int start = f.steps();

    detune->setValue (8.0, juce::sendNotificationSync);
    f.now += 40.0;
    detune->setValue (12.0, juce::sendNotificationSync);   // a wheel, no drag start

    CHECK_MSG (f.steps() == start + 1, "detune made " + juce::String (f.steps() - start) + " entries");
    f.settle();
    CHECK_NEAR (tuning.getStringTuning (0).detuneCents, 12.0, 1.0e-6);

    f.processor.undo();
    f.settle();
    CHECK_NEAR (tuning.getStringTuning (0).detuneCents, 0.0, 1.0e-6);
}

//==============================================================================
LUTHIER_TEST (UndoCoverage, characterEditsAreGroupedEntries)
{
    Fixture f;
    CharacterPanel panel (f.processor);
    panel.setSize (900, 700);

    auto& character = f.processor.getEngine().getCharacterEngine();
    auto* looseness = withTooltip<juce::Slider> (panel, "How badly the machine heads");
    CHECK (looseness != nullptr);

    if (looseness == nullptr)
        return;

    const double before = character.getTunerLooseness();
    const int start = f.steps();

    looseness->setValue (40.0, juce::sendNotificationSync);
    f.now += 20.0;
    looseness->setValue (60.0, juce::sendNotificationSync);

    CHECK (f.steps() == start + 1);
    CHECK (f.processor.getUndoDescription() == "Change tuner looseness");

    f.processor.undo();
    CHECK_NEAR (character.getTunerLooseness(), before, 1.0e-6);
}

//==============================================================================
LUTHIER_TEST (UndoCoverage, practiceScaleAndLooperLayerAreEntries)
{
    Fixture f;

    {
        ScaleTab tab (f.processor);
        auto* keyBox = find<juce::ComboBox> (tab, [] (juce::ComboBox& b)
                                             { return b.getNumItems() == 12 && b.getItemText (0) == "C"; });
        CHECK (keyBox != nullptr);

        if (keyBox != nullptr)
        {
            auto& trainer = f.processor.getScaleTrainer();
            const int before = trainer.getKey();
            keyBox->setSelectedId (3, juce::sendNotificationSync);
            CHECK (trainer.getKey() == 2);
            f.processor.undo();
            CHECK (trainer.getKey() == before);
            f.processor.redo();
            CHECK (trainer.getKey() == 2);
        }
    }

    f.later();

    {
        LooperTab tab (f.processor);
        auto* mute = find<juce::TextButton> (tab, [] (juce::TextButton& b) { return b.getButtonText() == "M"; });
        CHECK (mute != nullptr);

        if (mute != nullptr)
        {
            auto& layer = f.processor.getLooper().getLayer (0);
            const bool before = layer.isMuted();
            click (*mute);
            CHECK (layer.isMuted() != before);
            CHECK (f.processor.getUndoDescription().startsWith ("Change loop layer 1"));
            f.processor.undo();
            CHECK (layer.isMuted() == before);
        }
    }

    // The entries outlive the tabs: undo and redo after both are gone.
    f.processor.redo();
    CHECK (f.processor.getLooper().getLayer (0).isMuted());
}

//==============================================================================
LUTHIER_TEST (UndoCoverage, snapshotRenameColourAndDeleteAreEntries)
{
    Fixture f;
    auto& bank = f.processor.getSnapshots();

    f.processor.captureSnapshotAsUserAction (0, "Verse");
    f.later();

    f.processor.renameSnapshotAsUserAction (0, "Ch");
    f.now += 50.0;
    f.processor.renameSnapshotAsUserAction (0, "Chorus");   // typing groups
    CHECK (bank.getSnapshot (0).label == "Chorus");

    f.later();
    const int colour = bank.getSnapshot (0).colourTag;
    f.processor.setSnapshotColourAsUserAction (0, (colour + 1) % Snapshot::kNumColourTags);

    f.later();
    f.processor.deleteSnapshotAsUserAction (0, false);
    CHECK (bank.getSnapshot (0).isEmpty());

    f.processor.undo();
    CHECK (! bank.getSnapshot (0).isEmpty());

    f.processor.undo();
    CHECK (bank.getSnapshot (0).colourTag == colour);

    f.processor.undo();   // both renames at once
    CHECK (bank.getSnapshot (0).label == "Verse");

    // The Live panel's Clear button goes through the same entry.
    f.later();
    LivePanel panel (f.processor);
    panel.setSize (900, 700);

    if (auto* clear = withTooltip<juce::Button> (panel, "Empty the selected slot"))
    {
        const int start = f.steps();
        click (*clear);
        CHECK (f.steps() == start + 1);
        CHECK (f.processor.getUndoDescription().startsWith ("Delete snapshot"));
    }
    else
    {
        CHECK_MSG (false, "no Clear button on the Live panel");
    }
}

//==============================================================================
LUTHIER_TEST (UndoCoverage, modPanelRouteEditsAreEntries)
{
    Fixture f;
    auto& matrix = f.processor.getModMatrix();
    matrix.clearRoutes();

    ModRoute route;
    route.sourceId = modSourceIdForSlot (ModSourceSlots::lfoBase);
    route.destinationId = ParamIDs::masterGain;
    route.depth = 0.5f;
    matrix.addRoute (route);

    ModRouteTable table (f.processor);
    table.setSize (600, 300);
    table.refresh();

    const auto e = mouseAt (table, {});
    constexpr int curveColumn = 4, enabledColumn = 5, removeColumn = 6;

    table.cellClicked (0, enabledColumn, e);
    CHECK (! matrix.getRoute (0).enabled);

    f.later();
    const auto curve = matrix.getRoute (0).curve;
    table.refresh();
    table.cellClicked (0, curveColumn, e);
    CHECK (matrix.getRoute (0).curve != curve);

    f.later();
    table.refresh();
    table.cellClicked (0, removeColumn, e);
    CHECK (matrix.getNumRoutes() == 0);

    f.processor.undo();
    CHECK (matrix.getNumRoutes() == 1);

    f.processor.undo();
    CHECK (matrix.getRoute (0).curve == curve);

    f.processor.undo();
    CHECK (matrix.getRoute (0).enabled);
}

//==============================================================================
LUTHIER_TEST (UndoCoverage, setlistEditsAreEntries)
{
    Fixture f;
    LivePanel panel (f.processor);
    panel.setSize (900, 700);

    auto* add = withTooltip<juce::Button> (panel, "Put the selected snapshot at the end of the set");
    auto* remove = withTooltip<juce::Button> (panel, "Take the selected row out of the set");
    auto* list = find<juce::ListBox> (panel, [] (juce::ListBox&) { return true; });

    CHECK (add != nullptr && remove != nullptr && list != nullptr);

    if (add == nullptr || remove == nullptr || list == nullptr)
        return;

    auto count = [&f] { return f.processor.getSetlist().getSetlist().getNumEntries(); };

    click (*add);
    CHECK (count() == 1);
    CHECK (f.processor.getUndoDescription() == "Add setlist entry");

    f.later();
    list->updateContent();
    list->selectRow (0);
    click (*remove);
    CHECK (count() == 0);

    f.processor.undo();
    CHECK (count() == 1);

    f.processor.undo();
    CHECK (count() == 0);
}

//==============================================================================
/*  8: undoing a family switch warns that parts fitted since cannot be kept. */
LUTHIER_TEST (UndoCoverage, undoingAFamilySwitchWarns)
{
    Fixture f;

    auto* type = f.processor.getState().getParameter (ParamIDs::guitarType);
    type->setValueNotifyingHost (type->convertTo0to1 ((float) GuitarType::LesPaul));
    f.processor.getParameterBridge().applyAllNow();

    CHECK (f.processor.switchGuitarFamily ("bass"));
    f.processor.takeGuitarNotices();   // the switch's own banner

    f.processor.undo();
    CHECK (f.processor.getCurrentGuitar().family != "bass");
    CHECK (f.processor.takeGuitarNotices().contains (UndoHistory::kFamilySwitchUndoWarning));

    // A plain entry's undo says nothing.
    f.later();
    f.processor.pushUndoState ("edit");
    f.processor.undo();
    CHECK (f.processor.takeGuitarNotices().isEmpty());
}

//==============================================================================
/*  9: the history list's search filter, and a click on a filtered row. */
LUTHIER_TEST (UndoCoverage, theHistoryListSearchesAndUndoesToAChosenRow)
{
    Fixture f;
    const juce::String gain (ParamIDs::ampGain);

    auto set = [&f, &gain] (float plain)
    {
        auto* p = dynamic_cast<juce::AudioParameterFloat*> (f.processor.getState().getParameter (gain));
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };

    set (0.1f);
    f.processor.pushUndoState ("Change gain first");
    set (0.2f);
    f.processor.pushUndoBoundary ("Load preset Something");
    set (0.3f);
    f.processor.pushUndoState ("Change gain again");
    set (0.4f);

    UndoHistoryPanel panel (f.processor);
    panel.setSize (320, 360);
    CHECK (panel.getShownItems().size() == 3);

    panel.setSearchText ("change");
    CHECK (panel.getShownItems().size() == 2);
    CHECK (panel.getShownItems()[0].description == "Change gain again");

    panel.setSearchText ("PRESET");
    CHECK (panel.getShownItems().size() == 1 && panel.getShownItems()[0].boundary);

    panel.setSearchText ("first");
    bool chosen = false;
    panel.onChosen = [&chosen] { chosen = true; };
    panel.chooseRow (0);

    CHECK (chosen);
    CHECK (f.processor.getNumUndoSteps() == 0);
    auto* p = dynamic_cast<juce::AudioParameterFloat*> (f.processor.getState().getParameter (gain));
    CHECK_NEAR (p->get(), 0.1f, 1.0e-3);
}

//==============================================================================
/*  12: Options -> Diagnostics "Show undo depth", and the footer's text. */
LUTHIER_TEST (UndoCoverage, diagnosticsShowsUndoDepth)
{
    CHECK (UndoHistory::describeDepth (3, 1) == "Undo: 3 / 200; Redo: 1");

    auto& prefs = UiPreferences::get();
    const bool was = prefs.getBool (UndoHistory::kShowDepthPreference, false);

    Fixture f;
    DiagnosticsPage page (f.processor);
    page.setSize (700, 500);
    page.refresh();

    auto* toggle = find<juce::ToggleButton> (page, [] (juce::ToggleButton& b)
                                             { return b.getButtonText() == "Show undo depth in the footer"; });
    CHECK (toggle != nullptr);

    if (toggle != nullptr)
    {
        CHECK (toggle->getToggleState() == was);
        click (*toggle);
        CHECK (prefs.getBool (UndoHistory::kShowDepthPreference, was) != was);
    }

    prefs.setBool (UndoHistory::kShowDepthPreference, was);
}
