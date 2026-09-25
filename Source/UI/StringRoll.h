#pragma once

/*  The string roll: a small piano roll whose lanes are the strings.

    One lane per string in the fretboard's top-to-bottom order, time running
    left to right with "now" at the right edge. The notes the engine actually
    played (the performance capture, notation-export.md 6 - voiced, not the
    incoming MIDI) scroll by as rounded bars labelled with their fret; the
    right edge of each lane glows from the engine's live string level so a
    pluck lights up at once rather than a capture drain later.

    And the other way round: clicking a lane plucks that string, the fret from
    where in the lane you click (higher up the lane is higher up the neck) and
    the velocity from how far right. Lanes take keyboard focus; Enter or Space
    plucks (accessibility.md 2), and each lane names its string to a screen
    reader (accessibility.md 1).

    It lives on the NOTATION tab above the live tab (gui-integration.md 19,
    "Live TAB view") and is built so the Advanced top strip can offer it as a
    FRETS | ROLL alternative to the fretboard.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <array>

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class StringRollComponent : public juce::Component,
                            private juce::Timer,
                            private juce::ComponentListener
{
public:
    explicit StringRollComponent (LuthierAudioProcessor& processor);
    ~StringRollComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** What a click, Enter or a screen reader's press on a lane does. */
    void pluck (int stringIndex, int fret, double velocity);
    void release (int stringIndex);

    /** Re-reads the capture and the engine (the timer's work; tests call it). */
    void refresh();

    /*  The timer runs only while the roll can be seen: two of these live in
        the editor (the strip and the NOTATION tab), and a hidden one walking
        the capture thirty times a second bought nothing. Own visibility, and
        every ancestor's, since a hidden parent hides this without telling it. */
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

    /** True while the refresh timer runs (tests). */
    bool isRefreshRunning() const noexcept { return isTimerRunning(); }

    //==========================================================================
    // For tests.
    int getVisibleNoteCount() const noexcept    { return visibleNotes; }
    bool isShowingEmptyHint() const noexcept    { return visibleNotes == 0; }
    double getWindowSeconds() const noexcept    { return windowSeconds; }
    int getRefreshHz() const noexcept;
    int getNumLanes() const noexcept            { return numStrings; }
    juce::Component& getLane (int stringIndex);
    juce::String getLaneTooltip (int stringIndex, int fret) const;

private:
    class Lane;
    friend class Lane;

    void timerCallback() override;
    int wantedRefreshHz() const;
    void rebuildLanes (int count);

    // The ancestors' visibility (see visibilityChanged).
    void componentVisibilityChanged (juce::Component&) override;
    void watchAncestors();
    void updateTimerState();
    juce::Array<juce::Component::SafePointer<juce::Component>> watchedAncestors;

    juce::Rectangle<int> rollArea() const;
    juce::Rectangle<int> laneBounds (int stringIndex) const;
    float xForSample (juce::int64 sample, juce::Rectangle<int> roll) const;
    int fretAtY (float y, float laneHeight) const;
    double velocityAtX (float x, float laneWidth) const;
    void setHover (int stringIndex, int fret);

    LuthierAudioProcessor& processor;

    static constexpr int kMaxLanes = 12;

    int numStrings = 6;
    int runningHz = 0;
    juce::OwnedArray<Lane> lanes;
    std::array<juce::String, kMaxLanes> names;

    // The live edge (gui-engine-dataflow 2, per-string activity): eased when
    // motion is allowed, hard-set under reduced motion.
    std::array<float, kMaxLanes> glow {};
    bool reducedMotion = false;

    // The clock the window scrolls on: the take's newest sample, advanced by
    // wall time between drains so the bars move smoothly at 30 Hz rather than
    // stepping at the capture's 10 Hz.
    bool anchored = false;
    juce::int64 anchorSample = 0;
    double anchorWallMs = 0.0;
    juce::int64 nowSample = 0;
    double sampleRate = 48000.0;

    bool captureOff = false;
    bool musical = false;
    double bpm = 120.0;
    int numerator = 4, denominator = 4;
    double windowSeconds = 8.0;
    int visibleNotes = 0;

    int hoverString = -1, hoverFret = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringRollComponent)
};

} // namespace luthier
