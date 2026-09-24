#include "LiveControls.h"

#include "../Widgets.h"
#include "../CircuitPanel.h"

#include <set>

namespace luthier
{

//==============================================================================
// global-search.md 3.2: LearnTarget's constructor and destructor are the one
// place every parameter widget registers.
LearnTarget::LearnTarget()           { search::LiveControls::add (this); }
LearnTarget::~LearnTarget()          { search::LiveControls::remove (this); }

namespace search
{
namespace
{
    std::set<LearnTarget*>& registry()
    {
        static std::set<LearnTarget*> targets;
        return targets;
    }
}

void LiveControls::add (LearnTarget* target)       { registry().insert (target); }
void LiveControls::remove (LearnTarget* target)    { registry().erase (target); }
int LiveControls::getNumRegistered()               { return (int) registry().size(); }

std::vector<juce::Component*> LiveControls::find (const juce::String& parameterId)
{
    std::vector<juce::Component*> found;

    if (parameterId.isEmpty())
        return found;

    for (auto* target : registry())
        if (target->getLearnParameterId() == parameterId)
            if (auto* c = dynamic_cast<juce::Component*> (target))
                found.push_back (c);

    return found;
}

void LiveControls::forEach (const std::function<void (juce::Component&, const juce::String&)>& fn)
{
    // A copy: the callback may rebuild a panel, which adds and removes targets.
    const std::vector<LearnTarget*> snapshot (registry().begin(), registry().end());

    for (auto* target : snapshot)
        if (registry().count (target) > 0)
            if (auto* c = dynamic_cast<juce::Component*> (target))
                fn (*c, target->getLearnParameterId());
}

juce::Component* LiveControls::innerControl (juce::Component& learn)
{
    if (auto* k = dynamic_cast<LuthierKnob*> (&learn))   return &k->getSlider();
    if (auto* s = dynamic_cast<LuthierSlider*> (&learn)) return &s->getSlider();
    if (auto* c = dynamic_cast<LuthierChoice*> (&learn)) return &c->getComboBox();
    if (auto* t = dynamic_cast<LuthierToggle*> (&learn)) return &t->getButton();

    // Anything else: its first slider, box or button, else itself.
    for (auto* child : learn.getChildren())
        if (dynamic_cast<juce::Slider*> (child) != nullptr || dynamic_cast<juce::ComboBox*> (child) != nullptr
              || dynamic_cast<juce::Button*> (child) != nullptr)
            return child;

    return &learn;
}

//==============================================================================
void SearchAnchors::tag (juce::Component& component, const juce::String& placeId,
                         std::function<void()> open, const juce::String& title)
{
    auto& props = component.getProperties();
    props.set (placeProperty, placeId);

    if (title.isNotEmpty())
        props.set (titleProperty, title);

    if (open != nullptr)
        props.set (openerProperty, juce::var (juce::var::NativeFunction (
            [fn = std::move (open)] (const juce::var::NativeFunctionArgs&) -> juce::var
            {
                fn();
                return {};
            })));
}

juce::String SearchAnchors::getPlace (const juce::Component& component)
{
    return component.getProperties()[placeProperty].toString();
}

juce::String SearchAnchors::getPlaceTitle (const juce::Component& component)
{
    return component.getProperties()[titleProperty].toString();
}

bool SearchAnchors::hasOpener (const juce::Component& component)
{
    return component.getProperties()[openerProperty].isMethod();
}

bool SearchAnchors::open (juce::Component& component)
{
    const auto opener = component.getProperties()[openerProperty];

    if (! opener.isMethod())
        return false;

    const juce::var::NativeFunctionArgs args (juce::var(), nullptr, 0);
    opener.getNativeFunction() (args);
    return true;
}

void SearchAnchors::tagSetting (juce::Component& component, const juce::String& settingId, const juce::String& titleKey)
{
    component.getProperties().set (settingProperty, settingId);
    component.getProperties().set (settingTitleProperty, titleKey);
}

juce::String SearchAnchors::getSetting (const juce::Component& component)
{
    return component.getProperties()[settingProperty].toString();
}

juce::String SearchAnchors::getSettingTitleKey (const juce::Component& component)
{
    return component.getProperties()[settingTitleProperty].toString();
}

std::vector<SearchAnchors::Step> SearchAnchors::chain (juce::Component& c)
{
    std::vector<Step> steps;

    for (auto* p = &c; p != nullptr; p = p->getParentComponent())
    {
        const auto place = getPlace (*p);

        if (place.isNotEmpty())
            steps.insert (steps.begin(), { p, place });
    }

    return steps;
}

juce::Component* SearchAnchors::findPlace (juce::Component& root, const juce::String& placeId)
{
    if (getPlace (root) == placeId)
        return &root;

    for (auto* child : root.getChildren())
        if (auto* found = findPlace (*child, placeId))
            return found;

    return nullptr;
}

std::vector<SearchAnchors::Step> SearchAnchors::allPlaces (juce::Component& root)
{
    std::vector<Step> steps;

    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        if (const auto place = getPlace (c); place.isNotEmpty())
            steps.push_back ({ &c, place });

        for (auto* child : c.getChildren())
            walk (*child);
    };

    walk (root);
    return steps;
}

} // namespace search
} // namespace luthier
