#pragma once

/*  What the band is doing, for the UI (jam-mode.md 8.3).

    The audio thread publishes a JamStatus after each block into a double
    buffer with an atomic sequence number; the UI drains it at 30 Hz. After
    250 ms without an update the snapshot is stale: the playhead hides and
    the status line shows "-".
*/

#include "../Rhythm/ChordDetector.h"
#include <array>
#include <atomic>

namespace luthier
{

enum class JamState { off = 0, armed, counting, playing, ending };
const char* getJamStateName (JamState s) noexcept;

struct JamStatus
{
    static constexpr int kNumLanes = 16;      ///< 7 drum lanes, bass, 8 spare
    static constexpr int kLaneSteps = 32;
    static constexpr int kChordTrack = 5;     ///< 2 bars back, this bar, 2 ahead

    enum class NextSource : uint8_t { none = 0, tune, predicted };
    enum class Clock : uint8_t { own = 0, host, tune };

    JamState state = JamState::off;
    int64_t bar = 0;               ///< 0-based from the band's start (negative while counting in)
    int beat = 0;                  ///< 0-based within the bar
    int step = 0, stepsInBar = 16, stepsPerBeat = 4;
    int baseIntensity = 3, effectiveIntensity = 3;
    int style = 0, variation = 0, kit = 0, bassVoice = 0;
    double bpm = 120.0;
    int meterNumerator = 4, meterDenominator = 4;
    Clock clock = Clock::own;

    ChordSymbol currentChord, nextChord;
    NextSource nextSource = NextSource::none;
    bool predicting = false;
    bool waitingForChord = true;
    bool fillActive = false;
    bool genericGroove = false;       ///< the style has no groove in this meter
    bool bassResting = false;         ///< the player is the bassist
    bool tuneBassPlaying = false;     ///< the tune's bass is playing through the Jam bass
    bool followingTune = false;
    bool noPlayHead = false;          ///< Host Transport with no transport

    /** 16 lanes of 32 steps: 0 = nothing, else the hit's velocity (1-127). */
    std::array<std::array<uint8_t, kLaneSteps>, kNumLanes> lanes {};

    /** 16 hit bitmasks (one bit per step), the same lanes. */
    std::array<uint32_t, kNumLanes> laneHits {};

    std::array<ChordSymbol, kChordTrack> chordTrack {};
    std::array<uint8_t, kChordTrack> chordTrackSource {};   ///< NextSource per slot

    double drumsPeak = 0.0, bassPeak = 0.0;
    uint32_t sequence = 0;
};

/** The double buffer. One writer (the audio thread), one reader (the UI). */
class JamStatusChannel
{
public:
    void publish (const JamStatus& s) noexcept
    {
        const auto next = counter.load (std::memory_order_relaxed) + 1;
        auto& slot = slots[(size_t) (next & 1u)];
        slot = s;
        slot.sequence = next;
        counter.store (next, std::memory_order_release);
    }

    /** False when nothing has been published yet or the read raced twice. */
    bool read (JamStatus& out) const noexcept
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto before = counter.load (std::memory_order_acquire);

            if (before == 0)
                return false;

            out = slots[(size_t) (before & 1u)];

            if (counter.load (std::memory_order_acquire) == before)
                return true;
        }

        return false;
    }

    uint32_t getSequence() const noexcept { return counter.load (std::memory_order_acquire); }

private:
    std::array<JamStatus, 2> slots {};
    std::atomic<uint32_t> counter { 0 };
};

} // namespace luthier
