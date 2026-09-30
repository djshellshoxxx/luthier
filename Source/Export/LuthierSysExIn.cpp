#include "LuthierSysExIn.h"

namespace luthier
{

void LuthierSysExIn::read (juce::uint8* data, int n) noexcept
{
    int start1, size1, start2, size2;
    fifo.prepareToRead (n, start1, size1, start2, size2);

    std::memcpy (data, ring.data() + start1, (size_t) size1);

    if (size2 > 0)
        std::memcpy (data + size1, ring.data() + start2, (size_t) size2);

    fifo.finishedRead (size1 + size2);
}

void LuthierSysExIn::capture (const juce::MidiBuffer& midi) noexcept
{
    for (const auto metadata : midi)
    {
        // F0 <data> F7: the event is the data between them.
        if (metadata.numBytes < 3 || metadata.data[0] != 0xf0)
            continue;

        const auto* data = metadata.data + 1;
        const int n = metadata.numBytes - (metadata.data[metadata.numBytes - 1] == 0xf7 ? 2 : 1);

        if (! LuthierEvents::isLuthierSysEx (data, n))
            continue;

        if (n > kMaxEventBytes || fifo.getFreeSpace() < n + 2)
        {
            dropped.fetch_add (1, std::memory_order_relaxed);
            continue;
        }

        // Length prefix, then the bytes: both written before the reader sees
        // either, because the length's write is only finished with the data's.
        const juce::uint8 length[2] = { (juce::uint8) (n & 0xff), (juce::uint8) (n >> 8) };
        int start1, size1, start2, size2;
        fifo.prepareToWrite (n + 2, start1, size1, start2, size2);

        auto put = [&] (int index, juce::uint8 byte)
        {
            ring[(size_t) (index < size1 ? start1 + index : start2 + (index - size1))] = byte;
        };

        put (0, length[0]);
        put (1, length[1]);

        for (int i = 0; i < n; ++i)
            put (i + 2, data[i]);

        fifo.finishedWrite (n + 2);
    }
}

int LuthierSysExIn::drain (const std::function<void (const LuthierEvent&)>& apply)
{
    int decoded = 0;
    std::array<juce::uint8, kMaxEventBytes> buffer {};

    while (fifo.getNumReady() >= 2)
    {
        juce::uint8 length[2];
        read (length, 2);

        const int n = length[0] | (length[1] << 8);

        if (n <= 0 || n > kMaxEventBytes || fifo.getNumReady() < n)
            break;   // cannot happen with the writer above; stop rather than misread

        read (buffer.data(), n);

        LuthierEvent event;
        juce::int64 correction = 0;
        juce::String error;

        if (LuthierEvents::decodeSysEx (buffer.data(), n, event, correction, error))
        {
            apply (event);
            ++decoded;
        }
    }

    return decoded;
}

} // namespace luthier
