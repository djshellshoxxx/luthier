#include "Edition.h"

namespace luthier
{

namespace
{
    // -1 means use the compile-time product edition. Tests may temporarily
    // override this without creating a second production edition mechanism.
    std::atomic<int> testOverride { -1 };
}

Edition Editions::current() noexcept
{
    const int overrideValue = testOverride.load (std::memory_order_relaxed);

    if (overrideValue >= 0)
        return (Edition) overrideValue;

    return edition::isPro ? Edition::pro : Edition::free;
}

void Editions::set (Edition e) noexcept
{
    testOverride.store ((int) e, std::memory_order_relaxed);
}

void Editions::clearTestOverride() noexcept
{
    testOverride.store (-1, std::memory_order_relaxed);
}

} // namespace luthier
