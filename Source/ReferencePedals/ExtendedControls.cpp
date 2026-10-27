// OWNER: WP2
#include "ExtendedControls.h"
namespace luthier::reference
{
ExtendedControls compileExtendedControls (const BoardSettings& settings, const std::vector<PedalInstance>& instances,
    const ExtendedControls* previous, const BoardCommand* intent, bool hostEcho, uint32_t version, AbSelect resolved)
{
    ExtendedControls result;
    const auto put = [&] (ExtendedHostField field, auto value) { result.values[static_cast<size_t> (field)] = static_cast<float> (value); };
    const auto slot = [&] (const InstanceId& id) { for (const auto& instance : instances) if (instance.instanceId == id) return instance.slot + 1; return 0; };
    put (ExtendedHostField::MixLaw, settings.mixLaw); put (ExtendedHostField::ReportLatency, settings.reportLatency);
    put (ExtendedHostField::LimiterOn, settings.limiterOn); put (ExtendedHostField::LimiterCeilDb, settings.limiterCeilDb);
    put (ExtendedHostField::Trails, settings.trails); put (ExtendedHostField::MixTail, settings.mixTail); put (ExtendedHostField::Mode, settings.mode);
    put (ExtendedHostField::FeedbackOn, settings.fb.on); put (ExtendedHostField::FeedbackGain, settings.fb.gain); put (ExtendedHostField::FeedbackLpHz, settings.fb.lpHz);
    put (ExtendedHostField::FeedbackSrc, slot (settings.fb.src)); put (ExtendedHostField::FeedbackDst, slot (settings.fb.dst)); put (ExtendedHostField::FeedbackRunawayOk, settings.fb.runawayOk);
    put (ExtendedHostField::AbSelect, resolved); put (ExtendedHostField::AbOut, settings.abOut);
    put (ExtendedHostField::ReampEnable, settings.reampEnable); put (ExtendedHostField::ReampReplace, settings.reampReplace); put (ExtendedHostField::ReampGainDb, settings.reampGainDb);
    put (ExtendedHostField::CpuCapPct, settings.cpuCapPct); put (ExtendedHostField::MidiChan, settings.midiChan); put (ExtendedHostField::TempoSource, settings.tempoSource);
    put (ExtendedHostField::NumLanes, settings.lanes.numLanes); put (ExtendedHostField::ParallelNorm, settings.lanes.norm); put (ExtendedHostField::FeedbackKill, settings.fb.kill);
    for (int lane = 0; lane < kMaxBranches; ++lane)
    {
        result.values[static_cast<size_t> (25 + lane * 2)] = settings.lanes.gainDb[static_cast<size_t> (lane)];
        result.values[static_cast<size_t> (26 + lane * 2)] = settings.lanes.phaseInvert[static_cast<size_t> (lane)] ? 1.f : 0.f;
    }
    for (int loop = 0; loop < kMaxLoops; ++loop)
    {
        const auto& source = settings.loops[static_cast<size_t> (loop)]; const int base = 33 + loop * 8;
        result.values[base] = source.used ? 1.f : 0.f; result.values[base + 1] = source.enabled ? 1.f : 0.f;
        result.values[base + 2] = static_cast<float> (source.kind); result.values[base + 3] = source.blend;
        result.values[base + 4] = source.sendDb; result.values[base + 5] = source.returnDb;
        result.values[base + 6] = static_cast<float> (slot (source.first)); result.values[base + 7] = static_cast<float> (slot (source.last));
    }
    if (previous) result.serials = previous->serials;
    if (! previous || ! intent) result.serials.fill (version);
    else if (! hostEcho)
    {
        for (size_t i = 0; i < result.values.size(); ++i) if (result.values[i] != previous->values[i]) result.serials[i] = version;
        const auto mark = [&] (ExtendedHostField field) { result.serials[static_cast<size_t> (field)] = version; };
        if (std::holds_alternative<cmd::SetMixLaw> (*intent)) mark (ExtendedHostField::MixLaw);
        if (std::holds_alternative<cmd::SetFeedback> (*intent))
            for (const auto field : { ExtendedHostField::FeedbackOn, ExtendedHostField::FeedbackGain, ExtendedHostField::FeedbackLpHz, ExtendedHostField::FeedbackRunawayOk, ExtendedHostField::FeedbackKill }) mark (field);
        if (const auto* loop = std::get_if<cmd::SetLoop> (intent))
            for (const int index : { 1, 3, 4, 5 }) result.serials[static_cast<size_t> (33 + loop->loop * 8 + index)] = version;
        if (const auto* scalar = std::get_if<cmd::SetBoardParam> (intent))
        {
            const char* keys[25] { "", "reportLatency", "limiterOn", "limiterCeilDb", "trails", "mixTail", "", "", "", "", "", "", "", "", "", "abSelect", "abOut", "reampEnable", "reampReplace", "reampGainDb", "cpuCapPct", "midiChan", "tempoSource", "numLanes", "parallelNorm" };
            for (int i = 0; i < 25; ++i) if (scalar->key == keys[i]) result.serials[static_cast<size_t> (i)] = version;
            for (int lane = 0; lane < kMaxBranches; ++lane)
            {
                if (scalar->key == "laneGainDb" + std::to_string (lane)) result.serials[static_cast<size_t> (25 + lane * 2)] = version;
                if (scalar->key == "lanePhase" + std::to_string (lane)) result.serials[static_cast<size_t> (26 + lane * 2)] = version;
            }
            if (scalar->key == "fbKill") mark (ExtendedHostField::FeedbackKill);
        }
    }
    return result;
}
}
