#pragma once

/*  spec.md identity 1 and 4 (SP-111, SP-114): the validator's corrections said
    to the player.

    "Impossible tuning rejected with a warning" and "all pickups off -> silence
    and a small warning" were only visible in the Debug window: a player who
    switched every pickup off got silence and no reason. This watches the
    validator's counters (lock-free, safe to read from the message thread) and
    turns a sustained change into one banner per episode.

    An episode is a run of increases with a quiet gap after it, so a condition
    that stays true says so once, not every second, and a one-off blip while a
    guitar loads (the pickup engine is briefly empty) says nothing. */

#include "Notifications.h"
#include "../Validator.h"

#include <vector>

namespace luthier
{

class ValidatorNotices
{
public:
    /** How long without a new failure ends an episode. */
    static constexpr juce::uint32 quietMs = 3000;

    /** The pickup check reports about once a second while everything is off; two
        reports (a second of silence) make it real. */
    static constexpr int pickupHitsNeeded = 2;

    static constexpr const char* pickupsOffId      = "pickups-off";
    static constexpr const char* tensionCorrectedId = "tension-corrected";

    /** Reads the counters; returns the banners to post now (usually none). */
    std::vector<Notification> poll (const Validator& validator, juce::uint32 nowMs);

    static juce::String pickupsOffMessage();
    static juce::String tensionCorrectedMessage();

private:
    struct Watch
    {
        ValidationCheck check;
        int hitsNeeded;
        int seen = 0, hits = 0;
        bool posted = false;
        juce::uint32 lastIncreaseMs = 0;
    };

    Watch watches[2] { { ValidationCheck::PickupOutput, pickupHitsNeeded },
                       { ValidationCheck::StringTension, 1 } };
    bool primed = false;
};

} // namespace luthier
