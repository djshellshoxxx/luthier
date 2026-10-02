#include "PedalsMod.h"

namespace luthier
{

namespace
{
    const char* const kLfoShapes[]    = { "Sine", "Triangle", "Square", "Random" };
    const char* const kDelayChar[]    = { "Digital", "Analogue", "Tape" };
    const char* const kSyncDiv[]      = { "Free", "1/1", "1/2", "1/4", "1/4.", "1/8", "1/8.", "1/8T", "1/16" };
    const char* const kOnOff[]        = { "Off", "On" };
    const char* const kReverbChar[]   = { "Plate", "Hall", "Room", "Chamber" };
    const char* const kRotarySpeed[]  = { "Slow", "Fast" };

    /** Beat multiplier for each entry of kSyncDiv, relative to a quarter note. */
    const double kSyncMultipliers[] = { 1.0, 4.0, 2.0, 1.0, 1.5, 0.5, 0.75, 1.0 / 3.0, 0.25 };
}

//==============================================================================
//  ModDelayLine
//==============================================================================
void ModDelayLine::prepare (double sampleRate, double maxDelayMs)
{
    sr = sampleRate;

    const int required = (int) std::ceil (sr * maxDelayMs * 0.001) + 8;
    size = juce::nextPowerOfTwo (juce::jmax (64, required));
    mask = size - 1;

    buffer.assign ((size_t) size, 0.0);
    writeIndex = 0;
}

void ModDelayLine::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0);
    writeIndex = 0;
}

void ModDelayLine::write (double input) noexcept
{
    if (buffer.empty())
        return;

    buffer[(size_t) writeIndex] = flushDenormal (input);
    writeIndex = (writeIndex + 1) & mask;
}

double ModDelayLine::tap (double delayMs) const noexcept
{
    if (buffer.empty())
        return 0.0;

    const double d = juce::jlimit (1.0, (double) (size - 3), delayMs * 0.001 * sr);

    const int i0 = (int) d;
    const double frac = d - (double) i0;

    // Cubic interpolation: a modulated delay read with linear interpolation has
    // audible high-frequency loss that varies with the modulation, which is heard
    // as a "swishing" artefact on a chorus.
    const double y0 = buffer[(size_t) ((writeIndex - i0 + 1) & mask)];
    const double y1 = buffer[(size_t) ((writeIndex - i0)     & mask)];
    const double y2 = buffer[(size_t) ((writeIndex - i0 - 1) & mask)];
    const double y3 = buffer[(size_t) ((writeIndex - i0 - 2) & mask)];

    const double c0 = y1;
    const double c1 = 0.5 * (y2 - y0);
    const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
    const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

double ModDelayLine::process (double input, double delayMs) noexcept
{
    const double out = tap (delayMs);
    write (input);
    return out;
}

//==============================================================================
//  Chorus
//==============================================================================
void ChorusPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);

    for (int v = 0; v < kMaxVoices; ++v)
    {
        lineL[v].prepare (sr, 60.0);
        lineR[v].prepare (sr, 60.0);
        lfo[v].prepare (sr);
        lfo[v].setShape (Lfo::Shape::Sine);

        // Spreading the voices evenly around the LFO cycle keeps the summed
        // modulation smooth instead of lumpy.
        lfo[v].setPhase ((double) v / (double) kMaxVoices);
    }

    resetParametersToDefault();
    reset();
}

void ChorusPedal::reset() noexcept
{
    for (int v = 0; v < kMaxVoices; ++v)
    {
        lineL[v].reset();
        lineR[v].reset();
        lfo[v].reset();
        lfo[v].setPhase ((double) v / (double) kMaxVoices);
    }
}

const PedalParam& ChorusPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Rate",   "Hz",  0.05,  8.0,  0.60, 0.5, false, 0, nullptr },
        { "Depth",  "ms",  0.10, 12.0,  3.50, 0.7, false, 0, nullptr },
        { "Delay",  "ms",  5.00, 40.0, 18.00, 1.0, false, 0, nullptr },
        { "Voices", "",    1.00,  4.0,  2.00, 1.0, true,  4, nullptr },
        { "Mix",    "",    0.00,  1.0,  0.45, 1.0, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 4, index)];
}

void ChorusPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: rateHz = value; break;
        case 1: depthMs = value; break;
        case 2: centreMs = value; break;
        case 3: numVoices = juce::jlimit (1, kMaxVoices, (int) value + 1); break;
        case 4: mix = value; break;
        default: break;
    }

    for (int v = 0; v < kMaxVoices; ++v)
    {
        // Slightly detuned rates per voice: identical rates sum into one thicker
        // voice rather than a chorus.
        lfo[v].setRate (rateHz * (1.0 + 0.11 * (double) v));
    }
}

void ChorusPedal::process (double* left, double* right, int numSamples) noexcept
{
    const double norm = 1.0 / std::sqrt ((double) numVoices);

    for (int i = 0; i < numSamples; ++i)
    {
        double wetL = 0.0, wetR = 0.0;

        for (int v = 0; v < numVoices; ++v)
        {
            const double m = lfo[v].next();

            // The two channels read opposite sides of the same modulation, which
            // widens the image. They are never polarity-inverted, so the effect
            // stays mono-compatible.
            const double dl = centreMs + m * depthMs;
            const double dr = centreMs - m * depthMs;

            wetL += lineL[v].process (left[i], juce::jmax (0.5, dl));
            wetR += lineR[v].process (right[i], juce::jmax (0.5, dr));
        }

        wetL *= norm;
        wetR *= norm;

        left[i]  = sanitise (left[i]  * (1.0 - mix * 0.5) + wetL * mix);
        right[i] = sanitise (right[i] * (1.0 - mix * 0.5) + wetR * mix);
    }
}

//==============================================================================
//  Phaser
//==============================================================================
void PhaserPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    lfo.prepare (sr);
    lfo.setShape (Lfo::Shape::Sine);
    resetParametersToDefault();
    reset();
}

void PhaserPedal::reset() noexcept
{
    for (int i = 0; i < kMaxStages; ++i)
    {
        apL[i].reset();
        apR[i].reset();
    }

    lfo.reset();
    fbL = fbR = 0.0;
}

