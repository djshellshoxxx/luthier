#pragma once

/*  Freeze: the captured-loop infinite sustain.

    ambiguity-resolutions.md section 2 resolves "freeze / sustain infinite" as a
    hybrid. This file is the Freeze half of that resolution (2.1): a window of the
    signal is captured and crossfaded into a layer that holds indefinitely. The
    E-Bow half (2.2) is a different mechanism entirely - it drives the string at
    its own resonance through the feedback path - and lives in LuthierEngine.

    The two are separate features with separate enables, because the resolution
    says so. A single "Freeze / E-Bow" control would be the old ambiguity back.

    Why a seam-crossfaded loop rather than two overlapping read heads: the spec's
    test is that the layer's RMS varies by less than 0.5 dB across a sixty-second
    hold. Two heads half a window apart under Hann windows satisfy constant
    overlap-add, but that only guarantees the *windows* sum to one. For material
    the two heads read as uncorrelated - which is most of it, since they are
    hundreds of milliseconds apart - power adds rather than amplitude, so the
    layer's level follows sqrt(wA^2 + wB^2) and swings about 3 dB every cycle.
    Crossfading the seam once, at capture time, and then reading a single head
    makes every cycle bit-identical to the last, which is the property the test
    is actually asking for.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Common/DspCommon.h"

namespace luthier
{

//==============================================================================
class FreezeOverlay
{
public:
    /** ambiguity-resolutions 2.1 ranges. */
    static constexpr double kMinCaptureMs = 200.0;
    static constexpr double kMaxCaptureMs = 1000.0;
    static constexpr double kMinAttackMs  = 5.0;
    static constexpr double kMaxAttackMs  = 500.0;
    static constexpr double kMinReleaseMs = 20.0;
    static constexpr double kMaxReleaseMs = 2000.0;

    /** How much of the captured window is spent hiding the seam. */
    static constexpr double kCrossfadeMs = 25.0;

    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;

    /** Rising edge captures a fresh window; a new freeze replaces the layer. */
    void setEnabled (bool shouldBeEnabled) noexcept;
    bool isEnabled() const noexcept { return enabled; }

    /** True once a window has been captured and the layer is sounding. */
    bool isHolding() const noexcept { return state == State::holding; }

    /** The loop's period in samples, or 0 before one has been captured. */
    int getLoopLength() const noexcept { return loopLength; }

    void setCaptureMs (double ms) noexcept;
    void setLevelDb (double db) noexcept;
    double getLevelGain() const noexcept { return levelLinear; }   // SPEC-SWEEP AR-10
    void setAttackMs (double ms) noexcept;
    void setReleaseMs (double ms) noexcept;
    void setLowpassHz (double hz) noexcept;
    void setHighpassHz (double hz) noexcept;

    /** Audio thread. Captures from the buffer while filling, and adds the held
        layer into it once there is one. Never allocates. */
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    enum class State { idle = 0, capturing, holding, releasing };

    void beginCapture() noexcept;

    /** Blends the natural continuation back over the loop start, once. */
    void buildLoop() noexcept;

    void updateFilters() noexcept;

    double sr = 44100.0;
    int channels = 2;

    juce::AudioBuffer<float> captured;

    State state = State::idle;
    bool enabled = false;

    int captureLength = 0;     ///< samples requested, including the crossfade tail
    int crossfade = 0;         ///< samples of seam blend
    int loopLength = 0;        ///< captureLength - crossfade; the period actually played
    int capturedSoFar = 0;

    int readPosition = 0;

    double captureMs = 400.0;
    double levelLinear = 0.5011872336272722;   ///< -6 dB, the spec's default
    double attackMs = 40.0;
    double releaseMs = 300.0;
    double lowpassHz = 18000.0;
    double highpassHz = 20.0;

    /** The attack / release envelope on the layer, not on the input. */
    double envelope = 0.0;

    std::array<Biquad, 2> lowpass {};
    std::array<Biquad, 2> highpass {};

    bool filtersDirty = true;
};

} // namespace luthier
