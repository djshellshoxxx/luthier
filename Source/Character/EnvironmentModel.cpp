#include "EnvironmentModel.h"

namespace luthier
{

namespace
{
    // 2.3: the sorption isotherm at room temperature.
    constexpr double kRh[]  = { 0.0, 10.0, 20.0, 30.0, 45.0, 55.0, 65.0, 75.0, 85.0, 95.0, 100.0 };
    constexpr double kEmc[] = { 0.0,  2.5,  4.5,  6.0,  8.5, 10.0, 12.0, 14.5, 18.0, 23.0,  27.0 };
    constexpr int kIsothermPoints = 11;

    double finiteOr (double x, double fallback) noexcept { return std::isfinite (x) ? x : fallback; }
}

//==============================================================================
EnvironmentModel::ProfileShape EnvironmentModel::getProfile (EnvProfile p) noexcept
{
    // 3.1's table.
    switch (p)
    {
        case EnvProfile::stageLights:    return { 10.0, -10.0,  600.0, false };
        case EnvProfile::outdoorEvening: return { -8.0,  15.0, 1800.0, false };
        case EnvProfile::coldCase:       return {  0.0,   0.0,    1.0, true  };
        case EnvProfile::airConditioned: return { -4.0, -15.0, 1200.0, false };
        case EnvProfile::humidClub:      return {  6.0,  25.0,  900.0, false };
        case EnvProfile::staticRoom:
        case EnvProfile::numProfiles:
        default:                         return {  0.0,   0.0,    1.0, false };
    }
}

double EnvironmentModel::emc (double rh) noexcept
{
    rh = juce::jlimit (0.0, 100.0, finiteOr (rh, kReferenceRh));

    for (int i = 1; i < kIsothermPoints; ++i)
    {
        if (rh <= kRh[i])
        {
            if (rh == kRh[i])
                return kEmc[i];

            const double u = (rh - kRh[i - 1]) / (kRh[i] - kRh[i - 1]);
            return kEmc[i - 1] + (kEmc[i] - kEmc[i - 1]) * u;
        }
    }

    return kEmc[kIsothermPoints - 1];
}

double EnvironmentModel::topRise (Chambering c) noexcept
{
    switch (c)
    {
        case Chambering::acoustic:   return 0.10;
        case Chambering::hollow:     return 0.06;
        case Chambering::semiHollow: return 0.02;
        case Chambering::chambered:  return 0.005;
        case Chambering::solid:
        case Chambering::numChamberings:
        default:                     return 0.0;
    }
}

double EnvironmentModel::lagResponse (double deltaK, double tp, double tau, double time) noexcept
{
    if (deltaK == 0.0 || time <= 0.0)
        return 0.0;

    tp = juce::jmax (1.0e-6, tp);
    tau = juce::jmax (1.0e-6, tau);

    // The limit form when the two time constants meet.
    if (std::abs (tp - tau) < 1.0e-9 * tau)
        return deltaK * (1.0 - std::exp (-time / tau) * (1.0 + time / tau));

    return deltaK * (1.0 - (tp * std::exp (-time / tp) - tau * std::exp (-time / tau)) / (tp - tau));
}

double EnvironmentModel::thermalCents (double eps, double alphaString, double dTString, double dTNeck) noexcept
{
    if (! (eps > 1.0e-7))
        return 0.0;

    return kCentsPerStrain * (-alphaString * dTString + kAlphaNeck * dTNeck) / eps;
}

double EnvironmentModel::stretchCents (double hN, double hE, double x, double length, double eps) noexcept
{
    if (! (eps > 1.0e-7) || x <= 0.0 || x >= length || hN > 1.0e6 || hE > 1.0e6)
        return 0.0;

    const double geometry = 1.0 / x + 1.0 / (length - x);
    const double deltaL = 0.5 * (hE * hE - hN * hN) * geometry;
    return kCentsPerStrain * deltaL / (length * eps);
}

//==============================================================================
EnvironmentModel::EnvironmentModel()
{
    strain.fill (0.0);
    alpha.fill (kAlphaSteel);
    tauString.fill (5.0);
    autoString.fill (kRoomC);
    refString.fill (kRoomC);
    state.stringTempC.fill (kRoomC);

    for (auto& row : fretTable)
        row.fill (0.0);

    for (int i = 0; i < kMaxStrings; ++i)
    {
        loadedRefString[(size_t) i].store (kRoomC);
        pubStringTemp[(size_t) i].store (kRoomC);
        pubCents[(size_t) i].store (0.0);
    }
}

void EnvironmentModel::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (1.0, sampleRate);
    reset();
}

