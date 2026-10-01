#pragma once

/*  SPEC-SWEEP ER-65/66, error-recovery 10: a user config file that cannot be
    read is renamed to `<name>.corrupted-<timestamp>`, defaults are used, the
    error log says so, and the window shows "Preferences reset (previous file
    corrupted, backed up)." UiPreferences had this; this is the same path for
    every other settings file (accessibility, telemetry, licence, expression
    calibrations, live actions, performance).

    Message thread (the settings loaders run there); the recovered-file list is
    locked anyway, since a loader may run on a host's thread at construction.
*/

#include <juce_core/juce_core.h>

namespace luthier
{
namespace ConfigRecovery
{
    /** The file parsed as a JSON object, or a void var when it is missing or
        unreadable. An unreadable one (or one whose `schema` is above
        `maxSchema`, when that is >= 0) is moved aside first. */
    juce::var loadObject (const juce::File& file, const juce::String& module, int maxSchema = -1);

    /** Moves an unreadable file aside and records it. Returns where it went. */
    juce::File setAside (const juce::File& file, const juce::String& module);

    /** The files set aside since the last call, for the window's banner. */
    juce::StringArray takeRecoveredFiles();
}
} // namespace luthier
