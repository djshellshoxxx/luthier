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
    explicit FretboardComponent (LuthierAudioProcessor& processor);
    ~FretboardComponent() override;

    //==========================================================================
    void setScaleOverlay (ScaleOverlay scale, int rootPitchClass);
    ScaleOverlay getScaleOverlay() const noexcept { return scale; }
    int getScaleRoot() const noexcept { return scaleRoot; }

    void setCapoFret (int fret);
    int getCapoFret() const noexcept { return capoFret; }

    void setStringMuted (int stringIndex, bool muted);
    bool isStringMuted (int stringIndex) const;

    /** Highlights one string as the selected one in Advanced mode. */
    void setSelectedString (int stringIndex);
    int getSelectedString() const noexcept { return selectedString; }

    std::function<void (int stringIndex)> onStringSelected;

    /** Compact mode drops the fret numbers and inlay row to save height. */
    void setCompact (bool shouldBeCompact);

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
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

    /** For tests: where the bar is drawn, and how visible it is (0-1). */
    double getDrawnBarFret() const noexcept { return barFret; }
    float getBarOpacity() const noexcept { return barOpacity; }

private:
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FretboardComponent)
};

} // namespace luthier
