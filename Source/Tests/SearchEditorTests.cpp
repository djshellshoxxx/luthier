/*  Global search in the real editor (global-search.md 15): the "nothing is
    missed" tests GS-01 to GS-06, inline undo and refusals (GS-21, GS-24), the
    palette (GS-30 to GS-39), and the combinations (GS-40, GS-42, GS-43,
    GS-45).

    The editor is built as GuiReachabilityTests builds it, at 1600 x 1000 and
    never put on the desktop: under xvfb a desktop peer cannot be created (the
    X server rejects the window's atoms), so nothing can hold real keyboard
    focus. Focus is checked as the request search made
    (SearchNavigator::getLastFocusRequest), and asynchronous steps are pumped
    by hand (SearchNavigator::pumpPending, SearchHighlighter::advance).

    LUTHIER_COMBO_VERBOSE=1 prints every GS-02 navigation.
*/

#include "ComboHarness.h"

#include "../PluginEditor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"
#include "../Presets/PresetManager.h"
#include "../UI/HeaderBar.h"
#include "../UI/HelpContent.h"
#include "../UI/OptionsPages.h"
#include "../UI/Overlays.h"
#include "../UI/UiPreferences.h"
#include "../UI/Widgets.h"
#include "../UI/Search/CommandPalette.h"
#include "../UI/Search/LiveControls.h"
#include "../UI/Search/ParameterVisibility.h"
#include "../UI/Search/SearchCatalog.h"
#include "../UI/Search/SearchNavigator.h"
#include "../UI/Search/SearchOptionsGroup.h"
#include "../Tune/TuneTemplates.h"

#if defined (LUTHIER_ALLOCATION_COUNTER)
namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;
}
#endif

using namespace luthier;
using namespace luthier::tests;
using namespace luthier::combo;
using namespace luthier::search;

namespace
{
    struct CleanRecent
    {
        CleanRecent()
        {
            wasOn = RecentStore::get().isEnabled();
            wasAuto = isAutoSwitchModeOn();
            RecentStore::get().setEnabled (true);
            RecentStore::get().clear();
            setAutoSwitchMode (true);
        }

        ~CleanRecent()
        {
            RecentStore::get().clear();
            RecentStore::get().setEnabled (wasOn);
            setAutoSwitchMode (wasAuto);
        }

        bool wasOn = true, wasAuto = true;
    };

    /** A processor in a context, and its editor. */
    struct Win
    {
        explicit Win (const std::function<void (Rig&)>& setup = {}, int width = 1600, int height = 1000)
        {
            rig.p().resetEverything();

            if (setup)
                setup (rig);

            rig.apply();

            owner.reset (rig.p().createEditor());
            ed = dynamic_cast<LuthierAudioProcessorEditor*> (owner.get());
            ed->setVisible (true);
            ed->setSize (width, height);
            nav = &ed->getSearch();
        }

        ~Win()
        {
            nav->getPalette().close (false);
            owner.reset();
        }

        LuthierAudioProcessor& p() { return rig.p(); }
        CommandPalette& palette()  { return nav->getPalette(); }

        bool isAdvanced()
        {
            for (auto* c : ed->getChildren())
                if (auto* h = dynamic_cast<HeaderBar*> (c))
                    return h->isAdvancedMode();

            return false;
        }

        void setAdvanced (bool advanced)
        {
            if (isAdvanced() != advanced)
                ed->performAction ("toggleAdvanced");
        }

        HeaderBar& header()
        {
            for (auto* c : ed->getChildren())
                if (auto* h = dynamic_cast<HeaderBar*> (c))
                    return *h;

            jassertfalse;
            return *dynamic_cast<HeaderBar*> (ed->getChildComponent (0));
        }

        const SearchItem* item (const juce::String& id) { return nav->getIndex().find (id); }

        Availability availability (const SearchItem& i)
        {
            auto* provider = nav->getIndex().getProviderFor (i);
            return provider != nullptr ? provider->availabilityOf (i) : Availability::available;
        }

        /** Visible all the way up to the editor, with a size. */
        bool onScreen (juce::Component* c)
        {
            if (c == nullptr || c->getWidth() <= 0 || c->getHeight() <= 0)
                return false;

            for (auto* x = c; x != nullptr; x = x->getParentComponent())
            {
                if (! x->isVisible())
                    return false;

                if (x == ed)
                    return true;
            }

            return false;
        }

        juce::MemoryBlock state()
        {
            juce::MemoryBlock block;
            p().getStateInformation (block);
            return block;
        }

        Rig rig;
        std::unique_ptr<juce::AudioProcessorEditor> owner;
        LuthierAudioProcessorEditor* ed = nullptr;
        SearchNavigator* nav = nullptr;
    };

    void setChoiceContaining (Rig& r, const juce::String& id, std::initializer_list<const char*> needles)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (r.param (id)))
            for (int i = 0; i < c->choices.size(); ++i)
                for (auto* n : needles)
                    if (c->choices[i].containsIgnoreCase (n))
                    {
                        r.setIndex (id, i);
                        return;
                    }
    }

    /** Counts gestures and writes on one parameter (GS-21: the host's view). */
    struct GestureCounter : juce::AudioProcessorParameter::Listener
    {
        explicit GestureCounter (juce::AudioProcessorParameter& p) : parameter (p) { parameter.addListener (this); }
        ~GestureCounter() override { parameter.removeListener (this); }

        void parameterValueChanged (int, float) override { ++writes; }
        void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }

        juce::AudioProcessorParameter& parameter;
        int begins = 0, ends = 0, writes = 0;
    };

    juce::MouseEvent mouseEventAt (juce::Component& c, juce::Point<int> at, juce::ModifierKeys mods = {})
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        const auto p = at.toFloat();
        return juce::MouseEvent (source, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c,
                                 juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false);
    }

    juce::StringArray menuTexts (const juce::PopupMenu& menu)
    {
        juce::StringArray texts;

        for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
            texts.add (it.getItem().text + "#" + juce::String (it.getItem().itemID));

        return texts;
    }
}

//==============================================================================
/*  GS-01: every automatable parameter not intentionally hidden is exactly one
    param: item with a title. */
LUTHIER_TEST (SearchEditor, GS01_everyParameterIndexed)
{
    CleanRecent clean;
    Win w;

    int total = 0;
    juce::StringArray missing;

    for (auto* prm : w.p().getParameters())
    {
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm);

        if (r == nullptr || ParameterVisibility::isIntentionallyHidden (r->getParameterID()))
            continue;

        ++total;
        const auto* item = w.item ("param:" + r->getParameterID());

        if (item == nullptr || item->title.trim().isEmpty())
            missing.add (r->getParameterID());
    }

    for (auto& id : missing)
        CHECK_MSG (false, id + " is not indexed with a title");

    int paramItems = 0;

    for (auto* item : w.nav->getIndex().getItems())
        if (item->kind == ItemKind::parameter)
            ++paramItems;

    CHECK_MSG (paramItems == total, juce::String (paramItems) + " param items for " + juce::String (total) + " parameters");
    CHECK_MSG (w.nav->getIndex().getDuplicateLog().isEmpty(), w.nav->getIndex().getDuplicateLog().joinIntoString (", "));

    for (const auto& [id, reason] : ParameterVisibility::intentionallyHidden())
        CHECK_MSG (w.item ("param:" + id) == nullptr, id + " is hidden on purpose and must not be indexed");

    std::cout << "    parameters indexed: " << paramItems << std::endl;
}

