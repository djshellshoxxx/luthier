#pragma once

#include <juce_core/juce_core.h>
#include <optional>

namespace luthier
{

class LicenceStore
{
public:
    explicit LicenceStore (juce::File root = {});

    bool saveEnvelope (const juce::String& json) const;
    std::optional<juce::String> loadEnvelope() const;
    bool removeEnvelope() const;

    juce::Time loadLastSeen() const;
    bool updateLastSeen (juce::Time now) const;

    juce::File getLicenceFile() const { return root.getChildFile ("licence.json"); }
    juce::File getStateFile() const { return root.getChildFile ("licence-state.json"); }

    static juce::File defaultRoot();

private:
    bool atomicWrite (const juce::File&, const juce::String&) const;
    juce::File root;
};

} // namespace luthier
