#include "ChordNameFader.h"

namespace luthier
{

void ChordNameFader::clear() noexcept
{
    text.clear();
    outgoing.clear();
    chordStartMs = -1.0e9;
    releaseMs = -1.0;
    outgoingFromMs = -1.0e9;
    outgoingFrom = 0.0f;
}

void ChordNameFader::update (double nowMs, bool sounding, bool newOnset, const juce::String& name)
{
    if (! sounding)
    {
        // Release: the hold ends now (or at 1.2 s, whichever is first).
        if (releaseMs < 0.0 && text.isNotEmpty())
            releaseMs = nowMs;

        return;
    }

    const bool shown = getOpacity (nowMs) > 0.0f;

    if (newOnset)
    {
        // A strum's later strings, within 30 ms of its first: the same chord.
        if (shown && releaseMs < 0.0 && nowMs - chordStartMs <= kBurstMs)
        {
            text = name;
            return;
        }

        // A new chord or note: the old name (if showing) crossfades out.
        if (shown && name != text)
        {
            outgoing = text;
            outgoingFrom = getOpacity (nowMs);
            outgoingFromMs = nowMs;
        }

        // The same name struck again restarts its hold without a flash.
        const bool sameAndShown = shown && name == text;
        text = name;
        releaseMs = -1.0;
        chordStartMs = sameAndShown ? nowMs - kFadeInMs : nowMs;
        return;
    }

    // Legato, bend or slide: no new onset, the text changes in place.
    if (text.isNotEmpty() && releaseMs < 0.0)
        text = name;
}

float ChordNameFader::envelope (double nowMs) const noexcept
{
    const double t = nowMs - chordStartMs;

    if (text.isEmpty() || t < 0.0)
        return 0.0f;

    // The hold runs while notes sound, up to 1.2 s after the fade in.
    const double holdEnd = releaseMs >= 0.0 ? juce::jmin (releaseMs - chordStartMs, kFadeInMs + kHoldMs)
                                            : kFadeInMs + kHoldMs;

    if (reducedMotion)
        return t <= juce::jmax (0.0, holdEnd) ? (float) kPeak : 0.0f;

    if (t < kFadeInMs && t < holdEnd)
        return (float) (kPeak * t / kFadeInMs);

    if (t <= holdEnd)
        return (float) kPeak;

    // Fading from wherever it was when the hold ended.
    const double at = holdEnd < kFadeInMs ? kPeak * juce::jmax (0.0, holdEnd) / kFadeInMs : kPeak;
    const double out = (t - holdEnd) / kFadeOutMs;
    return out >= 1.0 ? 0.0f : (float) (at * (1.0 - out));
}

float ChordNameFader::getOpacity (double nowMs) const noexcept
{
    return envelope (nowMs);
}

juce::String ChordNameFader::getOutgoingText (double nowMs) const
{
    return getOutgoingOpacity (nowMs) > 0.0f ? outgoing : juce::String();
}

float ChordNameFader::getOutgoingOpacity (double nowMs) const noexcept
{
    if (outgoing.isEmpty() || reducedMotion)
        return 0.0f;

    const double t = (nowMs - outgoingFromMs) / kCrossfadeMs;
    return t >= 1.0 || t < 0.0 ? 0.0f : outgoingFrom * (float) (1.0 - t);
}

} // namespace luthier
