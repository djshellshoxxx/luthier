#include "TabDocumentNormalizer.h"
#include "AsciiTabReader.h"

#include <algorithm>

namespace luthier
{
namespace
{
    int countAndReplace (juce::String& text, const juce::String& needle,
                         const juce::String& replacement)
    {
        if (needle.isEmpty())
            return 0;

        int count = 0;
        int from = 0;
        while ((from = text.indexOf (from, needle)) >= 0)
        {
            ++count;
            from += needle.length();
        }

        if (count > 0)
            text = text.replace (needle, replacement);
        return count;
    }

    juce::String stripWholeLineMarkdown (juce::String line, int& removed)
    {
        const auto trimmed = line.trim();
        for (const auto marker : { juce::String ("**"), juce::String ("__") })
        {
            if (trimmed.startsWith (marker) && trimmed.endsWith (marker)
                && trimmed.length() >= marker.length() * 2)
            {
                const auto inner = trimmed.substring (marker.length(),
                                                      trimmed.length() - marker.length());
                const int leading = line.indexOf (trimmed);
                line = line.substring (0, juce::jmax (0, leading)) + inner;
                ++removed;
                break;
            }
        }
        return line;
    }

    int firstIntegerAfter (const juce::String& text, const juce::String& marker)
    {
        const int start = text.toLowerCase().indexOf (marker.toLowerCase());
        if (start < 0)
            return -1;

        const auto tail = text.substring (start + marker.length());
        for (int i = 0; i < tail.length(); ++i)
        {
            if (juce::CharacterFunctions::isDigit (tail[i]))
            {
                int j = i;
                while (j < tail.length() && juce::CharacterFunctions::isDigit (tail[j]))
                    ++j;
                return tail.substring (i, j).getIntValue();
            }
        }
        return -1;
    }

    bool sameTuning (const std::vector<int>& a, const std::vector<int>& b)
    {
        return a == b;
    }

    std::vector<int> namedTuning (const juce::String& lower, juce::String& name)
    {
        if (lower.contains ("dadgad"))
        {
            name = "DADGAD";
            return { 62, 57, 55, 50, 45, 38 };
        }
        if (lower.contains ("drop d"))
        {
            name = "Drop D";
            return { 64, 59, 55, 50, 45, 38 };
        }
        if (lower.contains ("drop c"))
        {
            name = "Drop C";
            return { 62, 57, 53, 48, 43, 36 };
        }
        if (lower.contains ("open g"))
        {
            name = "Open G";
            return { 62, 59, 55, 50, 43, 38 };
        }
        if (lower.contains ("half step down") || lower.contains ("half-step down")
            || lower.contains ("1/2 step down") || lower.contains ("tuned down 1/2"))
        {
            name = "Eb Standard";
            return { 63, 58, 54, 49, 44, 39 };
        }
        if (lower.contains ("whole step down") || lower.contains ("full step down"))
        {
            name = "D Standard";
            return { 62, 57, 53, 48, 43, 38 };
        }
        if (lower.contains ("standard") || lower.contains ("normal tuning")
            || lower.contains ("eadgbe"))
        {
            name = "Standard";
            return { 64, 59, 55, 50, 45, 40 };
        }
        return {};
    }

    bool lineLooksLikeTuningStatement (const juce::String& lower)
    {
        return lower.contains ("tuning") || lower.contains ("tuned ")
            || lower.contains ("drop d") || lower.contains ("drop c")
            || lower.contains ("dadgad") || lower.contains ("open g")
            || lower.contains ("half step down") || lower.contains ("half-step down")
            || lower.contains ("whole step down") || lower.contains ("normal tuning")
            || lower.contains ("standard (") || lower.contains ("standard tuning");
    }

    juce::String tuningPayload (const juce::String& line)
    {
        int split = line.indexOfChar (':');
        if (split < 0)
            split = line.indexOfChar ('=');
        if (split >= 0)
            return line.substring (split + 1).trim();

        const auto lower = line.toLowerCase();
        const int pos = lower.indexOf ("tuning");
        if (pos >= 0)
        {
            auto before = line.substring (0, pos).trim();
            auto after = line.substring (pos + 6).trim();
            if (after.isNotEmpty() && ! after.startsWithChar ('('))
                return after;
            if (before.isNotEmpty() && before.length() <= 32)
                return before;
        }

        const int open = line.indexOfChar ('(');
        const int close = line.lastIndexOfChar (')');
        if (open >= 0 && close > open)
            return line.substring (open + 1, close).trim();

        return {};
    }

    void addTuningCandidate (NormalizedTabDocument& out, const juce::String& line,
                             int sourceLine)
    {
        const auto lower = line.toLowerCase();
        if (! lineLooksLikeTuningStatement (lower))
            return;

        TabTuningCandidate candidate;
        candidate.rawText = line.trim();
        candidate.source = { sourceLine, sourceLine, 0, line.length() };
        candidate.confidence = TabConfidence::exact;

        candidate.midiHighFirst = namedTuning (lower, candidate.canonicalName);

        if (candidate.midiHighFirst.empty())
        {
            auto payload = tuningPayload (line);
            std::vector<int> parsed;
            if (payload.isNotEmpty()
                && AsciiTabReader::parseTuningNames (payload, parsed, true)
                && parsed.size() >= 3)
            {
                candidate.midiHighFirst = std::move (parsed);
                candidate.canonicalName = payload;
                candidate.explicitlyListedNotes = true;
            }
        }

        if (candidate.midiHighFirst.empty())
            return;

        out.metadata.tuningCandidates.push_back (candidate);
        ++out.diagnostics.headerLines;
    }

