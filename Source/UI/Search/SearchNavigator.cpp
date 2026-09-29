#include "SearchNavigator.h"
#include "CommandPalette.h"
#include "SearchCatalog.h"
#include "LiveControls.h"
#include "SearchOptionsGroup.h"

#include "../../PluginEditor.h"
#include "../../Parameters.h"
#include "../../Support/ErrorLog.h"
#include "../../Practice/PracticeRoutine.h"
#include "../SetupGroup.h"
#include "../MicPlacementView.h"   // mic-placement.md 8 (INTEGRATE-2)
#include "../SlideGroup.h"
#include "../SlapGroup.h"
#include "../StrumGroup.h"
#include "../BassGridGroup.h"
#include "../NoiseGroups.h"
#include "../RealismGroups.h"
#include "../RealismGroupsC.h"
#include "../HarmonicsGroup.h"
#include "../RightHandGroup.h"
#include "../StringInteractionGroup.h"
#include "../MidiOutPanel.h"
#include "../HelpContent.h"

namespace luthier::search
{
namespace
{
    const juce::String kSep (juce::CharPointer_UTF8 (" \xe2\x80\xba "));   // " › "

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

    template <typename T>
    T* findFirst (juce::Component& root)
    {
        juce::Array<T*> found;
        collectAll<T> (root, found);
        return found.isEmpty() ? nullptr : found.getFirst();
    }

    juce::TextButton* findButton (juce::Component& root, const juce::String& text)
    {
        juce::Array<juce::TextButton*> buttons;
        collectAll<juce::TextButton> (root, buttons);

        for (auto* b : buttons)
            if (b->getButtonText() == text)
                return b;

        return nullptr;
    }

    juce::String titleCase (const juce::String& upper)
    {
        juce::StringArray words;
        words.addTokens (upper.toLowerCase(), " ", {});

        for (auto& w : words)
            if (w.isNotEmpty())
                w = w.substring (0, 1).toUpperCase() + w.substring (1);

        return words.joinIntoString (" ");
    }

    bool isInsideLearnTarget (juce::Component& c)
    {
        for (auto* p = c.getParentComponent(); p != nullptr; p = p->getParentComponent())
            if (dynamic_cast<LearnTarget*> (p) != nullptr)
                return true;

        return false;
    }

    juce::String workshopCategoryFor (PartType type)
    {
        switch (type)
        {
            case PartType::body: case PartType::top: case PartType::pickguard: return "Body";
            case PartType::neck: case PartType::fretboard:                     return "Neck";
            case PartType::frets:     return "Frets";
            case PartType::nut:       return "Nut";
            case PartType::bridge: case PartType::tailpiece:                   return "Bridge";
            case PartType::tuners:    return "Tuners";
            case PartType::strings:   return "Strings";
            case PartType::pickup:    return "Pickups";
            case PartType::wiring:    return "Wiring";
            case PartType::pick:      return "Pick";
            case PartType::slide:     return "Slide";
            case PartType::capo:      return "Capo";
            case PartType::numTypes:
            default: break;
        }

        return "Guitar";
    }

    constexpr int kSnapshotCommands = 16;
    constexpr int kMaxPendingAttempts = 30;
}

//==============================================================================
SearchHighlighter::SearchHighlighter()
{
    setInterceptsMouseClicks (false, false);
    setAccessible (false);
}

SearchHighlighter::~SearchHighlighter()
{
    stopTimer();
}

void SearchHighlighter::flash (juce::Component* newTarget, juce::Rectangle<int> area)
{
    target = newTarget;
    targetArea = area;
    // accessibility 5, and cpu-quality-modes 6 (INTEGRATE-2): with Reduced
    // motion or a quality that stops transitions, a static ring.
    reduced = AccessibilitySettings::get().isReducedMotion()
           || ! AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition);
    elapsed = 0.0;
    flashing = newTarget != nullptr;
    frameAlphas.clear();
    alpha = 1.0f;
    lastTickMs = juce::Time::getMillisecondCounterHiRes();

    setVisible (flashing);

    if (flashing)
    {
        toFront (false);

        // The pulse animates at 60 Hz; the static ring needs one tick to end.
        if (reduced)
            startTimer ((int) kStaticMs);
        else
            startTimerHz (60);
    }

    repaint();
}

void SearchHighlighter::stop()
{
    flashing = false;
    stopTimer();
    setVisible (false);
}

juce::Rectangle<int> SearchHighlighter::getRingBounds() const
{
    auto* t = target.getComponent();

    if (t == nullptr || t->getTopLevelComponent() != getTopLevelComponent())
        return {};

    return getLocalArea (t, targetArea.isEmpty() ? t->getLocalBounds() : targetArea).expanded (4);
}

void SearchHighlighter::advance (double elapsedMs)
{
    if (! flashing)
        return;

    // A control deleted mid-flash just ends the flash (4.2 step 8).
    if (target == nullptr)
    {
        stop();
        return;
    }

    elapsed += elapsedMs;

    if (reduced)
    {
        // accessibility 5: a static ring, no intermediate frames.
        alpha = 1.0f;

        if (elapsed >= kStaticMs)
            stop();
    }
    else
    {
        if (elapsed >= kPulseMs)
        {
            stop();
            return;
        }

        // Three pulses over 900 ms.
        const double phase = elapsed / (kPulseMs / 3.0);
        alpha = (float) (0.35 + 0.65 * (0.5 + 0.5 * std::cos (juce::MathConstants<double>::twoPi * phase)));
    }

    repaint();
}

void SearchHighlighter::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    advance (now - lastTickMs);
    lastTickMs = now;
}

void SearchHighlighter::paint (juce::Graphics& g)
{
    if (! flashing)
        return;

    const auto ring = getRingBounds();

    if (ring.isEmpty())
        return;

    frameAlphas.push_back (alpha);

    // theme.md accent, 2 px, 4 px outset, 4 px corners: a shape as well as a colour.
    g.setColour (Palette::accent.withAlpha (alpha));
    g.drawRoundedRectangle (ring.toFloat(), 4.0f, 2.0f);
}

//==============================================================================
SearchNavigator::SearchNavigator (LuthierAudioProcessorEditor& e, LuthierAudioProcessor& p)
    : editor (e), processor (p)
{
    palette = std::make_unique<CommandPalette> (*this);
}

SearchNavigator::~SearchNavigator()
{
    processor.getPresetManager().removeChangeListener (this);
    stopTimer();
    endNudgeGesture();
}

void SearchNavigator::initialise()
{
    tagSurfaces();
    registerActions();

    addDefaultProviders (index, processor, this, &actions);
    processor.getPresetManager().addChangeListener (this);

    // 4.1: +40 for an item visible in the current mode without switching.
    index.isVisibleNow = [this] (const SearchItem& item)
    {
        return editor.advancedMode ? item.inAdvanced : item.inEasy;
    };
}

