#include "PedalsDrive.h"

namespace luthier
{

namespace
{
    const char* const kCompCharacter[] = { "Optical", "FET" };
    const char* const kWahMode[]       = { "Pedal", "Auto" };
    const char* const kFilterDir[]     = { "Up", "Down" };
    const char* const kTaper[]         = { "Log", "Linear" };
    const char* const kOdVoicing[]     = { "Warm", "Bright", "Fat" };
    const char* const kDistVoicing[]   = { "Classic", "Modern", "Scooped" };
    const char* const kFuzzVoicing[]   = { "Face", "Muff", "Tone Bender" };
    const char* const kBoostVoicing[]  = { "Clean", "Treble", "Full" };
}

//==============================================================================
//  Compressor
//==============================================================================
void CompressorPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    detector.prepare (sr);
    grSmooth.prepare (sr, 0.010);
    grSmooth.snapTo (1.0);
    resetParametersToDefault();
    reset();
}

void CompressorPedal::reset() noexcept
{
    detector.reset();
    grSmooth.snapTo (1.0);
    lastGrDb = 0.0;
}

const PedalParam& CompressorPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[6] =
    {
        { "Threshold", "dB",    -48.0,   0.0, -18.0, 1.0, false, 0, nullptr },
        { "Ratio",     ":1",      1.0,  20.0,   4.0, 0.6, false, 0, nullptr },
        { "Attack",    "ms",      0.1, 100.0,   8.0, 0.4, false, 0, nullptr },
        { "Release",   "ms",     10.0,1000.0, 120.0, 0.4, false, 0, nullptr },
        { "Makeup",    "dB",      0.0,  24.0,   0.0, 1.0, false, 0, nullptr },
        { "Character", "",        0.0,   1.0,   0.0, 1.0, true,  2, kCompCharacter }
    };

    return params[juce::jlimit (0, 5, index)];
}

void CompressorPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: thresholdDb = value; break;
        case 1: ratio = juce::jmax (1.0, value); break;
        case 2: attackMs = value; break;
        case 3: releaseMs = value; break;
        case 4: makeupDb = value; break;
        case 5: character = (int) value; break;
        default: break;
    }

    // An optical cell's attack and release are both slower and less abrupt than a
    // FET's, so the same knob positions give a noticeably gentler action.
    const double scale = (character == 0) ? 2.2 : 0.55;
    detector.setTimes (attackMs * 0.001 * scale, releaseMs * 0.001 * scale);
}

void CompressorPedal::process (double* left, double* right, int numSamples) noexcept
{
    const double makeup = dbToGain (makeupDb);
    const double knee = (character == 0) ? 8.0 : 2.0;

    double maxGr = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        // Detect on the louder channel so the stereo image never shifts.
        const double detect = juce::jmax (std::abs (left[i]), std::abs (right[i]));
        const double envDb = gainToDb (detector.process (detect) + 1.0e-12);

        double overDb = envDb - thresholdDb;
        double reductionDb = 0.0;

        if (overDb > knee * 0.5)
        {
            reductionDb = overDb * (1.0 - 1.0 / ratio);
        }
        else if (overDb > -knee * 0.5)
        {
            // Soft knee: a quadratic blend across the knee width.
            const double t = overDb + knee * 0.5;
            reductionDb = (1.0 - 1.0 / ratio) * (t * t) / (2.0 * knee);
        }

        const double target = dbToGain (-reductionDb);
        grSmooth.setTarget (target);
        const double g = grSmooth.next();

        left[i]  = sanitise (left[i]  * g * makeup);
        right[i] = sanitise (right[i] * g * makeup);

        maxGr = juce::jmax (maxGr, reductionDb);
    }

    lastGrDb = maxGr;
}

//==============================================================================
//  Noise gate
//==============================================================================
void NoiseGatePedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    detector.prepare (sr);
    detector.setTimes (0.0005, 0.010);
    resetParametersToDefault();
    reset();
}

void NoiseGatePedal::reset() noexcept
{
    detector.reset();
    envelope = 0.0;
    holdCounter = 0;
}