void EnvironmentModel::reset() noexcept
{
    t = 0.0;
    lastHostSeconds = -1.0;
    lastInputs = inputs;
    firstInputs = false;
    restartAutomation();
    deriveReference();
    computeOutputs();
}

void EnvironmentModel::setNumStrings (int n) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
    geometryInitialised = false;
}

void EnvironmentModel::setStringMaterial (int s, double eps, double alphaString, double diameterMm) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return;

    strain[(size_t) s] = juce::jmax (0.0, finiteOr (eps, 0.0));
    alpha[(size_t) s] = finiteOr (alphaString, kAlphaSteel);
    tauString[(size_t) s] = wireTau (finiteOr (diameterMm, 0.25));
    state.invStrain[(size_t) s] = strain[(size_t) s] > 1.0e-7 ? 1.0 / strain[(size_t) s] : 0.0;
    geometryInitialised = false;
}

void EnvironmentModel::setChambering (Chambering c) noexcept
{
    chambering = c;
}

void EnvironmentModel::setInputs (const Inputs& in) noexcept
{
    inputs = in;
    inputs.temperatureC = juce::jlimit (-60.0, 100.0, finiteOr (in.temperatureC, kRoomC));
    inputs.tunedAtC = juce::jlimit (-60.0, 100.0, finiteOr (in.tunedAtC, kRoomC));
    inputs.humidityPct = juce::jlimit (0.0, 100.0, finiteOr (in.humidityPct, kReferenceRh));

    if (firstInputs)
    {
        // Before the first reset the parts simply are where the inputs say.
        lastInputs = inputs;
        firstInputs = false;
        restartAutomation();
        deriveReference();
        computeOutputs();
    }
}

void EnvironmentModel::requestRetune (double displayedTunedAt) noexcept
{
    retuneDisplay.store (displayedTunedAt);
    retunePending.store (true);
}

//==============================================================================
void EnvironmentModel::restartAutomation() noexcept
{
    // 3.3: after a seek the automation part restarts at its steady state.
    autoString.fill (inputs.temperatureC);
    autoNeck = inputs.temperatureC;
    autoBody = inputs.temperatureC;
}

void EnvironmentModel::deriveReference() noexcept
{
    // 3.2: tuned at env_tuned_at_c; the cold case is tuned on arrival.
    const auto shape = getProfile (inputs.profile);
    const double ref = shape.coldCase ? inputs.temperatureC - kColdCaseKelvin : inputs.tunedAtC;

    refString.fill (ref);
    refNeck = ref;
    lastTunedAt = inputs.tunedAtC;
}

