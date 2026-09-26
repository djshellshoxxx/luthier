#pragma once

/*  The JAM tab's read-only lanes (jam-mode.md 8.1, 8.3, 12).

    A chord track (2 bars back, this bar, 2 ahead; predicted and tune chords
    marked), the seven drum lanes and the bass lane of the current bar, with
    velocity drawn as opacity and a glyph per lane so colour is never the only
    cue. The playhead hides when the status is stale (250 ms) and steps per
    beat under reduced motion. Its accessible description reads the bar. */

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Jam/JamStatus.h"

namespace luthier
{

class JamLaneView : public juce::Component
{
public:
    JamLaneView();

    /** The panel's 30 Hz drain calls this; `fresh` is false once 250 ms pass without an update. */
    void setStatus (const JamStatus& status, bool fresh);

    void paint (juce::Graphics&) override;

    static constexpr int kPreferredHeight = 100;   ///< 8.1: the lanes keep 96 px at 480 px wide
    static constexpr int kNameWidth = 56;

    /** Where the playhead is drawn, in steps (-1 = hidden). For tests. */
    int getPlayheadStep() const noexcept { return playheadStep; }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    JamStatus status;
    bool fresh = false, haveStatus = false;
    int playheadStep = -1;
    juce::String description;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JamLaneView)
};

} // namespace luthier