const PedalParam& NoiseGatePedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[4] =
    {
        { "Threshold", "dB", -90.0, -20.0, -60.0, 1.0, false, 0, nullptr },
        { "Attack",    "ms",   0.1,  50.0,   1.0, 0.4, false, 0, nullptr },
        { "Hold",      "ms",   0.0, 500.0,  40.0, 0.5, false, 0, nullptr },
        { "Release",   "ms",  10.0,2000.0, 120.0, 0.4, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 3, index)];
}

void NoiseGatePedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: thresholdDb = value; break;
        case 1: attackMs = value;  attackCoeff  = std::exp (-1.0 / juce::jmax (1.0, attackMs  * 0.001 * sr)); break;
        case 2: holdMs = value;    holdSamples  = (int) (holdMs * 0.001 * sr); break;
        case 3: releaseMs = value; releaseCoeff = std::exp (-1.0 / juce::jmax (1.0, releaseMs * 0.001 * sr)); break;
        default: break;
    }
}

void NoiseGatePedal::process (double* left, double* right, int numSamples) noexcept
{
    const double threshold = dbToGain (thresholdDb);

    for (int i = 0; i < numSamples; ++i)
    {
        const double detect = detector.process (juce::jmax (std::abs (left[i]), std::abs (right[i])));

        if (detect > threshold)
        {
            holdCounter = holdSamples;
            envelope = 1.0 + (envelope - 1.0) * attackCoeff;
        }
        else if (holdCounter > 0)
        {
            --holdCounter;
        }
        else
        {
            envelope *= releaseCoeff;
        }

        envelope = juce::jlimit (0.0, 1.0, envelope);

        left[i]  = sanitise (left[i]  * envelope);
        right[i] = sanitise (right[i] * envelope);
    }
}

//==============================================================================
//  Wah
//==============================================================================
void WahPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    lfo.prepare (sr);
    lfo.setShape (Lfo::Shape::Triangle);
    sweep.prepare (sr, 0.008);
    sweep.snapTo (0.5);
    resetParametersToDefault();
    reset();
}

void WahPedal::reset() noexcept
{
    filterL.reset();
    filterR.reset();
    lfo.reset();
    lastFreq = 0.0;
}

const PedalParam& WahPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Low",   "Hz",  200.0, 1200.0,  400.0, 0.5, false, 0, nullptr },
        { "High",  "Hz", 1000.0, 4000.0, 2200.0, 0.5, false, 0, nullptr },
        { "Q",     "",      1.0,   12.0,    4.0, 0.6, false, 0, nullptr },
        { "Level", "",      0.0,    2.0,    1.0, 1.0, false, 0, nullptr },
        { "Mode",  "",      0.0,    1.0,    0.0, 1.0, true,  2, kWahMode }
    };

    return params[juce::jlimit (0, 4, index)];
}

void WahPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: minHz = value; break;
        case 1: maxHz = value; break;
        case 2: q = value; break;
        case 3: level = value; break;
        case 4: autoMode = (int) value; break;
        default: break;
    }

    lfo.setRate (autoRateHz);
}

void WahPedal::process (double* left, double* right, int numSamples) noexcept
{
    const double lo = juce::jmin (minHz, maxHz);
    const double hi = juce::jmax (minHz, maxHz);

    for (int i = 0; i < numSamples; ++i)
    {
        double position = expression;

        if (autoMode == 1)
            position = lfo.next() * 0.5 + 0.5;

        sweep.setTarget (position);
        const double p = sweep.next();

        // The pedal travel is logarithmic in frequency: that is what makes the
        // sweep sound even across the throw rather than bunched at the top.
        const double freq = lo * std::pow (hi / lo, p);

        // Redesigning a biquad every sample is wasteful; 0.4 % is well below the
        // point where the steps are audible.
        if (std::abs (freq - lastFreq) > lastFreq * 0.004)
        {
            filterL.setBandpass (sr, freq, q);
            filterR.setBandpass (sr, freq, q);
            lastFreq = freq;
        }

        // A real wah is a resonant peak on top of the signal, not a pure bandpass:
        // the low end is never fully removed.
        const double wetL = filterL.process (left[i]);
        const double wetR = filterR.process (right[i]);

        left[i]  = sanitise ((left[i]  * 0.25 + wetL * 1.6) * level);
        right[i] = sanitise ((right[i] * 0.25 + wetR * 1.6) * level);
    }
}

