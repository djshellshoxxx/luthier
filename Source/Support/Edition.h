#pragma once

/* Transitional runtime edition seam.

   Real product edition is compile-time (Source/Edition.h). The runtime override
   remains solely so the existing edition tests can exercise Free behaviour from
   the Pro test binary. Production code should query luthier::edition directly.
*/

#include "../Edition.h"
#include <atomic>

namespace luthier
{

enum class Edition { free = 0, pro };

namespace Editions
{
    Edition current() noexcept;
    void set (Edition e) noexcept;
    void clearTestOverride() noexcept;
    inline bool isPro() noexcept { return current() == Edition::pro; }

    constexpr const char* kProSuffix = " (Pro)";
}

} // namespace luthier
