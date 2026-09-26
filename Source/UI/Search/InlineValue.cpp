#include "InlineValue.h"
#include "SearchMatcher.h"
#include "SearchIndex.h"
#include "../../PhysicalRange.h"

namespace luthier::search
{
namespace
{
    /** A unit's family and its factor to the family's base unit. */
    struct UnitInfo { const char* name; const char* family; double factor; };

    constexpr UnitInfo kUnits[] =
    {
        { "hz",      "freq",  1.0 },
        { "khz",     "freq",  1000.0 },
        { "db",      "db",    1.0 },
        { "ms",      "time",  0.001 },
        { "s",       "time",  1.0 },
        { "sec",     "time",  1.0 },
        { "cents",   "pitch", 1.0 },
        { "cent",    "pitch", 1.0 },
        { "ct",      "pitch", 1.0 },
        { "st",      "pitch", 100.0 },
        { "semi",    "pitch", 100.0 },
        { "semitones", "pitch", 100.0 },
        { "mm",      "len",   0.001 },
        { "cm",      "len",   0.01 },
        { "m",       "len",   1.0 },
        { "ohm",     "ohm",   1.0 },
        { "k",       "ohm",   1000.0 },
        { "kohm",    "ohm",   1000.0 },
        { "bpm",     "bpm",   1.0 },
    };

    const UnitInfo* findUnit (juce::String u)
    {
        u = u.trim().toLowerCase().replace (juce::String (juce::CharPointer_UTF8 ("\xce\xa9")), "ohm");

        for (const auto& info : kUnits)
            if (u == info.name)
                return &info;

        return nullptr;
    }

    bool isNumber (const juce::String& s)
    {
        const auto t = s.trim();

        if (t.isEmpty())
            return false;

        bool digits = false, dot = false;

        for (int i = 0; i < t.length(); ++i)
        {
            const auto c = t[i];

            if (i == 0 && (c == '+' || c == '-'))
                continue;

            if (c == '.' || c == ',')
            {
                if (dot) return false;
                dot = true;
                continue;
            }

            if (! juce::CharacterFunctions::isDigit (c))
                return false;

            digits = true;
        }

        return digits;
    }

    double toDouble (const juce::String& s)
    {
        return s.trim().replaceCharacter (',', '.').getDoubleValue();
    }

    /** "442hz", "-3db", "20ms": a number glued to its unit. */
    bool splitGlued (const juce::String& token, juce::String& number, juce::String& unit)
    {
        int i = 0;

        while (i < token.length() && (juce::CharacterFunctions::isDigit (token[i]) || token[i] == '.'
                                      || token[i] == ',' || (i == 0 && (token[i] == '+' || token[i] == '-'))))
            ++i;

        if (i == 0 || i == token.length())
            return false;

        number = token.substring (0, i);
        unit = token.substring (i);
        return isNumber (number) && findUnit (unit) != nullptr;
    }

    juce::String formatNumber (double v)
    {
        const double a = std::abs (v);
        const int decimals = a >= 1000.0 ? 0 : a >= 100.0 ? 1 : a >= 1.0 ? 1 : 3;
        return juce::String (v, decimals);
    }

