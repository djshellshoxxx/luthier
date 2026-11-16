#pragma once

#include "../Model/Playing/PlayingEvents.h"
#include "../Support/BoundedMidi.h"
#include <array>

namespace luthier
{
/** Audio-thread-only, bounded conversion from string strokes to MIDI.
    Off events identify a string, so resolve their pitch against the note that
    was actually emitted. Pending strum crossings retain their sample times.
    The output router owns final delivery and releases notes when routing stops. */
class RhythmMidiWriter
{
public:
    static constexpr int kCapacity = 128;

    RhythmMidiWriter() noexcept { held.fill (-1); }

    // A transport/chord/engine stop cancels future attacks and releases notes
    // already sent. Failed releases remain pending until output space returns.
    void cancel() noexcept { count = 0; releasing = true; }

    void disconnect() noexcept
    {
        count = 0;
        held.fill (-1);
        releasing = false;
    }

    void process (const PlayEventQueue& strokes, bool driving,
                  juce::MidiBuffer* output, int samples, int offset) noexcept
    {
        if (output == nullptr)
        {
            disconnect();
            return; // MidiOutRouter releases its actually-delivered notes.
        }
        if (samples <= 0)
            return;
        if (! driving)
            cancel();

        auto release = [&] (int string, int when)
        {
            auto& note = held[(size_t) string];
            if (note < 0)
                return true;
            const juce::uint8 bytes[] { (juce::uint8) (0x80 | string), (juce::uint8) note, 0 };
            if (! BoundedMidi::add (*output, bytes, 3, offset + when))
                return false;
            note = -1;
            return true;
        };

        auto drainReleases = [&]
        {
            bool complete = true;
            for (int string = 0; string < kMaxStrings; ++string)
                complete = release (string, 0) && complete;
            releasing = ! complete;
        };

        // A reset can leave a release obligation even with no held notes.
        // Discharge it before collecting this block's strokes so playback's
        // first attack is retained. A blocked release still suppresses attacks.
        if (releasing)
            drainReleases();
        if (releasing || ! driving)
            return;

        if (! releasing)
        {
            for (int i = 0; i < strokes.getNumNoteOffs(); ++i)
            {
                const auto& e = strokes.getNoteOff (i);
                enqueue ({e.stringIndex, 0, 0, juce::jmax (0, e.sampleOffset)});
            }
            for (int i = 0; i < strokes.getNumNoteOns(); ++i)
            {
                const auto& e = strokes.getNoteOn (i);
                if (! e.deadStrike && std::isfinite (e.velocity) && e.velocity > 0.0)
                    enqueue ({e.stringIndex, juce::jlimit (0, 127, e.midiNote),
                              juce::jlimit (1, 127, (int) std::lround (juce::jlimit (0.0, 1.0, e.velocity) * 127.0)),
                              juce::jmax (0, e.sampleOffset)});
            }
        }


        // An overflow discovered while collecting cancels these new attacks.
        if (releasing)
        {
            drainReleases();
            return;
        }

        int kept = 0;
        for (int i = 0; i < count; ++i)
        {
            auto event = pending[(size_t) i];
            if (event.when >= samples)
            {
                event.when -= samples;
                pending[(size_t) kept++] = event;
                continue;
            }
            // Re-striking a string must close its previous MIDI note first.
            if (! release (event.string, event.when))
            {
                cancel();
                return;
            }
            if (event.velocity > 0)
            {
                const juce::uint8 bytes[] { (juce::uint8) (0x90 | event.string),
                                           (juce::uint8) event.note, (juce::uint8) event.velocity };
                if (BoundedMidi::add (*output, bytes, 3, offset + event.when))
                    held[(size_t) event.string] = event.note;
            }
        }
        count = kept;
    }

private:
    struct Event { int string, note, velocity, when; };
    std::array<Event, kCapacity> pending {};
    std::array<int, kMaxStrings> held {};
    int count = 0;
    bool releasing = false;

    void enqueue (Event event) noexcept
    {
        if (releasing || ! juce::isPositiveAndBelow (event.string, kMaxStrings))
            return;
        if (count == kCapacity)
        {
            cancel(); // Overflow must never strand a note or keep stale attacks.
            return;
        }
        int at = count++;
        while (at > 0 && (pending[(size_t) at - 1].when > event.when
                         || (pending[(size_t) at - 1].when == event.when
                             && event.velocity == 0 && pending[(size_t) at - 1].velocity > 0)))
        {
            pending[(size_t) at] = pending[(size_t) at - 1];
            --at;
        }
        pending[(size_t) at] = event;
    }
};
} // namespace luthier
