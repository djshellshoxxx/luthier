#pragma once

/*  Performance Assist's "show what it did" feed (auto-articulation.md 7.3).

    A 128-entry single-producer, single-consumer ring of POD entries. The audio
    thread pushes from triggerNote, the mute lift and the vibrato start; the UI
    drains it on the fretboard's existing 30 Hz timer. When the UI falls behind
    the oldest entries are overwritten, which is right for a display: the
    newest decisions are the ones worth drawing. No allocation, no lock.
*/

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>

namespace luthier
{

/** What a fretboard label says (7.3). */
enum class AssistLabel : juce::uint8
{
    none = 0,
    hammerOn,     ///< H
    pullOff,      ///< P
    slideUp,      ///< /
    slideDown,    ///< backslash
    vibrato,      ///< ~
    palmMute,     ///< PM
    upStroke,     ///< up arrow
    accent,       ///< >
    bendHalf,     ///< b1/2
    bendWhole,    ///< b1
    fall,         ///< down-right arrow
    strumDown,    ///< a vertical arrow over the struck strings
    strumUp,
    slideIn,      ///< / into the note
    muteLift,     ///< the palm lifted (no glyph of its own; lists as "mute lift")
    numLabels
};

/** "H", "P", "/", ... as drawn; UTF-8. */
const char* getAssistLabelGlyph (AssistLabel) noexcept;

/** "hammer-on", "pull-off", ... as the decision list reads them (8). */
const char* getAssistLabelName (AssistLabel) noexcept;

struct AutoArticulationFeedEntry
{
    juce::int64 sample = 0;        ///< absolute, on the engine's clock
    juce::int8 string = -1;        ///< -1 for a strum, which spans stringMask
    float fret = 0.0f;
    juce::uint8 label = 0;         ///< AssistLabel
    juce::uint8 rule = 0;          ///< the aa_rules bit index that decided it
    juce::uint16 stringMask = 0;   ///< a strum's struck strings
};

class AutoArticulationFeed
{
public:
    static constexpr int kCapacity = 128;

    /** Audio thread. */
    void push (const AutoArticulationFeedEntry& e) noexcept
    {
        const auto w = writeIndex.load (std::memory_order_relaxed);
        entries[(size_t) (w % (juce::uint64) kCapacity)] = e;
        writeIndex.store (w + 1, std::memory_order_release);
    }

    /** UI thread: calls fn for each entry not yet read, oldest first. Entries
        the writer lapped are skipped. Returns the number delivered. */
    template <typename Fn>
    int drain (Fn&& fn)
    {
        const auto w = writeIndex.load (std::memory_order_acquire);

        if (w - readIndex > (juce::uint64) kCapacity)
            readIndex = w - (juce::uint64) kCapacity;

        int n = 0;

        for (; readIndex < w; ++readIndex, ++n)
        {
            const auto e = entries[(size_t) (readIndex % (juce::uint64) kCapacity)];

            // Lapped while we read it: the entry is newer than the one we wanted.
            if (writeIndex.load (std::memory_order_acquire) - readIndex > (juce::uint64) kCapacity)
                continue;

            fn (e);
        }

        return n;
    }

    juce::uint64 getWriteCount() const noexcept { return writeIndex.load (std::memory_order_acquire); }

    /** Message thread, with the audio thread not pushing (prepare / tests). */
    void clear() noexcept
    {
        readIndex = writeIndex.load (std::memory_order_acquire);
    }

private:
    std::array<AutoArticulationFeedEntry, kCapacity> entries {};
    std::atomic<juce::uint64> writeIndex { 0 };
    juce::uint64 readIndex = 0;
};

} // namespace luthier
