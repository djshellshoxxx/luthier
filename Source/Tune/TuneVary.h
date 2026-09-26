#pragma once

/*  "Vary" (tune-builder.md 3.3: "creates a subtle variation of the section as a
    new sibling"). TUNE-HELP-ONBOARDING workstream.

    Subtle means the section is still recognisably itself:
      - the chords stay, except the last one, which takes the first common
        substitution TuneHarmony suggests for it (a turnaround, not a new song);
      - the melody's locked notes stay byte-identical (0.3), and the rest is
        regenerated with a new seed - a drawn melody, all of it locked, stays;
      - feel and strum move a little, so the rhythm breathes differently;
      - everything else (kit, pattern, bass, layers, role) is copied.
    The sibling goes straight after the original in the section list and, when
    there is a setlist, plays straight after the original's last entry.
    Deterministic for a seed. `tune-section-edit`.
*/

#include "TuneModel.h"

namespace luthier
{

/** Adds the variation and returns its index, or -1 (no such section, or the
    section limit). */
int createSectionVariation (Tune& tune, int sectionIndex, int seed);

} // namespace luthier
