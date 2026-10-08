/*  The processor's tune-builder.md 14 hooks (TUNE-HELP-ONBOARDING): the two
    tune parameters as the mod matrix and host automation leave them. Kept out
    of PluginProcessor.cpp so the hub file carries only the calls.
*/

#include "PluginProcessor.h"

namespace luthier
{

float LuthierAudioProcessor::tuneModValue (const char* parameterId) const noexcept
{
    auto& b = const_cast<ParameterBridge&> (bridge);
    const float base = b.baseValue (parameterId);
    const auto* matrix = b.getModMatrix();

    if (matrix == nullptr || ! matrix->isActive())
        return base;

    return matrix->apply (b.parameterIndex (parameterId), base);
}

} // namespace luthier

namespace luthier
{

juce::var LuthierAudioProcessor::captureTuneSnapshotState() const
{
    const auto& tune = tuneSession.getTune();
    const auto spans = tune.getPlayOrder();
    const int span = tunePlayer.getPlayingSpan();

    // The section playing, else the one being edited.
    const int section = tunePlayer.isPlaying() && juce::isPositiveAndBelow (span, (int) spans.size())
                          ? spans[(size_t) span].sectionIndex
                          : tuneSession.getSelectedSection();

    if (! tune.isValidSection (section))
        return {};

    auto* object = new juce::DynamicObject();
    object->setProperty ("section", section);
    object->setProperty ("name", tune.arrangement.sections[(size_t) section].name);
    return juce::var (object);
}

void LuthierAudioProcessor::requestTuneSnapshotState (const juce::var& state) noexcept
{
    // Possibly the audio thread (a program change): only an int changes hands.
    if (state.isObject())
        pendingTuneSection.store ((int) state.getProperty ("section", -1), std::memory_order_release);
}

void LuthierAudioProcessor::applyPendingTuneSection()
{
    const int requested = pendingTuneSection.exchange (-1, std::memory_order_acq_rel);
    const auto& tune = tuneSession.getTune();

    if (! tune.isValidSection (requested))
        return;

    tuneSession.setSelectedSection (requested);

    // Playing: go there now, from the section's start.
    if (tunePlayer.isPlaying())
    {
        const auto spans = tune.getPlayOrder();

        for (size_t i = 0; i < spans.size(); ++i)
            if (spans[i].sectionIndex == requested)
            {
                tunePlayer.playFromSection ((int) i);
                break;
            }
    }
}

} // namespace luthier