    void resolveTunings (NormalizedTabDocument& out)
    {
        if (out.metadata.tuningCandidates.empty())
            return;

        const auto& first = out.metadata.tuningCandidates.front();
        out.metadata.tuningMidiHighFirst = first.midiHighFirst;
        out.metadata.tuningName = first.canonicalName;
        out.metadata.numStrings = (int) first.midiHighFirst.size();

        for (size_t i = 1; i < out.metadata.tuningCandidates.size(); ++i)
        {
            const auto& c = out.metadata.tuningCandidates[i];
            if (! sameTuning (first.midiHighFirst, c.midiHighFirst))
            {
                out.metadata.tuningAmbiguous = true;
                ++out.diagnostics.metadataConflicts;
                if (out.diagnostics.warnings.size() < 24)
                    out.diagnostics.warnings.add ("Conflicting tuning declarations at lines "
                                                  + juce::String (first.source.sourceLineStart)
                                                  + " and " + juce::String (c.source.sourceLineStart));
            }
        }
    }

    bool looksLikeStaffLine (const juce::String& line)
    {
        const auto trimmed = line.trimStart();
        const int bar = trimmed.indexOfChar ('|');
        if (bar < 0 || bar > 4)
            return false;

        int staffChars = 0;
        for (int i = bar + 1; i < trimmed.length(); ++i)
        {
            const auto c = trimmed[i];
            if (c == '-' || c == '|' || juce::CharacterFunctions::isDigit (c)
                || c == 'x' || c == 'X' || c == 'h' || c == 'p' || c == 'b'
                || c == '/' || c == '\\' || c == '~' || c == '^' || c == ' ')
                ++staffChars;
        }
        return staffChars >= juce::jmax (3, trimmed.length() - bar - 3);
    }
}

bool TabDocumentNormalizer::normalize (const juce::String& source,
                                       NormalizedTabDocument& out) const
{
    return normalize (source, out, Options {});
}

bool TabDocumentNormalizer::normalize (const juce::String& source,
                                       NormalizedTabDocument& out,
                                       const Options& options) const
{
    out = {};
    out.originalText = source;

    if (source.getNumBytesAsUTF8() > options.maxInputBytes)
    {
        out.diagnostics.warnings.add ("Tab source exceeds the configured input-size limit");
        return false;
    }

    juce::String cleaned = source.replace ("\r\n", "\n").replace ("\r", "\n");

    out.diagnostics.entitiesDecoded += countAndReplace (cleaned, "&#x20;", " ");
    out.diagnostics.entitiesDecoded += countAndReplace (cleaned, "&#xA0;", " ");
    out.diagnostics.entitiesDecoded += countAndReplace (cleaned, "&#xa0;", " ");
    out.diagnostics.entitiesDecoded += countAndReplace (cleaned, "&nbsp;", " ");
    out.diagnostics.entitiesDecoded += countAndReplace (cleaned, "&lt;", "<");
    out.diagnostics.entitiesDecoded += countAndReplace (cleaned, "&gt;", ">");
    out.diagnostics.entitiesDecoded += countAndReplace (cleaned, "&amp;", "&");

    juce::StringArray lines;
    lines.addLines (cleaned);
    if (lines.size() > options.maxLines)
    {
        out.diagnostics.warnings.add ("Tab source exceeds the configured line-count limit");
        return false;
    }

    juce::StringArray normalizedLines;
    normalizedLines.ensureStorageAllocated (lines.size());

    int nonEmpty = 0;
    int currentBlockStart = -1;
    juce::StringArray currentStaffLines;

    auto flushStaff = [&]
    {
        if (currentStaffLines.isEmpty())
            return;
        NormalizedTabBlock block;
        block.kind = TabBlockKind::staff;
        block.lines = currentStaffLines;
        block.source = { currentBlockStart, currentBlockStart + currentStaffLines.size() - 1, 0, -1 };
        block.confidence = currentStaffLines.size() >= 4 ? TabConfidence::high : TabConfidence::medium;
        out.blocks.push_back (std::move (block));
        currentStaffLines.clear();
        currentBlockStart = -1;
    };

    for (int i = 0; i < lines.size(); ++i)
    {
        auto line = stripWholeLineMarkdown (lines[i], out.diagnostics.markdownWrappersRemoved);
        normalizedLines.add (line);
        if (line.trim().isNotEmpty())
            ++nonEmpty;

        addTuningCandidate (out, line, i + 1);

        const auto lower = line.toLowerCase();
        if (lower.contains ("capo"))
        {
            const int capo = firstIntegerAfter (line, "capo");
            if (capo >= 0 && capo <= 24)
            {
                if (out.metadata.capoFret >= 0 && out.metadata.capoFret != capo)
                {
                    ++out.diagnostics.metadataConflicts;
                    out.diagnostics.warnings.add ("Conflicting capo declarations");
                }
                else
                {
                    out.metadata.capoFret = capo;
                }
                ++out.diagnostics.headerLines;
            }
        }

        if (looksLikeStaffLine (line))
        {
            if (currentBlockStart < 0)
                currentBlockStart = i + 1;
            currentStaffLines.add (line);
        }
        else
        {
            flushStaff();
        }
    }
    flushStaff();

    out.diagnostics.totalLines = nonEmpty;
    out.normalizedText = normalizedLines.joinIntoString ("\n");
    resolveTunings (out);

    return nonEmpty > 0;
}

} // namespace luthier
