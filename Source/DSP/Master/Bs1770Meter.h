#pragma once

/*  Integrated loudness per ITU-R BS.1770-4 (output-normalization.md 4.3).

    - K-weighting: the two BS.1770 biquads (a +4 dB high shelf near 1682 Hz
      and an RLB highpass near 38 Hz), with the coefficients recomputed for the
      sample rate from their analogue prototypes, as libebur128 does, so the
      response is the standard's at any rate.
    - 400 ms gating blocks with 75 % overlap (a 100 ms hop).
    - An absolute gate at -70 LUFS, then a relative gate 10 LU below the
      absolute-gated loudness.

    Used by NormalizationCalibrator on its worker, and meant to be the one
    implementation the preset-preview renderer shares
    (preset-browser-previews.md 3.2 step 4). Not for the audio thread: the
    per-hop energies grow a vector. The existing short-term meter in MasterBus
    is separate and unchanged.
*/

#include <vector>
#include <cmath>

namespace luthier
{

class Bs1770Meter
{
public:
    /** Silence (and anything below the absolute gate) reads this. */
    static constexpr double kSilenceLufs = -120.0;
    static constexpr double kAbsoluteGateLufs = -70.0;
    static constexpr double kRelativeGateLu = -10.0;

    void prepare (double sampleRate, int numChannels = 2);
    void reset();

    /** Feeds `numSamples` of each channel. `right` may be nullptr for mono. */
    void process (const float* left, const float* right, int numSamples);

    /** Gated integrated loudness of everything fed since reset(), in LUFS.
        kSilenceLufs when no block passes the absolute gate. */
    double getIntegratedLufs() const;

    /** The loudest 400 ms block, ungated (for diagnostics). */
    double getMaxMomentaryLufs() const;

    /** One biquad in direct form I, double precision. */
    struct Stage
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        double process (double x) noexcept
        {
            const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }

        void clear() noexcept { x1 = x2 = y1 = y2 = 0; }
    };

    /** The K-weighting pair for a sample rate (exposed for tests). */
    static void makeKWeighting (double sampleRate, Stage& shelf, Stage& highpass);

private:
    double sr = 48000.0;
    int channels = 2;
    int hopLength = 4800;

    Stage shelf[2], hp[2];

    double hopAccum = 0.0;          ///< sum over channels of squared K-weighted samples
    int hopCount = 0;
    std::vector<double> hopEnergy;  ///< per 100 ms hop: sum of mean squares
};

} // namespace luthier