const PedalParam& PhaserPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Rate",     "Hz", 0.02, 8.0, 0.40, 0.5, false, 0, nullptr },
        { "Depth",    "",   0.00, 1.0, 0.70, 1.0, false, 0, nullptr },
        { "Feedback", "",   0.00, 0.9, 0.35, 1.0, false, 0, nullptr },
        { "Stages",   "",   0.00, 3.0, 0.00, 1.0, true,  4, nullptr },
        { "Mix",      "",   0.00, 1.0, 0.50, 1.0, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 4, index)];
}

void PhaserPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: rateHz = value; lfo.setRate (rateHz); break;
        case 1: depth = value; break;
        case 2: feedback = value; break;
        case 3:
        {
            const int table[4] = { 4, 6, 8, 12 };
            stages = table[juce::jlimit (0, 3, (int) value)];
            break;
        }
        case 4: mix = value; break;
        default: break;
    }
}

void PhaserPedal::process (double* left, double* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const double m = lfo.next() * depth;

        // Sweep the all-pass corner between roughly 200 Hz and 2 kHz. Converting
        // the corner into an all-pass coefficient directly is cheaper than
        // redesigning a biquad and is exact for a first-order section.
        const double freqL = 350.0 * std::pow (2.0, (m + 1.0) * 1.6);
        const double freqR = 350.0 * std::pow (2.0, (-m + 1.0) * 1.6);

        auto coeffFor = [this] (double f) noexcept
        {
            const double t = std::tan (constants::kPi * juce::jlimit (20.0, sr * 0.45, f) / sr);
            return (t - 1.0) / (t + 1.0);
        };

        const double cl = coeffFor (freqL);
        const double cr = coeffFor (freqR);

        double xl = left[i] + fbL * feedback;
        double xr = right[i] + fbR * feedback;

        for (int s = 0; s < stages; ++s)
        {
            apL[s].setCoefficient (cl);
            apR[s].setCoefficient (cr);
            xl = apL[s].process (xl);
            xr = apR[s].process (xr);
        }

        fbL = sanitise (xl);
        fbR = sanitise (xr);

        left[i]  = sanitise (left[i]  * (1.0 - mix) + xl * mix);
        right[i] = sanitise (right[i] * (1.0 - mix) + xr * mix);
    }
}

//==============================================================================
//  Flanger
//==============================================================================
void FlangerPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    lineL.prepare (sr, 30.0);
    lineR.prepare (sr, 30.0);
    lfo.prepare (sr);
    lfo.setShape (Lfo::Shape::Triangle);
    resetParametersToDefault();
    reset();
}

void FlangerPedal::reset() noexcept
{
    lineL.reset();
    lineR.reset();
    lfo.reset();
    fbL = fbR = 0.0;
}

const PedalParam& FlangerPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Rate",     "Hz", 0.02, 6.0, 0.25, 0.5, false, 0, nullptr },
        { "Depth",    "ms", 0.10, 8.0, 3.00, 0.8, false, 0, nullptr },
        { "Manual",   "ms", 0.30,10.0, 4.00, 1.0, false, 0, nullptr },
        { "Feedback", "",  -0.95, 0.95, 0.60, 1.0, false, 0, nullptr },
        { "Mix",      "",   0.00, 1.0, 0.50, 1.0, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 4, index)];
}

void FlangerPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: rateHz = value; lfo.setRate (rateHz); break;
        case 1: depthMs = value; break;
        case 2: centreMs = value; break;
        case 3: feedback = juce::jlimit (-0.95, 0.95, value); break;
        case 4: mix = value; break;
        default: break;
    }
}

void FlangerPedal::process (double* left, double* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const double m = lfo.next();

        const double dl = juce::jmax (0.15, centreMs + m * depthMs);
        const double dr = juce::jmax (0.15, centreMs - m * depthMs * 0.85);

        const double wetL = lineL.process (left[i]  + fbL * feedback, dl);
        const double wetR = lineR.process (right[i] + fbR * feedback, dr);

        fbL = sanitise (wetL);
        fbR = sanitise (wetR);

        left[i]  = sanitise (left[i]  * (1.0 - mix) + wetL * mix);
        right[i] = sanitise (right[i] * (1.0 - mix) + wetR * mix);
    }
}

//==============================================================================
//  Tremolo
//==============================================================================
void TremoloPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    lfoL.prepare (sr);
    lfoR.prepare (sr);
    resetParametersToDefault();
    reset();
}

void TremoloPedal::reset() noexcept
{
    lfoL.reset();
    lfoR.reset();
    lfoR.setPhase (stereoPhase / 360.0);
}

const PedalParam& TremoloPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Rate",   "Hz",  0.1,  20.0,   5.0, 0.5, false, 0, nullptr },
        { "Depth",  "",    0.0,   1.0,   0.6, 1.0, false, 0, nullptr },
        { "Shape",  "",    0.0,   3.0,   0.0, 1.0, true,  4, kLfoShapes },
        { "Stereo", "deg", 0.0, 180.0,   0.0, 1.0, false, 0, nullptr },
        { "Sync",   "",    0.0,   8.0,   0.0, 1.0, true,  9, kSyncDiv }
    };

    return params[juce::jlimit (0, 4, index)];
}

void TremoloPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: rateHz = value; break;
        case 1: depth = value; break;
        case 2:
        {
            shape = (int) value;
            const Lfo::Shape shapes[4] = { Lfo::Shape::Sine, Lfo::Shape::Triangle,
                                           Lfo::Shape::Square, Lfo::Shape::RandomSmooth };
            lfoL.setShape (shapes[juce::jlimit (0, 3, shape)]);
            lfoR.setShape (shapes[juce::jlimit (0, 3, shape)]);
            break;
        }
        case 3: stereoPhase = value; lfoR.setPhase (lfoL.getPhase() + stereoPhase / 360.0); break;
        case 4: sync = (int) value; break;
        default: break;
    }

    double effectiveRate = rateHz;

    if (sync > 0)
    {
        const double beats = kSyncMultipliers[juce::jlimit (0, 8, sync)];
        effectiveRate = (tempoBpm / 60.0) / juce::jmax (0.01, beats);
    }

    lfoL.setRate (effectiveRate);
    lfoR.setRate (effectiveRate);
}

