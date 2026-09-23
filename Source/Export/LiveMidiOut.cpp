#include "LiveMidiOut.h"

#include <cmath>

namespace luthier
{

//==============================================================================
namespace LiveMidiClock
{

double ppqToSampleExact (double ppq, juce::int64 blockStartSample, double blockStartPpq,
                         double bpm, double sampleRate) noexcept
{
    const double samplesPerQuarter = sampleRate * 60.0 / juce::jmax (1.0e-6, bpm);
    return (double) blockStartSample + (ppq - blockStartPpq) * samplesPerQuarter;
}

juce::int64 ppqToSample (double ppq, juce::int64 blockStartSample, double blockStartPpq,
                         double bpm, double sampleRate) noexcept
{
    return (juce::int64) std::llround (ppqToSampleExact (ppq, blockStartSample, blockStartPpq, bpm, sampleRate));
}

double sampleToPpq (juce::int64 sample, juce::int64 blockStartSample, double blockStartPpq,
                    double bpm, double sampleRate) noexcept
{
    return blockStartPpq + (double) (sample - blockStartSample) * juce::jmax (1.0e-6, bpm)
                             / (60.0 * juce::jmax (1.0, sampleRate));
}

int offsetInBlock (juce::int64 sample, juce::int64 blockStartSample, int numSamples) noexcept
{
    const auto offset = sample - blockStartSample;
    return (offset >= 0 && offset < (juce::int64) numSamples) ? (int) offset : -1;
}

} // namespace LiveMidiClock

//==============================================================================
namespace
{
    /** Writes into a fixed buffer and notes, rather than overruns, when it is full. */
    struct ByteWriter
    {
        juce::uint8* data = nullptr;
        int capacity = 0;
        int size = 0;
        bool overflowed = false;

        void put (juce::uint8 byte) noexcept
        {
            if (size < capacity)
                data[size++] = byte;
            else
                overflowed = true;
        }

        void put (const char* text) noexcept
        {
            if (text != nullptr)
                for (; *text != 0; ++text)
                    put ((juce::uint8) *text);
        }

        /** LuthierEvents::escapeValue's rule, byte by byte, so the file and
            the wire agree on every value. */
        void putEscaped (const char* text) noexcept
        {
            static constexpr const char* hex = "0123456789ABCDEF";

            if (text == nullptr)
                return;

            for (auto* p = reinterpret_cast<const unsigned char*> (text); *p != 0; ++p)
            {
                const auto c = *p;

                if (c > 0x20 && c < 0x7F && c != '%' && c != '=')
                {
                    put ((juce::uint8) c);
                }
                else
                {
                    put ((juce::uint8) '%');
                    put ((juce::uint8) hex[c >> 4]);
                    put ((juce::uint8) hex[c & 0x0F]);
                }
            }
        }

        void putInt (juce::int64 value) noexcept
        {
            char digits[24];
            int count = 0;

            const bool negative = value < 0;

            // -(value + 1) + 1 keeps the most negative value in range.
            auto magnitude = negative ? (juce::uint64) (-(value + 1)) + 1u : (juce::uint64) value;

            do
            {
                digits[count++] = (char) ('0' + (int) (magnitude % 10u));
                magnitude /= 10u;
            }
            while (magnitude != 0 && count < 24);

            if (negative)
                put ((juce::uint8) '-');

            while (count > 0)
                put ((juce::uint8) digits[--count]);
        }

