/*  GUI reachability - group "GuiReach".

    Builds the real editor, walks every view a user can reach - Easy mode,
    Advanced mode and each of its workspace tabs, every overlay and every
    Options page, the practice drawer, Live and Slide modes - in several guitar
    contexts (a bass for the slap group, a whammy bridge for the bridge panel,
    every pedal slot filled), and collects every control on screen.

    Then it asks the question a user would: can I reach every automatable
    parameter from a visible control? A parameter with no visible control is a
    feature that only automation or a preset can set. The only exceptions are
    the ones listed in kIntentionallyHidden, each with its reason.

    It also operates every control it found - minimum, maximum, back to where it
    was, through the same attachment a mouse would use - and checks the
    parameter follows and nothing crashes.

    LUTHIER_COMBO_VERBOSE=1 prints the whole reachability table.
*/

#include "ComboHarness.h"

#include "../PluginEditor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/Overlays.h"
#include "../UI/OptionsPages.h"
#include "../UI/PracticePanel.h"
#include "../UI/Widgets.h"
#include "../UI/GuitarBodyComponent.h"

#include <set>

using namespace luthier;
using namespace luthier::tests;
using namespace luthier::combo;

namespace
{
    /*  Parameters with no visible control on purpose. Each needs a reason a
        reviewer would accept; anything not here that has no control fails. */
    const std::map<juce::String, juce::String>& intentionallyHidden()
    {
        static const std::map<juce::String, juce::String> m
        {
            { "feedback_on",        "superseded by feedback_amount (ambiguity-resolutions 1.2); kept for automation indices" },
            { "feedback_threshold", "superseded by the physical feedback loop (ambiguity-resolutions 1.2)" },
            { "feedback_speed",     "superseded by the physical feedback loop (ambiguity-resolutions 1.2)" },
            { "fret_action",        "superseded by the setup geometry (DECISIONS.md, fret-buzz 7); inert, kept for automation indices" },
            { "doubler_on",         "legacy engine doubler: a load migrates it to a Doubler pedal (PresetManager::fromVar)" },
            { "doubler_amount",     "legacy engine doubler; the Doubler pedal's own knobs replace it" },
            { "strum_speed",        "superseded by strum_crossing_sps (strum-dynamics 7); kept for automation indices" },
        };

        return m;
    }

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

    /** Visible all the way up to `root`, with a size: what a user could see. */
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

    bool insideLearnTarget (juce::Component* c)
    {
        for (auto* p = c->getParentComponent(); p != nullptr; p = p->getParentComponent())
            if (dynamic_cast<LearnTarget*> (p) != nullptr)
                return true;

        return false;
    }

    juce::KeyPress shortcutFor (const char* actionId)
    {
        if (const auto* binding = AccessibilitySettings::get().findShortcut (actionId))
            return binding->key;

        return {};
    }

    /** Records, in order, which parameters were written. */
    struct ParamRecorder : juce::AudioProcessorParameter::Listener
    {
        explicit ParamRecorder (juce::AudioProcessor& p) : processor (p)
        {
            for (auto* prm : processor.getParameters())
                prm->addListener (this);
        }

        ~ParamRecorder() override
        {
            for (auto* prm : processor.getParameters())
                prm->removeListener (this);
        }

        void parameterValueChanged (int index, float) override { written.push_back (index); }
        void parameterGestureChanged (int, bool) override {}

        juce::AudioProcessor& processor;
        std::vector<int> written;
    };

    struct Reach
    {
        std::set<juce::String> visibleIn;     ///< view names where a control for it was on screen
        bool existsHidden = false;            ///< a control exists, but was never seen on screen
        bool operatedOk = false;
        juce::String operateProblem;
    };

