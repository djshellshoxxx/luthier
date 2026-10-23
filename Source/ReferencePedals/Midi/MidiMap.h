#pragma once
#include "Curves.h"
#include <array>
namespace luthier::reference::midi
{
constexpr int kMaxBindings = 1024;
constexpr int kSourceTypes = 14;
bool reservedCc (int cc) noexcept;
struct MapSnap
{
    struct Bucket { std::array<uint16_t, 8> index {}; uint8_t count = 0; };
    uint32_t version = 0;
    int numBindings = 0;
    int midiChan = 0;
    TempoSource tempoSource = TempoSource::Auto;
    double internalBpm = 120;
    int keyboardBaseNote = 36, keyboardSlots = 0;
    std::array<Binding, kMaxBindings> bindings {};
    std::array<CurveLut, kMaxBindings> curves {};
    std::array<Bucket, kSourceTypes * 16 * 128> lookup {};
    bool add (const Binding&) noexcept;
    void compile() noexcept; // message thread only, before publication
    const Bucket& find (MidiSrc type, int channel, int number) const noexcept;
};
}
