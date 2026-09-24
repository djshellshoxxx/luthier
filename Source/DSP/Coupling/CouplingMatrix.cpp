#include "CouplingMatrix.h"

namespace luthier
{

void CouplingMatrix::prepare (double sampleRate, int n) noexcept
{
    sr = sampleRate;
    setNumStrings (n);

    for (int i = 0; i < kMaxStrings; ++i)
    {
        frequencies[(size_t) i] = 110.0;
        receiveDC[(size_t) i].prepare (sr, 8.0);
        receiveFilter[(size_t) i].setBandpass (sr, 110.0, 1.2);
    }

    buildDefault();
    reset();
}

void CouplingMatrix::reset() noexcept
{
    for (int i = 0; i < kMaxStrings; ++i)
    {
        receiveFilter[(size_t) i].reset();
        receiveDC[(size_t) i].reset();
    }

    lastLimiting = 0.0;

    // Forget the designed pitches: setStringFrequency skips a move under 0.5 Hz,
    // so the filters would otherwise be designed at wherever the last render
    // left them, and the next render would depend on the one before.
    frequencies.fill (0.0);

    // Pitches unknown: every pair at full strength until they arrive.
    for (auto& row : unisonScale)
        row.fill (1.0);
}

void CouplingMatrix::setNumStrings (int n) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
}

//==============================================================================
void CouplingMatrix::buildDefault (double baseCoupling) noexcept
{
    const double base = juce::jlimit (0.0, 0.10, baseCoupling);

    for (int i = 0; i < kMaxStrings; ++i)
    {
        for (int j = 0; j < kMaxStrings; ++j)
        {
            if (i == j)
            {
                matrix[(size_t) i][(size_t) j] = 0.0;
                continue;
            }

            // Adjacent saddles share more of the bridge, so coupling falls off
            // with string distance. The floor keeps the far strings ringing a
            // little, which is audible on open chords.
            const int distance = std::abs (i - j);
            const double falloff = 1.0 / (1.0 + 0.85 * (double) (distance - 1));

            matrix[(size_t) i][(size_t) j] = base * juce::jmax (0.25, falloff);
        }
    }
}

void CouplingMatrix::setPairCoupling (int i, int j, double value) noexcept
{
    if (! juce::isPositiveAndBelow (i, kMaxStrings) || ! juce::isPositiveAndBelow (j, kMaxStrings))
        return;

    if (i == j)
        return;

    const double v = juce::jlimit (0.0, 0.12, value);

    // The bridge is a passive mechanical link, so coupling is symmetric.
    matrix[(size_t) i][(size_t) j] = v;
    matrix[(size_t) j][(size_t) i] = v;
}

double CouplingMatrix::getPairCoupling (int i, int j) const noexcept
{
    if (! juce::isPositiveAndBelow (i, kMaxStrings) || ! juce::isPositiveAndBelow (j, kMaxStrings))
        return 0.0;

    return matrix[(size_t) i][(size_t) j];
}

void CouplingMatrix::setStringFrequency (int stringIndex, double hz) noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    const double f = juce::jlimit (constants::kMinStringHz, sr * 0.45, hz);

    // Only redesign when the pitch has actually moved enough to matter: this is
    // called every block and biquad design is not free.
    if (std::abs (f - frequencies[(size_t) stringIndex]) < 0.5)
        return;

    frequencies[(size_t) stringIndex] = f;

    // A fairly wide resonance: the receiving string responds to anything near its
    // fundamental and its low partials, not just an exact match.
    receiveFilter[(size_t) stringIndex].setBandpass (sr, f, 1.1);

    updateUnisonScale (stringIndex);
}

void CouplingMatrix::updateUnisonScale (int i) noexcept
{
    for (int j = 0; j < kMaxStrings; ++j)
    {
        double scale = 1.0;

        if (j != i && frequencies[(size_t) i] > 0.0 && frequencies[(size_t) j] > 0.0)
        {
            const double cents = std::abs (1200.0 * std::log2 (frequencies[(size_t) i] / frequencies[(size_t) j]));
            const double t = juce::jlimit (0.0, 1.0, cents / kUnisonCents);
            scale = kUnisonFloor + (1.0 - kUnisonFloor) * t * t;
        }

        unisonScale[(size_t) i][(size_t) j] = scale;
        unisonScale[(size_t) j][(size_t) i] = scale;
    }
}

//==============================================================================
void CouplingMatrix::process (const double* bridgeOutputs, double* couplingInputs) noexcept
{
    if (bridgeOutputs == nullptr || couplingInputs == nullptr)
        return;

    lastLimiting = 0.0;

    for (int i = 0; i < numStrings; ++i)
    {
        double sum = 0.0;

        for (int j = 0; j < numStrings; ++j)
        {
            if (i == j)
                continue;

            sum += matrix[(size_t) i][(size_t) j] * unisonScale[(size_t) i][(size_t) j] * bridgeOutputs[j];
        }

        sum *= globalAmount;

        // Shape the arriving energy with the receiving string's own resonance,
        // then block DC so a long chord cannot walk the delay line off centre.
        double shaped = receiveFilter[(size_t) i].process (sum);
        shaped = receiveDC[(size_t) i].process (shaped);

        // Safety cap (engine spec 5.6): a hard ceiling on what any one string can
        // receive per sample makes feedback runaway impossible regardless of how
        // the matrix was edited.
        if (std::abs (shaped) > kEnergyCap)
        {
            lastLimiting = juce::jmax (lastLimiting, std::abs (shaped) - kEnergyCap);
            shaped = std::copysign (kEnergyCap, shaped);
        }

        couplingInputs[i] = sanitise (shaped);
    }

    for (int i = numStrings; i < kMaxStrings; ++i)
        couplingInputs[i] = 0.0;
}

} // namespace luthier
