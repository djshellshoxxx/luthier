#include "TabTextSanitizer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace luthier
{
namespace
{
    void appendUtf8 (std::string& out, juce::juce_wchar c)
    {
        if (c < 0x80)       { out.push_back ((char) c); }
        else if (c < 0x800) { out.push_back ((char) (0xC0 | (c >> 6))); out.push_back ((char) (0x80 | (c & 0x3F))); }
        else if (c < 0x10000)
        {
            out.push_back ((char) (0xE0 | (c >> 12)));
            out.push_back ((char) (0x80 | ((c >> 6) & 0x3F)));
            out.push_back ((char) (0x80 | (c & 0x3F)));
        }
        else
        {
            out.push_back ((char) (0xF0 | (c >> 18)));
            out.push_back ((char) (0x80 | ((c >> 12) & 0x3F)));
            out.push_back ((char) (0x80 | ((c >> 6) & 0x3F)));
            out.push_back ((char) (0x80 | (c & 0x3F)));
        }
    }

    /** What an exotic character means in tab. 0 = keep, -1 = drop, else the
        ASCII replacement; `doubleBar` is signalled by returning 1 ("||"). */
    int mapGlyph (juce::juce_wchar c) noexcept
    {
        // Invisible characters.
        if (c == 0xFEFF || c == 0x200B || c == 0x200C || c == 0x200D || c == 0x2060 || c == 0x00AD
            || c == 0x200E || c == 0x200F)
            return -1;

        // Spaces.
        if (c == 0x00A0 || (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F || c == 0x3000)
            return ' ';

        // Dashes and minus signs.
        if ((c >= 0x2010 && c <= 0x2015) || c == 0x2212 || c == 0x2043 || c == 0x23AF
            || c == 0xFE58 || c == 0xFE63 || c == 0xFF0D || c == 0x2796 || c == 0x2E3A || c == 0x2E3B)
            return '-';

        // Box drawing.
        if (c >= 0x2500 && c <= 0x257F)
        {
            switch (c)
            {
                case 0x2500: case 0x2501: case 0x2504: case 0x2505: case 0x2508: case 0x2509:
                case 0x254C: case 0x254D: case 0x2550: case 0x2574: case 0x2576: case 0x257C: case 0x257E:
                    return '-';
                case 0x2502: case 0x2503: case 0x2506: case 0x2507: case 0x250A: case 0x250B:
                case 0x254E: case 0x254F: case 0x2551: case 0x2575: case 0x2577: case 0x257D: case 0x257F:
                    return '|';
                default: break;
            }

            // Corners are borders, not staff content.
            if ((c >= 0x250C && c <= 0x251B) || (c >= 0x2552 && c <= 0x255D) || (c >= 0x256D && c <= 0x2570))
                return ' ';

            return '|';   // tees and crosses are bar lines meeting a string
        }

        // Vertical bars.
        if (c == 0xFF5C || c == 0x2223 || c == 0x01C0 || c == 0x00A6 || c == 0x2758 || c == 0xFFE8)
            return '|';
        if (c == 0x2016)
            return 1;   // double vertical line

        // Full-width ASCII.
        if (c >= 0xFF10 && c <= 0xFF19) return (int) (c - 0xFF10 + '0');
        if (c >= 0xFF21 && c <= 0xFF3A) return (int) (c - 0xFF21 + 'A');
        if (c >= 0xFF41 && c <= 0xFF5A) return (int) (c - 0xFF41 + 'a');
        if (c == 0xFF0F) return '/';
        if (c == 0xFF3C) return '\\';
        if (c == 0xFF5E || c == 0x223C) return '~';
        if (c == 0xFF08) return '(';
        if (c == 0xFF09) return ')';
        if (c == 0xFF3B) return '[';
        if (c == 0xFF3D) return ']';
        if (c == 0xFF1C) return '<';
        if (c == 0xFF1E) return '>';
        if (c == 0xFF0A) return '*';
        if (c == 0xFF1A) return ':';
        if (c == 0xFF0E) return '.';

        // Symbols guitarists type.
        if (c == 0x00D7 || c == 0x2715 || c == 0x2716 || c == 0x2573) return 'x';
        if (c == 0x266F) return '#';
        if (c == 0x266D) return 'b';

        return 0;
    }

    bool isControl (juce::juce_wchar c) noexcept
    {
        return (c < 0x20 && c != '\n' && c != '\r' && c != '\t') || c == 0x7F || (c >= 0x80 && c < 0xA0)
            || c == 0xFFFD;
    }

    //==========================================================================
    struct Row
    {
        enum Kind { none, numeric, note, drum } kind = none;
        int number = 0;          // numeric label
        int pitchClass = -1;     // note label
        size_t labelStart = 0, labelEnd = 0;   // [start, end) in the line
    };

    int letterPc (char c) noexcept
    {
        switch (std::tolower ((unsigned char) c))
        {
            case 'c': return 0; case 'd': return 2; case 'e': return 4; case 'f': return 5;
            case 'g': return 7; case 'a': return 9; case 'b': return 11; default: return -1;
        }
    }

    bool looksLikeStaffBody (const std::string& line, size_t from)
    {
        int dashes = 0, junk = 0;
        for (size_t i = from; i < line.size(); ++i)
        {
            const char c = line[i];
            if (c == '-') ++dashes;
            else if (std::isalpha ((unsigned char) c) && std::strchr ("hpbrsxHPBRSXvVtT", c) == nullptr) ++junk;
        }
        return dashes >= 2 && junk * 3 <= (int) (line.size() - from);
    }

    Row classifyRow (const std::string& line)
    {
        Row row;
        size_t i = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= line.size())
            return row;

        const size_t start = i;
        const char c = line[i];

        auto afterLabel = [&] (size_t j) -> bool
        {
            while (j < line.size() && line[j] == ' ') ++j;
            if (j >= line.size() || (line[j] != '|' && line[j] != ':')) return false;
            return looksLikeStaffBody (line, j + 1);
        };

        if (c >= '1' && c <= '9')
        {
            if (afterLabel (i + 1))
            {
                row.kind = Row::numeric;
                row.number = c - '0';
                row.labelStart = start;
                row.labelEnd = i + 1;
            }
            return row;
        }

        if (letterPc (c) >= 0)
        {
            size_t j = i + 1;
            int pc = letterPc (c);

            if (j < line.size() && (line[j] == '#' || line[j] == 'b') && j + 1 < line.size()
                && (line[j + 1] == '|' || line[j + 1] == ' ' || line[j + 1] == ':'))
            {
                pc = (pc + (line[j] == '#' ? 1 : 11)) % 12;
                ++j;
            }

            if (afterLabel (j))
            {
                row.kind = Row::note;
                row.pitchClass = pc;
                row.labelStart = start;
                row.labelEnd = j;
                return row;
            }
        }

        // Drum-kit rows: a 2-4 letter label that is not a note name.
        if (std::isalpha ((unsigned char) c) || std::isdigit ((unsigned char) c))
        {
            size_t j = i;
            while (j < line.size() && j - i < 5 && std::isalnum ((unsigned char) line[j])) ++j;
            const size_t len = j - i;
            if (len >= 2 && len <= 4 && afterLabel (j) && j < line.size() + 1)
            {
                row.kind = Row::drum;
                row.labelStart = start;
                row.labelEnd = j;
            }
        }

        return row;
    }

    const char* const* guitarNames() noexcept
    {
        static const char* const names[] = { "e", "B", "G", "D", "A", "E", "B", "F#" };
        return names;
    }

    const char* const* bassNames() noexcept
    {
        static const char* const names[] = { "G", "D", "A", "E", "B", "F#" };
        return names;
    }

    /** Cost of reading `pcs` top-to-bottom as high-to-low (descending intervals ~5). */
    int orientationCost (const std::vector<int>& pcs, bool descending)
    {
        int cost = 0;
        for (size_t i = 0; i + 1 < pcs.size(); ++i)
        {
            const int d = descending ? (pcs[i] - pcs[i + 1] + 12) % 12 : (pcs[i + 1] - pcs[i] + 12) % 12;
            cost += std::abs (d - 5);
        }
        return cost;
    }

    void processRun (std::vector<std::string>& lines, size_t begin, size_t end,
                     const std::vector<Row>& rows, bool bassHint, TabImportDiagnostics& d)
    {
        const size_t n = end - begin;
        if (n < 3)
            return;

        // Drum rows: blank them.
        int drumRows = 0;
        for (size_t i = begin; i < end; ++i)
            if (rows[i].kind == Row::drum)
                ++drumRows;

        if (drumRows >= 2)
        {
            for (size_t i = begin; i < end; ++i)
                if (rows[i].kind == Row::drum)
                {
                    lines[i].clear();
                    ++d.drumLinesSkipped;
                }
            return;
        }

        int numeric = 0, notes = 0;
        for (size_t i = begin; i < end; ++i)
        {
            numeric += rows[i].kind == Row::numeric;
            notes += rows[i].kind == Row::note;
        }

        if (numeric == (int) n)
        {
            // Numbered strings: 1 is the highest.
            const int first = rows[begin].number, last = rows[end - 1].number;
            const bool lowFirst = first > last;
            const int count = juce::jlimit (3, 8, juce::jmax (first, last));
            const bool bass = count == 4 || count == 5 || (bassHint && count < 6);
            const char* const* names = bass ? bassNames() : guitarNames();

            for (size_t i = begin; i < end; ++i)
            {
                const int idx = juce::jlimit (1, 8, rows[i].number) - 1;
                std::string& l = lines[i];
                l = l.substr (0, rows[i].labelStart) + names[idx] + l.substr (rows[i].labelEnd);
                ++d.stringLabelsRewritten;
            }

            if (lowFirst)
            {
                std::reverse (lines.begin() + (std::ptrdiff_t) begin, lines.begin() + (std::ptrdiff_t) end);
                ++d.systemsReversed;
            }
            return;
        }

        if (notes == (int) n && n >= 4 && n <= 8)
        {
            std::vector<int> pcs;
            for (size_t i = begin; i < end; ++i)
                pcs.push_back (rows[i].pitchClass);

            const int down = orientationCost (pcs, true);
            const int up = orientationCost (pcs, false);

            if (up + 2 <= down)
            {
                std::reverse (lines.begin() + (std::ptrdiff_t) begin, lines.begin() + (std::ptrdiff_t) end);
                ++d.systemsReversed;
            }
        }
    }
}

//==============================================================================
bool TabTextSanitizer::sanitize (const juce::String& source, juce::String& cleaned,
                                 TabImportDiagnostics& d, const Options& options)
{
    std::string text;
    text.reserve ((size_t) source.getNumBytesAsUTF8() + 16);

    int controls = 0, total = 0, column = 0;
    bool skippingLine = false;
    int truncated = 0;

    auto p = source.getCharPointer();

    while (! p.isEmpty())
    {
        juce::juce_wchar c = p.getAndAdvance();
        ++total;

        if (c == '\r')
        {
            if (p.getAddress() != nullptr && *p == '\n')
                p.getAndAdvance();
            c = '\n';
        }

        if (c == '\n')
        {
            text.push_back ('\n');
            column = 0;
            skippingLine = false;
            continue;
        }

        if (skippingLine)
            continue;

        if (c == '\t')
        {
            const int next = (column / 8 + 1) * 8;
            text.append ((size_t) (next - column), ' ');
            column = next;
        }
        else
        {
            if (isControl (c))
            {
                ++controls;
                continue;
            }

            const int mapped = mapGlyph (c);
            if (mapped == -1)
                continue;

            if (mapped == 1)
            {
                text += "||";
                column += 2;
                ++d.unicodeGlyphsMapped;
            }
            else if (mapped > 1)
            {
                text.push_back ((char) mapped);
                ++column;
                ++d.unicodeGlyphsMapped;
            }
            else
            {
                appendUtf8 (text, c);
                ++column;
            }
        }

        if (column >= options.maxLineChars)
        {
            skippingLine = true;
            ++truncated;
        }
    }

    if (total > 16 && controls > (int) (options.binaryFraction * total))
    {
        d.warnings.add ("That looks like a binary file, not text tablature.");
        cleaned = {};
        return false;
    }

    if (truncated > 0)
    {
        d.linesTruncated += truncated;
        d.warnings.add (juce::String (truncated) + " very long line(s) were cut to " + juce::String (options.maxLineChars)
                        + " characters.");
    }

    // Row-level fixes.
    std::vector<std::string> lines;
    {
        size_t from = 0;
        while (from <= text.size())
        {
            const size_t nl = text.find ('\n', from);
            if (nl == std::string::npos)
            {
                lines.emplace_back (text, from);
                break;
            }
            lines.emplace_back (text, from, nl - from);
            from = nl + 1;
        }
    }

    const bool bassHint = [&]
    {
        for (const auto& l : lines)
        {
            if (l.size() < 200 && l.size() >= 4)
            {
                std::string lower = l;
                std::transform (lower.begin(), lower.end(), lower.begin(), [] (unsigned char ch) { return (char) std::tolower (ch); });
                if (lower.find ("bass") != std::string::npos)
                    return true;
            }
        }
        return false;
    }();

    std::vector<Row> rows (lines.size());
    for (size_t i = 0; i < lines.size(); ++i)
        if (lines[i].size() < (size_t) options.maxLineChars)
            rows[i] = classifyRow (lines[i]);

    size_t i = 0;
    while (i < lines.size())
    {
        if (rows[i].kind == Row::none)
        {
            ++i;
            continue;
        }

        size_t j = i;
        while (j < lines.size() && rows[j].kind != Row::none)
            ++j;

        const size_t len = j - i;
        if (len >= 3 && len <= 8)
            processRun (lines, i, j, rows, bassHint, d);
        else if (len > 8)
        {
            // Stacked systems without blank lines between them.
            const size_t chunk = len % 6 == 0 ? 6 : len % 4 == 0 ? 4 : len % 7 == 0 ? 7 : 0;
            for (size_t k = i; chunk != 0 && k + chunk <= j; k += chunk)
                processRun (lines, k, k + chunk, rows, bassHint, d);
        }

        i = j;
    }

    if (d.drumLinesSkipped > 0)
        d.warnings.add (juce::String (d.drumLinesSkipped) + " drum-kit row(s) were ignored; Luthier plays guitar and bass tab.");
    if (d.systemsReversed > 0)
        d.warnings.add (juce::String (d.systemsReversed) + " system(s) were written lowest string first and were flipped.");

    std::string joined;
    joined.reserve (text.size());
    for (size_t k = 0; k < lines.size(); ++k)
    {
        joined += lines[k];
        if (k + 1 < lines.size())
            joined.push_back ('\n');
    }

    cleaned = juce::String::fromUTF8 (joined.c_str(), (int) joined.size());
    return true;
}

} // namespace luthier