void TremoloPedal::process (double* left, double* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const double ml = lfoL.next() * 0.5 + 0.5;
        const double mr = lfoR.next() * 0.5 + 0.5;

        const double gl = 1.0 - depth * (1.0 - ml);
        const double gr = 1.0 - depth * (1.0 - mr);

        left[i]  = sanitise (left[i]  * gl);
        right[i] = sanitise (right[i] * gr);
    }
}

//==============================================================================
//  Gater (rhythmic gate)
//==============================================================================
void GaterPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    resetParametersToDefault();
    reset();
}

void GaterPedal::reset() noexcept
{
    phase = 0.0;
    lastTempo = tempoBpm;
    updateIncrement();
    gateOpenness.store ((float) gainAtPhase (0.0), std::memory_order_relaxed);
}

const PedalParam& GaterPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[4] =
    {
        { "Rate",      "",   0.0,  8.0,  5.0, 1.0, true,  9, kSyncDiv },   // default 1/8
        { "Size",      "%",  1.0, 99.0, 50.0, 1.0, false, 0, nullptr },
        { "Shape",     "",   0.0,  1.0,  0.2, 1.0, false, 0, nullptr },
        { "Frequency", "Hz", 0.5, 20.0,  4.0, 0.5, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 3, index)];
}

void GaterPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: sync = juce::jlimit (0, 8, (int) value); break;
        case 1: size = juce::jlimit (0.01, 0.99, value * 0.01); break;
        case 2: shape = juce::jlimit (0.0, 1.0, value); break;
        case 3: freqHz = juce::jlimit (0.5, 20.0, value); break;
        default: break;
    }

    updateIncrement();
}

double GaterPedal::getEffectiveRateHz() const noexcept
{
    if (sync > 0)
        return (tempoBpm / 60.0) / juce::jmax (0.01, kSyncMultipliers[juce::jlimit (0, 8, sync)]);

    return freqHz;
}

void GaterPedal::updateIncrement() noexcept
{
    inc = juce::jlimit (0.0, 0.49, getEffectiveRateHz() / juce::jmax (1.0, sr));
}

double GaterPedal::gainAtPhase (double p) const noexcept
{
    // Distance from the centre of the open window, in cycles (0 .. 0.5).
    double d = p - size * 0.5;
    d -= std::floor (d + 0.5);
    d = std::abs (d);

    const double half = size * 0.5;                                   // the window's half-width
    const double fade = shape * 0.5 * juce::jmin (size, 1.0 - size);  // the edge's half-width

    if (d <= half - fade)
        return 1.0;

    if (d >= half + fade)
        return 0.0;

    // Raised cosine across the edge: 1 at the inner end, 0 at the outer.
    const double t = (d - (half - fade)) / juce::jmax (1.0e-9, 2.0 * fade);
    return 0.5 + 0.5 * std::cos (constants::kPi * t);
}

void GaterPedal::process (double* left, double* right, int numSamples) noexcept
{
    // A tempo change reaches the pedal through setTempoBpm, not a parameter;
    // one comparison a block keeps a synced Rate following it.
    if (sync > 0 && tempoBpm != lastTempo)
    {
        lastTempo = tempoBpm;
        updateIncrement();
    }

    double g = gainAtPhase (phase);

    for (int i = 0; i < numSamples; ++i)
    {
        g = gainAtPhase (phase);

        left[i]  = sanitise (left[i]  * g);
        right[i] = sanitise (right[i] * g);

        phase += inc;

        if (phase >= 1.0)
            phase -= 1.0;
    }

    gateOpenness.store ((float) g, std::memory_order_relaxed);
}

//==============================================================================
//  Rotary speaker
//==============================================================================
void RotaryPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);

    hornL.prepare (sr, 12.0);
    hornR.prepare (sr, 12.0);
    drumL.prepare (sr, 20.0);
    drumR.prepare (sr, 20.0);

    // 800 Hz crossover, as in the real cabinet.
    crossoverLowL.setLowpass (sr, 800.0, 0.707);
    crossoverLowR.setLowpass (sr, 800.0, 0.707);
    crossoverHighL.setHighpass (sr, 800.0, 0.707);
    crossoverHighR.setHighpass (sr, 800.0, 0.707);

    resetParametersToDefault();
    reset();
}

void RotaryPedal::reset() noexcept
{
    hornL.reset(); hornR.reset();
    drumL.reset(); drumR.reset();
    crossoverLowL.reset(); crossoverLowR.reset();
    crossoverHighL.reset(); crossoverHighR.reset();
    hornPhase = 0.0;
    drumPhase = 0.35;
}

const PedalParam& RotaryPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Speed",    "",    0.0,  1.0, 1.0, 1.0, true,  2, kRotarySpeed },
        { "Horn",     "Hz",  0.5, 10.0, 6.0, 1.0, false, 0, nullptr },
        { "Drum",     "Hz",  0.2,  5.0, 0.8, 1.0, false, 0, nullptr },
        { "Distance", "",    0.0,  1.0, 0.5, 1.0, false, 0, nullptr },
        { "Mix",      "",    0.0,  1.0, 0.7, 1.0, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 4, index)];
}

void RotaryPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: speedMode = (int) value; break;
        case 1: hornRateHz = value; break;
        case 2: drumRateHz = value; break;
        case 3: distance = value; break;
        case 4: mix = value; break;
        default: break;
    }

    // Slow is roughly a seventh of fast on a real cabinet.
    hornTarget = (speedMode == 1) ? hornRateHz : hornRateHz * 0.13;
    drumTarget = (speedMode == 1) ? drumRateHz : drumRateHz * 0.15;
}

