#pragma once

/*  Luthier-profile extension events (midi-export.md 2).

    Everything the plugin does that plain MIDI cannot say - a strum's crossing
    velocity, a squeak, a snapshot recall - travels as an extension event: a
    class name, a schema version and a list of tagged fields.

        STRUM 1 dt=-3 dir=down cv=200 striker=pick mute=0 mask=63

    The same text goes out twice (2.3): once as a text meta a person can read,
    once inside a SysEx a machine can check. Both are 7-bit ASCII, so a hex
    editor shows every field by name (2.1, "fields are tagged"). The byte-level
    layout is in docs/MIDI_EXPORT_LUTHIER_PROFILE.md.

    Field values are strings on the wire and in memory. That is what lets an
    unknown class, or an unknown field of a known class, survive a load and a
    save untouched (2.2): nothing has to understand a field to keep it.

    Nothing here runs on the audio thread; LiveMidiOut.h has the allocation-free
    encoder for live MIDI out, which writes the same bytes.
*/

#include <juce_audio_basics/juce_audio_basics.h>

#include <utility>
#include <vector>

namespace luthier
{

//==============================================================================
/** The classes of midi-export 2.1. `unknown` is a class this version cannot
    name, kept whole so a later version's file survives a save here. */
enum class LuthierEventClass
{
    note,
    bend,
    slide,
    vibrato,
    whammy,
    strum,
    rasgueado,
    pick,
    squeak,
    buzz,
    slideBar,
    clank,
    character,
    workshop,
    bassTech,
    ranges,
    snapshot,
    section,
    unknown
};

//==============================================================================
/** One documented field of a class: its tag, its default, and whether it
    identifies the user (midi-export 11). */
struct LuthierFieldSpec
{
    const char* key;
    const char* defaultValue;
    bool isIdentifier;
};

//==============================================================================
/** One extension event, at an exact sample of a performance. */
struct LuthierEvent
{
    LuthierEventClass eventClass = LuthierEventClass::unknown;
    juce::String className;          ///< "STRUM"; for an unknown class, what the file said
    int schemaVersion = 1;
    juce::int64 sample = 0;          ///< from the start of the performance
    int part = 0;                    ///< 0 is the guitar, 1 the bass (4.1 "per instrument")

    /** Tagged fields in written order. The reserved tags dt and part are never
        stored here: they are `sample` and `part` above. */
    std::vector<std::pair<juce::String, juce::String>> fields;

    /** An unknown class whose payload could not be read as tagged fields keeps
        its bytes, and they are written back as they came (2.2). The SysEx is
        held without its F0 and F7, as juce::MidiMessage::getSysExData gives it. */
    juce::MemoryBlock opaqueSysEx;
    juce::String opaqueText;

    /** An event of a known class with its name and current schema filled in. */
    static LuthierEvent make (LuthierEventClass eventClass, juce::int64 sample, int part = 0);

    bool has (const juce::String& key) const noexcept;

    /** The field, or the class's documented default when it is absent. */
    juce::String get (const juce::String& key) const;
    juce::int64 getInt (const juce::String& key) const;
    double getReal (const juce::String& key) const;

    /** Replaces the field if present, appends it if not. */
    LuthierEvent& set (const juce::String& key, const juce::String& value);
    LuthierEvent& setInt (const juce::String& key, juce::int64 value);
    LuthierEvent& setReal (const juce::String& key, double value);
    void remove (const juce::String& key);

    bool isOpaque() const noexcept { return opaqueSysEx.getSize() > 0 || opaqueText.isNotEmpty(); }

    /** Same class, schema, time, part and fields in the same order, and the
        same opaque bytes. */
    bool hasSameContent (const LuthierEvent& other) const;
};

//==============================================================================
namespace LuthierEvents
{
    /** 0x7D is the MIDI Association's non-commercial ID: the reserved range 2 names. */
    constexpr juce::uint8 kManufacturerId = 0x7D;

