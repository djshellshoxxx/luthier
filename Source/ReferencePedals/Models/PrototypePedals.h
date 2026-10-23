// OWNER: WP2
#pragma once
#include "../PedalBase.h"
#include "../BoardTypes.h"
#include "../LegacyModel.h"
namespace luthier::reference::models
{
// Provisional circuit sketches retained for compatibility, not reference models.
class PrototypePedal final : public PedalBase<3>
{
public:
    explicit PrototypePedal (Model);
    static const PedalDesc& description (Model);
    void prepare (double, int, int) override;
    void reset() noexcept override;
    void process (double*, double*, int) noexcept override;
private:
    struct Channel
    {
        double low = 0, tone = 0, bass = 0, high = 0;
        double apX[4] {}, apY[4] {};
    };
    double tick (Channel&, double, double drive, double tone, double level) noexcept;
    Model model;
    Channel channels[2];
    double rate = 48000, phase = 0;
    double inputAlpha = 0, toneAlpha = 0, bassAlpha = 0, highAlpha = 0, ap = 0;
    int controlCounter = 0;
};
}
