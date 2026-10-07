#pragma once

/*  Raw-text hygiene for pasted and loaded tab (robust-import pass).

    Real tab arrives in every encoding of the same idea: box-drawing characters
    instead of `-` and `|`, en/em dashes, full-width pipes, non-breaking spaces,
    a byte-order mark, tab characters, numbered strings (1|, 2| ...) instead of
    e|, B|, systems written low string first, drum-kit rows. This turns all of
    that into the plain ASCII dialect the rest of the pipeline reads, in one
    linear pass, and caps what a hostile or accidental input can cost.

    Pure text-to-text; no score semantics. Offline, any non-audio thread. */

#include "TabDocument.h"

namespace luthier
{

class TabTextSanitizer
{
public:
    struct Options
    {
        int maxLineChars = 16384;     ///< longer lines are cut (and counted)
        double binaryFraction = 0.10; ///< more control bytes than this: not text
    };

    /** Returns false (and a warning) when the input is binary rather than text. */
    static bool sanitize (const juce::String& source, juce::String& cleaned,
                          TabImportDiagnostics& diagnostics, const Options& options);
};

} // namespace luthier
