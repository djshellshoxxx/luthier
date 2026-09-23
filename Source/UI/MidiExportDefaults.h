#pragma once

/*  midi-export.md 8, Options -> MIDI: the export configuration a new export
    starts from - profile, PPQ, track split, realism in Generic, SysEx
    redundancy - plus the class subset and identifier stripping the MIDI OUT
    tab edits alongside them.

    User-global, in UiPreferences, because it is how this person likes their
    files, not part of a sound: a preset must not change what an export writes.
    Stored as the same JSON a .midprofile holds (7), so there is one reader.
*/

#include "../Export/MidiProfiles.h"

namespace luthier::MidiExportDefaults
{
    /** The saved defaults, or the spec's defaults when nothing is saved yet or
        what is saved cannot be read. */
    MidiExportOptions load();

    void save (const MidiExportOptions& options);

    constexpr const char* kPreferenceKey = "midi_export_defaults";
}
