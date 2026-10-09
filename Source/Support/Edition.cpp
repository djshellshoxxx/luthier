#include "Edition.h"

namespace luthier
{

namespace
{
    // -1 means use the compile-time product edition. Tests may temporarily
    // override this without creating a second production edition mechanism.
    std::atomic<int> testOverride { -1 };

    // The runtime Pro-features gate (spec/trial-lock.md), driven by the licence
    // state. Defaults to unlocked so nothing changes until the processor sets it.
    std::atomic<bool> proUnlocked { true };
}

Edition Editions::current() noexcept
{
    const int overrideValue = testOverride.load (std::memory_order_relaxed);

    if (overrideValue >= 0)
        return (Edition) overrideValue;

    if (! edition::isPro)
        return Edition::free;

    // A Pro build degrades to Free when the trial has expired and no unlock
    // code or paid licence keeps it unlocked.
    return proUnlocked.load (std::memory_order_relaxed) ? Edition::pro : Edition::free;
}

void Editions::set (Edition e) noexcept
{
    testOverride.store ((int) e, std::memory_order_relaxed);
}

void Editions::clearTestOverride() noexcept
{
    testOverride.store (-1, std::memory_order_relaxed);
}

void Editions::setProUnlocked (bool unlocked) noexcept
{
    proUnlocked.store (unlocked, std::memory_order_relaxed);
}

bool Editions::isProUnlocked() noexcept
{
    return proUnlocked.load (std::memory_order_relaxed);
}

} // namespace luthier
