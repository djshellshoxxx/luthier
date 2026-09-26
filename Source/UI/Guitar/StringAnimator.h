#pragma once

/*  The frame driver for the animated strings (animated-strings.md 2.6 and 4.3).

    The illustration and the fretboard each own one. It decides whether the
    strings may move at all:

        animate = pref.animateStrings && ! reduced motion && the owner is showing
                  && relief level < 2 && ! stale snapshot

    and, while they may and a string is above the floor, runs a frame clock (a
    VBlankAttachment throttled to 60 Hz at High, or a 30 Hz Timer at Low and
    whenever the owner has no peer). Each frame reads the SoundingNotes
    snapshot, updates the StringMotion model, and asks the owner to repaint one
    dirty rectangle per moving string. It never repaints the whole owner.

    The owner's existing 30 Hz timer calls poll(), which reads the preference
    and the gates and starts or stops the clock. That is also how every open
    editor picks up a change made in Options on its next tick (5).
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "StringMotion.h"
#include "../AnimationPolicy.h"   // cpu-quality-modes 6

#include <functional>
#include <memory>

namespace luthier
{

//==============================================================================
/** The two Options -> Appearance -> Visual aids preferences (animated-strings.md 5),
    stored in UiPreferences (ui.json), not in presets or host state. */
struct StringAnimationSettings
{
    static constexpr const char* enabledKey = "animateStrings";
    static constexpr const char* qualityKey = "animateStringsQuality";

    static bool isEnabled();                          ///< default off
    static void setEnabled (bool);
    static StringAnimationQuality getQuality();       ///< default High
    static void setQuality (StringAnimationQuality);
};

//==============================================================================
class StringAnimator : private juce::Timer
{
public:
    /** Fills the geometry in the owner's pixels; returns false while there is no
        scene yet (12: nothing moves until the scene exists). */
    using GeometryProvider = std::function<bool (StringMotionGeometry&)>;

    StringAnimator (juce::Component& owner, const SoundingNotes& source, GeometryProvider geometry);
    ~StringAnimator() override;

    /** The owner's 30 Hz tick: reads the preference and gates, starts or stops
        the clock. Cheap when idle. */
    void poll();

    /** The family or the scene changed: the next frame starts from rest (10). */
    void resetMotion();

    /** performance-budget 8's hook: 1 forces Low, 2 or more pauses. */
    void setReliefLevel (int level);
    int getReliefLevel() const noexcept { return reliefLevel; }

    //==========================================================================
    /** The preference is on and not overridden by Reduced motion: the owner
        draws speaking lengths itself (and omits them from its cache). */
    bool isAnimationEnabled() const noexcept { return enabledByUser; }

    /** The ghost replaces the layer-26 glow (2.3): enabled, showing, not paused. */
    bool isMotionActive() const noexcept { return motionActive; }

    /** The frame the owner paints; nullptr while nothing moves. */
    const StringMotionFrame* getFrameToPaint() const noexcept { return drawing ? &frame : nullptr; }

    /** The quality in effect: the preference, forced Low by relief or by the
        over-budget fallback (12). */
    StringAnimationQuality getEffectiveQuality() const noexcept;

    /** The owner reports how long its paint took, for 12's over-budget fallback. */
    void notePaintMilliseconds (double ms) noexcept { lastPaintMs = ms; }

    //==========================================================================
    // Test hooks (4.3).
    void setClockForTesting (std::function<double()> clock);
    void stepFrameForTesting();
    bool simulateVBlankForTesting (double timestampSeconds);
    bool isRunning() const noexcept;
    juce::Rectangle<int> getLastDirtyUnion() const noexcept { return lastDirtyUnion; }
    int getLastDirtyArea() const noexcept { return lastDirtyArea; }
    const juce::RectangleList<int>& getLastDirtyRects() const noexcept { return lastDirtyRects; }
    int getFullRepaintCount() const noexcept { return fullRepaints; }
    int getRepaintCallCount() const noexcept { return repaintCalls; }
    int getFrameCount() const noexcept { return frames; }
    bool isStale() const noexcept { return stale; }
    bool isDroppedToLow() const noexcept { return droppedToLow; }
    const StringMotionFrame& getFrame() const noexcept { return frame; }

    /** For tests that have no desktop window: treat the owner as showing when it
        and its parents are visible (isShowing() needs a peer). */
    static bool isEffectivelyShowing (const juce::Component& c);

    static constexpr double kStaleSeconds = 0.250;
    static constexpr double kEaseSeconds = 0.120;
    static constexpr double kHighMinInterval = 0.015;
    static constexpr double kLowMinInterval = 0.030;
    static constexpr double kFrameBudgetMs = 2.0;

private:
    void timerCallback() override;
    void onVBlank (double timestampSeconds);
    void frameAt (double now);
    void startClock();
    void stopClock();
    void goToRest();
    double now() const;
    bool readSnapshot (double now);
    float staleGain (double now) const noexcept;

    juce::Component& owner;
    const SoundingNotes& source;
    GeometryProvider geometryProvider;

    /*  cpu-quality-modes 6 / animated-strings 2.6: the strings are Decorative.
        The 30 Hz fallback clock runs through the registration, and a policy
        change re-polls (Limited forces the Low style, Off the static overlay). */
    AnimationPolicy::Registration motionRegistration { owner, AnimationPolicy::Decorative, "StringAnimator",
                                                        [this] { poll(); } };

    StringMotion motion;
    StringMotionFrame frame;
    StringMotionGeometry geometry;
    SoundingNotes::Frame snapshot;

    std::unique_ptr<juce::VBlankAttachment> vblank;
    std::function<double()> testClock;

    uint32_t lastSequence = 0;
    double lastSequenceChange = -1.0e9;
    double lastFrameTime = -1.0e9;

    int reliefLevel = 0;
    bool enabledByUser = false;
    bool motionActive = false;
    bool drawing = false;
    bool stale = true;
    bool droppedToLow = false;
    int overBudgetFrames = 0;
    double lastPaintMs = 0.0;

    juce::Rectangle<int> lastDirtyUnion;
    juce::RectangleList<int> lastDirtyRects;
    std::array<juce::Rectangle<int>, StringMotionFrame::kMaxStrings> dirtyScratch {};
    int lastDirtyArea = 0;
    int fullRepaints = 0, repaintCalls = 0, frames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringAnimator)
};

} // namespace luthier
