#include "Boost.h"
#include "Compressor.h"
#include "Fuzz.h"
#include "Overdrive.h"
#include <cstdio>
namespace luthier::tests
{
long allocationsOnThisThread() noexcept;
}
using namespace luthier::reference;
int checks = 0, failed = 0;
void check(bool v)
{
    ++checks;
    if (!v)
        ++failed;
}
template <class T> void audit()
{
    for (double sr : {8000., 44100., 48000., 88200., 96000., 192000.})
        for (int policy : {0, 1, 2})
        {
            const long before = luthier::tests::allocationsOnThisThread();
            const auto required = T::prepareMemory(sr, 4096, policy, nullptr);
            check(before == luthier::tests::allocationsOnThisThread());
            check(required.ownedBytes == sizeof(T));
            check(required.temporaryBytes == 0);
            T p;
            const auto prepareBefore = luthier::tests::allocationsOnThisThread();
            p.prepare(sr, 4096, policy);
            check(prepareBefore == luthier::tests::allocationsOnThisThread());
            check(p.getPreparedMemoryBytes() == sizeof(T));
            p.reset();
            check(p.getPreparedMemoryBytes() == sizeof(T));
        }
    std::printf("%s sizeof=%zu\n", T::descriptor().modelId, sizeof(T));
}
int main()
{
    audit<generic::Boost>();
    audit<generic::Overdrive>();
    audit<generic::Fuzz>();
    audit<generic::Compressor>();
    std::printf("%d checks %d failures\n", checks, failed);
    return failed ? 1 : 0;
}
