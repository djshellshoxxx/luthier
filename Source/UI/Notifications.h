#pragma once

/*  Notification banners, gui-integration.md section 15.

    "Non-modal banners under the header strip, 32 px, dismissible." Nine triggers
    are listed, and they have one thing in common: every one of them is news the
    plugin learned on its own. A crash dump found at startup, a licence sliding
    into its grace period, an update on the server, a policy file an administrator
    dropped on the machine - the user did nothing to ask about any of it.

    That is what separates this from `InlineNotice`, which the window already has
    and which stays. An InlineNotice answers something the user just did, at the
    place they did it: Advanced Mode refusing to open at 940 points puts its
    reason in the panel that will not open. A banner interrupts with something
    unrelated to whatever is being done at the time. Same visual idea, different
    job, and folding them together would mean either the refusal floating at the
    top of the window away from the control that caused it, or the crash report
    appearing at the bottom of a panel it has nothing to do with.

    Section 15's two rules that are not about pixels:

      - "dismissible" - every banner, always, with no exception for the important
        ones. A banner that cannot be got rid of is a modal dialog wearing a
        different hat.
      - "auto-dismiss after 5 s unless they contain an action" - news with nothing
        to do about it should not need clearing, and news with a button must wait
        for an answer. The timer is not started at all in the second case, rather
        than started and cancelled, so there is no window in which a slow hand
        loses the button.

    Banners queue rather than stack. Two at 32 px each would be 64 px of window
    gone, and the triggers arrive in bursts - a crash dump and a policy file are
    both found in the same second at startup - so one is shown at a time and the
    rest wait. Posting an id that is already queued replaces it instead of adding
    a second copy, because the alternative is a countdown that posts itself once a
    second and a queue that never empties.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace luthier
{

//==============================================================================
/** One thing the plugin wants to tell the user about. */
struct Notification
{
    /*  SPEC-SWEEP ER-81, error-recovery 14: "errors > warnings > info". The
        queue shows the most severe waiting banner next (in arrival order within
        a level); an error is drawn in the clip red. */
    enum class Level { info, warning, error };

    /*  Identity, not text. Two postings with the same id are the same piece of
        news said twice - a licence countdown that ticked, a crash report noticed
        again - and the second replaces the first rather than joining it. */
    juce::String id;

    juce::String message;
    Level level = Level::info;

    /** Optional. A banner with action text gets a button and no auto-dismiss. */
    juce::String actionText;
    std::function<void()> action;

    /** Optional second action, beside the first (output-normalization.md 5.3:
        [Options] and [Don't show again]). Only shown when `action` is set. */
    juce::String secondaryActionText;
    std::function<void()> secondaryAction;
};

//==============================================================================
/** The banner strip under the header. Shows one notification at a time. */
class NotificationCentre final : public juce::Component,
                                 private juce::Timer
{
public:
    NotificationCentre();
    ~NotificationCentre() override;

    /** Section 15: 32 px. */
    static constexpr int preferredHeight = 32;

    /** Section 15: "auto-dismiss after 5 s unless they contain an action". */
    static constexpr int autoDismissMs = 5000;

    /** Queues one. If its id is already showing it is updated in place without
        restarting the timer; if its id is waiting in the queue it replaces that
        entry. Either way nothing is ever shown twice. */
    void post (Notification notification);

    /** Drops the one on screen and shows the next, if there is one. */
    void dismissCurrent();

    /** Drops everything, shown and queued. */
    void clear();

    bool isShowingNotification() const noexcept { return current.id.isNotEmpty(); }

    juce::String getCurrentId() const      { return current.id; }
    juce::String getCurrentMessage() const { return current.message; }

    bool currentHasAction() const noexcept { return current.action != nullptr; }

    /** How many are waiting behind the one on screen. */
    int getNumQueued() const noexcept { return (int) queue.size(); }

    /*  Whether the five-second clock is running on the banner that is showing.
        Section 15's rule - auto-dismiss unless there is an action - is otherwise
        only observable by waiting five seconds, which is not a test, and the rule
        is the whole reason an actionable banner does not evaporate mid-reach. */
    bool isAutoDismissScheduled() const noexcept { return isTimerRunning(); }

    /** Runs the current banner's action and dismisses it, which is what clicking
        the button does. Does nothing when there is no action. */
    void performCurrentAction();

    /** The second button, when the banner has one. */
    void performCurrentSecondaryAction();
    bool currentHasSecondaryAction() const noexcept { return current.action != nullptr && current.secondaryAction != nullptr; }

    /** True if this id is on screen or waiting. */
    bool contains (const juce::String& id) const;

    /** Called whenever the strip appears or disappears, so the window can give
        the space back. */
    std::function<void()> onVisibilityChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void showNext();
    void timerCallback() override;

    /** The dismiss cross, at the right-hand end. */
    juce::Rectangle<int> getDismissBounds() const;

    Notification current;
    std::vector<Notification> queue;

    juce::TextButton actionButton, secondaryButton;
    void updateButtons();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotificationCentre)
};

} // namespace luthier
