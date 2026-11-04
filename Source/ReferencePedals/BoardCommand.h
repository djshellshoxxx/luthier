// OWNER: WP1 (frozen interface I-4; changes only via errata request to the WP1 owner)
#pragma once

// Every mutation of the board goes through one BoardCommand applied to the
// BoardModel on the message thread (BOARD_CORE 10.6, errata E23). The UI, the
// state reader, undo/redo, the host-automation mirror and MIDI all use the same
// vocabulary. Commands are plain value types: copyable, comparable by content,
// never touched on the audio thread.

#include <string>
#include <variant>
#include "BoardTypes.h"

namespace luthier::reference::cmd
{

struct Add
{
    std::string modelId;
    int index = -1;                       // position in the order; -1 appends
    Placement placement = Placement::beforeExistingPre;
    int lane = 0;
    InstanceId id = InstanceId::null();   // null: the model assigns one (undo/redo and state restore pass the old id)
};
struct Remove { InstanceId id; };
struct Duplicate { InstanceId id; InstanceId newId = InstanceId::null(); };
struct Move { InstanceId id; int newIndex = 0; };   // index in the final order
struct SetModel { InstanceId id; std::string modelId; };
struct SetParam
{
    InstanceId id;
    int paramIdx = 0;                     // descriptor index
    float norm = 0.f;
    uint32_t gesture = 0;                 // non-zero: consecutive edits with the same id/param/gesture coalesce into one undo step
};
struct SetEnabled { InstanceId id; bool enabled = true; };
struct SetPlacement { InstanceId id; Placement placement = Placement::beforeExistingPre; };
struct SetMode { Mode mode = Mode::Series; };
struct SetLane { InstanceId id; int lane = 0; };
struct SetLoop { int loop = 0; LoopSettings settings; };
struct SetFeedback { FeedbackSettings settings; };
struct SetStyle { BoardStyle style = BoardStyle::plywood; };
struct SetMounting { InstanceId id; Mounting mounting = Mounting::automatic; };
struct SetMix { float mix = 1.f; uint32_t gesture = 0; };
struct SetMixLaw { MixLaw law = MixLaw::EqualPower; };
struct SetBoardEnabled { bool enabled = false; };
struct SetTrim { InstanceId id; float inDb = 0.f, outDb = 0.f; uint32_t gesture = 0; };   // E4
struct SetMidiBinding
{
    enum class Op : uint8_t { Add, Remove, Edit };
    Op op = Op::Add;
    juce::var binding;                    // MIDI_EXPRESSION 11 binding object (WP6 owns the shape)
};
struct SetSlotChannel { InstanceId id; int chan = 0; bool useProfile = false; int fsMode = -1; }; // E62: -1 preserves; 0 latch,1 momentary,2 auto   // 0 = no explicit channel
struct SetScene { int scene = 0; juce::var data; };          // store scene `scene`
struct ApplySnapshot { int index = 0; juce::var data; };     // data: snapshot to apply; null var = stored snapshot `index`
// Scalar board settings that need no dedicated command, by key (reportLatency,
// limiterOn, trails, mixTail, abSelect, abOut, parallelNorm, reampEnable,
// reampReplace, reampGainDb, cpuCapPct, tempoSource, tempoBpm, midiChan,
// limiterCeilDb, laneGainDb<k>, lanePhase<k>, numLanes). Unknown keys reject.
struct SetBoardParam { std::string key; double value = 0.0; };

} // namespace luthier::reference::cmd

namespace luthier::reference
{

using BoardCommand = std::variant<
    cmd::Add, cmd::Remove, cmd::Duplicate, cmd::Move, cmd::SetModel, cmd::SetParam,
    cmd::SetEnabled, cmd::SetPlacement, cmd::SetMode, cmd::SetLane, cmd::SetLoop,
    cmd::SetFeedback, cmd::SetStyle, cmd::SetMounting, cmd::SetMix, cmd::SetMixLaw,
    cmd::SetBoardEnabled, cmd::SetTrim, cmd::SetMidiBinding, cmd::SetSlotChannel,
    cmd::SetScene, cmd::ApplySnapshot, cmd::SetBoardParam>;

// Human-readable command name for undo menu text and rejection messages.
inline const char* commandName (const BoardCommand& c) noexcept
{
    static constexpr const char* names[] {
        "Add pedal", "Remove pedal", "Duplicate pedal", "Move pedal", "Change model", "Change control",
        "Toggle pedal", "Change placement", "Change routing mode", "Change lane", "Change loop",
        "Change feedback", "Change board style", "Change mounting", "Change board mix", "Change mix law",
        "Toggle board", "Change trim", "Edit MIDI mapping", "Change MIDI channel",
        "Store scene", "Apply snapshot", "Change board setting" };
    static_assert (sizeof (names) / sizeof (names[0]) == std::variant_size_v<BoardCommand>,
                   "commandName table must list every BoardCommand alternative in order");
    return names[c.index()];
}

} // namespace luthier::reference
