#include "Bs1770Meter.h"

#include <algorithm>

namespace luthier
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    double energyToLufs (double energy)
    {
        return energy > 0.0 ? -0.691 + 10.0 * std::log10 (energy) : Bs1770Meter::kSilenceLufs;
    }
}

void Bs1770Meter::makeKWeighting (double sampleRate, Stage& shelfStage, Stage& highpassStage)
{
    // Stage 1: the high shelf (BS.1770-4 table 1 at 48 kHz, from its analogue
    // prototype so any rate gets the same response).
    {
        const double f0 = 1681.974450955533;
        const double G  = 3.999843853973347;
        const double Q  = 0.7071752369554196;

        const double K  = std::tan (kPi * f0 / sampleRate);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;

        shelfStage.b0 = (Vh + Vb * K / Q + K * K) / a0;
        shelfStage.b1 = 2.0 * (K * K - Vh) / a0;
        shelfStage.b2 = (Vh - Vb * K / Q + K * K) / a0;
        shelfStage.a1 = 2.0 * (K * K - 1.0) / a0;
        shelfStage.a2 = (1.0 - K / Q + K * K) / a0;
    }

    // Stage 2: the RLB highpass.
    {
        const double f0 = 38.13547087602444;
        const double Q  = 0.5003270373238773;
        const double K  = std::tan (kPi * f0 / sampleRate);
        const double a0 = 1.0 + K / Q + K * K;

        highpassStage.b0 = 1.0;
        highpassStage.b1 = -2.0;
        highpassStage.b2 = 1.0;
        highpassStage.a1 = 2.0 * (K * K - 1.0) / a0;
        highpassStage.a2 = (1.0 - K / Q + K * K) / a0;
    }

    shelfStage.clear();
    highpassStage.clear();
}

void Bs1770Meter::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    channels = std::clamp (numChannels, 1, 2);
    hopLength = std::max (1, (int) std::lround (sr * 0.1));

    for (int ch = 0; ch < 2; ++ch)
        makeKWeighting (sr, shelf[ch], hp[ch]);

    hopEnergy.reserve (1024);
    reset();
}

void Bs1770Meter::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        shelf[ch].clear();
        hp[ch].clear();
    }

    hopAccum = 0.0;
    hopCount = 0;
    hopEnergy.clear();
}

void Bs1770Meter::process (const float* left, const float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        double sum = 0.0;

        {
            const double x = left != nullptr ? (double) left[i] : 0.0;
            const double k = hp[0].process (shelf[0].process (std::isfinite (x) ? x : 0.0));
            sum += k * k;
        }

        if (channels > 1)
        {
            const double x = right != nullptr ? (double) right[i] : (left != nullptr ? (double) left[i] : 0.0);
            const double k = hp[1].process (shelf[1].process (std::isfinite (x) ? x : 0.0));
            sum += k * k;
        }

        hopAccum += sum;

        if (++hopCount >= hopLength)
        {
            hopEnergy.push_back (hopAccum / (double) hopLength);
            hopAccum = 0.0;
            hopCount = 0;
        }
    }
}

double Bs1770Meter::getMaxMomentaryLufs() const
{
    double best = kSilenceLufs;

    for (size_t i = 3; i < hopEnergy.size(); ++i)
    {
        const double e = 0.25 * (hopEnergy[i] + hopEnergy[i - 1] + hopEnergy[i - 2] + hopEnergy[i - 3]);
        best = std::max (best, energyToLufs (e));
    }

    return best;
}

double Bs1770Meter::getIntegratedLufs() const
{
    // 400 ms blocks at a 100 ms hop: every run of four consecutive hops.
    std::vector<double> blocks;

    for (size_t i = 3; i < hopEnergy.size(); ++i)
        blocks.push_back (0.25 * (hopEnergy[i] + hopEnergy[i - 1] + hopEnergy[i - 2] + hopEnergy[i - 3]));

    double sum = 0.0;
    int count = 0;

    for (double e : blocks)
        if (energyToLufs (e) > kAbsoluteGateLufs)
        {
            sum += e;
            ++count;
        }

    if (count == 0)
        return kSilenceLufs;

    const double relativeGate = energyToLufs (sum / count) + kRelativeGateLu;

    double gatedSum = 0.0;
    int gatedCount = 0;

    for (double e : blocks)
    {
        const double l = energyToLufs (e);

        if (l > kAbsoluteGateLufs && l > relativeGate)
        {
            gatedSum += e;
            ++gatedCount;
        }
    }

    return gatedCount > 0 ? energyToLufs (gatedSum / gatedCount) : kSilenceLufs;
}

} // namespace luthier