//==============================================================================
//  Envelope filter
//==============================================================================
void EnvelopeFilterPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    follower.prepare (sr);
    resetParametersToDefault();
    reset();
}

void EnvelopeFilterPedal::reset() noexcept
{
    follower.reset();
    filterL.reset();
    filterR.reset();
    lastFreq = 0.0;
}

const PedalParam& EnvelopeFilterPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[6] =
    {
        { "Sense",     "",    0.0,   1.0,   0.5, 1.0, false, 0, nullptr },
        { "Base",      "Hz", 80.0,1000.0, 250.0, 0.5, false, 0, nullptr },
        { "Range",     "oct", 0.5,   5.0,   3.0, 1.0, false, 0, nullptr },
        { "Q",         "",    1.0,  10.0,   3.5, 0.6, false, 0, nullptr },
        { "Attack",    "ms",  1.0, 100.0,  10.0, 0.4, false, 0, nullptr },
        { "Direction", "",    0.0,   1.0,   0.0, 1.0, true,  2, kFilterDir }
    };

    return params[juce::jlimit (0, 5, index)];
}

void EnvelopeFilterPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: sensitivity = value; break;
        case 1: baseHz = value; break;
        case 2: rangeOctaves = value; break;
        case 3: q = value; break;
        case 4: attackMs = value; break;
        case 5: direction = (int) value; break;
        default: break;
    }

    follower.setTimes (attackMs * 0.001, releaseMs * 0.001);
}

void EnvelopeFilterPedal::process (double* left, double* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const double env = follower.process (juce::jmax (std::abs (left[i]), std::abs (right[i])));

        // Map the envelope through a compressive curve so that quiet playing still
        // moves the filter: a linear map only opens on the loudest notes.
        double amount = juce::jlimit (0.0, 1.0, std::pow (env * (1.0 + sensitivity * 20.0), 0.55));

        if (direction == 1)
            amount = 1.0 - amount;

        const double freq = juce::jlimit (60.0, sr * 0.45,
                                          baseHz * std::pow (2.0, amount * rangeOctaves));

        if (std::abs (freq - lastFreq) > lastFreq * 0.004)
        {
            filterL.setBandpass (sr, freq, q);
            filterR.setBandpass (sr, freq, q);
            lastFreq = freq;
        }

        left[i]  = sanitise (filterL.process (left[i])  * 1.8);
        right[i] = sanitise (filterR.process (right[i]) * 1.8);
    }
}

//==============================================================================
//  Octaver
//==============================================================================
void OctaverPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);

    inputHp.prepare (sr);
    inputHp.setCutoff (70.0);

    trackLp.prepare (sr);
    trackLp.setCutoff (320.0);

    outputLp.prepare (sr);
    amplitude.prepare (sr);
    amplitude.setTimes (0.002, 0.050);

    resetParametersToDefault();
    reset();
}

void OctaverPedal::reset() noexcept
{
    inputHp.reset();
    trackLp.reset();
    outputLp.reset();
    amplitude.reset();
    flipFlop1 = flipFlop2 = false;
    lastSign = 0.0;
}

const PedalParam& OctaverPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[4] =
    {
        { "Sub 1",  "",    0.0,    1.0,    0.6, 1.0, false, 0, nullptr },
        { "Sub 2",  "",    0.0,    1.0,    0.0, 1.0, false, 0, nullptr },
        { "Direct", "",    0.0,    1.0,    1.0, 1.0, false, 0, nullptr },
        { "Tone",   "Hz", 500.0, 6000.0, 2500.0, 0.5, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 3, index)];
}

void OctaverPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: subLevel = value; break;
        case 1: sub2Level = value; break;
        case 2: dryLevel = value; break;
        case 3: toneHz = value; outputLp.setCutoff (juce::jmin (toneHz, sr * 0.47)); break;
        default: break;
    }
}