//==============================================================================
/*  GS-02: every indexed parameter navigates to an on-screen control of its
    own, focused and ringed, in each context; a gated one says so instead. */
LUTHIER_TEST (SearchEditor, GS02_everyParameterNavigates)
{
    CleanRecent clean;

    struct Context { const char* name; std::function<void (Rig&)> setup; };

    std::vector<Context> contexts
    {
        { "default", [] (Rig&) {} },
        { "bass",    [] (Rig& r) { setChoiceContaining (r, ParamIDs::guitarType, { "bass" }); } },
        { "whammy",  [] (Rig& r) { setChoiceContaining (r, ParamIDs::bridgeType, { "trem", "whammy", "floyd", "bigsby" }); } },
        { "slide",   [] (Rig& r) { r.setIndex (ParamIDs::slideGuitar, 1); } },

        // Beyond the spec's four: every slot filled, so each pedal's knobs are live.
        { "pedals",  [] (Rig& r)
            {
                const int types = r.numChoices (ParamIDs::slotType (false, 0));

                for (int chain = 0; chain < 2; ++chain)
                    for (int s = 0; s < EffectsChain::kNumSlots; ++s)
                        r.setIndex (ParamIDs::slotType (chain == 1, s), 1 + ((chain * EffectsChain::kNumSlots + s) % juce::jmax (1, types - 1)));
            } },
    };

    juce::StringArray failures, noControl, zeroSize;
    int navigated = 0, gated = 0;

    for (auto& context : contexts)
    {
        Win w (context.setup);
        w.setAdvanced (true);

        for (auto* item : w.nav->getIndex().getItems())
        {
            if (item->kind != ItemKind::parameter || item->hiddenByDefault)
                continue;

            const auto availability = w.availability (*item);
            const auto outcome = w.nav->goToParameterControl (*item, false);

            for (int i = 0; i < 3 && w.nav->getLastNavigation().pending; ++i)
                w.nav->pumpPending();

            const auto& last = w.nav->getLastNavigation();
            const auto where = juce::String (context.name) + ": " + item->target;

            if (verbose())
                std::cout << "      " << where << " -> " << (int) outcome.status << " " << outcome.message << std::endl;

            if (isContextGate (availability))
            {
                if (outcome.status == SearchNavigator::Outcome::Status::needsConfirm)
                    ++gated;
                else
                    failures.add (where + " is gated but navigation did not say so");

                continue;
            }

            if (outcome.status != SearchNavigator::Outcome::Status::done || ! last.ok)
            {
                bool dummy = false;

                // A parameter with no control anywhere is GuiReachabilityTests'
                // failure (a missing panel), not the navigator's.
                if (w.nav->chooseControl (item->target, dummy) == nullptr && LiveControls::find (item->target).empty())
                    noControl.addIfNotAlreadyThere (item->target);
                else
                {
                    juce::String why;

                    if (auto* c = w.nav->chooseControl (item->target, dummy))
                    {
                        why << " [" << c->getBounds().toString() << " in " << typeid (*c->getParentComponent()).name();

                        for (auto* x = c; x != nullptr; x = x->getParentComponent())
                            if (! x->isVisible() || x->getWidth() <= 0 || x->getHeight() <= 0)
                            {
                                why << ", hidden/empty: " << typeid (*x).name() << " " << x->getBounds().toString();
                                break;
                            }

                        why << "]";
                    }

                    // A control whose own panel lays it out at zero size is
                    // GuiReachabilityTests' "hidden control only" - a panel's
                    // layout, not the route to it.
                    if (why.contains ("hidden/empty") && ! why.contains ("hidden/empty: N7luthier11EditorBridge"))
                    {
                        bool zeroSizeOnly = true;

                        if (auto* c = w.nav->chooseControl (item->target, dummy))
                            for (auto* x = c; x != nullptr; x = x->getParentComponent())
                                zeroSizeOnly = zeroSizeOnly && x->isVisible();

                        if (zeroSizeOnly)
                        {
                            zeroSize.addIfNotAlreadyThere (item->target);
                            continue;
                        }
                    }

                    failures.add (where + ": " + outcome.message + why);
                }

                continue;
            }

            auto* control = last.control.getComponent();
            auto* learn = dynamic_cast<LearnTarget*> (control);

            if (learn == nullptr || learn->getLearnParameterId() != item->target)
                failures.add (where + ": landed on the wrong control");
            else if (! w.onScreen (control))
                failures.add (where + ": control not on screen");
            else if (w.nav->getLastFocusRequest() != LiveControls::innerControl (*control))
                failures.add (where + ": focus not requested on the control");
            else if (w.nav->getHighlighter().getTarget() != control || w.nav->getHighlighter().getRingBounds().isEmpty())
                failures.add (where + ": not highlighted");
            else
                ++navigated;
        }
    }

    std::cout << "    navigated: " << navigated << ", gated as expected: " << gated
              << ", no control anywhere (GuiReach): " << noControl.size() << std::endl;

    if (noControl.size() > 0)
        std::cout << "    NO CONTROL: " << noControl.joinIntoString (" ") << std::endl;

    if (zeroSize.size() > 0)
        std::cout << "    ZERO SIZE IN ITS PANEL (GuiReach: hidden control only): " << zeroSize.joinIntoString (" ") << std::endl;

    for (auto& f : failures)
        CHECK_MSG (false, f);

    CHECK (navigated > 500);
}

