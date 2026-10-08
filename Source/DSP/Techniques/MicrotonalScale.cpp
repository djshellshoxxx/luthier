#include "MicrotonalScale.h"
#include <algorithm>

namespace luthier
{

void MicrotonalScale::sortAndClamp() noexcept
{
    std::sort (points.begin(), points.begin() + numPoints);
}

void MicrotonalScale::setEqual (int divisions) noexcept
{
    divisions = juce::jlimit (1, 96, divisions);
    const double step = 1200.0 / (double) divisions;

    // 12 and 24 divide the semitone grid itself; the rest are rooted on middle C.
    const double root = (1200 % divisions == 0 && divisions % 12 == 0) ? 0.0 : kRootMidiCents;

    numPoints = 0;

    for (int k = (int) std::floor ((-100.0 - root) / step); numPoints < kMaxPoints; ++k)
    {
        const double p = root + (double) k * step;

        if (p > 13000.0)
            break;

        if (p >= -100.0)
            points[(size_t) numPoints++] = p;
    }

    name = juce::String (divisions) + "-EDO";
    sourceText = {};
    tun = false;
}

bool MicrotonalScale::loadScala (const juce::String& text, juce::String& error)
{
    // Scala: '!' lines are comments; the first other line is the description,
    // the second the number of notes, then one pitch per line - cents if it
    // has a '.', a ratio a/b, or a whole number a (a/1).
    juce::StringArray lines;

    for (auto line : juce::StringArray::fromLines (text))
        if (! line.trimStart().startsWithChar ('!'))
            lines.add (line.trim());

    if (lines.size() < 2)
    {
        error = "Not a Scala file: no description and note count.";
        return false;
    }

    const juce::String description = lines[0];
    const int count = lines[1].getIntValue();

    if (count <= 0 || count > 1024 || lines.size() < 2 + count)
    {
        error = "The Scala file lists " + juce::String (count) + " notes but has "
                  + juce::String (juce::jmax (0, lines.size() - 2)) + ".";
        return false;
    }

    std::array<double, 1025> degrees {};
    degrees[0] = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto token = lines[2 + i].upToFirstOccurrenceOf (" ", false, false).trim();
        double cents = 0.0;

        if (token.containsChar ('.'))
        {
            cents = token.getDoubleValue();
        }
        else
        {
            const double num = token.upToFirstOccurrenceOf ("/", false, false).getDoubleValue();
            const double den = token.containsChar ('/') ? token.fromFirstOccurrenceOf ("/", false, false).getDoubleValue() : 1.0;

            if (num <= 0.0 || den <= 0.0)
            {
                error = "Line \"" + lines[2 + i] + "\" is not a pitch.";
                return false;
            }

            cents = 1200.0 * std::log2 (num / den);
        }

        degrees[(size_t) i + 1] = cents;
    }

    const double period = degrees[(size_t) count];

    if (period <= 1.0)
    {
        error = "The scale's period (its last degree) must be above the 1/1.";
        return false;
    }

    numPoints = 0;

    for (int k = (int) std::floor ((-100.0 - kRootMidiCents) / period) - 1; numPoints < kMaxPoints - count; ++k)
    {
        const double base = kRootMidiCents + (double) k * period;

        if (base > 13000.0)
            break;

        for (int i = 0; i < count && numPoints < kMaxPoints; ++i)
        {
            const double p = base + degrees[(size_t) i];

            if (p >= -100.0 && p <= 13000.0)
                points[(size_t) numPoints++] = p;
        }
    }

    sortAndClamp();
    name = description.isNotEmpty() ? description : juce::String ("Scala scale");
    sourceText = text;
    tun = false;
    return true;
}

bool MicrotonalScale::loadTun (const juce::String& text, juce::String& error)
{
    constexpr double kMidiZeroHz = 8.1757989156437;

    double baseFreq = kMidiZeroHz;
    std::array<double, 128> notes {};
    std::array<bool, 128> seen {};
    bool inTuning = false;

    for (auto raw : juce::StringArray::fromLines (text))
    {
        const auto line = raw.upToFirstOccurrenceOf (";", false, false).trim();

        if (line.startsWithChar ('['))
        {
            const auto section = line.toLowerCase();
            inTuning = section == "[tuning]" || section == "[exact tuning]";
            continue;
        }

        if (! inTuning || ! line.containsChar ('='))
            continue;

        const auto key = line.upToFirstOccurrenceOf ("=", false, false).trim().toLowerCase();
        const double value = line.fromFirstOccurrenceOf ("=", false, false).trim().getDoubleValue();

        if (key == "basefreq")
        {
            if (value > 0.0)
                baseFreq = value;
        }
        else if (key.startsWith ("note"))
        {
            const int n = key.fromFirstOccurrenceOf ("note", false, false).trim().getIntValue();

            if (juce::isPositiveAndBelow (n, 128))
            {
                notes[(size_t) n] = value;
                seen[(size_t) n] = true;
            }
        }
    }

    const double offset = 1200.0 * std::log2 (baseFreq / kMidiZeroHz);
    int found = 0;

    for (int n = 0; n < 128; ++n)
        if (seen[(size_t) n])
            ++found;

    if (found == 0)
    {
        error = "No \"note N=cents\" lines under [Tuning] or [Exact Tuning].";
        return false;
    }

    numPoints = 0;

    for (int n = 0; n < 128; ++n)
        if (seen[(size_t) n])
            points[(size_t) numPoints++] = notes[(size_t) n] + offset;

    sortAndClamp();
    name = "Tuning file";
    sourceText = text;
    tun = true;
    return true;
}

bool MicrotonalScale::loadFile (const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "The file is not there.";
        return false;
    }

    const auto text = file.loadFileAsString();
    const bool ok = file.hasFileExtension ("tun") ? loadTun (text, error) : loadScala (text, error);

    if (ok && ! tun && name == "Scala scale")
        name = file.getFileNameWithoutExtension();

    if (ok && tun)
        name = file.getFileNameWithoutExtension();

    return ok;
}

double MicrotonalScale::nearest (double midiCents) const noexcept
{
    if (numPoints <= 0 || ! std::isfinite (midiCents))
        return midiCents;

    const auto* begin = points.data();
    const auto* end = begin + numPoints;
    const auto* it = std::lower_bound (begin, end, midiCents);

    if (it == begin)
        return *it;

    if (it == end)
        return *(end - 1);

    return (midiCents - *(it - 1) <= *it - midiCents) ? *(it - 1) : *it;
}

juce::var MicrotonalScale::toVar() const
{
    if (sourceText.isEmpty())
        return {};

    auto* o = new juce::DynamicObject();
    o->setProperty ("format", tun ? "tun" : "scl");
    o->setProperty ("name", name);
    o->setProperty ("text", sourceText);
    return juce::var (o);
}

void MicrotonalScale::fromVar (const juce::var& state)
{
    auto* o = state.getDynamicObject();

    if (o == nullptr)
    {
        setEqual (12);
        return;
    }

    juce::String error;
    const auto text = o->getProperty ("text").toString();
    const bool ok = o->getProperty ("format").toString() == "tun" ? loadTun (text, error) : loadScala (text, error);

    if (! ok)
        setEqual (12);
    else if (o->hasProperty ("name"))
        name = o->getProperty ("name").toString();
}

} // namespace luthier