    bool isChoiceLike (const juce::RangedAudioParameter& p)
    {
        return dynamic_cast<const juce::AudioParameterChoice*> (&p) != nullptr
            || dynamic_cast<const juce::AudioParameterBool*> (&p) != nullptr
            || dynamic_cast<const juce::AudioParameterInt*> (&p) != nullptr;
    }
}

//==============================================================================
const juce::StringArray& InlineValue::keywords()
{
    static const juce::StringArray k { "on", "off", "toggle", "min", "max", "default", "reset" };
    return k;
}

ValueSpec InlineValue::parse (const juce::StringArray& tail)
{
    ValueSpec v;

    if (tail.isEmpty())
        return v;

    v.text = tail.joinIntoString (" ");

    if (tail.size() == 2)
    {
        // "442 Hz", "-3 dB", "+50 cents"
        if (isNumber (tail[0]) && findUnit (tail[1]) != nullptr)
        {
            v.number = toDouble (tail[0]);
            v.unit = findUnit (tail[1])->name;
            v.kind = (tail[0].startsWithChar ('+') || tail[0].startsWithChar ('-')) && tail[1].isEmpty()
                       ? ValueSpec::Kind::relative : ValueSpec::Kind::absolute;
            return v;
        }

        v.kind = ValueSpec::Kind::text;
        return v;
    }

    if (tail.size() != 1)
    {
        v.kind = ValueSpec::Kind::text;
        return v;
    }

    const auto t = tail[0].trim();
    const auto lower = t.toLowerCase();

    if (keywords().contains (lower))
    {
        v.kind = ValueSpec::Kind::keyword;
        v.keyword = lower == "reset" ? juce::String ("default") : lower;
        return v;
    }

    if (t.endsWithChar ('%') && isNumber (t.dropLastCharacters (1)))
    {
        v.kind = ValueSpec::Kind::percent;
        v.number = toDouble (t.dropLastCharacters (1));
        return v;
    }

    if (isNumber (t))
    {
        v.number = toDouble (t);
        v.kind = (t.startsWithChar ('+') || t.startsWithChar ('-')) ? ValueSpec::Kind::relative
                                                                     : ValueSpec::Kind::absolute;

        // "-3" is a step on a knob and a value on a dB control: a negative
        // number is read as absolute when the parameter's range goes below
        // zero, which resolve() decides; here it is marked relative.
        return v;
    }

    juce::String number, unit;

    if (splitGlued (lower, number, unit))
    {
        v.kind = ValueSpec::Kind::absolute;
        v.number = toDouble (number);
        v.unit = findUnit (unit)->name;
        return v;
    }

    v.kind = ValueSpec::Kind::text;
    return v;
}

std::vector<ValueSplit> InlineValue::splits (const juce::String& rawQuery)
{
    juce::StringArray tokens;
    tokens.addTokens (rawQuery.substring (0, SearchMatcher::kMaxQueryLength), " ", "\"");
    tokens.trim();
    tokens.removeEmptyStrings();

    std::vector<ValueSplit> result;

    if (tokens.size() < 2)
        return result;

    auto add = [&] (int tailCount)
    {
        if (tailCount >= tokens.size())
            return;

        juce::StringArray tail, head;

        for (int i = 0; i < tokens.size(); ++i)
            (i >= tokens.size() - tailCount ? tail : head).add (tokens[i]);

        auto spec = parse (tail);

        if (spec.isValid())
            result.push_back ({ head.joinIntoString (" "), spec });
    };

    // A number and its unit.
    if (tokens.size() >= 3 && isNumber (tokens[tokens.size() - 2]) && findUnit (tokens[tokens.size() - 1]) != nullptr)
        add (2);

    add (1);

    // Option text of two or three words ("vintage cap").
    add (2);
    add (3);

    // Keep the first of each distinct name part (the numeric reading wins).
    std::vector<ValueSplit> unique;

    for (auto& s : result)
    {
        bool seen = false;

        for (auto& u : unique)
            seen = seen || (u.namePart == s.namePart);

        if (! seen)
            unique.push_back (s);
    }

    return unique;
}

double InlineValue::displayScale (const juce::RangedAudioParameter& p)
{
    if (dynamic_cast<const juce::AudioParameterFloat*> (&p) == nullptr)
        return 1.0;

    const auto& range = p.getNormalisableRange();

    return p.getLabel().trim().isEmpty() && range.start == 0.0f && range.end == 1.0f ? 10.0 : 1.0;
}

juce::String InlineValue::displayText (const juce::RangedAudioParameter& p, float normalised)
{
    if (const double scale = displayScale (p); scale != 1.0)
        return juce::String ((double) p.convertFrom0to1 (normalised) * scale, 1);

    auto text = p.getText (normalised, 64);
    const auto label = p.getLabel().trim();

    if (label.isNotEmpty() && ! isChoiceLike (p))
        text << " " << label;

    return text;
}

float InlineValue::arrowStep (const juce::RangedAudioParameter& p, bool fine)
{
    const auto& range = p.getNormalisableRange();
    const float span = range.end - range.start;

    if (span <= 0.0f)
        return 0.0f;

    if (range.interval > 0.0f && isChoiceLike (p))
        return juce::jmin (1.0f, range.interval / span);

    // A slider's arrow step: 1% of the travel, a tenth of that for fine.
    return fine ? 0.001f : 0.01f;
}

ResolvedValue InlineValue::resolve (const ValueSpec& value, const juce::RangedAudioParameter& p,
                                    const PhysicalRange* lockedRange, const juce::String& lockedNoticeText)
{
    ResolvedValue r;

    const auto& range = p.getNormalisableRange();
    const float current = p.getValue();
    const double scale = displayScale (p);
    const double currentReal = range.convertFrom0to1 (current);

    r.oldText = displayText (p, current);

    auto finishReal = [&] (double wanted) -> ResolvedValue&
    {
        const double lo = range.start, hi = range.end;
        const double clamped = juce::jlimit (lo, hi, wanted);

        r.clamped = std::abs (clamped - wanted) > 1.0e-9 * juce::jmax (1.0, std::abs (hi - lo));

        // The parameter's own text function (4.4 / ui-wiring 1).
        r.normalised = juce::jlimit (0.0f, 1.0f, p.getValueForText (juce::String (clamped, 6)));

        if (std::abs (range.convertFrom0to1 (r.normalised) - clamped) > 1.0e-3 * juce::jmax (1.0, hi - lo))
            r.normalised = range.convertTo0to1 ((float) clamped);

        r.ok = true;
        return r;
    };

    auto choiceOptions = [&]() -> juce::StringArray
    {
        if (auto* c = dynamic_cast<const juce::AudioParameterChoice*> (&p))
            return c->choices;

        if (dynamic_cast<const juce::AudioParameterBool*> (&p) != nullptr)
            return { "Off", "On" };

        return {};
    };

    switch (value.kind)
    {
        case ValueSpec::Kind::none:
            return r;

        case ValueSpec::Kind::keyword:
        {
            const auto options = choiceOptions();

            if (value.keyword == "min")          r.normalised = 0.0f;
            else if (value.keyword == "max")     r.normalised = 1.0f;
            else if (value.keyword == "default") r.normalised = p.getDefaultValue();
            else if (value.keyword == "toggle")
            {
                if (options.size() != 2 && dynamic_cast<const juce::AudioParameterBool*> (&p) == nullptr)
                    return r;

                r.normalised = current >= 0.5f ? 0.0f : 1.0f;
            }
            else   // on / off
            {
                const bool on = value.keyword == "on";

                if (dynamic_cast<const juce::AudioParameterBool*> (&p) != nullptr)
                    r.normalised = on ? 1.0f : 0.0f;
                else if (options.size() >= 2)
                {
                    // "off" is the option called Off / None / Bypass, else the
                    // first; "on" is the option called On, else the second.
                    int index = on ? 1 : 0;

                    for (int i = 0; i < options.size(); ++i)
                    {
                        const auto o = options[i].toLowerCase();

                        if (on ? o == "on" : (o == "off" || o == "none" || o == "bypass"))
                        {
                            index = i;
                            break;
                        }
                    }

                    r.normalised = p.convertTo0to1 ((float) index);
                }
                else
                    return r;
            }

            r.ok = true;
            break;
        }

        case ValueSpec::Kind::percent:
            if (isChoiceLike (p))
                return r;

            r.clamped = value.number < 0.0 || value.number > 100.0;
            r.normalised = (float) juce::jlimit (0.0, 1.0, value.number / 100.0);
            r.ok = true;
            break;

        case ValueSpec::Kind::text:
        {
            const auto options = choiceOptions();
            const auto wanted = SearchMatcher::normalise (value.text);

            if (options.isEmpty() || wanted.isEmpty())
                return r;

            int found = -1;

            for (int i = 0; i < options.size() && found < 0; ++i)
                if (SearchMatcher::normalise (options[i]) == wanted)
                    found = i;

            for (int i = 0; i < options.size() && found < 0; ++i)
                if (SearchMatcher::normalise (options[i]).startsWith (wanted))
                    found = i;

            if (found < 0)
                return r;

            r.normalised = p.convertTo0to1 ((float) found);
            r.ok = true;
            break;
        }

        case ValueSpec::Kind::relative:
        case ValueSpec::Kind::absolute:
        {
            if (auto* c = dynamic_cast<const juce::AudioParameterChoice*> (&p))
            {
                // A number on a choice is its 1-based position.
                if (value.kind == ValueSpec::Kind::relative || value.unit.isNotEmpty())
                    return r;

                const int index = juce::jlimit (0, c->choices.size() - 1, (int) std::round (value.number) - 1);
                r.clamped = index != (int) std::round (value.number) - 1;
                r.normalised = p.convertTo0to1 ((float) index);
                r.ok = true;
                break;
            }

            double number = value.number;

            // Units: convert into the parameter's own.
            if (value.unit.isNotEmpty())
            {
                const auto* typed = findUnit (value.unit);
                const auto* own = findUnit (p.getLabel());

                if (typed == nullptr || own == nullptr || juce::String (typed->family) != own->family)
                    return r;

                number = number * typed->factor / own->factor;
            }

            // A signed number is a step, unless the range itself goes negative
            // (a dB trim, a detune), where "-3" means minus three.
            const bool relative = value.kind == ValueSpec::Kind::relative && value.unit.isEmpty()
                                  && ! (range.start < 0.0f && value.number < 0.0);

            finishReal (relative ? currentReal + number / scale
                                 : number / (value.unit.isEmpty() ? scale : 1.0));
            break;
        }
    }

    if (! r.ok)
        return r;

    r.newText = displayText (p, r.normalised);
    r.preview = "Set to " + r.newText + " (now " + r.oldText + ")";

    if (r.clamped)
    {
        const bool atMax = r.normalised >= 0.5f;

        const bool stockEdge = lockedRange != nullptr
                                 && (atMax ? lockedRange->advancedMax > lockedRange->stockMax
                                           : lockedRange->advancedMin < lockedRange->stockMin);

        r.clampText = "Clamped to " + r.newText + (atMax ? " (max)" : " (min)");

        // A stock-locked physical parameter says why it stopped there, in
        // the words the control's own edge notice uses (advanced-ranges 6.3).
        if (stockEdge && lockedNoticeText.isNotEmpty())
        {
            r.atStockEdge = true;
            r.clampText << ". " << lockedNoticeText;
        }
    }

    juce::ignoreUnused (formatNumber);
    return r;
}

ValueReading InlineValue::read (SearchIndex& index, const juce::String& rawQuery, const ParameterLookup& lookup,
                                const StockLockedTest& stockLocked, const juce::String& lockedNoticeText)
{
    ValueReading reading;

    SearchIndex::Scope scope = SearchIndex::Scope::all;
    const auto text = SearchIndex::parseScope (rawQuery, scope);

    // Only a query that could name a parameter can carry a value.
    if (scope != SearchIndex::Scope::all && scope != SearchIndex::Scope::parametersOnly)
        return reading;

    const auto whole = index.query (rawQuery);
    const double wholeScore = whole.empty() ? 0.0 : whole.front().score;

    for (const auto& split : splits (text))
    {
        const auto best = index.bestMatch (split.namePart, { ItemKind::parameter, ItemKind::choiceOption });

        if (best.item == nullptr || best.matchScore < kMinimumNameScore)
            continue;

        auto* p = lookup != nullptr ? lookup (best.item->target) : nullptr;

        if (p == nullptr)
            continue;

        const auto* locked = stockLocked != nullptr ? stockLocked (best.item->target) : nullptr;
        auto resolved = resolve (split.value, *p, locked, lockedNoticeText);

        if (! resolved.ok || best.score <= wholeScore)
            continue;

        reading.item = best.item;
        reading.parameterId = best.item->target;
        reading.namePart = split.namePart;
        reading.value = split.value;
        reading.resolved = resolved;
        return reading;
    }

    return reading;
}

} // namespace luthier::search