void EnvironmentModel::advance (double seconds, double hostSeconds, bool hostPlaying) noexcept
{
    seconds = juce::jmax (0.0, finiteOr (seconds, 0.0));

    // ---- structural input changes ----------------------------------------------
    if (inputs.profile != lastInputs.profile)
    {
        // A different session is a new scene: parts at steady state, the
        // reference re-derived as on reset. The profile's clock is unchanged,
        // so the host timeline stays bounce-exact.
        restartAutomation();
        deriveReference();
    }
    else if (inputs.tunedAtC != lastTunedAt)
    {
        refString.fill (inputs.tunedAtC);
        refNeck = inputs.tunedAtC;
        lastTunedAt = inputs.tunedAtC;
    }

    lastInputs = inputs;

    if (settlePending.exchange (false))
        restartAutomation();

    if (refLoaded.exchange (false))
    {
        for (int s = 0; s < kMaxStrings; ++s)
            refString[(size_t) s] = loadedRefString[(size_t) s].load();

        refNeck = loadedRefNeck.load();
    }

    // ---- the clock (3.4) --------------------------------------------------------
    if (inputs.clock == EnvClock::hostTimeline && hostPlaying && hostSeconds >= 0.0)
    {
        const double expected = t + seconds;

        // A jump is a seek (or the transport starting somewhere new): the
        // analytic part lands where it belongs and the automation part
        // restarts at steady state, which is the documented behaviour.
        if (lastHostSeconds < 0.0 || std::abs (hostSeconds - expected) > juce::jmax (0.05, 2.0 * seconds))
            restartAutomation();

        t = hostSeconds;
        lastHostSeconds = hostSeconds;
    }
    else
    {
        t += seconds;
        lastHostSeconds = -1.0;
    }

    // ---- automation one-poles (3.3) -------------------------------------------------
    const double target = inputs.temperatureC;

    if (seconds > 0.0)
    {
        for (int s = 0; s < numStrings; ++s)
            autoString[(size_t) s] += (target - autoString[(size_t) s]) * (1.0 - std::exp (-seconds / tauString[(size_t) s]));

        autoNeck += (target - autoNeck) * (1.0 - std::exp (-seconds / kTauNeck));
        autoBody += (target - autoBody) * (1.0 - std::exp (-seconds / kTauBody));
    }

    computeOutputs();

    // ---- Retune (3.2) -------------------------------------------------------------
    if (retunePending.exchange (false))
    {
        for (int s = 0; s < kMaxStrings; ++s)
            refString[(size_t) s] = state.stringTempC[(size_t) s];

        refNeck = state.neckTempC;
        lastTunedAt = retuneDisplay.load();
        computeOutputs();
    }
}

