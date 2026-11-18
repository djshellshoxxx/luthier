#!/usr/bin/env python3
"""Apply/check Luthier's narrow JUCE 8.0.10 untrusted-text patches, each pinned to the exact
8.0.10 code block it replaces (applied once; refuses any other source).

Found by LuthierFuzz (Source/Fuzz). Both affect every parse of host state,
presets, tunes and imported files, which are untrusted input:

1. juce_CharPointer_UTF8.h, operator++: a lead byte advanced over as many bytes
   as it announced, continuation or not, so malformed UTF-8 stepped over the
   string's terminator (heap-buffer-overflow in XmlDocument on a MusicXML file).
   It now stops at the first non-continuation byte, as operator* already does.
   Valid UTF-8 is unaffected.
2. juce_JSON.cpp, parseNumber: an integer literal longer than int64 overflowed
   a signed multiply (undefined behaviour). It is now read as a double, the
   same path a literal with a '.' or exponent takes. In-range integers are
   unaffected.
3. juce_XmlDocument.cpp, readChildElements: a processing instruction inside
   element text ("<?pi?>x") made the parser loop forever (a crafted MusicXML
   file hung the importer). It is now skipped like a comment.
4. juce_CharacterFunctions.h, readDoubleValue: the exponent of a number such
   as 1e9999999999 overflowed int. It now saturates; the value is unchanged
   (infinity or zero either way).
5. juce_JSON.cpp, JSONParser: recursion depth is bounded (256); deeply nested
   brackets in a session or preset overflowed the stack.
6. juce_XmlDocument.cpp, readEntity: a hex character reference ended by an
   illegal digit or the terminator ("&#x" at the end) read past the text.

The upstream licence is retained. Call during configuration, never from audio.
"""
import argparse
import hashlib
import os
from pathlib import Path
import stat
import sys
import tempfile

