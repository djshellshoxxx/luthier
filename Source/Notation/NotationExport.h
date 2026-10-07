#pragma once

/*  Notation export (notation-export.md section 2).

    Four formats, written from one PerformanceScore:

      - MusicXML 4.0, which every notation program reads. Written as a TAB
        staff by default, or as a standard staff (real noteheads, no tab)
        when `NotationExportOptions::staffMode` asks for one.
      - Guitar Pro, which is what the target audience actually uses.
      - ASCII tab, which is what gets pasted into a forum post.
      - Standard MIDI, which is what gets dragged into a DAW.

    Each is a separate writer over the same score, and each documents its own
    round-trip loss where the format cannot express something the score holds.
    Everything here is offline and runs on a worker thread (rule 1 of section 0).

    The Guitar Pro writer needs a word of explanation. `.gp` is a zipped bundle
    of XML, and the format is not published: writing a byte-exact one would mean
    reverse-engineering a moving target. What is written instead is the bundle's
    structure with a GPIF score inside it, which Guitar Pro 7 and 8 open, and
    which is the documented-enough part of the format. Where that is not
    achievable the writer says so rather than producing a file that will not
    open - see `getLastError`.
*/

#include "PerformanceScore.h"
#include "AsciiTabReader.h"

namespace luthier
{

//==============================================================================
/** Options shared by the exporters (notation-export 5). */
struct NotationExportOptions
{
    /** ASCII tab line width. */
    int lineWidth = 80;

    /** Guitar Pro: insert a chord diagram the first time each chord appears. */
    bool chordDiagrams = true;

    /** Include chord symbols where the detector found them. */
    bool chordSymbols = true;

    /** Export only part of the score. Zero length means all of it. */
    double fromBeat = 0.0;
    double lengthBeats = 0.0;

    /** ASCII tab: how much technique notation to include. */
    enum class SymbolDensity { full = 0, minimal, notesOnly };
    SymbolDensity density = SymbolDensity::full;

    /** MusicXML: which staff to write. The ASCII/GP tab lane owns tablature;
        `standardStaff` is the plain staff a reader who does not tab needs -
        noteheads on a five-line staff, treble clef with the standard guitar
        octave-down convention, no string/fret or tab-only technique marks. */
    enum class StaffMode { tabStaff = 0, standardStaff };
    StaffMode staffMode = StaffMode::tabStaff;
};

//==============================================================================
/** The formats (notation-export 2). */
enum class NotationFormat
{
    musicXml = 0,
    guitarPro,
    asciiTab,
    midi,
    numFormats
};

const char* getNotationFormatName (NotationFormat format) noexcept;
const char* getNotationFormatExtension (NotationFormat format) noexcept;

/** What each format cannot carry, in one line, for the export dialog. */
const char* getNotationFormatLoss (NotationFormat format) noexcept;

//==============================================================================
class NotationExporter
{
public:
    /** Writes the score in the chosen format. Returns false and sets the error
        if it could not be written. Worker thread. */
    bool write (const PerformanceScore& score, NotationFormat format,
                const juce::File& destination, const NotationExportOptions& options = {});

    juce::String getLastError() const { return lastError; }

    //==========================================================================
    /** The exporters, individually, for the preview pane and for tests. */

    juce::String renderMusicXml (const PerformanceScore& score,
                                 const NotationExportOptions& options = {}) const;

    juce::String renderAsciiTab (const PerformanceScore& score,
                                 const NotationExportOptions& options = {}) const;

    /** notation-export 3: the live view shows the last N beats as ASCII tab. */
    juce::String renderAsciiTabWindow (const PerformanceScore& score,
                                       int firstMeasure, int numMeasures,
                                       const NotationExportOptions& options = {}) const;

    bool writeMidi (const PerformanceScore& score, const juce::File& destination,
                    const NotationExportOptions& options = {}) const;

    bool writeGuitarPro (const PerformanceScore& score, const juce::File& destination,
                         const NotationExportOptions& options = {});

    /** The GPIF XML a Guitar Pro bundle contains, exposed for tests. */
    juce::String renderGuitarProXml (const PerformanceScore& score,
                                     const NotationExportOptions& options = {}) const;

private:
    mutable juce::String lastError;
};

//==============================================================================
/** Reading notation back in (practice-tools.md section 6).

    The tab reader loads what a player already has. ASCII and MusicXML are read
    properly; the Guitar Pro formats are binary and version-specific, so what is
    supported is stated rather than implied - see `canRead`. */
class NotationImporter
{
public:
    /** True when the file's extension is one this can actually parse. Checked
        before offering to open it, so the user is not told a file is corrupt
        when the truth is that the format is not supported. */
    static bool canRead (const juce::File& file);

    /** What a file actually is: the bytes decide where they can (MIDI, Guitar
        Pro, zip containers), the extension where they cannot (text). */
    enum class FileKind
    {
        asciiTab, musicXml, compressedMusicXml, guitarPro7, guitarProLegacy,
        guitarProGpx, powerTab, midi, unknown
    };

    static FileKind detectKind (const juce::File& file);

    /** Parses a file into a score. Returns false and sets the error otherwise.
        Partial reads (tab-import-export 7) return true and say what was
        skipped in getLastDiagnostics(). */
    bool read (const juce::File& file, PerformanceScore& destination);

    /** Parses ASCII tab (AsciiTabReader). Exposed separately because the tab
        view pastes it. `diagnostics` receives what was read and skipped; the
        same report is kept in getLastDiagnostics(). */
    bool readAsciiTab (const juce::String& text, PerformanceScore& destination,
                       TabImportDiagnostics* diagnostics = nullptr);

    bool readMusicXml (const juce::String& text, PerformanceScore& destination);

    /** tab-import-export 8: a standard MIDI file as a tab. A Luthier-profile
        file (or a per-string export) keeps its strings and frets; a generic
        file is fingered by TabFingering's guess. The diagnostics report how
        many notes were fingered or clamped. */
    bool readMidi (const juce::File& file, PerformanceScore& destination);
    bool readMidi (const void* data, size_t numBytes, PerformanceScore& destination);
    /*  SPEC-SWEEP NE-4 (notation-export 2.2): Guitar Pro 7/8 - a `.gp` zip with
        Content/score.gpif inside. Reads the structure Luthier writes and GP7
        uses (master bars -> bars -> voices -> beats -> notes, rhythms, tuning
        and the note techniques GPIF names). `.gp5`/`.gpx`/`.ptb` stay unread. */
    bool readGuitarPro (const juce::File& file, PerformanceScore& destination);
    bool readGpif (const juce::String& xml, PerformanceScore& destination);

    /** Guitar Pro 3/4/5 binary (.gp3/.gp4/.gp5). Partial files import what was
        readable and say so in the diagnostics. `preferredTrack` is a 0-based
        track index, or -1 for the first pitched track. */
    bool readGuitarProLegacy (const void* data, size_t numBytes, PerformanceScore& destination);
    void setPreferredTrack (int trackIndex) noexcept { preferredTrack = trackIndex; }

    /** Compressed MusicXML (.mxl): a zip whose META-INF/container.xml names the score. */
    bool readCompressedMusicXml (const juce::File& file, PerformanceScore& destination);

    juce::String getLastError() const { return lastError; }

    /** What the last read did: bars, notes, skipped lines, guessed tuning. */
    const TabImportDiagnostics& getLastDiagnostics() const noexcept { return lastDiagnostics; }

private:
    juce::String lastError;
    TabImportDiagnostics lastDiagnostics;
    int preferredTrack = -1;
};

} // namespace luthier
