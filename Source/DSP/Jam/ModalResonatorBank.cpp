#include "ModalResonatorBank.h"

namespace luthier
{

void ModalResonatorBank::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (1000.0, sampleRate);

    for (int m = 0; m < kMaxModes; ++m)
        updateCoefficients (m);

    reset();
}

void ModalResonatorBank::reset() noexcept
{
    y1.fill (0.0);
    y2.fill (0.0);
    running = false;
}

void ModalResonatorBank::setNumModes (int n) noexcept
{
    numModes = juce::jlimit (0, kMaxModes, n);
    paddedModes = juce::jmin (kMaxModes, (numModes + 3) & ~3);

    // The padding lanes must be silent whatever they held before; a mode
    // brought back keeps the settings it was given.
    for (int m = 0; m < kMaxModes; ++m)
    {
        updateCoefficients (m);

        if (m >= numModes)
            y1[(size_t) m] = y2[(size_t) m] = 0.0;
    }
}

void ModalResonatorBank::setMode (int index, double hz, double t60Seconds, double modeGain) noexcept
{
    if (! juce::isPositiveAndBelow (index, kMaxModes))
        return;

    freq[(size_t) index] = hz;
    t60[(size_t) index] = t60Seconds;
    gain[(size_t) index] = modeGain;
    updateCoefficients (index);
}

void ModalResonatorBank::setModeFrequency (int index, double hz) noexcept
{
    if (! juce::isPositiveAndBelow (index, kMaxModes))
        return;

    freq[(size_t) index] = hz;
    updateCoefficients (index);
}

void ModalResonatorBank::setModeDecay (int index, double t60Seconds) noexcept
{
    if (! juce::isPositiveAndBelow (index, kMaxModes))
        return;

    t60[(size_t) index] = t60Seconds;
    updateCoefficients (index);
}

void ModalResonatorBank::setModeGain (int index, double modeGain) noexcept
{
    if (! juce::isPositiveAndBelow (index, kMaxModes))
        return;

    gain[(size_t) index] = modeGain;
    updateCoefficients (index);
}

void ModalResonatorBank::updateCoefficients (int m) noexcept
{
    const double hz = freq[(size_t) m];

    if (m >= numModes || hz <= 0.0 || hz >= sr * 0.45)
    {
        // Past Nyquist's margin a mode would alias: it is left out, not folded.
        a1[(size_t) m] = 0.0;
        a2[(size_t) m] = 0.0;
        b[(size_t) m] = 0.0;
        return;
    }

    const double w = constants::kTwoPi * hz / sr;
    const double decay = juce::jmax (1.0e-4, t60[(size_t) m]);
    const double r = std::pow (10.0, -3.0 / (decay * sr));

    a1[(size_t) m] = 2.0 * r * std::cos (w);
    a2[(size_t) m] = r * r;
    b[(size_t) m] = std::sin (w) * gain[(size_t) m];
}

namespace
{
    /** One group of modes over a block. `N` independent recursions run
        interleaved so the multiply latency of each is hidden behind the
        others (the loop is latency-bound with fewer), and the compiler packs
        them into SIMD lanes. */
    template <int N, bool HasInput>
    inline void runGroup (const double* c1, const double* c2, const double* g, double* y1, double* y2,
                          const double* in, double* out, int n) noexcept
    {
        double p[N], q[N], a[N], b[N], k[N];

        for (int j = 0; j < N; ++j)
        {
            p[j] = y1[j]; q[j] = y2[j];
            a[j] = c1[j]; b[j] = c2[j]; k[j] = g[j];
        }

        for (int i = 0; i < n; ++i)
        {
            double v[N];
            const double x = HasInput ? in[i] : 0.0;

            for (int j = 0; j < N; ++j)
                v[j] = a[j] * p[j] - b[j] * q[j] + (HasInput ? k[j] * x : 0.0);

            double sum = 0.0;

            for (int j = 0; j < N; ++j)
            {
                q[j] = p[j];
                p[j] = v[j];
                sum += v[j];
            }

            out[i] += sum;
        }

        for (int j = 0; j < N; ++j)
        {
            y1[j] = p[j];
            y2[j] = q[j];
        }
    }
}

void ModalResonatorBank::processBlock (const double* in, double* out, int n) noexcept
{
    if (! running)
    {
        bool any = false;

        if (in != nullptr)
            for (int i = 0; i < n && ! any; ++i)
                any = in[i] != 0.0;

        if (! any)
            return;

        running = true;
    }

    int m = 0;

    // Eight at a time, then the last four.
    for (; m + 8 <= paddedModes; m += 8)
    {
        if (in != nullptr)
            runGroup<8, true>  (&a1[(size_t) m], &a2[(size_t) m], &b[(size_t) m], &y1[(size_t) m], &y2[(size_t) m], in, out, n);
        else
            runGroup<8, false> (&a1[(size_t) m], &a2[(size_t) m], &b[(size_t) m], &y1[(size_t) m], &y2[(size_t) m], in, out, n);
    }

    for (; m < paddedModes; m += 4)
    {
        if (in != nullptr)
            runGroup<4, true>  (&a1[(size_t) m], &a2[(size_t) m], &b[(size_t) m], &y1[(size_t) m], &y2[(size_t) m], in, out, n);
        else
            runGroup<4, false> (&a1[(size_t) m], &a2[(size_t) m], &b[(size_t) m], &y1[(size_t) m], &y2[(size_t) m], in, out, n);
    }
}

bool ModalResonatorBank::housekeep() noexcept
{
    if (! running)
        return true;

    double level = 0.0;
    bool finite = true;

    for (int m = 0; m < paddedModes; ++m)
    {
        const double v1 = y1[(size_t) m], v2 = y2[(size_t) m];

        if (! std::isfinite (v1) || ! std::isfinite (v2) || std::abs (v1) > 1.0e6)
        {
            finite = false;
            break;
        }

        y1[(size_t) m] = flushDenormal (v1);
        y2[(size_t) m] = flushDenormal (v2);
        level = juce::jmax (level, std::abs (v1), std::abs (v2));
    }

    if (! finite)
    {
        // engine.md 0.3 / jam-mode 13: clamp out, reset the piece, count it.
        reset();
        getNanResetCounter().fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    if (level < kSilence)
        reset();

    return true;
}

double ModalResonatorBank::getStateLevel() const noexcept
{
    double level = 0.0;

    for (int m = 0; m < paddedModes; ++m)
        level = juce::jmax (level, std::abs (y1[(size_t) m]));

    return level;
}

void ModalResonatorBank::scaleState (double g) noexcept
{
    for (int m = 0; m < paddedModes; ++m)
    {
        y1[(size_t) m] *= g;
        y2[(size_t) m] *= g;
    }
}

void ModalResonatorBank::copyFrom (const ModalResonatorBank& other) noexcept
{
    sr = other.sr;
    numModes = other.numModes;
    paddedModes = other.paddedModes;
    running = other.running;
    a1 = other.a1; a2 = other.a2; b = other.b;
    y1 = other.y1; y2 = other.y2;
    freq = other.freq; t60 = other.t60; gain = other.gain;
}

std::atomic<int>& ModalResonatorBank::getNanResetCounter() noexcept
{
    static std::atomic<int> counter { 0 };
    return counter;
}

} // namespace luthier