//==============================================================================
void SearchNavigator::tagSurfaces()
{
    SearchAnchors::tag (editor.easyPanel, "mode:easy");
    SearchAnchors::tag (editor.advancedPanel, "mode:advanced");
    SearchAnchors::tag (editor.header, "header");
    SearchAnchors::tag (editor.liveStrip, "livestrip");

    // Column 4's tabs (gui-integration 4.4).
    auto& adv = editor.advancedPanel;

    for (int i = 0; i < adv.getNumWorkspaceTabs(); ++i)
        if (auto* panel = adv.getWorkspacePanel (i))
            SearchAnchors::tag (*panel, "tab:" + adv.getWorkspaceTabName (i), [&adv, i] { adv.setWorkspaceTab (i); },
                                adv.getWorkspaceTabName (i));

    // The overlays the editor owns.
    const struct { OverlayPanel* panel; const char* name; } overlays[] =
    {
        { &editor.optionsPanel, "options" }, { &editor.presetBrowser, "presetBrowser" }, { &editor.helpPanel, "help" },
        { &editor.exportPanel, "export" },   { &editor.chordPanel, "chords" },          { &editor.debugPanel, "debug" },
        { &editor.saveAsPanel, "saveAs" },   { &editor.workshopOverlay, "workshop" },   { &editor.secretPanel, "secret" },
    };

    for (const auto& o : overlays)
    {
        auto* panel = o.panel;
        SearchAnchors::tag (*panel, juce::String ("overlay:") + o.name, [this, panel]
        {
            if (editor.overlayHost.getCurrentOverlay() != panel)
                editor.showOverlay (panel);
        });
    }

    // Options pages, in tab order (gui-integration 5).
    {
        juce::Array<OptionsPage*> pages;
        collectAll<OptionsPage> (editor.optionsPanel, pages);
        const auto names = editor.optionsPanel.getPageNames();

        for (int i = 0; i < juce::jmin (pages.size(), names.size()); ++i)
        {
            const auto name = names[i];
            SearchAnchors::tag (*pages[i], "options:" + name, [this, name]
            {
                editor.optionsPanel.showPageNamed (name);

                if (editor.overlayHost.getCurrentOverlay() != &editor.optionsPanel)
                    editor.showOverlay (&editor.optionsPanel);
            }, name);
        }
    }

    // The practice drawer and its tabs (practice-tools 9).
    SearchAnchors::tag (editor.practicePanel, "drawer", [this]
    {
        if (! editor.practicePanel.isOpen())
        {
            editor.practicePanel.setOpen (true);
            editor.resized();
        }
    });

    {
        juce::Array<PracticeTab*> tabs;
        collectAll<PracticeTab> (editor.practicePanel, tabs);

        for (int i = 0; i < tabs.size() && i < (int) PracticeTool::numTools; ++i)
        {
            const juce::String label (getPracticeToolTabLabel ((PracticeTool) i));
            SearchAnchors::tag (*tabs[i], "drawer:" + label, [this, i]
            {
                editor.practicePanel.setOpen (true);
                editor.practicePanel.showTool ((PracticeTool) i);
                editor.resized();
            }, label);
        }
    }

    // Easy's compact racks, pre then post in child order (EasyPanel adds them so).
    {
        juce::Array<CompactRack*> racks;
        collectAll<CompactRack> (editor.easyPanel, racks);

        if (racks.size() >= 2)
        {
            SearchAnchors::tag (*racks[0], "rack:pre");
            SearchAnchors::tag (*racks[1], "rack:post");
        }
    }

    // Groups inside workspace tabs (CHARACTER and RHYTHM).
    auto tagGroup = [] (juce::Component* c, const char* id, const char* title)
    {
        if (c != nullptr)
            SearchAnchors::tag (*c, id, {}, title);
    };

    // The tabs' panels, not the AdvancedPanel: only the selected one is its child.
    for (int i = 0; i < adv.getNumWorkspaceTabs(); ++i)
    {
        auto* panel = adv.getWorkspacePanel (i);

        if (panel == nullptr)
            continue;

        tagGroup (findFirst<SetupGroup> (*panel),    "group:CHARACTER:SETUP", "Setup");
        tagGroup (findFirst<NoiseGroups> (*panel),   "group:CHARACTER:NOISE", "Noise");
        tagGroup (findFirst<SlideGroup> (*panel),    "group:CHARACTER:SLIDE", "Slide");
        tagGroup (findFirst<SlapGroup> (*panel),     "group:CHARACTER:SLAP", "Slap");
        tagGroup (findFirst<StrumGroup> (*panel),    "group:RHYTHM:STRUM", "Strum");
        tagGroup (findFirst<BassGridGroup> (*panel), "group:RHYTHM:BASS GRID", "Bass grid");

        // The realism groups (REALISM-A/B/C), also in CHARACTER.
        tagGroup (findFirst<StringAgingGroup> (*panel),       "group:CHARACTER:STRING AGING", "String aging");
        tagGroup (findFirst<EnvironmentGroup> (*panel),       "group:CHARACTER:ENVIRONMENT", "Environment");
        tagGroup (findFirst<BodyCouplingGroup> (*panel),      "group:CHARACTER:BODY COUPLING", "Body coupling");
        tagGroup (findFirst<NoiseFloorGroup> (*panel),        "group:CHARACTER:NOISE FLOOR", "Noise floor");
        tagGroup (findFirst<SustainShapeGroup> (*panel),      "group:CHARACTER:SUSTAIN SHAPE", "Sustain shape");
        tagGroup (findFirst<TuningStabilityGroup> (*panel),   "group:CHARACTER:TUNING STABILITY", "Tuning stability");
        tagGroup (findFirst<HarmonicsGroup> (*panel),         "group:CHARACTER:HARMONICS", "Harmonics");
        tagGroup (findFirst<RightHandGroup> (*panel),         "group:CHARACTER:RIGHT HAND", "Right hand");
        tagGroup (findFirst<StringInteractionGroup> (*panel), "group:CHARACTER:STRING INTERACTION", "String interaction");
    }
}

void SearchNavigator::registerActions()
{
    actions.clear();

    auto undoFor = [] (const juce::String& id)
    {
        if (id == "panic" || id == "tapTempo")                       return UndoClass::skipsStack;
        if (id == "toggleSlideMode")                                 return UndoClass::slideMode;
        if (id == "newPreset" || id == "previousItem" || id == "nextItem") return UndoClass::boundary;
        if (id == "randomise" || id == "resetAll")                   return UndoClass::parameter;
        return UndoClass::none;
    };

    // Every shortcut is a command, through the one action path (4.3).
    for (const auto& b : AccessibilitySettings::get().getShortcuts())
    {
        ActionDef def;
        def.id = b.id;
        def.titleKey = b.descriptionKey;
        def.undo = undoFor (b.id);
        def.perform = [this, id = b.id] { return editor.performAction (id); };
        actions.add (std::move (def));
    }

    auto add = [this] (const juce::String& id, const juce::String& titleKey, UndoClass undo,
                       std::function<Availability()> available = {}, const juce::String& fixedTitle = {})
    {
        ActionDef def;
        def.id = id;
        def.titleKey = titleKey;
        def.fixedTitle = fixedTitle;
        def.undo = undo;
        def.available = std::move (available);
        def.perform = [this, id] { return editor.performAction (id); };
        actions.add (std::move (def));
    };

    // Commands with no key (4.3).
    add ("openWorkshop", "search.cmd.openWorkshop", UndoClass::none);
    add ("exportAudio", "search.cmd.exportAudio", UndoClass::none);
    add ("exportMidi", "search.cmd.exportMidi", UndoClass::none);
    add ("importMidi", "search.cmd.importMidi", UndoClass::none);
    add ("retuneAll", "search.cmd.retuneAll", UndoClass::none);
    // "New tune" is a shortcut now (Ctrl+T, TUNE-HELP-ONBOARDING): registered above.
    add ("openChords", "search.cmd.openChords", UndoClass::none);
    add ("clearRecentSearches", "search.cmd.clearRecentSearches", UndoClass::none);
    add ("resetMicPlacement", "search.cmd.resetMicPlacement", UndoClass::parameter);   // global-search.md 8 (FEAT-MIC)

    for (int n = 1; n <= kSnapshotCommands; ++n)
    {
        add ("recallSnapshot" + juce::String (n), {}, UndoClass::snapshot,
             [this, n] { return n <= processor.getSnapshots().getNumSnapshots() ? Availability::available : Availability::notBuilt; },
             SearchCatalog::text ("search.cmd.recallSnapshot", { { "n", juce::String (n) } }));

        add ("saveSnapshot" + juce::String (n), {}, UndoClass::snapshot, {},
             SearchCatalog::text ("search.cmd.saveSnapshot", { { "n", juce::String (n) } }));
    }

    // Arm / disarm each technique that has an arm switch.
    add ("armSlap", {}, UndoClass::parameter, [this] { return ParameterLocations::evaluate (ParameterLocations::Gate::bass, processor); },
         SearchCatalog::text ("search.cmd.armTechnique", { { "technique", "slap" } }));
    add ("armScrape", {}, UndoClass::parameter, {},
         SearchCatalog::text ("search.cmd.armTechnique", { { "technique", "pick scrape" } }));
}

