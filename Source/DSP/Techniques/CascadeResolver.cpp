#include "CascadeResolver.h"

namespace luthier
{

const char* getCascadeTechniqueName (CascadeTechnique t) noexcept
{
    switch (t)
    {
        case CascadeTechnique::scrape: return "Scrape";
        case CascadeTechnique::slide:  return "Slide";
        case CascadeTechnique::slap:   return "Slap";
        case CascadeTechnique::mute:   return "Mute";
        case CascadeTechnique::tap:    return "Tap";
        case CascadeTechnique::bend:   return "Bend";
        case CascadeTechnique::numTechniques:
        default:                       return "";
    }
}

const char* getCascadeRelationName (CascadeRelation r) noexcept
{
    switch (r)
    {
        case CascadeRelation::compatible: return "compatible";
        case CascadeRelation::queue:      return "queue";
        case CascadeRelation::alternate:  return "alternate";
        case CascadeRelation::conflict:   return "conflict";
        case CascadeRelation::same:       return "same technique";
        default:                          return "";
    }
}

CascadeRelation CascadeResolver::relation (CascadeTechnique active, CascadeTechnique incoming) noexcept
{
    using R = CascadeRelation;
    constexpr R C = R::compatible, Q = R::queue, A = R::alternate, X = R::conflict, S = R::same;

    // technique-cascade.md 2, rows active, columns incoming:
    //                 Scrape Slide Slap Mute Tap Bend
    static const R table[kNumTechniques][kNumTechniques] =
    {
        /* Scrape */ {  Q,     X,    X,   C,   X,  C },
        /* Slide  */ {  X,     S,    X,   C,   X,  C },
        /* Slap   */ {  X,     X,    Q,   C,   A,  C },
        /* Mute   */ {  C,     C,    C,   S,   C,  C },
        /* Tap    */ {  X,     X,    A,   C,   Q,  C },
        /* Bend   */ {  C,     C,    C,   C,   C,  S },
    };

    const int a = juce::jlimit (0, kNumTechniques - 1, (int) active);
    const int b = juce::jlimit (0, kNumTechniques - 1, (int) incoming);
    return table[a][b];
}

void CascadeResolver::reset() noexcept
{
    for (auto& row : slots)
        for (auto& slot : row)
            slot = Slot {};

    for (auto& p : published)
        p.store (0);
}

bool CascadeResolver::isLive (const Slot& slot, juce::int64 sample) const noexcept
{
    return slot.held || (double) (sample - slot.since) < kWindowSeconds * sr;
}

int CascadeResolver::activeMask (int s, juce::int64 sample) const noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return 0;

    int mask = 0;

    for (int t = 0; t < kNumTechniques; ++t)
        if (isLive (slots[(size_t) s][(size_t) t], sample))
            mask |= 1 << t;

    return mask;
}

CascadeResolver::Outcome CascadeResolver::request (CascadeTechnique incoming, int s, juce::int64 sample,
                                                   bool userTriggered) noexcept
{
    Outcome o;

    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return o;

    auto& row = slots[(size_t) s];

    for (int t = 0; t < kNumTechniques && o.accepted; ++t)
    {
        const auto& slot = row[(size_t) t];

        if (! isLive (slot, sample))
            continue;

        const auto active = (CascadeTechnique) t;

        switch (relation (active, incoming))
        {
            case CascadeRelation::compatible:
            case CascadeRelation::same:
                break;

            case CascadeRelation::queue:
                // Same class: it follows the current gesture. The engines keep
                // their own order (a held tap and a new one are capos in
                // series; a slap's strikes are timed), so it still plays.
                o.queued = true;
                break;

            case CascadeRelation::alternate:
                // Both occur, not overlapping: the latest wins.
                o.preemptMask |= 1 << t;
                break;

            case CascadeRelation::conflict:
                if (active == CascadeTechnique::slide && slot.held && incoming != CascadeTechnique::slide)
                {
                    o.accepted = false;             // 3.4: the bar holds the string
                }
                else if (incoming == CascadeTechnique::slide)
                {
                    o.preemptMask |= 1 << t;        // the bar lands: what was there goes
                }
                else if (slot.user && ! userTriggered)
                {
                    o.accepted = false;             // 3.1: the user's gesture stays
                }
                else
                {
                    o.preemptMask |= 1 << t;        // 3.2: the most recent wins
                }
                break;

            default:
                break;
        }
    }

    if (! o.accepted)
    {
        o.preemptMask = 0;
        return o;
    }

    for (int t = 0; t < kNumTechniques; ++t)
        if ((o.preemptMask & (1 << t)) != 0)
            row[(size_t) t] = Slot {};

    auto& mine = row[(size_t) incoming];
    mine.since = sample;
    mine.user = userTriggered;
    return o;
}

void CascadeResolver::setHeld (CascadeTechnique t, int s, bool held, juce::int64 sample, bool userTriggered) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return;

    auto& slot = slots[(size_t) s][(size_t) juce::jlimit (0, kNumTechniques - 1, (int) t)];

    if (held && ! slot.held)
    {
        slot.since = sample;
        slot.user = userTriggered;
    }
    else if (! held && slot.held)
    {
        slot.since = std::numeric_limits<juce::int64>::min() / 2;   // released: no lingering window
    }

    slot.held = held;
}

void CascadeResolver::publish (int numStrings, juce::int64 sample) noexcept
{
    for (int s = 0; s < juce::jmin (numStrings, kMaxStrings); ++s)
        published[(size_t) s].store (activeMask (s, sample), std::memory_order_relaxed);
}

juce::String CascadeResolver::conflictMessage (CascadeTechnique incoming, CascadeTechnique active,
                                               int activeStrings, int incomingStrings, int numStrings)
{
    const auto r = relation (active, incoming);

    if (r != CascadeRelation::conflict && r != CascadeRelation::alternate)
        return {};

    const int all = (1 << juce::jlimit (1, kMaxStrings, numStrings)) - 1;
    const int a = activeStrings == 0 ? all : (activeStrings & all);
    const int b = incomingStrings == 0 ? all : (incomingStrings & all);
    const int overlap = a & b;

    if (overlap == 0)
        return {};   // 0.4: different strings never conflict

    // Strings as a player numbers them: 1 is the high E (index 0).
    juce::String strings;
    int first = -1, last = -1;
    juce::StringArray runs;

    auto flush = [&]
    {
        if (first >= 0)
            runs.add (first == last ? juce::String (first + 1) : juce::String (first + 1) + "-" + juce::String (last + 1));

        first = last = -1;
    };

    for (int s = 0; s < numStrings; ++s)
    {
        if ((overlap & (1 << s)) != 0)
        {
            if (first < 0) first = s;
            last = s;
        }
        else
        {
            flush();
        }
    }

    flush();
    strings = runs.joinIntoString (", ");

    const auto activeName = juce::String (getCascadeTechniqueName (active));
    const auto incomingName = juce::String (getCascadeTechniqueName (incoming));

    juce::String who;

    if (active == CascadeTechnique::slide)
        who = "Slide will preempt on trigger.";
    else if (incoming == CascadeTechnique::slide)
        who = "Slide will preempt on trigger.";
    else if (r == CascadeRelation::alternate)
        who = "They alternate: the latest wins.";
    else
        who = "The most recent will preempt.";

    return "Conflicts with " + activeName + " on string" + (overlap & (overlap - 1) ? "s " : " ") + strings + ". " + who;
}

} // namespace luthier
