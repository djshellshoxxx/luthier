#pragma once

/*  Retrospective MIDI capture (build spec, "MIDI capture / retrospective record").

    Keeps the last N seconds of incoming MIDI in a lock-free ring, so that when a
    player hits something good without having pressed record, "Save last take"
    still gets it. Writing the .mid file happens on the message thread.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>

namespace luthier
{

class MidiCapture
{
public:
    void prepare (double sampleRate, double bufferSeconds = 60.0);
    void reset() noexcept;

    void setEnabled (bool e) noexcept { enabled.store (e); }
    bool isEnabled() const noexcept { return enabled.load(); }

    double getBufferSeconds() const noexcept { return bufferSeconds; }
    void setBufferSeconds (double seconds);

    /** Call from processBlock. Real-time safe: writes into a fixed ring. */
    void capture (const juce::MidiBuffer& midi, int64_t blockStartSample) noexcept;

    /** How much material is currently held, in seconds. */
    double getCapturedSeconds() const noexcept;

    int getEventCount() const noexcept { return (int) juce::jmin (written.load(), (int64_t) capacity); }

    //==========================================================================
    /** Builds a MIDI file from everything captured. The tempo is used for the
        file's tempo map; note timing is preserved exactly as played. */
    juce::MidiFile buildMidiFile (double tempoBpm = 120.0) const;

    /** Writes the capture to disk. Returns false if there is nothing to write. */
    bool writeToFile (const juce::File& file, double tempoBpm = 120.0) const;

    /** A default filename with a timestamp, e.g. "Luthier Take 2026-09-17 14-32-05.mid". */
    static juce::String makeDefaultFileName();

private:
    struct Event
    {
        int64_t sample = 0;
        juce::uint8 bytes[3] = {};
        int numBytes = 0;
    };

    double sr = 44100.0;
    double bufferSeconds = 60.0;
    int capacity = 0;

    std::vector<Event> ring;
    // 64-bit: an int counter wrapped negative after 2^31 events (weeks of a
    // dense CC or clock stream) and `% capacity` then indexed before the ring.
    std::atomic<int64_t> writeIndex { 0 };
    std::atomic<int64_t> written { 0 };
    std::atomic<bool> enabled { true };
    std::atomic<int64_t> newestSample { 0 };
    std::atomic<int64_t> oldestSample { 0 };
};

} // namespace luthier