void EnvironmentModel::computeOutputs() noexcept
{
    const auto shape = getProfile (inputs.profile);
    const double cold = shape.coldCase ? -kColdCaseKelvin : 0.0;

    auto part = [&] (double automation, double tau)
    {
        double temp = automation + lagResponse (shape.deltaK, shape.tauSeconds, tau, t);

        if (cold != 0.0)
            temp += cold * std::exp (-t / tau);

        return temp;
    };

    state.neckTempC = part (autoNeck, kTauNeck);
    state.bodyTempC = part (autoBody, kTauBody);
    state.ambientC = inputs.temperatureC
                     + (shape.deltaK != 0.0 ? shape.deltaK * (1.0 - std::exp (-t / shape.tauSeconds)) : 0.0);

    const double dNeck = state.neckTempC - refNeck;
    bool anyCents = false;

    for (int s = 0; s < kMaxStrings; ++s)
    {
        if (s >= numStrings)
        {
            state.stringTempC[(size_t) s] = state.neckTempC;
            state.openCents[(size_t) s] = 0.0;
            continue;
        }

        const double temp = part (autoString[(size_t) s], tauString[(size_t) s]);
        state.stringTempC[(size_t) s] = temp;

        double cents = thermalCents (strain[(size_t) s], alpha[(size_t) s],
                                     temp - refString[(size_t) s], dNeck);
        cents = juce::jlimit (-kMaxCents, kMaxCents, finiteOr (cents, 0.0));
        state.openCents[(size_t) s] = cents;
        anyCents = anyCents || cents != 0.0;
    }

    // ---- moisture (2.3, 2.4) -----------------------------------------------------------
    state.ambientRh = juce::jlimit (0.0, 100.0, inputs.humidityPct
                                                  + (shape.deltaRh != 0.0 ? shape.deltaRh * (1.0 - std::exp (-t / shape.tauSeconds)) : 0.0));
    state.rhAcclimatised = juce::jlimit (0.0, 100.0, inputs.humidityPct
                                                       + lagResponse (shape.deltaRh, shape.tauSeconds, kTauMoisture, t));

    const double dMc = emc (state.rhAcclimatised) - kReferenceEmc;

    state.reliefDeltaMm = 0.015 * dMc;
    state.actionDeltaMm = 0.5 * topRise (chambering) * dMc;

    // ---- body (2.6) -----------------------------------------------------------------------
    state.plateFreqMul = juce::jlimit (0.7, 1.3, (1.0 - 0.008 * dMc) * (1.0 - 0.002 * (state.bodyTempC - kRoomC)));
    state.plateQMul = juce::jlimit (0.5, 2.0, 1.0 / juce::jmax (0.05, 1.0 + 0.04 * dMc));

    // The speed of sound, as a ratio to 22 C, written so that 22 C is exactly 1.
    double air = std::sqrt (juce::jmax (0.01, 1.0 + (state.ambientC - kRoomC) / (273.15 + kRoomC)));

    if (chambering == Chambering::acoustic || chambering == Chambering::hollow)
        air *= 1.0 - 0.002 * dMc;

    state.airFreqMul = juce::jlimit (0.7, 1.3, finiteOr (air, 1.0));
    state.plateFreqMul = finiteOr (state.plateFreqMul, 1.0);
    state.plateQMul = finiteOr (state.plateQMul, 1.0);

    // 2.7: the corrosion hook.
    state.corrosionRate = juce::jlimit (0.5, 3.0, 1.0 + 0.03 * (state.rhAcclimatised - kReferenceRh));

    active = anyCents || dMc != 0.0 || state.plateFreqMul != 1.0 || state.airFreqMul != 1.0;

    // ---- publish ---------------------------------------------------------------------------
    for (int s = 0; s < kMaxStrings; ++s)
    {
        pubStringTemp[(size_t) s].store (state.stringTempC[(size_t) s], std::memory_order_relaxed);
        pubCents[(size_t) s].store (state.openCents[(size_t) s], std::memory_order_relaxed);
    }

    pubNeck.store (state.neckTempC, std::memory_order_relaxed);
    pubBody.store (state.bodyTempC, std::memory_order_relaxed);
    pubRh.store (state.rhAcclimatised, std::memory_order_relaxed);
    pubRelief.store (state.reliefDeltaMm, std::memory_order_relaxed);
    pubAction.store (state.actionDeltaMm, std::memory_order_relaxed);
    pubSerial.fetch_add (1, std::memory_order_relaxed);
}

//==============================================================================
double EnvironmentModel::steadyCentsPerKelvin() const noexcept
{
    double sum = 0.0;
    int n = 0;

    for (int s = 0; s < numStrings; ++s)
    {
        if (strain[(size_t) s] > 1.0e-7)
        {
            sum += thermalCents (strain[(size_t) s], alpha[(size_t) s], 1.0, 1.0);
            ++n;
        }
    }

    return n > 0 ? sum / (double) n : 0.0;
}

//==============================================================================
double EnvironmentModel::fretCents (int s, double fret) const noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings) || ! (fret > 0.0))
        return 0.0;

    const auto& row = fretTable[(size_t) s];
    const double f = juce::jmin (fret, (double) SetupGeometry::kMaxFrets);
    const int lo = (int) std::floor (f);
    const int hi = juce::jmin (lo + 1, SetupGeometry::kMaxFrets);
    const double u = f - (double) lo;

    return row[(size_t) lo] + (row[(size_t) hi] - row[(size_t) lo]) * u;
}

