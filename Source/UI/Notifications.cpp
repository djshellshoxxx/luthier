#include "Notifications.h"

namespace luthier
{

//==============================================================================
NotificationCentre::NotificationCentre()
{
    setVisible (false);
    setInterceptsMouseClicks (true, true);

    actionButton.setVisible (false);
    actionButton.onClick = [this] { performCurrentAction(); };

    addChildComponent (actionButton);

    secondaryButton.setVisible (false);
    secondaryButton.onClick = [this] { performCurrentSecondaryAction(); };
    addChildComponent (secondaryButton);
}

void NotificationCentre::updateButtons()
{
    actionButton.setButtonText (current.actionText);
    actionButton.setVisible (current.action != nullptr);
    secondaryButton.setButtonText (current.secondaryActionText);
    secondaryButton.setVisible (currentHasSecondaryAction());
}

void NotificationCentre::performCurrentSecondaryAction()
{
    if (! currentHasSecondaryAction())
        return;

    auto action = current.secondaryAction;
    dismissCurrent();
    action();
}

NotificationCentre::~NotificationCentre()
{
    stopTimer();
}

//==============================================================================
bool NotificationCentre::contains (const juce::String& id) const
{
    if (current.id == id)
        return true;

    for (const auto& queued : queue)
        if (queued.id == id)
            return true;

    return false;
}

void NotificationCentre::post (Notification notification)
{
    // An id is required: without one there is no way to tell a repeat from a
    // second piece of news, and the countdown triggers repeat by nature.
    jassert (notification.id.isNotEmpty());

    if (notification.id.isEmpty())
        return;

    /*  Already on screen. Update the wording in place - a countdown that went
        from 3 days to 2 should say so - but do not restart the clock, or a
        trigger that reposts every second would never time out. */
    if (current.id == notification.id)
    {
        const bool hadAction = current.action != nullptr;

        current = std::move (notification);

        updateButtons();

        // If it has gained or lost its action, the timer rule changes with it.
        if (hadAction != (current.action != nullptr))
        {
            if (current.action != nullptr)
                stopTimer();
            else
                startTimer (autoDismissMs);
        }

        resized();
        repaint();
        return;
    }

    for (auto& queued : queue)
    {
        if (queued.id == notification.id)
        {
            queued = std::move (notification);
            return;
        }
    }

    queue.push_back (std::move (notification));

    if (! isShowingNotification())
        showNext();
}

//==============================================================================
void NotificationCentre::showNext()
{
    stopTimer();

    const bool wasVisible = isVisible();

    if (queue.empty())
    {
        current = {};

        actionButton.setVisible (false);
        secondaryButton.setVisible (false);
        setVisible (false);

        if (wasVisible && onVisibilityChanged != nullptr)
            onVisibilityChanged();

        return;
    }

    // SPEC-SWEEP ER-81: the most severe waiting banner first, oldest first within a level.
    auto next = queue.begin();

    for (auto it = queue.begin(); it != queue.end(); ++it)
        if ((int) it->level > (int) next->level)
            next = it;

    current = std::move (*next);
    queue.erase (next);

    updateButtons();

    setVisible (true);

    /*  Section 15: five seconds, unless there is something to do about it. The
        timer is never started for an actionable banner rather than being started
        and stopped, so there is no window where the button vanishes mid-reach. */
    if (current.action == nullptr)
        startTimer (autoDismissMs);

    resized();
    repaint();

    if (! wasVisible && onVisibilityChanged != nullptr)
        onVisibilityChanged();
}

void NotificationCentre::dismissCurrent()
{
    if (! isShowingNotification())
        return;

    showNext();
}

void NotificationCentre::clear()
{
    queue.clear();

    if (isShowingNotification())
        showNext();
}

void NotificationCentre::performCurrentAction()
{
    if (current.action == nullptr)
        return;

    /*  Taken by value before the dismiss, because dismissing overwrites `current`
        and the action would be destroyed while running. */
    auto action = current.action;

    dismissCurrent();
    action();
}

void NotificationCentre::timerCallback()
{
    dismissCurrent();
}

//==============================================================================
juce::Rectangle<int> NotificationCentre::getDismissBounds() const
{
    return getLocalBounds().removeFromRight (preferredHeight);
}

void NotificationCentre::resized()
{
    if (current.action == nullptr)
    {
        actionButton.setBounds ({});
        secondaryButton.setBounds ({});
        return;
    }

    auto bounds = getLocalBounds();
    bounds.removeFromRight (preferredHeight);           // the dismiss cross

    if (currentHasSecondaryAction())
        secondaryButton.setBounds (bounds.removeFromRight (Metrics::grid * 16).reduced (Metrics::gridHalf, 5));
    else
        secondaryButton.setBounds ({});

    bounds = bounds.removeFromRight (Metrics::grid * 14);

    actionButton.setBounds (bounds.reduced (Metrics::gridHalf, 5));
}

void NotificationCentre::paint (juce::Graphics& g)
{
    if (! isShowingNotification())
        return;

    const auto tint = current.level == Notification::Level::error   ? Palette::clip
                    : current.level == Notification::Level::warning ? Palette::warning
                                                                    : Palette::secondary;

    auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (tint.withAlpha (0.12f));
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (tint.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // The same marker bar InlineNotice uses, so the two read as one family.
    g.setColour (tint);
    g.fillRect (bounds.withWidth (3.0f).reduced (0.0f, 5.0f));

    /*  The text stops where the button starts. Without this the message runs
        under the action button and the ellipsis lands in the wrong place, which
        is only visible on the longest message and so is exactly the kind of
        thing that ships. */
    auto textArea = getLocalBounds().reduced (Metrics::grid + 2, 0);
    textArea.removeFromRight (getDismissBounds().getWidth());

    if (current.action != nullptr)
        textArea.removeFromRight (actionButton.getWidth() + Metrics::grid);

    if (currentHasSecondaryAction())
        textArea.removeFromRight (secondaryButton.getWidth() + Metrics::grid);

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (11.5f));
    g.drawText (current.message, textArea, juce::Justification::centredLeft, true);

    // The dismiss cross.
    const auto cross = getDismissBounds().toFloat().reduced (11.0f);

    g.setColour (Palette::textMuted);
    g.drawLine (cross.getX(), cross.getY(), cross.getRight(), cross.getBottom(), 1.2f);
    g.drawLine (cross.getX(), cross.getBottom(), cross.getRight(), cross.getY(), 1.2f);

    /*  How many are waiting. Without this a burst at startup looks like one
        notification, and dismissing it appears to summon another from nowhere. */
    if (! queue.empty())
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (9.0f));
        g.drawText ("+" + juce::String ((int) queue.size()),
                    getDismissBounds().translated (-getDismissBounds().getWidth(), 0),
                    juce::Justification::centred, false);
    }
}

void NotificationCentre::mouseDown (const juce::MouseEvent& e)
{
    /*  Section 15 says dismissible, and the cross is where a user looks for it.
        Clicking anywhere else on a banner that has no action dismisses it too -
        there is nothing else the click could mean - but a banner with a button
        only goes on the cross, so a near-miss on the button does not throw away
        the thing it was offering. */
    if (getDismissBounds().contains (e.getPosition()) || current.action == nullptr)
        dismissCurrent();
}

} // namespace luthier
