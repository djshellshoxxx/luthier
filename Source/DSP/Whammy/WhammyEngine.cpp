#include "WhammyEngine.h"

namespace luthier
{

void WhammyEngine::prepare (double sampleRate, int strings) noexcept
{
    sr = sampleRate;
    numStrings = juce::jlimit (1, kMaxStrings, strings);

    // Engine spec 8: 5 ms smoothing, fast enough to track shred-speed flutter and
    // slow enough that a dive-bomb never zippers.
    positionSmooth.prepare (sr, 0.005);
    positionSmooth.snapTo (0.0);

    for (auto& s : stringSmooth)
    {
        s.prepare (sr, 0.005);
        s.snapTo (0.0);
    }

    springBand1.setBandpass (sr, 240.0, 5.0);
    springBand2.setBandpass (sr, 430.0, 7.0);

    // Springs ring for roughly 80 ms.
    springDecay = std::exp (-1.0 / (0.080 * sr));

    setBridgeType (bridgeType);
    reset();
}

void WhammyEngine::reset() noexcept
{
    positionSmooth.snapTo (0.0);

    for (auto& s : stringSmooth)
        s.snapTo (0.0);

    centOffsets.fill (0.0);

    springEnv = 0.0;
    lastPosition = 0.0;
    springBand1.reset();
    springBand2.reset();
    springRng.setSeed (kSpringSeed);
}

//==============================================================================
void WhammyEngine::setBridgeType (BridgeType t) noexcept
{
    bridgeType = t;

    switch (t)
    {
        case BridgeType::Fixed:
            downRange = 0.0; upRange = 0.0; restrictToDown = false;
            break;

        case BridgeType::VintageTrem:
            downRange = 2.0; upRange = 1.0; restrictToDown = false;
            break;

        case BridgeType::FloydRose:
            downRange = 24.0; upRange = 12.0; restrictToDown = false;
            break;

        case BridgeType::TransTrem:
            downRange = 12.0; upRange = 5.0; restrictToDown = false;
            break;

        case BridgeType::Bigsby:
            downRange = 1.0; upRange = 0.5; restrictToDown = false;
            break;

        case BridgeType::NumTypes:
        default:
            break;
    }
}

void WhammyEngine::setPosition (double normalised) noexcept
{
    double p = juce::jlimit (-1.0, 1.0, normalised);

    if (restrictToDown && p > 0.0)
        p = 0.0;

    positionSmooth.setTarget (p);
}

void WhammyEngine::setRange (double downSemitones, double upSemitones) noexcept
{
    // A hardtail has no arm. The bridge sends the user's ranges every block,
    // which used to undo setBridgeType (Fixed)'s zero range.
    if (bridgeType == BridgeType::Fixed)
    {
        downRange = upRange = 0.0;
        return;
    }

    downRange = juce::jlimit (0.0, 36.0, downSemitones);
    upRange = juce::jlimit (0.0, 24.0, upSemitones);
}

void WhammyEngine::setTransposeLock (int semitones) noexcept
{
    transposeLock = juce::jlimit (-12, 12, semitones);
}

void WhammyEngine::setStringPosition (int stringIndex, double normalised) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        stringSmooth[(size_t) stringIndex].setTarget (juce::jlimit (-1.0, 1.0, normalised));
}

//==============================================================================
void WhammyEngine::triggerSprings (double velocity) noexcept
{
    if (bridgeType != BridgeType::FloydRose || springAmount <= 1.0e-4)
        return;

    springEnv = juce::jmin (1.0, springEnv + std::abs (velocity) * 4.0);
}

void WhammyEngine::updateBlock (int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    // Advance the smoothers by a whole block in one step.
    double pos = 0.0;

    for (int i = 0; i < numSamples; ++i)
        pos = positionSmooth.next();

    if (perString)
        for (int s = 0; s < numStrings; ++s)
            for (int i = 0; i < numSamples; ++i)
                stringSmooth[(size_t) s].next();

    // A fast return toward centre sets the springs ringing.
    const double velocity = (pos - lastPosition) / juce::jmax (1.0, (double) numSamples) * sr;

    if (std::abs (velocity) > 6.0 && std::abs (pos) < std::abs (lastPosition))
        triggerSprings (juce::jmin (1.0, std::abs (velocity) / 40.0));

    lastPosition = pos;

    // ---- map bar position to per-string offsets ------------------------------
    for (int s = 0; s < numStrings; ++s)
    {
        double p = perString ? stringSmooth[(size_t) s].getCurrent() : pos;

        if (restrictToDown && p > 0.0)
            p = 0.0;

        double semitones = (p < 0.0) ? p * downRange : p * upRange;

        if (transposeLockEnabled && bridgeType == BridgeType::TransTrem)
            semitones += (double) transposeLock;

        // Both mappings are the same arithmetic in cents; the difference is that
        // a TransTrem applies the SAME RATIO to every string, while a vintage trem
        // applies the same BRIDGE MOVEMENT, which is a bigger pitch change on the
        // slacker strings. Modelling that unevenness is what makes a vintage trem
        // detune a chord.
        if (bridgeType == BridgeType::TransTrem)
        {
            centOffsets[(size_t) s] = semitones * 100.0;
        }
        else
        {
            // Thinner, tighter strings move proportionally less for the same
            // bridge travel. String 0 is the thinnest, so it is the least affected.
            const double slackness = 0.82 + 0.36 * ((double) s / juce::jmax (1.0, (double) (numStrings - 1)));
            centOffsets[(size_t) s] = semitones * 100.0 * slackness;
        }
    }

    for (int s = numStrings; s < kMaxStrings; ++s)
        centOffsets[(size_t) s] = 0.0;
}

double WhammyEngine::getCentOffset (int stringIndex) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return 0.0;

    return centOffsets[(size_t) stringIndex];
}

double WhammyEngine::getRatio (int stringIndex) const noexcept
{
    return centsToRatio (getCentOffset (stringIndex));
}

//==============================================================================
double WhammyEngine::processSpringNoise() noexcept
{
    if (springEnv <= 1.0e-6)
        return 0.0;

    const double n = springRng.nextBipolar();
    const double ring = springBand1.process (n) * 0.7 + springBand2.process (n) * 0.5;

    springEnv *= springDecay;

    if (springEnv < 1.0e-6)
        springEnv = 0.0;

    return sanitise (ring * springEnv * springAmount * 0.06);
}

} // namespace luthier
