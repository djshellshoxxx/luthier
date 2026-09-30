#include "BendEngine.h"

namespace luthier
{

TechniqueTriggerConfig BendSettings::triggerConfig() const noexcept
{
    // 2, "Pre-bend: keyswitch or CC triggers a pre-bent note-on".
    TechniqueTriggerConfig c;
    c.armed = armed;
    c.keyswitches = { TechniqueKeyswitch::preBend, -1, -1, -1 };
    c.source = preBendOnCc ? TriggerSource::controller : TriggerSource::keyswitch;
    c.triggerCc = preBendCc;
    return c;
}

//==============================================================================
BendEngine::BendEngine()
{
    // The drawn curve starts as the exponential one, which is where a player
    // drawing their own would begin. Here, not in prepare(): a host may save
    // before it prepares, and a prepare after a restore must keep the curve.
    for (int i = 0; i < kCurvePoints; ++i)
    {
        const double x = (double) i / (double) (kCurvePoints - 1);
        curvePoints[(size_t) i].store (x * x);
    }
}

void BendEngine::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (1.0, sampleRate);

    reset();
}

void BendEngine::reset() noexcept
{
    for (int s = 0; s < kMaxStrings; ++s)
    {
        strings[(size_t) s] = StringState {};
        liveCents[(size_t) s].store (0.0);
    }

    preBendArmed = false;
}

void BendEngine::setCustomScale (const MicrotonalScale& scale) noexcept
{
    const int building = 1 - liveScale.load();
    scales[(size_t) building] = scale;
    liveScale.store (building);
}

void BendEngine::setDrawnCurvePoint (int index, double value) noexcept
{
    if (juce::isPositiveAndBelow (index, kCurvePoints))
        curvePoints[(size_t) index].store (juce::jlimit (0.0, 1.0, value));
}

double BendEngine::getDrawnCurvePoint (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, kCurvePoints) ? curvePoints[(size_t) index].load() : 0.0;
}

//==============================================================================
double BendEngine::curveValue (BendCurve curve, double x) const noexcept
{
    x = juce::jlimit (0.0, 1.0, x);

    switch (curve)
    {
        // "exponential (matches finger mechanics)": the last of the throw
        // takes the most effort, so the pitch comes late.
        case BendCurve::exponential:
            return x * x;

        case BendCurve::drawn:
        {
            const double pos = x * (double) (kCurvePoints - 1);
            const int i = juce::jlimit (0, kCurvePoints - 2, (int) std::floor (pos));
            const double f = pos - (double) i;
            return juce::jmap (f, curvePoints[(size_t) i].load(), curvePoints[(size_t) i + 1].load());
        }

        case BendCurve::linear:
        case BendCurve::numCurves:
        default:
            return x;
    }
}

double BendEngine::shapeBend (double x) const noexcept
{
    const double shaped = curveValue (settings.bendCurve, std::abs (x));
    return x < 0.0 ? -shaped : shaped;
}

double BendEngine::shapeRelease (double t) const noexcept
{
    return 1.0 - curveValue (settings.releaseCurve, t);
}

double BendEngine::quantiseTarget (double midiCents) const noexcept
{
    switch (settings.quantise)
    {
        case BendQuantise::quarterTone:
        case BendQuantise::edo24:  return std::round (midiCents / 50.0) * 50.0;
        case BendQuantise::semitone: return std::round (midiCents / 100.0) * 100.0;

        case BendQuantise::edo22:
        case BendQuantise::edo31:
        case BendQuantise::edo53:
        {
            const double step = 1200.0 / (settings.quantise == BendQuantise::edo22 ? 22.0
                                          : settings.quantise == BendQuantise::edo31 ? 31.0 : 53.0);
            return MicrotonalScale::kRootMidiCents
                     + std::round ((midiCents - MicrotonalScale::kRootMidiCents) / step) * step;
        }

        case BendQuantise::custom:
            return scales[(size_t) liveScale.load()].nearest (midiCents);

        case BendQuantise::none:
        case BendQuantise::numModes:
        default:
            return midiCents;
    }
}

//==============================================================================
void BendEngine::noteOn (int s, int channel) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return;

    auto& st = strings[(size_t) s];
    st.sounding = true;
    st.channel = juce::jlimit (1, 16, channel);

    // 4: "Per-string bend range change takes effect at next note-on to avoid
    // mid-note pitch jumps." The global range likewise.
    st.globalRange = settings.globalRangeCents;
    st.stringRange = settings.stringRangeCents[(size_t) s];
    st.sinceOn = 0.0;
    st.vibPhase = 0.0;

    if (preBendArmed && settings.armed)
    {
        // 2: the note starts bent and releases toward nominal.
        st.preBend = settings.preBendCents;
        st.preBendElapsed = 0.0;
        preBendArmed = false;
    }
    else
    {
        st.preBend = 0.0;
    }
}

