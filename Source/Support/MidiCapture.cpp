#include "MidiCapture.h"

namespace luthier
{

void MidiCapture::prepare (double sampleRate, double seconds)
{
    sr = sampleRate;
    setBufferSeconds (seconds);
}

void MidiCapture::setBufferSeconds (double seconds)
{
    bufferSeconds = juce::jlimit (5.0, 600.0, seconds);

    // A generous estimate of the densest playing anyone will do: 60 events per
    // second, which covers fast strumming with pitch bend on every string.
    capacity = juce::jmax (256, (int) (bufferSeconds * 60.0));

    ring.assign ((size_t) capacity, Event {});
    reset();
}

void MidiCapture::reset() noexcept
{
    writeIndex.store (0);
    written.store (0);
    newestSample.store (0);
    oldestSample.store (0);
}

//==============================================================================
void MidiCapture::capture (const juce::MidiBuffer& midi, int64_t blockStartSample) noexcept
{
    if (! enabled.load (std::memory_order_relaxed) || capacity <= 0)
        return;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int size = message.getRawDataSize();

        if (size <= 0 || size > 3)
            continue;

        const int index = writeIndex.fetch_add (1, std::memory_order_relaxed) % capacity;

        auto& e = ring[(size_t) index];
        e.sample = blockStartSample + metadata.samplePosition;
        e.numBytes = size;

        const auto* data = message.getRawData();

        for (int i = 0; i < size; ++i)
            e.bytes[i] = data[i];

        newestSample.store (e.sample, std::memory_order_relaxed);

        const int count = written.fetch_add (1, std::memory_order_relaxed) + 1;

        if (count > capacity)
        {
            // The ring has wrapped: the oldest event is now the one we are about
            // to overwrite next time round.
            const int oldestIndex = (index + 1) % capacity;
            oldestSample.store (ring[(size_t) oldestIndex].sample, std::memory_order_relaxed);
        }
    }
}

double MidiCapture::getCapturedSeconds() const noexcept
{
    const int64_t newest = newestSample.load (std::memory_order_relaxed);
    const int64_t oldest = oldestSample.load (std::memory_order_relaxed);

    if (newest <= oldest)
        return 0.0;

    return juce::jmin (bufferSeconds, (double) (newest - oldest) / sr);
}

//==============================================================================
juce::MidiFile MidiCapture::buildMidiFile (double tempoBpm) const
{
    juce::MidiFile file;

    const int ticksPerQuarter = 960;
    file.setTicksPerQuarterNote (ticksPerQuarter);

    juce::MidiMessageSequence tempoTrack;
    tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent (
        (int) (60000000.0 / juce::jmax (20.0, tempoBpm))));
    tempoTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4));
    tempoTrack.addEvent (juce::MidiMessage::textMetaEvent (3, "Luthier Capture"));
    file.addTrack (tempoTrack);

    juce::MidiMessageSequence notes;

    const int count = juce::jmin (written.load(), capacity);

    if (count <= 0)
    {
        file.addTrack (notes);
        return file;
    }

    // Walk the ring oldest-first.
    const int start = (written.load() > capacity) ? (writeIndex.load() % capacity) : 0;

    int64_t firstSample = std::numeric_limits<int64_t>::max();

    for (int i = 0; i < count; ++i)
    {
        const auto& e = ring[(size_t) ((start + i) % capacity)];

        if (e.numBytes > 0)
            firstSample = juce::jmin (firstSample, e.sample);
    }

    if (firstSample == std::numeric_limits<int64_t>::max())
        firstSample = 0;

    const double ticksPerSecond = (tempoBpm / 60.0) * ticksPerQuarter;

    for (int i = 0; i < count; ++i)
    {
        const auto& e = ring[(size_t) ((start + i) % capacity)];

        if (e.numBytes <= 0)
            continue;

        auto message = juce::MidiMessage (e.bytes, e.numBytes);
        const double seconds = (double) (e.sample - firstSample) / sr;
        message.setTimeStamp (seconds * ticksPerSecond);

        notes.addEvent (message);
    }

    notes.updateMatchedPairs();
    file.addTrack (notes);

    return file;
}

bool MidiCapture::writeToFile (const juce::File& file, double tempoBpm) const
{
    if (juce::jmin (written.load(), capacity) <= 0)
        return false;

    file.getParentDirectory().createDirectory();

    juce::TemporaryFile temp (file);

    if (auto stream = temp.getFile().createOutputStream())
    {
        const auto midiFile = buildMidiFile (tempoBpm);

        if (! midiFile.writeTo (*stream))
            return false;

        stream->flush();
        stream.reset();

        return temp.overwriteTargetFileWithTemporary();
    }

    return false;
}

juce::String MidiCapture::makeDefaultFileName()
{
    const auto now = juce::Time::getCurrentTime();

    return "Luthier Take " + now.formatted ("%Y-%m-%d %H-%M-%S") + ".mid";
}

} // namespace luthier
