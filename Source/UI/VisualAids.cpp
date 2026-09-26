#include "VisualAids.h"
#include "UiPreferences.h"

namespace luthier::VisualAids
{

namespace
{
    constexpr const char* kChordNames   = "visualAids.chordNames";
    constexpr const char* kAnnounce     = "visualAids.announceChordNames";
    constexpr const char* kRollAdvanced = "visualAids.pianoRollAdvanced";
    constexpr const char* kRollEasy     = "visualAids.pianoRollEasy";
    constexpr const char* kShowsRoll    = "visualAids.pianoRollShowsRoll";
}

bool showChordNames()                   { return UiPreferences::get().getBool (kChordNames, true); }
void setShowChordNames (bool on)        { UiPreferences::get().setBool (kChordNames, on); }

bool announceChordNamesSetting()        { return UiPreferences::get().getBool (kAnnounce, false); }
bool announceChordNames()               { return showChordNames() && announceChordNamesSetting(); }
void setAnnounceChordNames (bool on)    { UiPreferences::get().setBool (kAnnounce, on); }

bool showPianoRoll (bool advancedMode)
{
    return UiPreferences::get().getBool (advancedMode ? kRollAdvanced : kRollEasy, advancedMode);
}

void setShowPianoRoll (bool advancedMode, bool on)
{
    UiPreferences::get().setBool (advancedMode ? kRollAdvanced : kRollEasy, on);
}

bool pianoRollShowsRoll()               { return UiPreferences::get().getBool (kShowsRoll, true); }
void setPianoRollShowsRoll (bool withRoll) { UiPreferences::get().setBool (kShowsRoll, withRoll); }

} // namespace luthier::VisualAids
