#include "PickupEngine.h"

namespace luthier
{

namespace
{
    /** Longest positional comb we ever need: half a string length at the lowest
        pitch we support. Sized in prepare() from the real sample rate. */
    int combBufferSizeFor (double sampleRate) noexcept
    {
        const double longest = sampleRate / constants::kMinStringHz;
        int size = 64;

        while ((double) size < longest + 8.0)
            size <<= 1;

        return size;
    }
}

//==============================================================================
PickupSpec PickupSpec::makeDefault (PickupType t, double position)
{
    PickupSpec s;
    s.type = t;
    s.position = juce::jlimit (0.02, 0.48, position);

    switch (t)
    {
        case PickupType::SingleCoil:
            // L=2.5H, C=200pF, R=6k -> resonance around 7 kHz (engine spec 7.2).
            s.inductanceHenries = 2.5;
            s.capacitancePf = 200.0;
            s.resistanceKOhm = 6.0;
            s.magnet = MagnetType::Alnico5;
            break;

        case PickupType::Humbucker:
            s.inductanceHenries = 6.0;
            s.capacitancePf = 180.0;
            s.resistanceKOhm = 8.0;
            s.coilSpacingMm = 17.5;
            s.magnet = MagnetType::Alnico5;
            s.outputTrimDb = 2.5;
            break;

        case PickupType::P90:
            s.inductanceHenries = 4.0;
            s.capacitancePf = 200.0;
            s.resistanceKOhm = 8.0;
            s.magnet = MagnetType::Alnico2;
            s.outputTrimDb = 1.5;
            break;

        case PickupType::Piezo:
            s.inductanceHenries = 0.0;
            s.capacitancePf = 0.0;
            s.resistanceKOhm = 1.0;
            s.position = 0.0;
            break;

        case PickupType::MagneticSoundhole:
            s.inductanceHenries = 5.0;
            s.capacitancePf = 220.0;
            s.resistanceKOhm = 7.5;
            s.position = 0.30;
            s.magnet = MagnetType::Alnico5;
            break;

        case PickupType::InternalMic:
            s.inductanceHenries = 0.0;
            s.capacitancePf = 0.0;
            s.position = 0.0;
            break;

        case PickupType::NumTypes:
        default:
            break;
    }

    return s;
}

//==============================================================================
void PickupEngine::Coil::reset() noexcept
{
    for (auto& h : history)
        std::fill (h.begin(), h.end(), 0.0);

    writeIndex.fill (0);
    magnetEq.reset();
    coverEq.reset();   // it kept the last render's tail, so the next one differed
}

//==============================================================================
void PickupEngine::prepare (double sampleRate, int strings)
{
    sr = sampleRate;
    numStrings = juce::jlimit (1, kMaxStrings, strings);

    const int combSize = combBufferSizeFor (sr);

    for (int p = 0; p < kMaxPickups; ++p)
    {
        for (int c = 0; c < 2; ++c)
        {
            for (int s = 0; s < kMaxStrings; ++s)
                coils[(size_t) p][(size_t) c].history[(size_t) s].assign ((size_t) combSize, 0.0);
        }

        slotGain[(size_t) p].prepare (sr, constants::kSwitchCrossfadeSeconds);
        slotGain[(size_t) p].snapTo (0.0);
    }

    // Sensible three-pickup default: a Strat.
    specs[0] = PickupSpec::makeDefault (PickupType::SingleCoil, 0.13);
    specs[1] = PickupSpec::makeDefault (PickupType::SingleCoil, 0.25);
    specs[2] = PickupSpec::makeDefault (PickupType::SingleCoil, 0.40);


    blendAmount.prepare (sr, constants::kParamSmoothSeconds);
    piezoMicBlend.prepare (sr, constants::kParamSmoothSeconds);
    humLevel.prepare (sr, constants::kParamSmoothSeconds);

    blendAmount.snapTo (0.5);
    piezoMicBlend.snapTo (0.0);
    humLevel.snapTo (0.0);

    selectorFade.prepare (sr);
    outputDc.prepare (sr, 8.0);

    // Piezo: highpass at 40 Hz, lowpass at 15 kHz, resonance around 3 kHz.
    piezoHp.setHighpass (sr, 40.0, 0.707);
    piezoLp.setLowpass (sr, juce::jmin (15000.0, sr * 0.45), 0.707);
    piezoRes.setPeaking (sr, 3000.0, 1.2, 4.5);

    // Internal mic: a gentle woody tilt, plus the body's low-mid warmth.
    micTilt.setHighShelf (sr, 4000.0, 0.7, -2.5);
    micBody.setPeaking (sr, 250.0, 0.9, 2.0);

    setMainsFrequency (60.0);

    for (int p = 0; p < kMaxPickups; ++p)
    {
        updateCoil (p, 0);
        updateCoil (p, 1);
    }

    updateSelection();
    reset();
}

void PickupEngine::reset() noexcept
{
    for (auto& slot : coils)
        for (auto& c : slot)
            c.reset();

    piezoHp.reset();
    piezoLp.reset();
    piezoRes.reset();
    micTilt.reset();
    micBody.reset();
    outputDc.reset();
    humPhase = 0.0;

    blendAmount.snapToTarget();
    piezoMicBlend.snapToTarget();
    humLevel.snapToTarget();

    for (auto& g : slotGain)
        g.snapToTarget();

    // Finish a selector crossfade too (prepare leaves it idle), so a render
    // after a preset that changed the selector does not open mid-fade.
    selectorFade.prepare (sr);
}

//==============================================================================
void PickupEngine::setNumStrings (int n) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
}

