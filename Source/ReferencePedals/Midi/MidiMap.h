// OWNER: WP5
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
    bool boardEnabled = true; // standalone tables default live; model supplies actual OFF truth
    TempoSource tempoSource = TempoSource::Auto;
    double internalBpm = 120;
    int keyboardBaseNote = 36, keyboardSlots = 0;
    std::array<Binding, kMaxBindings> bindings {};
    std::array<float, kMaxBindings> initialValues {};
    std::array<uint16_t, kMaxBindings> steps {};
    std::array<uint8_t, kMaxBindings> layers {}; // explicit0, focus1, local2
    std::array<std::array<int8_t, 128>, 16> footCc = [] { std::array<std::array<int8_t, 128>, 16> a {}; for (auto& c : a) c.fill (-1); return a; }();
    std::array<std::array<uint8_t, 128>, 16> footLayer = [] { std::array<std::array<uint8_t, 128>, 16> a {}; for (auto& c : a) c.fill (255); return a; }();
    std::array<int8_t, 128> noteToSlot = [] { std::array<int8_t, 128> a {}; a.fill (-1); return a; }();
    std::array<CurveLut, kMaxBindings> curves {};
    std::array<Bucket, kSourceTypes * 16 * 128> lookup {};
    bool add (const Binding&, uint8_t layer = 0) noexcept;
    void compile() noexcept; // message thread only, before publication
    const Bucket& find (MidiSrc type, int channel, int number) const noexcept;
};
}
