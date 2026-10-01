#include "SlideEngine.h"
#include "../Noise/PlayingNoise.h"

namespace luthier
{

const char* const SlideEngine::kLowActionMessage =
    "This guitar was not built for slide. Open Workshop to fit a hi-nut or a "
    "different bridge.";

//==============================================================================
const SlideMaterialProperties& getSlideMaterial (SlideMaterial m) noexcept
{
    // slide-guitar.md 2.1. Clank centres from 5.2 - brass about 1.2 kHz, glass
    // about 2.5 kHz - with the others placed between them by hardness.
    static const SlideMaterialProperties table[(size_t) SlideMaterial::numMaterials] =
    {
        { "Glass",                  0.30, 0.70, 0.25, 2500.0, 0xffbfe3e8 },
        { "Glass (thick wall)",     0.25, 0.65, 0.25, 2200.0, 0xffa9d6dc },
        { "Brass",                  0.12, 0.85, 0.40, 1200.0, 0xffd7b458 },
        { "Steel",                  0.10, 0.95, 0.45, 1600.0, 0xffc8ccd2 },
        { "Ceramic",                0.22, 0.75, 0.30, 1900.0, 0xffeae4d8 },
        { "Bone",                   0.40, 0.55, 0.50, 1500.0, 0xffe8dcc2 },
        { "Brass-plated steel bar", 0.11, 0.90, 0.42, 1400.0, 0xffdcbf6a },
    };

    return table[juce::jlimit (0, (int) SlideMaterial::numMaterials - 1, (int) m)];
}

//==============================================================================
void SlideEngine::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (8000.0, sampleRate);
    reset();
}

void SlideEngine::reset() noexcept
{
    underBar.fill (false);
    assistPrimed.fill (false);
    assistedFret.fill (0.0);
    moves.fill ({});
    barString = -1;
    landing = false;
    overlayFret = -1.0;

    // The technique controls' bar state goes back to its start too: left
    // behind, the auto-vibrato's hold timer, ramp and phase carried on from the
    // last render, so a reset render shook from its first block where a fresh
    // one waited out the hold ("Auto-Vibrato Hold", Combo determinism).
    lastPositionSource = ControlSource::none;
    controlledFret = -1.0;
    relativeOffset = 0.0;
    smoothedTarget = -1.0;
    relativeActive = false;
    gestureActive = false;
    runningGesture = {};
    gestureElapsed = 0.0;
    lastControlledFret = -1.0;
    heldSeconds = 0.0;
    vibratoPhase = 0.0;
    vibratoRamp = 0.0;
    controlVibratoCents = 0.0;
}

bool SlideEngine::noteOn (int s, int numStrings) noexcept
{
    landing = false;

    if (! settings.enabled || ! juce::isPositiveAndBelow (s, kMaxStrings))
        return false;

    // slide-technique-controls.md 1: a string outside the contact mask is fretted.
    if (! contactsString (s))
        return false;

    bool anyUnder = false;

    for (int i = 0; i < juce::jmin (numStrings, kMaxStrings); ++i)
        anyUnder = anyUnder || underBar[(size_t) i];

    if (settings.mode == SlideMode::hybrid)
    {
        // One string under the bar at a time; the fingers fret the rest. A
        // note on another string while the bar is sounding is fretted.
        if (barString >= 0 && barString != s && underBar[(size_t) barString])
            return false;

        barString = s;
    }

    landing = ! anyUnder;
    underBar[(size_t) s] = true;
    assistPrimed[(size_t) s] = false;
    return true;
}

void SlideEngine::noteOff (int s) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return;

    underBar[(size_t) s] = false;

    if (barString == s)
        barString = -1;
}

bool SlideEngine::isUnderBar (int s) const noexcept
{
    return settings.enabled && juce::isPositiveAndBelow (s, kMaxStrings) && underBar[(size_t) s];
}

//==============================================================================
double SlideEngine::contactFret (int s, double barFret, int numStrings, double scaleLengthMm) const noexcept
{
    const double length = juce::jmax (100.0, scaleLengthMm);
    const double x = length * (1.0 - std::pow (2.0, -barFret / 12.0));

    // 3.1: tan(slant) x spacing x (s - centre), along the string. String 0 is
    // the highest, so a positive slant reaches further up the neck on the
    // treble side.
    const double centre = 0.5 * (double) (juce::jmax (1, numStrings) - 1);
    const double offset = std::tan (juce::degreesToRadians (settings.slantDegrees))
                          * kStringSpacingMm * ((double) s - centre);

    const double xs = juce::jlimit (0.0, length * 0.95, x - offset);
    return -12.0 * std::log2 (1.0 - xs / length);
}

