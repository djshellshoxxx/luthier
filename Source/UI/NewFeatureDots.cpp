#include "NewFeatureDots.h"
#include "Theme.h"
#include "UiPreferences.h"

namespace luthier::NewFeatureDots
{

const std::vector<Entry>& getTable()
{
    // Add a row with the version that introduces an entry point after 1.0.
    static const std::vector<Entry> table {};
    return table;
}

void noteLaunch (const juce::String& version, juce::Time now)
{
    const auto key = "newFeatures.firstLaunch." + version;

    if (UiPreferences::get().getString (key, {}).isEmpty())
        UiPreferences::get().setString (key, juce::String (now.toMilliseconds()));
}

bool isNew (const juce::String& entryPoint, const juce::String& version, juce::Time now, const std::vector<Entry>& table)
{
    for (const auto& e : table)
    {
        if (entryPoint != e.entryPoint || version != e.version)
            continue;

        const auto first = UiPreferences::get().getString ("newFeatures.firstLaunch." + version, {});

        if (first.isEmpty())
            return true;   // this is the first launch

        const auto days = (now.toMilliseconds() - first.getLargeIntValue()) / (1000.0 * 60.0 * 60.0 * 24.0);
        return days >= 0.0 && days < kShowDays;
    }

    return false;
}

void apply (juce::Component& root, const juce::String& version, juce::Time now, const std::vector<Entry>& table)
{
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            juce::String name = child->getTitle();

            if (auto* b = dynamic_cast<juce::Button*> (child))
                name = b->getButtonText();

            const bool fresh = name.isNotEmpty() && isNew (name, version, now, table);

            if ((bool) child->getProperties().getWithDefault (kProperty, false) != fresh)
            {
                child->getProperties().set (kProperty, fresh);
                child->repaint();
            }

            walk (*child);
        }
    };

    walk (root);
}

void paintDot (juce::Graphics& g, juce::Component& c)
{
    if (! (bool) c.getProperties().getWithDefault (kProperty, false))
        return;

    const float r = 3.0f;
    g.setColour (Palette::accentBright);
    g.fillEllipse ((float) c.getWidth() - r * 2.0f - 3.0f, 3.0f, r * 2.0f, r * 2.0f);
}

} // namespace luthier::NewFeatureDots