void PickupEngine::setNumPickups (int n) noexcept
{
    numPickups = juce::jlimit (1, kMaxPickups, n);
    updateSelection();
}

void PickupEngine::setPickupSpec (int slot, const PickupSpec& spec) noexcept
{
    if (! juce::isPositiveAndBelow (slot, kMaxPickups))
        return;

    specs[(size_t) slot] = spec;
    updateCoil (slot, 0);
    updateCoil (slot, 1);
}

const PickupSpec& PickupEngine::getPickupSpec (int slot) const noexcept
{
    return specs[(size_t) juce::jlimit (0, kMaxPickups - 1, slot)];
}

void PickupEngine::setSelector (PickupSelector s) noexcept
{
    if (s == selector)
        return;

    selector = s;
    selectorFade.trigger();
    updateSelection();
}

void PickupEngine::setPickupVolume (int slot, double linearGain) noexcept
{
    if (juce::isPositiveAndBelow (slot, kMaxPickups))
    {
        userVolume[(size_t) slot] = juce::jlimit (0.0, 2.0, linearGain);
        updateSelection();
    }
}

void PickupEngine::setBlend (double blend) noexcept
{
    blendAmount.setTarget (juce::jlimit (0.0, 1.0, blend));
}

PickupEngine::SelectedCoil PickupEngine::getSelectedCoil() const noexcept
{
    SelectedCoil result;

    double inverseL = 0.0, inverseR = 0.0;

    for (int slot = 0; slot < juce::jmin (numPickups, kMaxPickups); ++slot)
    {
        if (! slotOn[(size_t) slot])
            continue;

        const auto& s = specs[(size_t) slot];

        if (s.inductanceHenries <= 1.0e-6)
            continue;

        result.hasCoil = true;
        inverseL += 1.0 / s.inductanceHenries;
        inverseR += 1.0 / juce::jmax (100.0, s.resistanceKOhm * 1000.0);
        result.capacitance += s.capacitancePf * 1.0e-12;
    }

    if (result.hasCoil)
    {
        result.inductance = 1.0 / inverseL;
        result.resistance = 1.0 / inverseR;
    }

    return result;
}

void PickupEngine::setHumAmount (double amount) noexcept
{
    // noise-floor.md 3: the advanced range reaches 4.
    humLevel.setTarget (juce::jlimit (0.0, 4.0, amount));
}

void PickupEngine::setMainsFrequency (double hz) noexcept
{
    humIncrement = juce::jlimit (40.0, 70.0, hz) / sr;
}

void PickupEngine::setPiezoMicBlend (double blend) noexcept
{
    piezoMicBlend.setTarget (juce::jlimit (0.0, 1.0, blend));
}

//==============================================================================
void PickupEngine::applyMagnetEq (Biquad& eq, MagnetType m, double sampleRate) noexcept
{
    switch (m)
    {
        case MagnetType::Alnico2:  eq.setPeaking (sampleRate,  800.0, 0.8,  2.0); break;
        case MagnetType::Alnico3:  eq.setPeaking (sampleRate, 1200.0, 0.7, -0.6); break;
        case MagnetType::Alnico5:  eq.setPeaking (sampleRate, 2400.0, 0.8,  1.6); break;
        case MagnetType::Ceramic:  eq.setHighShelf (sampleRate, 3000.0, 0.7, 3.0); break;
        case MagnetType::NumMagnets:
        default:                   eq.setBypass(); break;
    }
}

