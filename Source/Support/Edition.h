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

    /*  The runtime Pro-features gate driven by the licence state
        (spec/trial-lock.md). It defaults to true, so a Pro build behaves as Pro
        until the processor flips it from the licence; a Free build is unaffected
        (current() is Free regardless). When false, current() returns Free on a
        Pro build, degrading the product to the Free feature set. A test override
        set with set() still wins over this. */
    void setProUnlocked (bool unlocked) noexcept;
    bool isProUnlocked() noexcept;

    constexpr const char* kProSuffix = " (Pro)";
}

} // namespace luthier
