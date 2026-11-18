#include "UntrustedText.h"

namespace luthier::UntrustedText
{
namespace
{
    // Length of the well-formed sequence at p (1..4), or 0 if malformed.
    int sequenceLength (const uint8_t* p, size_t remaining) noexcept
    {
        const uint8_t b0 = p[0];
        if (b0 < 0x80)
            return b0 == 0 ? 0 : 1;

        int length = 0;
        uint32_t lowest = 0, cp = 0;
        if ((b0 & 0xe0) == 0xc0)      { length = 2; lowest = 0x80;    cp = b0 & 0x1f; }
        else if ((b0 & 0xf0) == 0xe0) { length = 3; lowest = 0x800;   cp = b0 & 0x0f; }
        else if ((b0 & 0xf8) == 0xf0) { length = 4; lowest = 0x10000; cp = b0 & 0x07; }
        else
            return 0;

        if ((size_t) length > remaining)
            return 0;

        for (int i = 1; i < length; ++i)
        {
            if ((p[i] & 0xc0) != 0x80)
                return 0;
            cp = (cp << 6) | (p[i] & 0x3f);
        }

        if (cp < lowest || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            return 0;
        return length;
    }
}

bool isValidUtf8 (const void* data, size_t numBytes) noexcept
{
    const auto* p = static_cast<const uint8_t*> (data);
    for (size_t at = 0; at < numBytes;)
    {
        const int n = sequenceLength (p + at, numBytes - at);
        if (n == 0)
            return false;
        at += (size_t) n;
    }
    return true;
}

juce::String fromBytes (const void* data, size_t numBytes)
{
    if (data == nullptr || numBytes == 0)
        return {};

    const auto* p = static_cast<const uint8_t*> (data);
    if (numBytes >= 3 && p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf)
    {
        p += 3;
        numBytes -= 3;
    }

    // Up to the first NUL: the C-string view every consumer has of the text.
    numBytes = (size_t) (std::find (p, p + numBytes, uint8_t (0)) - p);

    if (isValidUtf8 (p, numBytes))
        return juce::String::fromUTF8 (reinterpret_cast<const char*> (p), (int) numBytes);

    static constexpr uint8_t replacement[] { 0xef, 0xbf, 0xbd };   // U+FFFD
    std::vector<uint8_t> clean;
    clean.reserve (numBytes + 16);
    for (size_t at = 0; at < numBytes;)
    {
        const int n = sequenceLength (p + at, numBytes - at);
        if (n == 0)
        {
            clean.insert (clean.end(), std::begin (replacement), std::end (replacement));
            ++at;   // resynchronise on the next byte
        }
        else
        {
            clean.insert (clean.end(), p + at, p + at + n);
            at += (size_t) n;
        }
    }
    return juce::String::fromUTF8 (reinterpret_cast<const char*> (clean.data()), (int) clean.size());
}

juce::String loadFile (const juce::File& file)
{
    juce::MemoryBlock bytes;
    if (! file.loadFileAsData (bytes))
        return {};

    // UTF-16 with a byte-order mark (Notepad's "Unicode"): JUCE decodes it as
    // loadFileAsString always has, then any unpaired surrogate is replaced.
    const auto* p = static_cast<const uint8_t*> (bytes.getData());
    if (bytes.getSize() >= 2 && ((p[0] == 0xff && p[1] == 0xfe) || (p[0] == 0xfe && p[1] == 0xff)))
        return sanitise (juce::String::createStringFromData (bytes.getData(), (int) bytes.getSize()));

    return fromBytes (bytes.getData(), bytes.getSize());
}

juce::String sanitise (const juce::String& text)
{
    const auto* raw = text.toRawUTF8();
    const auto size = text.getNumBytesAsUTF8();
    return isValidUtf8 (raw, size) ? text : fromBytes (raw, size);
}
}