void PickupEngine::updateCoil (int slot, int coilIndex) noexcept
{
    if (! juce::isPositiveAndBelow (slot, kMaxPickups))
        return;

    const auto& s = specs[(size_t) slot];
    auto& coil = coils[(size_t) slot][(size_t) coilIndex];

    // The coil's LCR resonance is not here any more. It depends on everything
    // the pickup is loaded by - pots, cable, amp - so GuitarCircuit solves it
    // as part of that network (volume-knob-interaction.md 0.1).

    applyMagnetEq (coil.magnetEq, s.magnet, sr);

    // A high shelf at 4 kHz is half its gain there, so the shelf is twice the
    // loss the part states at 4 kHz. The pole pieces tilt the same region.
    coil.coverEq.setHighShelf (sr, juce::jmin (4000.0, sr * 0.45), 0.7,
                               2.0 * s.coverLossDbAt4k + gainToDb (juce::jlimit (0.5, 1.5, s.poleBrightness)));

    // ---- geometry -------------------------------------------------------------
    // The two coils of a humbucker sit either side of the nominal position. Their
    // spacing is expressed as a fraction of a 648 mm string.
    const double spacingFraction = s.coilSpacingMm / 648.0;

    if (s.type == PickupType::Humbucker && ! s.coilTapped)
        coil.positionOffset = (coilIndex == 0 ? -0.5 : 0.5) * spacingFraction;
    else
        coil.positionOffset = 0.0;

    // Pickup height: closer is louder and slightly brighter, because the field
    // gradient the string moves through is steeper.
    const double heightGain = juce::jlimit (0.35, 1.8, 3.2 / juce::jmax (1.0, s.heightMm));

    coil.gain = dbToGain (s.outputTrimDb) * heightGain;

    // A humbucker's two coils are wound in opposite senses; summing them cancels
    // externally-induced hum while the string signal adds.
    if (s.type == PickupType::Humbucker && ! s.coilTapped && coilIndex == 1)
        coil.gain *= (s.reverseWound ? -1.0 : 1.0);
}

void PickupEngine::updateSelection() noexcept
{
    bool on[kMaxPickups] = { false, false, false };

    const int bridge = 0;
    const int middle = (numPickups >= 3) ? 1 : -1;
    const int neck = juce::jmax (0, numPickups - 1);

    auto enable = [&on] (int idx) { if (juce::isPositiveAndBelow (idx, kMaxPickups)) on[idx] = true; };

    switch (selector)
    {
        case PickupSelector::Bridge:       enable (bridge); break;
        case PickupSelector::BridgeMiddle: enable (bridge); enable (middle); break;
        case PickupSelector::Middle:       enable (middle >= 0 ? middle : bridge); break;
        case PickupSelector::MiddleNeck:   enable (middle); enable (neck); break;
        case PickupSelector::Neck:         enable (neck); break;
        case PickupSelector::BridgeNeck:   enable (bridge); enable (neck); break;
        case PickupSelector::All:          for (int i = 0; i < numPickups; ++i) enable (i); break;
        case PickupSelector::NumSelections:
        default:                           enable (bridge); break;
    }

    activeCount = 0;

    for (int i = 0; i < kMaxPickups; ++i)
        if (on[i] && i < numPickups)
            ++activeCount;

    // Parallel pickups sum, so two together would be louder than one. Real guitars
    // are roughly level across switch positions; normalise for that.
    const double norm = (activeCount > 1) ? (1.0 / std::sqrt ((double) activeCount)) : 1.0;

    for (int i = 0; i < kMaxPickups; ++i)
    {
        const bool active = on[i] && i < numPickups;
        slotOn[(size_t) i] = active;
        slotGain[(size_t) i].setTarget (active ? userVolume[(size_t) i] * norm : 0.0);
    }
}

//==============================================================================
double PickupEngine::combSample (Coil& coil, int stringIndex, double input, double delaySamples) noexcept
{
    auto& hist = coil.history[(size_t) stringIndex];

    if (hist.empty())
        return input;

    const int size = (int) hist.size();
    const int mask = size - 1;

    int& widx = coil.writeIndex[(size_t) stringIndex];

    hist[(size_t) widx] = flushDenormal (input);

    // The wave travels to the far end and back before the pickup sees the
    // reflection, so the comb delay is twice the pickup's distance along the
    // string: y[n] = x[n] - x[n - 2*p*D].
    const double combDelay = juce::jlimit (1.0, (double) (size - 2), delaySamples * 2.0);

    const int intDelay = (int) combDelay;
    const double frac = combDelay - (double) intDelay;

    const int i0 = (widx - intDelay) & mask;
    const int i1 = (widx - intDelay - 1) & mask;

    const double delayed = hist[(size_t) i0] * (1.0 - frac) + hist[(size_t) i1] * frac;

    widx = (widx + 1) & mask;

    return input - delayed;
}

