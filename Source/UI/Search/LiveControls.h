#pragma once

/*  global-search.md 3.2: where a parameter's control is, found live.

    LearnTarget (Widgets.h) is the base of every parameter-bound widget -
    LuthierKnob, LuthierChoice, LuthierToggle, LuthierSlider,
    StandardValueChoice - and its constructor and destructor add and remove it
    here. That is the single insertion point: every control, including a pedal
    card's knobs rebuilt when its pedal changes, is findable without anyone
    registering it.

    SearchAnchors tags the components a control sits inside ("tab:CHARACTER",
    "options:AUDIO", "drawer:METRO"...) with the function that opens them, so
    the navigator can derive the route to any control by walking up its
    parents - there is no second table of where things are to fall out of
    date.

    Message thread only.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace luthier
{
struct LearnTarget;
}

namespace luthier::search
{

//==============================================================================
namespace LiveControls
{
    void add (LearnTarget* target);
    void remove (LearnTarget* target);

    int getNumRegistered();

    /** Bumped on every add and remove: the window's controls changed. */
    int getChangeCount();

    /** Every registered control editing `parameterId`, as components. */
    std::vector<juce::Component*> find (const juce::String& parameterId);

    /** Calls `fn (component, parameterId)` for every registered control. */
    void forEach (const std::function<void (juce::Component&, const juce::String&)>& fn);

    /** The component a LearnTarget operates: its slider, box or button - what
        gets keyboard focus (4.2 step 7), as the reachability walk's
        innerControl does. */
    juce::Component* innerControl (juce::Component& learnTarget);
}

//==============================================================================
namespace SearchAnchors
{
    constexpr const char* placeProperty   = "luthier.place";
    constexpr const char* openerProperty  = "luthier.place.open";
    constexpr const char* titleProperty   = "luthier.place.title";
    constexpr const char* settingProperty = "luthier.setting";
    constexpr const char* settingTitleProperty = "luthier.setting.title";

    /** Tags a host component with its place id and, optionally, the function
        that brings it on screen (select its tab, open its page...). Hosts call
        this once when they build. */
    void tag (juce::Component& component, const juce::String& placeId,
              std::function<void()> open = {}, const juce::String& title = {});

    juce::String getPlace (const juce::Component& component);
    juce::String getPlaceTitle (const juce::Component& component);

    /** Runs the component's opener, if it has one. */
    bool open (juce::Component& component);
    bool hasOpener (const juce::Component& component);

    /** 3.2: a non-parameter setting ("set:APPEARANCE:reducedMotion"). */
    void tagSetting (juce::Component& component, const juce::String& settingId, const juce::String& titleKey);
    juce::String getSetting (const juce::Component& component);
    juce::String getSettingTitleKey (const juce::Component& component);

    struct Step
    {
        juce::Component* component = nullptr;
        juce::String placeId;
    };

    /** The tagged ancestors of `c`, outermost first, up to and excluding the
        component with no parent. */
    std::vector<Step> chain (juce::Component& c);

    /** The first descendant of `root` (depth first) tagged `placeId`. */
    juce::Component* findPlace (juce::Component& root, const juce::String& placeId);

    /** Every tagged descendant of `root`, depth first. */
    std::vector<Step> allPlaces (juce::Component& root);
}

} // namespace luthier::search