void RotaryPedal::process (double* left, double* right, int numSamples) noexcept
{
    // The rotors have real mass: they take about a second to spin up and longer to
    // coast down. That ramp is most of the character of a speed change.
    const double rampUp = 1.0 - std::exp (-1.0 / (0.9 * sr));
    const double rampDown = 1.0 - std::exp (-1.0 / (1.8 * sr));

    for (int i = 0; i < numSamples; ++i)
    {
        hornCurrent += (hornTarget - hornCurrent) * ((hornTarget > hornCurrent) ? rampUp : rampDown);
        drumCurrent += (drumTarget - drumCurrent) * ((drumTarget > drumCurrent) ? rampUp : rampDown);

        hornPhase += hornCurrent / sr;
        drumPhase += drumCurrent / sr;

        if (hornPhase >= 1.0) hornPhase -= 1.0;
        if (drumPhase >= 1.0) drumPhase -= 1.0;

        const double hornAngle = constants::kTwoPi * hornPhase;
        const double drumAngle = constants::kTwoPi * drumPhase;

        const double mono = (left[i] + right[i]) * 0.5;

        const double lowIn = crossoverLowL.process (mono);
        const double highIn = crossoverHighL.process (mono);

        // Doppler: the path length to each mic changes as the rotor turns, so the
        // delay is modulated sinusoidally in quadrature between the two mics.
        const double hornDepth = 0.9 + distance * 1.6;
        const double drumDepth = 1.4 + distance * 2.4;

        const double hl = hornL.process (highIn, 3.0 + std::sin (hornAngle) * hornDepth);
        const double hr = hornR.process (highIn, 3.0 + std::sin (hornAngle + constants::kPi) * hornDepth);

        const double dl = drumL.process (lowIn, 5.0 + std::sin (drumAngle) * drumDepth);
        const double dr = drumR.process (lowIn, 5.0 + std::sin (drumAngle + constants::kPi) * drumDepth);

        // Amplitude modulation: the horn is directional, so it is loudest when it
        // points at the mic.
        const double hAmpL = 0.72 + 0.28 * std::cos (hornAngle);
        const double hAmpR = 0.72 + 0.28 * std::cos (hornAngle + constants::kPi);
        const double dAmpL = 0.85 + 0.15 * std::cos (drumAngle);
        const double dAmpR = 0.85 + 0.15 * std::cos (drumAngle + constants::kPi);

        const double wetL = hl * hAmpL + dl * dAmpL;
        const double wetR = hr * hAmpR + dr * dAmpR;

        left[i]  = sanitise (left[i]  * (1.0 - mix) + wetL * mix);
        right[i] = sanitise (right[i] * (1.0 - mix) + wetR * mix);
    }
}

//==============================================================================
//  Delay
//==============================================================================
void DelayPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);

    // Four seconds is enough for any musical delay at any supported tempo.
    size = juce::nextPowerOfTwo ((int) (sr * 4.0) + 8);
    mask = size - 1;
    bufL.assign ((size_t) size, 0.0);
    bufR.assign ((size_t) size, 0.0);
    writeIndex = 0;

    timeSmooth.prepare (sr, 0.080);
    timeSmooth.snapTo (timeMs * 0.001 * sr);

    dampL.prepare (sr); dampR.prepare (sr);
    hpL.prepare (sr);   hpR.prepare (sr);
    hpL.setCutoff (90.0);
    hpR.setCutoff (90.0);

    wow.prepare (sr);
    flutter.prepare (sr);
    wow.setShape (Lfo::Shape::Sine);
    flutter.setShape (Lfo::Shape::RandomSmooth);
    wow.setRate (0.7);
    flutter.setRate (7.0);

    dcL.prepare (sr, 12.0);
    dcR.prepare (sr, 12.0);

    resetParametersToDefault();
    reset();
}

void DelayPedal::reset() noexcept
{
    std::fill (bufL.begin(), bufL.end(), 0.0);
    std::fill (bufR.begin(), bufR.end(), 0.0);
    writeIndex = 0;
    dampL.reset(); dampR.reset();
    hpL.reset();   hpR.reset();
    wow.reset();   flutter.reset();
    dcL.reset();   dcR.reset();
    timeSmooth.snapTo (timeMs * 0.001 * sr);
}

const PedalParam& DelayPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[7] =
    {
        { "Time",      "ms",   20.0, 2000.0, 375.0, 0.5, false, 0, nullptr },
        { "Feedback",  "",      0.0,   0.95,  0.35, 1.0, false, 0, nullptr },
        { "Mix",       "",      0.0,   1.00,  0.30, 1.0, false, 0, nullptr },
        { "Tone",      "Hz",  800.0,16000.0,6000.0, 0.5, false, 0, nullptr },
        { "Character", "",      0.0,   2.00,  0.00, 1.0, true,  3, kDelayChar },
        { "Sync",      "",      0.0,   8.00,  0.00, 1.0, true,  9, kSyncDiv },
        { "Ping Pong", "",      0.0,   1.00,  0.00, 1.0, true,  2, kOnOff }
    };

    return params[juce::jlimit (0, 6, index)];
}

void DelayPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: timeMs = value; break;
        case 1: feedback = value; break;
        case 2: mix = value; break;
        case 3: toneHz = value; break;
        case 4: character = (int) value; break;
        case 5: syncMode = (int) value; break;
        case 6: pingPong = (int) value; break;
        default: break;
    }

    // An analogue bucket-brigade delay darkens every repeat; tape darkens it less
    // but adds wow and flutter. A digital delay repeats faithfully.
    const double charTone = (character == 1) ? 0.45 : (character == 2) ? 0.70 : 1.0;

    dampL.setCutoff (juce::jmin (toneHz * charTone, sr * 0.47));
    dampR.setCutoff (juce::jmin (toneHz * charTone, sr * 0.47));

    timeSmooth.setTarget (currentDelaySamples());
}

double DelayPedal::currentDelaySamples() const noexcept
{
    double ms = timeMs;

    if (syncMode > 0)
    {
        const double beats = kSyncMultipliers[juce::jlimit (0, 8, syncMode)];
        ms = (60000.0 / juce::jmax (20.0, tempoBpm)) * beats;
    }

    return juce::jlimit (4.0, (double) (size - 8), ms * 0.001 * sr);
}

double DelayPedal::readTap (const std::vector<double>& buf, double delaySamples) const noexcept
{
    const double d = juce::jlimit (2.0, (double) (size - 3), delaySamples);

    const int i0 = (int) d;
    const double frac = d - (double) i0;

    const double a = buf[(size_t) ((writeIndex - i0) & mask)];
    const double b = buf[(size_t) ((writeIndex - i0 - 1) & mask)];

    return a * (1.0 - frac) + b * frac;
}

