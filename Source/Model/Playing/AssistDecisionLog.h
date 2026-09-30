#pragma once

/*  The message thread's side of Performance Assist's feed (auto-articulation.md
    7.3, 7.5, 8).

    One reader drains the audio thread's SPSC feed for everyone who shows it:
    the fretboard labels (both fretboards), the AUTO pill's flash and the
    PLAYING group's decision list. Each of them calls drainIfDue from its own
    30 Hz timer; the log drains at most once per UI frame, so however many are
    open the feed still has exactly one reader. Session state: never saved.

    Staleness (gui-engine-dataflow): an entry more than 600 ms older than the
    newest one delivered in the same drain is dropped unread - it describes a
    note the player has long since moved on from, typically one decided while
    no editor was open.
*/

#include "AutoArticulationFeed.h"

#include <array>
#include <functional>

namespace luthier
{

class AssistDecisionLog
{
public:
    static constexpr int kListSize = 16;        ///< 7.2: the last 16, newest first
    static constexpr int kLiveSize = 64;
    static constexpr double kLabelMs = 600.0;   ///< 7.3: a label's life

    struct Decision
    {
        juce::int64 sample = 0;
        int string = -1;
        float fret = 0.0f;
        AssistLabel label = AssistLabel::none;
        juce::uint16 stringMask = 0;
        double drainedAtMs = 0.0;               ///< the UI clock when it arrived
    };

    /** Drains `feed` unless it was drained within the last UI frame (or `force`).
        Returns the number of decisions taken in. */
    int drainIfDue (AutoArticulationFeed& feed, double sampleRate, bool force = false)
    {
        const double now = nowMs();

        if (! force && now - lastDrainMs < 30.0)
            return 0;

        lastDrainMs = now;
        rate = sampleRate > 0.0 ? sampleRate : 48000.0;

        // Two passes over one drain would need the entries twice; the newest
        // sample is known only once all are read, so they are staged first.
        std::array<AutoArticulationFeedEntry, AutoArticulationFeed::kCapacity> staged {};
        int n = 0;
        feed.drain ([&] (const AutoArticulationFeedEntry& e) { if (n < (int) staged.size()) staged[(size_t) n++] = e; });

        juce::int64 newest = 0;

        for (int i = 0; i < n; ++i)
            newest = juce::jmax (newest, staged[(size_t) i].sample);

        const auto stale = (juce::int64) (kLabelMs * 0.001 * rate);
        int taken = 0;

        for (int i = 0; i < n; ++i)
        {
            const auto& e = staged[(size_t) i];

            if (e.sample < newest - stale)
                continue;

            Decision d;
            d.sample = e.sample;
            d.string = e.string;
            d.fret = e.fret;
            d.label = (AssistLabel) e.label;
            d.stringMask = e.stringMask;
            d.drainedAtMs = now;

            live[(size_t) (liveHead++ % kLiveSize)] = d;
            list[(size_t) (listHead++ % kListSize)] = d;
            ++count;
            ++taken;
        }

        if (taken > 0)
            lastDecisionMs = now;

        return taken;
    }

    /** Decisions so far this session. */
    juce::uint32 getDecisionCount() const noexcept { return count; }
    double getLastDecisionMs() const noexcept { return lastDecisionMs; }
    double getSampleRate() const noexcept { return rate; }

    /** 7.2's list: `index` 0 is the newest. */
    int getNumListed() const noexcept { return (int) juce::jmin ((juce::uint32) kListSize, count); }
    const Decision& getListed (int index) const noexcept
    {
        const auto i = (listHead - 1 - (juce::uint32) juce::jlimit (0, kListSize - 1, index)) % (juce::uint32) kListSize;
        return list[(size_t) i];
    }

    /** The labels still inside their 600 ms at `atMs`, oldest first. */
    template <typename Fn>
    void forEachLive (double atMs, Fn&& fn) const
    {
        const auto n = juce::jmin ((juce::uint32) kLiveSize, liveHead);

        for (juce::uint32 k = n; k > 0; --k)
        {
            const auto& d = live[(size_t) ((liveHead - k) % kLiveSize)];
            const double age = atMs - d.drainedAtMs;

            if (age >= 0.0 && age < kLabelMs)
                fn (d, age);
        }
    }

    /** "12.4 seconds, string 3 fret 7, hammer-on" (8), or the short form the
        list draws: "12.4s  str 3 fr 7  Hammer-on". */
    juce::String describe (const Decision& d, bool spoken) const
    {
        const auto seconds = juce::String ((double) d.sample / rate, 1);
        const juce::String name (getAssistLabelName (d.label));

        if (d.string < 0)
            return spoken ? seconds + " seconds, " + name
                          : seconds + "s  " + name.substring (0, 1).toUpperCase() + name.substring (1);

        const int stringNumber = d.string + 1;
        const int fret = juce::roundToInt (d.fret);

        return spoken ? seconds + " seconds, string " + juce::String (stringNumber) + " fret " + juce::String (fret) + ", " + name
                      : seconds + "s  str " + juce::String (stringNumber) + " fr " + juce::String (fret) + "  "
                          + name.substring (0, 1).toUpperCase() + name.substring (1);
    }

    void clear() noexcept
    {
        liveHead = listHead = count = 0;
    }

    /** Tests: a clock to use instead of the real one. */
    void setClockForTests (std::function<double()> clock) { testClock = std::move (clock); }

    double nowMs() const { return testClock ? testClock() : juce::Time::getMillisecondCounterHiRes(); }

private:
    std::array<Decision, kLiveSize> live {};
    std::array<Decision, kListSize> list {};
    juce::uint32 liveHead = 0, listHead = 0, count = 0;
    double lastDrainMs = -1.0e9, lastDecisionMs = -1.0e9, rate = 48000.0;
    std::function<double()> testClock;
};

} // namespace luthier