void OctaverPedal::process (double* left, double* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const double in = (left[i] + right[i]) * 0.5;

        // Track on a heavily low-passed copy: the analogue originals could only
        // follow a fundamental, which is why they glitch on chords. Keeping that
        // behaviour is the point.
        const double tracked = trackLp.process (inputHp.process (in));
        const double sign = (tracked >= 0.0) ? 1.0 : -1.0;

        if (sign != lastSign && sign > 0.0)
        {
            flipFlop1 = ! flipFlop1;

            if (flipFlop1)
                flipFlop2 = ! flipFlop2;
        }

        lastSign = sign;

        // The square waves are amplitude-modulated by the input envelope so the
        // octave follows the player's dynamics instead of gating on and off.
        const double amp = amplitude.process (in);

        double sub = (flipFlop1 ? 1.0 : -1.0) * amp * subLevel;
        sub += (flipFlop2 ? 1.0 : -1.0) * amp * sub2Level * 0.8;

        sub = outputLp.process (sub);

        const double out = sanitise (in * dryLevel + sub * 1.4);

        left[i] = out;
        right[i] = out;
    }
}

//==============================================================================
//  Pitch shifter
//==============================================================================
void PitchShifterPedal::Shifter::prepare (int bufferSize, int window)
{
    size = juce::nextPowerOfTwo (juce::jmax (256, bufferSize));
    buffer.assign ((size_t) size, 0.0);
    windowSamples = juce::jlimit (64, size / 2, window);
    reset();
}

void PitchShifterPedal::Shifter::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0);
    writeIndex = 0;
    readPhase = 0.0;
}

double PitchShifterPedal::Shifter::process (double input, double ratio) noexcept
{
    if (buffer.empty())
        return input;

    const int mask = size - 1;

    buffer[(size_t) writeIndex] = flushDenormal (input);
    writeIndex = (writeIndex + 1) & mask;

    // Two read heads half a window apart, crossfaded with a raised cosine. As one
    // head drifts too far from the write pointer the other takes over, which is
    // what stops the classic single-head "motorboating".
    readPhase += (ratio - 1.0);

    if (readPhase >= (double) windowSamples) readPhase -= (double) windowSamples;
    if (readPhase < 0.0)                     readPhase += (double) windowSamples;

    auto readAt = [this, mask] (double delay) noexcept
    {
        const double d = juce::jlimit (1.0, (double) (size - 2), delay);
        const int i0 = (int) d;
        const double frac = d - (double) i0;

        const double a = buffer[(size_t) ((writeIndex - i0) & mask)];
        const double b = buffer[(size_t) ((writeIndex - i0 - 1) & mask)];

        return a * (1.0 - frac) + b * frac;
    };

    const double d1 = readPhase + 2.0;
    const double d2 = readPhase + (double) windowSamples * 0.5 + 2.0;

    const double t = readPhase / (double) windowSamples;
    const double w1 = 0.5 - 0.5 * std::cos (constants::kTwoPi * t);
    const double w2 = 1.0 - w1;

    return readAt (d1) * w1 + readAt (d2 > windowSamples ? d2 - windowSamples : d2) * w2;
}

void PitchShifterPedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);

    // A 40 ms window tracks a low E without smearing the attack too badly.
    const int window = (int) (sr * 0.040);
    shifterL.prepare (window * 4, window);
    shifterR.prepare (window * 4, window);

    resetParametersToDefault();
    reset();
}

void PitchShifterPedal::reset() noexcept
{
    shifterL.reset();
    shifterR.reset();
}

const PedalParam& PitchShifterPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[4] =
    {
        { "Pitch", "st",   -12.0,  12.0, -12.0, 1.0, false, 0, nullptr },
        { "Fine",  "cent", -50.0,  50.0,   0.0, 1.0, false, 0, nullptr },
        { "Mix",   "",       0.0,   1.0,   0.5, 1.0, false, 0, nullptr },
        { "Level", "",       0.0,   2.0,   1.0, 1.0, false, 0, nullptr }
    };

    return params[juce::jlimit (0, 3, index)];
}

void PitchShifterPedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: semitones = value; break;
        case 1: fineCents = value; break;
        case 2: mix = value; break;
        case 3: break;
        default: break;
    }

    ratio = semitonesToRatio (semitones) * centsToRatio (fineCents);
}

void PitchShifterPedal::process (double* left, double* right, int numSamples) noexcept
{
    const double level = getParameterValue (3);

    for (int i = 0; i < numSamples; ++i)
    {
        const double wetL = shifterL.process (left[i], ratio);
        const double wetR = shifterR.process (right[i], ratio);

        left[i]  = sanitise ((left[i]  * (1.0 - mix) + wetL * mix) * level);
        right[i] = sanitise ((right[i] * (1.0 - mix) + wetR * mix) * level);
    }
}