void DelayPedal::process (double* left, double* right, int numSamples) noexcept
{
    timeSmooth.setTarget (currentDelaySamples());

    const bool isTape = (character == 2);

    for (int i = 0; i < numSamples; ++i)
    {
        double delaySamples = timeSmooth.next();

        if (isTape)
        {
            // Wow is the slow speed drift of the capstan; flutter is the fast,
            // irregular component. Both are fractions of a percent on a good
            // machine and that is exactly how much colour they add.
            const double w = wow.next() * 0.0035;
            const double f = flutter.next() * 0.0012;
            delaySamples *= (1.0 + w + f);
        }

        delaySamples = juce::jlimit (2.0, (double) (size - 8), delaySamples);

        double wetL = readTap (bufL, delaySamples);
        double wetR = readTap (bufR, delaySamples);

        wetL = hpL.process (dampL.process (wetL));
        wetR = hpR.process (dampR.process (wetR));

        if (isTape)
        {
            // Tape saturates on the loud repeats, which is what stops a runaway
            // feedback setting from simply exploding.
            wetL = softClip (wetL * 1.2) * 0.85;
            wetR = softClip (wetR * 1.2) * 0.85;
        }

        wetL = sanitise (wetL);
        wetR = sanitise (wetR);

        // Feedback is hard-capped below unity so no combination of automation can
        // produce a runaway.
        const double fb = juce::jlimit (0.0, 0.95, feedback);

        if (pingPong == 1)
        {
            // Repeats alternate channels: each side feeds the other.
            bufL[(size_t) writeIndex] = flushDenormal (left[i] + wetR * fb);
            bufR[(size_t) writeIndex] = flushDenormal (right[i] + wetL * fb);
        }
        else
        {
            bufL[(size_t) writeIndex] = flushDenormal (left[i] + wetL * fb);
            bufR[(size_t) writeIndex] = flushDenormal (right[i] + wetR * fb);
        }

        writeIndex = (writeIndex + 1) & mask;

        // qa-polish.md 5.9: the DC blocker is on the wet path only, so the dry
        // path is untouched and Mix 0 is a true bypass.
        left[i]  = sanitise (left[i]  * (1.0 - mix * 0.35) + dcL.process (wetL * mix));
        right[i] = sanitise (right[i] * (1.0 - mix * 0.35) + dcR.process (wetR * mix));
    }
}

//==============================================================================
//  Reverb (FDN)
//==============================================================================
void ReverbPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);

    preSize = juce::nextPowerOfTwo ((int) (sr * 0.25) + 8);
    preMask = preSize - 1;
    preDelayBuf.assign ((size_t) preSize, 0.0);
    preIndex = 0;

    for (int i = 0; i < kFdnSize; ++i)
        lineDamp[i].prepare (sr);

    // Input diffusion: four all-passes per channel, at mutually prime-ish
    // coefficients so the echo density builds without a metallic ring.
    const double diff[4] = { 0.72, 0.68, 0.61, 0.55 };

    for (int i = 0; i < 4; ++i)
    {
        diffusionL[i].setCoefficient (diff[i]);
        diffusionR[i].setCoefficient (-diff[i]);
    }

    dcL.prepare (sr, 12.0);
    dcR.prepare (sr, 12.0);

    // Sized for the longest line rebuildLines can ask for, so a Size or
    // Character change on the audio thread never allocates.
    for (int i = 0; i < kFdnSize; ++i)
        lines[i].assign ((size_t) (sr * kMaxLineSeconds) + 1, 0.0);

    resetParametersToDefault();
    rebuildLines();
    reset();
}

void ReverbPedal::reset() noexcept
{
    for (int i = 0; i < kFdnSize; ++i)
    {
        std::fill (lines[i].begin(), lines[i].end(), 0.0);
        lineIndex[i] = 0;
        lineDamp[i].reset();
    }

    std::fill (preDelayBuf.begin(), preDelayBuf.end(), 0.0);
    preIndex = 0;

    for (int i = 0; i < 4; ++i)
    {
        diffusionL[i].reset();
        diffusionR[i].reset();
    }

    dcL.reset();
    dcR.reset();
}

const PedalParam& ReverbPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[6] =
    {
        { "Size",      "",    0.0,   1.0,  0.50, 1.0, false, 0, nullptr },
        { "Decay",     "s",   0.2,  15.0,  2.00, 0.6, false, 0, nullptr },
        { "Damping",   "",    0.0,   1.0,  0.50, 1.0, false, 0, nullptr },
        { "Pre-Delay", "ms",  0.0, 200.0, 20.00, 0.7, false, 0, nullptr },
        { "Character", "",    0.0,   3.0,  0.00, 1.0, true,  4, kReverbChar },
        { "Mix",       "",    0.0,   1.0,  0.25, 1.0, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 5, index)];
}

void ReverbPedal::parameterChanged (int index, double value)
{
    bool needsRebuild = false;

    switch (index)
    {
        // The bridge re-sends every parameter every block; only a real change
        // may rebuild, or the tail is wiped each block and the pedal is dry.
        case 0: needsRebuild = value != size; size = value; break;
        case 1: decaySeconds = value; break;
        case 2: damping = value; break;
        case 3: preDelayMs = value; break;
        case 4: needsRebuild = (int) value != character; character = (int) value; break;
        case 5: mix = value; break;
        default: break;
    }

    if (needsRebuild)
        rebuildLines();

    const double dampHz = juce::jmap (1.0 - damping, 0.0, 1.0, 900.0, 16000.0);

    for (int i = 0; i < kFdnSize; ++i)
        lineDamp[i].setCutoff (juce::jmin (dampHz, sr * 0.47));
}

void ReverbPedal::rebuildLines()
{
    // Mutually prime delay lengths avoid the coincident echoes that make a cheap
    // reverb sound like a metal pipe.
    static const int primes[kFdnSize] = { 1123, 1291, 1483, 1663, 1871, 2053, 2273, 2467 };

    // Each character is a different room scale and diffusion.
    const double charScale = (character == 0) ? 0.55    // plate: small and dense
                           : (character == 1) ? 1.60    // hall: large
                           : (character == 2) ? 0.75    // room
                                              : 1.05;   // chamber

    const double scale = (0.45 + size * 1.10) * charScale * (sr / 44100.0);

    for (int i = 0; i < kFdnSize; ++i)
    {
        lineLengths[i] = juce::jlimit (64, (int) (sr * kMaxLineSeconds), (int) (primes[i] * scale));

        if (lines[i].size() < (size_t) lineLengths[i])   // only before prepare
            lines[i].assign ((size_t) lineLengths[i], 0.0);
        else
            std::fill (lines[i].begin(), lines[i].begin() + lineLengths[i], 0.0);

        lineIndex[i] = 0;
    }
}

