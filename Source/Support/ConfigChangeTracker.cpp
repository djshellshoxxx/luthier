#include "ConfigChangeTracker.h"

namespace luthier
{

ConfigChangeTracker::ConfigChangeTracker (juce::AudioProcessor& p)
    : processor (p)
{
    const auto& all = processor.getParameters();
    const auto n = (size_t) all.size();

    params.reserve (n);
    role.assign (n, (std::uint8_t) LoudnessRole::config);
    immediateEvent.assign (n, 0);
    lastSeen.assign (n, 0.0f);
    performanceWrite.reset (new std::atomic<std::uint8_t>[n]);

    for (size_t i = 0; i < n; ++i)
    {
        auto* prm = all[(int) i];
        params.push_back (prm);
        performanceWrite[i].store (0, std::memory_order_relaxed);
        lastSeen[i] = prm->getValue();

        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (prm))
        {
            const auto& id = withId->paramID;
            role[i] = (std::uint8_t) LoudnessRoles::roleFor (id);

            // 2.3.1: these Config parameters are discrete configuration events.
            if (id == "guitar_type" || id == "oversampling"
                 || (id.startsWith ("pre_slot") && id.endsWith ("_type"))
                 || (id.startsWith ("post_slot") && id.endsWith ("_type")))
                immediateEvent[i] = 1;
        }

        prm->addListener (this);
    }
}

ConfigChangeTracker::~ConfigChangeTracker()
{
    for (auto* prm : params)
        prm->removeListener (this);
}

LoudnessRole ConfigChangeTracker::getRole (int parameterIndex) const noexcept
{
    return juce::isPositiveAndBelow (parameterIndex, (int) role.size()) ? (LoudnessRole) role[(size_t) parameterIndex]
                                                                          : LoudnessRole::config;
}

void ConfigChangeTracker::parameterValueChanged (int parameterIndex, float)
{
    if (juce::isPositiveAndBelow (parameterIndex, (int) params.size()))
        performanceWrite[(size_t) parameterIndex].store (PerformanceWriteScope::isActive() ? 1 : 0,
                                                         std::memory_order_release);
}

void ConfigChangeTracker::markConfigDirty (bool immediate) noexcept
{
    if (immediate)
        immediatePending.store (true, std::memory_order_release);
}

bool ConfigChangeTracker::process (std::int64_t timelineStart, int numSamples, double sampleRate,
                                   std::int64_t& fireSample) noexcept
{
    const bool on = tracking.load (std::memory_order_acquire);

    // Off: nothing at all per block (13: unchanged while inactive). Turning on
    // resyncs the view first, so a change made while off is not reported.
    if (! on)
    {
        needResync = true;
        dirty = false;
        return false;
    }

    bool immediate = immediatePending.exchange (false, std::memory_order_acq_rel);
    bool changed = false;

    for (size_t i = 0; i < params.size(); ++i)
    {
        const float v = params[i]->getValue();

        if (v == lastSeen[i])
            continue;

        lastSeen[i] = v;

        if (needResync || role[i] != (std::uint8_t) LoudnessRole::config)
            continue;

        if (performanceWrite[i].load (std::memory_order_acquire) != 0)
            continue;

        if (immediateEvent[i] != 0)
            immediate = true;
        else
            changed = true;
    }

    needResync = false;

    if (changed)
    {
        dirty = true;
        dirtyAt = timelineStart;
    }

    const auto debounce = (std::int64_t) std::llround (kDebounceSeconds * (sampleRate > 0.0 ? sampleRate : 48000.0));
    std::int64_t at = -1;

    if (immediate)
        at = timelineStart;
    else if (dirty && dirtyAt + debounce < timelineStart + numSamples)
        at = juce::jmax (timelineStart, dirtyAt + debounce);

    if (at < 0)
        return false;

    dirty = false;
    fireSample = at;
    requestTimeline.store (at, std::memory_order_release);
    requestSerial.fetch_add (1, std::memory_order_acq_rel);
    return true;
}

} // namespace luthier
