#pragma once

#include "../Updates/Telemetry.h"
#include "MachineFingerprint.h"
#include <juce_core/juce_core.h>

namespace luthier
{

class DeactivationQueue
{
public:
    explicit DeactivationQueue (juce::File file = {});

    bool enqueue (const juce::String& licenseId,
                  const juce::StringArray& fingerprints = MachineFingerprint::collect());

    /** Attempts entries in order. Successful entries are removed; the first
        network/server failure remains queued along with everything after it. */
    int drain (Transport& transport, const juce::String& endpoint);
    int size() const;

    static juce::File defaultFile();

private:
    juce::Array<juce::var> load() const;
    bool save (const juce::Array<juce::var>&) const;

    juce::File file;
};

} // namespace luthier