//==============================================================================
/*  GS-03: places, both directions. */
LUTHIER_TEST (SearchEditor, GS03_everyPlaceIndexedAndReal)
{
    CleanRecent clean;
    Win w;
    w.setAdvanced (true);

    auto has = [&w] (const juce::String& id) { return w.item (id) != nullptr; };

    // Real surfaces -> catalogue.
    AdvancedPanel* advanced = nullptr;
    OptionsPanel* options = nullptr;

    std::function<void (juce::Component&)> find = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* a = dynamic_cast<AdvancedPanel*> (child)) advanced = a;
            find (*child);
        }
    };

    find (*w.ed);

    for (auto* o : w.nav->getOwnedOverlays())
        if (auto* op = dynamic_cast<OptionsPanel*> (o))
            options = op;

    CHECK (advanced != nullptr && options != nullptr);

    if (advanced == nullptr || options == nullptr)
        return;

    juce::StringArray realTabs, realPages, realSections, realDrawer, realOverlays, realGroups;

    for (int i = 0; i < advanced->getNumWorkspaceTabs(); ++i)
        realTabs.add (advanced->getWorkspaceTabName (i));

    realPages = options->getPageNames();

    for (int c = 1; c <= 3; ++c)
        realSections.addArray (advanced->getColumnSections (c));

    for (int t = 0; t < (int) PracticeTool::numTools; ++t)
        realDrawer.add (getPracticeToolTabLabel ((PracticeTool) t));

    for (auto* o : w.nav->getOwnedOverlays())
    {
        const auto tag = SearchAnchors::getPlace (*o);
        CHECK_MSG (tag.startsWith ("overlay:"), "an owned overlay is not tagged");
        realOverlays.add (tag.substring (8));
    }

    for (int i = 0; i < advanced->getNumWorkspaceTabs(); ++i)
        for (const auto& step : SearchAnchors::allPlaces (*advanced->getWorkspacePanel (i)))
            if (step.placeId.startsWith ("group:"))
                realGroups.add (step.placeId.substring (6));

    for (auto& t : realTabs)     CHECK_MSG (has ("place:tab:" + t), "tab " + t + " has no place item");
    for (auto& p : realPages)    CHECK_MSG (has ("place:options:" + p), "Options page " + p + " has no place item");
    for (auto& s : realSections) CHECK_MSG (has ("place:section:" + s.toUpperCase()), "section " + s + " has no place item");
    for (auto& d : realDrawer)   CHECK_MSG (has ("place:drawer:" + d), "drawer tab " + d + " has no place item");
    for (auto& g : realGroups)   CHECK_MSG (has ("place:group:" + g), "group " + g + " has no place item");

    for (auto& o : realOverlays)
        if (o != "secret")   // include.md's hidden effect stays hidden (DECISIONS in docs/coverage/FEAT-SEARCH.md)
            CHECK_MSG (has ("place:overlay:" + o), "overlay " + o + " has no place item");

    // Catalogue -> real surfaces (no dangling places), and each one opens.
    int opened = 0;

    for (const auto& place : PlaceProvider::catalogue())
    {
        const auto tag = place.id.fromFirstOccurrenceOf ("place:", false, false);
        const auto kind = tag.upToFirstOccurrenceOf (":", false, false);
        const auto name = tag.fromFirstOccurrenceOf (":", false, false);

        bool real = true;

        if (kind == "tab")          real = realTabs.contains (name);
        else if (kind == "options") real = realPages.contains (name);
        else if (kind == "drawer")  real = realDrawer.contains (name);
        else if (kind == "overlay") real = realOverlays.contains (name);
        else if (kind == "group")   real = realGroups.contains (name);
        else if (kind == "column")  real = name.getIntValue() >= 1 && name.getIntValue() <= 3;
        else if (kind == "easy")    real = juce::StringArray { "illustration", "playing", "tone", "rhythm", "rig" }.contains (name);
        else if (kind == "section")
        {
            real = false;

            for (auto& s : realSections)
                real = real || s.toUpperCase() == name;
        }

        CHECK_MSG (real, place.id + " names no real surface");

        const auto* item = w.item (place.id);

        if (item == nullptr)
            continue;

        const auto availability = w.availability (*item);
        const auto outcome = w.nav->goToPlace (*item, false);

        if (isContextGate (availability))
        {
            CHECK_MSG (outcome.status == SearchNavigator::Outcome::Status::needsConfirm, place.id + " is gated but did not say so");
            continue;
        }

        CHECK_MSG (outcome.status == SearchNavigator::Outcome::Status::done, place.id + ": " + outcome.message);

        auto* target = w.nav->getLastNavigation().control.getComponent();
        const bool visible = target != nullptr && (w.onScreen (target) || (target->isVisible() && target->isShowing()));

        if (target != nullptr && ! w.onScreen (target))
        {
            bool shownButEmpty = target->getHeight() <= 0 || target->getWidth() <= 0;

            for (auto* x = target; x != nullptr && x != w.ed; x = x->getParentComponent())
                shownButEmpty = shownButEmpty && x->isVisible();

            // Laid out at zero size by its own panel: GuiReachabilityTests'
            // "hidden control only" (see GS-02), not the route to it.
            if (shownButEmpty)
                std::cout << "    ZERO SIZE IN ITS PANEL: " << place.id << std::endl;
            else
                CHECK_MSG (false, place.id + " did not become visible");
        }
        else if (visible)
        {
            ++opened;
        }
    }

    std::cout << "    places opened and visible: " << opened << " of " << PlaceProvider::catalogue().size() << std::endl;
}

//==============================================================================
/*  GS-04: every shortcut is a command and a key item, and the palette and the
    key do the same thing. */
LUTHIER_TEST (SearchEditor, GS04_shortcutsAreCommands)
{
    CleanRecent clean;

    {
        Win w;

        for (const auto& b : AccessibilitySettings::get().getShortcuts())
        {
            CHECK_MSG (w.item ("cmd:" + b.id) != nullptr, "no cmd:" + b.id);
            CHECK_MSG (w.item ("key:" + b.id) != nullptr, "no key:" + b.id);
        }
    }

    for (const char* id : { "panic", "undo", "toggleSlideMode", "newPreset", "toggleAdvanced", "togglePractice" })
    {
        auto prepare = [] (Win& w)
        {
            w.setAdvanced (false);

            // Something to undo, so "undo" does something.
            if (auto* p = w.p().getState().getParameter (ParamIDs::ampGain))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (0.8f);
                p->endChangeGesture();
            }
        };

        Win byKey, byPalette;
        prepare (byKey);
        prepare (byPalette);

        const auto* binding = AccessibilitySettings::get().findShortcut (id);
        CHECK (binding != nullptr);

        if (binding == nullptr)
            continue;

        CHECK_MSG (byKey.ed->keyPressed (binding->key), juce::String (id) + " key did nothing");

        const auto* item = byPalette.item (juce::String ("cmd:") + id);
        CHECK (item != nullptr);

        if (item == nullptr)
            continue;

        byPalette.nav->openPalette();
        const auto outcome = byPalette.nav->activate (*item, ActivationKind::primary, false);
        CHECK_MSG (outcome.status == SearchNavigator::Outcome::Status::done, juce::String (id) + ": " + outcome.message);

        CHECK_MSG (byKey.p().getNumUndoSteps() == byPalette.p().getNumUndoSteps(), juce::String (id) + ": undo depth differs");
        CHECK_MSG (byKey.state() == byPalette.state(), juce::String (id) + ": processor state differs");
        CHECK_MSG (byKey.isAdvanced() == byPalette.isAdvanced(), juce::String (id) + ": mode differs");
    }
}

//==============================================================================
/*  GS-05: every Help topic, by every alias. */
LUTHIER_TEST (SearchEditor, GS05_helpTopicsAndAliases)
{
    CleanRecent clean;
    Win w;

    int aliases = 0, anywhereTop3 = 0;

    for (int i = 0; i < HelpContent::getNumTopics(); ++i)
    {
        const auto& topic = HelpContent::getTopic (i);
        const auto id = juce::String ("help:") + topic.id;
        CHECK_MSG (w.item (id) != nullptr, id + " is not indexed");

        for (const auto& alias : HelpContent::getAliases (topic))
        {
            ++aliases;

            auto inTop3 = [&] (SearchIndex::Scope scope)
            {
                const auto r = w.nav->getIndex().query (alias, scope);

                for (int k = 0; k < juce::jmin (3, (int) r.size()); ++k)
                    if (r[(size_t) k].item->id == id)
                        return true;

                return false;
            };

            // In the Help scope ("? alias", the chip) a topic's alias always
            // finds it; across everything a control of the same name may
            // rightly come first, which is counted rather than failed.
            CHECK_MSG (inTop3 (SearchIndex::Scope::help), "'" + alias + "' does not find " + id);
            anywhereTop3 += inTop3 (SearchIndex::Scope::all) ? 1 : 0;
        }
    }

    std::cout << "    aliases: " << aliases << ", topic in the top 3 of an unscoped search: " << anywhereTop3 << std::endl;
}

