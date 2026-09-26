#pragma once

#include "ModSources.h"

namespace luthier
{

/**
    SPEC-SWEEP (ui-wiring UW-5): one edit of a modulation source's settings, as
    plain data.

    The MOD tab's source card used to call the LFO / envelope / sequencer /
    follower setters from the message thread while the audio thread ticked the
    same objects. It now fills one of these and posts it to the ModMatrix, which
    applies it at the top of its next processBlock (or at once when no audio
    block has run for a while, so a stopped host still sees the edit).
*/
struct ModSourceEdit
{
    enum class Kind : int { none = 0, lfo, envelope, sequencer, follower };

    Kind kind = Kind::none;
    int index = 0;   ///< which LFO / envelope / sequencer / follower

    // LFO
    int lfoShape = 0, lfoDivision = 0, lfoRetrigger = 0;
    double lfoRateHz = 1.0, lfoDepth = 1.0, lfoSymmetry = 0.5, lfoSmoothingMs = 0.0;
    bool lfoSynced = false, lfoBipolar = true;

    // Envelope
    double envDelay = 0.0, envAttack = 0.01, envHold = 0.0, envDecay = 0.2, envSustain = 0.7, envRelease = 0.3;

    // Sequencer
    int seqLength = 16, seqDirection = 0, seqDivision = 0;
    double seqSwing = 0.0;
    bool seqSynced = true;

    // Follower
    int followerSource = 0, followerDetection = 0;
    double followerAttackMs = 5.0, followerReleaseMs = 100.0, followerThreshold = 0.0;
};

} // namespace luthier