    /** The byte after "LT" in every event SysEx: the wire layout, not a class schema. */
    constexpr juce::uint8 kWireVersion = 1;

    constexpr int kNumClasses = (int) LuthierEventClass::unknown;

    constexpr const char* kTextPrefix  = "LUTHIER: ";
    constexpr const char* kBeginMarker = "LUTHIER-BEGIN ";
    constexpr const char* kEndMarker   = "LUTHIER-END";
    constexpr const char* kTimingTag   = "LUTHIER-AT";

    const char* getClassName (LuthierEventClass) noexcept;

    /** `unknown` for a name this version does not know. */
    LuthierEventClass getClassForName (const juce::String& name) noexcept;

    int getSchemaVersion (LuthierEventClass) noexcept;

    /** The documented fields of a class, in the order they are written. */
    const std::vector<LuthierFieldSpec>& getFields (LuthierEventClass) noexcept;
    const LuthierFieldSpec* findField (LuthierEventClass, const juce::String& key) noexcept;

    /** Class names are upper-case ASCII, digits and underscores; keys are
        lower-case ASCII, digits and underscores. */
    bool isValidClassName (const juce::String&) noexcept;
    bool isValidKey (const juce::String&) noexcept;

    //==========================================================================
    /** A real is written as the shortest text that reads back as the same
        double, so "0.6" stays "0.6" and nothing is lost. */
    juce::String formatReal (double value);
    bool parseReal (const juce::String& text, double& result);
    bool parseInt (const juce::String& text, juce::int64& result);

    /** Spaces, '=', '%', control characters and anything outside ASCII become
        %XX over the UTF-8 bytes, which keeps a payload one line of 7-bit text. */
    juce::String escapeValue (const juce::String& value);
    bool unescapeValue (const juce::String& text, juce::String& result);

    /** The payload checksum: the sum of its bytes, seven bits. */
    juce::uint8 checksum (const void* data, size_t numBytes) noexcept;

    //==========================================================================
    struct WriteOptions
    {
        /** dt: samples from the tick the event is written at to where it really is. */
        juce::int64 sampleCorrection = 0;

        /** dt and part. Off for the Generic profile's text, which has no exact timing. */
        bool writeTiming = true;

        /** midi-export 11: leaves out every field marked as an identifier. */
        bool stripIdentifiers = false;
    };

    /** "STRUM 1 dt=-3 dir=down ..." - the class, its schema, then the fields. */
    juce::String encodePayload (const LuthierEvent&, const WriteOptions&);

    /** "LUTHIER: STRUM dir=down ..." - the payload without its schema, which
        the LUTHIER-BEGIN marker carries. This is also the Generic profile's
        realism text (3). */
    juce::String encodeText (const LuthierEvent&, const WriteOptions&);

    /** F0 7D 'L' 'T' <wire version> <payload> <checksum> F7. */
    juce::MidiMessage encodeSysEx (const LuthierEvent&, const WriteOptions&);

    //==========================================================================
    /** Reads "CLASS SCHEMA k=v ...". `dt` comes back in `sampleCorrection`. */
    bool decodePayload (const juce::String& payload, LuthierEvent& event,
                        juce::int64& sampleCorrection, juce::String& error);

    /** Reads "LUTHIER: CLASS k=v ...", taking the schema from the marker. */
    bool decodeText (const juce::String& text, int schemaVersion, LuthierEvent& event,
                     juce::int64& sampleCorrection, juce::String& error);

    /** Reads a SysEx's data: the bytes between F0 and F7. */
    bool decodeSysEx (const juce::uint8* data, int numBytes, LuthierEvent& event,
                      juce::int64& sampleCorrection, juce::String& error);

    /** True if a SysEx's data carries the event signature (7D 'L' 'T'),
        whether or not the rest of it is intact. */
    bool isLuthierSysEx (const juce::uint8* data, int numBytes) noexcept;

    /** True if the checksum at the end of an event SysEx matches its payload. */
    bool hasValidChecksum (const juce::uint8* data, int numBytes) noexcept;
}

} // namespace luthier
