/*  The TUNE tab's chord tools (tune-builder.md 5). TUNE-HELP-ONBOARDING.
    Each is one undoable edit on the tune model's own tool. */

#include "TunePanel.h"
#include "../PluginProcessor.h"
#include "../Tune/TuneHarmony.h"

namespace luthier
{

juce::PopupMenu TunePanel::buildToolsMenu() const
{
    const auto& tune = session.getTune();
    const auto* section = tune.getSection (session.getSelectedSection());
    const bool flats = tune.preferFlats();
    const double bar = tune.getBeatsPerBar();

    // "Diatonic palette: click chords from a wheel of the current key's degrees."
    juce::PopupMenu palette;

    for (int degree = 1; degree <= 7; ++degree)
    {
        const auto triad = makeDiatonicChord (tune.meta.keyTonic, tune.meta.mode, degree, false, bar);
        const auto seventh = makeDiatonicChord (tune.meta.keyTonic, tune.meta.mode, degree, true, bar);
        palette.addItem (diatonicBase + degree, getRomanNumeral (triad, tune.meta.keyTonic, tune.meta.mode)
                                                  + "  " + getChordSymbol (triad, flats), section != nullptr);
        palette.addItem (diatonicBase + 10 + degree, "    " + getChordSymbol (seventh, flats), section != nullptr);
    }

    // "Suggest next chord: three common next moves."
    juce::PopupMenu suggest;

    if (section != nullptr)
    {
        const auto* last = section->chords.empty() ? nullptr : &section->chords.back();
        const auto options = suggestNextChords (last, tune.meta.keyTonic, tune.meta.mode, section->genreKitId, bar);

        for (int i = 0; i < (int) options.size(); ++i)
            suggest.addItem (suggestBase + i, getChordSymbol (options[(size_t) i], flats));
    }

    juce::PopupMenu transpose;

    for (int n : { -5, -3, -2, -1, 1, 2, 3, 5, 7, 12 })
        transpose.addItem (transposeBase + 12 + n, (n > 0 ? "+" : "") + juce::String (n) + " semitones");

    juce::PopupMenu modes;

    for (int m = 0; m < (int) TuneMode::numModes; ++m)
        modes.addItem (modeBase + m, getTuneModeName ((TuneMode) m), true, (int) tune.meta.mode == m);

    modes.addSeparator();
    modes.addItem (followModeItem, "Follow mode (move the melody too)", true, followMode);

    juce::PopupMenu menu;
    menu.addSubMenu ("Diatonic palette", palette, section != nullptr);
    menu.addSubMenu ("Suggest next chord", suggest, suggest.getNumItems() > 0);
    menu.addItem (reharmonizeItem, "Reharmonize section", section != nullptr && ! section->chords.empty());
    menu.addSeparator();
    menu.addSubMenu ("Transpose", transpose);
    menu.addSubMenu ("Modal shift", modes);
    return menu;
}

void TunePanel::performToolsItem (int itemId)
{
    const int index = session.getSelectedSection();
    const auto& tune = session.getTune();
    const double bar = tune.getBeatsPerBar();

    if (itemId == followModeItem)
    {
        followMode = ! followMode;
        return;
    }

    if (itemId == reharmonizeItem)
    {
        session.edit (TuneEditClass::chordEdit, "Reharmonize",
                      [index] (Tune& t) { return reharmonizeSection (t, index, {}); });
        return;
    }

    if (itemId > diatonicBase && itemId <= diatonicBase + 17)
    {
        const int degree = (itemId - diatonicBase) % 10;
        const bool seventh = itemId - diatonicBase > 10;
        const auto cell = makeDiatonicChord (tune.meta.keyTonic, tune.meta.mode, degree, seventh, bar);
        session.edit (TuneEditClass::chordEdit, "Add chord from palette",
                      [index, cell] (Tune& t) { return t.insertChord (index, -1, cell); });
        return;
    }

    if (itemId >= suggestBase && itemId < suggestBase + 10)
    {
        const auto* section = tune.getSection (index);

        if (section == nullptr)
            return;

        const auto* last = section->chords.empty() ? nullptr : &section->chords.back();
        const auto options = suggestNextChords (last, tune.meta.keyTonic, tune.meta.mode, section->genreKitId, bar);
        const int k = itemId - suggestBase;

        if (juce::isPositiveAndBelow (k, (int) options.size()))
        {
            const auto cell = options[(size_t) k];
            session.edit (TuneEditClass::chordEdit, "Add suggested chord",
                          [index, cell] (Tune& t) { return t.insertChord (index, -1, cell); });
        }

        return;
    }

    if (itemId >= transposeBase && itemId <= transposeBase + 24)
    {
        const int semitones = itemId - transposeBase - 12;
        session.edit (TuneEditClass::chordEdit, "Transpose", [semitones] (Tune& t) { return transposeTune (t, semitones); });
        return;
    }

    if (itemId >= modeBase && itemId < modeBase + (int) TuneMode::numModes)
    {
        const auto mode = (TuneMode) (itemId - modeBase);
        const bool follow = followMode;
        session.edit (TuneEditClass::chordEdit, "Modal shift", [mode, follow] (Tune& t) { return shiftMode (t, mode, follow); });
    }
}

} // namespace luthier
