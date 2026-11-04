// OWNER: WP19
#pragma once
#include "ModelFace.h"
namespace luthier::reference
{
class GenericFace final : public ModelFace
{
public:
    explicit GenericFace(const PedalDesc* descriptor):ModelFace(genericFaceProfile(descriptor),descriptor){}
};
}
