#pragma once

/*  MIDI export and import in the Luthier and Generic profiles (midi-export.md).

    Both profiles write a format-1 standard MIDI file (1): track 0 holds the
    title, copyright, tempo map, time signature and key signature, and every
    other track holds channel messages. Each event track opens with the
    pitch-bend-range RPN on every channel it uses (3), so a host that respects
    it bends by the right amount.

    The Luthier profile (2) adds, and a Generic reader ignores:

      - the header, the first message of track 0: a sequencer-specific meta
        (FF 7F) reading 7D "LUTHIER" and then tagged fields - sample rate,
        PPQ, split, counts, identifiers - with a checksum. Import detects the
        profile by it (5); without it a file is read as Generic, silently (12).

      - a LUTHIER-AT text meta in front of any channel message that does not
        fall exactly on a tick, giving the samples from its tick (dt), its part
        when not the guitar, and its place in the performance (n) when the
        split spreads it over several tracks. Ticks carry the beat (0.4); dt
        carries the sample, so every PPQ from 96 to 3840 round-trips exactly.

      - every extension event as LUTHIER-BEGIN <class> <schema>, a text copy,
        a SysEx copy (unless SysEx redundancy is off, 8) and LUTHIER-END, all
        on the event's tick (2, 2.3).

    The Generic profile (3) writes the channel stream on the tick grid and,
    when realism is included, each extension event as a "LUTHIER:" text meta
    for a person to read. It never writes identifiers (11).

    Import refuses a damaged file with the byte it went wrong at (12, "banner
    with line reference"); it never trusts a length it has not checked against
    the bytes that are actually there.

    None of this may run on the audio thread (0.1).
*/

#include "MidiPerformance.h"

#include <vector>

namespace luthier
{

//==============================================================================
enum class MidiProfile
{
    luthier,
    generic
};

/** midi-export 4.1. */
enum class MidiTrackSplit
{
    single,
    perSection,
    perInstrument,
    perString
};

//==============================================================================
struct MidiExportOptions
{
    static constexpr int kDefaultPpq = 960;
    static constexpr int kMinPpq = 96;
    static constexpr int kMaxPpq = 3840;

    MidiProfile profile = MidiProfile::luthier;
    MidiTrackSplit split = MidiTrackSplit::single;
    int ppq = kDefaultPpq;

    /** Generic only: extension events as LUTHIER: text metas (8, default off). */
    bool includeRealism = false;

    /** Luthier only: the SysEx copy of every extension event (8). */
    bool sysExRedundancy = true;

    /** Luthier only: leave out the guitar name, preset name and character
        seed (11). The Generic profile always leaves them out. */
    bool stripIdentifiers = false;

    /** Which extension classes are written, one bit per LuthierEventClass: a
        .midprofile's subset (7). An unknown class read from a later version's
        file is always kept (2.2). */
    juce::uint32 classMask = allClasses();

    /** The stretch to export, in samples; empty means all of it (4.1). */
    juce::Range<juce::int64> range;

    static constexpr juce::uint32 allClasses() noexcept
    {
        return (juce::uint32) ((1u << LuthierEvents::kNumClasses) - 1u);
    }

    int getPpq() const noexcept { return juce::jlimit (kMinPpq, kMaxPpq, ppq); }

    bool includesClass (LuthierEventClass eventClass) const noexcept;
    void setClassIncluded (LuthierEventClass eventClass, bool shouldInclude) noexcept;
};

//==============================================================================
struct MidiImportResult
{
    bool ok = false;
    MidiProfile detectedProfile = MidiProfile::generic;

    /** Why the file was refused, starting "Byte 0x..." when a byte is to blame. */
    juce::String error;
    juce::int64 errorByteOffset = -1;

    /** Things the user should hear about before the performance is used: an
        advanced range (9), a newer schema, a sample-rate change. */
    juce::StringArray warnings;

    /** "STRUM.cv": documented fields missing from the file, read as today's
        defaults - the load notification of 10. */
    juce::StringArray defaultedFields;

    int ppq = 0;
    int numTracks = 0;

    /** Generic realism texts seen and ignored (3 says they are for people). */
    int ignoredRealismTexts = 0;
};

//==============================================================================
namespace MidiProfiles
{
    const char* getProfileName (MidiProfile) noexcept;         ///< "luthier", "generic"
    const char* getSplitName (MidiTrackSplit) noexcept;        ///< "single", "section", "instrument", "string"
    bool profileFromName (const juce::String&, MidiProfile&) noexcept;
    bool splitFromName (const juce::String&, MidiTrackSplit&) noexcept;