void ReverbPedal::process (double* left, double* right, int numSamples) noexcept
{
    const int preDelaySamples = juce::jlimit (0, preSize - 2, (int) (preDelayMs * 0.001 * sr));

    // Average line length sets how much loss each pass must apply to hit the
    // requested decay time.
    double avgLength = 0.0;

    for (int i = 0; i < kFdnSize; ++i)
        avgLength += (double) lineLengths[i];

    avgLength /= (double) kFdnSize;

    feedbackGain = std::exp (-6.907755 * avgLength / (juce::jmax (0.05, decaySeconds) * sr));
    feedbackGain = juce::jlimit (0.0, 0.998, feedbackGain);   // SPEC-SWEEP: EN-90, engine.md 20.18

    for (int n = 0; n < numSamples; ++n)
    {
        const double inMono = (left[n] + right[n]) * 0.5;

        // ---- pre-delay ------------------------------------------------------
        preDelayBuf[(size_t) preIndex] = flushDenormal (inMono);
        const int readPre = (preIndex - preDelaySamples) & preMask;
        double x = preDelayBuf[(size_t) readPre];
        preIndex = (preIndex + 1) & preMask;

        // ---- input diffusion --------------------------------------------------
        double dL = x, dR = x;

        for (int i = 0; i < 4; ++i)
        {
            dL = diffusionL[i].process (dL);
            dR = diffusionR[i].process (dR);
        }

        // ---- read the delay network -------------------------------------------
        double taps[kFdnSize];

        for (int i = 0; i < kFdnSize; ++i)
            taps[i] = lines[i][(size_t) lineIndex[i]];

        // ---- Householder feedback matrix ---------------------------------------
        // An 8x8 Householder reflection is lossless and mixes every line into
        // every other, which is what produces a smooth, dense tail.
        double sum = 0.0;

        for (int i = 0; i < kFdnSize; ++i)
            sum += taps[i];

        const double coupling = sum * (2.0 / (double) kFdnSize);

        for (int i = 0; i < kFdnSize; ++i)
        {
            double v = (taps[i] - coupling) * feedbackGain;
            v = lineDamp[i].process (v);

            // Alternate which diffused input feeds which line so the two channels
            // develop genuinely different tails.
            v += ((i & 1) ? dR : dL) * 0.30;

            lines[i][(size_t) lineIndex[i]] = flushDenormal (sanitise (v));
            lineIndex[i] = (lineIndex[i] + 1) % lineLengths[i];
        }

        // ---- output: two different sums, so the tail is wide but mono-safe ----
        double wetL = 0.0, wetR = 0.0;

        for (int i = 0; i < kFdnSize; ++i)
        {
            if (i % 2 == 0) wetL += taps[i];
            else            wetR += taps[i];
        }

        const double norm = 2.0 / (double) kFdnSize;
        wetL = dcL.process (wetL * norm);
        wetR = dcR.process (wetR * norm);

        left[n]  = sanitise (left[n]  * (1.0 - mix * 0.4) + wetL * mix);
        right[n] = sanitise (right[n] * (1.0 - mix * 0.4) + wetR * mix);
    }
}

//==============================================================================
//  Spring reverb
//==============================================================================
void SpringReverbPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);

    // Three springs of slightly different length, as in a real Accutronics tank.
    const double lengthsMs[kSprings] = { 28.0, 34.5, 41.0 };

    for (int s = 0; s < kSprings; ++s)
    {
        springLen[s] = juce::jmax (32, (int) (lengthsMs[s] * 0.001 * sr));
        springBuf[s].assign ((size_t) springLen[s], 0.0);
        springIndex[s] = 0;
        springTone[s].prepare (sr);
    }

    bodyResonance.setPeaking (sr, 2800.0, 1.6, 4.0);
    dcOut.prepare (sr, 20.0);

    resetParametersToDefault();
    reset();
}

void SpringReverbPedal::reset() noexcept
{
    for (int s = 0; s < kSprings; ++s)
    {
        std::fill (springBuf[s].begin(), springBuf[s].end(), 0.0);
        springIndex[s] = 0;
        springTone[s].reset();

        for (int d = 0; d < kDispersion; ++d)
            dispersion[s][d].reset();
    }

    bodyResonance.reset();
    dcOut.reset();
}

const PedalParam& SpringReverbPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Decay",   "",     0.0,    1.0,   0.50, 1.0, false, 0, nullptr },
        { "Tension", "",     0.0,    1.0,   0.50, 1.0, false, 0, nullptr },
        { "Drip",    "",     0.0,    1.0,   0.50, 1.0, false, 0, nullptr },
        { "Tone",    "Hz", 800.0, 8000.0, 4500.0, 0.5, false, 0, nullptr },
        { "Mix",     "",     0.0,    1.0,   0.30, 1.0, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 4, index)];
}

void SpringReverbPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: decay = value; break;
        case 1: tension = value; break;
        case 2: drip = value; break;
        case 3: toneHz = value; break;
        case 4: mix = value; break;
        default: break;
    }

    for (int s = 0; s < kSprings; ++s)
        springTone[s].setCutoff (juce::jmin (toneHz * (0.85 + 0.12 * s), sr * 0.47));

    // A tighter spring disperses less, so the chirp is shorter and higher.
    const double coeff = -juce::jmap (drip, 0.0, 1.0, 0.25, 0.72)
                         * juce::jmap (tension, 0.0, 1.0, 1.15, 0.80);

    for (int s = 0; s < kSprings; ++s)
        for (int d = 0; d < kDispersion; ++d)
            dispersion[s][d].setCoefficient (juce::jlimit (-0.9, 0.0, coeff * (1.0 + 0.04 * s)));
}

