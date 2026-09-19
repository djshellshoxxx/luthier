#pragma once

/*  Genre kits (rhythm-engine.md section 7).

    A kit is a named bundle of taste: which voicing style and density suit a
    style, which of the library's patterns belong to it, how loose the playing
    should be, and which factory rig it was written against.

    A kit is data and nothing more. Applying one writes into the RhythmEngine and
    reports which rig preset it would like; it never loads that preset itself,
    because the spec is explicit that the rig reference is a soft one the user is
    free to ignore. Loading is the caller's decision.

    Kits are built in code and may be overridden from `Resources/Genres/*.json`
    by name, the same arrangement PatternLibrary uses for patterns: the built-in
    table is what guarantees the plugin has kits at all, and the directory is
    what lets someone edit them.
*/

#include "Patterns.h"
#include "RhythmEngine.h"

#include <vector>

namespace luthier
{

//==============================================================================
/** One genre's defaults (rhythm-engine 7). */
struct GenreKit
{
    juce::String name;

    /** How this style voices chords. */
    VoicingStyle voicingStyle = VoicingStyle::open;
    double voicingDensity = 100.0;

    /** Where on the neck the style tends to sit. */
    int handPositionHint = 0;

    /** Patterns this kit offers, by name, resolved against a PatternLibrary.
        The first strum pattern and the first fingerpick pattern are the ones a
        kit selects when it is applied. */
    juce::StringArray strumPatterns;
    juce::StringArray fingerpickPatterns;

    /** Humanisation the style wants. */
    RhythmHumanise humanise;

    /** How long a strum takes to cross the strings, and how even it is. */
    double strumDurationMs = 22.0;
    double strumEvenness = 0.6;

    /** A factory preset name. A soft reference: applying a kit reports this so
        the caller can offer it, and never loads it. */
    juce::String preferredPreset;

    juce::StringArray tags;

    bool isValid() const noexcept { return name.isNotEmpty(); }

    juce::var toVar() const;
    static GenreKit fromVar (const juce::var& state);

    bool loadFrom (const juce::File& file);
    bool saveTo (const juce::File& file) const;
};

//==============================================================================
/** The kits the plugin knows about. */
class GenreKitLibrary
{
public:
    GenreKitLibrary();

    /** Rebuilds the built-in table and re-scans both directories. Message thread
        only - it touches the file system. */
    void refresh();

    int getNumKits() const noexcept { return (int) kits.size(); }
    const GenreKit& getKit (int index) const noexcept;

    /** Index of the kit with this name, or -1. */
    int indexOf (const juce::String& kitName) const;

    juce::Array<int> findByTag (const juce::String& tag) const;

    juce::StringArray getNames() const;

    /** Writes a kit into the user directory and into the in-memory table,
        replacing any kit of the same name. */
    bool save (const GenreKit& kit);

    /** Pushes a kit's settings into an engine, and installs its first strum
        pattern (or fingerpick pattern, if the kit has no strum patterns) from
        `patterns`. Returns false if the kit names no pattern the library holds,
        in which case the engine's settings are still applied.

        The kit's `preferredPreset` is deliberately not acted on here. */
    static bool apply (const GenreKit& kit, RhythmEngine& engine,
                       const PatternLibrary& patterns);

    /** Picks a pattern from the kit at random, for the panel's "randomise within
        style" dice (rhythm-engine 8.1). Returns -1 if the kit names nothing the
        library holds. */
    static int chooseRandomPattern (const GenreKit& kit, const PatternLibrary& patterns,
                                    juce::Random& random);

    static juce::File getFactoryDirectory();
    static juce::File getUserDirectory();

private:
    void addFactoryKits();
    void scanDirectory (const juce::File& directory);

    std::vector<GenreKit> kits;
};

} // namespace luthier
