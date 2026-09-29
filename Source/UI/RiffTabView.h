#pragma once

/*  The riff preview's tab (riff-library.md 7.2, 10).

    Draws the compiled placement - the strings and frets that will sound on
    the loaded guitar - with technique marks in the notation-export ASCII
    style (b bend, r release, pb prebend, / \ slides, h p, ~ vibrato, PM,
    <> natural, * pinch, x dead, T tap, tr trill). The playhead is the
    player's atomic beat position, drained at 30 Hz by the browser; it hides
    after 250 ms without an update (gui-engine-dataflow's stale rule).

    Its accessible description is a text rendering of the notes, two bars at a
    time ("Bar 1: string 1 fret 8 bend whole step, ..."); Ctrl+Down reads the
    next bars and Ctrl+Up the previous.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Riffs/RiffCompiler.h"

namespace luthier
{

class RiffTabView : public juce::Component
{
public:
    RiffTabView();

    /** The compiled riff to draw; nullptr for none. */
    void setCompiled (std::shared_ptr<const CompiledRiff> compiled);
    std::shared_ptr<const CompiledRiff> getCompiled() const { return compiled; }

    /** The playhead in riff beats (< 0 hides it); `stamp` changes with every
        audio-thread update, so a stalled player is noticed. */
    void setPlayhead (double beat, juce::uint32 stamp);
    bool isPlayheadShowing() const noexcept { return playheadShowing; }

    /** The text rendering of bars [firstBar, firstBar + 2). */
    juce::String describeBars (int firstBar, int count = 2) const;
    int getAnnouncedBar() const noexcept { return announcedBar; }

    /** The mark a note's techniques draw as ("8b10", "7/9", "x"). */
    static juce::String markFor (const ScoreNote& note);

    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    void updateDescription();

    std::shared_ptr<const CompiledRiff> compiled;
    double playhead = -1.0;
    juce::uint32 lastStamp = 0;
    double lastStampChangeMs = 0.0;
    bool playheadShowing = false;
    int announcedBar = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiffTabView)
};

} // namespace luthier
