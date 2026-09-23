#pragma once

/*  The tune template library (tune-builder.md 10; factory-content.md).

    Ten templates ship, as tune-builder 10 lists them (DECISIONS C-16:
    onboarding's "12" is a miscount). Each is an ordinary `.luthiertune` under
    `Resources/Tunes/Templates/`, so a template is edited the way any tune is
    and loads through the same validated path. Files are named with a two-digit
    prefix so the picker lists them in the spec's order.

    Blank is also built in code: "New Tune" must work on an install whose
    Resources folder is missing or damaged, and a blank tune needs nothing from
    disk. The tests check the Blank file and createBlank() are the same tune.
*/

#include "TuneFile.h"

namespace luthier
{

struct TuneTemplate
{
    juce::String name;    ///< The template's title, for the picker.
    juce::File file;
    Tune tune;
};

class TuneTemplateLibrary
{
public:
    static constexpr int kNumFactoryTemplates = 10;

    /** `Resources/Tunes/Templates`, or an invalid File when no Resources folder
        was found. */
    static juce::File getFactoryDirectory();

    /** Every `.luthiertune` in a folder, in file-name order. A file that does
        not load is skipped and its error added to `errors`, never thrown. */
    static std::vector<TuneTemplate> loadDirectory (const juce::File& directory,
                                                    juce::StringArray* errors = nullptr);

    static std::vector<TuneTemplate> loadFactory (juce::StringArray* errors = nullptr);

    /** tune-builder 10's Blank: no sections, no chords. */
    static Tune createBlank();

    /** A new tune from a template: untitled, not attributed to "Factory", not
        tagged as a template, stamped with the time it was made. */
    static Tune instantiate (const Tune& templateTune);
};

} // namespace luthier