void SlideEngine::startMove (int s, double fromFret, double toFret, double seconds) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return;

    auto& m = moves[(size_t) s];
    m.from = fromFret;
    m.to = toFret;
    m.elapsed = 0.0;
    m.duration = juce::jmax (0.0, seconds);
    m.active = m.duration > 0.0;
}

double SlideEngine::advanceBar (int s, double heldFret, int numSamples) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return heldFret;

    auto& m = moves[(size_t) s];

    if (! m.active)
        return heldFret;

    m.elapsed += (double) numSamples / sr;

    if (m.elapsed >= m.duration)
    {
        m.active = false;
        return m.to;
    }

    return m.from + (m.to - m.from) * (m.elapsed / m.duration);
}

double SlideEngine::vibratoCents (double depthMm, double barFret, double scaleLengthMm) noexcept
{
    // f ~ 1 / (L - x), so a small movement dx is 1200 / ln 2 x dx / (L - x) cents.
    const double length = juce::jmax (100.0, scaleLengthMm);
    const double sounding = length * std::pow (2.0, -barFret / 12.0);
    return 1200.0 / std::log (2.0) * depthMm / juce::jmax (10.0, sounding);
}

double SlideEngine::assist (int s, double rawFret, int numSamples) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return rawFret;

    const double amount = juce::jlimit (0.0, 1.0, settings.intonationAssist);
    const double target = rawFret + amount * (std::round (rawFret) - rawFret);

    auto& current = assistedFret[(size_t) s];

    // A new note starts where the bar is, not where the last one ended up.
    if (! assistPrimed[(size_t) s])
    {
        current = rawFret;
        assistPrimed[(size_t) s] = true;
    }

    const double alpha = 1.0 - std::exp (-(double) numSamples / (kAssistSeconds * sr));
    current += (target - current) * alpha;
    return current;
}

double SlideEngine::sustainScale (int s) const noexcept
{
    if (! isUnderBar (s))
        return 1.0;

    const auto& material = getSlideMaterial (bar.material);

    // 3: the segment behind the contact is damped (fully for lap steel and
    // dobro, partly for a finger behind a bottleneck)...
    // The string's decay is only partly set by this scale (the loop filter and
    // the body take their share), so the scale has to move further than the
    // decay it produces: full damping here is what shortens a lap-steel note
    // by the quarter slide-guitar.md 9 asks for.
    const double behind = 1.0 - 0.6 * juce::jlimit (0.0, 1.0, settings.dampingBehind);

    // ...and the bar absorbs energy at the contact: softer materials more,
    // heavier bars less, because they couple less.
    const double massFactor = std::pow (65.0 / juce::jlimit (5.0, 500.0, bar.massGrams), 0.6);
    const double contact = 1.0 - material.damping * 0.5 * juce::jmin (2.0, massFactor);

    // Too little pressure and the string rides on the bar and loses more.
    const double pressure = 0.85 + 0.15 * juce::jlimit (0.0, 1.0, settings.pressure);

    // SPEC-SWEEP SG-10 (slide-guitar.md 3): too much, and the string is
    // pressed onto the frets underneath and chokes - from 0.8 up to a third
    // off the sustain at full pressure.
    const double over = juce::jmax (0.0, juce::jlimit (0.0, 1.0, settings.pressure) - 0.8) / 0.2;
    const double choke = 1.0 - 0.35 * over * over;

    return juce::jlimit (0.05, 1.0, behind * contact * pressure * choke);
}

NoiseEvent SlideEngine::makeClank (int s, double velocity) const noexcept
{
    NoiseEvent e;
    e.noiseClass = NoiseClass::clank;
    e.stringIndex = s;

    const auto& material = getSlideMaterial (bar.material);

    // 5.2: level is amount x landing velocity, plus a rattle when the bar
    // sits too lightly to hold the string.
    const double rattle = settings.pressure < 0.3 ? (0.3 - settings.pressure) / 0.3 : 0.0;

    e.level = PlayingNoise::kNoteReference * dbToGain (-20.0) * settings.clankAmount
              * (juce::jlimit (0.0, 1.0, velocity) + 0.5 * rattle);

    // Spectrum by material, and mass lowers it.
    e.startHz = e.endHz = material.clankHz * std::pow (65.0 / juce::jlimit (5.0, 500.0, bar.massGrams), 0.25);
    e.q = 5.0 + 10.0 * (1.0 - material.damping);
    e.brightness = material.brightness * 0.6;
    e.texture = NoiseTexture::metallic;
    e.attackMs = 0.3;
    e.decayMs = 20.0 + 60.0 * (1.0 - material.damping);

    // A rattling bar keeps hitting the string, once a cycle.
    if (rattle > 0.0)
    {
        e.holdMs = 150.0 * rattle;
        e.burstHz = 0.0;
    }

    return e;
}