void BendEngine::noteOff (int s) noexcept
{
    if (juce::isPositiveAndBelow (s, kMaxStrings))
        strings[(size_t) s].sounding = false;
}

void BendEngine::processBlock (int numSamples, const TechniqueTriggers& triggers) noexcept
{
    const double dt = (double) numSamples / sr;

    if (preBendRequested.exchange (false) && settings.armed)
        preBendArmed = true;

    if (settings.armed)
        for (int i = 0; i < triggers.getNumEvents(); ++i)
        {
            const auto& g = triggers.getEvent (i);

            if (g.technique == TechniqueId::bend && g.role == 0 && g.on)
                preBendArmed = true;
        }

    for (auto& st : strings)
    {
        st.sinceOn += dt;
        st.vibPhase = std::fmod (st.vibPhase + dt * settings.vibratoRateHz, 1.0);

        if (st.preBend != 0.0)
        {
            st.preBendElapsed += dt;

            if (st.preBendElapsed * 1000.0 >= juce::jmax (1.0, settings.preBendReleaseMs))
                st.preBend = 0.0;
        }
    }
}

double BendEngine::centsFor (int s, double baseMidiCents, const TechniqueControls& controls) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return 0.0;

    auto& st = strings[(size_t) s];

    // ---- 1.1 the global bend ------------------------------------------------------
    double global = 0.0;

    {
        const int channel = settings.stringSource == StringBendSource::mpePitchBend ? 1 : 0;
        const auto source = settings.globalSource == ControlSource::none ? ControlSource::pitchBend : settings.globalSource;
        const double v = controls.read (source, settings.globalCc, channel, source == ControlSource::pitchBend);
        global = shapeBend (v) * st.globalRange;
    }

    // ---- 1.2 the string's own ------------------------------------------------------
    double own = 0.0;

    switch (settings.stringSource)
    {
        case StringBendSource::mpePitchBend:
            own = shapeBend (controls.pitchBend (st.channel)) * st.stringRange;
            break;

        case StringBendSource::mpeY:
            if (controls.hasValue (ControlSource::mpeY, 74, st.channel))
                own = shapeBend (controls.read (ControlSource::mpeY, 74, st.channel, true)) * st.stringRange;
            break;

        case StringBendSource::customCc:
        {
            const int number = juce::jlimit (0, 127, settings.stringCcBase + s);

            if (controls.hasValue (ControlSource::customCc, number, 0))
                own = shapeBend (controls.read (ControlSource::customCc, number, 0, true)) * st.stringRange;
            break;
        }

        case StringBendSource::none:
        case StringBendSource::numSources:
        default:
            break;
    }

    // The fretboard's drag bends the strings like a hand pushing across them.
    own += shapeBend (controls.getFretboardBend()) * st.stringRange;

    // ---- 1.4 the pre-bend, releasing ----------------------------------------------
    double pre = 0.0;

    if (st.preBend != 0.0)
    {
        const double t = st.preBendElapsed * 1000.0 / juce::jmax (1.0, settings.preBendReleaseMs);
        pre = st.preBend * shapeRelease (t);
    }

    double bend = global + own + pre;

    // ---- 3 quantise the held pitch --------------------------------------------------
    if (settings.quantise != BendQuantise::none)
    {
        const double absolute = baseMidiCents + bend;
        const double target = quantiseTarget (absolute);
        bend += (target - absolute) * juce::jlimit (0.0, 1.0, settings.snap);
    }

    // ---- 1.3 vibrato, on top ---------------------------------------------------------
    const double sinceOnset = st.sinceOn - settings.vibratoOnsetMs * 0.001;

    if (settings.vibratoSource != VibratoSource::off && sinceOnset > 0.0 && st.sounding)
    {
        // Engages after the delay, reaching full depth in 30 ms (9: within 50),
        // shaped like a release run backwards.
        const double ramp = 1.0 - shapeRelease (juce::jmin (1.0, sinceOnset / kVibratoRampSeconds));

        double depth = settings.vibratoDepthCents;

        if (settings.vibratoSource == VibratoSource::aftertouch)
            depth *= controls.pressure (0);
        else if (settings.vibratoSource == VibratoSource::mpeZ)
            depth *= controls.pressure (st.channel);

        bend += depth * ramp * std::sin (st.vibPhase * constants::kTwoPi);
    }

    bend = std::isfinite (bend) ? juce::jlimit (-4800.0, 4800.0, bend) : 0.0;
    liveCents[(size_t) s].store (bend, std::memory_order_relaxed);
    return bend;
}

} // namespace luthier
