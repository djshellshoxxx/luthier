#pragma once

/*  The product edition (editions.md; auto-articulation.md 11).

    The Free / Pro split itself happens later. Until then this is the runtime
    check features gate on: one process-wide flag, defaulting to Pro, which a
    licence (or a test) sets. Reading it is a relaxed atomic load, so it is safe
    on the audio thread.
*/

#include <atomic>

namespace luthier
{

enum class Edition { free = 0, pro };

namespace Editions
{
    Edition current() noexcept;
    void set (Edition e) noexcept;
    inline bool isPro() noexcept { return current() == Edition::pro; }

    /** " (Pro)", the name suffix of a Pro-only parameter in Free (editions.md 4). */
    constexpr const char* kProSuffix = " (Pro)";
}

} // namespace luthier
