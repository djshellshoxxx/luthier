#pragma once

/*  output-normalization.md 5: the UI of output normalization, other than the
    header badge (NormalizationBadge.h).

      NormalizationOptionsGroup   Options -> AUDIO, under Oversampling (5.1):
                                  the switch, the target, the readout and the
                                  permanent warning caption.
      NormalizationCaption        a one-line note that shows itself only while
                                  normalization is on: the ROUTING caption and
                                  the Workshop bench note (5.4).
      NormalizationUi             the banner (5.3), focusing the switch, the
                                  Diagnostics / debug-overlay lines (5.4), and
                                  the UiPreferences defaults (6).
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Notifications.h"
#include "../Support/OutputNormalization.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class NormalizationOptionsGroup final : public juce::Component,
                                        private juce::Timer
{
public:
    explicit NormalizationOptionsGroup (LuthierAudioProcessor& processor);
    ~NormalizationOptionsGroup() override;

    /** The height AudioPage reserves for it. */
    static constexpr int preferredHeight = 196;

    /** The switch's component ID, so the badge can focus it (5.1). */
    static constexpr const char* switchComponentId = "normalizationSwitch";

    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

    // Tests.
    juce::ToggleButton& getSwitch() noexcept      { return toggle; }
    juce::ComboBox& getTargetBox() noexcept       { return target; }
    juce::Label& getReadout() noexcept            { return readout; }
    juce::Label& getCaption() noexcept            { return caption; }

    /** 9: announcements actually sent (tests of the 2 s rate limit). */
    int getNumAnnouncements() const noexcept { return announcements; }

    /** Tests: the clock the rate limit reads, in ms. */
    std::function<juce::uint32()> clock = [] { return juce::Time::getMillisecondCounter(); };

private:
    void timerCallback() override { refresh(); }

    LuthierAudioProcessor& processor;

    juce::ToggleButton toggle;
    juce::Label targetLabel;
    juce::ComboBox target;
    juce::Label readout, caption, note;

    bool updating = false;
    double lastAnnouncedDb = 1.0e9;
    juce::uint32 lastAnnouncementMs = 0;
    int announcements = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NormalizationOptionsGroup)
};

//==============================================================================
/** A muted one-line note, visible only while normalization is on. */
class NormalizationCaption final : public juce::Label,
                                   private juce::Timer
{
public:
    NormalizationCaption (LuthierAudioProcessor& processor, const juce::String& localeKey);
    ~NormalizationCaption() override;

    /** Shows or hides it to match the switch (the timer does this at 4 Hz). */
    void update();

private:
    void timerCallback() override { update(); }

    LuthierAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NormalizationCaption)
};

//==============================================================================
namespace NormalizationUi
{
    /** 5.3: posts the `normalization.on` banner unless the user suppressed it.
        `openOptions` is the [Options] action. */
    void postEnabledBanner (NotificationCentre& centre, std::function<void()> openOptions);

    /** 12: "Normalization could not measure this sound", once per session. */
    void postFailureBanner (NotificationCentre& centre);

    /** 5.1: focuses the switch, wherever it is under `root`. */
    bool focusSwitchIn (juce::Component& root);

    /** 5.4: "What's on the audio path" / State Inspector / debug overlay lines. */
    juce::StringArray diagnosticsLines (const LuthierAudioProcessor& processor);

    /** 6: records the user's last choice as the default for new instances. */
    void rememberAsDefault (const OutputNormalization& normalization);

    /** 9: the rebindable command (unbound by default). Turning the switch on
        this way posts the banner, like the switch. */
    void toggleFromCommand (LuthierAudioProcessor& processor);
}

} // namespace luthier
