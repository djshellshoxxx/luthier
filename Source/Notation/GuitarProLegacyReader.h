#pragma once

/*  Guitar Pro 3, 4 and 5 binary files (.gp3 / .gp4 / .gp5) as a PerformanceScore.

    The files are a sequence of little-endian fields with no index, so a single
    wrong length would shift everything after it. The reader therefore treats
    every read as fallible: a short read or an implausible count stops the parse
    and whatever was read before it is kept (a partial import with a warning),
    never a crash, a hang or an unbounded allocation.

    One track becomes the score (PerformanceScore holds one capture timeline);
    the others are listed in the diagnostics. Repeats and alternative endings
    are unrolled. Offline, worker/message thread only. */

#include "PerformanceScore.h"
#include "TabDocument.h"

namespace luthier
{

class GuitarProLegacyReader
{
public:
    /** True when the bytes start with a Guitar Pro 3/4/5 version string. */
    static bool looksLikeLegacyGuitarPro (const void* data, size_t numBytes) noexcept;

    /** True for the proprietary containers this does not read (.gpx "BCFZ/BCFS"). */
    static bool looksLikeGpx (const void* data, size_t numBytes) noexcept;

    /** True for a PowerTab file ("ptab"). */
    static bool looksLikePowerTab (const void* data, size_t numBytes) noexcept;

    /** `preferredTrack` is an index into the file's tracks, or -1 for the first
        pitched track that has notes. */
    bool read (const void* data, size_t numBytes, PerformanceScore& destination,
               TabImportDiagnostics* diagnostics = nullptr, int preferredTrack = -1);

    juce::String getLastError() const { return lastError; }

    /** Names of every track in the last file read, in file order. */
    const juce::StringArray& getTrackNames() const noexcept { return trackNames; }

    static constexpr int kMaxMeasures = 20000;
    static constexpr int kMaxTracks = 64;
    static constexpr int kMaxUnrolledMeasures = 6000;
    static constexpr size_t kMaxFileBytes = 64u * 1024u * 1024u;

private:
    juce::String lastError;
    juce::StringArray trackNames;
};

} // namespace luthier