bool SearchNavigator::performExtendedAction (const juce::String& id)
{
    if (id == "search")
    {
        togglePalette();
        return true;
    }

    if (id.startsWith ("__showShortcut:"))
        return openShortcutRow (id.fromFirstOccurrenceOf (":", false, false));

    if (id.startsWith ("__searchHelp:"))
    {
        const auto query = id.fromFirstOccurrenceOf (":", false, false);
        editor.openHelp (query);

        // "Search Help for ..." opens Help with its search filtered.
        auto& view = editor.advancedMode && editor.advancedPanel.getHelpTab() != nullptr ? *editor.advancedPanel.getHelpTab()
                                                                                        : editor.helpPanel.getView();
        view.getShortcutSearch().setText (query, juce::sendNotificationSync);
        return true;
    }

    if (id == "openWorkshop")
    {
        // gui-integration.md 6: the WORKSHOP tab in Advanced, the bench as an overlay in Easy.
        if (editor.advancedMode)
            editor.advancedPanel.setWorkspaceTabNamed ("WORKSHOP");
        else
            editor.showOverlay (&editor.workshopOverlay);

        return true;
    }

    if (id == "resetMicPlacement")
    {
        // mic-placement.md 7 and 8 (INTEGRATE-2): both mics, electric and
        // acoustic, back to their default places - one undo entry, as the
        // expanded editor's Reset does.
        const juce::StringArray ids { ParamIDs::micX, ParamIDs::micY, ParamIDs::micDist, ParamIDs::micAngle,
                                      ParamIDs::micSpeaker, ParamIDs::micRear,
                                      ParamIDs::micX2, ParamIDs::micY2, ParamIDs::micDist2, ParamIDs::micAngle2,
                                      ParamIDs::micSpeaker2, ParamIDs::micRear2,
                                      ParamIDs::acMicAlong, ParamIDs::acMicAcross, ParamIDs::acMicDist, ParamIDs::acMicAngle,
                                      ParamIDs::acMicAlong2, ParamIDs::acMicAcross2, ParamIDs::acMicDist2, ParamIDs::acMicAngle2 };
        MicEdit edit (processor, SearchCatalog::text ("search.cmd.resetMicPlacement"), ids);

        for (const auto& pid : ids)
            if (auto* prm = processor.getState().getParameter (pid))
                prm->setValueNotifyingHost (prm->getDefaultValue());

        return true;
    }

    if (id == "exportAudio") { editor.showOverlay (&editor.exportPanel); return true; }
    if (id == "openChords")  { editor.showOverlay (&editor.chordPanel);  return true; }

    if (id == "exportMidi")
    {
        // The MIDI OUT tab's own export: switch there (auto-switch rules),
        // then press its button, so there is one code path.
        Outcome outcome;

        if (! switchModeFor (Mode::advanced, SearchCatalog::text ("search.cmd.exportMidi"), true, outcome))
        {
            showFooterMessage (outcome.message, true);
            return false;
        }

        editor.advancedPanel.setWorkspaceTabNamed ("MIDI OUT");
        editor.resized();

        if (onExportMidiPressed != nullptr)
        {
            onExportMidiPressed();
            return true;
        }

        if (auto* midiOut = findFirst<MidiOutPanel> (editor.advancedPanel))
            if (auto* button = findButton (*midiOut, "EXPORT MIDI..."); button != nullptr && button->onClick != nullptr)
            {
                button->onClick();
                return true;
            }

        return false;
    }

    if (id == "importMidi")
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Import a MIDI file", juce::File(), "*.mid;*.midi");
        juce::Component::SafePointer<LuthierAudioProcessorEditor> safe (&editor);

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe, chooser] (const juce::FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult().existsAsFile())
                safe->importMidiFile (fc.getResult());
        });

        return true;
    }

    if (id == "retuneAll")
    {
        // The CHARACTER panel's own Retune button (character-wear 10).
        if (auto* character = findFirst<CharacterPanel> (editor.advancedPanel))
            if (auto* button = findButton (*character, "Retune"); button != nullptr && button->onClick != nullptr)
            {
                button->onClick();
                return true;
            }

        processor.getEngine().getCharacterEngine().retune();
        return true;
    }

    if (id == "clearRecentSearches")
    {
        clearRecentSearches();
        postNotice (SearchCatalog::text ("search.recentCleared"));
        return true;
    }

    if (id.startsWith ("recallSnapshot"))
    {
        const int n = id.getTrailingIntValue();
        return n >= 1 && processor.recallSnapshot (n - 1);
    }

    if (id.startsWith ("saveSnapshot"))
    {
        const int n = id.getTrailingIntValue();
        return n >= 1 && processor.captureSnapshot (n - 1);
    }

    if (id == "armSlap" || id == "armScrape")
    {
        if (auto* p = processor.getState().getParameter (id == "armSlap" ? ParamIDs::slapArmed : ParamIDs::scrapeArmed))
            return ChoiceOptionProvider::setAsGesture (*p, p->getValue() >= 0.5f ? 0.0f : 1.0f);

        return false;
    }

    return false;
}

//==============================================================================
void SearchNavigator::openPalette (const juce::String& initialText)
{
    // 6.2: opening the palette cancels an armed MIDI Learn.
    if (processor.getMidiLearn().isArmed())
        editor.setMidiLearnArmed (false);

    // The folder-backed lists are refreshed here, not in collect (8).
    if (auto* guitars = dynamic_cast<GuitarProvider*> (index.getProvider ("guitar")))
        guitars->rescan();

    if (auto* tunes = dynamic_cast<TuneProvider*> (index.getProvider ("tune")))
        tunes->rescan();

    palette->open (initialText);
}

void SearchNavigator::closePalette()
{
    palette->close (true);
}

void SearchNavigator::togglePalette()
{
    if (palette->isOpen())
        palette->close (true);
    else
        openPalette();
}

bool SearchNavigator::isPaletteOpen() const
{
    return palette->isOpen();
}

void SearchNavigator::layout (juce::Rectangle<int> editorBounds, int)
{
    palette->setBounds (editorBounds);
    highlighter.setBounds (editorBounds);

    // 13: resized below 1000 px while open, rows re-evaluate modeUnavailable.
    if (palette->isOpen())
        palette->refresh();
}

bool SearchNavigator::isLiveMode() const
{
    return processor.isLiveMode();
}

void SearchNavigator::requestFocus (juce::Component* c)
{
    lastFocusRequest = c;

    if (c != nullptr)
        c->grabKeyboardFocus();
}

void SearchNavigator::announce (const juce::String& text)
{
    if (text.isEmpty())
        return;

    if (onAnnouncement)
        onAnnouncement (text);

    juce::AccessibilityHandler::postAnnouncement (text, juce::AccessibilityHandler::AnnouncementPriority::medium);
}

//==============================================================================
// Where controls are.

void SearchNavigator::rebuildControlMap()
{
    if (controlMapChangeCount == LiveControls::getChangeCount())
        return;

    controlMap.clear();

    LiveControls::forEach ([this] (juce::Component& c, const juce::String& id)
    {
        if (id.isNotEmpty() && belongsToEditor (c))
            controlMap[id].push_back (&c);
    });

    controlMapChangeCount = LiveControls::getChangeCount();
}

bool SearchNavigator::isWorkspacePanel (const juce::Component* c) const
{
    auto& adv = editor.advancedPanel;

    for (int i = 0; i < adv.getNumWorkspaceTabs(); ++i)
        if (adv.getWorkspacePanel (i) == c)
            return true;

    return false;
}

bool SearchNavigator::belongsToEditor (juce::Component& c) const
{
    if (editor.isParentOf (&c))
        return true;

    // An overlay that is not up has no parent, nor has a workspace tab that
    // is not selected (the viewport holds one panel); a popover is its own window.
    auto* top = &c;

    while (top->getParentComponent() != nullptr)
        top = top->getParentComponent();

    if (isWorkspacePanel (top))
        return true;

    if (dynamic_cast<OverlayPanel*> (top) != nullptr)
        for (auto* o : std::initializer_list<juce::Component*> { &editor.optionsPanel, &editor.presetBrowser, &editor.helpPanel,
                                                                 &editor.exportPanel, &editor.chordPanel, &editor.debugPanel,
                                                                 &editor.saveAsPanel, &editor.workshopOverlay, &editor.secretPanel })
            if (o == top)
                return true;

    return dynamic_cast<juce::CallOutBox*> (top) != nullptr && top->isOnDesktop();
}

