// OWNER: WP16
#pragma once
#include "LooperCore.h"
#include <optional>
#include <cmath>

namespace luthier::reference::common
{
// Audio-owner RC-5 down-edge cycle and optional timed double-press selection.
// The caller applies the returned action at the current sample boundary.
// Full hold/clear handling must intercept provisional actions; this helper
// alone is not the complete main pedal gesture decoder.
class Rc5PressCycle
{
public:
    struct Actions
    {
        std::array<LooperAction,2> actions{};
        int count=0;
    };
    std::optional<LooperAction> down(LooperCore::State state) noexcept
    {
        invalidateCandidate();
        return edge(state);
    }
    // Timed main-press prerequisite only. Caller applies the batch at sample
    // with no intervening audio, and invalidates before external transport,
    // history or media actions. State alone cannot prove provisional ownership.
    Actions down(LooperCore::State state,int64_t sample,double sampleRate,double doubleMs) noexcept
    {
        if(sample<0 || sample<lastSample || !std::isfinite(sampleRate) || sampleRate<1 || sampleRate>192000
            || !std::isfinite(doubleMs) || doubleMs<150 || doubleMs>600)
        {invalidateCandidate();return {};}
        lastSample=sample;
        if(armed && (sampleRate!=pairRate || doubleMs!=pairMs))invalidateCandidate();
        const auto action=edge(state);
        if(!action)return {};
        Actions result;
        if(armed && sample-firstDown<=window
            && (state==LooperCore::State::Playing || state==LooperCore::State::Overdubbing))
        {
            if(provisional && state==LooperCore::State::Overdubbing)
                result.actions[size_t(result.count++)]=LooperAction::CancelOverdub;
            result.actions[size_t(result.count++)]=LooperAction::Stop;
            invalidateCandidate();
            return result;
        }
        invalidateCandidate();
        result.actions[0]=*action;result.count=1;
        if(state==LooperCore::State::Playing || state==LooperCore::State::Overdubbing)
        {
            armed=true;provisional=state==LooperCore::State::Playing;firstDown=sample;
            window=int64_t(std::floor(sampleRate*doubleMs/1000.));pairRate=sampleRate;pairMs=doubleMs;
        }
        return result;
    }
    void up() noexcept {held=false;}
    void invalidateCandidate() noexcept
    {armed=provisional=false;firstDown=window=0;pairRate=pairMs=0;}
    void reset() noexcept {held=false;lastSample=-1;invalidateCandidate();}
private:
    std::optional<LooperAction> edge(LooperCore::State state) noexcept
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
    bool held=false,armed=false,provisional=false;
    int64_t firstDown=0,window=0,lastSample=-1;
    double pairRate=0,pairMs=0;
};
}