//==============================================================================
//  Drive pedal base
//==============================================================================
void DrivePedalBase::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);

    dcL.prepare (sr);
    dcR.prepare (sr);
    dcL.setCutoff (18.0);
    dcR.setCutoff (18.0);

    oversampler.prepare (sr, 4);
    oversamplerR.prepare (sr, 4);

    driveSmooth.prepare (sr, constants::kParamSmoothSeconds);
    levelSmooth.prepare (sr, constants::kParamSmoothSeconds);
    driveSmooth.snapTo (drive);
    levelSmooth.snapTo (level);

    resetParametersToDefault();
    updateFilters();
    reset();
}

void DrivePedalBase::reset() noexcept
{
    preL.reset();  preR.reset();
    postL.reset(); postR.reset();
    midL.reset();  midR.reset();
    dcL.reset();   dcR.reset();
    oversampler.reset();
    oversamplerR.reset();
}

void DrivePedalBase::setOversamplingFactor (int factor) noexcept
{
    oversampler.setFactor (factor);
    oversamplerR.setFactor (factor);
}

void DrivePedalBase::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: drive = value; driveSmooth.setTarget (drive); break;
        case 1: tone = value;  break;
        case 2: level = value; levelSmooth.setTarget (level); break;
        case 3: presence = value; break;
        case 4: voicing = (int) value; break;
        default: break;
    }

    updateFilters();
}

void DrivePedalBase::process (double* left, double* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const double d = driveSmooth.next();
        const double l = levelSmooth.next();

        // Drive maps to gain exponentially: the useful range of a real drive pedal
        // spans about 40 dB and a linear knob would spend most of its travel doing
        // nothing audible.
        const double gain = dbToGain (juce::jmap (d, 0.0, 1.0, -3.0, 40.0));

        double xl = preL.process (left[i]) * gain;
        double xr = preR.process (right[i]) * gain;

        xl = oversampler.processSample (xl, [this] (double v) { return shape (v); });
        xr = oversamplerR.processSample (xr, [this] (double v) { return shape (v); });

        xl = midL.process (xl);
        xr = midR.process (xr);

        xl = postL.process (xl);
        xr = postR.process (xr);

        xl = dcL.process (xl);
        xr = dcR.process (xr);

        left[i]  = sanitise (xl * l);
        right[i] = sanitise (xr * l);
    }
}

//==============================================================================
//  Overdrive - Tube Screamer voicing
//==============================================================================
const PedalParam& OverdrivePedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Drive",    "", 0.0, 1.0, 0.40, 1.0, false, 0, nullptr },
        { "Tone",     "", 0.0, 1.0, 0.50, 1.0, false, 0, nullptr },
        { "Level",    "", 0.0, 1.0, 0.50, 1.0, false, 0, nullptr },
        { "Presence", "", 0.0, 1.0, 0.50, 1.0, false, 0, nullptr },
        { "Voicing",  "", 0.0, 1.0, 0.00, 1.0, true,  3, kOdVoicing }
    };

    return params[juce::jlimit (0, 4, index)];
}

double OverdrivePedal::shape (double x) const noexcept
{
    // Symmetric soft clipping: two diodes to ground, the classic TS circuit.
    // The cubic term below the knee is what keeps quiet playing clean.
    const double t = std::tanh (x * 0.9);
    return t * 0.85 + softClip (x * 0.35) * 0.15;
}

void OverdrivePedal::updateFilters()
{
    // A Tube Screamer's defining feature is the high-pass BEFORE the clipper: it
    // keeps the low end out of the distortion, which is why it tightens a muddy
    // amp instead of thickening it.
    const double hp = (voicing == 2) ? 300.0 : (voicing == 1) ? 900.0 : 720.0;

    preL.setHighpass (sr, hp, 0.707);
    preR.setHighpass (sr, hp, 0.707);

    // The mid hump that makes a TS cut through a band.
    const double midDb = (voicing == 2) ? 7.0 : 5.0;
    midL.setPeaking (sr, 720.0, 0.9, midDb);
    midR.setPeaking (sr, 720.0, 0.9, midDb);

    const double toneHz = juce::jmap (tone, 0.0, 1.0, 1400.0, 7000.0)
                          * juce::jmap (presence, 0.0, 1.0, 0.75, 1.35);

    postL.setLowpass (sr, juce::jmin (toneHz, sr * 0.46), 0.707);
    postR.setLowpass (sr, juce::jmin (toneHz, sr * 0.46), 0.707);
}

