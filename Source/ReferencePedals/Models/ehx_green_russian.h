// OWNER: WP13
#pragma once
#include "../Common/CircuitPedal.h"
#include "../Common/OnePole.h"
namespace luthier::reference::models
{
// Unregistered estimate following the Green Russian spec's closed-form clipper,
// separate from the NYC implicit transistor-feedback circuit.
class EhxGreenRussian final : public common::CircuitPedal<8>
{
  public:
    EhxGreenRussian();
    static const PedalDesc &descriptor() noexcept;
    bool controlAvailable(int i) const noexcept { return i >= 0 && i < 8 && i != 7; }
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;
    void onMidi(const MidiEvent &) noexcept override;

  private:
    struct Channel
    {
        common::OnePole inHp, q1Dc, coupling1, coupling2, feedback1, feedback2, toneLp, toneHp, outDc;
    };
    void update() noexcept;
    std::array<Channel, 2> channels;
    double pressure = 0, pressureTarget = 0, pressureCoefficient = 0, driveGain = 1, volumeGain = 0,
           toneLoss = 1, smoothedSustain = 0, smoothedVolume = 0;
};
} // namespace luthier::reference::models
