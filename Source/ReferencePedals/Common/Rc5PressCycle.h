// OWNER: WP16
#pragma once
#include "LooperCore.h"
#include <optional>

namespace luthier::reference::common
{
// Audio-owner, untimed RC-5 down-edge cycle only (spec §2.1/2.2).
// The caller applies the returned action at the current sample boundary.
// Full double/hold handling must intercept provisional actions; this helper
// alone is not the complete main pedal gesture decoder.
class Rc5PressCycle
{
public:
    std::optional<LooperAction> down(LooperCore::State state) noexcept
    {
        if(held)return std::nullopt;
        held=true;
        switch(state)
        {
            case LooperCore::State::Empty:return LooperAction::Record;
            case LooperCore::State::Recording:return LooperAction::Play;
            case LooperCore::State::Playing:return LooperAction::Overdub;
            case LooperCore::State::Overdubbing:return LooperAction::Play;
            case LooperCore::State::Stopped:return LooperAction::Play;
        }
        return std::nullopt;
    }
    void up() noexcept {held=false;}
    void reset() noexcept {held=false;} // bookkeeping only; no transport/media access
private:
    bool held=false;
};
}