//==============================================================================
/*  GS-06: presets, factory guitars and parts; a new preset appears; a deleted
    one says so and leaves the recent items. */
LUTHIER_TEST (SearchEditor, GS06_contentIndexedAndFresh)
{
    CleanRecent clean;
    Win w;
    auto& presets = w.p().getPresetManager();

    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (const auto* info = presets.getPreset (i))
            CHECK_MSG (w.item (PresetProvider::idFor (info->name, info->isFactory)) != nullptr, "preset " + info->name);

    for (const auto& f : PartLibrary::getFactoryGuitarsFolder().findChildFiles (juce::File::findFiles, true, "*.luthierguitar"))
        CHECK_MSG (w.item ("guitar:Factory/" + f.getRelativePathFrom (PartLibrary::getFactoryGuitarsFolder()).replaceCharacter ('\\', '/')) != nullptr,
                   "guitar " + f.getFileName());

    int parts = 0;

    for (int t = 0; t < (int) PartType::numTypes; ++t)
        for (const auto& part : w.p().getPartLibrary().getParts ((PartType) t))
        {
            ++parts;
            CHECK_MSG (w.item ("part:" + juce::String (getPartTypeId ((PartType) t)) + "/" + part->name) != nullptr, "part " + part->name);
        }

    std::cout << "    presets " << presets.getNumPresets() << ", parts " << parts << std::endl;

    // global-search.md 12: genre kits and tune files.
    auto& kits = w.p().getGenreKits();

    for (int i = 0; i < kits.getNumKits(); ++i)
        CHECK_MSG (w.item ("kit:" + kits.getKit (i).name) != nullptr, "genre kit " + kits.getKit (i).name);

    for (int i = 0; i < kits.getNumKits(); ++i)
        if (kits.getKit (i).name.containsIgnoreCase ("funk"))
        {
            bool found = false;

            for (const auto& r : w.nav->getIndex().query ("funk"))
                found = found || r.item->id == "kit:" + kits.getKit (i).name;

            CHECK_MSG (found, "'funk' does not find " + kits.getKit (i).name);
            break;
        }

    int tunes = 0;

    for (auto* item : w.nav->getIndex().getItems())
        tunes += item->id.startsWith ("tune:") ? 1 : 0;

    CHECK (tunes >= (int) TuneTemplateLibrary::loadFactory().size());

    // A new user preset appears on the next query.
    const juce::String name ("Search Test Preset " + juce::String (juce::Random::getSystemRandom().nextInt (100000)));
    CHECK (presets.saveAs (name, "Test"));
    presets.refresh();

    const auto found = w.nav->getIndex().query (name);
    CHECK_MSG (! found.empty() && found.front().item->id == PresetProvider::idFor (name, false), "the new preset was not found");

    // Deleting it: "no longer exists", and the recent entry goes.
    const auto id = PresetProvider::idFor (name, false);
    w.nav->getIndex().recordActivation (id, name);
    CHECK (RecentStore::get().find (id) != nullptr);

    SearchItem stale = *w.item (id);
    const int index = presets.indexOfPreset (name);
    CHECK (index >= 0 && presets.deletePreset (index));
    presets.refresh();

    w.nav->openPalette();
    const auto outcome = w.nav->activate (stale, ActivationKind::primary, false);
    CHECK (outcome.status != SearchNavigator::Outcome::Status::done);
    CHECK_MSG (outcome.message.contains ("no longer exists"), outcome.message);
    CHECK (RecentStore::get().find (id) == nullptr);
    CHECK (w.item (id) == nullptr);
}

//==============================================================================
/*  GS-21: one inline set is one undo entry and one gesture; nudges within
    200 ms merge. */
LUTHIER_TEST (SearchEditor, GS21_inlineSetUndoAndGestures)
{
    CleanRecent clean;
    Win w;
    auto* gain = w.p().getState().getParameter (ParamIDs::ampGain);
    const float before = gain->getValue();
    GestureCounter counter (*gain);

    const int depth = w.p().getNumUndoSteps();
    const auto reading = w.nav->readValue ("gain 7");
    CHECK (reading.isValid());
    CHECK (w.nav->applyValue (reading).status == SearchNavigator::Outcome::Status::done);

    CHECK_NEAR (gain->getValue(), 0.7, 1.0e-4);
    CHECK (w.p().getNumUndoSteps() == depth + 1);
    CHECK (counter.begins == 1 && counter.ends == 1);
    CHECK (w.p().getUndoDescription().contains (gain->getName (64)));

    w.p().undo();
    CHECK_NEAR (gain->getValue(), before, 1.0e-4);

    // Five nudges inside 200 ms: one gesture, one entry.
    const int depth2 = w.p().getNumUndoSteps();
    counter.begins = counter.ends = 0;

    for (int i = 0; i < 5; ++i)
        CHECK (w.nav->nudge (ParamIDs::ampGain, 1, false));

    w.nav->endNudgeGesture();
    CHECK (counter.begins == 1 && counter.ends == 1);
    CHECK (w.p().getNumUndoSteps() == depth2 + 1);
    CHECK_NEAR (gain->getValue(), before + 0.05, 1.0e-3);
}

//==============================================================================
/*  GS-24: no inline set where it would do nothing, or is not sold. */
LUTHIER_TEST (SearchEditor, GS24_inlineSetRefusals)
{
    CleanRecent clean;
    Win w ([] (Rig& r) { setChoiceContaining (r, ParamIDs::bridgeType, { "hardtail", "fixed", "tune-o", "wraparound" }); });

    CHECK (! ParameterLocations::isWhammyFitted (w.p()));

    auto refused = [&w] (const juce::String& query, const juce::String& id)
    {
        auto* p = w.p().getState().getParameter (id);
        const float before = p->getValue();
        const auto reading = w.nav->readValue (query);

        if (! reading.isValid())
            return true;   // no value reading at all is a refusal too

        const auto outcome = w.nav->applyValue (reading);
        return outcome.status == SearchNavigator::Outcome::Status::refused && p->getValue() == before;
    };

    CHECK_MSG (refused ("whammy down range 3", ParamIDs::whammyDown), "whammy on a hardtail");
    CHECK_MSG (refused ("slide pressure 50%", ParamIDs::slidePressure), "slide with Slide Mode off");

    // A Free build: circuit parameters are Pro here.
    w.nav->proLockPredicate = [] (const SearchItem& i) { return i.target.startsWith ("circuit_"); };
    w.nav->getIndex().invalidate();
    CHECK_MSG (refused ("treble bleed off", ParamIDs::circuitTrebleBleed), "a Pro parameter in Free");
}

//==============================================================================
/*  GS-30: Ctrl+K, the magnifier, Escape; focus comes back; the state is
    untouched by searching and navigating. */
