#pragma once

/*  piano-roll-chord-display.md 2: fills the SoundingNotes snapshot from the
    engine after each block (audio thread).

    - The note: the nearest semitone to what the string is actually sounding,
      so a slide or bend moves the lit key as it crosses a semitone; the
      interpreter's note for the string is the fallback. A string sounds from
      its note-on to its note-off (the string-activity stream the capture and
      MIDI out read), so voiced and rhythm-engine notes show too (ground rule 2).
    - The bend: cents from that semitone.
    - The start: the absolute sample of the string's last note-on.
*/

#include "SoundingNotes.h"

namespace luthier
{

class LuthierEngine;

class SoundingNotesPublisher
{
public:
    /** Audio thread, after the engine's block starting at `blockStartSample`. */
    void publish (LuthierEngine& engine, std::int64_t blockStartSample, SoundingNotes& target) noexcept;

    void reset() noexcept { starts.fill (0); }

private:
    std::array<std::int64_t, SoundingNotes::kMaxStrings> starts {};
};

} // namespace luthier
