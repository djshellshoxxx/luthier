#include "CaptureRanges.h"
#include "../PluginProcessor.h"

namespace luthier
{

void CaptureRanges::addItems (juce::ComboBox& box)
{
    box.addItem ("Entire capture", entire);
    box.addItem ("Last N seconds", lastSeconds);
    box.addItem ("Marked region", markedRegion);
    box.addItem ("Current section (tune)", currentSection);
}

juce::Range<double> CaptureRanges::currentSectionPpq (LuthierAudioProcessor& processor)
{
    const auto& session = processor.getTuneSession();
    const auto& tune = session.getTune();
    const int selected = session.getSelectedSection();

    for (const auto& span : tune.getPlayOrder())
        if (span.sectionIndex == selected)
            return { span.startBeat, span.startBeat + span.lengthBeats };

    return {};
}

void CaptureRanges::apply (LuthierAudioProcessor& processor, int id, double seconds, CaptureScoreOptions& options)
{
    options.lastSeconds = id == lastSeconds ? seconds : 0.0;
    options.sampleRange = id == markedRegion ? processor.getPerformanceCapture().getMarkedRegion() : juce::Range<juce::int64>();
    options.ppqRange = id == currentSection ? currentSectionPpq (processor) : juce::Range<double>();

    // A choice with nothing to cover selects nothing, rather than everything.
    if ((id == markedRegion && options.sampleRange.isEmpty()) || (id == currentSection && options.ppqRange.isEmpty()))
        options.sampleRange = { -2, -1 };
}

juce::Range<juce::int64> CaptureRanges::midiCaptureRange (LuthierAudioProcessor& processor, const MidiPerformance& performance,
                                                          int id, double seconds)
{
    switch (id)
    {
        case lastSeconds:
            return seconds > 0.0 ? performance.getLastSecondsRange (seconds) : juce::Range<juce::int64>();

        case markedRegion:
        case currentSection:
        {
            auto& take = processor.getPerformanceCapture();
            processor.drainPerformanceCapture();

            const auto absolute = id == markedRegion ? take.getMarkedRegion()
                                                     : take.sampleRangeForPpq (currentSectionPpq (processor));
            const auto first = processor.getMidiCapture().getFirstHeldSample();

            if (absolute.isEmpty() || first < 0)
                return { 0, 1 };   // nothing: one silent sample, not "all of it" (empty means all)

            // The performance counts from the MIDI capture's first held event.
            const auto start = juce::jmax ((juce::int64) 0, absolute.getStart() - first);
            const auto end = juce::jmax (start, absolute.getEnd() - first);
            return { start, juce::jmax (start + 1, end) };
        }

        case entire:
        default:
            return {};
    }
}

bool CaptureRanges::isAvailable (LuthierAudioProcessor& processor, int id)
{
    if (id == markedRegion)
        return processor.getPerformanceCapture().hasMarkedRegion();

    if (id == currentSection)
        return ! currentSectionPpq (processor).isEmpty();

    return true;
}

} // namespace luthier
