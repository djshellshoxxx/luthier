#pragma once

/*  The vibrating-string model for the display (animated-strings.md 2 and 4.2).

    Pure logic, no Component: given a SoundingNotes snapshot and the strings'
    geometry in pixels, it works out, per string, the swept envelope a real
    string shows the eye (2.2), the sideways push of a finger bend (2.4), the
    damping mask and extra decay of a mute (2.5), and the dirty rectangle a
    frame must repaint (4.2).

    The geometry is the same for both views. A string runs from its nut point to
    its bridge point, and a fret f stops it at nut + (bridge - nut) * (1 - 2^(-f/12)).
    On the illustration the bridge point is the saddle; on the fretboard it is
    the virtual scale-length point beyond the board's end (2.1).

    Everything is fixed-size, so update() allocates nothing (11).
*/

#include <juce_graphics/juce_graphics.h>
#include "../../Support/SoundingNotes.h"

#include <array>

namespace luthier
{

//==============================================================================
enum class StringAnimationQuality { low = 0, high = 1 };

//==============================================================================
/** Where each string lies on screen, in the owner component's pixels. */
struct StringMotionGeometry
{
    static constexpr int kMaxStrings = SoundingNotes::kMaxStrings;

    struct String
    {
        juce::Point<float> nut, bridge;
        float strokeWidthPx = 1.0f;    ///< the string's drawn width, for the dirty margin
    };

    std::array<String, kMaxStrings> strings {};
    int numStrings = 0;
    float numFrets = 24.0f;            ///< stopFret is clamped to [0, numFrets] (12)
    juce::Rectangle<float> clip;       ///< dirty rects are clipped to this; empty = no clip

    /** The point `fret` frets from the nut on string s. */
    juce::Point<float> pointAt (int s, float fret) const noexcept;

    /** The point a fraction x of the way from the nut to the bridge. */
    juce::Point<float> pointAtFraction (int s, float x) const noexcept;

    /** The distance to the nearest neighbouring string at the same fraction. */
    float spacingAt (int s, float x) const noexcept;

    /** A unit vector from string s toward the bass side (higher engine index). */
    juce::Point<float> towardBass (int s, float x) const noexcept;

    static float fractionForFret (float fret) noexcept;
};

//==============================================================================
struct StringMotionFrame
{
    static constexpr int kMaxSamples = 33;
    static constexpr int kMaxStrings = SoundingNotes::kMaxStrings;

    struct String
    {
        bool active = false;                  ///< vibrating above the floor this frame
        juce::Point<float> nut, stop, displacedStop, bridge;
        juce::Point<float> normal;            ///< unit, perpendicular to displacedStop -> bridge
        std::array<float, kMaxSamples> samples {};   ///< half-width in px at u = i / (numSamples - 1)
        int numSamples = 0;
        float levelNorm = 0.0f;               ///< L_n after damping decay and staleness
        float peakPx = 0.0f;                  ///< the largest half-width
        float amplitudeMaxPx = 0.0f;          ///< A_max (after LightTouch)
        float displacementPx = 0.0f;          ///< the bend's push (2.4)

        juce::Rectangle<float> swept;         ///< this frame's bounds, margins included; empty when inactive
        juce::Rectangle<int> dirty;           ///< what must be repainted this frame
        bool hasDirty = false;

        /** The half-width at u in [0, 1], interpolated from the samples. */
        float amplitudeAt (float u) const noexcept;

        /** The point at u on the speaking length, offset by `along` times the normal. */
        juce::Point<float> pointAt (float u, float along) const noexcept
        {
            return displacedStop + (bridge - displacedStop) * u + normal * along;
        }
    };

    std::array<String, kMaxStrings> strings {};
    int numStrings = 0;
    StringAnimationQuality quality = StringAnimationQuality::high;

    int getNumActive() const noexcept;
};

//==============================================================================
class StringMotion
{
public:
    StringMotion() { reset(); }

    /** Forgets everything: the family switched, or the animation stopped (10). */
    void reset() noexcept;

    /** L_n = clamp (level * 4, 0, 1): the illustration's own normaliser, shared by
        both views (2.2). A non-finite level is 0. */
    static float normaliseLevel (float level) noexcept;

    /** 2.4: the lateral push in px for `spacingPx` and a finger bend. */
    static float bendDisplacement (float spacingPx, float bendCents) noexcept;

    /** 2.2's E(u) before the damping mask, normalised so its peak over the sample
        grid is 1, written into out[0..numSamples-1]. */
    static void envelope (float pluckPosition, double noteSeconds, int harmonicPartial,
                          StringAnimationQuality quality, int numSamples, float* out) noexcept;

    /** 2.5's D(u). */
    static float dampingMask (uint8_t damping, float u) noexcept;

    /** Applies 2.2, 2.4 and 2.5 for one frame at display time `nowSeconds`.
        `gain` scales every level: 1 normally, easing to 0 when the snapshot goes
        stale (2.6). Allocates nothing. */
    void update (const SoundingNotes::Frame& snapshot, const StringMotionGeometry& geometry,
                 StringAnimationQuality quality, double nowSeconds, float gain,
                 StringMotionFrame& frame) noexcept;

    /** True when update() would find a string above the floor: the animator asks
        this before starting its clock, so an idle view costs no frames (11). */
    bool anyAboveFloor (const SoundingNotes::Frame& snapshot, const StringMotionGeometry& geometry,
                        double nowSeconds, float gain) const noexcept;

    static constexpr float kFloorPx = 0.5f;
    static constexpr double kMuteDecaySeconds = 0.025;

private:
    float levelFor (int s, const SoundingNotes::Motion& rec, double nowSeconds, float gain) const noexcept;

    std::array<juce::Rectangle<float>, StringMotionFrame::kMaxStrings> previousSwept {};
    std::array<bool, StringMotionFrame::kMaxStrings> previousActive {};
    std::array<double, StringMotionFrame::kMaxStrings> muteOnset {};
    std::array<float, StringMotionFrame::kMaxSamples> scratch {};
};

} // namespace luthier
