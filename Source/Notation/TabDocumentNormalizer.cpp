#include "TabDocumentNormalizer.h"
#include "AsciiTabReader.h"

#include <algorithm>
#include <utility>

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
            const auto before = line.substring (0, pos).trim();
            const auto after = line.substring (pos + 6).trim();
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
            const auto payload = tuningPayload (line);
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
                || c == 'B' || c == 'H' || c == '/' || c == '\\' || c == '~'
                || c == '^' || c == ' ' || c == '.')
                ++staffChars;
        }
        return staffChars >= juce::jmax (3, trimmed.length() - bar - 3);
    }

    bool isLegendHeader (const juce::String& lower)
    {
        const auto t = lower.trim();
        return t == "key:" || t == "key" || t == "legend:" || t == "legend"
            || t == "notation:" || t == "notation" || t == "symbols:" || t == "symbols";
    }

    juce::String canonicalTechniqueMeaning (const juce::String& raw)
    {
        const auto lower = raw.toLowerCase();
        if (lower.contains ("hammer"))       return "hammerOn";
        if (lower.contains ("pull"))         return "pullOff";
        if (lower.contains ("harmonic"))     return "naturalHarmonic";
        if (lower.contains ("bend"))         return "bend";
        if (lower.contains ("slide up"))     return "slideUp";
        if (lower.contains ("slide down"))   return "slideDown";
        if (lower.contains ("vibrato"))      return "vibrato";
        if (lower.contains ("palm mute"))    return "palmMute";
        if (lower.contains ("pop"))          return "pop";
        if (lower.contains ("slap"))         return "slap";
        if (lower.contains ("trill"))        return "trill";
        if (lower.contains ("tremolo"))      return "tremoloPicking";
        return {};
    }

    juce::String generalizeLegendPattern (juce::String lhs)
    {
        lhs = lhs.trim();
        while (lhs.startsWithChar ('|') || lhs.startsWithChar ('-')
               || lhs.startsWithChar ('*') || lhs.startsWithChar ((juce::juce_wchar) 0x2022))
            lhs = lhs.substring (1).trimStart();

        int p = 0;
        while (p < lhs.length() && juce::CharacterFunctions::isDigit (lhs[p]))
            ++p;

        if (p > 0)
        {
            const auto suffix = lhs.substring (p);
            if (suffix == "B" || suffix == "b")
                return "<fret>B";
            if (suffix == "H" || suffix == "h")
                return "<fret>H";

            if (suffix.startsWithChar ('^'))
            {
                int q = 1;
                while (q < suffix.length() && juce::CharacterFunctions::isDigit (suffix[q]))
                    ++q;
                if (q > 1 && q == suffix.length())
                    return "<fret>^<higher-fret>";
            }
        }

        return lhs;
    }

    bool parseLegendDefinition (const juce::String& line, TabNotationDefinition& out,
                                int sourceLine)
    {
        auto text = line.trim();
        while (text.startsWithChar ('-') || text.startsWithChar ('*')
               || text.startsWithChar ((juce::juce_wchar) 0x2022))
            text = text.substring (1).trimStart();
        if (text.startsWithChar ('|'))
            text = text.substring (1).trimStart();

        int split = text.indexOfChar ('=');
        if (split < 0)
            split = text.indexOfChar (':');

        juce::String lhs, rhs;
        if (split >= 0)
        {
            lhs = text.substring (0, split).trim();
            rhs = text.substring (split + 1).trim();
        }
        else
        {
            int firstSpace = -1;
            for (int i = 0; i < text.length(); ++i)
                if (juce::CharacterFunctions::isWhitespace (text[i]))
                {
                    firstSpace = i;
                    break;
                }
            if (firstSpace < 0)
                return false;
            lhs = text.substring (0, firstSpace).trim();
            rhs = text.substring (firstSpace).trim();
        }

        const auto meaning = canonicalTechniqueMeaning (rhs);
        if (lhs.isEmpty() || meaning.isEmpty())
            return false;

        out.pattern = generalizeLegendPattern (lhs);
        out.canonicalMeaning = meaning;
        out.rawDefinition = line.trim();
        out.source = { sourceLine, sourceLine, 0, line.length() };
        out.confidence = TabConfidence::exact;
        return true;
    }

    bool isSectionHeading (const juce::String& line, juce::String& section)
    {
        const auto trimmed = line.trim();
        const auto lower = trimmed.toLowerCase();
        const char* prefixes[] = { "intro", "verse", "chorus", "bridge", "solo",
                                   "outro", "interlude", "riff", "fill" };
        for (const auto* p : prefixes)
        {
            if (lower.startsWith (p))
            {
                int end = trimmed.indexOfChar (':');
                const int paren = trimmed.indexOfChar ('(');
                if (end < 0 || (paren >= 0 && paren < end))
                    end = paren;
                section = (end >= 0 ? trimmed.substring (0, end) : trimmed).trim();
                return true;
            }
        }
        return false;
    }

    int detectPart (const juce::String& line, juce::String& name)
    {
        const auto lower = line.toLowerCase();
        if (lower.contains ("guitar 2") || lower.contains ("guitar two")
            || lower.contains ("second guitar") || lower.contains ("gtr. 2")
            || lower.contains ("gtr 2"))
        {
            name = "Guitar 2";
            return 2;
        }
        if (lower.contains ("guitar 1") || lower.contains ("guitar one")
            || lower.contains ("first guitar") || lower.contains ("gtr. 1")
            || lower.contains ("gtr 1"))
        {
            name = "Guitar 1";
            return 1;
        }
        if (lower.contains ("rhythm guitar"))
        {
            name = "Rhythm Guitar";
            return 1;
        }
        if (lower.contains ("lead guitar"))
        {
            name = "Lead Guitar";
            return 2;
        }
        return 0;
    }

    bool isAttribution (const juce::String& line)
    {
        const auto lower = line.toLowerCase();
        return lower.contains ("sent by") || lower.contains ("tabbed by")
            || lower.contains ("transcribed by") || lower.contains ("http://")
            || lower.contains ("https://") || line.containsChar ('@');
    }

    void addBlock (NormalizedTabDocument& out, TabBlockKind kind, const juce::String& line,
                   int sourceLine, TabConfidence confidence, const juce::String& section,
                   int partIndex)
    {
        NormalizedTabBlock block;
        block.kind = kind;
        block.lines.add (line);
        block.source = { sourceLine, sourceLine, 0, line.length() };
        block.confidence = confidence;
        block.sectionName = section;
        block.partIndex = partIndex;
        out.blocks.push_back (std::move (block));
    }

    void maybeAddDirective (NormalizedTabDocument& out, const juce::String& line,
                            int sourceLine, const juce::String& section, int partIndex)
    {
        const auto lower = line.toLowerCase();
        juce::String name;
        if (lower.contains ("harmonic"))
            name = "naturalHarmonic";
        else if (lower.contains ("tremolo picking") || lower.contains ("tremolo pick"))
            name = "tremoloPicking";
        else if (lower.contains ("aka trill") || lower.contains (" trill"))
            name = "trill";
        else if (lower.contains ("palm mute throughout"))
            name = "palmMute";
        else if (lower.contains ("let ring"))
            name = "letRing";

        if (name.isEmpty())
            return;

        TabDirective d;
        d.canonicalName = name;
        d.rawText = line.trim();
        d.sectionName = section;
        d.partIndex = partIndex;
        d.source = { sourceLine, sourceLine, 0, line.length() };
        d.confidence = TabConfidence::high;
        d.scope = section.isNotEmpty() ? DirectiveScope::section
                                       : (partIndex > 0 ? DirectiveScope::part
                                                        : DirectiveScope::nextSystem);
        out.metadata.directives.push_back (std::move (d));
        ++out.diagnostics.proseDirectives;
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
    juce::String currentSection;
    int currentPart = 0;
    bool legendMode = false;

    auto flushStaff = [&]
    {
        if (currentStaffLines.isEmpty())
            return;
        NormalizedTabBlock block;
        block.kind = TabBlockKind::staff;
        block.lines = currentStaffLines;
        block.source = { currentBlockStart, currentBlockStart + currentStaffLines.size() - 1, 0, -1 };
        block.confidence = currentStaffLines.size() >= 4 ? TabConfidence::high : TabConfidence::medium;
        block.sectionName = currentSection;
        block.partIndex = currentPart;
        out.blocks.push_back (std::move (block));
        currentStaffLines.clear();
        currentBlockStart = -1;
    };

    for (int i = 0; i < lines.size(); ++i)
    {
        const int sourceLine = i + 1;
        auto line = stripWholeLineMarkdown (lines[i], out.diagnostics.markdownWrappersRemoved);
        normalizedLines.add (line);
        const auto trimmed = line.trim();
        const auto lower = trimmed.toLowerCase();
        if (trimmed.isNotEmpty())
            ++nonEmpty;

        addTuningCandidate (out, line, sourceLine);

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
            legendMode = false;
            if (currentBlockStart < 0)
                currentBlockStart = sourceLine;
            currentStaffLines.add (line);
            continue;
        }

        flushStaff();

        if (trimmed.isEmpty())
        {
            legendMode = false;
            continue;
        }

        if (isLegendHeader (lower))
        {
            legendMode = true;
            addBlock (out, TabBlockKind::legend, line, sourceLine, TabConfidence::exact,
                      currentSection, currentPart);
            continue;
        }

        TabNotationDefinition definition;
        if ((legendMode || trimmed.startsWithChar ('|') || trimmed.startsWithChar ('-'))
            && parseLegendDefinition (line, definition, sourceLine))
        {
            out.notation.push_back (std::move (definition));
            ++out.diagnostics.legendEntries;
            addBlock (out, TabBlockKind::legend, line, sourceLine, TabConfidence::exact,
                      currentSection, currentPart);
            continue;
        }
        legendMode = false;

        juce::String section;
        if (isSectionHeading (line, section))
        {
            currentSection = section;
            if (! out.metadata.sectionNames.contains (section))
                out.metadata.sectionNames.add (section);
            maybeAddDirective (out, line, sourceLine, currentSection, currentPart);
            addBlock (out, TabBlockKind::sectionHeading, line, sourceLine, TabConfidence::high,
                      currentSection, currentPart);
            continue;
        }

        juce::String partName;
        const int part = detectPart (line, partName);
        if (part > 0)
        {
            currentPart = part;
            if (! out.metadata.partNames.contains (partName))
                out.metadata.partNames.add (partName);
            ++out.diagnostics.multiPartBlocks;
            maybeAddDirective (out, line, sourceLine, currentSection, currentPart);
            addBlock (out, TabBlockKind::proseInstruction, line, sourceLine, TabConfidence::high,
                      currentSection, currentPart);
            continue;
        }

        if (isAttribution (line))
        {
            addBlock (out, TabBlockKind::attribution, line, sourceLine, TabConfidence::high,
                      currentSection, currentPart);
            continue;
        }

        const auto before = out.metadata.directives.size();
        maybeAddDirective (out, line, sourceLine, currentSection, currentPart);
        if (out.metadata.directives.size() != before)
        {
            addBlock (out, TabBlockKind::proseInstruction, line, sourceLine, TabConfidence::high,
                      currentSection, currentPart);
            continue;
        }

        if (lineLooksLikeTuningStatement (lower) || lower.contains ("capo"))
        {
            addBlock (out, TabBlockKind::proseInstruction, line, sourceLine, TabConfidence::exact,
                      currentSection, currentPart);
            continue;
        }

        addBlock (out, TabBlockKind::unknown, line, sourceLine, TabConfidence::low,
                  currentSection, currentPart);
    }
    flushStaff();

    out.diagnostics.totalLines = nonEmpty;
    out.normalizedText = normalizedLines.joinIntoString ("\n");
    resolveTunings (out);

    return nonEmpty > 0;
}

} // namespace luthier
