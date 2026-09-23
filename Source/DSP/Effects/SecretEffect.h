#pragma once

/*  "Wolf" - the hidden effect.

    Not a joke feature: it is a real, musically useful processor that happens to be
    hidden. A wolf tone is what happens when a note lands exactly on a strong body
    resonance and the two feed each other until the note howls. This exaggerates
    that: a dispersive feedback delay whose length is modulated, so transients
    smear into a rising, chirping swell and sustained notes bloom into a howl.

    It is built from the same parts as the spring reverb - an all-pass cascade in a
    feedback loop - which is why it sounds like an instrument misbehaving rather
    than like a digital effect.
*/

#include "../Common/DspCommon.h"
#include <vector>

namespace luthier
{

class SecretEffect
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        const int maxDelay = (int) (sr * 0.12) + 8;
        size = juce::nextPowerOfTwo (juce::jmax (256, maxDelay));
        mask = size - 1;

        bufferL.assign ((size_t) size, 0.0);
        bufferR.assign ((size_t) size, 0.0);
        writeIndex = 0;

        lfo.prepare (sr);
        lfo.setShape (Lfo::Shape::Sine);

        toneL.prepare (sr);
        toneR.prepare (sr);
        toneL.setCutoff (4200.0);
        toneR.setCutoff (4200.0);

        dcL.prepare (sr, 18.0);
        dcR.prepare (sr, 18.0);

        mixSmooth.prepare (sr, constants::kParamSmoothSeconds);
        mixSmooth.snapTo (0.0);

        setRate (rateHz);
        setDepth (depth);

        reset();
    }

    void reset() noexcept
    {
        std::fill (bufferL.begin(), bufferL.end(), 0.0);
        std::fill (bufferR.begin(), bufferR.end(), 0.0);
        writeIndex = 0;

        for (int i = 0; i < kStages; ++i)
        {
            apL[i].reset();
            apR[i].reset();
        }

        toneL.reset();
        toneR.reset();
        dcL.reset();
        dcR.reset();
        lfo.reset();
        mixSmooth.snapToTarget();
    }

    void setEnabled (bool e) noexcept { enabled = e; }
    bool isEnabled() const noexcept { return enabled; }

    void setRate (double hz) noexcept
    {
        rateHz = juce::jlimit (0.02, 8.0, hz);
        lfo.setRate (rateHz);
    }

    void setDepth (double d) noexcept
    {
        depth = juce::jlimit (0.0, 1.0, d);

        // Deeper warp means more dispersion as well as more delay modulation,
        // which is what turns a wobble into a howl.
        const double coeff = -juce::jmap (depth, 0.0, 1.0, 0.15, 0.78);

        for (int i = 0; i < kStages; ++i)
        {
            apL[i].setCoefficient (coeff);
            apR[i].setCoefficient (coeff * 0.96);
        }
    }

    void setFeedback (double f) noexcept
    {
        // Hard-capped below unity: the effect is meant to howl, not to run away.
        feedback = juce::jlimit (0.0, 0.92, f);
    }

    void setMix (double m) noexcept { mixSmooth.setTarget (juce::jlimit (0.0, 1.0, m)); }

    void process (double* left, double* right, int numSamples) noexcept
    {
        if (! enabled || bufferL.empty())
        {
            // Still advance the mix smoother so re-engaging is not a jump.
            for (int i = 0; i < numSamples; ++i)
                mixSmooth.next();

            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const double mod = lfo.next();

            // 8 to 70 ms, modulated. Short enough to be resonant, long enough to
            // be heard as a separate event on the slow settings.
            const double delayMs = 26.0 + mod * depth * 18.0;
            const double delaySamples = juce::jlimit (4.0, (double) (size - 4), delayMs * 0.001 * sr);

            const int i0 = (int) delaySamples;
            const double frac = delaySamples - (double) i0;

            auto read = [this, i0, frac] (const std::vector<double>& buf) noexcept
            {
                const double a = buf[(size_t) ((writeIndex - i0) & mask)];
                const double b = buf[(size_t) ((writeIndex - i0 - 1) & mask)];
                return a * (1.0 - frac) + b * frac;
            };

            double wetL = read (bufferL);
            double wetR = read (bufferR);

            for (int s = 0; s < kStages; ++s)
            {
                wetL = apL[s].process (wetL);
                wetR = apR[s].process (wetR);
            }

            wetL = toneL.process (wetL);
            wetR = toneR.process (wetR);

            wetL = sanitise (wetL);
            wetR = sanitise (wetR);

            // Cross-coupled feedback: the two channels feed each other, which is
            // what makes the howl swirl rather than sit still.
            bufferL[(size_t) writeIndex] = flushDenormal (softClip (left[i] + wetR * feedback));
            bufferR[(size_t) writeIndex] = flushDenormal (softClip (right[i] + wetL * feedback));

            writeIndex = (writeIndex + 1) & mask;

            const double mix = mixSmooth.next();

            left[i]  = sanitise (dcL.process (left[i]  * (1.0 - mix * 0.4) + wetL * mix * 1.2));
            right[i] = sanitise (dcR.process (right[i] * (1.0 - mix * 0.4) + wetR * mix * 1.2));
        }
    }

private:
    static constexpr int kStages = 8;

    double sr = 44100.0;
    bool enabled = false;

    double rateHz = 0.35, depth = 0.55, feedback = 0.62;

    std::vector<double> bufferL, bufferR;
    int size = 0, mask = 0, writeIndex = 0;

    Allpass1 apL[kStages], apR[kStages];
    OnePoleLP toneL, toneR;
    DCBlocker dcL, dcR;
    Lfo lfo;
    ExpSmoother mixSmooth;
};

} // namespace luthier
