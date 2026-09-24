#include "TechniquesPanel.h"
#include "../UiPreferences.h"
#include "../../PluginProcessor.h"

namespace luthier
{

//==============================================================================
/** 10: "vertical text with generous padding". */
class TechniquesPanel::RailButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
        const bool on = getToggleState();

        g.setColour (on ? Palette::panelRaised : (over || down ? Palette::panel : Palette::background));
        g.fillRoundedRectangle (area, Metrics::controlCorner);

        if (on)
        {
            g.setColour (Palette::accent);
            g.fillRect (area.withWidth (2.5f));
        }

        if (hasKeyboardFocus (false))
        {
            g.setColour (Palette::textPrimary);
            g.drawRoundedRectangle (area, Metrics::controlCorner, 1.0f);
        }

        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                         area.getCentreX(), area.getCentreY()));
        const auto rotated = area.withSizeKeepingCentre (area.getHeight(), area.getWidth());
        g.setColour (on ? Palette::accentBright : Palette::textMuted);
        g.setFont (Fonts::ui (11.0f, true));
        g.drawText (getButtonText(), rotated, juce::Justification::centred, false);
        g.restoreState();
    }
};

//==============================================================================
const char* TechniquesPanel::getSubTabName (int index) noexcept
{
    if (index == TechniqueTable::cascadeSubTab)
        return "CASCADE";

    return TechniqueTable::get ((TechniqueSlot) juce::jlimit (0, TechniqueTable::count - 1, index)).name;
}

TechniquesPanel::TechniquesPanel (LuthierAudioProcessor& p)
    : processor (p)
{
    setComponentID (kOnboardingAnchor);

    pages[(size_t) TechniqueSlot::scrape] = std::make_unique<ScrapePage> (p);
    pages[(size_t) TechniqueSlot::slide]  = std::make_unique<SlidePage> (p);
    pages[(size_t) TechniqueSlot::slap]   = std::make_unique<SlapPage> (p);
    pages[(size_t) TechniqueSlot::mute]   = std::make_unique<MutePage> (p);
    pages[(size_t) TechniqueSlot::tap]    = std::make_unique<TapPage> (p);
    pages[(size_t) TechniqueSlot::bend]   = std::make_unique<BendPage> (p);
    pages[(size_t) TechniqueTable::cascadeSubTab] = std::make_unique<CascadePage> (p);

    for (int i = 0; i < kNumSubTabs; ++i)
    {
        auto* button = rail.add (new RailButton (getSubTabName (i)));
        button->setClickingTogglesState (true);
        button->setRadioGroupId (0x7ec);
        button->onClick = [this, i] { showSubTab (i); };
        button->setTooltip (i == TechniqueTable::cascadeSubTab
                              ? juce::String ("Which techniques are live on which strings, and what conflicts")
                              : juce::String (TechniqueTable::get ((TechniqueSlot) i).summary));
        AccessibleSetup::configureButton (*button, juce::String (getSubTabName (i)) + " sub-tab",
                                          "Shows the " + juce::String (getSubTabName (i)) + " controls");

        if (i == TechniqueTable::cascadeSubTab)
            button->setComponentID (kOnboardingCascadeAnchor);

        addAndMakeVisible (button);

        if (i < TechniqueTable::count)
        {
            // 1: "Each sub-tab has an arm pill at the top".
            auto* pill = armPills.add (new TechniquePill (p, (TechniqueSlot) i));
            pill->onHold = [] (TechniqueSlot) {};
            addChildComponent (pill);
        }
    }

    viewport.setViewedComponent (&holder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    for (auto& page : pages)
        holder.addChildComponent (*page);

    showSubTab (juce::jlimit (0, kNumSubTabs - 1, UiPreferences::get().getInt (kSubTabPreferenceKey, 0)));
}

TechniquesPanel::~TechniquesPanel() = default;

juce::Component* TechniquesPanel::getPage (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, kNumSubTabs) ? pages[(size_t) index].get() : nullptr;
}

TechniquePill* TechniquesPanel::getArmPill (int index) const noexcept
{
    return armPills[index];
}

void TechniquesPanel::showSubTab (int index)
{
    shown = juce::jlimit (0, kNumSubTabs - 1, index);

    for (int i = 0; i < kNumSubTabs; ++i)
    {
        rail[i]->setToggleState (i == shown, juce::dontSendNotification);
        pages[(size_t) i]->setVisible (i == shown);

        if (auto* pill = armPills[i])
            pill->setVisible (i == shown);
    }

    UiPreferences::get().setInt (kSubTabPreferenceKey, shown);
    resized();
}

void TechniquesPanel::resized()
{
    auto area = getLocalBounds();

    auto railArea = area.removeFromLeft (kRailWidth);
    const int buttonH = juce::jlimit (48, 96, railArea.getHeight() / juce::jmax (1, kNumSubTabs));

    for (auto* b : rail)
        b->setBounds (railArea.removeFromTop (buttonH));

    area.removeFromLeft (Metrics::grid);

    if (auto* pill = armPills[shown])
    {
        auto top = area.removeFromTop (TechniquePill::preferredHeight + Metrics::grid);
        pill->setBounds (top.removeFromLeft (TechniquePill::preferredWidth + 20).withHeight (TechniquePill::preferredHeight));
    }

    viewport.setBounds (area);

    const int width = juce::jmax (120, area.getWidth() - viewport.getScrollBarThickness() - 2);
    auto& page = *pages[(size_t) shown];
    const int height = juce::jmax (area.getHeight(), page.getHeightForWidth (width));

    holder.setSize (width, height);
    page.setBounds (0, 0, width, height);
}

void TechniquesPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // 10: a subtle divider between the rail and the panel.
    g.setColour (Palette::edge);
    g.drawVerticalLine (kRailWidth + Metrics::gridHalf, 0.0f, (float) getHeight());
}

} // namespace luthier
