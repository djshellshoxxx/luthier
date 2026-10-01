#include "MuteEngine.h"

namespace luthier
{

void MuteEngine::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (1.0, sampleRate);
    reset();
}

void MuteEngine::reset() noexcept
{
    releaseAt.fill (-1);
    releaseT60.fill (0.0);
    playingStep.store (-1, std::memory_order_relaxed);
}

//==============================================================================
void MuteEngine::setLiveStep (int step, MuteType type) noexcept
{
    if (juce::isPositiveAndBelow (step, kLiveMuteSteps))
        liveGrid[(size_t) step].store (juce::isPositiveAndBelow ((int) type, (int) MuteType::numTypes) ? (int) type : 0,
                                       std::memory_order_relaxed);
}

MuteType MuteEngine::getLiveStep (int step) const noexcept
{
    return juce::isPositiveAndBelow (step, kLiveMuteSteps)
             ? (MuteType) liveGrid[(size_t) step].load (std::memory_order_relaxed)
             : MuteType::open;
}

void MuteEngine::clearLiveGrid() noexcept
{
    for (auto& cell : liveGrid)
        cell.store (0, std::memory_order_relaxed);
}

void MuteEngine::applyGridPreset (int presetIndex) noexcept
{
    const auto& preset = getMuteGridPreset (presetIndex);

    for (int i = 0; i < kLiveMuteSteps; ++i)
        setLiveStep (i, muteTypeFromLetter (preset.cells[i]));
}

//==============================================================================
void MuteEngine::apply (PlayEventQueue& queue, double sampleRate, double bpm, double ppq,
                        bool playing, bool fromRhythm) noexcept
{
    const int n = queue.getNumNoteOns();
    const double beatsPerSample = juce::jlimit (20.0, 300.0, bpm) / (60.0 * juce::jmax (1.0, sampleRate));

    if (! settings.armed)
        playingStep.store (-1, std::memory_order_relaxed);

    for (int i = 0; i < n; ++i)
    {
        auto& e = queue.getMutableNoteOn (i);

        // Idle: disarmed, and nothing the pattern wrote. One test.
        if (! settings.armed && e.muteType == 0)
            continue;

        MuteStep step;
        step.type = juce::isPositiveAndBelow (e.muteType, (int) MuteType::numTypes) ? (MuteType) e.muteType
                                                                                     : MuteType::open;
        step.pressure = e.mutePressure;
        step.positionMm = e.mutePositionMm;

        // The sixteenth this note lands in, from the host's own ppq (2: "syncs to host tempo").
        const double at = ppq + (double) e.sampleOffset * beatsPerSample;
        const double sixteenths = std::floor (juce::jmax (0.0, at) * 4.0 + 1.0e-6);
        const int stepIndex = (int) std::fmod (sixteenths, (double) kLiveMuteSteps);
        const auto loop = (juce::uint32) (sixteenths / (double) kLiveMuteSteps);

        if (settings.armed && ! fromRhythm && playing && step.isOpen())
        {
            step.type = getLiveStep (stepIndex);
            playingStep.store (stepIndex, std::memory_order_relaxed);
        }

        const bool isStrum = e.stepDynamic >= 0.0 || e.technique == Technique::Strum;
        const double dynamic = e.stepDynamic >= 0.0 ? e.stepDynamic : e.velocity;

        const auto r = Muting::resolve (step, settings, isStrum, dynamic, seed, loop, stepIndex);

        e.muteType = (int) r.type;
        e.mutePressure = r.pressure;
        e.mutePositionMm = r.positionMm;

        if (r.type == MuteType::ghost)
            e.velocity = juce::jlimit (0.02, 1.0, e.velocity * juce::jlimit (0.0, 1.0, settings.ghostVelocity));

        // 1: a chuka is the hand chopping across the strings: strum-dynamics 6.1's chuck.
        if (r.type == MuteType::chuka && e.chuck <= 0.0)
            e.chuck = chuckDamping;

        if (r.type != MuteType::open)
        {
            lastMuteType.store ((int) r.type, std::memory_order_relaxed);
            fireCount.fetch_add (1, std::memory_order_relaxed);
        }
    }
}

MuteDamping MuteEngine::dampingFor (const NoteOnEvent& e) const noexcept
{
    if (! juce::isPositiveAndBelow (e.muteType, (int) MuteType::numTypes) || e.muteType == 0)
        return {};

    return Muting::dampingFor ((MuteType) e.muteType, settings, e.mutePressure, e.mutePositionMm);
}

bool MuteEngine::deadensOtherStrings (const NoteOnEvent& e) const noexcept
{
    // 3 / string-interaction.md: rock spread lays the spare fingers across the
    // strings; classical fingertip keeps them off. Only a palm mute or a ghost
    // brings the hand down.
    if (settings.frettingStyle != FrettingMuteStyle::rockSpread)
        return false;

    const auto t = (MuteType) juce::jlimit (0, (int) MuteType::numTypes - 1, e.muteType);
    return isPalmMute (t) || t == MuteType::ghost;
}

//==============================================================================
void MuteEngine::noteStruck (int s, const MuteDamping& damping, juce::int64 atSample, double sampleRate) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return;

    if (damping.releaseAfterSeconds > 0.0)
    {
        releaseAt[(size_t) s] = atSample + (juce::int64) std::llround (damping.releaseAfterSeconds * sampleRate);
        releaseT60[(size_t) s] = damping.releaseT60Seconds;
    }
    else
    {
        releaseAt[(size_t) s] = -1;
    }
}

void MuteEngine::noteReleased (int s) noexcept
{
    if (juce::isPositiveAndBelow (s, kMaxStrings))
        releaseAt[(size_t) s] = -1;
}

bool MuteEngine::takeDueRelease (int s, juce::int64 upToSample, double& t60) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings) || releaseAt[(size_t) s] < 0 || releaseAt[(size_t) s] > upToSample)
        return false;

    releaseAt[(size_t) s] = -1;
    t60 = releaseT60[(size_t) s];
    return true;
}

//==============================================================================
juce::var MuteEngine::toVar() const
{
    juce::String cells;

    for (int i = 0; i < kLiveMuteSteps; ++i)
        cells << (i > 0 ? "," : "") << getMuteTypeId (getLiveStep (i));

    return cells;
}

void MuteEngine::fromVar (const juce::var& state)
{
    clearLiveGrid();

    const auto ids = juce::StringArray::fromTokens (state.toString(), ",", "");

    for (int i = 0; i < juce::jmin (kLiveMuteSteps, ids.size()); ++i)
        setLiveStep (i, muteTypeFromId (ids[i].trim()));
}

} // namespace luthier
