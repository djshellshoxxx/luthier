#include "RiffAnalysis.h"

#include <cmath>
#include <set>

namespace luthier
{

namespace RiffAnalysis
{
    namespace
    {
        // Krumhansl-Kessler key profiles.
        constexpr double kMajor[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        constexpr double kMinor[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

        double correlate (const double* weights, const double* profile, int shift)
        {
            double meanW = 0.0, meanP = 0.0;

            for (int i = 0; i < 12; ++i)
            {
                meanW += weights[i];
                meanP += profile[i];
            }

            meanW /= 12.0;
            meanP /= 12.0;

            double num = 0.0, dw = 0.0, dp = 0.0;

            for (int i = 0; i < 12; ++i)
            {
                const double w = weights[(i + shift) % 12] - meanW;
                const double p = profile[i] - meanP;
                num += w * p;
                dw += w * w;
                dp += p * p;
            }

            const double den = std::sqrt (dw * dp);
            return den > 0.0 ? num / den : 0.0;
        }

        bool isDead (const ScoreNote& n)
        {
            return n.hasTechnique (ScoreTechnique::Type::deadNote);
        }
    }

    Key detectKey (const std::vector<ScoreNote>& notes)
    {
        double weights[12] = {};

        for (const auto& n : notes)
            if (! isDead (n))
                weights[((n.midiNote % 12) + 12) % 12] += juce::jmax (0.0, n.durationBeats);

        Key best;
        best.correlation = -2.0;

        for (int root = 0; root < 12; ++root)
        {
            const double major = correlate (weights, kMajor, root);
            const double minor = correlate (weights, kMinor, root);

            if (major > best.correlation + 1.0e-12)
                best = { root, "ionian", major };

            if (minor > best.correlation + 1.0e-12)
                best = { root, "aeolian", minor };
        }

        if (best.correlation < -1.0)
            best = {};

        return best;
    }

    int estimateDifficulty (const Riff& riff)
    {
        std::set<long long> onsets;

        for (const auto& n : riff.notes)
            onsets.insert (std::llround (n.startBeat * 1.0e6));

        const double seconds = riff.lengthBeats * 60.0 / juce::jmax (1.0, riff.tempoBpm);
        const double nps = seconds > 0.0 ? (double) onsets.size() / seconds : 0.0;
        const int distinct = riff.computeTechniques().size();

        int lo = 1000, hi = -1;

        for (const auto& n : riff.notes)
        {
            if (n.fret > 0)
            {
                lo = juce::jmin (lo, n.fret);
                hi = juce::jmax (hi, n.fret);
            }
        }

        const int span = hi >= lo ? hi - lo : 0;
        const double estimate = 1.0 + nps / 3.0 + distinct / 3.0 + juce::jmax (0, span - 4) / 4.0;

        // Python's round() is half-to-even; the generator uses it, so this does too.
        const double floorValue = std::floor (estimate);
        const double fraction = estimate - floorValue;
        double rounded = fraction > 0.5 ? floorValue + 1.0 : fraction < 0.5 ? floorValue
                                        : (std::fmod (floorValue, 2.0) == 0.0 ? floorValue : floorValue + 1.0);

        return juce::jlimit (1, 5, (int) rounded);
    }

    void analyse (Riff& riff)
    {
        riff.updatePitches();

        const auto key = detectKey (riff.notes);
        riff.keyRoot = RiffVocabulary::rootName (key.root);
        riff.scale = key.scale;
        riff.techniques = riff.computeTechniques();
        riff.difficulty = estimateDifficulty (riff);
    }
}

} // namespace luthier
