#pragma once

/*  Sample content that ships with the tune builder (onboarding.md 6,
    tune-builder.md 2.1). TUNE-HELP-ONBOARDING workstream.

      EXAMPLE TUNES - "6 example tunes ready to open, each written to show off
          a different capability: a fingerstyle etude, a jazz standard, a folk
          sketch, a metal riff, a slide blues, a funk-slap bass line." They are
          built here, in code, and written to `Resources/Tunes/Examples/` by the
          generator (the SampleContent test with LUTHIER_WRITE_SAMPLE_CONTENT=1);
          the shipped files are checked against what this builds, so the two
          cannot drift. All original material.

      EXAMPLE MIDI CLIPS - "12 example MIDI clips in Resources/Examples/, one
          per genre kit": the build has 28 kits, so twelve representative ones,
          each a four-bar phrase in its kit with an Auto melody, written by the
          same generator through the tune's MIDI export.

      EXAMPLE SETLISTS - "10 example setlists". A setlist names preset files by
          path, which is machine-specific, so they are built at run time from
          the factory bank and installed into the user's Setlists folder on a
          first run (never over a file already there).

      KIT SUGGESTIONS (2.1) - "Every genre kit ships with a suggested tempo,
          feel, and a chord palette." Kept here, beside the tune builder's other
          per-kit table (getKitMelodyDensity), rather than in GenreKit, whose
          file belongs to the rhythm engine.
*/

#include "TuneTemplates.h"

#include <vector>

namespace luthier
{

class PresetManager;
class Setlist;

//==============================================================================
namespace TuneKits
{
    /** What a kit suggests when a tune starts from it (2.1). */
    struct Suggestion
    {
        double tempoBpm = 100.0;
        double swingPercent = 0.0;   ///< the tune's swing (feel), 0 straight .. 100 triplet
        double feel = 0.5;           ///< the section's Feel slider
        juce::StringArray palette;   ///< roman numerals in the key: "I", "vi", "V7", "bVII" ...
    };

    /** The kit's suggestion; a sensible default for a kit it does not know. */
    Suggestion getSuggestion (const juce::String& genreKitName);

    /** The palette resolved in a key, one bar each. Unknown numerals are skipped. */
    std::vector<ChordCell> resolvePalette (const juce::StringArray& numerals, int tonic, TuneMode mode, double beats);
}

//==============================================================================
namespace TuneExamples
{
    inline constexpr int kNumExampleTunes = 6;
    inline constexpr int kNumMidiClips = 12;
    inline constexpr int kNumExampleSetlists = 10;

    struct Example
    {
        juce::String fileName;   ///< "01-fingerstyle-etude.luthiertune"
        Tune tune;
    };

    /** The six example tunes, deterministic. */
    std::vector<Example> buildExampleTunes();

    /** `Resources/Tunes/Examples`, or an invalid File without a Resources folder. */
    juce::File getExampleDirectory();

    /** The shipped example tunes, in file-name order (TuneTemplateLibrary's loader). */
    std::vector<TuneTemplate> loadExamples (juce::StringArray* errors = nullptr);

    //==========================================================================
    struct Clip
    {
        juce::String fileName;   ///< "01-nashville-country.mid"
        juce::String genreKit;
        Tune tune;
    };

    /** The twelve MIDI clips' tunes, deterministic. */
    std::vector<Clip> buildMidiClips();

    /** `Resources/Examples`. */
    juce::File getMidiClipDirectory();

    /** The bytes a clip's MIDI file has. */
    juce::MemoryBlock renderMidiClip (const Clip& clip);

    /** Writes the example tunes and the MIDI clips under a Resources folder;
        returns the number of files written. The generator's work. */
    int writeShippedContent (const juce::File& resourcesFolder, juce::String& error);

    //==========================================================================
    /** The ten example setlists, over the factory presets `presets` has found.
        An entry whose preset is missing is left out. */
    std::vector<Setlist> buildExampleSetlists (const PresetManager& presets);

    /** Writes any example setlist not already in `directory` (default: the
        user's Setlists folder). Returns how many were written. */
    int installExampleSetlists (const PresetManager& presets, const juce::File& directory = {});
}

} // namespace luthier
