#pragma once

/*  Untrusted bytes into text.

    JUCE's String assumes valid UTF-8 (String::fromUTF8 only asserts it), and
    its text code - JSON, XML, trim, iteration in both directions - can read out
    of bounds on malformed sequences (found by LuthierFuzz: an XML close tag and
    a MIDI text event). Everything that arrives from outside the process - host
    state, imported or user-chosen files, MIDI meta text - becomes a String
    through here, which decodes strictly (RFC 3629: no overlongs, surrogates or
    code points above U+10FFFF) and replaces each malformed sequence with
    U+FFFD. Valid UTF-8, the normal case, is passed through unchanged.

    Any thread except the audio thread (it allocates).
*/

#include <juce_core/juce_core.h>

namespace luthier::UntrustedText
{
    /** True when the bytes are complete, well-formed UTF-8 with no NUL. */
    bool isValidUtf8 (const void* data, size_t numBytes) noexcept;

    /** Text from bytes of unknown origin; a UTF-8 byte-order mark is dropped.
        Stops at the first NUL, as a C string would. */
    juce::String fromBytes (const void* data, size_t numBytes);

    /** The whole file as text, or an empty string if it cannot be read. */
    juce::String loadFile (const juce::File& file);

    /** A String that may hold malformed UTF-8 (e.g. built by a library from raw
        bytes), made safe. Returns `text` itself when it is already valid. */
    juce::String sanitise (const juce::String& text);
}