void SpringReverbPedal::process (double* left, double* right, int numSamples) noexcept
{
    const double springGain = juce::jmap (decay, 0.0, 1.0, 0.55, 0.92);

    for (int n = 0; n < numSamples; ++n)
    {
        const double in = (left[n] + right[n]) * 0.5;

        double wet = 0.0;

        for (int s = 0; s < kSprings; ++s)
        {
            double v = springBuf[s][(size_t) springIndex[s]];

            // The all-pass cascade is what makes a spring chirp: low frequencies
            // take longer to travel the spring than high ones, so a transient
            // arrives smeared into a descending whistle.
            for (int d = 0; d < kDispersion; ++d)
                v = dispersion[s][d].process (v);

            v = springTone[s].process (v) * springGain;
            v = sanitise (v);

            springBuf[s][(size_t) springIndex[s]] = flushDenormal (v + in * 0.32);
            springIndex[s] = (springIndex[s] + 1) % springLen[s];

            wet += v;
        }

        wet = bodyResonance.process (wet * (1.0 / (double) kSprings));
        wet = dcOut.process (wet);
        wet = sanitise (wet);

        // A real tank is mono; the width comes from the amp's cabinet, so both
        // channels get the same tail. That also makes it perfectly mono-safe.
        left[n]  = sanitise (left[n]  * (1.0 - mix * 0.35) + wet * mix * 1.3);
        right[n] = sanitise (right[n] * (1.0 - mix * 0.35) + wet * mix * 1.3);
    }
}

//==============================================================================
//  Graphic EQ
//==============================================================================
const double GraphicEqPedal::kBandFrequencies[GraphicEqPedal::kBands] =
    { 100.0, 250.0, 630.0, 1600.0, 4000.0, 8000.0 };

void GraphicEqPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    resetParametersToDefault();
    reset();
}

void GraphicEqPedal::reset() noexcept
{
    for (int b = 0; b < kBands; ++b)
    {
        bandL[b].reset();
        bandR[b].reset();
    }
}

const PedalParam& GraphicEqPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[7] =
    {
        { "100 Hz",  "dB", -15.0, 15.0, 0.0, 1.0, false, 0, nullptr },
        { "250 Hz",  "dB", -15.0, 15.0, 0.0, 1.0, false, 0, nullptr },
        { "630 Hz",  "dB", -15.0, 15.0, 0.0, 1.0, false, 0, nullptr },
        { "1.6 kHz", "dB", -15.0, 15.0, 0.0, 1.0, false, 0, nullptr },
        { "4 kHz",   "dB", -15.0, 15.0, 0.0, 1.0, false, 0, nullptr },
        { "8 kHz",   "dB", -15.0, 15.0, 0.0, 1.0, false, 0, nullptr },
        { "Level",   "dB", -18.0, 18.0, 0.0, 1.0, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 6, index)];
}

void GraphicEqPedal::parameterChanged (int index, double value)
{
    if (index >= 0 && index < kBands)
    {
        gains[index] = value;

        // Q of 1.4 gives roughly one-octave bands, which is what a six-band
        // graphic on a pedalboard actually has.
        bandL[index].setPeaking (sr, kBandFrequencies[index], 1.4, value);
        bandR[index].setPeaking (sr, kBandFrequencies[index], 1.4, value);
    }
    else if (index == kBands)
    {
        outputLevel = dbToGain (value);
    }
}

void GraphicEqPedal::process (double* left, double* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        double l = left[i], r = right[i];

        for (int b = 0; b < kBands; ++b)
        {
            if (std::abs (gains[b]) > 0.01)
            {
                l = bandL[b].process (l);
                r = bandR[b].process (r);
            }
        }

        left[i]  = sanitise (l * outputLevel);
        right[i] = sanitise (r * outputLevel);
    }
}

//==============================================================================
//  Parametric EQ
//==============================================================================
void ParametricEqPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    resetParametersToDefault();
    reset();
}

void ParametricEqPedal::reset() noexcept
{
    lowL.reset();  lowR.reset();
    midBL.reset(); midBR.reset();
    highL.reset(); highR.reset();
    hpL.reset();   hpR.reset();
    lpL.reset();   lpR.reset();
}

const PedalParam& ParametricEqPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[9] =
    {
        { "Low Freq",  "Hz",   40.0,   400.0,   120.0, 0.5, false, 0, nullptr },
        { "Low Gain",  "dB",  -18.0,    18.0,     0.0, 1.0, false, 0, nullptr },
        { "Mid Freq",  "Hz",  200.0,  5000.0,   800.0, 0.5, false, 0, nullptr },
        { "Mid Gain",  "dB",  -18.0,    18.0,     0.0, 1.0, false, 0, nullptr },
        { "Mid Q",     "",      0.2,    10.0,     1.0, 0.6, false, 0, nullptr },
        { "High Freq", "Hz", 1500.0, 16000.0,  4000.0, 0.5, false, 0, nullptr },
        { "High Gain", "dB",  -18.0,    18.0,     0.0, 1.0, false, 0, nullptr },
        { "Highpass",  "Hz",   20.0,  1000.0,    20.0, 0.4, false, 0, nullptr },
        { "Lowpass",   "Hz", 1000.0, 20000.0, 20000.0, 0.4, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 8, index)];
}

void ParametricEqPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: lowFreq = value; break;
        case 1: lowGain = value; break;
        case 2: midFreq = value; break;
        case 3: midGain = value; break;
        case 4: midQ = value; break;
        case 5: highFreq = value; break;
        case 6: highGain = value; break;
        case 7: hpFreq = value; break;
        case 8: lpFreq = value; break;
        default: break;
    }

    lowL.setLowShelf (sr, lowFreq, 0.7, lowGain);
    lowR.setLowShelf (sr, lowFreq, 0.7, lowGain);

    midBL.setPeaking (sr, midFreq, midQ, midGain);
    midBR.setPeaking (sr, midFreq, midQ, midGain);

    highL.setHighShelf (sr, highFreq, 0.7, highGain);
    highR.setHighShelf (sr, highFreq, 0.7, highGain);

    hpL.setHighpass (sr, hpFreq, 0.707);
    hpR.setHighpass (sr, hpFreq, 0.707);

    lpL.setLowpass (sr, juce::jmin (lpFreq, sr * 0.47), 0.707);
    lpR.setLowpass (sr, juce::jmin (lpFreq, sr * 0.47), 0.707);
}

void ParametricEqPedal::process (double* left, double* right, int numSamples) noexcept
{
    const bool doHp = hpFreq > 21.0;
    const bool doLp = lpFreq < 19900.0;

    for (int i = 0; i < numSamples; ++i)
    {
        double l = left[i], r = right[i];

        if (doHp) { l = hpL.process (l); r = hpR.process (r); }

        if (std::abs (lowGain)  > 0.01) { l = lowL.process (l);  r = lowR.process (r); }
        if (std::abs (midGain)  > 0.01) { l = midBL.process (l); r = midBR.process (r); }
        if (std::abs (highGain) > 0.01) { l = highL.process (l); r = highR.process (r); }

        if (doLp) { l = lpL.process (l); r = lpR.process (r); }

        left[i]  = sanitise (l);
        right[i] = sanitise (r);
    }
}

