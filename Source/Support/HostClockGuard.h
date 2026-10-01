#pragma once

/*  RT-SAFETY / host-input resilience (docs/review/CODEX_RTSAFETY.md P2): the
    host's clock is validated once, at the processor boundary, before anything
    divides by it or turns it into a sample index.

    A non-finite or non-positive BPM, and a non-finite PPQ, bar start or time,
    are rejected; the last valid value is kept in their place (or, before any
    valid value has arrived, the field is left out, which every reader already
    handles as "the host does not say"). Audio thread; no allocation. */

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

namespace luthier
{

class HostClockGuard
{
public:
    using PositionInfo = juce::AudioPlayHead::PositionInfo;

    /** A sample rate fit to prepare with: finite and positive, else @p fallback. */
    static double validSampleRate (double candidate, double fallback) noexcept
    {
        if (std::isfinite (candidate) && candidate > 0.0)
            return candidate;

        return std::isfinite (fallback) && fallback > 0.0 ? fallback : 44100.0;
    }

    void reset() noexcept { *this = HostClockGuard(); }

    /** The position with every invalid clock field replaced. */
    PositionInfo sanitise (PositionInfo p) noexcept
    {
        if (auto bpm = p.getBpm())
        {
            if (std::isfinite (*bpm) && *bpm > 0.0)
                lastBpm = *bpm, haveBpm = true;
            else
                reject(), p.setBpm (haveBpm ? juce::makeOptional (lastBpm) : juce::Optional<double>());
        }

        keepFinite (p.getPpqPosition(), lastPpq, havePpq,
                    [&p] (juce::Optional<double> v) { p.setPpqPosition (v); });
        keepFinite (p.getPpqPositionOfLastBarStart(), lastBarStart, haveBarStart,
                    [&p] (juce::Optional<double> v) { p.setPpqPositionOfLastBarStart (v); });
        keepFinite (p.getTimeInSeconds(), lastSeconds, haveSeconds,
                    [&p] (juce::Optional<double> v) { p.setTimeInSeconds (v); });

        return p;
    }

    /** The sanitised position of @p playHead this block, if it has one. */
    juce::Optional<PositionInfo> read (juce::AudioPlayHead* playHead) noexcept
    {
        if (playHead == nullptr)
            return {};

        if (auto position = playHead->getPosition())
            return sanitise (*position);

        return {};
    }

    /** How many invalid fields have been replaced (diagnostics and tests). */
    int getRejectedCount() const noexcept { return rejected; }

private:
    double lastBpm = 120.0, lastPpq = 0.0, lastBarStart = 0.0, lastSeconds = 0.0;
    bool haveBpm = false, havePpq = false, haveBarStart = false, haveSeconds = false;
    int rejected = 0;

    void reject() noexcept { ++rejected; }

    template <typename Setter>
    void keepFinite (juce::Optional<double> value, double& last, bool& have, Setter&& set) noexcept
    {
        if (! value)
            return;

        if (std::isfinite (*value))
        {
            last = *value;
            have = true;
            return;
        }

        reject();
        set (have ? juce::makeOptional (last) : juce::Optional<double>());
    }
};

} // namespace luthier
