#pragma once

#include "AnimationPolicy.h"   // cpu-quality-modes 6

/*  The interactive fretboard.

    Shows every string and fret, lights up the notes the engine is actually
    sounding, and lets the user click a fret to hear it. It reads its state from
    the engine rather than from the MIDI stream, so what is lit is what is
    ringing - including notes the chord voicer placed somewhere the player did not
    expect, which is exactly when you want to see it.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "Guitar/GuitarRenderer.h"
#include "Guitar/StringAnimator.h"   // animated-strings.md 4.3

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
enum class ScaleOverlay
{
    None, Major, NaturalMinor, HarmonicMinor, MajorPentatonic, MinorPentatonic,
    Blues, Dorian, Mixolydian, Lydian, Phrygian, WholeTone, Chromatic,
    NumScales
};

const char* getScaleName (ScaleOverlay s) noexcept;

//==============================================================================
class FretboardComponent : public juce::Component,
                           public juce::SettableTooltipClient,
                           private juce::Timer
{
public:
    /** SPEC-SWEEP (GD-2, gui-engine-dataflow 0.2): the spec's drain rate. */
    static constexpr int kRefreshHz = 60;

    /** SPEC-SWEEP (GD-14, gui-engine-dataflow 6.4): the slide bar's alpha - 80%
        of its fade-in opacity, dimmed to 60% of that once the bar has not moved
        for kSlideStaleMs. */
    static constexpr double kSlideStaleMs = 200.0;
    static float slideBarAlpha (float opacity, double msSinceMove) noexcept
    {
        return 0.8f * opacity * (msSinceMove > kSlideStaleMs ? 0.6f : 1.0f);
    }
    int getRefreshIntervalMs() const noexcept { return getTimerInterval(); }

    explicit FretboardComponent (LuthierAudioProcessor& processor);
    ~FretboardComponent() override;

    /** One frame of the timer's work (tests). */
    void tickForTest() { timerCallback(); }

    //==========================================================================
    void setScaleOverlay (ScaleOverlay scale, int rootPitchClass);
    ScaleOverlay getScaleOverlay() const noexcept { return scale; }
    int getScaleRoot() const noexcept { return scaleRoot; }

    void setCapoFret (int fret);
    int getCapoFret() const noexcept { return capoFret; }

    /*  piano-roll-chord-display.md 3, "Show fingering": the voicing a pianist's
        keys would get, as hollow ghost dots, before anything sounds. `fret` is
        counted from the capo, as the engine counts it. */
    struct GhostDot { int string = 0; double fret = 0.0; };
    void setGhostDots (const std::vector<GhostDot>& dots);
    const std::vector<GhostDot>& getGhostDots() const noexcept { return ghostDots; }

    void setStringMuted (int stringIndex, bool muted);
    bool isStringMuted (int stringIndex) const;

    /** Highlights one string as the selected one in Advanced mode. */
    void setSelectedString (int stringIndex);
    int getSelectedString() const noexcept { return selectedString; }

    std::function<void (int stringIndex)> onStringSelected;

private:
    std::vector<GhostDot> ghostDots;   // piano-roll-chord-display.md 3
public:

    /** Compact mode drops the fret numbers and inlay row to save height. */
    void setCompact (bool shouldBeCompact);

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // SPEC-SWEEP (KS-23/KS-24): the right-click menu split from showing it
    // (1 mute, 2 select, 3 capo here, 4 remove capo, 100+ scale, 200+ root),
    // the last click's velocity, and where a string/fret/lane point is.
    juce::PopupMenu buildContextMenu (int stringIndex, int fret);
    void applyContextMenuResult (int stringIndex, int fret, int result);
    double getLastClickVelocity() const noexcept { return lastClickVelocity; }
    juce::Point<int> pointOnString (int stringIndex, int fret, float withinLane) const;

private:
    double lastClickVelocity = 0.0;   // SPEC-SWEEP KS-23
    void timerCallback() override;

    /** Fret positions follow the real rule: each fret sits at
        scale / 2^(n/12) from the nut, so they crowd together up the neck exactly
        as they do on the instrument. */
    float fretX (double fret) const;
    double fretAtX (float x) const;
    int stringAtY (float y) const;
    float stringY (int stringIndex) const;

    bool isNoteInScale (int stringIndex, int fret) const;
    int pitchClassAt (int stringIndex, int fret) const;

    LuthierAudioProcessor& processor;

    int numStrings = 6;
    int numFrets = 24;
    bool compact = false;

    ScaleOverlay scale = ScaleOverlay::None;
    int scaleRoot = 4;   // E
    int capoFret = 0;

    /*  slide-guitar.md 7 and gui-integration 21: the bar, drawn where it is
        and eased toward it over 80 ms, fading in and out as it lands and
        lifts. */
    double barFret = -1.0;
    float barOpacity = 0.0f;

    /*  cpu-quality-modes 6: Decorative. At Off the timer stops and the
        policy's 4 Hz poll runs the same refresh in static mode: sounding
        strings get a fixed glow, the slide bar jumps instead of easing, and it
        repaints only when something shown changed. */
    bool staticMode = false;
    void staticRefresh() { staticMode = true; timerCallback(); staticMode = false; }
    AnimationPolicy::Registration motion { *this, AnimationPolicy::Decorative, "FretboardComponent",
                                           [this] { staticRefresh(); repaint(); },
                                           [this] { staticRefresh(); } };
    float barSlantDegrees = 0.0f;
    double barLastMoveMs = 0.0, barLastTarget = -1.0;   // SPEC-SWEEP GD-14
    juce::Colour barColour;