LUTHIER_TEST (SearchEditor, GS30_openCloseFocusAndState)
{
    CleanRecent clean;
    Win w;
    w.setAdvanced (true);

    const auto* binding = AccessibilitySettings::get().findShortcut ("search");
    CHECK (binding != nullptr && binding->key == juce::KeyPress ('k', juce::ModifierKeys::commandModifier, 0));

    // Something had focus before.
    auto& header = w.header();
    w.nav->requestFocus (&header.getSearchButton());

    CHECK (w.ed->keyPressed (binding->key));
    CHECK (w.palette().isOpen());
    CHECK (w.nav->getLastFocusRequest() == &w.palette().getField());

    // The key again closes it, from inside the field.
    CHECK (w.palette().handleKey (binding->key));
    CHECK (! w.palette().isOpen());
    CHECK (w.nav->getLastFocusRequest() == &header.getSearchButton());

    // Escape closes, whatever the query.
    w.ed->keyPressed (binding->key);
    w.palette().setQuery ("treble");
    CHECK (w.palette().handleKey (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (! w.palette().isOpen());

    // The header's magnifier.
    header.getSearchButton().onClick();
    CHECK (w.palette().isOpen());
    w.palette().close();

    // 50 searches and navigations leave the saved state byte-identical.
    const auto before = w.state();
    const char* queries[] = { "treble bleed", "gain", "room", "pick", "mic", "reverb", "concert", "cab", "tone", "bass" };

    for (int i = 0; i < 50; ++i)
    {
        w.nav->openPalette (queries[i % 10]);
        const auto& rows = w.palette().getRows();

        for (auto& r : rows)
            if (r.item != nullptr && r.item->kind == ItemKind::parameter && ! r.valueReading && r.item->inAdvanced)
            {
                w.nav->activate (*r.item, ActivationKind::goOnly, false);
                break;
            }

        w.palette().close();
    }

    CHECK_MSG (w.state() == before, "getStateInformation changed after searching and navigating");
}

//==============================================================================
/*  GS-31: mode switching - automatic to Advanced, and staying in Easy for a
    mirrored control. */
LUTHIER_TEST (SearchEditor, GS31_modeSwitching)
{
    CleanRecent clean;
    Win w;
    w.setAdvanced (false);

    const auto* bleed = w.item ("param:" + juce::String (ParamIDs::circuitTrebleBleed));
    CHECK (bleed != nullptr && ! bleed->inEasy && bleed->inAdvanced);

    const auto outcome = w.nav->activate (*bleed, ActivationKind::primary, false);
    CHECK (outcome.status == SearchNavigator::Outcome::Status::done);
    CHECK (w.isAdvanced());
    CHECK (w.nav->getLastNavigation().ok);
    CHECK (w.nav->getLastNotice().contains ("Switched to Advanced Mode"));
    CHECK (w.onScreen (w.nav->getLastNavigation().control.getComponent()));

    // The amp gain has a mirror on Easy's amp card: the mode stays Easy.
    w.setAdvanced (false);
    const auto* gain = w.item ("param:" + juce::String (ParamIDs::ampGain));
    CHECK (gain != nullptr && gain->inEasy);
    CHECK (w.nav->activate (*gain, ActivationKind::primary, false).status == SearchNavigator::Outcome::Status::done);
    CHECK (! w.isAdvanced());
    CHECK (w.onScreen (w.nav->getLastNavigation().control.getComponent()));
}

//==============================================================================
/*  GS-32: too narrow for Advanced (and Live Mode): no switch, the fallback
    text, and the inline set still works. */
LUTHIER_TEST (SearchEditor, GS32_advancedUnavailable)
{
    CleanRecent clean;

    for (int variant = 0; variant < 2; ++variant)
    {
        Win w ({}, variant == 0 ? 960 : 1600, 700);

        if (variant == 1)
        {
            w.p().setLiveMode (true);
            w.setAdvanced (false);
        }

        CHECK (! w.isAdvanced());

        const auto* bleed = w.item ("param:" + juce::String (ParamIDs::circuitTrebleBleed));
        CHECK (bleed != nullptr);
        CHECK (w.availability (*bleed) == Availability::modeUnavailable);

        const auto outcome = w.nav->activate (*bleed, ActivationKind::primary, false);
        CHECK (outcome.status == SearchNavigator::Outcome::Status::refused && outcome.allowInline);
        CHECK_MSG (outcome.message.contains (variant == 0 ? "unavailable at this width" : "Live Mode"), outcome.message);
        CHECK (! w.isAdvanced());

        // The third option, by number (a name would be the option row itself).
        const auto reading = w.nav->readValue ("treble bleed 3");
        CHECK (reading.isValid());
        CHECK (w.nav->applyValue (reading).status == SearchNavigator::Outcome::Status::done);
        CHECK_NEAR (w.p().getState().getParameter (ParamIDs::circuitTrebleBleed)->convertFrom0to1 (
                        w.p().getState().getParameter (ParamIDs::circuitTrebleBleed)->getValue()), 2.0, 0.01);

        if (variant == 1)
            w.p().setLiveMode (false);
    }
}

//==============================================================================
/*  GS-33: auto-switch off asks first. */
LUTHIER_TEST (SearchEditor, GS33_autoSwitchOffConfirms)
{
    CleanRecent clean;
    setAutoSwitchMode (false);

    Win w;
    w.setAdvanced (false);
    w.nav->openPalette ("treble bleed");

    auto& palette = w.palette();
    CHECK (palette.getSelected() != nullptr && palette.getSelected()->item != nullptr
           && palette.getSelected()->item->target == ParamIDs::circuitTrebleBleed);

    palette.activateSelected (ActivationKind::primary);
    CHECK (palette.isOpen());
    CHECK (! w.isAdvanced());
    CHECK (palette.getSubtitle (palette.getSelectedRow()).contains ("Enter again"));

    palette.activateSelected (ActivationKind::primary);
    CHECK (w.isAdvanced());
    CHECK (! palette.isOpen());
}

//==============================================================================
/*  GS-34: the empty state. */
LUTHIER_TEST (SearchEditor, GS34_emptyState)
{
    CleanRecent clean;
    Win w;

    w.nav->getIndex().recordActivation ("param:" + juce::String (ParamIDs::inputGain), "input");
    w.nav->openPalette();

    auto countType = [&w] (CommandPalette::Row::Type type, const juce::String& text = {})
    {
        int n = 0;

        for (auto& r : w.palette().getRows())
            if (r.type == type && (text.isEmpty() || r.text == text))
                ++n;

        return n;
    };

    CHECK (countType (CommandPalette::Row::Type::header, SearchCatalog::text ("search.section.recent")) == 1);
    CHECK (countType (CommandPalette::Row::Type::header, SearchCatalog::text ("search.section.suggestions")) == 1);
    CHECK (countType (CommandPalette::Row::Type::recentQuery) == 1);
    CHECK (countType (CommandPalette::Row::Type::hint) == 1);

    bool suggestsShortcuts = false;

    for (auto& r : w.palette().getRows())
        suggestsShortcuts = suggestsShortcuts || (r.item != nullptr && r.item->id == "cmd:showShortcuts");

    CHECK (suggestsShortcuts);
    w.palette().close();

    // "Remember recent" off: both stores empty, no recent sections.
    RecentStore::get().setEnabled (false);
    CHECK (RecentStore::get().getItems().empty() && RecentStore::get().getQueries().isEmpty());

    w.nav->getIndex().recordActivation ("param:" + juce::String (ParamIDs::inputGain), "input");
    CHECK (RecentStore::get().getItems().empty());

    w.nav->openPalette();
    CHECK (countType (CommandPalette::Row::Type::header, SearchCatalog::text ("search.section.recent")) == 0);
    CHECK (countType (CommandPalette::Row::Type::recentQuery) == 0);
}

//==============================================================================
/*  GS-35: no results offers a correction, which replaces the query. */
LUTHIER_TEST (SearchEditor, GS35_didYouMean)
{
    CleanRecent clean;
    Win w;
    w.nav->openPalette ("gxxin");

    auto& rows = w.palette().getRows();
    CHECK (! rows.empty() && rows.front().type == CommandPalette::Row::Type::noResults);

    int dym = -1;

    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i].type == CommandPalette::Row::Type::didYouMean)
            dym = i;

    CHECK (dym >= 0);

    if (dym < 0)
        return;

    const auto suggestion = rows[(size_t) dym].text;
    w.palette().selectRow (dym);
    w.palette().activateSelected (ActivationKind::primary);
    CHECK (w.palette().getQuery() == suggestion);
    CHECK_MSG (! w.palette().getRows().empty() && w.palette().getRows().front().type == CommandPalette::Row::Type::item,
               "'" + suggestion + "' found nothing");
}

