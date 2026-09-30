#pragma once

/*  Column 4's TECHNIQUES tab (gui-techniques-updates.md 1, 10).

    +-----------------+-----------------------------------------------+
    | [sub-tab rail]  |  [arm pill]                                   |
    | SCRAPE          |  [active technique panel]                     |
    | SLIDE ...       |                                               |
    | CASCADE         |                                               |
    +-----------------+-----------------------------------------------+

    A vertical rail of seven sub-tabs, each with vertical text, then the
    active page under its arm pill (CASCADE has no pill: it is an overview).
    The page scrolls when the column is short.

    onboarding.md (via gui-techniques-updates.md 8): the tour's Techniques
    stop points at this panel. `kOnboardingAnchor` is its component ID, and
    `getOnboardingAnchor()` the component - the tour itself is another
    workstream's.
*/

#include "TechniquePages.h"

namespace luthier
{

class TechniquesPanel : public juce::Component
{
public:
    explicit TechniquesPanel (LuthierAudioProcessor& processor);
    ~TechniquesPanel() override;

    static constexpr int kNumSubTabs = TechniqueTable::count + 1;
    static const char* getSubTabName (int index) noexcept;

    int getNumSubTabs() const noexcept { return kNumSubTabs; }
    int getSubTab() const noexcept { return shown; }
    void showSubTab (int index, bool persist = true);

    /** Selects the sub-tab of a technique (the pills' right-click). */
    void showTechnique (TechniqueSlot slot) { showSubTab ((int) slot); }

    juce::Component* getPage (int index) const noexcept;
    TechniquePill* getArmPill (int index) const noexcept;
    juce::Button* getRailButton (int index) const noexcept { return rail[index]; }

    /** The Techniques stop of the onboarding tour (onboarding.md, gui-techniques-updates 8). */
    static constexpr const char* kOnboardingAnchor = "onboarding.techniques";
    juce::Component& getOnboardingAnchor() noexcept { return *this; }

    /** The cascade stop within it: the tour points at the CASCADE sub-tab's rail button. */
    static constexpr const char* kOnboardingCascadeAnchor = "onboarding.techniques.cascade";

    void resized() override;
    void paint (juce::Graphics&) override;

    static constexpr int kRailWidth = 34;

    /** The UiPreferences key the last sub-tab is kept under. */
    static constexpr const char* kSubTabPreferenceKey = "techniques.subTab";

private:
    class RailButton;

    LuthierAudioProcessor& processor;
    juce::OwnedArray<juce::Button> rail;
    std::array<std::unique_ptr<ControlFlow>, (size_t) kNumSubTabs> pages;
    juce::OwnedArray<TechniquePill> armPills;
    juce::Viewport viewport;
    juce::Component holder;
    int shown = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TechniquesPanel)
};

} // namespace luthier
