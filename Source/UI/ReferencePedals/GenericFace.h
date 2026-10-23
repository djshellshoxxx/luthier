// OWNER: WP19
#pragma once
#include "PedalFace.h"
namespace luthier::reference
{
class GenericFace final : public PedalFace
{
public:
    explicit GenericFace (const PedalDesc*);
    juce::Point<float> designSizeMm() const override;
    void paintFace (juce::Graphics&, const FaceState&) override;
    void layout (FaceLayout&) override;
private:
    const PedalDesc* desc;
};
}