bool SearchNavigator::isOnScreen (juce::Component& c) const
{
    if (c.getWidth() <= 0 || c.getHeight() <= 0)
        return false;

    for (auto* p = &c; p != nullptr; p = p->getParentComponent())
    {
        if (! p->isVisible())
            return false;

        if (p == &editor)
            return true;

        if (p->getParentComponent() == nullptr)
            return p->isOnDesktop();
    }

    return false;
}

OverlayPanel* SearchNavigator::overlayContaining (juce::Component& c) const
{
    for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        if (auto* o = dynamic_cast<OverlayPanel*> (p))
            return o;

    return nullptr;
}

OverlayPanel* SearchNavigator::overlayNamed (const juce::String& name) const
{
    if (name == "options")       return &editor.optionsPanel;
    if (name == "presetBrowser") return &editor.presetBrowser;
    if (name == "help")          return &editor.helpPanel;
    if (name == "export")        return &editor.exportPanel;
    if (name == "chords")        return &editor.chordPanel;
    if (name == "debug")         return &editor.debugPanel;
    if (name == "saveAs")        return &editor.saveAsPanel;
    if (name == "workshop")      return &editor.workshopOverlay;
    return nullptr;
}

bool SearchNavigator::canShowPopovers() const
{
    // A popover is its own desktop window (a CallOutBox), which needs the
    // editor on screen; without one (a test with no desktop peer) the
    // canonical Advanced control is used instead.
    return editor.isShowing();
}

SearchNavigator::Mode SearchNavigator::modeOf (juce::Component& c) const
{
    if (editor.easyPanel.isParentOf (&c))     return Mode::easy;
    if (editor.advancedPanel.isParentOf (&c)) return Mode::advanced;

    for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        if (isWorkspacePanel (p))
            return Mode::advanced;

    return Mode::either;
}

juce::Component* SearchNavigator::chooseControl (const juce::String& parameterId, bool& needsModeSwitch)
{
    rebuildControlMap();
    needsModeSwitch = false;

    auto it = controlMap.find (parameterId);

    if (it == controlMap.end())
        return nullptr;

    const auto current = editor.advancedMode ? Mode::advanced : Mode::easy;
    juce::Component* best = nullptr;
    int bestRank = 1000;

    for (auto* c : it->second)
    {
        const auto mode = modeOf (*c);
        const bool inOverlay = overlayContaining (*c) != nullptr;

        // Step 1: on screen now beats reachable in this mode beats the other
        // mode; inside the window beats inside an overlay; Advanced is canonical.
        int rank = isOnScreen (*c) ? 0 : (mode == Mode::either || mode == current) ? 10 : 20;
        rank += inOverlay ? 2 : 0;
        rank += mode == Mode::advanced ? 0 : 1;

        if (rank < bestRank)
        {
            bestRank = rank;
            best = c;
        }
    }

    if (best == nullptr)
        return nullptr;

    // A control that exists only in the other mode, while this mode has a
    // popover that holds it (the headstock, a rack slot), stays in this mode.
    if (bestRank >= 20 && current == Mode::easy && canShowPopovers()
          && ! ParameterLocations::popoverLocationFor (parameterId, processor).isEmpty())
        return nullptr;

    const auto mode = modeOf (*best);
    needsModeSwitch = mode != Mode::either && mode != current;
    return best;
}

juce::String SearchNavigator::easyStripFor (const juce::Component& c) const
{
    auto& easy = editor.easyPanel;

    if (! easy.isParentOf (&c))
        return {};

    const auto centre = easy.getLocalArea (&c, c.getLocalBounds()).getCentre();

    if (easy.getRigArea().contains (centre))     return "Rig";
    if (easy.getPlayingArea().contains (centre)) return "Playing";
    if (easy.getToneArea().contains (centre))    return "Tone";
    if (easy.getRhythmArea().contains (centre))  return "Rhythm";
    return "Guitar";
}

juce::String SearchNavigator::breadcrumbFor (juce::Component& control)
{
    juce::StringArray parts;

    switch (modeOf (control))
    {
        case Mode::easy:     parts.add ("Easy"); break;
        case Mode::advanced: parts.add ("Adv"); break;
        case Mode::either:
        default: break;
    }

    if (const auto strip = easyStripFor (control); strip.isNotEmpty())
        parts.add (strip);

    int column = 0;
    const auto section = editor.advancedPanel.getColumnSectionFor (&control, column);

    if (column > 0)
    {
        parts.add ("Col " + juce::String (column));

        if (section.isNotEmpty())
            parts.add (section);
    }

    for (const auto& step : SearchAnchors::chain (control))
    {
        const auto place = step.placeId;
        const auto title = SearchAnchors::getPlaceTitle (*step.component);

        if (place.startsWith ("mode:") || place.startsWith ("column:") || place == "drawer" || place.startsWith ("rack:"))
            continue;

        if (place.startsWith ("tab:"))            parts.add (titleCase (place.substring (4)));
        else if (place.startsWith ("options:"))   { parts.add ("Options"); parts.add (titleCase (place.substring (8))); }
        else if (place.startsWith ("drawer:"))    { parts.add ("Practice"); parts.add (titleCase (place.substring (7))); }
        else if (place.startsWith ("overlay:"))   parts.add (dynamic_cast<OverlayPanel*> (step.component) != nullptr
                                                               ? step.component->getName().isNotEmpty() ? step.component->getName()
                                                                                                        : titleCase (place.substring (8))
                                                               : titleCase (place.substring (8)));
        else if (place.startsWith ("group:"))     parts.add (title.isNotEmpty() ? title : titleCase (place.fromLastOccurrenceOf (":", false, false)));
        else if (title.isNotEmpty())              parts.add (title);
    }

    return parts.joinIntoString (kSep);
}

void SearchNavigator::describeParameter (const juce::String& parameterId, juce::String& breadcrumb,
                                         bool& inEasy, bool& inAdvanced)
{
    rebuildControlMap();

    inEasy = inAdvanced = false;
    juce::Component* preferred = nullptr;

    if (auto it = controlMap.find (parameterId); it != controlMap.end())
    {
        for (auto* c : it->second)
        {
            const auto mode = modeOf (*c);

            inEasy = inEasy || mode != Mode::advanced;
            inAdvanced = inAdvanced || mode != Mode::easy;

            // The canonical control (Advanced) names the place (4.2 step 1).
            if (preferred == nullptr || (mode == Mode::advanced && modeOf (*preferred) != Mode::advanced))
                preferred = c;
        }
    }

    if (! ParameterLocations::popoverLocationFor (parameterId, processor).isEmpty())
        inEasy = true;

    breadcrumb = preferred != nullptr ? breadcrumbFor (*preferred) : juce::String();
}

juce::uint32 SearchNavigator::getUiGeneration()
{
    return (juce::uint32) LiveControls::getChangeCount() + 1u;
}

bool SearchNavigator::isAdvancedModeAvailable()
{
    // 4.2 step 3: below 1000 px, or Live Mode locking the toggle.
    return editor.isAdvancedModeAvailable() && ! processor.isLiveMode();
}

bool SearchNavigator::isAdvancedMode()
{
    return editor.advancedMode;
}

bool SearchNavigator::isProLocked (const SearchItem& item)
{
    return proLockPredicate != nullptr && proLockPredicate (item);
}

//==============================================================================
// Navigation.

