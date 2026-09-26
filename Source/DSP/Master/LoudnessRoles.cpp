#include "LoudnessRoles.h"

namespace luthier
{

namespace
{
    // output-normalization.md 3.1, Performance: playing gestures.
    const char* const kPerformanceIds[] =
    {
        "guitar_volume", "whammy_position", "playing_mode", "mpe_enabled", "bend_range",
        "strum_speed", "strum_direction", "chord_window", "legato_window", "transpose_lock",
        // macro_humanize is Config here, not Performance as 3.1 lists it: it is
        // a preset's setting that no controller moves, and its velocity jitter
        // shifts loudness by about 1 LU (0.4 vs 0.7), so rendering it at its
        // default missed ON-03's +/-1 LU target (docs/coverage/FEAT-NORMALIZE.md).
        "preset_morph_position",
        // Merge (SPEC-SWEEP LP-16): the snapshot morph is a performance control
        // like the preset morph; it is never stored in a sound.
        "snapshot_morph",

        // strum-dynamics: strum_crossing_sps ... chuck_damping.
        "strum_crossing_sps", "strum_acceleration", "strum_up_velocity_ratio", "strum_tilt",
        "strum_miss_probability", "strum_striker_down", "strum_striker_up",
        "chuck_amount", "chuck_damping",

        "finger_alternation_variation", "rest_stroke"
    };

    const char* const kPerformancePrefixes[] =
    {
        "vibrato_", "freeze_", "ebow_", "feedback_", "hum_",
        "scrape_", "slap_", "pop_", "ghost_", "double_thump_"
    };

    // 3.1, Mix: forced neutral in the reference render.
    const char* const kMixIds[] =
    {
        "master_gain", "limiter_on", "aux1_pre_circuit"
    };

    bool inPerformance (const juce::String& id)
    {
        for (auto* p : kPerformanceIds)
            if (id == p)
                return true;

        for (auto* p : kPerformancePrefixes)
            if (id.startsWith (p))
                return true;

        return false;
    }

    // 10: jam-mode's band is mixed after the master bus and is never in the
    // reference render, so none of its controls is part of the sound measured.
    const char* const kMixPrefixes[] =
    {
        "jam_"
    };

    bool inMix (const juce::String& id)
    {
        for (auto* p : kMixIds)
            if (id == p)
                return true;

        for (auto* p : kMixPrefixes)
            if (id.startsWith (p))
                return true;

        return false;
    }
}

LoudnessRole LoudnessRoles::roleFor (const juce::String& paramId)
{
    if (inMix (paramId))
        return LoudnessRole::mix;

    if (inPerformance (paramId))
        return LoudnessRole::performance;

    return LoudnessRole::config;
}

bool LoudnessRoles::isListed (const juce::String& paramId)
{
    return inMix (paramId) || inPerformance (paramId);
}

juce::StringArray LoudnessRoles::findConflicts (const juce::StringArray& paramIds)
{
    juce::StringArray conflicts;

    for (const auto& id : paramIds)
        if (inMix (id) && inPerformance (id))
            conflicts.add (id);

    return conflicts;
}

const std::vector<LoudnessRoles::Expectation>& LoudnessRoles::specExpectations()
{
    static const std::vector<Expectation> e
    {
        { "guitar_volume", LoudnessRole::performance },
        { "whammy_position", LoudnessRole::performance },
        { "playing_mode", LoudnessRole::performance },
        { "mpe_enabled", LoudnessRole::performance },
        { "bend_range", LoudnessRole::performance },
        { "strum_speed", LoudnessRole::performance },
        { "strum_direction", LoudnessRole::performance },
        { "chord_window", LoudnessRole::performance },
        { "vibrato_depth", LoudnessRole::performance },
        { "legato_window", LoudnessRole::performance },
        { "transpose_lock", LoudnessRole::performance },
        { "freeze_enable", LoudnessRole::performance },
        { "ebow_enable", LoudnessRole::performance },
        { "feedback_on", LoudnessRole::performance },
        { "feedback_amount", LoudnessRole::performance },
        { "hum_timing", LoudnessRole::performance },
        { "macro_humanize", LoudnessRole::config },   // deviation, see kPerformanceIds
        { "preset_morph_position", LoudnessRole::performance },
        { "scrape_pressure", LoudnessRole::performance },
        { "slap_strength", LoudnessRole::performance },
        { "pop_strength", LoudnessRole::performance },
        { "ghost_level", LoudnessRole::performance },
        { "double_thump_enabled", LoudnessRole::performance },
        { "strum_crossing_sps", LoudnessRole::performance },
        { "chuck_damping", LoudnessRole::performance },
        { "finger_alternation_variation", LoudnessRole::performance },
        { "rest_stroke", LoudnessRole::performance },

        { "master_gain", LoudnessRole::mix },
        { "limiter_on", LoudnessRole::mix },
        { "aux1_pre_circuit", LoudnessRole::mix },
        { "jam_volume", LoudnessRole::mix },

        // 3.1: the pickup selector, per-pickup volumes and the tone knob are
        // Config; so are the amp, the tone strip and oversampling.
        { "guitar_type", LoudnessRole::config },
        { "amp_model", LoudnessRole::config },
        { "amp_gain", LoudnessRole::config },
        { "amp_master", LoudnessRole::config },
        { "output_mix", LoudnessRole::config },
        { "stereo_width", LoudnessRole::config },
        { "input_gain", LoudnessRole::config },
        { "oversampling", LoudnessRole::config },
        { "macro_drive", LoudnessRole::config }
    };

    return e;
}

bool LoudnessRoles::isPerformanceSlotParameter (int, int) noexcept
{
    return false;
}

const char* LoudnessRoles::roleName (LoudnessRole role) noexcept
{
    switch (role)
    {
        case LoudnessRole::config:      return "Config";
        case LoudnessRole::performance: return "Performance";
        case LoudnessRole::mix:         return "Mix";
    }

    return "?";
}

} // namespace luthier
