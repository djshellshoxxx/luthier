// OWNER: WP1 (frozen interface I-2; changes only via errata request to the WP1 owner)
#pragma once

// Explicit, linker-safe registry of every reference pedal model (errata E14):
// no static registrars. registerAll() calls one function per package (see
// Registration.h); each package owns its Register*.cpp. Message thread only.

#include <memory>
#include <juce_core/juce_core.h>
#include "IPedal.h"

namespace luthier::reference
{

// Host-automation lane assignment for one model (BOARD_CORE 9.1 / 9.2, E2):
// walk PedalDesc::params in order, skip Engage / Config / Ext / Trigger /
// Momentary params, hand the rest to lanes p01.. until kMaxLaneParams. Params
// beyond that stay state-only (still MIDI-learnable). A model revision may only
// APPEND params, so saved automation stays valid.
struct LaneMap
{
    int numLanes = 0;                                // lanes in use, <= kMaxLaneParams
    int laneToParam[kMaxLaneParams];                 // lane k -> descriptor index, -1 unused
    int paramToLane[kMaxParams];                     // descriptor index -> lane, -1 none
    int engageParam = -1;                            // descriptor index aliased to slot "enabled", -1 none
    int numOverflow = 0;                             // automatable params that did not fit a lane

    LaneMap() noexcept
    {
        for (auto& v : laneToParam) v = -1;
        for (auto& v : paramToLane) v = -1;
    }
};

class PedalRegistry
{
public:
    // Explicit list, idempotent. Calls the register*() function of every package.
    static void registerAll();

    // Validates and stores a copy of the descriptor. Returns false (and leaves
    // the registry unchanged) when validate() fails or the id/prefix is a
    // duplicate. Debug builds assert. Errata E25: returns bool, was void.
    static bool add (const PedalDesc&, PedalFactory, PreparedMemoryEstimator = nullptr);
    // Off audio; no factory invocation. Missing callback returns unsupported.
    static PreparedMemoryRequirement memoryRequirement (const char* modelId, double sampleRate,
        int maxBlock, int oversamplePolicy, const float* normalisedParams);

    // Message thread only. nullptr for an unknown model id or a null factory result.
    static std::unique_ptr<IPedal> make (const char* modelId);
    static const PedalDesc* find (const char* modelId);

    // Order: generic (GENERIC_PEDALS order), then models (catalog order). The
    // view is invalidated by the next add(); do not hold it across one.
    static juce::Span<const PedalDesc* const> all();

    // Never null: unknown model ids yield a shared empty map.
    static const LaneMap& laneMap (const char* modelId);

    // Pure descriptor rules (T-REG). On failure fills `reason` when not null.
    //   modelId and displayName non-empty, revision >= 1
    //   paramPrefix non-empty, ends with '_', only [a-z0-9_]
    //   0 <= numParams <= kMaxParams, params non-null when numParams > 0
    //   every param: id starts with paramPrefix, id snake_case ([a-z0-9_]) and
    //     unique within the pedal, label non-null, min < max for continuous
    //     kinds, def within [min,max], Enum/Discrete have steps >= 2 and choices
    //     for Enum, Toggle/Momentary/Trigger are 0..1
    //   at most one PF_Engage param, and it is a Toggle
    //   maxLatencySamples within 0..kMaxLatencySamp
    static bool validate (const PedalDesc&, juce::String* reason = nullptr);

    // Builds the lane map of a descriptor without touching the registry.
    static LaneMap buildLaneMap (const PedalDesc&);

    // Index of the param with this exact id, or -1.
    static int findParam (const PedalDesc&, const char* paramId);

    // Test support: swaps in an empty registry for the scope and restores the
    // previous content (and the registerAll() flag) on destruction.
    class TestScope
    {
    public:
        TestScope();
        ~TestScope();
        TestScope (const TestScope&) = delete;
        TestScope& operator= (const TestScope&) = delete;
    private:
        struct Saved;
        std::unique_ptr<Saved> saved;
    };
};

} // namespace luthier::reference
