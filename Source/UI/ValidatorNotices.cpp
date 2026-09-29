#include "ValidatorNotices.h"

namespace luthier
{

juce::String ValidatorNotices::pickupsOffMessage()
{
    return "All pickups are off, so Luthier is silent. Choose a pickup with the selector.";
}

juce::String ValidatorNotices::tensionCorrectedMessage()
{
    return "A string tension was outside a playable range and has been corrected.";
}

std::vector<Notification> ValidatorNotices::poll (const Validator& validator, juce::uint32 nowMs)
{
    std::vector<Notification> out;

    for (auto& w : watches)
    {
        const int count = validator.getFailureCount (w.check);

        // Whatever the counters held when the editor opened is history, not news.
        if (! primed)
        {
            w.seen = count;
            continue;
        }

        if (count > w.seen)
        {
            w.hits += count - w.seen;
            w.seen = count;
            w.lastIncreaseMs = nowMs;
        }
        else if (nowMs - w.lastIncreaseMs > quietMs)
        {
            w.hits = 0;       // the episode is over; a new one may be announced
            w.posted = false;
        }

        if (w.hits >= w.hitsNeeded && ! w.posted)
        {
            w.posted = true;

            Notification n;
            n.level = Notification::Level::warning;

            if (w.check == ValidationCheck::PickupOutput)
            {
                n.id = pickupsOffId;
                n.message = pickupsOffMessage();
            }
            else
            {
                n.id = tensionCorrectedId;
                n.message = tensionCorrectedMessage();
                n.level = Notification::Level::info;
            }

            out.push_back (std::move (n));
        }
    }

    primed = true;
    return out;
}

} // namespace luthier