PATCHES = [
    {
        "relative": Path("modules/juce_core/text/juce_CharPointer_UTF8.h"),
        "before": b"""            while ((static_cast<uint8> (n) & bit) != 0 && bit > 0x8)
            {
                ++data;
                bit = static_cast<uint8> (bit >> 1);
            }""",
        "after": b"""            while ((static_cast<uint8> (n) & bit) != 0 && bit > 0x8)
            {
                // Luthier (JUCE 8.0.10 patch): never step over a byte that is not a
                // continuation - malformed UTF-8 must not skip the terminator.
                if ((static_cast<uint8> (*data) & 0xc0) != 0x80)
                    break;

                ++data;
                bit = static_cast<uint8> (bit >> 1);
            }""",
    },
    {
        "relative": Path("modules/juce_core/json/juce_JSON.cpp"),
        "before": b"""            if (isPositiveAndBelow (digit, 10))
            {
                intValue = intValue * 10 + digit;
                continue;
            }""",
        "after": b"""            if (isPositiveAndBelow (digit, 10))
            {
                // Luthier (JUCE 8.0.10 patch): a literal beyond int64 is read as a
                // double instead of overflowing a signed multiply. intValue is
                // negative when the literal began "--" (the first '-' chose
                // parseNumber, the second was read as a digit).
                if (intValue < 0 || intValue > (std::numeric_limits<int64>::max() - digit) / 10)
                {
                    currentLocation = originalPos;
                    auto asDouble = CharacterFunctions::readDoubleValue (currentLocation);
                    return var (isNegative ? -asDouble : asDouble);
                }

                intValue = intValue * 10 + digit;
                continue;
            }""",
    },
    {
        "relative": Path("modules/juce_core/xml/juce_XmlDocument.cpp"),
        "before": b"""                        input += closeComment + 3;
                        continue;
                    }

                    break;
                }""",
        "after": b"""                        input += closeComment + 3;
                        continue;
                    }

                    // Luthier (JUCE 8.0.10 patch): a processing instruction inside
                    // text is skipped like a comment. Without this, "<?pi?>x" in an
                    // element looped forever: skipNextWhiteSpace stepped over the
                    // instruction, the text block rolled back to before it and
                    // stopped at its '<' having read nothing.
                    if (input[1] == '?')
                    {
                        input += 2;
                        auto closeInstruction = input.indexOf (CharPointer_ASCII ("?>"));

                        if (closeInstruction < 0)
                        {
                            setLastError ("unterminated processing instruction", false);
                            outOfData = true;
                            return;
                        }

                        input += closeInstruction + 2;
                        continue;
                    }

                    break;
                }""",
    },
    {
        "relative": Path("modules/juce_core/text/juce_CharacterFunctions.h"),
        "before": b"""                if (digit != 0 || exponent != 0)
                    exponent = (exponent * 10) + digit;""",
        "after": b"""                // Luthier (JUCE 8.0.10 patch): saturate instead of overflowing int.
                // Any exponent this large already yields infinity or zero.
                if ((digit != 0 || exponent != 0) && exponent < 100000)
                    exponent = (exponent * 10) + digit;""",
    },
    {
        "relative": Path("modules/juce_core/json/juce_JSON.cpp"),
        "before": b"""    String::CharPointerType startLocation, currentLocation;
""",
        "after": b"""    String::CharPointerType startLocation, currentLocation;

    // Luthier (JUCE 8.0.10 patch): bounded nesting. The parser recurses once per
    // '[' or '{', so a session or preset of nested brackets overflowed the stack.
    int nestingDepth = 0;
    static constexpr int maxNestingDepth = 256;

    struct NestingScope
    {
        NestingScope (JSONParser& parserIn, String::CharPointerType location) : parser (parserIn)
        {
            if (++parser.nestingDepth > maxNestingDepth)
            {
                --parser.nestingDepth;
                parser.throwError ("Nesting too deep", location);
            }
        }

        ~NestingScope() { --parser.nestingDepth; }

        JSONParser& parser;
    };
""",
    },
    {
        "relative": Path("modules/juce_core/json/juce_JSON.cpp"),
        "before": b"""            case '{':    return parseObject();
            case '[':    return parseArray();
""",
        "after": b"""            case '{':    { const NestingScope scope (*this, originalLocation); return parseObject(); }
            case '[':    { const NestingScope scope (*this, originalLocation); return parseArray(); }
""",
    },
    {
        "relative": Path("modules/juce_core/xml/juce_XmlDocument.cpp"),
        "before": b"""                charCode = (charCode << 4) | hexValue;
                ++input;
            }

            ++input;
        }""",
        "after": b"""                charCode = (charCode << 4) | hexValue;
                ++input;
            }

            // Luthier (JUCE 8.0.10 patch): step over the ';' only. After an
            // illegal digit - including the terminator, as in a trailing
            // "&#x" - this stepped past the end of the text.
            if (*input == ';')
                ++input;
        }""",
    },
]


def apply_one(root, patch, check):
    target = root / patch["relative"]
    original = target.read_bytes()
    canonical = original.replace(b"\r\n", b"\n")
    if canonical.count(patch["after"]) == 1:
        return f"{patch['relative'].name}: verified"
    if canonical.count(patch["before"]) != 1:
        raise ValueError(f"{patch['relative']}: source differs from the pinned JUCE 8.0.10 preimage; refusing to patch")
    if check:
        raise ValueError(f"{patch['relative']}: untrusted-text patch is not applied")
    patched = canonical.replace(patch["before"], patch["after"], 1)
    if b"\r\n" in original:
        patched = patched.replace(b"\n", b"\r\n")
    mode = stat.S_IMODE(target.stat().st_mode)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=target.parent, prefix=".luthier-text-", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(patched)
            stream.flush()
            os.fsync(stream.fileno())
        temporary.chmod(mode)
        os.replace(temporary, target)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return f"{patch['relative'].name}: applied"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("juce_root", type=Path)
    parser.add_argument("--check", action="store_true", help="Require the patches without writing")
    args = parser.parse_args()
    try:
        results = [apply_one(args.juce_root, patch, args.check) for patch in PATCHES]
        print("JUCE 8.0.10 untrusted-text patches: " + ", ".join(results))
        return 0
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
