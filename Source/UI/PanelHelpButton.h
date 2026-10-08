#pragma once

/*  The ? on a panel (gui-integration.md 20: "Every panel with more than one row
    of controls has a ? icon; click opens Help pinned to that panel's docs").
    TUNE-HELP-ONBOARDING workstream.

    It carries the name of the panel it sits on - a column section heading, a
    workspace tab name, an Easy strip - and asks its host to open Help on it;
    HelpContent::findTopic turns the name into the topic. The first-week pulse
    (onboarding 4) is the DiscoveryLayer's, keyed by getDiscoveryKey().
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{

class PanelHelpButton : public juce::Button
{
public:
    explicit PanelHelpButton (const juce::String& topic = {});

    static constexpr int kSize = 18;

    void setTopic (const juce::String& newTopic);
    juce::String getTopic() const { return topic; }

    /** The pulse's "seen" key: one per panel. */
    juce::String getDiscoveryKey() const { return "help_icon_" + topic.toLowerCase().replaceCharacter (' ', '_'); }

    /** Called with the topic on click. */
    std::function<void (const juce::String& topic)> onHelp;

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override;

private:
    juce::String topic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelHelpButton)
};

} // namespace luthier