//==============================================================================
/*  GS-36: roles, row titles, announcements, and a keyboard-only run. */
LUTHIER_TEST (SearchEditor, GS36_accessibility)
{
    CleanRecent clean;
    Win w;
    w.setAdvanced (true);

    juce::StringArray announced;
    w.palette().onAnnouncement = [&announced] (const juce::String& s) { announced.add (s); };

    w.nav->openPalette();
    CHECK (announced.contains (SearchCatalog::text ("search.announce.open")));

    // Handlers are created on demand for a window with a peer; the roles are
    // what each component would create.
    auto roleOf = [] (juce::Component& c)
    {
        auto handler = c.createAccessibilityHandler();
        return handler != nullptr ? handler->getRole() : juce::AccessibilityRole::unspecified;
    };

    CHECK (roleOf (w.palette()) == juce::AccessibilityRole::dialogWindow);
    CHECK (w.palette().getTitle() == "Search");
    CHECK (roleOf (w.palette().getField()) == juce::AccessibilityRole::editableText);
    CHECK (w.palette().getField().getTitle() == "Search everything");
    CHECK (roleOf (w.palette().getList()) == juce::AccessibilityRole::list);

    // Keyboard only: type, arrows, Enter.
    w.palette().setQuery ("treble bleed");
    w.palette().announceResultsNow();
    CHECK_MSG (announced[announced.size() - 1].startsWith ("2 results") || announced[announced.size() - 1].contains ("results, first:"),
               announced[announced.size() - 1]);

    for (int i = 0; i < (int) w.palette().getRows().size(); ++i)
        if (w.palette().getRows()[(size_t) i].isSelectable())
            CHECK_MSG (w.palette().getAccessibleRowTitle (i).isNotEmpty(), "row " + juce::String (i) + " has no accessible title");

    CHECK (w.palette().handleKey (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK (w.palette().handleKey (juce::KeyPress (juce::KeyPress::upKey)));
    const auto chosen = w.palette().getSelected() != nullptr ? w.palette().getSelected()->itemId : juce::String();
    CHECK (w.palette().handleKey (juce::KeyPress (juce::KeyPress::returnKey)));
    CHECK (! w.palette().isOpen());
    CHECK_MSG (w.nav->getLastNavigation().ok, chosen + ": " + w.nav->getLastNavigation().message);
    CHECK (w.nav->getLastNavigation().itemId == "param:" + juce::String (ParamIDs::circuitTrebleBleed));
}

//==============================================================================
/*  GS-37: reduced motion is a static ring. */
LUTHIER_TEST (SearchEditor, GS37_reducedMotionHighlight)
{
    CleanRecent clean;
    Win w;
    w.setAdvanced (true);

    auto& settings = AccessibilitySettings::get();
    const bool was = settings.isReducedMotion();

    auto run = [&w] (std::vector<float>& alphas)
    {
        const auto* item = w.item ("param:" + juce::String (ParamIDs::circuitTrebleBleed));
        w.nav->activate (*item, ActivationKind::primary, false);

        auto& h = w.nav->getHighlighter();
        juce::Image image (juce::Image::ARGB, w.ed->getWidth(), w.ed->getHeight(), true);

        for (int frame = 0; frame < 120 && h.isFlashing(); ++frame)
        {
            juce::Graphics g (image);
            h.paintEntireComponent (g, true);
            h.advance (1000.0 / 60.0);
        }

        alphas = h.getFrameAlphas();
        return ! h.isFlashing();
    };

    settings.setReducedMotion (true);
    std::vector<float> still;
    CHECK (run (still));
    CHECK (! still.empty());

    for (auto a : still)
        CHECK (a == 1.0f);

    settings.setReducedMotion (false);
    std::vector<float> pulsing;
    CHECK (run (pulsing));

    bool varied = false;
    for (auto a : pulsing)
        varied = varied || a < 0.99f;

    CHECK (varied);
    settings.setReducedMotion (was);
}

//==============================================================================
/*  GS-38: mouse - a click on a row, the scrim, right-click. */
LUTHIER_TEST (SearchEditor, GS38_mouse)
{
    CleanRecent clean;
    Win w;
    w.setAdvanced (true);
    w.nav->openPalette ("treble bleed");

    auto& palette = w.palette();
    CHECK (! palette.getRows().empty());

    // A click on the first row activates it.
    auto* row = palette.getList().getComponentForRowNumber (0);
    CHECK (row != nullptr);

    if (row != nullptr)
    {
        const auto centre = row->getLocalBounds().getCentre().withX (20);
        row->mouseDown (mouseEventAt (*row, centre));
        row->mouseUp (mouseEventAt (*row, centre));
        CHECK (! palette.isOpen());
        CHECK (w.nav->getLastNavigation().ok);
    }

    // A click on the scrim closes it.
    w.nav->openPalette ("gain");
    palette.mouseDown (mouseEventAt (palette, { 2, 2 }));
    CHECK (! palette.isOpen());

    // Right-click on a parameter row is the control's own menu.
    w.nav->openPalette ("treble bleed");

    for (int i = 0; i < (int) palette.getRows().size(); ++i)
        if (auto* item = palette.getRows()[(size_t) i].item; item != nullptr && item->kind == ItemKind::parameter)
        {
            CHECK (menuTexts (palette.buildSecondaryMenu (i)) == menuTexts (buildParameterContextMenu (w.p(), item->target)));
            break;
        }
}

//==============================================================================
/*  GS-39: a Free build's Pro items: lock, refusal, and rank. */
LUTHIER_TEST (SearchEditor, GS39_proLocked)
{
    CleanRecent clean;
    Win w;
    w.setAdvanced (true);

    w.nav->proLockPredicate = [] (const SearchItem& i) { return i.id == "param:" + juce::String (ParamIDs::ampBass); };
    w.nav->getIndex().invalidate();

    const auto* bass = w.item ("param:" + juce::String (ParamIDs::ampBass));
    CHECK (w.availability (*bass) == Availability::proLocked);
    CHECK (w.nav->subtitleFor (*bass, Availability::proLocked).contains ("Luthier Pro"));

    const auto outcome = w.nav->activate (*bass, ActivationKind::primary, false);
    CHECK (outcome.status == SearchNavigator::Outcome::Status::refused);
    CHECK (outcome.message.contains ("Luthier Pro"));

    // An equal match that is available ranks above it.
    const auto results = w.nav->getIndex().query ("amp b");
    int bassAt = -1, brightAt = -1;

    for (int i = 0; i < (int) results.size(); ++i)
    {
        if (results[(size_t) i].item->id == "param:" + juce::String (ParamIDs::ampBass))   bassAt = i;
        if (results[(size_t) i].item->id == "param:" + juce::String (ParamIDs::ampBright)) brightAt = i;
    }

    CHECK (bassAt >= 0 && brightAt >= 0 && brightAt < bassAt);
    CHECK_NEAR (results[(size_t) bassAt].matchScore, results[(size_t) brightAt].matchScore, 1.0e-9);

    w.nav->openPalette ("amp bass");

    for (int i = 0; i < (int) w.palette().getRows().size(); ++i)
        if (w.palette().getRows()[(size_t) i].item == bass)
            CHECK (w.palette().getAccessibleRowTitle (i).contains ("Luthier Pro"));
}

//==============================================================================
/*  GS-40: queries during a render: no allocation in processBlock, and the
    output bit-identical to the same render without them. */
LUTHIER_TEST (SearchEditor, GS40_noAudioThreadEffect)
{
    CleanRecent clean;

    auto render = [] (bool withPalette, long& allocations)
    {
        Win w;
        auto& presets = w.p().getPresetManager();
        int rock = -1;

        for (int i = 0; i < presets.getNumPresets() && rock < 0; ++i)
            if (presets.getPreset (i)->name.containsIgnoreCase ("rock"))
                rock = i;

        if (rock >= 0)
            presets.loadPreset (rock);

        w.p().getParameterBridge().applyAllNow();
        w.p().prepareToPlay (kSr, kBlock);

        if (withPalette)
            w.nav->openPalette();

        const int blocks = (int) (30.0 * kSr / kBlock);
        const int blocksPer16ms = juce::jmax (1, (int) (0.016 * kSr / kBlock));
        const char* queries[] = { "gain", "treble bl", "reverb", "> slide", "? tone", "gain 7", "room 50%" };

        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;
        std::vector<float> out;
        out.reserve ((size_t) blocks * kBlock);
        allocations = 0;

        for (int b = 0; b < blocks; ++b)
        {
            midi.clear();

            if (b % 400 == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 40 + (b / 400) % 12, (juce::uint8) 100), 0);

            if (b % 400 == 300)
                midi.addEvent (juce::MidiMessage::noteOff (1, 40 + (b / 400) % 12), 0);

            if (withPalette && b % blocksPer16ms == 0)
                w.palette().setQuery (queries[(b / blocksPer16ms) % 7]);

            buffer.clear();

           #if defined (LUTHIER_ALLOCATION_COUNTER)
            const auto before = allocationsOnThisThread();
           #endif

            w.p().processBlock (buffer, midi);

           #if defined (LUTHIER_ALLOCATION_COUNTER)
            allocations += allocationsOnThisThread() - before;
           #endif

            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + kBlock);
        }

        return out;
    };

    long allocationsWithout = 0, allocationsWith = 0, warmUp = 0;
    render (false, warmUp);   // first-use statics are not the palette's
    const auto without = render (false, allocationsWithout);
    const auto with = render (true, allocationsWith);

    CHECK (without.size() == with.size());
    CHECK_MSG (std::memcmp (without.data(), with.data(), without.size() * sizeof (float)) == 0,
               "the render differs with the palette querying");

    std::cout << "    processBlock allocations: " << allocationsWithout << " without the palette, "
              << allocationsWith << " with it (warm-up render " << warmUp << ")" << std::endl;

    // The palette adds nothing to processBlock's own count, which is zero.
    CHECK (allocationsWithout == 0);
    CHECK_MSG (allocationsWith == allocationsWithout,
               juce::String (allocationsWith) + " allocations in processBlock with the palette, "
                 + juce::String (allocationsWithout) + " without");
}