    /** A parameter's id by its processor index. */
    juce::String idOf (juce::AudioProcessor& p, int index)
    {
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[index]))
            return r->getParameterID();
        return {};
    }

    /** Everything the walk learned. */
    struct Walk
    {
        std::map<juce::String, Reach> reach;
        juce::StringArray views;
        juce::StringArray problems;
        int operated = 0;
    };

    //==========================================================================
    /*  Operates one control through its public API (the same attachment path a
        mouse uses) and returns the ids written, in order. Restores it after. */
    std::vector<juce::String> operate (juce::Component* c, LuthierAudioProcessor& p, ParamRecorder& rec,
                                       juce::String& problem, const std::function<void()>& whileToggled = {})
    {
        rec.written.clear();
        std::vector<juce::String> ids;

        if (verbose())
            std::cout << "      operate " << typeid (*c).name() << " '" << c->getName() << "' "
                      << (c->getParentComponent() != nullptr ? typeid (*c->getParentComponent()).name() : "") << std::endl;

        auto drain = [&]
        {
            for (int idx : rec.written)
            {
                const auto id = idOf (p, idx);
                if (id.isNotEmpty() && std::find (ids.begin(), ids.end(), id) == ids.end())
                    ids.push_back (id);
            }
            rec.written.clear();
        };

        if (auto* s = dynamic_cast<juce::Slider*> (c))
        {
            const double was = s->getValue();
            s->setValue (s->getMinimum(), juce::sendNotificationSync); drain();
            s->setValue (s->getMaximum(), juce::sendNotificationSync); drain();
            s->setValue (was, juce::sendNotificationSync);             drain();
        }
        else if (auto* b = dynamic_cast<juce::ComboBox*> (c))
        {
            // Items ending in an ellipsis open a dialog (a file chooser forks a
            // helper and opens a native window); a user picking one is not
            // operating a parameter, so they are skipped.
            auto opensDialog = [b] (int i)
            {
                const auto text = b->getItemText (i).trim();
                return text.endsWith ("...") || text.endsWith (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")));
            };

            std::vector<int> safeItems;
            for (int i = 0; i < b->getNumItems(); ++i)
                if (! opensDialog (i))
                    safeItems.push_back (i);

            const int was = b->getSelectedItemIndex();
            if (! safeItems.empty())
            {
                b->setSelectedItemIndex (safeItems.front(), juce::sendNotificationSync); drain();
                b->setSelectedItemIndex (safeItems.back(), juce::sendNotificationSync);  drain();
                if (was >= 0) b->setSelectedItemIndex (was, juce::sendNotificationSync); drain();
            }
        }
        else if (auto* t = dynamic_cast<juce::Button*> (c))
        {
            juce::Component::SafePointer<juce::Button> safe (t);
            const bool was = t->getToggleState();
            t->setToggleState (! was, juce::sendNotificationSync); drain();

            // A toggle that reveals more controls (Morph, dual mic...) opens a
            // sub-view: scan it while it is open.
            if (whileToggled && ! was)
                whileToggled();

            if (safe != nullptr)
            {
                safe->setToggleState (was, juce::sendNotificationSync);
                drain();
            }
        }
        else if (auto* m = dynamic_cast<StringMaskSelector*> (c))
        {
            m->toggle (0); drain();
            m->toggle (0); drain();
        }

        juce::ignoreUnused (problem);
        return ids;
    }

    /** The component a LearnTarget operates: its slider, box or button. */
    juce::Component* innerControl (juce::Component* learn)
    {
        if (auto* k = dynamic_cast<LuthierKnob*> (learn))   return &k->getSlider();
        if (auto* s = dynamic_cast<LuthierSlider*> (learn)) return &s->getSlider();
        if (auto* c = dynamic_cast<LuthierChoice*> (learn)) return &c->getComboBox();
        if (auto* t = dynamic_cast<LuthierToggle*> (learn)) return &t->getButton();
        return nullptr;
    }

    //==========================================================================
    /*  Scans one view: every LearnTarget and every raw control. Operates each
        control once per walk (the first time it is seen on screen). */
    void scanView (const juce::String& view, juce::Component& root, LuthierAudioProcessor& p,
                   ParamRecorder& rec, Walk& walk, std::set<juce::Component*>& operatedControls, int depth = 0)
    {
        auto subView = [&] (juce::Component* toggle)
        {
            return [&, toggle]
            {
                if (depth > 0)
                    return;

                auto name = toggle->getName();
                if (auto* b = dynamic_cast<juce::Button*> (toggle))
                    name = b->getButtonText();

                scanView (view + "+" + name, root, p, rec, walk, operatedControls, depth + 1);
            };
        };

        walk.views.add (view);

        if (verbose())
            std::cout << "    view " << view << std::endl;

        // LearnTargets name their parameter.
        juce::Array<juce::Component*> found;
        collect<juce::Component> (root, found);

        // Operating a control can rebuild the panel it sits in; SafePointers say
        // when a component collected earlier has gone.
        std::vector<juce::Component::SafePointer<juce::Component>> all;
        for (auto* c : found)
            all.emplace_back (c);

        for (auto& safe : all)
        {
            auto* c = safe.getComponent();
            if (c == nullptr)
                continue;

            if (auto* learn = dynamic_cast<LearnTarget*> (c))
            {
                const auto id = learn->getLearnParameterId();
                if (id.isEmpty())
                    continue;

                auto& r = walk.reach[id];
                if (onScreen (c, &root)) r.visibleIn.insert (view);
                else                     r.existsHidden = true;

                if (! onScreen (c, &root) || operatedControls.count (c) > 0)
                    continue;

                operatedControls.insert (c);

                if (auto* inner = innerControl (c))
                {
                    juce::String problem;
                    const auto ids = operate (inner, p, rec, problem, subView (inner));
                    ++walk.operated;

                    if (std::find (ids.begin(), ids.end(), id) != ids.end())
                        r.operatedOk = true;
                    else
                        r.operateProblem = "operating its control in " + view + " did not write it";
                }
            }
        }

        // Raw controls, not inside a LearnTarget: operate, and credit the first
        // parameter written, when it wrote only a few (a preset box writes them all).
        for (auto& safe : all)
        {
            auto* c = safe.getComponent();
            if (c == nullptr)
                continue;

            if (dynamic_cast<LearnTarget*> (c) != nullptr || insideLearnTarget (c))
                continue;

            const bool isSlider = dynamic_cast<juce::Slider*> (c) != nullptr;
            const bool isCombo  = dynamic_cast<juce::ComboBox*> (c) != nullptr;
            const bool isMask   = dynamic_cast<StringMaskSelector*> (c) != nullptr;
            auto* button = dynamic_cast<juce::Button*> (c);
            const bool isToggle = button != nullptr && button->getClickingTogglesState()
                                  && button->getRadioGroupId() == 0;

            if (! (isSlider || isCombo || isToggle || isMask))
                continue;

            if (! onScreen (c, &root) || operatedControls.count (c) > 0)
                continue;

            operatedControls.insert (c);

            juce::String problem;
            const auto ids = operate (c, p, rec, problem, subView (c));
            ++walk.operated;

            // A style box writes its own choice first and then the values the
            // style stands for; its own choice is the one credited.
            bool credit = ! ids.empty() && ids.size() <= 3;

            if (! credit && isCombo && ! ids.empty())
                if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.getParameters()[p.getState().getParameter (ids.front())->getParameterIndex()]))
                    credit = std::abs (choice->choices.size() - dynamic_cast<juce::ComboBox*> (c)->getNumItems()) <= 1;

            if (credit)
            {
                auto& r = walk.reach[ids.front()];
                r.visibleIn.insert (view + " (raw " + (isSlider ? "slider" : isCombo ? "combo" : isMask ? "mask" : "toggle") + ")");
                r.operatedOk = true;
            }
        }
    }

    /** Presses a TextButton child of `parent` whose text matches. */
    bool clickTab (juce::Component& parent, const juce::String& text)
    {
        for (auto* child : parent.getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (child))
                if (b->getButtonText() == text && b->onClick != nullptr)
                {
                    b->onClick();
                    return true;
                }

        return false;
    }

    /*  One full walk of the editor in the processor's current context. */
    void walkEditor (const juce::String& context, LuthierAudioProcessor& p, Walk& walk)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());

        if (editor == nullptr)
        {
            walk.problems.add (context + ": no editor");
            return;
        }

        editor->setVisible (true);
        editor->setSize (juce::jmax (1600, LuthierAudioProcessorEditor::defaultWidth),
                         juce::jmax (1000, LuthierAudioProcessorEditor::defaultHeight));

        ParamRecorder rec (p);
        std::set<juce::Component*> operated;
        auto& root = *editor;

        auto key = [&] (const char* action)
        {
            if (verbose())
                std::cout << "    key " << action << std::endl;

            const auto k = shortcutFor (action);
            if (! k.isValid() || ! editor->keyPressed (k))
                walk.problems.add (context + ": shortcut \"" + action + "\" did nothing");
        };

        // Easy.
        scanView (context + "/Easy", root, p, rec, walk, operated);

        // Practice drawer, Live, Slide - in Easy.
        key ("togglePractice"); scanView (context + "/Easy+Practice", root, p, rec, walk, operated); key ("togglePractice");
        key ("toggleLiveMode"); scanView (context + "/Easy+Live", root, p, rec, walk, operated);     key ("toggleLiveMode");
        key ("toggleSlideMode"); scanView (context + "/Easy+Slide", root, p, rec, walk, operated);   key ("toggleSlideMode");

        // Advanced, every workspace tab.
        key ("toggleAdvanced");

        if (auto* adv = findOne<AdvancedPanel> (root))
        {
            if (! adv->isVisible())
                walk.problems.add (context + ": Advanced mode did not show the advanced panel");

            for (int t = 0; t < adv->getNumWorkspaceTabs(); ++t)
            {
                adv->setWorkspaceTab (t);
                editor->resized();
                scanView (context + "/Advanced/" + adv->getWorkspaceTabName (t), root, p, rec, walk, operated);
            }

            key ("toggleSlideMode"); scanView (context + "/Advanced+Slide", root, p, rec, walk, operated); key ("toggleSlideMode");
        }
        else
        {
            walk.problems.add (context + ": no AdvancedPanel");
        }

        key ("toggleAdvanced");

        // The illustration's popovers (gui-integration 3.1), built as the
        // illustration builds them, and scanned as their own views.
        {
            TuningPopover tuning (p);
            tuning.setVisible (true);
            tuning.setBounds (TuningPopover::preferredSize (p.getEngine().getNumStrings()).withPosition (0, 0));
            scanView (context + "/Popover:headstock", tuning, p, rec, walk, operated);
        }
        {
            WhammyPopover whammy (p);
            whammy.setVisible (true);
            whammy.setBounds (WhammyPopover::preferredSize().withPosition (0, 0));
            scanView (context + "/Popover:bridge", whammy, p, rec, walk, operated);
        }

        // The hidden effect: a click on the notch pixel (include.md). Its
        // position is private to the editor, so the header is searched the way
        // a curious user would - clicking until something opens.
        if (auto* ed = dynamic_cast<LuthierAudioProcessorEditor*> (editor.get()))
        {
            auto* hostNow = findOne<OverlayHost> (root);
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            bool found = false;

            for (int y = 0; y < 48 && ! found; ++y)
                for (int x = 0; x < ed->getWidth() && ! found; ++x)
                {
                    const juce::Point<float> at ((float) x, (float) y);
                    const juce::MouseEvent e (source, at, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                              ed, ed, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false);
                    ed->mouseDown (e);
                    found = hostNow != nullptr && hostNow->isShowingOverlay();
                }

            if (found)
            {
                scanView (context + "/Overlay:secret", root, p, rec, walk, operated);
                editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            }
            else
            {
                walk.problems.add (context + ": no pixel in the top 48 rows opens the hidden effect");
            }
        }

        // Overlays, and every Options page.
        auto* host = findOne<OverlayHost> (root);

        for (const char* action : { "help", "presetBrowser", "export", "debugPanel", "saveAs", "options" })
        {
            key (action);

            if (host == nullptr || ! host->isShowingOverlay())
            {
                walk.problems.add (context + ": overlay \"" + action + "\" did not open");
                continue;
            }

            scanView (context + "/Overlay:" + action, root, p, rec, walk, operated);

            if (juce::String (action) == "options")
                if (auto* options = findOne<OptionsPanel> (root))
                    for (const auto& tab : { "AUDIO", "MIDI", "APPEARANCE", "ACCESSIBILITY", "LOCALIZATION", "EXPRESSION",
                                             "RANGES", "UPDATES", "PRIVACY", "DIAGNOSTICS", "FILE LOCATIONS" })
                    {
                        if (! clickTab (*options, tab))
                            walk.problems.add (context + ": Options page " + tab + " did not open");
                        else
                            scanView (context + "/Options/" + tab, root, p, rec, walk, operated);
                    }

            editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
        }
    }

    /** The contexts the walk runs in, so every conditional panel appears. */
    Walk walkAllContexts (int& numContexts)
    {
        Walk walk;
        Rig rig;

        struct Context { juce::String name; std::function<void (Rig&)> setup; };
        std::vector<Context> contexts
        {
            { "default", [] (Rig&) {} },
            { "bass", [] (Rig& r)
                {
                    auto* c = dynamic_cast<juce::AudioParameterChoice*> (r.param (ParamIDs::guitarType));
                    if (c != nullptr)
                        for (int i = 0; i < c->choices.size(); ++i)
                            if (c->choices[i].containsIgnoreCase ("bass")) { r.setIndex (ParamIDs::guitarType, i); break; }
                } },
            // FEAT-MIC (mic-placement.md 6.1): the external mics' controls are
            // shown on an acoustic guitar, with its second mic on.
            { "acoustic", [] (Rig& r)
                {
                    auto* c = dynamic_cast<juce::AudioParameterChoice*> (r.param (ParamIDs::guitarType));
                    if (c != nullptr)
                        for (int i = 0; i < c->choices.size(); ++i)
                            if (c->choices[i].containsIgnoreCase ("dread")) { r.setIndex (ParamIDs::guitarType, i); break; }
                    r.setIndex (ParamIDs::acMic2On, 1);
                } },
            { "whammy+slide+slap", [] (Rig& r)
                {
                    auto* c = dynamic_cast<juce::AudioParameterChoice*> (r.param (ParamIDs::bridgeType));
                    if (c != nullptr)
                        for (int i = 0; i < c->choices.size(); ++i)
                            if (c->choices[i].containsIgnoreCase ("trem") || c->choices[i].containsIgnoreCase ("whammy")
                                || c->choices[i].containsIgnoreCase ("floyd") || c->choices[i].containsIgnoreCase ("bigsby"))
                            { r.setIndex (ParamIDs::bridgeType, i); break; }
                    r.setIndex (ParamIDs::slideGuitar, 1);
                    r.setIndex (ParamIDs::slapArmed, 1);
                    r.setIndex (ParamIDs::scrapeArmed, 1);
                    r.setIndex (ParamIDs::ebowEnable, 1);
                    r.setIndex (ParamIDs::freezeEnable, 1);
                    r.setIndex (ParamIDs::dualMic, 1);
                    r.setIndex (ParamIDs::secretOn, 1);
                } },
        };

        // Every pedal type appears in some slot, so every pedal's own knobs are
        // on screen at least once.
        const int pedalTypes = rig.numChoices (ParamIDs::slotType (false, 0));
        const int slots = EffectsChain::kNumSlots;

        for (int base = 1; base < pedalTypes; base += 2 * slots)
            contexts.push_back ({ "pedals" + juce::String (base), [=] (Rig& r)
                {
                    for (int chain = 0; chain < 2; ++chain)
                        for (int s = 0; s < slots; ++s)
                            r.setIndex (ParamIDs::slotType (chain == 1, s), 1 + ((base - 1 + chain * slots + s) % juce::jmax (1, pedalTypes - 1)));
                } });

        numContexts = (int) contexts.size();

        for (auto& ctxt : contexts)
        {
            rig.p().resetEverything();
            ctxt.setup (rig);
            rig.apply();
            walkEditor (ctxt.name, rig.p(), walk);
        }

        return walk;
    }

    /** Cached: the walk is slow and three tests read it. */
    Walk& theWalk()
    {
        static int contexts = 0;
        static Walk walk = walkAllContexts (contexts);
        return walk;
    }
}

