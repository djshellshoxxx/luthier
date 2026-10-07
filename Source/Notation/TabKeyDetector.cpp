#include "TabKeyDetector.h"
#include "AsciiTabReader.h"
#include "TabChordChart.h"

#include <cmath>

namespace luthier
{

namespace
{
    // Krumhansl & Kessler (1982) tonal hierarchies.
    const double kMajor[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    const double kMinor[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    double correlate (const double (&x)[12], const double (&profile)[12], int rotation)
    {
        double mx = 0.0, mp = 0.0;
        for (int i = 0; i < 12; ++i) { mx += x[i]; mp += profile[i]; }
        mx /= 12.0; mp /= 12.0;

        double num = 0.0, dx = 0.0, dp = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            const double a = x[i] - mx;
            const double b = profile[((i - rotation) % 12 + 12) % 12] - mp;
            num += a * b; dx += a * a; dp += b * b;
        }
        const double den = std::sqrt (dx * dp);
        return den > 1.0e-9 ? num / den : 0.0;
    }
}

TabKeyEstimate TabKeyDetector::estimateFromProfile (const double (&weights)[12])
{
    TabKeyEstimate out;

    double total = 0.0;
    for (double w : weights)
        total += std::isfinite (w) && w > 0.0 ? w : 0.0;
    if (total <= 0.0)
        return out;

    double x[12];
    for (int i = 0; i < 12; ++i)
        x[i] = std::isfinite (weights[i]) && weights[i] > 0.0 ? weights[i] : 0.0;

    double best = -2.0, second = -2.0;
    int bestRoot = 0; bool bestMinor = false;

    for (int root = 0; root < 12; ++root)
    {
        for (int m = 0; m < 2; ++m)
        {
            const double r = correlate (x, m == 0 ? kMajor : kMinor, root);
            if (r > best) { second = best; best = r; bestRoot = root; bestMinor = m == 1; }
            else if (r > second) second = r;
        }
    }

    out.rootPitchClass = bestRoot;
    out.minor = bestMinor;
    out.confidence = juce::jlimit (0.0, 1.0, (best - second) * 2.0);
    out.name = AsciiTabReader::keyName (bestRoot, bestMinor);
    return out;
}

TabKeyEstimate TabKeyDetector::estimate (const PerformanceScore& score, int trackIndex)
{
    double weights[12] = {};

    if (score.getNumTracks() == 0)
        return estimateFromProfile (weights);

    const auto& track = score.getTrack (juce::jlimit (0, score.getNumTracks() - 1, trackIndex));

    bool firstChord = true;
    int chordCount = 0;

    for (const auto& measure : track.measures)
    {
        for (const auto& voice : measure.voices)
            for (const auto& note : voice.notes)
            {
                if (note.hasTechnique (ScoreTechnique::Type::deadNote)) continue;
                const double d = std::isfinite (note.durationBeats) ? juce::jlimit (0.125, 4.0, note.durationBeats) : 0.25;
                // The bass string carries more tonal weight than the top string.
                const double stringWeight = 1.0 + 0.15 * juce::jlimit (0, 11, note.stringIndex);
                weights[((note.midiNote % 12) + 12) % 12] += d * stringWeight;
            }

        for (const auto& symbol : measure.chordSymbols)
        {
            TabChordChart::Chord chord;
            if (! TabChordChart::parseChord (symbol.second, chord) || ++chordCount > 4096)
                continue;

            const int root = chord.rootPitchClass;
            const bool minorChord = chord.quality.startsWith ("m") && ! chord.quality.startsWith ("maj");
            const bool dim = chord.quality == "dim";
            weights[root] += 2.0;
            weights[(root + (minorChord || dim ? 3 : 4)) % 12] += 1.0;
            weights[(root + (dim ? 6 : 7)) % 12] += 1.0;
            if (firstChord) { weights[root] += 2.0; firstChord = false; }
            if (chord.bassPitchClass >= 0) weights[chord.bassPitchClass] += 0.5;
        }
    }

    return estimateFromProfile (weights);
}

void TabKeyDetector::apply (PerformanceScore& score)
{
    if (score.getMeta().key.isNotEmpty())
        return;

    const auto estimate = TabKeyDetector::estimate (score);
    if (estimate.rootPitchClass >= 0)
        score.getMeta().key = estimate.name;
}

bool TabKeyDetector::parseKeyName (const juce::String& name, int& rootPitchClass, bool& minor)
{
    return AsciiTabReader::parseKeyStatement ("Key: " + name.trim(), rootPitchClass, minor);
}

} // namespace luthier
