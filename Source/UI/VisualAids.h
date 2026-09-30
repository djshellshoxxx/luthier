#pragma once

/*  piano-roll-chord-display.md 5: the "Visual aids" switches in Options ->
    General, beside "Show tooltips". User preferences (UiPreferences), saved at
    once; not preset data and not parameters - they change what is shown, not
    the sound. Every reader polls these, so a change takes effect everywhere
    without anyone being told.
*/

#include <juce_core/juce_core.h>

namespace luthier::VisualAids
{
    /** Show chord names on the guitar (default on). */
    bool showChordNames();
    void setShowChordNames (bool on);

    /** Announce chord names to screen readers (default off; only while the
        names are shown). */
    bool announceChordNames();          ///< the stored switch AND the names being on
    bool announceChordNamesSetting();   ///< the stored switch alone
    void setAnnounceChordNames (bool on);

    /** Show the piano roll: one switch per mode (default on in Advanced, off in Easy). */
    bool showPianoRoll (bool advancedMode);
    void setShowPianoRoll (bool advancedMode, bool on);

    /** The roll strip shows the keys only, or keys and the scrolling roll (default). */
    bool pianoRollShowsRoll();
    void setPianoRollShowsRoll (bool withRoll);
}