//==============================================================================
// slide-technique-controls.md (TECHNIQUES)
//==============================================================================
TechniqueTriggerConfig SlideControlSettings::triggerConfig (bool slideModeOn) const noexcept
{
    // 1, "Slide gesture trigger: keyswitch or CC".
    TechniqueTriggerConfig c;
    c.armed = slideModeOn;
    c.keyswitches = { TechniqueKeyswitch::slideGesture, -1, -1, -1 };
    c.source = gestureOnCc ? TriggerSource::controller : TriggerSource::keyswitch;
    c.triggerCc = gestureCc;
    return c;
}

int slideContactMaskFor (int choice, int numStrings) noexcept
{
    const int n = juce::jlimit (1, SlideEngine::kMaxStrings, numStrings);

    switch (choice)
    {
        case 1:  return ((1 << n) - 1) & ~((1 << juce::jmax (0, n - 3)) - 1);   // bass 3: the highest indices
        case 2:  return (1 << juce::jmin (3, n)) - 1;                             // treble 3: indices 0-2
        case 0:
        default: return 0;
    }
}

void SlideEngine::setControls (const SlideControlSettings& c) noexcept
{
    controls = c;
    controls.speedLimitCentsPerSecond = juce::jmax (1.0, c.speedLimitCentsPerSecond);
}

void SlideEngine::setPositionSource (ControlSource source, int cc) noexcept
{
    controls.positionSource = source;
    controls.positionCc = juce::jlimit (0, 127, cc);
}

bool SlideEngine::contactsString (int s) const noexcept
{
    return controls.contactMask == 0 || (juce::isPositiveAndBelow (s, kMaxStrings) && (controls.contactMask & (1 << s)) != 0);
}

void SlideEngine::triggerGesture (const SlideGesture& g) noexcept
{
    runningGesture = g;
    runningGesture.durationMs = juce::jmax (1.0, g.durationMs);
    gestureElapsed = 0.0;
    gestureActive = true;
    controlledFret = g.fromFret;
    smoothedTarget = g.fromFret;
}

double SlideEngine::advanceTowards (double current, double target, double seconds) const noexcept
{
    // 1: the speed limit, in cents per second; a fret is a hundred cents.
    const double maxStep = controls.speedLimitCentsPerSecond / 100.0 * seconds;
    return current + juce::jlimit (-maxStep, maxStep, target - current);
}

