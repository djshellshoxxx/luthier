#include "RangesUi.h"
#include "Theme.h"
#include "UiPreferences.h"
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace luthier
{
namespace RangesUi
{

const char* const kExplainerText =
    "This control has a stock range that matches real guitars and an "
    "advanced range for exaggerated effects. You're leaving the stock "
    "range. Values marked with * play back the same; presets with any "
    "advanced values show a padlock icon. Options -> Ranges lets you set "
    "the default.";

const char* const kLockedNoticeText =
    "This preset uses stock ranges. Options -> Ranges to unlock, or "
    "right-click to unlock this control only for this preset.";

//==============================================================================
bool markInWarningColour()
{
    return UiPreferences::get().getBool (kWarningColourKey, true);
}

void setMarkInWarningColour (bool shouldMark)
{
    UiPreferences::get().setBool (kWarningColourKey, shouldMark);
}

bool randomiseRespectsStock()
{
    return UiPreferences::get().getBool (kRandomiseInStockKey, true);
}

void setRandomiseRespectsStock (LuthierAudioProcessor& processor, bool shouldRespect)
{
    UiPreferences::get().setBool (kRandomiseInStockKey, shouldRespect);
    processor.setRandomiseRespectsStock (shouldRespect);
}

//==============================================================================
void tagSlider (juce::Slider& slider, const juce::String& parameterId)
{
    auto& properties = slider.getProperties();

    if (const auto* physical = RangeRegistry::find (parameterId))
    {
        properties.set (kStockMinProperty, physical->stockMin);
        properties.set (kStockMaxProperty, physical->stockMax);
    }
    else
    {
        properties.remove (kStockMinProperty);
        properties.remove (kStockMaxProperty);
    }
}

bool isMarked (const LuthierAudioProcessor& processor, const juce::String& parameterId)
{
    const auto* physical = RangeRegistry::find (parameterId);

    if (physical == nullptr)
        return false;

    auto& state = const_cast<LuthierAudioProcessor&> (processor).getState();

    if (auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (parameterId)))
        return physical->isOutsideStock (parameter->get());

    return false;
}

juce::String markReadout (const LuthierAudioProcessor& processor,
                          const juce::String& parameterId,
                          const juce::String& text)
{
    return isMarked (processor, parameterId) ? text + "*" : text;
}

juce::String formatValue (const LuthierAudioProcessor& processor,
                          const juce::String& parameterId, float plain)
{
    auto& state = const_cast<LuthierAudioProcessor&> (processor).getState();

    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (parameterId)))
        return parameter->getText (parameter->convertTo0to1 (plain), 16);

    return juce::String (plain, 2);
}

juce::String familyDisplayName (RangeFamily family)
{
    switch (family)
    {
        case RangeFamily::amp:        return "Amp";
        case RangeFamily::circuit:    return "Circuit";
        case RangeFamily::squeak:     return "Squeak";
        case RangeFamily::buzz:       return "Buzz";
        case RangeFamily::pick:       return "Pick";
        case RangeFamily::slide:      return "Slide";
        case RangeFamily::modulation: return "Modulation";
        case RangeFamily::numFamilies:
        default:                      return {};
    }
}

//==============================================================================
void resyncControls (juce::Component& root)
{
    if (auto* knob = dynamic_cast<LuthierKnob*> (&root))
        knob->resyncRange();
    else if (auto* slider = dynamic_cast<LuthierSlider*> (&root))
        slider->resyncRange();

    for (auto* child : root.getChildren())
        resyncControls (*child);
}

//==============================================================================
namespace
{
    /*  onboarding.md 7's popover: the words and one button. A CallOutBox so it
        points at the control that caused it, which is what makes it read as an
        explanation of that control rather than an announcement. */
    class ExplainerContent : public juce::Component
    {
    public:
        explicit ExplainerContent (const juce::String& text)
        {
            message.setText (text, juce::dontSendNotification);
            message.setFont (Fonts::ui (13.0f));
            message.setColour (juce::Label::textColourId, Palette::textPrimary);
            message.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (message);

            okButton.onClick = [this]
            {
                if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                    box->dismiss();
            };
            addAndMakeVisible (okButton);

            setSize (340, 150);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (10);
            okButton.setBounds (bounds.removeFromBottom (26).removeFromRight (110));
            bounds.removeFromBottom (6);
            message.setBounds (bounds);
        }

    private:
        juce::Label message;
        juce::TextButton okButton { "OK, got it" };
    };
}

void showExplainerIfFirstTime (juce::Component& anchor)
{
    auto& preferences = UiPreferences::get();

    // Not showing means not seen, so it stays owed.
    if (preferences.getBool (kExplainerShownKey, false) || ! anchor.isShowing())
        return;

    // Recorded as shown before it is on screen: a host that kills the window
    // mid-popover should not show it again, because it has been seen.
    preferences.setBool (kExplainerShownKey, true);

    juce::CallOutBox::launchAsynchronously (std::make_unique<ExplainerContent> (kExplainerText),
                                            anchor.getScreenBounds(), nullptr);
}

int apply (LuthierAudioProcessor& processor,
           const RangeState& newState,
           const juce::String& undoDescription,
           juce::Component* anchor)
{
    // 6.4: the first transition of any control to advanced, not the first
    // attempt while locked - the attempt gets the inline notice instead.
    bool widens = false;

    for (const auto& id : RangeRegistry::allIds())
        widens = widens || (newState.isParameterAdvanced (id)
                              && ! processor.getRanges().isParameterAdvanced (id));

    const int clamped = processor.changeRanges (newState, undoDescription);

    if (anchor != nullptr && widens)
        showExplainerIfFirstTime (*anchor);

    return clamped;
}

} // namespace RangesUi
} // namespace luthier