//==============================================================================
/*  GS-42: a host preset load while open refreshes and keeps the selection. */
LUTHIER_TEST (SearchEditor, GS42_presetLoadWhileOpen)
{
    CleanRecent clean;
    Win w;
    w.nav->openPalette ("gain");

    auto& palette = w.palette();
    CHECK (palette.getRows().size() >= 2);
    palette.selectRow (1);
    const auto selectedId = palette.getSelected()->itemId;

    auto& presets = w.p().getPresetManager();
    presets.loadPreset (juce::jmin (3, presets.getNumPresets() - 1));
    presets.dispatchPendingMessages();

    CHECK (palette.isOpen());
    CHECK (palette.getSelected() != nullptr && palette.getSelected()->itemId == selectedId);
}

//==============================================================================
/*  GS-43: a Workshop overlay open in Easy; a CAB control dismisses it; a part
    opens the Workshop on its category. */
LUTHIER_TEST (SearchEditor, GS43_workshopOverlay)
{
    CleanRecent clean;
    Win w;
    w.setAdvanced (false);

    w.ed->performAction ("openWorkshop");
    auto* overlays = w.nav->getOwnedOverlays()[7];   // the Workshop overlay
    CHECK (w.onScreen (overlays));

    const auto* cab = w.item ("param:" + juce::String (ParamIDs::cabType));
    CHECK (w.nav->activate (*cab, ActivationKind::primary, false).status == SearchNavigator::Outcome::Status::done);
    CHECK (! w.onScreen (overlays));
    CHECK (w.nav->getLastNavigation().ok);
    CHECK (! w.isAdvanced());

    // A part: the Workshop on its category.
    const SearchItem* part = nullptr;

    for (auto* item : w.nav->getIndex().getItems())
        if (item->kind == ItemKind::part && item->target == "bridge")
        {
            part = item;
            break;
        }

    CHECK (part != nullptr);

    if (part == nullptr)
        return;

    CHECK (w.nav->activate (*part, ActivationKind::primary, false).status == SearchNavigator::Outcome::Status::done);
    CHECK (w.onScreen (overlays));

    if (auto* workshop = dynamic_cast<WorkshopOverlay*> (overlays))
        CHECK (workshop->getPanel().getCategory() == "Bridge");
}

//==============================================================================
/*  GS-45: Retune all, Export MIDI and Toggle Slide Mode from the palette do
    what their buttons and keys do, with the same undo classes. */
