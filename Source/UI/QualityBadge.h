#pragma once

/*  cpu-quality-modes.md 5: the footer badge, and the editor's link to the
    quality controller.

        [ MED ] ▮▮▮▯▯ 41%   latency 133 smp

    QualityBadge replaces the CPU text the editor used to paint (gui-integration
    12, "Right: CPU %, voice count"). It is a focusable button, last in the
    footer's tab order: click, Enter or Space opens Options -> AUDIO with focus
    in the CPU quality group. A LiveReadout at 4 Hz; "-" once the figure has
    been stale for 5 s, and before anything has played.

    QualityEditorLink is what one open editor does for the feature:
      - feeds this instance's level and relief into AnimationPolicy (the most
        restrictive open editor wins), and withdraws it on close;
      - turns the controller's notices into banners (gui-integration 15) and
        polite screen-reader announcements, at most one per 5 s;
      - re-reads performance.json on open (another process may have changed it);
      - the "Cycle CPU quality" shortcut.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "AnimationPolicy.h"

namespace luthier
{

class LuthierAudioProcessor;
class NotificationCentre;

//==============================================================================
class QualityBadge final : public juce::Component,
                           public juce::SettableTooltipClient,
                           private juce::Timer
{
public:
    explicit QualityBadge (LuthierAudioProcessor& processor);
    ~QualityBadge() override;

    /** 132 x 16 at the footer's size. */
    static constexpr int preferredWidth = 132;

    /** Click, Enter or Space. */
    std::function<void()> onOpen;

    /** What the badge shows, for the tests (CQ-26). */
    juce::String getLabelText() const;
    juce::String getShareText() const;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    /** Re-reads the controller and the load now (the timer does it at 4 Hz). */
    void refresh();

private:
    void timerCallback() override { refresh(); }

    LuthierAudioProcessor& processor;
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "QualityBadge" };

    juce::String label, share;
    double load = 0.0;
    bool stale = true;
    long long lastBlocks = -1;
    double lastBlocksChangedMs = 0.0;
};

//==============================================================================
class QualityEditorLink final : private juce::ChangeListener,
                                private juce::Timer
{
public:
    QualityEditorLink (LuthierAudioProcessor& processor, NotificationCentre& notifications);
    ~QualityEditorLink() override;

    /** The shortcut: High -> Medium -> Low -> Auto -> High, on the global setting
        (or on this instance's override when it has one). */
    void cycleQuality();

    /** Runs one update now (the 4 Hz timer does it otherwise), for the tests. */
    void pumpForTesting() { update(); }

    /** Called with QualityOptions::oversamplingNoteFor whenever it changes
        (column 3's Master oversampling tooltip). */
    std::function<void (const juce::String&)> onOversamplingNote;

    /** Announcements actually posted, for the tests (CQ-27). */
    int getAnnouncementCount() const noexcept { return announcements; }

    static constexpr double kAnnouncementGapMs = 5000.0;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { update(); }
    void timerCallback() override { update(); }
    void update();

    LuthierAudioProcessor& processor;
    NotificationCentre& notifications;

    double lastAnnouncementMs = -1.0e12;
    juce::String pendingAnnouncement, lastOversamplingNote { "?" };
    int announcements = 0;
};

} // namespace luthier
