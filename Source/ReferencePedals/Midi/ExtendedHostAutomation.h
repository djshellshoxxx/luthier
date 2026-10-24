// OWNER: WP23
#pragma once
#include <array>
#include <cstdint>
namespace luthier::reference
{
// E57 append-only supplement to the original 323-entry HostAutomationSnapshot.
// Payloads are physical units. Zero serial means unwritten, inequality consumes
// once (including wrap); producer skips zero. Structural fields are handled by
// the message thread; their serials never imply an audio-thread topology edit.
enum class ExtendedHostField : uint8_t
{
    MixLaw, ReportLatency, LimiterOn, LimiterCeilDb, Trails, MixTail, Mode,
    Exp1, Exp2, FeedbackOn, FeedbackGain, FeedbackLpHz, FeedbackSrc, FeedbackDst,
    FeedbackRunawayOk, AbSelect, AbOut, ReampEnable, ReampReplace, ReampGainDb,
    CpuCapPct, MidiChan, TempoSource, NumLanes, ParallelNorm,
    Lane1GainDb, Lane1Phase, Lane2GainDb, Lane2Phase, Lane3GainDb, Lane3Phase, Lane4GainDb, Lane4Phase,
    Loop1Used, Loop1Enabled, Loop1Kind, Loop1Blend, Loop1SendDb, Loop1ReturnDb, Loop1First, Loop1Last,
    Loop2Used, Loop2Enabled, Loop2Kind, Loop2Blend, Loop2SendDb, Loop2ReturnDb, Loop2First, Loop2Last,
    FeedbackKill, Count
};
struct ExtendedHostParameter
{
    const char* id;
    const char* name;
    float minimum, maximum, defaultValue, interval;
    bool structural;
    const char* choices = nullptr; // pipe-delimited; nullptr => scalar/bool
};
inline constexpr std::array<ExtendedHostParameter, 50> extendedHostParameters {{
    { "ref_board_mix_law", "Mix law", 0,2,0,1,false,"Equal power|Linear|Third law" },
    { "ref_board_report_latency", "Report latency", 0,1,1,1,false },
    { "ref_board_limiter_on", "Limiter", 0,1,1,1,false },
    { "ref_board_limiter_ceil_db", "Limiter ceiling", -6,6,0,0,false },
    { "ref_board_trails", "Trails", 0,1,1,1,false },
    { "ref_board_mix_tail", "Process tails at dry mix", 0,1,1,1,false },
    { "ref_board_mode", "Routing mode", 0,3,0,1,true,"Series|Parallel|AB|Loop" },
    { "ref_board_exp1", "Expression 1", 0,1,0,0,false },
    { "ref_board_exp2", "Expression 2", 0,1,0,0,false },
    { "ref_board_fb_on", "Feedback enabled", 0,1,0,1,false },
    { "ref_board_fb_gain", "Feedback gain", 0,1.05f,.5f,0,false },
    { "ref_board_fb_lp", "Feedback low pass", 20,20000,12000,0,false },
    { "ref_board_fb_src", "Feedback source slot", 0,16,0,1,true },
    { "ref_board_fb_dst", "Feedback destination slot", 0,16,0,1,true },
    { "ref_board_fb_runaway_ok", "Allow feedback runaway", 0,1,0,1,false },
    { "ref_board_ab_select", "AB selection", 0,3,2,1,false,"A|B|A+B|Toggle" },
    { "ref_board_ab_out", "AB output", 0,1,0,1,true,"Sum|Dual" },
    { "ref_board_reamp_enable", "Reamp enabled", 0,1,0,1,false },
    { "ref_board_reamp_replace", "Reamp replaces input", 0,1,0,1,false },
    { "ref_board_reamp_gain", "Reamp gain", -120,12,0,0,false },
    { "ref_board_cpu_cap", "CPU budget", 1,100,30,1,true },
    { "ref_board_midi_chan", "MIDI channel", 0,16,0,1,true },
    { "ref_board_tempo_source", "Tempo source", 0,3,3,1,false,"Internal|Host|MIDI clock|Auto" },
    { "ref_board_num_lanes", "Parallel lanes", 2,4,2,1,true },
    { "ref_board_parallel_norm", "Parallel normalisation", 0,2,0,1,true,"Square root|Unity|None" },
    { "ref_board_lane1_gain", "Lane 1 gain", -120,6,0,0,false },
    { "ref_board_lane1_phase", "Lane 1 phase invert", 0,1,0,1,false },
    { "ref_board_lane2_gain", "Lane 2 gain", -120,6,0,0,false },
    { "ref_board_lane2_phase", "Lane 2 phase invert", 0,1,0,1,false },
    { "ref_board_lane3_gain", "Lane 3 gain", -120,6,0,0,false },
    { "ref_board_lane3_phase", "Lane 3 phase invert", 0,1,0,1,false },
    { "ref_board_lane4_gain", "Lane 4 gain", -120,6,0,0,false },
    { "ref_board_lane4_phase", "Lane 4 phase invert", 0,1,0,1,false },
    { "ref_board_loop1_used", "Loop 1 Used", 0,1,0,1,true },
    { "ref_board_loop1_enabled", "Loop 1 Enabled", 0,1,1,1,false },
    { "ref_board_loop1_kind", "Loop 1 Kind", 0,2,0,1,true,"Serial|Parallel|Kill dry" },
    { "ref_board_loop1_blend", "Loop 1 Blend", 0,1,1,0,false },
    { "ref_board_loop1_send_db", "Loop 1 Send", -24,12,0,0,false },
    { "ref_board_loop1_return_db", "Loop 1 Return", -24,12,0,0,false },
    { "ref_board_loop1_first", "Loop 1 First slot", 0,16,0,1,true },
    { "ref_board_loop1_last", "Loop 1 Last slot", 0,16,0,1,true },
    { "ref_board_loop2_used", "Loop 2 Used", 0,1,0,1,true },
    { "ref_board_loop2_enabled", "Loop 2 Enabled", 0,1,1,1,false },
    { "ref_board_loop2_kind", "Loop 2 Kind", 0,2,0,1,true,"Serial|Parallel|Kill dry" },
    { "ref_board_loop2_blend", "Loop 2 Blend", 0,1,1,0,false },
    { "ref_board_loop2_send_db", "Loop 2 Send", -24,12,0,0,false },
    { "ref_board_loop2_return_db", "Loop 2 Return", -24,12,0,0,false },
    { "ref_board_loop2_first", "Loop 2 First slot", 0,16,0,1,true },
    { "ref_board_loop2_last", "Loop 2 Last slot", 0,16,0,1,true },
    { "ref_board_fb_kill", "Kill feedback", 0,1,0,1,false }
}};
struct ExtendedHostAutomationSnapshot
{
    static constexpr int count = static_cast<int> (ExtendedHostField::Count);
    std::array<float, count> values {};
    std::array<uint32_t, count> serials {};
};
static_assert (ExtendedHostAutomationSnapshot::count == 50);
}