bool SearchNavigator::switchModeFor (Mode wanted, const juce::String& name, bool confirmed, Outcome& outcome)
{
    const auto current = editor.advancedMode ? Mode::advanced : Mode::easy;

    if (wanted == Mode::either || wanted == current)
        return true;

    if (wanted == Mode::advanced && ! isAdvancedModeAvailable())
    {
        outcome.status = Outcome::Status::refused;
        outcome.allowInline = true;
        outcome.warning = true;
        outcome.message = SearchCatalog::text (processor.isLiveMode() ? "search.advancedLocked" : "search.advancedUnavailable");
        return false;
    }

    // 4.2 step 2: switch automatically, unless the user asked to be asked.
    if (! isAutoSwitchModeOn() && ! confirmed)
    {
        outcome.status = Outcome::Status::needsConfirm;
        outcome.message = SearchCatalog::text ("search.opensInAdvancedConfirm");
        return false;
    }

    editor.setAdvancedMode (wanted == Mode::advanced);
    editor.header.setAdvancedMode (editor.advancedMode);

    const auto* binding = AccessibilitySettings::get().findShortcut ("toggleAdvanced");
    const auto key = binding != nullptr && binding->key.isValid() ? binding->key.getTextDescription() : juce::String ("Tab");

    lastNotice = SearchCatalog::text (wanted == Mode::advanced ? "search.switchedToAdvanced" : "search.switchedToEasy",
                                      { { "name", name }, { "key", key } });
    editor.inlineNotice.show (lastNotice);
    editor.resized();
    return true;
}

void SearchNavigator::scrollIntoView (juce::Component& c)
{
    // 4.2 step 6: every Viewport ancestor, inner first, least movement, 24 px margin.
    for (auto* p = c.getParentComponent(); p != nullptr; p = p->getParentComponent())
    {
        auto* viewport = dynamic_cast<juce::Viewport*> (p);

        if (viewport == nullptr)
            continue;

        auto* viewed = viewport->getViewedComponent();

        if (viewed == nullptr || ! viewed->isParentOf (&c))
            continue;

        const auto area = viewed->getLocalArea (&c, c.getLocalBounds()).expanded (24);
        const auto view = viewport->getViewArea();
        int x = view.getX(), y = view.getY();

        if (area.getRight() > view.getRight())  x = area.getRight() - view.getWidth();
        if (area.getX() < x)                    x = area.getX();
        if (area.getBottom() > view.getBottom()) y = area.getBottom() - view.getHeight();
        if (area.getY() < y)                    y = area.getY();

        viewport->setViewPosition (juce::jmax (0, x), juce::jmax (0, y));
    }
}

void SearchNavigator::focusAndHighlight (juce::Component& control, juce::Rectangle<int> area)
{
    auto* inner = LiveControls::innerControl (control);

    if (dynamic_cast<LearnTarget*> (&control) == nullptr)
    {
        // A place: its first interactive child, as an overlay does (accessibility 1).
        inner = nullptr;

        juce::Array<juce::Component*> all;
        collectAll<juce::Component> (control, all);

        for (auto* c : all)
            if (c->getWantsKeyboardFocus() && c->isShowing() && c->isEnabled())
            {
                inner = c;
                break;
            }
    }

    if (inner != nullptr)
    {
        inner->setWantsKeyboardFocus (true);
        requestFocus (inner);
    }

    last.focused = inner;
    last.control = &control;

    highlighter.flash (&control, area);
}

bool SearchNavigator::finishOnControl (juce::Component& control, const SearchItem& item)
{
    // Step 5: an overlay other than the target's comes down first.
    if (editor.overlayHost.isShowingOverlay())
        if (auto* shown = editor.overlayHost.getCurrentOverlay(); shown != nullptr && ! shown->isParentOf (&control))
            editor.overlayHost.dismiss();

    for (const auto& step : SearchAnchors::chain (control))
        if (! step.placeId.startsWith ("mode:"))
            SearchAnchors::open (*step.component);

    editor.resized();

    if (! isOnScreen (control))
        return false;

    scrollIntoView (control);
    focusAndHighlight (control);

    last.itemId = item.id;
    last.ok = true;
    last.pending = false;
    last.message.clear();

    // Step 9.
    announce (SearchCatalog::text ("search.announce.target", { { "name", item.title }, { "breadcrumb", item.breadcrumb } }));
    return true;
}

SearchNavigator::Outcome SearchNavigator::goToParameterControl (const SearchItem& item, bool confirmed)
{
    Outcome outcome;
    const auto id = item.target;

    last = {};
    last.itemId = item.id;

    auto* provider = index.getProviderFor (item);
    const auto availability = provider != nullptr ? provider->availabilityOf (item) : Availability::available;

    // Step 4: context gates.
    if (availability == Availability::needsSlideMode)
    {
        if (! confirmed)
        {
            outcome.status = Outcome::Status::needsConfirm;
            outcome.message = SearchCatalog::text ("search.slideModeOff");
            return outcome;
        }

        // The header's own undoable toggle (action-and-undo 3.3); the SLIDE
        // group shows on its next timer tick, so the rest waits for it.
        HeaderBar::toggleSlideMode (processor);
        pending = Pending { id, item.id, item.title, 0 };
        last.pending = true;
        startTimer (40);
        outcome.status = Outcome::Status::done;
        return outcome;
    }

    if (availability == Availability::needsBass || availability == Availability::needsWhammy
        || availability == Availability::needsAcoustic)
    {
        const bool whammy = availability == Availability::needsWhammy;
        const char* part = availability == Availability::needsBass ? "search.part.bass"
                         : whammy                                  ? "search.part.whammy"
                                                                   : "search.part.acoustic";

        if (! confirmed)
        {
            outcome.status = Outcome::Status::needsConfirm;
            outcome.message = SearchCatalog::text ("search.needsPart", { { "part", SearchCatalog::text (part) },
                                                                         { "category", whammy ? "Bridge" : "Body" } });
            return outcome;
        }

        openWorkshopOn (whammy ? PartType::bridge : PartType::body, {});
        outcome.status = Outcome::Status::done;
        return outcome;
    }

    bool needsSwitch = false;
    auto* control = chooseControl (id, needsSwitch);

    if (control == nullptr)
    {
        // A control that exists only when its popover is open (3.2).
        const auto location = ParameterLocations::popoverLocationFor (id, processor);

        if (! location.isEmpty() && ! editor.advancedMode && canShowPopovers())
        {
            for (const auto& step : location.steps)
                runStep (step);

            pending = Pending { id, item.id, item.title, 0 };
            last.pending = true;
            startTimer (40);
            outcome.status = Outcome::Status::done;
            return outcome;
        }

        // 13: a bug, not a user error - say so, log it, and GS-02 fails.
        ErrorLog::write (ErrorLog::Severity::warn, "Search", "SEARCH_NAVIGATE_MISS", "search.navigate.miss " + id);
        outcome.status = Outcome::Status::failed;
        outcome.warning = true;
        outcome.message = SearchCatalog::text ("search.navigateMiss", { { "name", item.title } });
        last.message = outcome.message;
        return outcome;
    }

    if (needsSwitch && ! switchModeFor (modeOf (*control), item.title, confirmed, outcome))
    {
        last.message = outcome.message;
        return outcome;
    }

    if (! finishOnControl (*control, item))
    {
        ErrorLog::write (ErrorLog::Severity::warn, "Search", "SEARCH_NAVIGATE_MISS", "search.navigate.miss " + id);
        outcome.status = Outcome::Status::failed;
        outcome.warning = true;
        outcome.message = SearchCatalog::text ("search.navigateMiss", { { "name", item.title } });
        last.message = outcome.message;
        return outcome;
    }

    outcome.status = Outcome::Status::done;
    return outcome;
}

void SearchNavigator::pumpPending()
{
    if (! pending.has_value())
        return;

    auto p = *pending;
    bool needsSwitch = false;

    if (auto* control = chooseControl (p.parameterId, needsSwitch); control != nullptr && ! needsSwitch)
    {
        if (isOnScreen (*control) || control->getTopLevelComponent() == &editor)
        {
            SearchItem item;
            item.id = p.itemId;
            item.title = p.name;
            item.target = p.parameterId;

            if (auto* indexed = index.find (p.itemId))
                item = *indexed;

            if (finishOnControl (*control, item))
            {
                pending.reset();
                return;
            }
        }
    }

    if (++p.attempts >= kMaxPendingAttempts)
    {
        pending.reset();
        last.pending = false;
        last.ok = false;
        last.message = SearchCatalog::text ("search.navigateMiss", { { "name", p.name } });
        ErrorLog::write (ErrorLog::Severity::warn, "Search", "SEARCH_NAVIGATE_MISS", "search.navigate.miss " + p.parameterId);
        editor.inlineNotice.show (last.message, InlineNotice::Level::warning);
        editor.resized();
        return;
    }

    pending = p;
}

