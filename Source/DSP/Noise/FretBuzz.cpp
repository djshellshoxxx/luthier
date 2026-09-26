#include "FretBuzz.h"
#include "PlayingNoise.h"

namespace luthier
{

//==============================================================================
double SetupGeometry::fretPositionMm (double fret) const noexcept
{
    return scaleLengthMm * (1.0 - std::pow (2.0, -fret / 12.0));
}

double SetupGeometry::actionFor (int stringIndex) const noexcept
{
    const double t = numStrings > 1 ? (double) stringIndex / (double) (numStrings - 1) : 0.0;
    return actionTreble + (actionBass - actionTreble) * juce::jlimit (0.0, 1.0, t);
}

double SetupGeometry::clearanceMm (int stringIndex, double frettedAt, int fret) const noexcept
{
    // Frets behind the finger are lifted off; the nut is not a fret.
    if (fret <= (int) std::floor (frettedAt + 1.0e-6) || fret < 1)
        return 1.0e9;

    const double length = scaleLengthMm;

    // The fret tops follow the board, which the truss rod bows into a
    // parabola deepest at fret 7 (1). Positive relief lowers the middle frets.
    auto fretTop = [this] (double n)
    {
        const double bow = 1.0 - ((n - 7.0) / 7.0) * ((n - 7.0) / 7.0);

        // SPEC-SWEEP FB-21: a worn crown sits lower than the board line.
        const int f = (int) std::round (n);
        const double wear = (f >= 1 && f <= kMaxFrets) ? fretWearMm[(size_t) f] : 0.0;

        return -relief * juce::jmax (0.0, bow) - wear;
    };

    // The open string runs from the nut (its slot clearance over fret 1) to
    // the saddle, whose height is whatever makes the 12th-fret action right.
    const double nut = nutDepth[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)];
    const double x12 = fretPositionMm (12.0);
    // (The setup is measured against the board line, not a worn 12th fret.)
    const double action12 = actionFor (stringIndex) + fretTop (12.0) + fretWearMm[12];
    const double saddle = nut + (action12 - nut) * length / x12;

    const double x = fretPositionMm ((double) fret);

    double stringHeight;

    if (frettedAt < 0.5)
    {
        stringHeight = nut + (saddle - nut) * x / length;
    }
    else
    {
        // Held down on fret f: the string leaves that fret's top and rises
        // straight to the saddle.
        const double xf = fretPositionMm (frettedAt);
        const double from = fretTop (frettedAt);
        stringHeight = from + (saddle - from) * (x - xf) / juce::jmax (1.0, length - xf);
    }

    return stringHeight - fretTop ((double) fret);
}

const SetupStyle& getSetupStyle (int index) noexcept
{
    static const SetupStyle styles[kNumSetupStyles] =
    {
        { "Factory low",      1.3, 1.6,  0.15 },
        { "Player-friendly",  1.6, 2.0,  0.20 },
        { "Clean / high",     2.1, 2.6,  0.25 },
        { "Slide setup",      2.8, 3.2,  0.30 },
        { "Blues / dug-in",   1.5, 1.9,  0.10 },
        { "Needs a tech",     1.1, 1.3, -0.03 },
    };

    return styles[juce::jlimit (0, kNumSetupStyles - 1, index)];
}

//==============================================================================
FretBuzz::FretBuzz()
{
    generatorIndex.fill (-1);

    for (auto& f : buzzingFret)
        f.store (-1);

    for (auto& row : heat)
        for (auto& cell : row)
            cell.store (-10.0f);
}

void FretBuzz::reset() noexcept
{
    generatorIndex.fill (-1);

    for (auto& f : buzzingFret)
        f.store (-1);
}

double FretBuzz::displacementMm (double level, double u, double pluckPosition) noexcept
{
    // 2: the fundamental plus the next two modes, weighted as a pluck at
    // `pluckPosition` excites them (sin(k pi p) / k^2), normalised so the
    // envelope is `level` at the fundamental's antinode.
    const double p = juce::jlimit (0.02, 0.5, pluckPosition);
    double sum = 0.0, norm = 0.0;

    for (int k = 1; k <= 3; ++k)
    {
        const double weight = std::abs (std::sin (k * constants::kPi * p)) / (double) (k * k);
        sum += weight * std::abs (std::sin (k * constants::kPi * u));
        norm += weight;
    }

    return FretBuzz::kMmPerLevelUnit * juce::jmax (0.0, level) * sum / juce::jmax (1.0e-9, norm);
}

FretBuzz::Contact FretBuzz::sense (int stringIndex, double frettedAt, double level, double pluckPosition) const noexcept
{
    Contact worst;

    const double length = geometry.scaleLengthMm;
    const double xf = frettedAt > 0.5 ? geometry.fretPositionMm (frettedAt) : 0.0;
    const double vibrating = juce::jmax (1.0, length - xf);

    // 3.2: a trim of up to 0.15 mm either way. At 0 a bad setup still buzzes.
    const double trim = (geometry.buzzThreshold - 0.5) * 0.3;

    for (int fret = (int) std::floor (frettedAt) + 1; fret <= geometry.numFrets; ++fret)
    {
        const double clearance = geometry.clearanceMm (stringIndex, frettedAt, fret);

        if (clearance > 1.0e8)
            continue;

        const double u = (geometry.fretPositionMm ((double) fret) - xf) / vibrating;
        const double excess = displacementMm (level, u, pluckPosition) + trim - clearance;

        if (excess > worst.excessMm)
        {
            worst.excessMm = excess;
            worst.fret = fret;
        }
    }

    return worst;
}

