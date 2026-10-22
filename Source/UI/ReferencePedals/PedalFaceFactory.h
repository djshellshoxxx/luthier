// OWNER: WP1 (frozen interface I-6; implementation PedalFaceFactory.cpp: WP19)
#pragma once

#include <memory>
#include "PedalFace.h"

namespace luthier::reference
{

using FaceMaker = std::unique_ptr<PedalFace> (*) ();

// Maps a PedalDesc::modelId to its face. Branded faces are procedural files
// under Faces/<snake_id>_face.cpp (WP20, WP21), generic ones come from
// GenericFace (WP19). Message thread only.
class PedalFaceFactory
{
public:
    // Registers a branded face; later registrations of the same id replace the earlier one.
    static void add (const char* modelId, FaceMaker maker);

    // Never null: unknown model ids (and MissingPedal) get a plain placeholder face.
    static std::unique_ptr<PedalFace> make (const char* modelId);

    static bool hasBrandedFace (const char* modelId);
};

} // namespace luthier::reference
