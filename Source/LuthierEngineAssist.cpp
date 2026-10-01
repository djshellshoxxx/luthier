/*  LuthierEngine's Performance Assist glue (auto-articulation.md 4.2).

    What the engine owes Performance Assist: the explicit context before the
    interpreter runs (5), the decisions' visible and captured traces when a
    note starts (7.3, 9), the scheduled mute lift (3.6), and the auto pitch
    and vibrato curves each block (3.4, 3.8). Kept in its own file so the hub
    file carries only one marked call per insertion point.

    Every path here is inert with Assist off: a note with autoRules == 0 never
    reaches assistNoteStarted, no lift is ever scheduled, and the curves are 0.
*/

#include "LuthierEngine.h"
#include "Capture/PerformanceCapture.h"

namespace luthier
{

AssistBypass LuthierEngine::getAssistBypass() const noexcept
{
    const auto b = midi.getAssistBypass();

    if (b == AssistBypass::none && rhythm.isEnabled() && rhythm.isDriving())
        return AssistBypass::rhythmDriving;

    return b;
}

void LuthierEngine::assistReset() noexcept
{
    assistNoteSerial.fill (0);
    assistLiftStart.fill (-1);
    assistLiftFrom.fill (0.0);
    assistLastPitch.fill (0.0);
}

//==============================================================================
void LuthierEngine::assistSetContext (bool rhythmPass) noexcept
{
    AssistExplicitContext c;
    c.preArticulated = assistPreArticulated;
    c.rhythmDriving = rhythmPass;

    const auto& slapSettings = slap.getSettings();
    c.slapHeld = slap.isModifierHeld();
    c.slapVelocityZone = slapSettings.armed && slapSettings.trigger == TriggerSource::velocityZone;
    c.slapZoneVelocity = slapSettings.velocityZone;

    for (int s = 0; s < numStrings; ++s)
        if (scrape.isStringActive (s))
            c.scrapeMask |= 1u << s;

    // two-hand-tapping.md 5 and muting-rhythm.md belong to the TECHNIQUES
    // workstream; its layer fills these two when it lands.
    c.tapArmed = false;
    c.muteGridActive = false;

    c.bassFamily = spec.category == GuitarCategory::Bass;
    c.bassFingers = usingFingers || ! PlayingNoise::getPickMaterial (pickMaterial).isPick;
    c.fretless = fretless;

    AssistTransport t;
    t.playing = hostPlaying;
    t.ppqAtBlockStart = hostPpq;
    t.bpm = tempoBpm;

    midi.setAssistContext (c, t, samplePosition);
}

//==============================================================================
void LuthierEngine::assistNoteStarted (const NoteOnEvent& e, int s, double fret) noexcept
{
    auto& aa = midi.getAutoArticulator();
    const juce::int64 now = blockStartSample + activeSampleOffset;
    const auto rules = e.autoRules;
    const int offset = captureOffset();

    auto feed = [&] (AssistLabel label, int bit, int stringIndex = -2, juce::uint16 mask = 0)
    {
        aa.pushFeed (now, stringIndex == -2 ? s : stringIndex, fret, label, bit, mask);
    };

    // 3.2 / 3.3
    if ((rules & AssistRule::legato) != 0)
    {
        if (e.technique == Technique::HammerOn)      feed (AssistLabel::hammerOn, 1);
        else if (e.technique == Technique::PullOff)  feed (AssistLabel::pullOff, 1);
        else if (e.technique == Technique::Slide)
            feed (fret >= e.slideFromFret ? AssistLabel::slideUp : AssistLabel::slideDown, 1);
    }

    if ((rules & AssistRule::slide) != 0 && e.technique == Technique::Slide)
        feed (fret >= e.slideFromFret ? AssistLabel::slideUp : AssistLabel::slideDown, 2);

    // 3.6
    if ((rules & AssistRule::palmMute) != 0 && e.technique == Technique::PalmMute)
    {
        feed (AssistLabel::palmMute, 8);

        if (perfCapture != nullptr)
            perfCapture->mark (offset, s, ScoreTechnique::Type::palmMute, e.palmMuteAmount);

        if (e.muteLiftSamples > 0 && numScheduled < kMaxScheduledEvents)
        {
            ScheduledEvent lift;
            lift.isNoteOn = false;
            lift.kind = kDampingLift;
            lift.noteOn = e;
            lift.noteOn.stringIndex = s;
            lift.serial = assistNoteSerial[(size_t) s];
            lift.absoluteSample = now + e.muteLiftSamples;
            scheduled[(size_t) numScheduled++] = lift;
        }
    }

    // 3.5
    if (e.autoAccent)
    {
        feed (AssistLabel::accent, 4);

        if (perfCapture != nullptr)
            perfCapture->mark (offset, s, ScoreTechnique::Type::accent);
    }

    // 3.7
    if ((rules & (AssistRule::alternate | AssistRule::strum)) != 0)
    {
        const int bit = (rules & AssistRule::strum) != 0 ? 6 : 5;

        if (e.upStroke && bit == 5)
            feed (AssistLabel::upStroke, bit);

        if (perfCapture != nullptr)
            perfCapture->mark (offset, s, e.upStroke ? ScoreTechnique::Type::pickStrokeUp
                                                     : ScoreTechnique::Type::pickStrokeDown);
    }

    if (e.autoStrumMask != 0)
        feed (e.upStroke ? AssistLabel::strumUp : AssistLabel::strumDown, 6, -1, e.autoStrumMask);

    // 3.8
    if (e.autoOrnament == 1 || e.autoOrnament == 2)
        feed (e.autoOrnament == 2 ? AssistLabel::bendWhole : AssistLabel::bendHalf, 7);
    else if (e.autoOrnament == 3)
    {
        feed (AssistLabel::slideIn, 7);

        if (perfCapture != nullptr)
            perfCapture->mark (offset, s, ScoreTechnique::Type::slideIn, e.slideFromFret);
    }

    if (perfCapture != nullptr)
        perfCapture->autoRules (offset, s, rules);
}

void LuthierEngine::assistFireLift (const ScheduledEvent& e) noexcept
{
    const int s = juce::jlimit (0, numStrings - 1, e.noteOn.stringIndex);

    // Only the note that was muted, and only while it still sounds (3.6).
    if (e.serial != assistNoteSerial[(size_t) s] || stringMidiNote[(size_t) s] != e.noteOn.midiNote)
        return;

    assistLiftStart[(size_t) s] = e.absoluteSample;
    assistLiftFrom[(size_t) s] = juce::jmax (0.0, e.noteOn.palmMuteAmount);
    midi.getAutoArticulator().pushFeed (e.absoluteSample, s, currentFret[(size_t) s], AssistLabel::muteLift, 8);
}

//==============================================================================
double LuthierEngine::assistPerBlockCents (int s, int numSamples, double& vib) noexcept
{
    auto& aa = midi.getAutoArticulator();
    const juce::int64 t = blockStartSample + numSamples;

    // 3.6: the palm lifts off over 60 ms.
    if (assistLiftStart[(size_t) s] >= 0)
    {
        const double x = (double) (t - assistLiftStart[(size_t) s]) / (0.060 * sr);
        auto& str = strings[(size_t) s];

        if (x >= 1.0 || stringMidiNote[(size_t) s] < 0)
        {
            if (stringMidiNote[(size_t) s] >= 0)
                str.setDamping (StringEngine::Damping::Open, 1.0);

            assistLiftStart[(size_t) s] = -1;
        }
        else if (x > 0.0)
        {
            str.setDamping (spec.category == GuitarCategory::Bass ? StringEngine::Damping::PalmMuteBass
                                                                   : StringEngine::Damping::PalmMute,
                            assistLiftFrom[(size_t) s] * (1.0 - x));
        }
    }

    const double pitchCents = aa.autoPitchCents (s, t);
    const double autoVib = aa.autoVibratoCents (s, t);

    // 3.4: the per-string target is the larger of the controller's and Assist's.
    if (autoVib != 0.0 && aa.autoVibratoDepth (s, t) > vibratoAmount[(size_t) s])
        vib = autoVib;

    if (pitchCents != 0.0 && ! slide.isUnderBar (s))
        vib += pitchCents;

    // The one-shot traces, for the feed and the capture.
    if (const int events = aa.takePendingEvents (s))
    {
        const int offset = (int) juce::jmax ((juce::int64) 0, t - hostBlockStart);

        if ((events & AutoArticulator::vibratoStarted) != 0)
        {
            aa.pushFeed (aa.getVibratoStartSample (s), s, currentFret[(size_t) s], AssistLabel::vibrato, 3);

            if (perfCapture != nullptr)
                perfCapture->mark (offset, s, ScoreTechnique::Type::vibrato,
                                   aa.getVibratoRate (s), aa.getVibratoDepthCents (s));
        }

        if ((events & AutoArticulator::fallStarted) != 0)
        {
            aa.pushFeed (t, s, currentFret[(size_t) s], AssistLabel::fall, 7);

            if (perfCapture != nullptr)
                perfCapture->mark (offset, s, ScoreTechnique::Type::slideOut);
        }
    }

    if (perfCapture != nullptr && pitchCents != assistLastPitch[(size_t) s])
        perfCapture->bend ((int) juce::jmax ((juce::int64) 0, t - hostBlockStart), s, pitchCents);

    assistLastPitch[(size_t) s] = pitchCents;
    return pitchCents;
}

} // namespace luthier