bool EnvironmentModel::updateGeometry (const SetupGeometry& requested, SetupGeometry& effective, bool requestedChanged) noexcept
{
    if (requestedChanged)
    {
        geometryInitialised = false;
        sensitivitiesValid = false;
    }

    const double relief = state.reliefDeltaMm;
    const double action = state.actionDeltaMm;

    const bool moved = std::abs (relief - lastAppliedRelief) > 0.005
                       || std::abs (action - lastAppliedAction) > 0.005;

    if (geometryInitialised && ! moved)
        return false;

    geometryInitialised = true;
    lastAppliedRelief = relief;
    lastAppliedAction = action;

    effective = requested;
    effective.relief += relief;
    effective.actionTreble += action;
    effective.actionBass += action;

    if (! sensitivitiesValid)
    {
        // SetupGeometry's clearance is linear in the relief and the action, so
        // one unit step of each gives its exact sensitivity.
        auto reliefStep = requested, actionStep = requested;
        reliefStep.relief += 1.0;
        actionStep.actionTreble += 1.0;
        actionStep.actionBass += 1.0;
        geometryLength = requested.scaleLengthMm;

        for (int n = 1; n <= SetupGeometry::kMaxFrets; ++n)
        {
            const double x = requested.fretPositionMm ((double) n);
            stretchGeometry[(size_t) n] = (x > 0.0 && x < geometryLength) ? 1.0 / x + 1.0 / (geometryLength - x) : 0.0;
        }

        for (int s = 0; s < kMaxStrings; ++s)
        {
            for (int n = 1; n <= SetupGeometry::kMaxFrets; ++n)
            {
                const double h = requested.clearanceMm (s, 0.0, n);
                hNominal[(size_t) s][(size_t) n] = h;
                dhRelief[(size_t) s][(size_t) n] = reliefStep.clearanceMm (s, 0.0, n) - h;
                dhAction[(size_t) s][(size_t) n] = actionStep.clearanceMm (s, 0.0, n) - h;
            }
        }

        sensitivitiesValid = true;
    }

    for (int s = 0; s < kMaxStrings; ++s)
    {
        auto& row = fretTable[(size_t) s];
        row.fill (0.0);

        if (s >= numStrings || (relief == 0.0 && action == 0.0) || ! (strain[(size_t) s] > 1.0e-7))
            continue;

        const double scale = kCentsPerStrain / (geometryLength * strain[(size_t) s]);

        for (int n = 1; n <= SetupGeometry::kMaxFrets; ++n)
        {
            const double hN = hNominal[(size_t) s][(size_t) n];
            const double hE = hN + relief * dhRelief[(size_t) s][(size_t) n] + action * dhAction[(size_t) s][(size_t) n];
            const double cents = 0.5 * (hE * hE - hN * hN) * stretchGeometry[(size_t) n] * scale;
            row[(size_t) n] = juce::jlimit (-kMaxCents, kMaxCents, finiteOr (cents, 0.0));
        }
    }

    return true;
}

//==============================================================================
juce::var EnvironmentModel::toVar() const
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> refs;

    for (int s = 0; s < kMaxStrings; ++s)
        refs.add (refString[(size_t) s]);

    o->setProperty ("ref_string_c", refs);
    o->setProperty ("ref_neck_c", refNeck);
    return juce::var (o);
}

void EnvironmentModel::fromVar (const juce::var& v)
{
    // Absent: the reference is derived from env_tuned_at_c at the next reset.
    auto* o = v.getDynamicObject();

    if (o == nullptr)
        return;

    if (auto* arr = o->getProperty ("ref_string_c").getArray())
        for (int s = 0; s < juce::jmin (arr->size(), kMaxStrings); ++s)
            loadedRefString[(size_t) s].store (finiteOr ((double) (*arr)[s], kRoomC));

    loadedRefNeck.store (finiteOr ((double) o->getProperty ("ref_neck_c"), kRoomC));
    refLoaded.store (true);
}

} // namespace luthier
