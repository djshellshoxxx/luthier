#pragma once

/*  The Workshop's spectrum delta (workshop-ui.md 6, ui-wiring.md 6.4).

    A fixture render - the same pluck on the same string at the same velocity -
    through the committed guitar and through the candidate, and the magnitude
    difference in dB against frequency. Flat means the change did nothing, and
    the pane says so plainly: honest magnitudes.

    Rendered on its own worker thread with its own engine, never the audio
    thread's. At most one render in flight; a newer request replaces an older
    one that has not started (coalesced while dragging).
*/

#include "../Model/Workshop/PartLibrary.h"
#include "../Model/Guitar/GuitarLibrary.h"

#include <juce_core/juce_core.h>
#include <vector>

namespace luthier
{

class SpectrumDelta : private juce::Thread
{
public:
    struct Result
    {
        std::vector<float> frequencies;   ///< Hz, log-spaced, 60 Hz - 12 kHz
        std::vector<float> deltaDb;       ///< candidate minus committed
        float largestDb = 0.0f;           ///< the biggest |delta|
        bool noChange = true;             ///< nothing over 0.5 dB anywhere
        juce::String summary;             ///< section 10's sentence for a screen reader
        std::vector<float> combNotchesHz; ///< for a pickup drag
        double renderMs = 0.0;
        juce::uint32 requestId = 0;
    };

    SpectrumDelta();
    ~SpectrumDelta() override;

    /** Queues a delta; the latest request wins. Message thread. Returns its id. */
    juce::uint32 request (const WorkshopGuitar& committed, const WorkshopGuitar& candidate, GuitarType standsFor,
                          std::vector<float> combNotchesHz = {});

    /** The newest finished result, once. Message thread. */
    bool takeResult (Result& out);

    /** Renders now, on the calling thread (tests, and the budget check). */
    static Result compute (const WorkshopGuitar& committed, const WorkshopGuitar& candidate, GuitarType standsFor);

    /** Where a pickup `positionMm` from the saddle cancels harmonics of a string (comb notches). */
    static std::vector<float> combNotches (double positionMm, double scaleMm, double openHz, double maxHz = 12000.0);

    static constexpr int kNumPoints = 96;

private:
    void run() override;

    struct Job
    {
        WorkshopGuitar committed, candidate;
        GuitarType type = GuitarType::Stratocaster;
        std::vector<float> notches;
        juce::uint32 id = 0;
    };

    juce::CriticalSection lock;
    std::unique_ptr<Job> pending;
    std::unique_ptr<Result> finished;
    juce::uint32 nextId = 1;
    juce::WaitableEvent wake;
};

} // namespace luthier