void SearchNavigator::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (palette->isOpen())
        palette->refresh();
}

std::vector<juce::Component*> SearchNavigator::getOwnedOverlays() const
{
    return { &editor.optionsPanel, &editor.presetBrowser, &editor.helpPanel, &editor.exportPanel, &editor.chordPanel,
             &editor.debugPanel, &editor.saveAsPanel, &editor.workshopOverlay, &editor.secretPanel };
}

void SearchNavigator::timerCallback()
{
    pumpPending();

    if (nudgeGestureOpen && juce::Time::getMillisecondCounterHiRes() - lastNudgeMs > 200.0)
        endNudgeGesture();

    if (! pending.has_value() && ! nudgeGestureOpen)
        stopTimer();
}

bool SearchNavigator::runStep (const LocationStep& step)
{
    using T = LocationStep::Type;
    auto& adv = editor.advancedPanel;

    switch (step.type)
    {
        case T::mode:
            return true;   // decided by the caller, by 4.2's rules

        case T::workspaceTab:
            return adv.setWorkspaceTabNamed (step.name);

        case T::column:
            if (auto* column = SearchAnchors::findPlace (adv, "column:" + juce::String (step.number)))
                return SearchAnchors::open (*column);
            return false;

        case T::subTab:
        {
            const auto kind = step.name.upToFirstOccurrenceOf ("|", false, false);
            const auto name = step.name.fromFirstOccurrenceOf ("|", false, false);

            if (kind == "section")
                return adv.revealColumnSection (name);

            if (kind == "group")
                if (auto* group = SearchAnchors::findPlace (*editor.advancedPanel.getWorkspacePanel (adv.getWorkspaceTab()), "group:" + name))
                {
                    scrollIntoView (*group);
                    return group->isVisible();
                }

            return kind == "easy";
        }

        case T::overlay:
            if (step.name == "workshop" && editor.advancedMode)
                return adv.setWorkspaceTabNamed ("WORKSHOP");

            if (auto* overlay = overlayNamed (step.name))
            {
                if (editor.overlayHost.getCurrentOverlay() != overlay)
                    editor.showOverlay (overlay);

                return true;
            }

            return false;

        case T::optionsPage:
            if (! editor.optionsPanel.showPageNamed (step.name))
                return false;

            if (editor.overlayHost.getCurrentOverlay() != &editor.optionsPanel)
                editor.showOverlay (&editor.optionsPanel);

            return true;

        case T::drawerTab:
            if (auto* tab = SearchAnchors::findPlace (editor.practicePanel, "drawer:" + step.name))
                return SearchAnchors::open (*tab);
            return false;

        case T::popover:
        {
            if (step.name == "headstock") { editor.easyPanel.getGuitar().showTuningPopover(); return true; }
            if (step.name == "bridge")    { editor.easyPanel.getGuitar().showWhammyPopover(); return true; }

            if (step.name.startsWith ("rack:"))
            {
                const auto chain = step.name.fromFirstOccurrenceOf (":", false, false).upToFirstOccurrenceOf (":", false, false);
                const int slot = step.name.getTrailingIntValue();

                if (auto* rack = dynamic_cast<CompactRack*> (SearchAnchors::findPlace (editor.easyPanel, "rack:" + chain)))
                    return rack->openSlot (slot) != nullptr;
            }

            return false;
        }

        case T::workshopCategory:
            if (auto* w = dynamic_cast<WorkshopPanel*> (workshopPanelShown()))
            {
                w->showCategory (step.name);
                return true;
            }

            return false;

        default:
            break;
    }

    return false;
}

juce::Component* SearchNavigator::workshopPanelShown() const
{
    return editor.advancedMode ? static_cast<juce::Component*> (editor.advancedPanel.getWorkshopPanel())
                               : static_cast<juce::Component*> (&editor.workshopOverlay.getPanel());
}

SearchNavigator::Outcome SearchNavigator::goToPlace (const SearchItem& item, bool confirmed)
{
    Outcome outcome;
    last = {};
    last.itemId = item.id;

    auto* provider = index.getProviderFor (item);
    const auto availability = provider != nullptr ? provider->availabilityOf (item) : Availability::available;

    if (availability == Availability::modeUnavailable)
    {
        // 4.2 step 3: a place result just shows the notice.
        outcome.status = Outcome::Status::refused;
        outcome.warning = true;
        outcome.message = SearchCatalog::text (processor.isLiveMode() ? "search.advancedLocked" : "search.advancedUnavailable");
        return outcome;
    }

    if (availability == Availability::needsSlideMode)
    {
        if (! confirmed)
        {
            outcome.status = Outcome::Status::needsConfirm;
            outcome.message = SearchCatalog::text ("search.slideModeOff");
            return outcome;
        }

        HeaderBar::toggleSlideMode (processor);
    }
    else if (availability == Availability::needsBass)
    {
        if (! confirmed)
        {
            outcome.status = Outcome::Status::needsConfirm;
            outcome.message = SearchCatalog::text ("search.needsPart", { { "part", SearchCatalog::text ("search.part.bass") }, { "category", "Body" } });
            return outcome;
        }

        openWorkshopOn (PartType::body, {});
        outcome.status = Outcome::Status::done;
        return outcome;
    }

    for (const auto& step : item.location.steps)
        if (step.type == LocationStep::Type::mode && step.name != "Either")
            if (! switchModeFor (step.name == "Advanced" ? Mode::advanced : Mode::easy, item.title, confirmed, outcome))
                return outcome;

    // An overlay that is not the target's comes down first (step 5).
    bool targetIsOverlay = false;

    for (const auto& step : item.location.steps)
        targetIsOverlay = targetIsOverlay || step.type == LocationStep::Type::overlay || step.type == LocationStep::Type::optionsPage;

    if (! targetIsOverlay && editor.overlayHost.isShowingOverlay())
        editor.overlayHost.dismiss();

    bool ok = true;

    for (const auto& step : item.location.steps)
        ok = runStep (step) && ok;

    editor.resized();

    // What to ring: the tagged component the place names, else its host.
    const auto placeTag = item.id.fromFirstOccurrenceOf ("place:", false, false);
    juce::Component* target = SearchAnchors::findPlace (editor, placeTag);
    juce::Rectangle<int> area;

    for (int i = 0; target == nullptr && i < editor.advancedPanel.getNumWorkspaceTabs(); ++i)
        if (auto* panel = editor.advancedPanel.getWorkspacePanel (i))
            target = SearchAnchors::findPlace (*panel, placeTag);

    if (placeTag == "overlay:workshop" && editor.advancedMode)
        target = workshopPanelShown();

    if (target == nullptr)
        for (auto* o : { overlayNamed ("options"), overlayNamed ("presetBrowser"), overlayNamed ("help"), overlayNamed ("export"),
                         overlayNamed ("chords"), overlayNamed ("debug"), overlayNamed ("saveAs"), overlayNamed ("workshop") })
            if (o != nullptr && (target = SearchAnchors::findPlace (*o, placeTag)) != nullptr)
                break;

    if (placeTag.startsWith ("section:"))
        if (auto* column = SearchAnchors::findPlace (editor.advancedPanel, "column:" + item.breadcrumb.getLastCharacters (1)))
            target = column;

    if (placeTag.startsWith ("easy:"))
    {
        auto& easy = editor.easyPanel;
        const auto strip = placeTag.substring (5);
        target = &easy;
        area = strip == "rig" ? easy.getRigArea() : strip == "playing" ? easy.getPlayingArea()
             : strip == "tone" ? easy.getToneArea() : strip == "rhythm" ? easy.getRhythmArea()
             : easy.getGuitar().getBoundsInParent();
    }

    if (placeTag.startsWith ("overlay:") && target == nullptr)
        target = editor.overlayHost.getCurrentOverlay();

    if (target == nullptr || ! ok)
    {
        outcome.status = ok ? Outcome::Status::done : Outcome::Status::failed;
        outcome.warning = ! ok;
        outcome.message = ok ? juce::String() : SearchCatalog::text ("search.navigateMiss", { { "name", item.title } });
        last.ok = ok;
        return outcome;
    }

    scrollIntoView (*target);
    focusAndHighlight (*target, area);

    last.ok = isOnScreen (*target);
    announce (SearchCatalog::text ("search.announce.target", { { "name", item.title }, { "breadcrumb", item.breadcrumb } }));

    outcome.status = Outcome::Status::done;
    return outcome;
}