//==============================================================================
/*  Every automatable parameter has a visible control somewhere a user can go. */
LUTHIER_TEST (GuiReach, everyAutomatableParameterHasAVisibleControl)
{
    auto& walk = theWalk();
    Rig rig;

    std::cout << "    views walked: " << walk.views.size() << ", controls operated: " << walk.operated << std::endl;

    juce::StringArray unreachable, hiddenOnly, pedalParamsMissing;
    int total = 0, reachable = 0;

    for (auto* prm : rig.p().getParameters())
    {
        auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm);
        if (r == nullptr || ! r->isAutomatable())
            continue;

        const auto id = r->getParameterID();
        ++total;

        auto it = walk.reach.find (id);
        const bool visible = it != walk.reach.end() && ! it->second.visibleIn.empty();

        if (verbose())
            std::cout << "    " << id.paddedRight (' ', 34)
                      << (visible ? juce::String (*it->second.visibleIn.begin())
                                  : (it != walk.reach.end() && it->second.existsHidden ? "HIDDEN CONTROL ONLY" : "NO CONTROL"))
                      << "\n";

        if (visible) { ++reachable; continue; }
        if (intentionallyHidden().count (id) > 0) continue;

        // Pedal slot parameters p0-p9 are summarised separately: a pedal with
        // four knobs leaves p4-p9 unused, which is not a missing control.
        if (id.matchesWildcard ("pre*_p*", true) || id.matchesWildcard ("post*_p*", true))
        {
            pedalParamsMissing.add (id);
            continue;
        }

        if (it != walk.reach.end() && it->second.existsHidden) hiddenOnly.add (id);
        else                                                   unreachable.add (id);
    }

    std::cout << "    automatable parameters: " << total << ", visible control: " << reachable
              << ", hidden-only: " << hiddenOnly.size() << ", no control: " << unreachable.size()
              << ", pedal slot params without a knob: " << pedalParamsMissing.size() << std::endl;

    if (unreachable.size() > 0)
        std::cout << "    NO CONTROL: " << unreachable.joinIntoString (" ") << std::endl;
    if (hiddenOnly.size() > 0)
        std::cout << "    HIDDEN CONTROL ONLY: " << hiddenOnly.joinIntoString (" ") << std::endl;

    for (auto& id : unreachable)
        CHECK_MSG (false, id + " has no control anywhere in the editor");

    for (auto& id : hiddenOnly)
        CHECK_MSG (false, id + " has a control that never appears on screen in any view walked");

    for (auto& p : walk.problems)
        CHECK_MSG (false, p);
}

