#include "MidiExportDefaults.h"
#include "UiPreferences.h"

namespace luthier::MidiExportDefaults
{

MidiExportOptions load()
{
    MidiExportOptions options;
    const auto text = UiPreferences::get().getString (kPreferenceKey, {});

    if (text.isEmpty())
        return options;

    MidiExportOptions parsed;

    if (MidiProfiles::profileFromVar (juce::JSON::parse (text), parsed, nullptr, nullptr))
        return parsed;

    return options;
}

void save (const MidiExportOptions& options)
{
    // The range is per export, never a default.
    auto stored = options;
    stored.range = {};

    UiPreferences::get().setString (kPreferenceKey,
                                    juce::JSON::toString (MidiProfiles::profileToVar (stored, "Defaults"), true));
}

} // namespace luthier::MidiExportDefaults