void SlideEngine::advanceControls (int numSamples, const TechniqueControls& sources, const TechniqueTriggers& triggers) noexcept
{
    blockSeconds = (double) numSamples / sr;

    if (gestureRequested.exchange (false) && settings.enabled)
        triggerGesture (controls.gesture);

    for (int i = 0; i < triggers.getNumEvents(); ++i)
    {
        const auto& g = triggers.getEvent (i);

        if (g.technique == TechniqueId::slide && g.role == 0 && g.on)
            triggerGesture (controls.gesture);
    }

    // 1: slant and pressure from their sources, when the user picked one.
    if (controls.slantSource != ControlSource::none && sources.hasValue (controls.slantSource, controls.slantCc, 0))
        settings.slantDegrees = 30.0 * sources.read (controls.slantSource, controls.slantCc, 0, true);

    if (controls.pressureSource != ControlSource::none && sources.hasValue (controls.pressureSource, controls.pressureCc, 0))
        settings.pressure = juce::jlimit (0.0, 1.0, sources.read (controls.pressureSource, controls.pressureCc, 0, false));

    bool anyUnder = false;

    for (auto u : underBar)
        anyUnder = anyUnder || u;

    if (gestureActive)
    {
        // 2: a scripted move, on its curve, slant and pressure travelling with it.
        gestureElapsed += blockSeconds;
        const double t = juce::jlimit (0.0, 1.0, gestureElapsed * 1000.0 / runningGesture.durationMs);

        double shaped = t;

        switch (runningGesture.curve)
        {
            case SlideCurve::easeIn:    shaped = t * t; break;
            case SlideCurve::easeOut:   shaped = 1.0 - (1.0 - t) * (1.0 - t); break;
            case SlideCurve::easeInOut: shaped = t * t * (3.0 - 2.0 * t); break;
            case SlideCurve::linear:
            case SlideCurve::numCurves:
            default: break;
        }

        controlledFret = runningGesture.fromFret + (runningGesture.toFret - runningGesture.fromFret) * shaped;
        settings.slantDegrees = juce::jmap (t, runningGesture.slantStartDegrees, runningGesture.slantEndDegrees);
        settings.pressure = juce::jlimit (0.0, 1.0, runningGesture.pressure);
        relativeActive = false;

        if (t >= 1.0)
            gestureActive = false;
    }
    else if (controls.positionSource != ControlSource::none
             && sources.hasValue (controls.positionSource, controls.positionCc, 0))
    {
        const bool bipolarSource = controls.positionSource == ControlSource::pitchBend;
        double target;

        if (controls.relative)
            target = sources.read (controls.positionSource, controls.positionCc, 0, bipolarSource) * controls.relativeRangeFrets;
        else
            target = juce::jlimit (0.0, SlideControlSettings::kAbsoluteFrets,
                                   juce::jmax (0.0, sources.read (controls.positionSource, controls.positionCc, 0, false))
                                     * SlideControlSettings::kAbsoluteFrets);

        const bool modeChanged = relativeActive != controls.relative;
        relativeActive = controls.relative;

        if (smoothedTarget < -0.5 || modeChanged)
        {
            smoothedTarget = target;
            (relativeActive ? relativeOffset : controlledFret) = target;
        }

        // 7: a source swap does not click - the target glides over 10 ms ...
        const double alpha = 1.0 - std::exp (-blockSeconds / SlideControlSettings::kSourceCrossfadeSeconds);
        smoothedTarget += (target - smoothedTarget) * alpha;

        // ... and the bar never moves faster than the speed limit.
        if (relativeActive)
        {
            relativeOffset = advanceTowards (relativeOffset, smoothedTarget, blockSeconds);
            controlledFret = -1.0;
        }
        else
        {
            controlledFret = advanceTowards (controlledFret < 0.0 ? smoothedTarget : controlledFret, smoothedTarget, blockSeconds);
        }
    }
    else if (! anyUnder)
    {
        // Nothing drives the bar and nothing is under it: let go of the last position.
        controlledFret = -1.0;
        smoothedTarget = -1.0;
        relativeActive = false;
        relativeOffset = 0.0;
    }

    lastPositionSource = controls.positionSource;

    // 1: "Auto-vibrato on hold": still for more than 300 ms, the bar starts to shake.
    const double position = controlledFret >= 0.0 ? controlledFret : overlayFret;

    if (controls.autoVibrato && anyUnder && position >= 0.0)
    {
        if (std::abs (position - lastControlledFret) < 0.02)
            heldSeconds += blockSeconds;
        else
            heldSeconds = 0.0;

        lastControlledFret = position;

        if (heldSeconds > SlideControlSettings::kAutoVibratoHoldSeconds)
        {
            vibratoRamp = juce::jmin (1.0, vibratoRamp + blockSeconds / 0.05);
            vibratoPhase = std::fmod (vibratoPhase + blockSeconds * controls.autoVibratoRateHz, 1.0);
            controlVibratoCents = controls.autoVibratoDepthCents * vibratoRamp
                                    * std::sin (vibratoPhase * juce::MathConstants<double>::twoPi);
        }
        else
        {
            vibratoRamp = 0.0;
            controlVibratoCents = 0.0;
        }
    }
    else
    {
        heldSeconds = 0.0;
        vibratoRamp = 0.0;
        controlVibratoCents = 0.0;
        lastControlledFret = position;
    }
}

double SlideEngine::controlledBarFret (int s, double noteBarFret) noexcept
{
    juce::ignoreUnused (s);

    if (relativeActive && ! gestureActive)
        return juce::jlimit (0.0, SlideControlSettings::kAbsoluteFrets, noteBarFret + relativeOffset);

    if (controlledFret >= 0.0)
        return controlledFret;

    return noteBarFret;
}

} // namespace luthier
