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
    publish (notes, bendCents, startSamples, numStrings, nullptr, 0, 44100.0);
}

void SoundingNotes::publish (const int* notes, const int* bendCents, const std::int64_t* startSamples, int numStrings,
                             const Motion* motion, std::int64_t samplePosition, double sampleRate) noexcept
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

    // animated-strings.md 4.1: the display's record, in the same buffer.
    for (int s = 0; s < n; ++s)
    {
        const Motion m = motion != nullptr ? motion[s] : Motion {};
        target.level[(size_t) s].store (m.level, std::memory_order_relaxed);
        target.stopFret[(size_t) s].store (m.stopFret, std::memory_order_relaxed);
        target.pushCents[(size_t) s].store (m.pushCents, std::memory_order_relaxed);
        target.pluckPosition[(size_t) s].store (m.pluckPosition, std::memory_order_relaxed);
        target.fret[(size_t) s].store (m.fret, std::memory_order_relaxed);
        target.exciteSample[(size_t) s].store (m.exciteSample, std::memory_order_relaxed);
        target.flags[(size_t) s].store ((std::uint32_t) m.damping | ((std::uint32_t) m.harmonicPartial << 8)
                                          | ((std::uint32_t) m.stopKind << 16), std::memory_order_relaxed);
    }

    target.samplePosition.store (samplePosition, std::memory_order_relaxed);
    target.sampleRate.store (sampleRate, std::memory_order_relaxed);

    target.bits[0].store (bits[0], std::memory_order_relaxed);
    target.bits[1].store (bits[1], std::memory_order_relaxed);
    target.numStrings.store (n, std::memory_order_relaxed);

    sequence.store (seq + 1, std::memory_order_release);
}

float SoundingNotes::readLevel (int s) const noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return 0.0f;

    return buffers[(size_t) (sequence.load (std::memory_order_acquire) & 1u)].level[(size_t) s].load (std::memory_order_relaxed);
}

float SoundingNotes::readFret (int s) const noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return 0.0f;

    return buffers[(size_t) (sequence.load (std::memory_order_acquire) & 1u)].fret[(size_t) s].load (std::memory_order_relaxed);
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

        // animated-strings.md 4.1.
        for (int s = 0; s < f.numStrings; ++s)
        {
            auto& m = f.motion[(size_t) s];
            m.level = source.level[(size_t) s].load (std::memory_order_relaxed);
            m.stopFret = source.stopFret[(size_t) s].load (std::memory_order_relaxed);
            m.pushCents = source.pushCents[(size_t) s].load (std::memory_order_relaxed);
            m.pluckPosition = source.pluckPosition[(size_t) s].load (std::memory_order_relaxed);
            m.fret = source.fret[(size_t) s].load (std::memory_order_relaxed);
            m.exciteSample = source.exciteSample[(size_t) s].load (std::memory_order_relaxed);
            const auto flags = source.flags[(size_t) s].load (std::memory_order_relaxed);
            m.damping = (std::uint8_t) (flags & 0xff);
            m.harmonicPartial = (std::uint8_t) ((flags >> 8) & 0xff);
            m.stopKind = (std::uint8_t) ((flags >> 16) & 0xff);
        }

        f.samplePosition = source.samplePosition.load (std::memory_order_relaxed);
        f.sampleRate = source.sampleRate.load (std::memory_order_relaxed);

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