double FretBuzz::levelFor (double excessMm) const noexcept
{
    // 4: min(1, excess / 0.3 mm), scaled by fret height - a taller fret is a
    // harder, brighter contact - against a light buzz 30-40 dB under the note.
    const double heightFactor = std::pow (juce::jmax (0.1, geometry.fretHeight) / 1.0, 0.8);

    return PlayingNoise::kNoteReference * dbToGain (-22.0)
           * juce::jlimit (0.0, 1.0, excessMm / 0.3) * heightFactor;
}

void FretBuzz::process (NoiseEngine& pool, const double* levels, const double* fretted,
                        const double* fundamentalHz, int numStrings, double pluckPosition) noexcept
{
    const int strings = juce::jmin (numStrings, SetupGeometry::kMaxStrings);

    for (int s = 0; s < strings; ++s)
    {
        const double level = levels[s];
        auto contact = sense (s, fretted[s], level, pluckPosition);

        // The heatmap sees every fret, not only the worst one.
        {
            const double xf = fretted[s] > 0.5 ? geometry.fretPositionMm (fretted[s]) : 0.0;
            const double vibrating = juce::jmax (1.0, geometry.scaleLengthMm - xf);
            const double trim = (geometry.buzzThreshold - 0.5) * 0.3;

            for (int fret = 1; fret <= SetupGeometry::kMaxFrets; ++fret)
            {
                const double clearance = geometry.clearanceMm (s, fretted[s], fret);
                float value = -10.0f;

                if (clearance < 1.0e8 && fret <= geometry.numFrets)
                {
                    const double u = (geometry.fretPositionMm ((double) fret) - xf) / vibrating;
                    value = (float) (displacementMm (level, u, pluckPosition) + trim - clearance);
                }

                heat[(size_t) s][(size_t) fret].store (value, std::memory_order_relaxed);
            }
        }

        // 5: sitar mode keeps a grazing contact through the whole decay, with
        // the threshold bypassed.
        if (geometry.sitarMode && level > 1.0e-4)
        {
            contact.excessMm = juce::jmax (contact.excessMm, 0.12 + 0.3 * juce::jmin (1.0, level * 4.0));

            if (contact.fret < 0)
                contact.fret = juce::jmax (1, (int) std::floor (fretted[s]) + 1);
        }

        auto* existing = generatorIndex[(size_t) s] >= 0
                           ? pool.getGenerator (NoiseClass::fretBuzz, generatorIndex[(size_t) s])
                           : nullptr;

        // A generator we started may have been stolen for another string.
        if (existing != nullptr && (! existing->isActive() || existing->getEvent().stringIndex != s))
        {
            existing = nullptr;
            generatorIndex[(size_t) s] = -1;
        }

        if (contact.excessMm > 0.0 && level > 1.0e-5)
        {
            const double buzzLevel = levelFor (contact.excessMm);

            if (existing != nullptr)
            {
                existing->setSustainLevel (buzzLevel / juce::jmax (1.0e-12, existing->getEvent().level));
            }
            else
            {
                NoiseEvent e;
                e.noiseClass = NoiseClass::fretBuzz;
                e.stringIndex = s;
                e.level = buzzLevel;

                // 3-6 kHz, rising with the contact fret.
                e.startHz = e.endHz = 3000.0 + 3000.0 * juce::jlimit (0.0, 1.0, contact.fret / 20.0);
                e.q = 3.0;
                e.brightness = juce::jlimit (0.2, 0.9, 0.3 + 0.3 * geometry.fretHeight);
                e.texture = NoiseTexture::metallic;
                e.attackMs = 0.5;
                e.decayMs = geometry.sitarMode ? 400.0 : 25.0;

                // One contact per cycle: a buzzing low E rattles at 82 Hz.
                e.burstHz = juce::jmax (20.0, fundamentalHz[s]);

                generatorIndex[(size_t) s] = pool.trigger (e);
            }

            buzzingFret[(size_t) s].store (contact.fret, std::memory_order_relaxed);
        }
        else
        {
            if (existing != nullptr)
                existing->release();

            generatorIndex[(size_t) s] = -1;
            buzzingFret[(size_t) s].store (-1, std::memory_order_relaxed);
        }
    }
}

float FretBuzz::getHeat (int stringIndex, int fret) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, SetupGeometry::kMaxStrings)
          || ! juce::isPositiveAndBelow (fret, SetupGeometry::kMaxFrets + 1))
        return -10.0f;

    return heat[(size_t) stringIndex][(size_t) fret].load (std::memory_order_relaxed);
}

int FretBuzz::getBuzzingFret (int stringIndex) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, SetupGeometry::kMaxStrings))
        return -1;

    return buzzingFret[(size_t) stringIndex].load (std::memory_order_relaxed);
}

} // namespace luthier
