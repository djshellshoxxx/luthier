#include "SoundingNotes.h"

namespace luthier
{

int SoundingNotes::Frame::countSounding() const noexcept
{
    int n = 0;

    for (auto word : noteBits)
        for (; word != 0; word &= word - 1)
            ++n;

    return n;
}

std::uint64_t SoundingNotes::pack (int note, int bendCents, std::int64_t startSample) noexcept
{
    // note + 1 in 8 bits (0 = none), bend + 128 in 8 bits, start in the low 48.
    const auto n = (std::uint64_t) juce::jlimit (0, 255, note + 1);
    const auto b = (std::uint64_t) juce::jlimit (0, 255, bendCents + 128);
    const auto s = (std::uint64_t) juce::jmax ((std::int64_t) 0, startSample) & 0xffffffffffffull;
    return (n << 56) | (b << 48) | s;
}

SoundingNotes::String SoundingNotes::unpack (std::uint64_t word) noexcept
{
    String s;
    s.note = (int) (word >> 56) - 1;
    s.bendCents = (int) ((word >> 48) & 0xff) - 128;
    s.startSample = (std::int64_t) (word & 0xffffffffffffull);
    return s;
}

void SoundingNotes::publish (const int* notes, const int* bendCents, const std::int64_t* startSamples, int numStrings) noexcept
{
    const auto seq = sequence.load (std::memory_order_relaxed);
    auto& target = buffers[(size_t) ((seq + 1) & 1u)];   // the one readers are not pointed at

    const int n = juce::jlimit (0, kMaxStrings, numStrings);
    std::array<std::uint64_t, 2> bits {};

    for (int s = 0; s < n; ++s)
    {
        const int note = notes[s];

        if (juce::isPositiveAndBelow (note, 128))
            bits[(size_t) (note >> 6)] |= (std::uint64_t) 1 << (note & 63);

        target.strings[(size_t) s].store (juce::isPositiveAndBelow (note, 128)
                                              ? pack (note, bendCents != nullptr ? bendCents[s] : 0,
                                                      startSamples != nullptr ? startSamples[s] : 0)
                                              : pack (-1, 0, 0),
                                          std::memory_order_relaxed);
    }

    target.bits[0].store (bits[0], std::memory_order_relaxed);
    target.bits[1].store (bits[1], std::memory_order_relaxed);
    target.numStrings.store (n, std::memory_order_relaxed);

    sequence.store (seq + 1, std::memory_order_release);
}

bool SoundingNotes::read (Frame& out) const noexcept
{
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const auto seq = sequence.load (std::memory_order_acquire);

        if (seq == 0)
        {
            out = Frame {};
            return true;
        }

        const auto& source = buffers[(size_t) (seq & 1u)];
        Frame f;
        f.numStrings = source.numStrings.load (std::memory_order_relaxed);
        f.noteBits[0] = source.bits[0].load (std::memory_order_relaxed);
        f.noteBits[1] = source.bits[1].load (std::memory_order_relaxed);

        for (int s = 0; s < f.numStrings; ++s)
            f.strings[(size_t) s] = unpack (source.strings[(size_t) s].load (std::memory_order_relaxed));

        f.sequence = seq;

        // The next publish writes the other buffer; the one after it rewrites
        // this one, and can only begin once the sequence has moved. An
        // unmoved sequence therefore means this copy is whole.
        std::atomic_thread_fence (std::memory_order_acquire);

        if (sequence.load (std::memory_order_relaxed) == seq)
        {
            out = f;
            return true;
        }
    }

    return false;
}

} // namespace luthier