    //==========================================================================
    /** The complete file, as bytes. */
    juce::MemoryBlock exportToMemory (const MidiPerformance&, const MidiExportOptions&);

    /** Writes through a temporary file, so a failed export never leaves half a
        file where a good one was. */
    bool exportToFile (const MidiPerformance&, const MidiExportOptions&,
                       const juce::File& destination, juce::String* error = nullptr);

    /** Reads either profile into `destination`, at `sampleRate`. On failure
        `destination` is untouched. */
    MidiImportResult importFromMemory (const void* data, size_t numBytes,
                                       MidiPerformance& destination, double sampleRate);

    MidiImportResult importFromFile (const juce::File& source,
                                     MidiPerformance& destination, double sampleRate);

    //==========================================================================
    /** 4.2: the file the session recorder's drag hands the host. Luthier
        profile, or Generic when Alt is held; the other options (PPQ, split)
        come from `defaults`. Written into the temp folder; returns the file, or
        File() if it could not be written. */
    juce::File writeDragOutFile (const MidiPerformance&, bool forceGeneric,
                                 const MidiExportOptions& defaults);

    /** 4.1's preview: the first section's opening bar as one line of text. */
    juce::String describeOpeningBar (const MidiPerformance&, const MidiExportOptions&);

    //==========================================================================
    /** 7: a .midprofile is a small JSON file holding an export configuration.

            { "magic": "luthier.midprofile", "schema": 1,
              "meta":   { "name": "Stems for mixing" },
              "config": { "profile": "luthier", "ppq": 960, "split": "string",
                          "includeRealism": false, "sysex": true,
                          "stripIdentifiers": false, "classes": [ "NOTE", ... ] } }
    */
    constexpr const char* kProfileMagic = "luthier.midprofile";
    constexpr const char* kProfileExtension = ".midprofile";
    constexpr int kProfileSchema = 1;

    juce::var profileToVar (const MidiExportOptions&, const juce::String& name);

    /** Fills `options` from a parsed .midprofile. Unknown class names and a
        newer schema are warnings; a missing magic or config is an error. */
    bool profileFromVar (const juce::var&, MidiExportOptions& options, juce::String* name,
                         juce::String* error, juce::StringArray* warnings = nullptr);

    bool saveProfile (const juce::File&, const MidiExportOptions&, const juce::String& name);
    bool loadProfile (const juce::File&, MidiExportOptions& options, juce::String* name,
                      juce::String* error, juce::StringArray* warnings = nullptr);

    //==========================================================================
    /*  The standard-MIDI-file reader import is built on, exposed so tests (and
        a file inspector) can see exactly what is in a file and where. */
    struct SmfEvent
    {
        juce::int64 tick = 0;
        size_t offset = 0;          ///< the first byte of the delta-time
        size_t length = 0;          ///< the delta-time and the event together
        juce::uint8 status = 0;     ///< 0x80-0xEF channel, 0xF0 / 0xF7 SysEx, 0xFF meta
        int metaType = -1;
        size_t dataOffset = 0;      ///< a meta's or SysEx's payload (a SysEx's includes its F7)
        size_t dataSize = 0;
        juce::uint8 data1 = 0, data2 = 0;

        bool isChannel() const noexcept { return status >= 0x80 && status < 0xF0; }
        bool isMeta() const noexcept    { return status == 0xFF; }
        bool isSysEx() const noexcept   { return status == 0xF0; }

        /** The event as a message; `file` is the buffer it was parsed from. */
        juce::MidiMessage toMessage (const juce::uint8* file) const;
    };

    struct SmfTrack
    {
        size_t chunkOffset = 0;     ///< where "MTrk" is
        size_t chunkLength = 0;     ///< the length its header gives
        std::vector<SmfEvent> events;
    };

    struct SmfFile
    {
        int format = 1;
        int ppq = MidiExportOptions::kDefaultPpq;
        std::vector<SmfTrack> tracks;
    };

    /** False with a reason and the offending byte when the bytes are not a
        well-formed format-0 or format-1 file with a PPQ time base. */
    bool parseSmf (const void* data, size_t numBytes, SmfFile& result,
                   juce::String& error, juce::int64& errorByteOffset);
}

} // namespace luthier
