#pragma once

/*  The suite's allocation counter (engine.md 0: nothing on the audio path
    allocates).

    CircuitTests.cpp replaces the global operator new / delete for the whole
    test program and counts the calls per thread; this is that counter's
    declaration, so any test can measure a stretch of code with

        const auto before = allocationsOnThisThread();
        ...
        CHECK (allocationsOnThisThread() == before);

    Tests guard their measurements with LUTHIER_ALLOCATION_COUNTER, which the
    test target defines (CMakeLists.txt, LuthierTests only), so a build that
    links these tests without the replaced operators still compiles. */

namespace luthier::tests
{
    /** Global operator new calls made on the calling thread so far. */
    long allocationsOnThisThread() noexcept;
}
