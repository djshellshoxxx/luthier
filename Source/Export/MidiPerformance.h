#pragma once

/*  A performance as MIDI export and import see it (midi-export.md 0, 5, 9).

    Two layers, kept apart:

      - the channel stream: every note, controller, pitch-bend, pressure and
        program message the engine would be sent, each at an exact sample;
      - the extension events (LuthierMidiEvents.h): what plain MIDI cannot say.

    The channel stream is what renders. Played into the engine with the same
    preset it reproduces the audio sample for sample, which is how the Luthier
    profile's round trip (2.2) is achieved and how it is tested. The extension
    events ride alongside, exactly placed, so nothing the plugin reported is
    lost on the way through a file.

    PerformanceScore is the notation view of the same thing (9, "two
    serialisations of the same data"): fromScore and toScore convert, using
    NotationExporter's conventions - one MIDI channel per string, a two-semitone
    bend range, pitch bend for bends and whammy - so the two exports agree.

    Built and read on the message thread or a worker (0.1). The only call meant
    for the audio thread is renderBlock, which reads and never allocates.
*/

#include "LuthierMidiEvents.h"
#include "../Notation/PerformanceScore.h"

#include <vector>

namespace luthier
{

class MidiCapture;

//==============================================================================
/** One channel-voice message at an exact sample. */
struct PerformanceMessage
{
    juce::int64 sample = 0;
    juce::MidiMessage message;
    int part = 0;                    ///< 0 is the guitar, 1 the bass
};

struct PerformanceTempo
{
    double beat = 0.0;               ///< quarter notes from the start
    double bpm = 120.0;
};

struct PerformanceTimeSignature
{
    double beat = 0.0;
    int numerator = 4;
    int denominator = 4;
};

//==============================================================================
class MidiPerformance
{
public:
    struct Meta
    {
        juce::String title;
        juce::String copyright;

        /*  midi-export 11: these identify the player's guitar and presets. The
            Luthier profile writes them unless identifiers are stripped; the
            Generic profile never does. */
        juce::String guitarName;
        juce::String presetName;
        juce::String characterSeed;

        int keySharpsOrFlats = 0;
        bool keyIsMinor = false;

        /** What the file's pitch-bend RPN declares (3). */
        double pitchBendRangeSemitones = 2.0;

        /** Part names by index, for the per-instrument split (4.1). */
        juce::StringArray partNames { "Guitar", "Bass" };
    };

    explicit MidiPerformance (double sampleRate = 48000.0);

    double getSampleRate() const noexcept { return sampleRate; }
    void setSampleRate (double newSampleRate) noexcept;

    Meta& getMeta() noexcept { return meta; }
    const Meta& getMeta() const noexcept { return meta; }

    /** Everything goes, including the tempo map, which returns to 120 bpm 4/4. */
    void clear();

    //==========================================================================
    // Tempo map and time signatures, in beats (quarter notes).

    /** Replaces the map with a single tempo. */
    void setTempo (double bpm);
    void addTempoChange (double beat, double bpm);
    const std::vector<PerformanceTempo>& getTempoMap() const noexcept { return tempoMap; }
    double getTempoAt (double beat) const noexcept;

    void setTimeSignature (int numerator, int denominator);
    void addTimeSignature (double beat, int numerator, int denominator);
    const std::vector<PerformanceTimeSignature>& getTimeSignatures() const noexcept { return timeSignatures; }
    PerformanceTimeSignature getTimeSignatureAt (double beat) const noexcept;

    double beatToSeconds (double beat) const noexcept;
    double secondsToBeat (double seconds) const noexcept;
    double beatToSample (double beat) const noexcept   { return beatToSeconds (beat) * sampleRate; }
    double sampleToBeat (double sample) const noexcept { return secondsToBeat (sample / sampleRate); }

    //==========================================================================
    /** Adds a channel-voice message (note, CC, pitch bend, pressure, program).
        Anything else - SysEx, meta, clock - is refused. Samples before zero are
        clamped to zero. A message lands after any already at the same sample,
        so the order things are added in is the order they play in. */
    bool addMessage (juce::int64 sample, const juce::MidiMessage& message, int part = 0);
    const std::vector<PerformanceMessage>& getMessages() const noexcept { return messages; }

    /** SPEC-SWEEP BT-25: the messages, for an export that adjusts them in a copy. */
    std::vector<PerformanceMessage>& getMessagesForEditing() noexcept { return messages; }

    /** Adds an extension event, after any already at the same sample. */
    void addEvent (const LuthierEvent& event);
    const std::vector<LuthierEvent>& getEvents() const noexcept { return events; }
    int countEvents (LuthierEventClass eventClass) const noexcept;

    /** One past the last message or event; zero when empty. */
    juce::int64 getLengthInSamples() const noexcept;

    static bool isChannelVoiceMessage (const juce::MidiMessage& message) noexcept;

    //==========================================================================
    // Ranges for the export dialog (4.1).

    juce::Range<juce::int64> getWholeRange() const noexcept { return { 0, getLengthInSamples() }; }
    juce::Range<juce::int64> getLastSecondsRange (double seconds) const noexcept;

    /** From a SECTION start to the next SECTION event, or to the end; empty if
        there is no section of that name. */
    juce::Range<juce::int64> getSectionRange (const juce::String& sectionName) const;
    juce::StringArray getSectionNames() const;

    /** A copy of one stretch, moved to start at sample zero. Notes that began
        before the range are left out; notes still sounding at its end are
        closed there; the controllers and bend in force at its start are
        restated at zero, so the extract sounds as it did in place. */
    MidiPerformance extractRange (juce::Range<juce::int64> range) const;

    //==========================================================================
    /** Adds every message in [blockStart, blockStart + numSamples) to
        `destination` at its offset, advancing `cursor` (start it at zero).
        Reads only, so a player on the audio thread can call it; the
        MidiBuffer is the caller's and should be reserved in advance. */
    void renderBlock (juce::MidiBuffer& destination, juce::int64 blockStart,
                      int numSamples, size_t& cursor) const;

    //==========================================================================
    /** notation-export's score as a performance: every track a part, every
        string its own channel, techniques as NOTE flags and BEND / SLIDE /
        VIBRATO / WHAMMY events, bends and whammy also as pitch bend. */
    static MidiPerformance fromScore (const PerformanceScore& score, double sampleRate);

    /** Part 0 as a score. The string and fret come from each note's NOTE event,
        or from its channel and the score's track-0 tuning when it has none, so
        set the tuning before calling. */
    void toScore (PerformanceScore& score) const;

    /** The session recorder's retrospective capture as a performance, at the
        exact samples it was played. `sampleRate` is the rate the capture was
        prepared with. */
    static MidiPerformance fromCapture (const MidiCapture& capture, double sampleRate, double tempoBpm);

    //==========================================================================
    /** The same messages at the same samples in the same order, and the same
        extension events. Tempo and meta are not compared. */
    bool isEquivalentTo (const MidiPerformance& other, juce::String* firstDifference = nullptr) const;

private:
    double sampleRate = 48000.0;
    Meta meta;

    std::vector<PerformanceTempo> tempoMap;
    std::vector<PerformanceTimeSignature> timeSignatures;

    std::vector<PerformanceMessage> messages;
    std::vector<LuthierEvent> events;
};

} // namespace luthier
