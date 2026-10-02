#pragma once

#include "NotationExport.h"

namespace luthier
{
// FEAT2-TAB: offline layout shared by file export, preview and the live TAB view.
// Reads the existing score; never guesses string/fret assignments from pitch.
class AsciiTabWriter
{
public:
    static juce::String render (const PerformanceScore&, const NotationExportOptions&);
    static juce::String renderWindow (const ScoreTrack&, int firstMeasure, int numMeasures,
                                      const NotationExportOptions&);
};
}