LUTHIER_TEST (SearchEditor, GS45_commandsMatchTheirButtons)
{
    CleanRecent clean;

    // Retune all: the CHARACTER panel's Retune, compared with pressing it.
    {
        auto runUp = [] (Win& w)
        {
            w.p().prepareToPlay (kSr, kBlock);
            juce::AudioBuffer<float> buffer (2, kBlock);
            juce::MidiBuffer midi;

            for (int b = 0; b < 200; ++b)
                w.p().processBlock (buffer, midi);
        };

        Win byButton, byPalette;
        runUp (byButton);
        runUp (byPalette);

        juce::TextButton* retune = nullptr;
        std::function<void (juce::Component&)> find = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (auto* b = dynamic_cast<juce::TextButton*> (child); b != nullptr && b->getButtonText() == "Retune")
                    retune = b;

                find (*child);
            }
        };

        for (int i = 0; i < 64; ++i)
            if (auto* adv = dynamic_cast<AdvancedPanel*> (byButton.ed->getChildComponent (i)))
                for (int t = 0; t < adv->getNumWorkspaceTabs(); ++t)
                    if (adv->getWorkspaceTabName (t) == "CHARACTER")
                        find (*adv->getWorkspacePanel (t));

        CHECK (retune != nullptr);

        const int depthButton = byButton.p().getNumUndoSteps(), depthPalette = byPalette.p().getNumUndoSteps();
        CHECK (byPalette.p().getEngine().getCharacterEngine().getSessionSeconds() > 0.0);

        if (retune != nullptr)
            retune->onClick();

        CHECK (byPalette.nav->activate (*byPalette.item ("cmd:retuneAll"), ActivationKind::primary, false).status == SearchNavigator::Outcome::Status::done);
        CHECK (byPalette.p().getEngine().getCharacterEngine().getSessionSeconds() == 0.0);
        CHECK (byButton.p().getEngine().getCharacterEngine().getSessionSeconds() == 0.0);
        CHECK (byPalette.p().getNumUndoSteps() - depthPalette == byButton.p().getNumUndoSteps() - depthButton);
    }

    // Export MIDI: the MIDI OUT tab's export.
    {
        Win w;
        int pressed = 0;
        w.nav->onExportMidiPressed = [&pressed] { ++pressed; };

        CHECK (w.nav->activate (*w.item ("cmd:exportMidi"), ActivationKind::primary, false).status == SearchNavigator::Outcome::Status::done);
        CHECK (pressed == 1);
        CHECK (w.isAdvanced());
    }

    // Toggle Slide Mode: palette and key, one undo entry each (3.3).
    {
        Win byKey, byPalette;
        const auto* binding = AccessibilitySettings::get().findShortcut ("toggleSlideMode");
        const int depthKey = byKey.p().getNumUndoSteps(), depthPalette = byPalette.p().getNumUndoSteps();

        byKey.ed->keyPressed (binding->key);
        byPalette.nav->activate (*byPalette.item ("cmd:toggleSlideMode"), ActivationKind::primary, false);

        CHECK (ParameterLocations::isSlideModeOn (byKey.p()) && ParameterLocations::isSlideModeOn (byPalette.p()));
        CHECK (byKey.p().getNumUndoSteps() == depthKey + 1);
        CHECK (byPalette.p().getNumUndoSteps() == depthPalette + 1);
        CHECK (byPalette.nav->getActions().find ("toggleSlideMode")->undo == UndoClass::slideMode);
    }
}

//==============================================================================
/*  6.1 and 7: the HELP tab field opens the palette on the ? scope; the
    Options group writes its preferences; the Search command is indexed. */
LUTHIER_TEST (SearchEditor, entryPointsAndOptions)
{
    CleanRecent clean;
    Win w;
    w.setAdvanced (true);

    // The Help overlay's HelpTab (the HELP tab's is the same class, wired the same way).
    HelpTab* help = nullptr;

    for (auto* o : w.nav->getOwnedOverlays())
        if (auto* panel = dynamic_cast<HelpPanel*> (o))
            help = &panel->getView();

    CHECK (help != nullptr);

    if (help != nullptr)
    {
        // Typing "t" (TextEditor's change message is asynchronous, so the
        // handler is run as it would be).
        help->getSearchField().setText ("t", false);
        help->getSearchField().onTextChange();
        CHECK (w.palette().isOpen());
        CHECK (w.palette().getQuery() == "? t");
        w.palette().close();
    }

    SearchOptionsGroup group;
    group.getAutoSwitchToggle().setToggleState (false, juce::sendNotificationSync);
    group.getAutoSwitchToggle().onClick();
    CHECK (! isAutoSwitchModeOn());
    group.getAutoSwitchToggle().setToggleState (true, juce::sendNotificationSync);
    group.getAutoSwitchToggle().onClick();
    CHECK (isAutoSwitchModeOn());

    w.nav->getIndex().recordActivation ("cmd:panic", "panic");
    group.getClearButton().onClick();
    CHECK (RecentStore::get().getItems().empty());

    CHECK (w.item ("cmd:search") != nullptr);
    CHECK (w.item ("cmd:clearRecentSearches") != nullptr);
    CHECK (w.item ("set:ACCESSIBILITY:search.autoSwitchMode") != nullptr);
}

//==============================================================================
/*  6.2 and 6.3: Live Mode rows, Tab between field / chips / list, Left and
    Right on the chips, Page and Ctrl+Home/End, F1. */
LUTHIER_TEST (SearchEditor, keyboardAndLiveRows)
{
    CleanRecent clean;
    Win w;
    w.nav->openPalette ("a");

    auto& palette = w.palette();
    const int normal = palette.getRowHeight();

    CHECK (palette.handleKey (juce::KeyPress (juce::KeyPress::tabKey)));
    CHECK (w.nav->getLastFocusRequest() == &palette.getChip (0));
    CHECK (palette.handleKey (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK (palette.getScope() == SearchIndex::Scope::controls);
    CHECK (palette.handleKey (juce::KeyPress (juce::KeyPress::tabKey)));
    CHECK (w.nav->getLastFocusRequest() == &palette.getList());
    CHECK (palette.handleKey (juce::KeyPress (juce::KeyPress::tabKey)));
    CHECK (w.nav->getLastFocusRequest() == &palette.getField());
    CHECK (palette.handleKey (juce::KeyPress (juce::KeyPress::tabKey, juce::ModifierKeys::shiftModifier, 0)));
    CHECK (w.nav->getLastFocusRequest() == &palette.getList());

    for (auto& r : palette.getRows())
        CHECK (r.item == nullptr || SearchIndex::scopeIncludes (SearchIndex::Scope::controls, r.item->kind));

    CHECK (palette.handleKey (juce::KeyPress (juce::KeyPress::endKey, juce::ModifierKeys::commandModifier, 0)));
    const int last = palette.getSelectedRow();
    CHECK (palette.handleKey (juce::KeyPress (juce::KeyPress::homeKey, juce::ModifierKeys::commandModifier, 0)));
    CHECK (palette.getSelectedRow() < last);
    CHECK (palette.handleKey (juce::KeyPress (juce::KeyPress::pageDownKey)));
    CHECK (palette.getSelectedRow() > 0);

    // Up from the first row wraps to the last.
    palette.handleKey (juce::KeyPress (juce::KeyPress::homeKey, juce::ModifierKeys::commandModifier, 0));
    palette.handleKey (juce::KeyPress (juce::KeyPress::upKey));
    CHECK (palette.getSelectedRow() == last);
    palette.close();

    // gui-integration 9: 44 px rows in Live Mode.
    w.p().setLiveMode (true);
    CHECK (palette.getRowHeight() > normal);
    CHECK (palette.getRowHeight() == AccessibilitySettings::get().scaled (44));
    w.p().setLiveMode (false);

    // Queries are capped at 200 characters (6.3).
    w.nav->openPalette (juce::String::repeatedString ("x", 500));
    CHECK (palette.getQuery().length() <= 200);
}