//==============================================================================
//  Doubler (ambiguity-resolutions.md 3)
//==============================================================================
namespace
{
    const char* const kDoublerWidth[] = { "Mono", "Stereo" };
}

void DoublerPedal::Voice::prepare (int maxDelaySamples, int windowSamples)
{
    size = juce::nextPowerOfTwo (juce::jmax (256, maxDelaySamples + windowSamples + 8));
    mask = size - 1;
    buffer.assign ((size_t) size, 0.0);
    window = juce::jmax (16, windowSamples);
    reset();
}

void DoublerPedal::Voice::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0);
    writeIndex = 0;

    // Centred: at 0 cents the first head sits exactly on the delay and carries
    // all the weight, so the take is a pure delay (3.2).
    phase = (double) window * 0.5;
}

double DoublerPedal::Voice::process (double input, double delaySamples, double ratio) noexcept
{
    if (buffer.empty())
        return 0.0;

    buffer[(size_t) writeIndex] = flushDenormal (input);

    // The read point drifts by the pitch offset; two heads half a window apart,
    // crossfaded, hand over as one drifts out, as a tape ADT's varispeed did.
    phase += (1.0 - ratio);

    if (phase >= (double) window) phase -= (double) window;
    if (phase < 0.0)               phase += (double) window;

    auto readAt = [this] (double delay) noexcept
    {
        const double d = juce::jlimit (0.0, (double) (size - 2), delay);
        const int i0 = (int) d;
        const double frac = d - (double) i0;
        const double a = buffer[(size_t) ((writeIndex - i0) & mask)];
        const double b = buffer[(size_t) ((writeIndex - i0 - 1) & mask)];
        return a * (1.0 - frac) + b * frac;
    };

    const double half = (double) window * 0.5;
    const double otherPhase = phase + half >= (double) window ? phase + half - (double) window : phase + half;

    const double w1 = 0.5 - 0.5 * std::cos (constants::kTwoPi * phase / (double) window);
    const double w2 = 1.0 - w1;

    const double out = readAt (delaySamples - half + phase) * w1
                     + readAt (delaySamples - half + otherPhase) * w2;

    writeIndex = (writeIndex + 1) & mask;
    return out;
}

void DoublerPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);

    // A 10 ms window: half of it is the shortest delay (5 ms), so a head is
    // never asked to read ahead of the write point.
    const int window = juce::jmax (16, (int) (sr * 0.010));
    const int maxDelay = (int) std::ceil (sr * 0.040 * 1.2) + 2;

    first.prepare (maxDelay, window);
    second.prepare (maxDelay, window);

    resetParametersToDefault();
    reset();
}

void DoublerPedal::reset() noexcept
{
    first.reset();
    second.reset();
    hp1.reset(); lp1.reset();
    hp2.reset(); lp2.reset();
}

const PedalParam& DoublerPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[7] =
    {
        { "Delay",  "ms",     5.0,    40.0,   22.0, 1.0, false, 0, nullptr },
        { "Pitch",  "cent", -25.0,    25.0,   -8.0, 1.0, false, 0, nullptr },
        { "Pan",    "",      -1.0,     1.0,   -0.7, 1.0, false, 0, nullptr },
        { "Width",  "",       0.0,     1.0,    1.0, 1.0, true,  2, kDoublerWidth },
        { "Mix",    "%",      0.0,   100.0,   40.0, 1.0, false, 0, nullptr },
        { "HP",     "Hz",    20.0,   500.0,  100.0, 0.5, false, 0, nullptr },
        { "LP",     "Hz",  2000.0, 20000.0, 8000.0, 0.5, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 6, index)];
}

void DoublerPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: delayMs = value; break;
        case 1: cents = value; break;
        case 2: pan = value; break;
        case 3: stereo = value > 0.5; break;
        case 4: mix = value * 0.01; break;
        case 5: hpHz = value; updateFilters(); break;
        case 6: lpHz = value; updateFilters(); break;
        default: break;
    }
}

void DoublerPedal::updateFilters() noexcept
{
    hp1.setHighpass (sr, hpHz, 0.707);
    hp2.setHighpass (sr, hpHz, 0.707);
    lp1.setLowpass (sr, lpHz, 0.707);
    lp2.setLowpass (sr, lpHz, 0.707);
}

void DoublerPedal::process (double* left, double* right, int numSamples) noexcept
{
    const double delay1 = delayMs * 0.001 * sr;

    // Stereo's second take mirrors the first: the other side, the other way
    // out of tune, and a little later so the two do not comb against each other.
    const double delay2 = delay1 * 1.18;
    const double ratio1 = std::pow (2.0, cents / 1200.0);
    const double ratio2 = std::pow (2.0, -cents / 1200.0);

    // Equal-power pan, -1 hard left .. +1 hard right.
    auto gains = [] (double p)
    {
        const double angle = (juce::jlimit (-1.0, 1.0, p) + 1.0) * juce::MathConstants<double>::pi * 0.25;
        return std::make_pair (std::cos (angle), std::sin (angle));
    };

    const auto [l1, r1] = gains (pan);
    const auto [l2, r2] = gains (-pan);
    const double voices = stereo ? std::sqrt (0.5) : 1.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const double in = 0.5 * (left[i] + right[i]);

        const double v1 = lp1.process (hp1.process (first.process (in, delay1, ratio1))) * voices;
        double wetL = v1 * l1, wetR = v1 * r1;

        if (stereo)
        {
            const double v2 = lp2.process (hp2.process (second.process (in, delay2, ratio2))) * voices;
            wetL += v2 * l2;
            wetR += v2 * r2;
        }

        // 3.2: mix 0 is the dry signal exactly; mix 100 the takes alone.
        left[i] = sanitise (left[i] * (1.0 - mix) + wetL * mix);
        right[i] = sanitise (right[i] * (1.0 - mix) + wetR * mix);
    }
}

} // namespace luthier
