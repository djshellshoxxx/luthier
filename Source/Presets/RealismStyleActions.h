#pragma once

/*  Picking a noise-floor or sustain style (noise-floor.md 3, sustain-and-decay.md
    6.1): the row's values written as one undoable action, and the style box's
    text, which reads "(modified)" once any value moves, as squeak_style does.
    Message thread. */

#include <juce_core/juce_core.h>

namespace luthier
{

class LuthierAudioProcessor;

void applyNoiseFloorStyle (LuthierAudioProcessor& processor, int index);
juce::String describeNoiseFloorStyle (LuthierAudioProcessor& processor);

void applySustainStyle (LuthierAudioProcessor& processor, int index);
juce::String describeSustainStyle (LuthierAudioProcessor& processor);

} // namespace luthier