//==============================================================================
SearchNavigator::Outcome SearchNavigator::activate (const SearchItem& item, ActivationKind kind, bool confirmed, int secondaryIndex)
{
    Outcome outcome;
    auto* provider = index.getProviderFor (item);
    const auto availability = provider != nullptr ? provider->availabilityOf (item) : Availability::available;

    if (availability == Availability::proLocked)
    {
        // 10: Enter opens the upsell. There is no upsell panel in this build
        // (editions.md is not built), so the palette says what the row means.
        outcome.status = Outcome::Status::refused;
        outcome.warning = true;
        outcome.message = SearchCatalog::text ("search.proLocked");
        return outcome;
    }

    if (availability == Availability::notBuilt)
    {
        outcome.status = Outcome::Status::refused;
        outcome.warning = true;
        outcome.message = SearchCatalog::text ("search.nothingHappened", { { "name", item.title } });
        return outcome;
    }

    if (kind == ActivationKind::secondary && provider != nullptr)
    {
        this->secondaryIndex = secondaryIndex;
        const bool ok = provider->activate (item, kind, *this);
        this->secondaryIndex = -1;

        outcome.status = ok ? Outcome::Status::done : Outcome::Status::failed;
        return outcome;
    }

    switch (item.kind)
    {
        case ItemKind::parameter:
        {
            outcome = goToParameterControl (item, confirmed);

            if (outcome.status == Outcome::Status::done && kind == ActivationKind::keepOpen)
                outcome.status = Outcome::Status::keptOpen;

            return outcome;
        }

        case ItemKind::choiceOption:
            if (kind == ActivationKind::goOnly)
                return goToParameterControl (item, confirmed);
            break;

        case ItemKind::place:
            return goToPlace (item, confirmed);

        case ItemKind::command:
            if (actions.find (item.target) == nullptr)
                break;

            // Commands run with the palette out of the way, so what they open
            // (an overlay, a menu) is in front.
            palette->close (false);
            outcome.status = editor.performAction (item.target) ? Outcome::Status::done : Outcome::Status::failed;

            if (outcome.status == Outcome::Status::failed)
                outcome.message = SearchCatalog::text ("search.nothingHappened", { { "name", item.title } });

            return outcome;

        default:
            break;
    }

    if (provider == nullptr)
    {
        outcome.status = Outcome::Status::failed;
        return outcome;
    }

    footerMessage.clear();
    const bool ok = provider->activate (item, kind, *this);

    outcome.status = ok ? (kind == ActivationKind::keepOpen ? Outcome::Status::keptOpen : Outcome::Status::done)
                        : Outcome::Status::failed;
    outcome.message = footerMessage.isNotEmpty() ? footerMessage
                    : ok ? juce::String() : SearchCatalog::text ("search.nothingHappened", { { "name", item.title } });
    outcome.warning = ! ok;
    return outcome;
}

SearchNavigator::Outcome SearchNavigator::applyValue (const ValueReading& reading)
{
    Outcome outcome;

    if (! reading.isValid())
        return outcome;

    auto* p = processor.getState().getParameter (reading.parameterId);

    if (p == nullptr)
        return outcome;

    auto* provider = index.getProviderFor (*reading.item);
    const auto availability = provider != nullptr ? provider->availabilityOf (*reading.item) : Availability::available;

    // 4.4 limits: refused for needs*, proLocked; modeUnavailable is allowed
    // (it is the fallback of 4.2 step 3).
    if (isContextGate (availability) || isLocked (availability))
    {
        outcome.status = Outcome::Status::refused;
        outcome.warning = true;
        outcome.message = SearchCatalog::text ("search.noInlineSet", { { "name", reading.item->title } });
        return outcome;
    }

    // A modulated parameter has its base value set, as a knob drag would.
    ChoiceOptionProvider::setAsGesture (*p, reading.resolved.normalised);

    const auto shown = InlineValue::displayText (*p, p->getValue());
    announce (shown);

    outcome.status = Outcome::Status::done;
    outcome.message = reading.resolved.clamped ? reading.resolved.clampText
                                               : SearchCatalog::text ("search.valueSet", { { "name", reading.item->title }, { "value", shown } });
    return outcome;
}

bool SearchNavigator::nudge (const juce::String& parameterId, int direction, bool fine)
{
    auto* p = processor.getState().getParameter (parameterId);

    if (p == nullptr)
        return false;

    if (auto* item = index.find ("param:" + parameterId))
        if (auto* provider = index.getProviderFor (*item))
            if (const auto a = provider->availabilityOf (*item); isContextGate (a) || isLocked (a))
                return false;

    const double now = juce::Time::getMillisecondCounterHiRes();

    // 4.4: nudges on one parameter within 200 ms are one gesture, so one undo
    // entry and one begin/end pair for the host.
    if (! nudgeGestureOpen || nudgeParameter != parameterId || now - lastNudgeMs > 200.0)
    {
        endNudgeGesture();
        p->beginChangeGesture();
        nudgeGestureOpen = true;
        nudgeParameter = parameterId;
    }

    const float step = InlineValue::arrowStep (*p, fine);
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + (float) direction * step));
    lastNudgeMs = now;

    startTimer (40);
    return true;
}

void SearchNavigator::endNudgeGesture()
{
    if (! nudgeGestureOpen)
        return;

    if (auto* p = processor.getState().getParameter (nudgeParameter))
        p->endChangeGesture();

    nudgeGestureOpen = false;
}

ValueReading SearchNavigator::readValue (const juce::String& rawQuery)
{
    return InlineValue::read (index, rawQuery,
                              [this] (const juce::String& id) { return processor.getState().getParameter (id); },
                              [this] (const juce::String& id) -> const PhysicalRange*
                              {
                                  return processor.getRanges().isParameterAdvanced (id) ? nullptr : RangeRegistry::find (id);
                              },
                              RangesUi::kLockedNoticeText);
}

juce::String SearchNavigator::valueTextFor (const SearchItem& item)
{
    if (item.kind != ItemKind::parameter && item.kind != ItemKind::choiceOption)
        return {};

    auto* p = processor.getState().getParameter (item.target);

    if (p == nullptr)
        return {};

    if (item.kind == ItemKind::choiceOption)
        return (int) std::round (p->convertFrom0to1 (p->getValue())) == item.index ? juce::String ("(current)") : juce::String();

    return InlineValue::displayText (*p, p->getValue());
}

juce::String SearchNavigator::subtitleFor (const SearchItem& item, Availability availability)
{
    switch (availability)
    {
        case Availability::needsSlideMode:  return SearchCatalog::text ("search.needsSlideMode");
        case Availability::needsBass:       return SearchCatalog::text ("search.needsPart", { { "part", SearchCatalog::text ("search.part.bass") }, { "category", "Body" } });
        case Availability::needsWhammy:     return SearchCatalog::text ("search.needsPart", { { "part", SearchCatalog::text ("search.part.whammy") }, { "category", "Bridge" } });
        case Availability::needsAcoustic:   return SearchCatalog::text ("search.needsPart", { { "part", SearchCatalog::text ("search.part.acoustic") }, { "category", "Body" } });
        case Availability::needsEmptySlot:  return SearchCatalog::text ("search.needsEmptySlot");
        case Availability::proLocked:       return SearchCatalog::text ("search.proLocked");
        case Availability::modeUnavailable: return SearchCatalog::text (processor.isLiveMode() ? "search.advancedLocked" : "search.advancedUnavailable");
        case Availability::notBuilt:
        case Availability::available:
        default: break;
    }

    if ((item.kind == ItemKind::parameter || item.kind == ItemKind::place) && ! editor.advancedMode && item.inAdvanced && ! item.inEasy)
        return SearchCatalog::text ("search.opensInAdvanced");

    return {};
}