public:
    /*  notation-export 3 (MODEL-GAPS, TODO 9): the current bar of the capture
        as tablature dots - each note of the bar at its string and fret,
        numbered, the newest brightest. Shown while the NOTATION tab's switch
        is on. */
    struct TabDot
    {
        int stringIndex = 0;
        double fret = 0.0;
        float age = 0.0f;   ///< 0 the newest note of the bar, 1 its first
    };

    const std::vector<TabDot>& getTabDots() const noexcept { return tabDots; }

    /** The current bar's notes, from the capture: what the timer does. */
    void refreshTabDots();

    // animated-strings.md 4.3: the strings' frame driver, and one 30 Hz tick for tests.
    StringAnimator& getStringAnimator() noexcept { return animator; }
    void tickForTesting() { timerCallback(); }
    const std::array<StringLook, 12>& getStringLooks() const noexcept { return looks; }

    /** The fretboard's drawn width of a string: the 0.9 + 1.5 s / (n - 1) rule,
        scaled by its gauge relative to the set's mean (animated-strings 2.3). */
    float stringThickness (int stringIndex) const;

    /** SPEC-SWEEP: A11Y-8 - one invisible, titled cell per string and fret
        ("String 1, fret 3, G4") that a screen reader can walk; clicks pass
        through to the board. Rebuilt when the tuning, capo or size changes. */
    juce::Component* getFretCell (int stringIndex, int fret) const;
    int getNumFretCells() const noexcept { return fretCells.size(); }

    /** Rebuilds the cells' titles if the tuning, capo or size changed (the
        timer calls it; public for tests). */
    void updateFretCells();

    /** For tests: where the bar is drawn, and how visible it is (0-1). */
    double getDrawnBarFret() const noexcept { return barFret; }
    float getBarOpacity() const noexcept { return barOpacity; }

private:
    // SPEC-SWEEP: A11Y-8
    class FretCell;
    juce::OwnedArray<juce::Component> fretCells;
    juce::String fretCellsSignature;
    void layoutFretCells();

    int selectedString = 0;

    std::array<bool, 12> muted {};

    // Live state, refreshed by the timer.
    std::array<double, 12> liveFret {};
    std::array<double, 12> liveLevel {};
    std::array<int, 12> liveNote {};

    int hoverString = -1;
    int hoverFret = -1;
    int playingString = -1;

    juce::Rectangle<int> boardArea;

    std::vector<TabDot> tabDots;   // MODEL-GAPS
    // ==== BEGIN REALISM-B fretboard ====
    /*  harmonic-realism.md 7: a hollow ring where a string is touched, fading
        over the touch (dashed when it missed a node); string-interaction.md 9:
        the palm's coverage as a band per string; fingerstyle-attack.md 7: each
        string's tool glyph at the picking end. FretboardRealismB.cpp. */
    struct RealismBView { float touchFret = -1.0f, life = 0.0f, palm = 0.0f; bool missed = false; int tool = 0; };
    std::array<RealismBView, 12> realismB {};
    void paintRealismB (juce::Graphics&);

public:
    /** From the timer (and the tests): re-reads the engine; true if anything moved. */
    bool refreshRealismB() noexcept;

    /** For tests: what the REALISM-B layer is drawing for a string. */
    const RealismBView& getRealismBView (int s) const noexcept { return realismB[(size_t) juce::jlimit (0, 11, s)]; }

private:
    // ==== END REALISM-B fretboard ====

    // animated-strings.md 2.1 and 6.1.
    bool fillMotionGeometry (StringMotionGeometry&);
    void refreshStringLooks (bool force);
    juce::Rectangle<int> noteDotArea (int stringIndex, double fret) const;

    // animated-strings.md 11: the static board, cached while the strings animate.
    void paintStaticLayer (juce::Graphics&);
    void paintLiveLayer (juce::Graphics&);
    void paintFretNumbers (juce::Graphics&);
    juce::int64 staticLayerKey (float scale) const;
    juce::Image staticCache;
    juce::int64 staticCacheKey = 0;

    std::array<StringLook, 12> looks {};
    juce::int64 looksKey = 0;
    int ticksSinceLooksCheck = 1000;

    StringAnimator animator;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FretboardComponent)
};

} // namespace luthier
