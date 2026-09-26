#pragma once

/*  cpu-quality-modes.md 2.7 / 4: Luthier's own share of the audio time.

    The audio thread stamps each processBlock: wall time spent in it divided by
    the block's duration. That is Luthier's load only - a plugin cannot see the
    host's total - and the UI says so.

    Everything is O(1) per block and allocation-free:
      - audio time is cut into 10 ms buckets (busy seconds, audio seconds);
        a ring of 2048 buckets covers 20 s, with running sums for the last
        200 ms, 2 s and 20 s;
      - a ring of the last 1024 block loads feeds a 64-bin histogram for the
        2 s p95 (at small block sizes it covers the last 1024 blocks instead,
        which is at least 0.68 s at 32 samples / 48 kHz).
    The means and the p95 are published as atomics for the message thread,
    which runs Auto and the E1/E2 governor from them. E3 reads the 200 ms mean
    on the audio thread directly (7), so it works while the message thread is
    blocked.

    This is the source of gui-engine-dataflow 11's CPU figure;
    LuthierEngine::cpuEstimate stays for the debug window.
*/

#include <array>
#include <atomic>
#include <cmath>

namespace luthier
{

class CpuLoadMonitor
{
public:
    static constexpr double kBucketSeconds = 0.010;
    static constexpr int kNumBuckets = 2048;
    static constexpr int kShortBuckets = 20;     // 200 ms
    static constexpr int kMidBuckets = 200;      // 2 s
    static constexpr int kLongBuckets = 2000;    // 20 s
    static constexpr int kBlockRing = 1024;
    static constexpr int kHistBins = 64;         // 0 .. 128 % in 2 % bins

    CpuLoadMonitor() { reset(); }

    /** Audio thread (or before audio starts). */
    void reset() noexcept
    {
        buckets.fill ({});
        bucketIndex = 0;
        current = {};
        shortSum = midSum = longSum = {};
        filledBuckets = 0;
        blockLoads.fill (0);
        blockIndex = 0;
        blocksFilled = 0;
        hist.fill (0);
        totalBlocks.store (0, std::memory_order_relaxed);
        publish();
    }

    /** Audio thread: one processed block. */
    void addBlock (double busySeconds, double blockSeconds) noexcept
    {
        if (! (blockSeconds > 0.0) || ! std::isfinite (busySeconds))
            return;

        busySeconds = busySeconds < 0.0 ? 0.0 : busySeconds;

        // --- p95 ring ---------------------------------------------------------
        const double load = busySeconds / blockSeconds;
        const int bin = binFor (load);

        if (blocksFilled == kBlockRing)
            --hist[(size_t) blockLoads[(size_t) blockIndex]];
        else
            ++blocksFilled;

        blockLoads[(size_t) blockIndex] = (unsigned char) bin;
        ++hist[(size_t) bin];
        blockIndex = (blockIndex + 1) % kBlockRing;

        // --- buckets ------------------------------------------------------------
        current.busy += busySeconds;
        current.audio += blockSeconds;

        if (current.audio >= kBucketSeconds)
            closeBucket();

        totalBlocks.fetch_add (1, std::memory_order_relaxed);

        // E3 wants the freshest short mean, including the open bucket.
        const double sAudio = shortSum.audio + current.audio;
        shortMeanLive = sAudio > 0.0 ? (shortSum.busy + current.busy) / sAudio : 0.0;
    }

    /** The 200 ms mean as the audio thread sees it, and whether 200 ms of
        audio has been measured. Audio thread only. */
    double getShortMeanOnAudioThread() const noexcept { return shortMeanLive; }
    bool hasShortWindowOnAudioThread() const noexcept { return filledBuckets >= kShortBuckets; }

    //==========================================================================
    // Any thread.
    double getMean200ms() const noexcept { return mean200.load (std::memory_order_relaxed); }
    double getMean2s() const noexcept    { return mean2s.load (std::memory_order_relaxed); }
    double getMean20s() const noexcept   { return mean20s.load (std::memory_order_relaxed); }
    double getP95_2s() const noexcept    { return p95.load (std::memory_order_relaxed); }

    /** Seconds of audio measured so far, capped at 20.48 s. */
    double getMeasuredSeconds() const noexcept { return measured.load (std::memory_order_relaxed); }

    long long getTotalBlocks() const noexcept { return totalBlocks.load (std::memory_order_relaxed); }

private:
    struct Bucket { double busy = 0.0, audio = 0.0; };

    static int binFor (double load) noexcept
    {
        const int b = (int) (load * 50.0);   // 2 % bins
        return b < 0 ? 0 : (b >= kHistBins ? kHistBins - 1 : b);
    }

    Bucket& at (int age) noexcept   // age 0 = newest closed bucket
    {
        return buckets[(size_t) ((bucketIndex - 1 - age + 2 * kNumBuckets) % kNumBuckets)];
    }

    void closeBucket() noexcept
    {
        // Drop what leaves each window, then add the new bucket.
        auto drop = [this] (Bucket& sum, int window)
        {
            if (filledBuckets >= window)
            {
                const auto& old = at (window - 1);
                sum.busy -= old.busy;
                sum.audio -= old.audio;
            }
        };

        drop (shortSum, kShortBuckets);
        drop (midSum, kMidBuckets);
        drop (longSum, kLongBuckets);

        buckets[(size_t) bucketIndex] = current;
        bucketIndex = (bucketIndex + 1) % kNumBuckets;
        filledBuckets = filledBuckets < kNumBuckets ? filledBuckets + 1 : kNumBuckets;

        for (auto* s : { &shortSum, &midSum, &longSum })
        {
            s->busy += current.busy;
            s->audio += current.audio;
        }

        current = {};
        publish();
    }

    void publish() noexcept
    {
        auto mean = [] (const Bucket& b) { return b.audio > 1.0e-9 ? b.busy / b.audio : 0.0; };
        mean200.store (mean (shortSum), std::memory_order_relaxed);
        mean2s.store (mean (midSum), std::memory_order_relaxed);
        mean20s.store (mean (longSum), std::memory_order_relaxed);
        measured.store ((double) filledBuckets * kBucketSeconds, std::memory_order_relaxed);

        // p95 from the histogram.
        double value = 0.0;

        if (blocksFilled > 0)
        {
            const int target = (int) std::ceil (0.95 * (double) blocksFilled);
            int seen = 0;

            for (int b = 0; b < kHistBins; ++b)
            {
                seen += hist[(size_t) b];

                if (seen >= target)
                {
                    value = ((double) b + 0.5) / 50.0;
                    break;
                }
            }
        }

        p95.store (value, std::memory_order_relaxed);
    }

    std::array<Bucket, kNumBuckets> buckets {};
    int bucketIndex = 0, filledBuckets = 0;
    Bucket current, shortSum, midSum, longSum;
    double shortMeanLive = 0.0;

    std::array<unsigned char, kBlockRing> blockLoads {};
    int blockIndex = 0, blocksFilled = 0;
    std::array<int, kHistBins> hist {};

    std::atomic<double> mean200 { 0.0 }, mean2s { 0.0 }, mean20s { 0.0 }, p95 { 0.0 }, measured { 0.0 };
    std::atomic<long long> totalBlocks { 0 };
};

} // namespace luthier
