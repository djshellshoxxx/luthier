// OWNER: WP18
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../ReferencePedals/BoardTypes.h"
namespace luthier::reference
{
struct BoardSurfacePainter
{
    static void paint (juce::Graphics&, juce::Rectangle<float>, BoardStyle);
    static Mounting defaultMount (BoardStyle);
    static void paintMount (juce::Graphics&, juce::Rectangle<float>, Mounting, int seed);
};
}
