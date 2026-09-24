#pragma once

/*  cpu-quality-modes.md 2.5-2.7, 4 and 7: turns the settings into the level the
    audio thread runs.

        choice    = per-instance override, or the global setting when "global"
        live      = choice's level, or Auto's current level
        effective = High when rendering offline with "Always render offline at
                    High" on, or whenever the choice is Auto; otherwise live

    Owned by LuthierAudioProcessor. Every getter is lock-free and computed from
    atomics, so the audio thread reads getEffectiveLevel() once per block and
    setNonRealtime() may be called from any thread.

    Auto and the E1/E2 load governor run on a 10 Hz message-thread timer
    (tick()). Tests drive tick() directly with an injected clock and load feed.
    E3 (a string dropped when Luthier runs out of CPU) is decided on the audio
    thread by the processor, from CpuLoadMonitor, so it works while the message
    thread is blocked.

    Headless-safe: nothing here knows about the UI. The editor feeds the level
    and relief into AnimationPolicy.
*/

#include "QualityProfile.h"
#include "CpuLoadMonitor.h"
#include "PerformanceSettings.h"
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>

namespace luthier
{

class QualityController : public juce::ChangeBroadcaster,
                          private juce::Timer
{
public:
    explicit QualityController (const CpuLoadMonitor& monitor);
    ~QualityController() override;

    //==========================================================================
    // Inputs.

    void setOverride (QualityOverride o);
    QualityOverride getOverride() const noexcept { return (QualityOverride) overrideChoice.load (std::memory_order_relaxed); }

    /** Any thread (the processor's setNonRealtime, prepareToPlay, processBlock). */
    void setNonRealtime (bool isOffline) noexcept { nonRealtime.store (isOffline, std::memory_order_relaxed); }
    bool isNonRealtime() const noexcept { return nonRealtime.load (std::memory_order_relaxed); }

    /** Choosing Auto again clears a hold (2.7). Message thread. */
    void resumeAuto();

    //==========================================================================
    // Outputs. Lock-free.

    /** Global setting or override, whichever applies. */
    QualityChoice getChoice() const noexcept;
    bool isAuto() const noexcept { return getChoice() == QualityChoice::Auto; }

    /** The level live audio runs at. */
    QualityLevel getLiveLevel() const noexcept;

    /** What the audio thread runs (offline rule included). */
    QualityLevel getEffectiveLevel() const noexcept;

    QualityLevel getAutoLevel() const noexcept { return (QualityLevel) autoLevel.load (std::memory_order_relaxed); }
    bool isAutoHeld() const noexcept { return autoHeld.load (std::memory_order_relaxed); }

    /** The E governor's display relief, 0 to 2 (7). */
    int getReliefLevel() const noexcept { return relief.load (std::memory_order_relaxed); }

    /** E2 freezes the shadow GuitarSpec audition; E1 suspends the data stream. */
    bool isShadowAuditionFrozen() const noexcept { return getReliefLevel() >= 2; }
    bool isDataStreamSuspended() const noexcept  { return getReliefLevel() >= 1; }

    //==========================================================================
    /** A message for the person: a banner (gui-integration 15) and/or a polite
        screen-reader announcement. The editor drains these. */
    struct Notice
    {
        bool banner = false;
        bool announce = false;
        juce::String text;
    };

    bool popNotice (Notice& out);
    int getPendingNoticeCount() const;

    /** The processor reports an E3 drop (audio thread sets a flag; the next
        tick turns it into a notice). */
    void noteStringDropped() noexcept { stringDrops.fetch_add (1, std::memory_order_relaxed); }

    //==========================================================================
    /** One Auto / governor step. Message thread; the timer calls it at 10 Hz. */
    void tick();

    struct LoadSnapshot
    {
        double mean200ms = 0.0, mean2s = 0.0, mean20s = 0.0, p95_2s = 0.0;
        double measuredSeconds = 0.0;
    };

    void setClockForTesting (std::function<double()> nowMs);
    void setLoadFeedForTesting (std::function<LoadSnapshot()> feed);

    /** Forces the live level (Auto and the settings are ignored); empty clears. */
    void forceLevelForTesting (int levelOrMinusOne) noexcept { forced.store (levelOrMinusOne, std::memory_order_relaxed); }

    /** Restarts Auto from `auto_last_level`, as a new session would. */
    void restartAutoForTesting();

    /** Stops the 10 Hz timer (tests drive tick() themselves). */
    void stopTimerForTesting() { stopTimer(); }

    //==========================================================================
    /** Plain-words names (UI strings go through the catalogue at the call site). */
    static juce::String levelName (QualityLevel l);
    static juce::String shortLabel (QualityChoice choice, QualityLevel live);

    /** Auto rules (2.7). */
    static constexpr double kDownMean = 0.65, kDownP95 = 0.85;
    static constexpr double kDownWindowMs = 2000.0, kDwellMs = 3000.0;
    static constexpr double kUpMean = 0.30, kUpPredicted = 0.50;
    static constexpr double kUpWindowMs = 20000.0, kNoUpAfterDownMs = 30000.0;
    static constexpr int kHoldDowns = 3;
    static constexpr double kHoldWindowMs = 600000.0;
    static constexpr double kBannerGapMs = 60000.0;
    static constexpr double kDefaultRatioMediumToHigh = 1.25, kDefaultRatioLowToMedium = 1.30;

    /** Governor (7). */
    static constexpr double kE1Enter = 0.85, kE2Enter = 0.90, kELeave = 0.70, kE2HoldMs = 200.0;

private:
    void timerCallback() override { tick(); }
    double now() const;
    LoadSnapshot readLoad() const;
    void stepAuto (int newLevel, bool down, double t);
    void pushNotice (Notice n);

    const CpuLoadMonitor& monitor;

    std::atomic<int> overrideChoice { (int) QualityOverride::Global };
    std::atomic<bool> nonRealtime { false };
    std::atomic<int> forced { -1 };

    // Auto (message thread writes, any thread reads the atomics).
    std::atomic<int> autoLevel { (int) QualityLevel::High };
    std::atomic<bool> autoHeld { false };
    bool autoWasActive = false;
    double levelSince = 0.0, lastStep = -1.0e12, lastDown = -1.0e12, lastBanner = -1.0e12;
    double downTimes[kHoldDowns] = { -1.0e12, -1.0e12, -1.0e12 };
    int downCount = 0;
    double measuredLoad[3] = { 0.0, 0.0, 0.0 };   ///< a level's 2 s mean seen this session

    // Governor.
    std::atomic<int> relief { 0 };
    double e2Since = -1.0;

    std::atomic<int> stringDrops { 0 };
    bool announcedFirstDrop = false;
    double lastAnnouncement = -1.0e12;

    juce::Array<Notice> notices;
    juce::CriticalSection noticeLock;

    std::function<double()> clock;
    std::function<LoadSnapshot()> loadFeed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QualityController)
};

} // namespace luthier