//==============================================================================
//  Distortion - DS-1 voicing
//==============================================================================
const PedalParam& DistortionPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Drive",    "", 0.0, 1.0, 0.55, 1.0, false, 0, nullptr },
        { "Tone",     "", 0.0, 1.0, 0.50, 1.0, false, 0, nullptr },
        { "Level",    "", 0.0, 1.0, 0.45, 1.0, false, 0, nullptr },
        { "Presence", "", 0.0, 1.0, 0.55, 1.0, false, 0, nullptr },
        { "Voicing",  "", 0.0, 1.0, 0.00, 1.0, true,  3, kDistVoicing }
    };

    return params[juce::jlimit (0, 4, index)];
}

double DistortionPedal::shape (double x) const noexcept
{
    // Harder knee than the overdrive, with a little asymmetry so the harmonic
    // series is not purely odd.
    const double biased = x + 0.08;
    const double clipped = juce::jlimit (-1.0, 1.0, biased * 1.6);
    return std::tanh (clipped * 2.2) * 0.9 - 0.06;
}

void DistortionPedal::updateFilters()
{
    const double hp = (voicing == 1) ? 220.0 : 120.0;

    preL.setHighpass (sr, hp, 0.707);
    preR.setHighpass (sr, hp, 0.707);

    // The DS-1's mid scoop, exaggerated in the Scooped voicing.
    const double scoopDb = (voicing == 2) ? -8.0 : (voicing == 1) ? -3.0 : -5.0;
    midL.setPeaking (sr, 500.0, 0.8, scoopDb);
    midR.setPeaking (sr, 500.0, 0.8, scoopDb);

    const double toneHz = juce::jmap (tone, 0.0, 1.0, 1800.0, 9000.0)
                          * juce::jmap (presence, 0.0, 1.0, 0.75, 1.4);

    postL.setLowpass (sr, juce::jmin (toneHz, sr * 0.46), 0.707);
    postR.setLowpass (sr, juce::jmin (toneHz, sr * 0.46), 0.707);
}

//==============================================================================
//  Fuzz
//==============================================================================
const PedalParam& FuzzPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Fuzz",     "", 0.0, 1.0, 0.70, 1.0, false, 0, nullptr },
        { "Tone",     "", 0.0, 1.0, 0.50, 1.0, false, 0, nullptr },
        { "Level",    "", 0.0, 1.0, 0.40, 1.0, false, 0, nullptr },
        { "Bias",     "", 0.0, 1.0, 0.50, 1.0, false, 0, nullptr },
        { "Voicing",  "", 0.0, 1.0, 0.00, 1.0, true,  3, kFuzzVoicing }
    };

    return params[juce::jlimit (0, 4, index)];
}

double FuzzPedal::shape (double x) const noexcept
{
    // Strongly asymmetric, and it collapses rather than saturating smoothly - a
    // germanium transistor starving for current. The "Bias" control (presence)
    // shifts the operating point, which is what produces the gated, spluttery
    // sound at the extremes of a real Fuzz Face.
    const double bias = (presence - 0.5) * 1.2;
    const double biased = x + bias;

    double y;

    if (voicing == 1)
    {
        // Big Muff: cascaded symmetric clipping, squarer and more sustaining.
        y = std::tanh (biased * 3.0);
        y = std::tanh (y * 2.5);
    }
    else if (voicing == 2)
    {
        // Tone Bender: harder on the positive swing, splatty on the negative.
        y = (biased > 0.0) ? std::tanh (biased * 2.2)
                           : -std::pow (std::abs (std::tanh (biased * 1.3)), 0.7);
    }
    else
    {
        // Fuzz Face.
        y = (biased > 0.0) ? (1.0 - std::exp (-biased * 2.4))
                           : (-1.0 + std::exp (biased * 1.1)) * 0.8;
    }

    return y - bias * 0.5;
}

