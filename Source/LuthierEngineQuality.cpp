/*  cpu-quality-modes.md: the engine side of the CPU quality level.

    Kept out of LuthierEngine.cpp so the feature's logic lives in one place;
    the hub only calls in at five points (setOversamplingFactor, note on, note
    off, the start and the end of each sub-block).

    Rules this file keeps (section 0):
      - no pitch, timing or latency change: the oversampling pads keep the
        latency, the dispersion cap holds the fundamental, nothing here moves
        an event;
      - identity: coupling is never switched off, the body is never bypassed,
        per-string outputs keep carrying signal (a sleeping string is below
        -100 dBFS, and wakes on the first sample anything reaches it);
      - real-time safe: integers, flags and crossfades only.
*/

#include "LuthierEngine.h"

namespace luthier
{

//==============================================================================
void LuthierEngine::applyOversamplingForQuality (bool crossfade) noexcept
{
    // 2.2: effective = min(nominal, cap); 1x stays 1x; latency is the nominal's.
    // Nominal is the parameter under performance-budget 7's sample-rate cap.
    const int nominal = effectiveOversamplingFactor (oversamplingFactor, sr);
    const int ampFactor = QualityProfile::capFactor (nominal, qualityProfile.ampOversamplingCap);
    const int driveFactor = QualityProfile::capFactor (nominal, qualityProfile.driveOversamplingCap);

    amp.setOversamplingFactor (ampFactor, nominal, crossfade);
    preEffects.setOversamplingFactor (driveFactor, nominal, crossfade);
    postEffects.setOversamplingFactor (driveFactor, nominal, crossfade);
}

void LuthierEngine::applyQuality (const QualityProfile& profile, bool hardSwitch) noexcept
{
    // 2.5: after 50 ms of output below -90 dBFS there is nothing to crossfade.
    if (qualitySilentSamples >= (juce::int64) (QualityProfile::kHardSwitchSilenceSeconds * sr))
        hardSwitch = true;

    if (hardSwitch)
        ++hardQualitySwitches;

    qualityProfile = profile;

    applyOversamplingForQuality (! hardSwitch);

    for (auto& str : strings)
    {
        // Latched per note in excite(): a ringing note keeps its stage count.
        str.setDispersionRule (profile.fourStageFromHz, profile.twoStageFromHz);
        str.setSleepEnabled (profile.idleSleep);
    }

    body.setQualityLevel (profile, hardSwitch);
    cabinet.setQualityLevel (profile, hardSwitch);
    acMic.setEvaluateEvery (profile.micEvaluateEvery);   // mic-placement.md 3 (INTEGRATE-2)
    room.setTapCount (profile.roomTaps, hardSwitch);

    // Events already running finish; only new ones see the smaller pools.
    playingNoise.getPool().setDegraded (profile.noiseDegraded);
}

//==============================================================================
void LuthierEngine::qualityNoteOn (int s) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return;

    qualityNotePeak[(size_t) s] = 0.0;
    qualityRingOutEligible[(size_t) s] = false;
    qualityLastExcite[(size_t) s] = blockStartSample + activeSampleOffset;
}

void LuthierEngine::qualityNoteOff (int s, bool heldOn) noexcept
{
    // 2.4: a released string (and an open string after its note-off) may be
    // truncated; one held by the sustain pedal, sostenuto or an E-Bow may not.
    if (juce::isPositiveAndBelow (s, kMaxStrings))
        qualityRingOutEligible[(size_t) s] = ! heldOn;
}

void LuthierEngine::qualityPerBlock() noexcept
{
    const auto& ebow = ebowDriver.getSettings();
    const bool feedbackDriving = feedbackLoop.isActive();

    const bool ringOut = qualityProfile.ringOutDb < 0.0;
    const double ringOutGain = ringOut ? juce::Decibels::decibelsToGain (qualityProfile.ringOutDb) : 0.0;
    const double fadeSeconds = QualityProfile::kRingOutFadeSeconds;

    int ringingOut = 0, quietest = -1;
    double quietestLevel = 1.0e9;

    for (int s = 0; s < numStrings; ++s)
    {
        auto& str = strings[(size_t) s];

        // Driven strings - an E-Bow on them, or the feedback loop - are never
        // put to sleep or truncated (CQ-30).
        const bool ebowDrives = ebow.enabled
                                && (ebow.stringMask == 0 ? stringMidiNote[(size_t) s] >= 0
                                                         : (ebow.stringMask & (1 << s)) != 0);
        const bool exempt = ebowDrives || feedbackDriving;
        str.setSleepExempt (exempt);

        const double level = str.getLevel();
        qualityNotePeak[(size_t) s] = juce::jmax (qualityNotePeak[(size_t) s], level);

        if (exempt || str.isSleeping() || str.isFadingToSleep() || ! qualityRingOutEligible[(size_t) s])
            continue;

        if (level < QualityProfile::kSleepLevel)
            continue;

        // Past the stated drop below the note's peak: a 30 ms fade to sleep.
        if (ringOut && level < qualityNotePeak[(size_t) s] * ringOutGain)
        {
            str.fadeToSleep (fadeSeconds);
            continue;
        }

        ++ringingOut;

        if (level < quietestLevel)
        {
            quietestLevel = level;
            quietest = s;
        }
    }

    // Low: beyond eight strings ringing out, the quietest fades.
    if (qualityProfile.maxRingingOut > 0 && ringingOut > qualityProfile.maxRingingOut && quietest >= 0)
        strings[(size_t) quietest].fadeToSleep (fadeSeconds);
}

void LuthierEngine::qualityAfterBlock (const juce::AudioBuffer<float>& output) noexcept
{
    const int n = output.getNumSamples();
    float peakLevel = 0.0f;

    for (int ch = 0; ch < output.getNumChannels(); ++ch)
        peakLevel = juce::jmax (peakLevel, output.getMagnitude (ch, 0, n));

    if (juce::Decibels::gainToDecibels (peakLevel, -200.0f) < (float) QualityProfile::kHardSwitchSilenceDb)
        qualitySilentSamples += n;
    else
        qualitySilentSamples = 0;
}

//==============================================================================
bool LuthierEngine::dropLeastRecentString() noexcept
{
    int victim = -1;
    juce::int64 oldest = std::numeric_limits<juce::int64>::max();

    for (int s = 0; s < numStrings; ++s)
    {
        const auto& str = strings[(size_t) s];

        if (str.isSleeping() || str.isFadingToSleep() || str.getLevel() < QualityProfile::kSleepLevel)
            continue;

        if (qualityLastExcite[(size_t) s] < oldest)
        {
            oldest = qualityLastExcite[(size_t) s];
            victim = s;
        }
    }

    if (victim < 0)
        return false;

    // 7: a 10 ms fade, not a cut.
    strings[(size_t) victim].fadeToSleep (QualityProfile::kEmergencyFadeSeconds, true);
    return strings[(size_t) victim].isFadingToSleep();
}

int LuthierEngine::getSleepingStringCount() const noexcept
{
    int n = 0;

    for (int s = 0; s < numStrings; ++s)
        n += strings[(size_t) s].isSleeping() ? 1 : 0;

    return n;
}

} // namespace luthier
