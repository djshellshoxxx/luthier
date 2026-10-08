#pragma once

/*  Options -> APPEARANCE -> VISUAL AIDS (animated-strings.md 5,
    piano-roll-chord-display.md 5).

    A section of its own so both specs' rows share one heading without either
    editing the other's layout: it starts directly under the row holding "Show
    tooltips on hover" and "Reduced motion", and the piano roll's rows (chord
    names, announce, piano roll, keys / roll) go below the string-animation rows
    in this component.

        VISUAL AIDS
        [ ] Animate strings            Quality [ High v ]
            Strings vibrate on the guitar and fretboard while they sound.
            Display only: no effect on the sound.
            Paused while Reduced motion is on.        <- only when it applies

    Every value is a UiPreferences key (ui.json), saved on change, never in a
    preset or the host session. Nothing here is ever greyed out (gui-integration
    0.7): the quality can be set with the toggle off.
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{

class VisualAidsSection : public juce::Component
{
public:
    VisualAidsSection();

    /** Re-reads the preferences and Reduced motion (the page's refresh). */
    void refresh();

    /** The height the section wants at its current state. */
    int getPreferredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** The accessible description: the help text, plus the paused note when it applies (8). */
    juce::String describe() const;

    // animated-strings.md 5's controls, public for tests (AS-25).
    juce::ToggleButton animateStringsToggle;
    juce::ComboBox animateQualityBox;
    juce::Label qualityLabel, animateHelpLabel, animateStatusLabel;

    static constexpr int kHeadingHeight = 18;
    static constexpr int kRowHeight = 26;

private:
    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VisualAidsSection)
};

} // namespace luthier
