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
    that its STRUM mask includes). Returns how many thumps were written. */
inline int planMutedThumps (const StrumStrike* strikes, int planned, juce::uint32 candidates,
                            MutedThump* out, int maxOut) noexcept
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

    for (int m = lo + 1; m < hi && written < maxOut; ++m)
    {
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
