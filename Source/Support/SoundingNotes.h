#pragma once

/*  piano-roll-chord-display.md 2: what the strings are sounding, published by
    the audio thread after every block for the piano roll and the chord name.

    Double-buffered behind one atomic sequence number: the audio thread fills
    the buffer the sequence does not point at, then bumps the sequence. It only
    stores and never waits. A reader copies the current buffer and re-reads the
    sequence; if a publish landed meanwhile it tries again (at most a few
    times - a 30 Hz timer can simply use the next frame).

    Per block: two 64-bit words for the 128-bit note set, one packed word per
    string (note, bend in cents, start sample) and the sequence increment.
    Everything is a relaxed atomic store, so there is no allocation, no lock
    and no data race (section 7, PR-07).

    animated-strings.md 4.1 extends the same publish with a per-string Motion
    record (level, where the string is stopped, the finger's push, the pluck
    position, damping, harmonic, stop kind) and the engine's sample clock, for
    the vibrating strings. LuthierEngine::publishSoundingNotes is the one
    writer, once per sub-block, whatever the display settings.
*/

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cstdint>

namespace luthier
{

class SoundingNotes
{
public:
    static constexpr int kMaxStrings = 12;

    /** One string: its note (-1 for none), how far it is bent (cents, the
        nearest semitone being `note`) and the sample its note started on. */
    struct String
    {
        int note = -1;
        int bendCents = 0;
        std::int64_t startSample = 0;
    };

    /** animated-strings.md 4.1: how a string is moving, for the display. */
    enum StopKind : std::uint8_t { open = 0, fretted = 1, tapped = 2, slide = 3 };

    struct Motion
    {
        float level = 0.0f;              ///< StringEngine::getLevel()
        float stopFret = 0.0f;           ///< absolute frets from the nut: capo, fret, tap or slide contact (2.1)
        float pushCents = 0.0f;          ///< the finger-bend part only: no whammy, no slide (2.4)
        float pluckPosition = 0.16f;     ///< the last Excitation::Params::pluckPosition, from the bridge
        float fret = 0.0f;               ///< the note's fret from the capo (LuthierEngine::getStringFret)
        std::int64_t exciteSample = -1;  ///< the last excitation; kept after note-off while the string rings
        std::uint8_t damping = 0;        ///< StringEngine::Damping
        std::uint8_t harmonicPartial = 0;///< 0/1 = none
        std::uint8_t stopKind = open;    ///< StopKind
    };

    /** A copy of one publish. */
    struct Frame
    {
        std::array<std::uint64_t, 2> noteBits {};   ///< bit n: MIDI note n is sounding
        std::array<String, kMaxStrings> strings {};
        int numStrings = 0;
        std::uint32_t sequence = 0;                   ///< 0: nothing published yet

        // animated-strings.md 4.1.
        std::array<Motion, kMaxStrings> motion {};
        std::int64_t samplePosition = 0;              ///< the engine's sample clock at the publish
        double sampleRate = 44100.0;

        bool isSounding (int note) const noexcept
        {
            return juce::isPositiveAndBelow (note, 128) && ((noteBits[(size_t) (note >> 6)] >> (note & 63)) & 1u) != 0;
        }

        int countSounding() const noexcept;
    };

    //==========================================================================
    /** Audio thread, once per block. `notes[s]` is -1 for a silent string. */
    void publish (const int* notes, const int* bendCents, const std::int64_t* startSamples, int numStrings) noexcept;

    /** animated-strings.md 4.1: the same, with each string's Motion (`motion[s]`,
        or nullptr for none) and the engine's clock. Audio thread, never waits. */
    void publish (const int* notes, const int* bendCents, const std::int64_t* startSamples, int numStrings,
                  const Motion* motion, std::int64_t samplePosition, double sampleRate) noexcept;

    /** Any thread. False if a consistent frame could not be read this time. */
    bool read (Frame& out) const noexcept;

    std::uint32_t getSequence() const noexcept { return sequence.load (std::memory_order_acquire); }

    /** animated-strings.md 4.1: one string's level or fret from the latest publish,
        a single atomic read (LuthierEngine::getStringLevel / getStringFret). */
    float readLevel (int s) const noexcept;
    float readFret (int s) const noexcept;

    //==========================================================================
    static std::uint64_t pack (int note, int bendCents, std::int64_t startSample) noexcept;
    static String unpack (std::uint64_t word) noexcept;

private:
    struct Buffer
    {
        std::array<std::atomic<std::uint64_t>, 2> bits {};
        std::array<std::atomic<std::uint64_t>, kMaxStrings> strings {};
        std::atomic<int> numStrings { 0 };

        // animated-strings.md 4.1.
        std::array<std::atomic<float>, kMaxStrings> level {}, stopFret {}, pushCents {}, pluckPosition {}, fret {};
        std::array<std::atomic<std::int64_t>, kMaxStrings> exciteSample {};
        std::array<std::atomic<std::uint32_t>, kMaxStrings> flags {};   ///< damping | harmonic << 8 | stop kind << 16
        std::atomic<std::int64_t> samplePosition { 0 };
        std::atomic<double> sampleRate { 44100.0 };
    };

    std::array<Buffer, 2> buffers;
    std::atomic<std::uint32_t> sequence { 0 };
};

} // namespace luthier
