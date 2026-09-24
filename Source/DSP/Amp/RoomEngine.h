#pragma once

/*  Room engine (engine spec 14).

    The room around the amp, in two parts:

      Early reflections - 16 tapped delays whose times and levels come from the
                          room's dimensions and the mic's distance from the cab.
                          These are what tell a listener how big the room is.
      Late reverb       - an 8-line feedback delay network with per-line damping,
                          for the diffuse tail.

    The mic-to-room-mic blend is the control an engineer actually reaches for:
    how much of the sound is the close mic and how much is the room.
*/

#include "../Common/DspCommon.h"
#include <vector>

namespace luthier
{

//==============================================================================
enum class RoomSize
{
    IsoBooth, SmallBooth, SmallStudio, LargeStudio, LiveRoom, ConcertHall, Cathedral,
    NumRoomSizes
};

enum class RoomMaterial
{
    Dry, Wood, Tile, Stone, NumMaterials
};

//==============================================================================
class RoomEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setEnabled (bool e) noexcept { enabled = e; }
    bool isEnabled() const noexcept { return enabled; }

    void setRoomSize (RoomSize s) noexcept;
    RoomSize getRoomSize() const noexcept { return roomSize; }

    void setMaterial (RoomMaterial m) noexcept;
    RoomMaterial getMaterial() const noexcept { return material; }

    /** 0 = close mic only, 1 = room mic only. */
    void setRoomBlend (double blend) noexcept;

    /** Multiplier on the room's natural decay, 0.25 to 4. */
    void setDecayScale (double scale) noexcept;

    /** Stereo width of the room mics, 0 to 1. */
    void setWidth (double width) noexcept;

    void processBlock (juce::AudioBuffer<float>& buffer) noexcept;

    //==========================================================================
    /** Captures the room's own contribution - early reflections plus late tail,
        at the current blend, without the dry signal - into an internal buffer,
        for routing-io's Aux 5. Off by default, because when nobody is listening
        to that bus the copy is pure waste.

        The tap is taken at the same point the blend is applied, so
        `dry + tap` reproduces the room output exactly. */
    void setRoomTapEnabled (bool e) noexcept { roomTapEnabled = e; }
    bool isRoomTapEnabled() const noexcept { return roomTapEnabled; }

    /** Valid until the next processBlock, and only when the tap is enabled and
        the room is not bypassed. */
    const float* getRoomTap (int channel) const noexcept
    {
        if (! roomTapValid || roomTap.getNumChannels() < 2)
            return nullptr;

        return roomTap.getReadPointer (juce::jlimit (0, 1, channel));
    }

    bool hasRoomTap() const noexcept { return roomTapValid; }
    int getRoomTapNumSamples() const noexcept { return roomTapSamples; }

    static const char* getRoomSizeName (RoomSize s) noexcept;
    static const char* getMaterialName (RoomMaterial m) noexcept;

private:
    static constexpr int kNumTaps = 16;
    static constexpr int kFdnSize = 8;

    void rebuild();
    void updateFeedbackGain() noexcept;

    double sr = 44100.0;
    bool enabled = true;

    RoomSize roomSize = RoomSize::SmallStudio;
    RoomMaterial material = RoomMaterial::Wood;
    double decayScale = 1.0;

    // Aux 5's room-only tap. Allocated in prepare, never in the audio callback.
    juce::AudioBuffer<float> roomTap;
    bool roomTapEnabled = false;
    bool roomTapValid = false;
    int  roomTapSamples = 0;

    // Early reflections.
    std::vector<double> erBuffer;
    int erSize = 0, erMask = 0, erIndex = 0;
    int tapDelays[kNumTaps] = {};
    double tapGainsL[kNumTaps] = {};
    double tapGainsR[kNumTaps] = {};
    Biquad tapFilterL, tapFilterR;

    // Late reverb.
    std::vector<double> lines[kFdnSize];
    int lineLengths[kFdnSize] = {};
    int lineIndex[kFdnSize] = {};
    OnePoleLP lineDamp[kFdnSize];
    double feedbackGain = 0.8;

    ExpSmoother blendSmooth, widthSmooth;
    DCBlocker dcL, dcR;

    JUCE_LEAK_DETECTOR (RoomEngine)
};

} // namespace luthier