std::vector<const SearchItem*> SearchNavigator::getSuggestions()
{
    std::vector<const SearchItem*> out;
    rebuildControlMap();

    // Three controls from the view in use (the tab or the Easy strip on show).
    juce::Component* view = editor.advancedMode
                              ? editor.advancedPanel.getWorkspacePanel (editor.advancedPanel.getWorkspaceTab())
                              : static_cast<juce::Component*> (&editor.easyPanel);

    if (view != nullptr)
        for (auto& [id, controls] : controlMap)
        {
            if ((int) out.size() >= 3)
                break;

            for (auto* c : controls)
                if (view->isParentOf (c) && isOnScreen (*c))
                {
                    if (auto* item = index.find ("param:" + id))
                        out.push_back (item);

                    break;
                }
        }

    for (const char* id : { "cmd:presetBrowser", "cmd:openWorkshop", "param:tuning_preset", "cmd:showShortcuts" })
        if (auto* item = index.find (id))
            out.push_back (item);

    return out;
}

//==============================================================================
// SearchServices

bool SearchNavigator::goToParameter (const SearchItem& item, ActivationKind)
{
    return goToParameterControl (item, false).status == Outcome::Status::done;
}

bool SearchNavigator::openPlace (const SearchItem& item)
{
    return goToPlace (item, false).status == Outcome::Status::done;
}

bool SearchNavigator::openHelpTopic (const juce::String& topicId, bool pinnedInTab)
{
    if (pinnedInTab && ! editor.advancedMode)
    {
        Outcome unused;

        if (switchModeFor (Mode::advanced, "Help", true, unused))
        {
            editor.advancedPanel.showHelp (topicId);
            return true;
        }
    }

    editor.openHelp (topicId);
    return true;
}

bool SearchNavigator::openWorkshopOn (PartType type, const juce::String& partName)
{
    // gui-integration 6 / global-search 12: the tab in Advanced, the overlay in Easy.
    if (editor.overlayHost.isShowingOverlay() && editor.overlayHost.getCurrentOverlay() != &editor.workshopOverlay)
        editor.overlayHost.dismiss();

    performExtendedAction ("openWorkshop");
    editor.resized();

    auto* workshop = dynamic_cast<WorkshopPanel*> (workshopPanelShown());

    if (workshop == nullptr)
        return false;

    workshop->showCategory (workshopCategoryFor (type));

    // The card focused: the drawer's hover highlight, without Alt (no audition).
    const auto& parts = workshop->getDrawerParts();

    for (int i = 0; i < parts.size(); ++i)
        if (parts[i] != nullptr && parts[i]->name == partName)
        {
            workshop->hoverCard (i, false);
            break;
        }

    focusAndHighlight (*workshop);
    last.ok = true;
    announce (SearchCatalog::text ("search.announce.target", { { "name", partName.isNotEmpty() ? partName : workshopCategoryFor (type) },
                                                               { "breadcrumb", "Workshop" + kSep + workshopCategoryFor (type) } }));
    return true;
}

bool SearchNavigator::openShortcutRow (const juce::String& actionId)
{
    editor.showOverlay (&editor.optionsPanel);
    editor.optionsPanel.showShortcutTable();

    if (auto* page = findFirst<AccessibilityPage> (editor.optionsPanel))
    {
        if (auto* filter = findFirst<juce::TextEditor> (*page))
        {
            juce::String text = actionId;

            if (const auto* b = AccessibilitySettings::get().findShortcut (actionId))
                text = tr (b->descriptionKey);

            filter->setText (text, juce::sendNotificationSync);
        }

        if (auto* list = findFirst<juce::ListBox> (*page))
            focusAndHighlight (*list);
    }

    return true;
}

std::vector<SearchServices::Setting> SearchNavigator::getSettings()
{
    std::vector<Setting> settings;
    juce::StringArray seen;

    for (const auto& place : SearchAnchors::allPlaces (editor.optionsPanel))
    {
        if (! place.placeId.startsWith ("options:"))
            continue;

        const auto page = place.placeId.substring (8);
        juce::Array<juce::Component*> all;
        collectAll<juce::Component> (*place.component, all);

        for (auto* c : all)
        {
            juce::String id = SearchAnchors::getSetting (*c), title;

            if (id.isNotEmpty())
            {
                title = SearchCatalog::text (SearchAnchors::getSettingTitleKey (*c));

                if (title.isEmpty())
                    title = tr (SearchAnchors::getSettingTitleKey (*c));
            }
            else
            {
                // Discovered: a labelled toggle, box or slider that is not a
                // parameter control (those are indexed as parameters).
                if (dynamic_cast<LearnTarget*> (c) != nullptr || isInsideLearnTarget (*c))
                    continue;

                if (auto* toggle = dynamic_cast<juce::ToggleButton*> (c))
                    title = toggle->getButtonText();
                else if (auto* tb = dynamic_cast<juce::TextButton*> (c); tb != nullptr && tb->getClickingTogglesState()
                                                                          && tb->getRadioGroupId() == 0)
                    title = tb->getButtonText();
                else if (dynamic_cast<juce::ComboBox*> (c) != nullptr || dynamic_cast<juce::Slider*> (c) != nullptr)
                    title = c->getTitle();

                title = title.trim();

                if (title.isEmpty())
                    continue;

                id = "set:" + page + ":" + title.replaceCharacters (" /:", "___").toLowerCase();
            }

            if (seen.contains (id))
                continue;

            seen.add (id);
            settings.push_back ({ id, title, page });
        }
    }

    return settings;
}

bool SearchNavigator::openSetting (const SearchItem& item)
{
    const auto page = item.target;

    if (! editor.optionsPanel.showPageNamed (page))
        return false;

    editor.showOverlay (&editor.optionsPanel);

    if (auto* host = SearchAnchors::findPlace (editor.optionsPanel, "options:" + page))
    {
        juce::Array<juce::Component*> all;
        collectAll<juce::Component> (*host, all);

        for (auto* c : all)
        {
            juce::String id = SearchAnchors::getSetting (*c);

            if (id.isEmpty())
            {
                juce::String title;

                if (auto* toggle = dynamic_cast<juce::ToggleButton*> (c))       title = toggle->getButtonText();
                else if (auto* tb = dynamic_cast<juce::TextButton*> (c))        title = tb->getButtonText();
                else                                                            title = c->getTitle();

                id = "set:" + page + ":" + title.trim().replaceCharacters (" /:", "___").toLowerCase();
            }

            if (id == item.id)
            {
                scrollIntoView (*c);
                c->setWantsKeyboardFocus (true);
                requestFocus (c);
                highlighter.flash (c);
                last.focused = c;
                last.control = c;
                last.ok = true;
                announce (SearchCatalog::text ("search.announce.target", { { "name", item.title }, { "breadcrumb", item.breadcrumb } }));
                return true;
            }
        }
    }

    return true;
}

bool SearchNavigator::showPresetInBrowser (int)
{
    editor.showOverlay (&editor.presetBrowser);
    return true;
}

//==============================================================================
// SearchContext

bool SearchNavigator::openLocation (const UiLocation& location, const juce::String& announceAs)
{
    Outcome unused;
    bool ok = true;

    for (const auto& step : location.steps)
    {
        if (step.type == LocationStep::Type::mode && step.name != "Either")
            ok = switchModeFor (step.name == "Advanced" ? Mode::advanced : Mode::easy, announceAs, true, unused) && ok;
        else
            ok = runStep (step) && ok;
    }

    editor.resized();
    announce (announceAs);
    return ok;
}

bool SearchNavigator::performAction (const juce::String& actionId)
{
    if (actionId.startsWith ("__"))
        return performExtendedAction (actionId);

    return editor.performAction (actionId);
}

void SearchNavigator::showFooterMessage (const juce::String& text, bool warning)
{
    footerMessage = text;

    if (palette->isOpen())
        palette->setFooter (text, warning);
}

void SearchNavigator::postNotice (const juce::String& text)
{
    if (text.isEmpty())
        return;

    lastNotice = text;
    editor.inlineNotice.show (text);
    editor.resized();
}

} // namespace luthier::search
