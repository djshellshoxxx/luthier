#pragma once

/*  CascadeResolver (technique-cascade.md 2-4, engine-technique-layer.md 1).

    Who gets a string when two techniques want it in the same ~200 ms. The
    compatibility matrix (2) is a table here; the priority rules (3) decide a
    conflict: slide holds its strings once engaged (3.4), a user gesture
    beats an automatic one (3.1), otherwise the most recent wins (3.2), and
    mute and bend are always added (3.5, 3.6). Different strings never
    interact (0.4).

    The engine tells it what is happening on each string (activity) and asks
    it before starting a gesture (request). What is preempted is released
    gracefully by the owning engine - the scrape and slap already fade over
    10 ms (3.3). It also publishes, per string, which techniques are live,
    for the CASCADE view (gui-techniques-updates.md 1, 10).

    O(strings x techniques) per request; audio thread; nothing allocates.
*/

#include "../Common/DspCommon.h"
#include <array>
#include <atomic>

namespace luthier
{

/** The six techniques, in the matrix's order. */
enum class CascadeTechnique { scrape = 0, slide, slap, mute, tap, bend, numTechniques };

/** technique-cascade.md 2's legend. */
enum class CascadeRelation { compatible = 0, queue, alternate, conflict, same };

const char* getCascadeTechniqueName (CascadeTechnique t) noexcept;
const char* getCascadeRelationName (CascadeRelation r) noexcept;

class CascadeResolver
{
public:
    static constexpr int kNumTechniques = (int) CascadeTechnique::numTechniques;

    /** 2: the relation when `active` is on a string and `incoming` arrives. */
    static CascadeRelation relation (CascadeTechnique active, CascadeTechnique incoming) noexcept;

    /** 2's window. */
    static constexpr double kWindowSeconds = 0.200;

    /** 3.3's crossfade for a preempted gesture. */
    static constexpr double kPreemptFadeSeconds = 0.010;

    void prepare (double sampleRate) noexcept { sr = juce::jmax (1.0, sampleRate); reset(); }
    void reset() noexcept;

    //==========================================================================
    struct Outcome
    {
        bool accepted = true;           ///< the incoming technique plays now
        bool queued = false;            ///< same technique: it follows the current gesture
        int preemptMask = 0;            ///< bit t: technique t is released on this string
    };

    /*  The engine wants to start `incoming` on string `s` at `sample`.
        `userTriggered` is false for automatic gestures (an auto pull-off,
        a scripted slide). Records the incoming technique if accepted. */
    Outcome request (CascadeTechnique incoming, int s, juce::int64 sample, bool userTriggered) noexcept;

    /*  Continuous state: a technique is (or stops being) held on a string -
        the slide bar, a held tap, a running scrape. Events (a slap, a mute
        strike, a bend) are recorded by request() and last the window. */
    void setHeld (CascadeTechnique t, int s, bool held, juce::int64 sample, bool userTriggered = true) noexcept;

    /** Techniques live on string `s` at `sample` (bit per CascadeTechnique). */
    int activeMask (int s, juce::int64 sample) const noexcept;

    /** For the CASCADE view: the last mask published per string. Any thread. */
    int getPublishedMask (int s) const noexcept { return published[(size_t) juce::jlimit (0, kMaxStrings - 1, s)].load (std::memory_order_relaxed); }

    /** Once a block: publishes the masks at `sample`. */
    void publish (int numStrings, juce::int64 sample) noexcept;

    //==========================================================================
    /*  6: arming `incoming` while `active` is armed on overlapping strings -
        the pill's red slash and its tooltip. `activeStrings` / `incomingStrings`
        are string masks (0 = all). Empty when there is no conflict. */
    static juce::String conflictMessage (CascadeTechnique incoming, CascadeTechnique active,
                                         int activeStrings, int incomingStrings, int numStrings);

private:
    struct Slot
    {
        bool held = false;
        bool user = true;
        juce::int64 since = std::numeric_limits<juce::int64>::min() / 2;
    };

    bool isLive (const Slot& slot, juce::int64 sample) const noexcept;

    double sr = 48000.0;
    std::array<std::array<Slot, (size_t) kNumTechniques>, kMaxStrings> slots {};
    std::array<std::atomic<int>, kMaxStrings> published {};
};

} // namespace luthier
