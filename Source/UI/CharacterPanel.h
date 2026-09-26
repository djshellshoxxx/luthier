#pragma once

/*  The CHARACTER panel (character-wear.md section 10).

    The instrument's imperfections, and the two maps that make them editable: a
    fretboard showing where each string's dead spots are, and a strip showing how
    worn each fret is. Both are drag-editable, because the spec asks for the user
    to be able to nudge what the seed rolled.

    Like the routing, mod-matrix and rhythm panels, this is built as a section in
    Column 4's scrolling list rather than as a tab, because that column has no tab
    strip.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "NoiseGroups.h"
#include "SetupGroup.h"
#include "SlideGroup.h"
#include "SlapGroup.h"   // bass-techniques 9 (MODEL-GAPS)
#include "RealismGroups.h"
#include "HarmonicsGroup.h"          // REALISM-B: harmonic-realism.md 7
#include "StringInteractionGroup.h"  // REALISM-B: string-interaction.md 9
#include "RightHandGroup.h"          // REALISM-B: fingerstyle-attack.md 7
#include "RealismGroupsC.h"   // REALISM-C
#include "../Character/CharacterEngine.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** The dead-spot map (character-wear 10): a fretboard with each string's spots
    marked, draggable to move them and to change their depth. */
class DeadSpotMap : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    explicit DeadSpotMap (LuthierAudioProcessor& processor);
    ~DeadSpotMap() override;

    void refresh();

    std::function<void()> onEdited;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    /** SPEC-SWEEP: CW-29 - the wheel over a spot widens or narrows it (2-5 frets). */
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int rowHeight = 16;
    static constexpr int numFretsShown = 22;
    static constexpr int preferredHeight = rowHeight * 6 + 18;

private:
    CharacterEngine& character();

    /** Which string and fret a point falls on. */
    bool hitTest (juce::Point<int> position, int& stringIndex, double& fret) const;

    float fretX (double fret) const;

    LuthierAudioProcessor& processor;

    int draggingString = -1;
    int draggingSpot = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeadSpotMap)
};

//==============================================================================
/** The fret wear map (character-wear 10): one bar per fret, click-drag to set. */
class FretWearMap : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    explicit FretWearMap (LuthierAudioProcessor& processor);
    ~FretWearMap() override;

    void refresh();

    std::function<void()> onEdited;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    static constexpr int preferredHeight = 56;

private:
    CharacterEngine& character();

    void setFromPosition (juce::Point<int> position);

    LuthierAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FretWearMap)
};

//==============================================================================
class CharacterPanel : public juce::Component,
                       private juce::Timer
{
public:
    explicit CharacterPanel (LuthierAudioProcessor& processor);
    ~CharacterPanel() override;

    int preferredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void buildControls();
    void refreshFromEngine();

    CharacterEngine& character();

    LuthierAudioProcessor& processor;

    // --- seed --------------------------------------------------------------------
    juce::Label seedLabel;
    juce::TextButton newCharacterButton { "New Character" };

    // --- master -------------------------------------------------------------------
    std::unique_ptr<LuthierToggle> enableToggle;
    juce::Slider amountSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    // --- maps ---------------------------------------------------------------------
    std::unique_ptr<DeadSpotMap> deadSpotMap;
    std::unique_ptr<FretWearMap> fretWearMap;
    juce::TextButton refretButton { "Refret" };

    // --- tuners -------------------------------------------------------------------
    juce::Slider loosenessSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton retuneButton { "Retune all" };   // tuning-stability.md 3
    juce::Label driftLabel;

    // --- electronics ----------------------------------------------------------------
    juce::Slider potLinearitySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider capDriftSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton jackToggle { "Dodgy jack" };
    juce::TextButton boneNutToggle { "Bone nut" };

    // --- body and environment ----------------------------------------------------------
    juce::Slider bodyAgeSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label sessionLabel;

    // --- presets ------------------------------------------------------------------------
    juce::TextButton allFreshButton { "All fresh" }, allOldButton { "All old" };

    // --- STRING NOISE and PICK (gui-integration 4.4) ----------------------------------
    std::unique_ptr<NoiseGroups> noiseGroups;
    std::unique_ptr<SetupGroup> setupGroup;
    std::unique_ptr<SlideGroup> slideGroup;
    std::unique_ptr<SlapGroup> slapGroup;   // bass-techniques 9: only on a bass (MODEL-GAPS)

public:
    SlapGroup* getSlapGroup() const noexcept { return slapGroup.get(); }

private:

public:
    // REALISM-A groups; public for the tests.
    std::unique_ptr<StringAgingGroup> stringAgingGroup;
    std::unique_ptr<EnvironmentGroup> environmentGroup;
    std::unique_ptr<BodyCouplingGroup> bodyCouplingGroup;

private:

    // REALISM-B: PICK -> HARMONICS, RIGHT HAND, STRING INTERACTION (gui-integration 4.4).
    std::unique_ptr<HarmonicsGroup> harmonicsGroup;
    std::unique_ptr<RightHandGroup> rightHandGroup;
    std::unique_ptr<StringInteractionGroup> interactionGroup;

    // REALISM-C: tuning-stability.md 6, noise-floor.md 5, sustain-and-decay.md 8.
    std::unique_ptr<TuningStabilityGroup> tuningStabilityGroup;
    std::unique_ptr<NoiseFloorGroup> noiseFloorGroup;
    std::unique_ptr<SustainShapeGroup> sustainShapeGroup;

public:
    TuningStabilityGroup* getTuningStabilityGroup() noexcept { return tuningStabilityGroup.get(); }
    NoiseFloorGroup* getNoiseFloorGroup() noexcept { return noiseFloorGroup.get(); }
    SustainShapeGroup* getSustainShapeGroup() noexcept { return sustainShapeGroup.get(); }
    juce::TextButton& getRetuneAllButton() noexcept { return retuneButton; }

private:

    /*  Sizes the panel to its content. The workspace viewport keeps whatever
        height a panel gives itself, and this one never gave itself one - so it
        sat at the viewport's 80-point minimum and scrolled nothing. */
    void fitToContent();
    juce::Label seedHeading, mapsHeading, tunerHeading, electronicsHeading,
                bodyHeading;

    bool updatingControls = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CharacterPanel)
};

} // namespace luthier
