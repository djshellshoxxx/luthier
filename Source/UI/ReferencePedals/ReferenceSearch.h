// OWNER: WP17 / global-search integration
#pragma once
#include "../../ReferencePedals/BoardModel.h"

namespace luthier::reference
{
struct ReferenceSearchTarget
{
    enum class Surface { toolbar, settings, pedal } surface = Surface::toolbar;
    bool recognized = false, available = false;
    int slot = -1, param = -1;
    InstanceId instance = InstanceId::null();
    juce::String title, breadcrumb, reason;
};
// Message thread only. Fixed host slots resolve through instance.slot, never order.
ReferenceSearchTarget resolveSearchTarget(const BoardModel&, const juce::String& hostId);
juce::uint32 searchModelGeneration(const BoardModel&);
}