//==============================================================================
double PickupEngine::processStrings (const double* stringOutputs,
                                     const double* delaySamples,
                                     int strings) noexcept
{
    if (stringOutputs == nullptr || delaySamples == nullptr)
        return 0.0;

    const int n = juce::jmin (strings, numStrings, kMaxStrings);

    double total = 0.0;

    for (int slot = 0; slot < numPickups; ++slot)
    {
        const double g = slotGain[(size_t) slot].next();

        if (g <= 1.0e-6)
            continue;

        const auto& spec = specs[(size_t) slot];

        // Piezo and internal mic do not sense the magnetic field; they are handled
        // by their own entry points.
        if (spec.type == PickupType::Piezo || spec.type == PickupType::InternalMic)
            continue;

        const bool dualCoil = (spec.type == PickupType::Humbucker && ! spec.coilTapped);
        const int coilCount = dualCoil ? 2 : 1;

        double slotSum = 0.0;

        for (int c = 0; c < coilCount; ++c)
        {
            auto& coil = coils[(size_t) slot][(size_t) c];

            const double pos = juce::jlimit (0.015, 0.49, spec.position + coil.positionOffset);

            double coilSum = 0.0;

            for (int s = 0; s < n; ++s)
                coilSum += combSample (coil, s, stringOutputs[s], delaySamples[s] * pos);

            // The electrical stage runs once per coil on its summed string signal,
            // because a real coil has one winding for all the strings.
            coilSum = coil.magnetEq.process (coilSum);
            coilSum = coil.coverEq.process (coilSum);

            slotSum += coilSum * coil.gain;
        }

        if (dualCoil)
            slotSum *= 0.5;

        total += slotSum * g;
    }

    // ---- mains hum ------------------------------------------------------------
    // Single coils pick up the mains field; a humbucker's reverse-wound second
    // coil cancels it. So the hum is scaled by how much of the active signal is
    // coming from single-coil-style pickups.
    const double hum = humLevel.next();
    lastHum = 0.0;

    if (hum > 1.0e-5)
    {
        const double share = getSingleCoilShare();

        if (share > 0.0)
        {
            humPhase += humIncrement;

            if (humPhase >= 1.0)
                humPhase -= 1.0;

            // Mains hum is not a sine: the third harmonic is always there.
            const double buzz = std::sin (constants::kTwoPi * humPhase)
                                + 0.35 * std::sin (constants::kTwoPi * 3.0 * humPhase)
                                + 0.15 * std::sin (constants::kTwoPi * 5.0 * humPhase);

            double h = buzz * hum * 0.0022 * share;

            // noise-floor.md 2.1: only when not 1, so the legacy hum is bit-identical.
            if (humPositionGain != 1.0)
                h *= humPositionGain;

            lastHum = h;
            total += h;
        }
    }

    // Tone and volume are GuitarCircuit's: they are part of the network the
    // coil is loaded by, not a filter after it.
    total = outputDc.process (total);

    return sanitise (total);
}

//==============================================================================
double PickupEngine::getSingleCoilShare() const noexcept
{
    // Single coils pick up the mains field; a humbucker's reverse-wound second
    // coil cancels it. So the hum is scaled by how much of the active signal is
    // coming from single-coil-style pickups (noise-floor.md 4.1 factors it out).
    double singleCoilShare = 0.0;
    double totalShare = 0.0;

    for (int slot = 0; slot < numPickups; ++slot)
    {
        const double g = slotGain[(size_t) slot].getCurrent();
        totalShare += g;

        const auto& spec = specs[(size_t) slot];
        const bool hums = (spec.type == PickupType::SingleCoil
                           || spec.type == PickupType::P90
                           || spec.type == PickupType::MagneticSoundhole
                           || (spec.type == PickupType::Humbucker && spec.coilTapped));

        if (hums)
            singleCoilShare += g;
    }

    return (totalShare > 1.0e-6 && singleCoilShare > 1.0e-6) ? singleCoilShare / totalShare : 0.0;
}

//==============================================================================
double PickupEngine::processPiezo (double bridgeSignal) noexcept
{
    double x = piezoHp.process (bridgeSignal);
    x = piezoRes.process (x);
    x = piezoLp.process (x);
    return sanitise (x * 0.9);
}

double PickupEngine::processInternalMic (double bodySignal) noexcept
{
    double x = micTilt.process (bodySignal);
    x = micBody.process (x);
    return sanitise (x);
}

//==============================================================================
double PickupEngine::getResonantFrequency (int slot) const noexcept
{
    const auto& s = getPickupSpec (slot);

    if (s.inductanceHenries <= 1.0e-6 || s.capacitancePf <= 1.0e-6)
        return 0.0;

    return 1.0 / (constants::kTwoPi * std::sqrt (s.inductanceHenries * s.capacitancePf * 1.0e-12));
}

double PickupEngine::getResonantQ (int slot) const noexcept
{
    const auto& s = getPickupSpec (slot);

    if (s.inductanceHenries <= 1.0e-6 || s.capacitancePf <= 1.0e-6)
        return 0.0;

    const double R = juce::jmax (100.0, s.resistanceKOhm * 1000.0);
    return (1.0 / R) * std::sqrt (s.inductanceHenries / (s.capacitancePf * 1.0e-12));
}

} // namespace luthier
