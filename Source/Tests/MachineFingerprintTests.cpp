#include "TestFramework.h"
#include "../Licence/MachineFingerprint.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (MachineFingerprint, returnsFiveSaltedHashesWithoutRawIdentifiers)
{
    const auto fp = MachineFingerprint::collect();

    CHECK (fp.size() == 5);

    for (const auto& value : fp)
    {
        CHECK (value.length() == 32);
        CHECK (! value.containsChar ('|'));
    }
}

LUTHIER_TEST (MachineFingerprint, componentHashIsStableAndDomainSeparated)
{
    const auto a = MachineFingerprint::hashComponent (0, "same");
    const auto b = MachineFingerprint::hashComponent (0, "same");
    const auto c = MachineFingerprint::hashComponent (1, "same");

    CHECK (a == b);
    CHECK (a != c);
    CHECK (a.length() == 32);
}
