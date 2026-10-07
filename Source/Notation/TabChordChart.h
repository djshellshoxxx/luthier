#pragma once

/*  Chord-only charts (robust-import pass).

    A page of chord names over lyrics - Ultimate-Guitar `[ch]Am[/ch]` markup,
    ChordPro `[Am]` inline chords, or plain "Am  G  C  F" lines - has no frets
    to read. Rather than refuse it, each chord becomes a bar of strummed open or
    barre-shape chord, so the practice player can strum along and the tab view
    shows the shapes.

    Pure text to PerformanceScore; offline, any non-audio thread. */

#include "PerformanceScore.h"
#include "TabDocument.h"

#include <utility>
#include <vector>

namespace luthier
{

class TabChordChart
{
public:
    struct Chord
    {
        juce::String name;
        int rootPitchClass = 0;
        int bassPitchClass = -1;       ///< slash-chord bass, or -1
        juce::String quality;          ///< normalised: "", m, 7, m7, maj7, sus4, sus2, dim, aug, 6, 9, 5, add9
    };

    /** Parses one chord symbol ("F#m7", "Bb", "D/F#", "Cmaj9", "Asus4"). */
    static bool parseChord (const juce::String& token, Chord& out);

    /** Finds the chords of a chart in reading order. Repeat marks ("x2") repeat their line. */
    static std::vector<Chord> extractChords (const juce::String& text);

    /** As above; with a key root (0-11) Nashville-number lines ("1 4 5 1", "6m 4 1 5") read as chords too. */
    static std::vector<Chord> extractChords (const juce::String& text, int keyRootPitchClass);

    /** A voicing searched on the actual tuning (highest string first, -1 = not
        played): chord tones within a four-fret span, bass on the lowest string. */
    static std::vector<int> voicingFor (const Chord& chord, const std::vector<int>& tuningHighFirst, int capo = 0);

    /** Inline chord diagrams ("Am: x02210", "Am x-0-2-2-1-0", "{define: Am ... frets x 0 2 2 1 0}"),
        frets written low string first, -1 = x. */
    static std::vector<std::pair<juce::String, std::vector<int>>> extractDiagrams (const juce::String& text);

    /** "Strumming: D DU UDU" / "Pattern: ↓ ↓↑ ↑↓↑" / "x" mutes: +1 down, -1 up, 0 rest, 2 mute per slot. */
    static bool extractStrumPattern (const juce::String& text, std::vector<int>& pattern);

    /** "Picking: p i m a" / "T 1 2 3": 0 = thumb (bass string of the shape), 1-3 = i m a (G, B, e); -1 rest. */
    static bool extractPickingPattern (const juce::String& text, std::vector<int>& order);

    /** Frets (highest string first, -1 = not played) for a chord in standard shapes. */
    static std::vector<int> shapeFor (const Chord& chord, int numStrings = 6);

    /** Builds a one-chord-per-bar strummed score. False when the text has no chords.
        `tuningHighFirst`/`capo` come from the document's metadata when it had any. */
    static bool read (const juce::String& text, PerformanceScore& destination,
                      TabImportDiagnostics* diagnostics = nullptr,
                      const std::vector<int>& tuningHighFirst = {}, int capo = 0, double tempoBpm = 0.0);

    static constexpr int kMaxChords = 4096;
};

} // namespace luthier
