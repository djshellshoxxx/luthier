#pragma once

/*  The chord cell popover (tune-builder.md 3.2: "Click a cell to open a
    popover: root, quality, bass, extensions, duration, strum override,
    emphasis"). TUNE-HELP-ONBOARDING workstream.

    Every field writes through TuneSession as a `tune-chord-edit`, grouped per
    cell within 200 ms (action-and-undo 3.9), so dragging through the duration
    list is one undo step. The popover edits whichever cell it was opened on
    and re-reads it after every change, so an undo while it is open shows.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Tune/TuneSession.h"

namespace luthier
{

class TuneChordEditor : public juce::Component
{
public:
    TuneChordEditor (TuneSession& session, int sectionIndex, int cellIndex, juce::StringArray patternNames);

    /** The qualities offered, canonical suffixes in the order a player reaches for them. */
    static const juce::StringArray& getQualityChoices();

    /** The durations offered, in beats; 0 is "hold to fill". */
    static const juce::Array<double>& getDurationChoices();

    /** Re-reads the cell into the controls. */
    void refresh();

    /** What each control's change does, callable from tests. */
    bool setRoot (int pitchClass);
    bool setQuality (const juce::String& canonicalSuffix);
    bool setBass (int pitchClassOrMinusOne);
    bool setExtensions (const juce::String& commaSeparated);
    bool setDuration (double beats);
    bool setStrumOverride (const juce::String& patternOrEmpty);
    bool setEmphasis (ChordEmphasis emphasis);

    juce::ComboBox& getRootBox() noexcept      { return rootBox; }
    juce::ComboBox& getQualityBox() noexcept   { return qualityBox; }
    juce::ComboBox& getBassBox() noexcept      { return bassBox; }
    juce::TextEditor& getExtensions() noexcept { return extensions; }
    juce::ComboBox& getDurationBox() noexcept  { return durationBox; }
    juce::ComboBox& getStrumBox() noexcept     { return strumBox; }
    juce::ComboBox& getEmphasisBox() noexcept  { return emphasisBox; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    bool change (const juce::String& description, const std::function<void (ChordCell&)>& edit);
    const ChordCell* getCell() const;

    TuneSession& session;
    const int sectionIndex, cellIndex;
    juce::StringArray patterns;
    bool updating = false;

    juce::ComboBox rootBox, qualityBox, bassBox, durationBox, strumBox, emphasisBox;
    juce::TextEditor extensions;
    juce::String extensionError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneChordEditor)
};

} // namespace luthier
