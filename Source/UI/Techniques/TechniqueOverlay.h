#pragma once

/*  The fretboard's technique overlays (gui-techniques-updates.md 4) and its
    technique input layer.

    Overlays, each independently toggleable (UiPreferences), drawn above the
    fretboard's own layers (guitar-illustration.md 5's layer 33 onward):

      33 mute-zone shading  a translucent band where the palm rests
      34 scrape trail       the pick's path along a string, with catch ticks, fading
      35 tap markers        squares at the taps; released ones fade over 100 ms
      36 bend arc           an arc from the fretted note toward the bent pitch, with a cents badge
      37 slap impact        the bridge end flashes on a slap

    Reduced motion (9): fades become instant state changes.

    Input (the fretboard forwards its mouse here): with TAP armed on the
    fretboard source, a click taps rather than picks (two-hand-tapping.md 3);
    with the slide's position source on Fretboard Drag, a horizontal drag moves
    the bar; with BEND armed, a vertical drag bends (microtonal-bends.md 0.2).
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../AnimationPolicy.h"
#include <array>

namespace luthier
{

class LuthierAudioProcessor;
class FretboardComponent;

class TechniqueOverlay : public juce::Component,
                         private juce::Timer
{
public:
    TechniqueOverlay (LuthierAudioProcessor& processor, FretboardComponent& board);
    ~TechniqueOverlay() override;

    enum Layer { muteZone = 0, scrapeTrail, tapMarkers, bendArc, slapFlash, numLayers };
    static const char* getLayerName (Layer layer) noexcept;
    static juce::String getPreferenceKey (Layer layer);
    static bool isLayerEnabled (Layer layer);
    static void setLayerEnabled (Layer layer, bool enabled);

    void paint (juce::Graphics&) override;

    /** For the tests and the budget check: what the last paint drew, and how long it took. */
    int getLastDrawnCount (Layer layer) const noexcept { return drawn[(size_t) layer]; }
    double getLastPaintMs() const noexcept { return lastPaintMs; }

    /** Pulls the live state now (the timer does it at 30 Hz). */
    void refresh();

    //==========================================================================
    // Input from the fretboard. Each returns true if the technique took it.
    static bool handleMouseDown (LuthierAudioProcessor& p, int string, double continuousFret, int numFrets);
    static void handleMouseDrag (LuthierAudioProcessor& p, double continuousFret, float dragPixelsUp, int numFrets);
    static void handleMouseUp (LuthierAudioProcessor& p);

private:
    void timerCallback() override { refresh(); }

    LuthierAudioProcessor& processor;
    FretboardComponent& board;

    struct TrailPoint { float fret = -1.0f; juce::uint32 at = 0; };
    static constexpr int kTrail = 24;
    std::array<std::array<TrailPoint, kTrail>, 12> trails {};
    std::array<int, 12> trailHead {};

    juce::uint32 lastSlapCount = 0, slapFlashAt = 0;
    std::array<int, numLayers> drawn {};
    double lastPaintMs = 0.0;
    bool anythingLive = false;
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "TechniqueOverlay" };   // cpu-quality-modes 6 (CQ-22)
};

} // namespace luthier