void FuzzPedal::updateFilters()
{
    // A real fuzz loads the pickup down: the input impedance is low enough to roll
    // off treble and to interact with the guitar's volume control. That loading is
    // modelled here as a gentle pre-filter rather than a flat input.
    preL.setHighpass (sr, 60.0, 0.6);
    preR.setHighpass (sr, 60.0, 0.6);

    midL.setPeaking (sr, 1100.0, 0.7, voicing == 1 ? -6.0 : 2.5);
    midR.setPeaking (sr, 1100.0, 0.7, voicing == 1 ? -6.0 : 2.5);

    const double toneHz = juce::jmap (tone, 0.0, 1.0, 900.0, 6500.0);

    postL.setLowpass (sr, juce::jmin (toneHz, sr * 0.46), 0.707);
    postR.setLowpass (sr, juce::jmin (toneHz, sr * 0.46), 0.707);
}

//==============================================================================
//  Boost
//==============================================================================
const PedalParam& BoostPedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[5] =
    {
        { "Gain",     "", 0.0, 1.0, 0.35, 1.0, false, 0, nullptr },
        { "Tone",     "", 0.0, 1.0, 0.60, 1.0, false, 0, nullptr },
        { "Level",    "", 0.0, 1.0, 0.60, 1.0, false, 0, nullptr },
        { "Treble",   "", 0.0, 1.0, 0.50, 1.0, false, 0, nullptr },
        { "Voicing",  "", 0.0, 1.0, 0.00, 1.0, true,  3, kBoostVoicing }
    };

    return params[juce::jlimit (0, 4, index)];
}

double BoostPedal::shape (double x) const noexcept
{
    // Nearly clean: just enough curvature at the top to thicken without fuzzing.
    return softClip (x * 0.55) * 1.6;
}

void BoostPedal::updateFilters()
{
    const double hp = (voicing == 1) ? 700.0 : (voicing == 2) ? 30.0 : 120.0;

    preL.setHighpass (sr, hp, 0.707);
    preR.setHighpass (sr, hp, 0.707);

    const double tiltDb = juce::jmap (presence, 0.0, 1.0, -4.0, 6.0);
    midL.setHighShelf (sr, 2200.0, 0.7, tiltDb);
    midR.setHighShelf (sr, 2200.0, 0.7, tiltDb);

    const double toneHz = juce::jmap (tone, 0.0, 1.0, 3000.0, 14000.0);
    postL.setLowpass (sr, juce::jmin (toneHz, sr * 0.46), 0.707);
    postR.setLowpass (sr, juce::jmin (toneHz, sr * 0.46), 0.707);
}

//==============================================================================
//  Volume pedal
//==============================================================================
void VolumePedal::prepare (double sampleRate, int maxBlockSize)
{
    prepareBase (sampleRate, maxBlockSize);
    gain.prepare (sr, 0.010);
    gain.snapTo (1.0);
    resetParametersToDefault();
    reset();
}

void VolumePedal::reset() noexcept
{
    gain.snapTo (expression);
}

const PedalParam& VolumePedal::getParameterDescriptor (int index) const noexcept
{
    static const PedalParam params[3] =
    {
        { "Min",   "", 0.0, 1.0, 0.0, 1.0, false, 0, nullptr },
        { "Max",   "", 0.0, 2.0, 1.0, 1.0, false, 0, nullptr },
        { "Taper", "", 0.0, 1.0, 0.0, 1.0, true,  2, kTaper }
    };

    return params[juce::jlimit (0, 2, index)];
}

void VolumePedal::parameterChanged (int index, double value)
{
    switch (index)
    {
        case 0: minLevel = value; break;
        case 1: maxLevel = value; break;
        case 2: taper = (int) value; break;
        default: break;
    }
}

void VolumePedal::process (double* left, double* right, int numSamples) noexcept
{
    // A real volume pedal uses an audio-taper pot, which is roughly logarithmic.
    const double position = (taper == 0) ? (expression * expression * expression) : expression;

    gain.setTarget (minLevel + (maxLevel - minLevel) * position);

    for (int i = 0; i < numSamples; ++i)
    {
        const double g = gain.next();
        left[i]  = sanitise (left[i]  * g);
        right[i] = sanitise (right[i] * g);
    }
}

} // namespace luthier
