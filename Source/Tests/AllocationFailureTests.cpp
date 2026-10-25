// OWNER: WP2 test allocator safety
#include "TestFramework.h"
#include "AllocationFailureTestHook.h"
#include <new>
using namespace luthier::tests;
LUTHIER_TEST (AllocationFailure, nothrowOverloadsShareCounterAndFailureSelector)
{
    for (const bool array : { false, true })
    {
        failNextAllocationOfSizeOnThisThread (173);
        const auto before = allocationsOnThisThread();
        void* pointer = array ? ::operator new[] (173, std::nothrow) : ::operator new (173, std::nothrow);
        const auto after = allocationsOnThisThread();
        clearAllocationFailureOnThisThread();
        CHECK (pointer == nullptr); CHECK (allocationFailureTriggeredOnThisThread()); CHECK (after == before + 1);
        if (array) ::operator delete[] (pointer); else ::operator delete (pointer);
        pointer = array ? ::operator new[] (173, std::nothrow) : ::operator new (173, std::nothrow);
        CHECK (pointer != nullptr);
        if (array) ::operator delete[] (pointer, std::nothrow); else ::operator delete (pointer, std::nothrow);
    }
}
