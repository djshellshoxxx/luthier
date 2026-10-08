#pragma once

/*  string-interaction.md 6 (REALISM-B): a strum crosses every string between
    its first and last, including the ones the fretting hand mutes. Those are
    struck too: a short, pitchless thump.

    The thump is timed from the strikes the gesture already planned - each
    muted string's crossing is interpolated between the struck strings either
    side of it - so the sounding strings keep exactly the timing they had and
    a thump level of 0 changes nothing at all.
*/

#include "StrumGesture.h"
#include "../Model/Playing/PlayingEvents.h"

namespace luthier
{

struct MutedThump
{
    int stringIndex = 0;
    double timeSeconds = 0.0;
    double force = 1.0;
};

/*  `candidates` is a mask of strings the voicing mutes (and, for a pattern,
    that its STRUM mask includes). A live chord's strum crosses only the
    strings between its first and last struck one (`withinSpanOnly`); a
    pattern's crosses every string in its STRUM mask, so a muted string
    outside the struck span is reached one string-interval before or after
    it. Returns how many thumps were written. */
inline int planMutedThumps (const StrumStrike* strikes, int planned, juce::uint32 candidates,
                            MutedThump* out, int maxOut, bool withinSpanOnly = true) noexcept
{
    int lo = kMaxStrings, hi = -1;

    for (int k = 0; k < planned; ++k)
    {
        if (strikes[k].missed)
            continue;

        lo = juce::jmin (lo, strikes[k].stringIndex);
        hi = juce::jmax (hi, strikes[k].stringIndex);
    }

    int written = 0;

    if (hi < 0)
        return 0;

    for (int m = 0; m < kMaxStrings && written < maxOut; ++m)
    {
        if (m <= lo || m >= hi)
        {
            if (withinSpanOnly || m == lo || m == hi
                || (candidates & ((juce::uint32) 1u << (juce::uint32) m)) == 0)
                continue;

            // Outside the struck span: extrapolate from the end the hand
            // reaches it from, at that end's string-to-string interval.
            const int end = m < lo ? lo : hi;
            const StrumStrike* atEnd = nullptr;
            const StrumStrike* inner = nullptr;

            for (int k = 0; k < planned; ++k)
            {
                const auto& st = strikes[k];

                if (st.missed)
                    continue;

                if (st.stringIndex == end)
                    atEnd = &st;
                else if (inner == nullptr || std::abs (st.stringIndex - end) < std::abs (inner->stringIndex - end))
                    inner = &st;
            }

            if (atEnd == nullptr)
                continue;

            const double perString = inner != nullptr
                                       ? (atEnd->timeSeconds - inner->timeSeconds) / (double) (atEnd->stringIndex - inner->stringIndex)
                                       : 0.005;

            MutedThump t;
            t.stringIndex = m;
            t.timeSeconds = juce::jmax (0.0, atEnd->timeSeconds + (double) (m - end) * perString);
            t.force = atEnd->force;
            out[written++] = t;
            continue;
        }

        if ((candidates & ((juce::uint32) 1u << (juce::uint32) m)) == 0)
            continue;

        // The struck strings either side of it, by string index.
        const StrumStrike* below = nullptr;
        const StrumStrike* above = nullptr;

        for (int k = 0; k < planned; ++k)
        {
            const auto& st = strikes[k];

            if (st.missed)
                continue;

            if (st.stringIndex < m && (below == nullptr || st.stringIndex > below->stringIndex))
                below = &st;

            if (st.stringIndex > m && (above == nullptr || st.stringIndex < above->stringIndex))
                above = &st;
        }

        if (below == nullptr || above == nullptr)
            continue;

        const double f = (double) (m - below->stringIndex) / (double) (above->stringIndex - below->stringIndex);

        MutedThump t;
        t.stringIndex = m;
        t.timeSeconds = below->timeSeconds + f * (above->timeSeconds - below->timeSeconds);
        t.force = below->force + f * (above->force - below->force);
        out[written++] = t;
    }

    return written;
}

/** 6: the thump's note-on. `velocity` is the strike's; `pitchHz` the fretting
    hand's position on that string. */
inline NoteOnEvent makeThumpEvent (int stringIndex, double strikeVelocity, double level, double pitchHz,
                                   int sampleOffset, int strikerMaterial) noexcept
{
    NoteOnEvent e;
    e.stringIndex = stringIndex;
    e.midiNote = 0;
    e.velocity = juce::jlimit (0.02, 1.0, strikeVelocity * juce::jlimit (0.0, 1.0, level) * 0.3);
    e.technique = Technique::Pluck;
    e.chuck = 0.92;
    e.deadStrike = true;
    e.pitchHz = pitchHz;
    e.sampleOffset = juce::jmax (0, sampleOffset);
    e.strikerMaterial = strikerMaterial;
    return e;
}

} // namespace luthier
