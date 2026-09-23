#pragma once

/*  The `.luthiertune` file (tune-builder.md 11, file-formats.md 5, 13 and 14).

    UTF-8 JSON with `"schema": 1` and `"magic": "luthier.tune"` at the top.
    Written canonically: two-space indent, keys in schema order, optional
    blocks omitted when empty, every beat value snapped to a millionth
    (TuneTheory). Saving the same Tune always produces the same bytes, and
    loading a file and saving it again reproduces it byte for byte
    (tune-builder 15, file-formats 16).

    Unknown fields are kept (file-formats 0.3): a field this version does not
    know goes into the owning struct's `extra` on load and is written back, in
    its original order, after the known ones on save.

    Loading never throws and never half-loads. Every failure is a named
    TuneLoadError with a message naming the field ("sections[2].chords[0].root:
    not a note name"), and on failure the destination Tune is left exactly as
    it was (file-formats 14: "Corrupt files are never silently overwritten").

    Saving is atomic (file-formats 13): the text goes to `<name>.tmp` beside
    the target, is flushed to disk, and replaces the target in one rename; the
    version being replaced is first copied into `.backup/<yyyy-mm-dd>/`.
*/

#include "TuneModel.h"

namespace luthier
{

//==============================================================================
enum class TuneLoadError
{
    none = 0,
    fileNotFound,
    unreadable,
    tooLarge,
    notUtf8,
    malformedJson,
    badMagic,
    unsupportedSchema,
    missingField,
    invalidField
};

/** "FileNotFound", "BadMagic", ... for the banner and the error log. */
const char* getTuneLoadErrorName (TuneLoadError error) noexcept;

struct TuneLoadResult
{
    TuneLoadError error = TuneLoadError::none;
    juce::String message;

    /** Things that loaded but were adjusted: a tempo clamped into range, a
        duplicate section name. Shown, never fatal. */
    juce::StringArray warnings;

    bool ok() const noexcept { return error == TuneLoadError::none; }
};

//==============================================================================
class TuneFile
{
public:
    static constexpr int kSchemaVersion = 1;
    static constexpr const char* kMagic = "luthier.tune";
    static constexpr const char* kFileExtension = ".luthiertune";

    /** Refuse anything this large: a tune is "< 100 KB typical" (0.2), and a
        hundred times that is not a tune. */
    static constexpr size_t kMaxFileBytes = 10 * 1024 * 1024;

    //==========================================================================
    static juce::var toVar (const Tune& tune);

    /** The canonical text, LF line endings, ending in a newline. */
    static juce::String toJson (const Tune& tune);

    static TuneLoadResult fromVar (const juce::var& root, Tune& destination);
    static TuneLoadResult fromJson (const juce::String& text, Tune& destination);

    /** Checks the bytes are UTF-8 before parsing, as file-formats 14.2 asks. */
    static TuneLoadResult fromBytes (const void* data, size_t numBytes, Tune& destination);

    //==========================================================================
    /** Reads a file. Message or worker thread: it touches the disk. */
    static TuneLoadResult load (const juce::File& file, Tune& destination);

    /** Writes atomically, keeping a dated backup of the file it replaces when
        `keepBackup`. Returns false and sets `error` if nothing was written; the
        previous file is then untouched. */
    static bool save (const Tune& tune, const juce::File& file, juce::String& error, bool keepBackup = true);

    /** `~/Documents/Luthier/Tunes`. */
    static juce::File getUserDirectory();

    /** Where save() files the previous version of `target` today. */
    static juce::File getBackupFolder (const juce::File& target);
};

} // namespace luthier