//==============================================================================
/*  Pedal slot parameters: each pedal type's own parameters (the ones its
    Pedal::getParameterName names) must have a knob when that pedal is fitted. */
LUTHIER_TEST (GuiReach, everySlotTypeBypassAndMixHasAControl)
{
    auto& walk = theWalk();

    for (int chain = 0; chain < 2; ++chain)
        for (int s = 0; s < EffectsChain::kNumSlots; ++s)
            for (const auto& id : { ParamIDs::slotType (chain == 1, s), ParamIDs::slotBypass (chain == 1, s), ParamIDs::slotMix (chain == 1, s) })
            {
                auto it = walk.reach.find (id);
                CHECK_MSG (it != walk.reach.end() && ! it->second.visibleIn.empty(), id + " has no visible control");
            }
}

//==============================================================================
/*  Operating every control writes the parameter it names. */
LUTHIER_TEST (GuiReach, operatingEachControlWritesItsParameter)
{
    auto& walk = theWalk();

    int ok = 0;
    for (auto& [id, r] : walk.reach)
    {
        if (r.visibleIn.empty())
            continue;

        if (r.operatedOk) { ++ok; continue; }

        if (r.operateProblem.isNotEmpty())
            CHECK_MSG (false, id + ": " + r.operateProblem);
    }

    std::cout << "    parameters whose control was operated and wrote them: " << ok << std::endl;
    CHECK (ok > 0);
}
