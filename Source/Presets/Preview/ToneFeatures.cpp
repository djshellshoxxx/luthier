#include "ToneFeatures.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>

namespace luthier
{

namespace
{
    constexpr int kFftOrder = 11;              // 2048-point frames (6.2)
    constexpr int kFftSize = 1 << kFftOrder;
    constexpr int kHop = kFftSize / 2;

    double median (std::vector<double> v)
    {
        if (v.empty())
            return 0.0;

        std::sort (v.begin(), v.end());
        const size_t mid = v.size() / 2;
        return (v.size() % 2 == 1) ? v[mid] : 0.5 * (v[mid - 1] + v[mid]);
    }

    /** A direct-form-I biquad in double precision. */
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        double process (double x) noexcept
        {
            const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };

    /** BS.1770-4's two K-weighting stages, derived for any sample rate. */
    std::array<Biquad, 2> kWeighting (double fs)
    {
        std::array<Biquad, 2> k;

        {
            const double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double vh = std::pow (10.0, gainDb / 20.0);
            const double vb = std::pow (vh, 0.4996667741545416);
            const double a0 = 1.0 + K / q + K * K;

            k[0].b0 = (vh + vb * K / q + K * K) / a0;
            k[0].b1 = 2.0 * (K * K - vh) / a0;
            k[0].b2 = (vh - vb * K / q + K * K) / a0;
            k[0].a1 = 2.0 * (K * K - 1.0) / a0;
            k[0].a2 = (1.0 - K / q + K * K) / a0;
        }

        {
            const double f0 = 38.13547087602444, q = 0.5003270373238773;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
            const double a0 = 1.0 + K / q + K * K;

            k[1].b0 = 1.0;
            k[1].b1 = -2.0;
            k[1].b2 = 1.0;
            k[1].a1 = 2.0 * (K * K - 1.0) / a0;
            k[1].a2 = (1.0 - K / q + K * K) / a0;
        }

        return k;
    }
}

//==============================================================================
const char* ToneFeatures::spectralName (int index) noexcept
{
    static const char* const names[kNumSpectral] = {
        "centroid", "rolloff", "flatness", "lowBand", "midBand", "highBand",
        "crest", "attack", "tail", "width"
    };

    return juce::isPositiveAndBelow (index, kNumSpectral) ? names[index] : "";
}

juce::var ToneFeatures::toVar() const
{
    auto* o = new juce::DynamicObject();
    const auto v = spectralVector();

    for (int i = 0; i < kNumSpectral; ++i)
        o->setProperty (spectralName (i), v[(size_t) i]);

    o->setProperty ("loudness", loudnessLufs);
    return juce::var (o);
}

ToneFeatures ToneFeatures::fromVar (const juce::var& v)
{
    ToneFeatures f;
    auto* o = v.getDynamicObject();

    if (o == nullptr || ! o->hasProperty ("centroid"))
        return f;

    auto get = [o] (const char* key) { return (double) o->getProperty (key); };

    f.centroidLog2Hz = get ("centroid");
    f.rolloffLog2Hz = get ("rolloff");
    f.flatness = get ("flatness");
    f.lowBand = get ("lowBand");
    f.midBand = get ("midBand");
    f.highBand = get ("highBand");
    f.crestDb = get ("crest");
    f.attack = get ("attack");
    f.tailSeconds = get ("tail");
    f.width = get ("width");
    f.loudnessLufs = get ("loudness");
    f.valid = true;
    return f;
}

//==============================================================================
double ToneFeatures::integratedLoudness (const juce::AudioBuffer<float>& buffer, double fs)
{
    const int channels = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();

    if (channels == 0 || n == 0 || fs <= 0.0)
        return -70.0;

    // K-weighted squares, per channel.
    std::vector<std::vector<double>> squares ((size_t) channels, std::vector<double> ((size_t) n));

    for (int ch = 0; ch < channels; ++ch)
    {
        auto k = kWeighting (fs);
        const float* x = buffer.getReadPointer (ch);

        for (int i = 0; i < n; ++i)
        {
            const double y = k[1].process (k[0].process ((double) x[i]));
            squares[(size_t) ch][(size_t) i] = y * y;
        }
    }

    // 400 ms blocks with 75 % overlap.
    const int block = (int) std::round (0.4 * fs);
    const int step = (int) std::round (0.1 * fs);

    if (n < block)
        return -70.0;

    std::vector<double> power;

    for (int start = 0; start + block <= n; start += step)
    {
        double sum = 0.0;

        for (int ch = 0; ch < channels; ++ch)
        {
            double s = 0.0;

            for (int i = start; i < start + block; ++i)
                s += squares[(size_t) ch][(size_t) i];

            sum += s / block;   // channel weights are 1 for L and R
        }

        power.push_back (sum);
    }

    auto lufsOf = [] (double p) { return -0.691 + 10.0 * std::log10 (juce::jmax (1.0e-20, p)); };

    // Absolute gate at -70 LUFS, then the relative gate 10 LU below.
    double sum = 0.0;
    int count = 0;

    for (double p : power)
        if (lufsOf (p) > -70.0) { sum += p; ++count; }

    if (count == 0)
        return -70.0;

    const double relative = lufsOf (sum / count) - 10.0;

    sum = 0.0;
    count = 0;

    for (double p : power)
        if (lufsOf (p) > -70.0 && lufsOf (p) > relative) { sum += p; ++count; }

    return count > 0 ? lufsOf (sum / count) : -70.0;
}

double ToneFeatures::truePeakDb (const juce::AudioBuffer<float>& buffer)
{
    constexpr int factor = 4;
    constexpr int halfTaps = 16;

    // A Kaiser-windowed sinc per phase, built once.
    static const auto kernel = []
    {
        std::array<std::array<double, 2 * halfTaps>, factor> k {};
        const double beta = 8.0;
        const double i0Beta = [] (double x)
        {
            double sum = 1.0, term = 1.0;

            for (int m = 1; m < 30; ++m)
            {
                term *= (x / (2.0 * m)) * (x / (2.0 * m));
                sum += term;
            }

            return sum;
        } (beta);

        auto besselI0 = [] (double x)
        {
            double sum = 1.0, term = 1.0;

            for (int m = 1; m < 30; ++m)
            {
                term *= (x / (2.0 * m)) * (x / (2.0 * m));
                sum += term;
            }

            return sum;
        };

        for (int p = 0; p < factor; ++p)
        {
            for (int j = 0; j < 2 * halfTaps; ++j)
            {
                const double t = (double) (j - halfTaps + 1) - (double) p / factor;
                const double sinc = std::abs (t) < 1.0e-12 ? 1.0
                                  : std::sin (juce::MathConstants<double>::pi * t) / (juce::MathConstants<double>::pi * t);
                const double r = t / (double) halfTaps;
                const double w = std::abs (r) >= 1.0 ? 0.0 : besselI0 (beta * std::sqrt (1.0 - r * r)) / i0Beta;
                k[(size_t) p][(size_t) j] = sinc * w;
            }
        }

        return k;
    }();

    double peak = 0.0;
    const int n = buffer.getNumSamples();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* x = buffer.getReadPointer (ch);

        for (int i = 0; i < n; ++i)
        {
            peak = juce::jmax (peak, (double) std::abs (x[i]));

            for (int p = 1; p < factor; ++p)
            {
                double s = 0.0;

                for (int j = 0; j < 2 * halfTaps; ++j)
                {
                    const int idx = i + j - halfTaps + 1;

                    if (idx >= 0 && idx < n)
                        s += (double) x[idx] * kernel[(size_t) p][(size_t) j];
                }

                peak = juce::jmax (peak, std::abs (s));
            }
        }
    }

