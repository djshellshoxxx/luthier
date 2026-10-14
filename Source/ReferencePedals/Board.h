#pragma once

// Experimental, independently routed reference-pedal engine.
// The existing EffectsChain/PedalType and its factory presets are untouched.
// The three nonlinear/filter models below are CIRCUIT-INSPIRED PROTOTYPES,
// not measured/revision-accurate emulations. See the per-model specs.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace luthier::reference
{

enum class Model : int { empty = 0, ts808Prototype, ds1Prototype, phase90Prototype };
enum class Placement : int { beforeExistingPre = 0, afterExistingPost = 1 };

class Board final
{
public:
    static constexpr int slotCount = 8;
    static constexpr int modelCount = 4;

    Board() = default;
    Board (const Board&) = delete;
    Board& operator= (const Board&) = delete;

    static const char* modelName (Model model) noexcept
    {
        switch (model)
        {
            case Model::ts808Prototype: return "TS808-inspired (prototype)";
            case Model::ds1Prototype: return "DS-1-inspired (prototype)";
            case Model::phase90Prototype: return "Phase 90-inspired (prototype)";
            default: return "Empty";
        }
    }

    void prepare (double sampleRate) noexcept
    {
        sr = std::isfinite (sampleRate) && sampleRate >= 8000.0 ? sampleRate : 48000.0;
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : state)
        {
            s = {};
        }
    }

    void setBoardEnabled (bool enabled) noexcept { boardEnabled.store (enabled, std::memory_order_release); }
    bool isBoardEnabled() const noexcept { return boardEnabled.load (std::memory_order_acquire); }

    void setModel (int slot, Model model) noexcept
    {
        if (valid (slot) && (int) model >= 0 && (int) model < modelCount)
            slots[(size_t) slot].model.store ((int) model, std::memory_order_release);
    }
    Model getModel (int slot) const noexcept
    {
        return valid (slot) ? (Model) slots[(size_t) slot].model.load (std::memory_order_acquire)
                            : Model::empty;
    }

    void setSlotEnabled (int slot, bool enabled) noexcept
    {
        if (valid (slot)) slots[(size_t) slot].enabled.store (enabled, std::memory_order_release);
    }
    bool isSlotEnabled (int slot) const noexcept
    {
        return valid (slot) && slots[(size_t) slot].enabled.load (std::memory_order_acquire);
    }
    void setPlacement (int slot, Placement p) noexcept
    {
        if (valid (slot)) slots[(size_t) slot].placement.store ((int) p, std::memory_order_release);
    }
    Placement getPlacement (int slot) const noexcept
    {
        return valid (slot) ? (Placement) slots[(size_t) slot].placement.load (std::memory_order_acquire)
                            : Placement::beforeExistingPre;
    }
    void setKnob (int slot, int knob, double v) noexcept
    {
        if (valid (slot) && knob >= 0 && knob < 3 && std::isfinite (v))
            slots[(size_t) slot].knobs[(size_t) knob].store (std::clamp (v, 0.0, 1.0), std::memory_order_release);
    }
    double getKnob (int slot, int knob) const noexcept
    {
        return valid (slot) && knob >= 0 && knob < 3
            ? slots[(size_t) slot].knobs[(size_t) knob].load (std::memory_order_acquire) : 0.0;
    }

    // Move a physical pedal instance, not its parameter values or DSP history.
    // One CAS commits the entire order, so the audio callback never sees a
    // duplicate or partially rearranged routing graph.
    void move (int from, int to) noexcept
    {
        if (!valid (from) || !valid (to) || from == to) return;
        std::uint32_t before = packedOrder.load (std::memory_order_acquire);
        for (;;)
        {
            int ids[slotCount];
            for (int i = 0; i < slotCount; ++i)
                ids[i] = (int) ((before >> (i * 4)) & 0xfu);
            const int id = ids[from];
            if (from < to)
                for (int i = from; i < to; ++i) ids[i] = ids[i + 1];
            else
                for (int i = from; i > to; --i) ids[i] = ids[i - 1];
            ids[to] = id;
            std::uint32_t after = 0;
            for (int i = 0; i < slotCount; ++i)
                after |= (std::uint32_t) ids[i] << (i * 4);
            if (packedOrder.compare_exchange_weak (before, after, std::memory_order_acq_rel))
                return;
        }
    }

    int getIdAtPosition (int position) const noexcept
    {
        if (! valid (position)) return -1;
        return (int) ((packedOrder.load (std::memory_order_acquire) >> (position * 4)) & 0xfu);
    }

    // Only one calling audio thread may invoke this method. No allocations,
    // waits, atomics writes, dynamic dispatch, or UI interactions.
    void processStereo (double* left, double* right, int samples, Placement placement) noexcept
    {
        if (! left || ! right || samples <= 0 || ! isBoardEnabled()) return;
        const auto order = packedOrder.load (std::memory_order_acquire);
        for (int position = 0; position < slotCount; ++position)
        {
            const int index = (int) ((order >> (position * 4)) & 0xfu);
            if (!valid (index)) continue;
            auto& slot = slots[(size_t) index];
            if (slot.placement.load (std::memory_order_acquire) != (int) placement) continue;
            const Model model = (Model) slot.model.load (std::memory_order_acquire);
            if (model == Model::empty) continue;
            auto& local = state[(size_t) index];
            if (local.lastModel != model)
            {
                local = {};
                local.lastModel = model;
            }

            const bool enabled = slot.enabled.load (std::memory_order_acquire);
            const double k0 = slot.knobs[0].load (std::memory_order_relaxed);
            const double k1 = slot.knobs[1].load (std::memory_order_relaxed);
            const double k2 = slot.knobs[2].load (std::memory_order_relaxed);
            // Approximately 5 ms for click suppression, no host latency.
            const double step = 1.0 / std::max (1.0, 0.005 * sr);
            for (int i = 0; i < samples; ++i)
            {
                local.mix = std::clamp (local.mix + (enabled ? step : -step), 0.0, 1.0);
                const double dryL = left[i], dryR = right[i];
                const double wetL = processOne (local.ch[0], model, dryL, k0, k1, k2);
                const double wetR = processOne (local.ch[1], model, dryR, k0, k1, k2);
                left[i] = safe (dryL + (wetL - dryL) * local.mix);
                right[i] = safe (dryR + (wetR - dryR) * local.mix);
            }
        }
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    static constexpr std::uint32_t initialOrder = 0x76543210u;
    struct Slot
    {
        std::atomic<int> model {0};
        std::atomic<int> placement {0};
        std::atomic<bool> enabled {true};
        std::array<std::atomic<double>, 3> knobs {
            std::atomic<double> {0.5}, std::atomic<double> {0.5}, std::atomic<double> {0.5}
        };
    };
    struct Channel
    {
        double hpPrevious = 0.0, lowPrevious = 0.0, toneLow = 0.0;
        double toneHigh = 0.0, phase = 0.0;
        double allpassX[4] {}, allpassY[4] {};
    };
    struct State
    {
        Channel ch[2];
        Model lastModel = Model::empty;
        double mix = 0.0;
    };
    static bool valid (int n) noexcept { return n >= 0 && n < slotCount; }
    static double safe (double x) noexcept
    {
        return std::isfinite (x) ? std::clamp (x, -16.0, 16.0) : 0.0;
    }
    double lp (double in, double hz, double& memory) const noexcept
    {
        const double alpha = 1.0 - std::exp (-2.0 * pi * std::clamp (hz, 1.0, sr * 0.45) / sr);
        memory += alpha * (in - memory);
        return memory;
    }

    double processOne (Channel& s, Model model, double x, double drive, double tone, double level) noexcept
    {
        x = safe (x);
        if (model == Model::ts808Prototype)
        {
            // Emphasises the TS808's feedback-clipping bass cut (~720 Hz);
            // tanh is only a provisional diode solver; hardware calibration and
            // circuit-accurate feedback clipping are still REQUIRED.
            const double low = lp (x, 720.0, s.hpPrevious);
            const double clipped = std::tanh ((x - low) * (2.0 + 38.0 * drive) + low * 0.6);
            const double treble = lp (clipped, 700.0 + 6500.0 * tone, s.lowPrevious);
            return safe (treble * (0.1 + 1.5 * level));
        }
        if (model == Model::ds1Prototype)
        {
            // DS-1 schematic: transistor boost -> gain stage -> antiparallel
            // ground-clipping diodes -> bass/treble blend. Exact transistor
            // bias and diode I/V are NOT yet implemented.
            const double low = lp (x, 72.0, s.hpPrevious);
            const double gain = 4.0 * (1.0 + 21.3 * drive);
            const double clipped = std::clamp ((x - low) * gain, -0.7, 0.7);
            const double bass = lp (clipped, 330.0, s.toneLow);
            const double highs = clipped - lp (clipped, 1350.0, s.toneHigh);
            return safe ((bass * (1.0 - tone) + highs * tone) * (0.12 + 1.7 * level));
        }
        if (model == Model::phase90Prototype)
        {
            // Four independent all-pass stages produce two swept notches on
            // dry/wet summation. LFO shape and FET control are prototypes.
            s.phase += (0.13 + 3.0 * drive) / sr;
            if (s.phase >= 1.0) s.phase -= 1.0;
            const double triangle = 1.0 - 4.0 * std::abs (s.phase - 0.5);
            const double frequency = 130.0 * std::pow (16.0, (triangle + 1.0) * 0.5);
            const double k = std::tan (pi * frequency / sr);
            const double a = (k - 1.0) / (k + 1.0);
            double y = x;
            for (int p = 0; p < 4; ++p)
            {
                const double z = -a * y + s.allpassX[p] + a * s.allpassY[p];
                s.allpassX[p] = y;
                s.allpassY[p] = z;
                y = z;
            }
            (void) tone; (void) level; // Phase 90 has one physical SPEED knob.
            return safe (0.5 * (x + y));
        }
        return x;
    }

    std::array<Slot, slotCount> slots;
    std::array<State, slotCount> state;
    std::atomic<bool> boardEnabled {false};
    std::atomic<std::uint32_t> packedOrder {initialOrder};
    double sr = 48000.0;
};

} // namespace luthier::reference
