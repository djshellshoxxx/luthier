// OWNER: WP1 (frozen interface I-3; deleted after the prototype tests are migrated)
#pragma once

// The prototype's fixed model enum, kept only so the legacy Board shims
// (setModel/getModel/setKnob...) and the prototype drawer and tests keep
// compiling. New code addresses models by PedalDesc::modelId through
// PedalRegistry. The three prototype models are registered by WP2 as
// "prototype.ts808", "prototype.ds1" and "prototype.phase90".

namespace luthier::reference
{

enum class Model : int { empty = 0, ts808Prototype, ds1Prototype, phase90Prototype };

constexpr int kLegacyModelCount = 4;

inline const char* legacyModelName (Model model) noexcept
{
    switch (model)
    {
        case Model::ts808Prototype: return "TS808-inspired (prototype)";
        case Model::ds1Prototype: return "DS-1-inspired (prototype)";
        case Model::phase90Prototype: return "Phase 90-inspired (prototype)";
        default: return "Empty";
    }
}

// Registry model id for a legacy enum value; nullptr for Model::empty.
inline const char* legacyModelId (Model model) noexcept
{
    switch (model)
    {
        case Model::ts808Prototype: return "prototype.ts808";
        case Model::ds1Prototype: return "prototype.ds1";
        case Model::phase90Prototype: return "prototype.phase90";
        default: return nullptr;
    }
}

} // namespace luthier::reference
