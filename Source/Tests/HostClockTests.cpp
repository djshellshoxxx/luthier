#include "TestFramework.h"

#include "../Support/HostClock.h"

#include <limits>

using namespace luthier;

LUTHIER_TEST (HostClock, rejectsInvalidTempoAndTransportPositions)
{
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto infinity = std::numeric_limits<double>::infinity();

    CHECK (! HostClock::isValidTempo (nan));
    CHECK (! HostClock::isValidTempo (infinity));
    CHECK (! HostClock::isValidTempo (-infinity));
    CHECK (! HostClock::isValidTempo (0.0));
    CHECK (! HostClock::isValidTempo (-120.0));
    CHECK (HostClock::isValidTempo (20.0));
    CHECK (HostClock::isValidTempo (300.0));

    CHECK (! HostClock::isValidPosition (nan));
    CHECK (! HostClock::isValidPosition (infinity));
    CHECK (! HostClock::isValidPosition (-infinity));
    CHECK (HostClock::isValidPosition (-4.0));
    CHECK (HostClock::isValidPosition (0.0));
    CHECK (HostClock::isValidPosition (123456.75));
}
