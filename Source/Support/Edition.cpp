#include "Edition.h"

namespace luthier
{

namespace
{
    std::atomic<int> edition { (int) Edition::pro };
}

Edition Editions::current() noexcept
{
    return (Edition) edition.load (std::memory_order_relaxed);
}

void Editions::set (Edition e) noexcept
{
    edition.store ((int) e, std::memory_order_relaxed);
}

} // namespace luthier