        /** Six decimal places with the trailing zeros dropped: exact enough
            for a live control, and free of the allocation that
            LuthierEvents::formatReal costs. */
        void putReal (double value) noexcept
        {
            if (! std::isfinite (value))
            {
                put ((juce::uint8) '0');
                return;
            }

            value = juce::jlimit (-1.0e15, 1.0e15, value);

            const double magnitude = std::abs (value);
            auto whole = (juce::int64) std::floor (magnitude);
            auto micro = (juce::int64) std::llround ((magnitude - (double) whole) * 1.0e6);

            if (micro >= 1000000)
            {
                ++whole;
                micro -= 1000000;
            }

            if (value < 0.0 && (whole != 0 || micro != 0))
                put ((juce::uint8) '-');

            putInt (whole);

            if (micro == 0)
                return;

            char fraction[6];

            for (int i = 5; i >= 0; --i)
            {
                fraction[i] = (char) ('0' + (int) (micro % 10));
                micro /= 10;
            }

            int length = 6;

            while (length > 0 && fraction[length - 1] == '0')
                --length;

            put ((juce::uint8) '.');

            for (int i = 0; i < length; ++i)
                put ((juce::uint8) fraction[i]);
        }
    };
}

//==============================================================================
LuthierSysExOut::Field LuthierSysExOut::Field::makeWord (const char* fieldKey, const char* value) noexcept
{
    Field field;
    field.key = fieldKey;
    field.kind = Kind::word;
    field.word = value;
    return field;
}

LuthierSysExOut::Field LuthierSysExOut::Field::makeInt (const char* fieldKey, juce::int64 value) noexcept
{
    Field field;
    field.key = fieldKey;
    field.kind = Kind::integer;
    field.integer = value;
    return field;
}

LuthierSysExOut::Field LuthierSysExOut::Field::makeReal (const char* fieldKey, double value) noexcept
{
    Field field;
    field.key = fieldKey;
    field.kind = Kind::real;
    field.real = value;
    return field;
}

bool LuthierSysExOut::push (LuthierEventClass eventClass, int sampleOffset,
                            const Field* fields, int numFields, int part) noexcept
{
    if (numPending >= kMaxEvents || eventClass == LuthierEventClass::unknown
          || numFields < 0 || numFields > kMaxFields || (numFields > 0 && fields == nullptr))
    {
        dropped.fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    auto& slot = pending[(size_t) numPending];

    ByteWriter out;
    out.data = slot.bytes.data();
    out.capacity = kMaxEventBytes;

    // F0 7D 'L' 'T' <wire version> <payload> <checksum> F7: LuthierEvents::encodeSysEx's layout.
    out.put ((juce::uint8) 0xF0);
    out.put (LuthierEvents::kManufacturerId);
    out.put ((juce::uint8) 'L');
    out.put ((juce::uint8) 'T');
    out.put (LuthierEvents::kWireVersion);

    const int payloadStart = out.size;

    out.put (LuthierEvents::getClassName (eventClass));
    out.put ((juce::uint8) ' ');
    out.putInt (LuthierEvents::getSchemaVersion (eventClass));

    if (part != 0)
    {
        out.put (" part=");
        out.putInt (juce::jlimit (0, 15, part));
    }

    for (int i = 0; i < numFields; ++i)
    {
        const auto& field = fields[i];

        if (field.key == nullptr)
            continue;

        out.put ((juce::uint8) ' ');
        out.put (field.key);
        out.put ((juce::uint8) '=');

        switch (field.kind)
        {
            case Field::Kind::word:    out.putEscaped (field.word); break;
            case Field::Kind::integer: out.putInt (field.integer);  break;
            case Field::Kind::real:    out.putReal (field.real);    break;
            default:                   break;
        }
    }

    const int payloadEnd = juce::jmin (out.size, kMaxEventBytes);

    out.put (LuthierEvents::checksum (slot.bytes.data() + payloadStart, (size_t) (payloadEnd - payloadStart)));
    out.put ((juce::uint8) 0xF7);

    if (out.overflowed)
    {
        dropped.fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    slot.sampleOffset = juce::jmax (0, sampleOffset);
    slot.numBytes = out.size;
    ++numPending;
    return true;
}

void LuthierSysExOut::appendTo (juce::MidiBuffer& destination, int numSamples) noexcept
{
    const int lastSample = juce::jmax (0, numSamples - 1);

    for (int i = 0; i < numPending; ++i)
    {
        const auto& slot = pending[(size_t) i];
        destination.addEvent (slot.bytes.data(), slot.numBytes, juce::jlimit (0, lastSample, slot.sampleOffset));
    }

    numPending = 0;
}

} // namespace luthier