    return juce::Decibels::gainToDecibels (peak, -200.0);
}

//==============================================================================
double ToneFeatures::idleFloorDb (const juce::AudioBuffer<float>& idle)
{
    const int hop = 480;   // 10 ms at 48 kHz
    std::vector<double> levels;

    for (int start = 0; start + hop <= idle.getNumSamples(); start += hop)
    {
        double sum = 0.0;

        for (int ch = 0; ch < idle.getNumChannels(); ++ch)
            for (int i = start; i < start + hop; ++i)
                sum += (double) idle.getSample (ch, i) * idle.getSample (ch, i);

        levels.push_back (10.0 * std::log10 (sum / (hop * juce::jmax (1, idle.getNumChannels())) + 1.0e-12));
    }

    return levels.empty() ? -200.0 : median (levels);
}

ToneFeatures ToneFeatures::analyse (const juce::AudioBuffer<float>& buffer, double fs, double noteEndSeconds,
                                    double idleFloor)
{
    ToneFeatures f;
    const int n = buffer.getNumSamples();
    const int channels = juce::jmin (2, buffer.getNumChannels());

    if (n < kFftSize || channels == 0 || fs <= 0.0)
        return f;

    // Mono sum for the spectral features.
    std::vector<float> mono ((size_t) n);

    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;

        for (int ch = 0; ch < channels; ++ch)
            s += buffer.getSample (ch, i);

        mono[(size_t) i] = s / (float) channels;
    }

    juce::dsp::FFT fft (kFftOrder);
    std::vector<float> window ((size_t) kFftSize);

    for (int i = 0; i < kFftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) kFftSize);

    std::vector<float> frame ((size_t) (2 * kFftSize));
    std::vector<double> power ((size_t) (kFftSize / 2 + 1));

    const double binHz = fs / kFftSize;
    const int bin250 = (int) std::round (250.0 / binHz);
    const int bin2k = (int) std::round (2000.0 / binHz);
    const int bin1k = (int) std::round (1000.0 / binHz);
    const int bin5k = juce::jmin (kFftSize / 2, (int) std::round (5000.0 / binHz));

    struct FrameStats { double energy, centroid, rolloff, flatness; };
    std::vector<FrameStats> frames;
    double low = 0.0, mid = 0.0, high = 0.0;

    for (int start = 0; start + kFftSize <= n; start += kHop)
    {
        std::fill (frame.begin(), frame.end(), 0.0f);

        for (int i = 0; i < kFftSize; ++i)
            frame[(size_t) i] = mono[(size_t) (start + i)] * window[(size_t) i];

        fft.performFrequencyOnlyForwardTransform (frame.data(), true);

        double energy = 0.0, weighted = 0.0, magnitude = 0.0;

        for (int b = 1; b <= kFftSize / 2; ++b)
        {
            const double m = (double) frame[(size_t) b];
            const double p = m * m;
            power[(size_t) b] = p;
            energy += p;

            // The centroid is magnitude-weighted, as usual: weighted by power it
            // only ever finds the fundamentals.
            magnitude += m;
            weighted += m * (b * binHz);

            if (b < bin250)      low += p;
            else if (b < bin2k)  mid += p;
            else                 high += p;
        }

        if (energy <= 1.0e-18)
            continue;

        double cumulative = 0.0, rolloffHz = binHz;

        for (int b = 1; b <= kFftSize / 2; ++b)
        {
            cumulative += power[(size_t) b];

            if (cumulative >= 0.85 * energy)
            {
                rolloffHz = b * binHz;
                break;
            }
        }

        double logSum = 0.0, linSum = 0.0;
        int count = 0;

        for (int b = bin1k; b <= bin5k; ++b)
        {
            const double p = power[(size_t) b] + 1.0e-20;
            logSum += std::log (p);
            linSum += p;
            ++count;
        }

        const double flat = count > 0 ? std::exp (logSum / count) / (linSum / count) : 0.0;

        frames.push_back ({ energy, std::log2 (juce::jmax (1.0, magnitude > 0.0 ? weighted / magnitude : 1.0)),
                            std::log2 (juce::jmax (1.0, rolloffHz)), flat });
    }

    if (frames.empty())
        return f;

    // Frames within 50 dB of the loudest carry the sound; the rest is silence.
    double loudest = 0.0;

    for (const auto& fr : frames)
        loudest = juce::jmax (loudest, fr.energy);

    std::vector<double> centroids, rolloffs, flats;

    for (const auto& fr : frames)
    {
        if (fr.energy < loudest * 1.0e-5)
            continue;

        centroids.push_back (fr.centroid);
        rolloffs.push_back (fr.rolloff);
        flats.push_back (fr.flatness);
    }

    f.centroidLog2Hz = median (centroids);
    f.rolloffLog2Hz = median (rolloffs);
    f.flatness = median (flats);

    const double total = juce::jmax (1.0e-20, low + mid + high);
    f.lowBand = low / total;
    f.midBand = mid / total;
    f.highBand = high / total;

    // Crest: peak over RMS of the whole clip.
    double peak = 0.0, sumSquares = 0.0;

    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
        {
            const double s = buffer.getSample (ch, i);
            peak = juce::jmax (peak, std::abs (s));
            sumSquares += s * s;
        }

    const double rms = std::sqrt (sumSquares / (double) (n * channels));
    f.crestDb = rms > 0.0 ? juce::Decibels::gainToDecibels (peak / rms) : 0.0;

    // Attack: mean positive jump of 10 ms log-energy over its 30 ms-earlier value,
    // over the jumps that are onsets (above 6 dB).
    {
        const int hop = juce::jmax (1, (int) std::round (0.010 * fs));
        std::vector<double> env;

        for (int start = 0; start + hop <= n; start += hop)
        {
            double s = 0.0;

            for (int i = start; i < start + hop; ++i)
                s += (double) mono[(size_t) i] * mono[(size_t) i];

            env.push_back (10.0 * std::log10 (s / hop + 1.0e-12));
        }

        constexpr double kSilenceDb = -100.0;
        double onsetSum = 0.0;
        int onsets = 0;

        // The clip's lead-in is digital silence (the engine's latency before the
        // first string sounds, 10-20 ms depending on the preset's chain). The jump
        // out of it is the clip starting, not a pick attack; counting it made a
        // riff with few other onsets (Modern Metal Chug) read ~100 dB, an outlier
        // that pushed it away from every other high-gain preset.
        size_t first = 0;

        while (first < env.size() && env[first] < kSilenceDb)
            ++first;

        for (size_t i = first + 3; i < env.size(); ++i)
        {
            const double rise = env[i] - env[i - 3];

            if (rise > 6.0 && env[i] > 10.0 * std::log10 (loudest / kFftSize + 1.0e-12) - 60.0)
            {
                onsetSum += rise;
                ++onsets;
            }
        }

        f.attack = onsets > 0 ? onsetSum / onsets : 0.0;

        // Tail: from the last note-off, how long until 30 dB below the level there.
        const size_t offFrame = (size_t) juce::jlimit (0.0, (double) env.size() - 1.0,
                                                       std::floor (noteEndSeconds / 0.010));
        double reference = -200.0;

        for (size_t i = offFrame; i < juce::jmin (env.size(), offFrame + 5); ++i)
            reference = juce::jmax (reference, env[i]);

        /*  30 dB down, or to within 6 dB of the rig's idle noise if that is
            nearer: a high-gain amp's hiss is not the note's tail. (The idle
            level is mono-summed mean square, as `env` is, give or take the
            sum's 3 dB, which the margin covers.) */
        const double target = juce::jmax (reference - 30.0, idleFloor + 6.0);
        size_t endFrame = env.size();

        for (size_t i = offFrame; i < env.size(); ++i)
            if (env[i] < target)
            {
                endFrame = i;
                break;
            }

        f.tailSeconds = (double) (endFrame - offFrame) * 0.010;
    }

    // Width: 1 minus the L/R correlation.
    if (channels == 2)
    {
        double lr = 0.0, ll = 0.0, rr = 0.0;
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getReadPointer (1);

        for (int i = 0; i < n; ++i)
        {
            lr += (double) l[i] * r[i];
            ll += (double) l[i] * l[i];
            rr += (double) r[i] * r[i];
        }

        f.width = (ll > 0.0 && rr > 0.0) ? 1.0 - lr / std::sqrt (ll * rr) : 0.0;
    }

    f.loudnessLufs = integratedLoudness (buffer, fs);
    f.valid = true;
    return f;
}

} // namespace luthier
