#include "StabilityModel.h"
#include "TuningEngine.h"
#include "../../DSP/Noise/NoiseEngine.h"

namespace luthier
{

namespace
{
    constexpr double kW0 = 5.4;                  // 2.1: about a minute of moderate playing
    constexpr double kSettleCents = 8.0;         // 2.1: C_s for steel
    constexpr double kCreepSeconds = 90.0;       // 2.4.2
    constexpr double kPingSeconds = 0.020;       // 2.2
    constexpr double kBacklashSeconds = 0.030;   // 2.3
    constexpr double kGlideSeconds = 0.250;      // 3
    constexpr double kIdleSeconds = 10.0;        // 3
    constexpr double kSilentLevel = 1.0e-3;      // 3
}

//==============================================================================
double StabilityModel::sigmaForAge (StringAge age) noexcept
{
    switch (age)
    {
        case StringAge::Fresh:    return 1.0;
        case StringAge::BrokenIn: return 0.2;
        case StringAge::Old:
        case StringAge::NumAges:
        default:                  return 0.0;
    }
}

double StabilityModel::settlingMaterialFactor (StringMaterial m) noexcept
{
    switch (m)
    {
        case StringMaterial::SilkAndSteel: return 1.5;
        case StringMaterial::Nylon:
        case StringMaterial::Fluorocarbon: return 4.0;
        default:                           return 1.0;
    }
}

double StabilityModel::backlashCents (const TuningHardware& h, int s) noexcept
{
    // 2.3: theta_b = 0.5 deg x (1 - stability) x (15 / ratio), through a 3 mm
    // post, over the string plus its 120 mm run to the post.
    const double thetaDeg = 0.5 * (1.0 - juce::jlimit (0.0, 1.0, h.tunerStability)) * (15.0 / juce::jmax (1.0, h.tunerRatio));
    const double arcMm = 3.0 * juce::degreesToRadians (thetaDeg);
    const double lTotal = h.scaleLengthMm + 120.0;
    return kStrainCents * h.eaOverT[(size_t) juce::jlimit (0, kMaxStrings - 1, s)] * arcMm / lTotal;
}

double StabilityModel::capoBiasCents (const TuningHardware& h, int s, int capoFret) noexcept
{
    if (capoFret <= 0)
        return 0.0;

    // 2.6: dL = h^2 / 2g, h = 0.5 x pressure x fret height.
    const double height = 0.5 * juce::jlimit (0.0, 1.0, h.capoPressure) * h.fretHeightMm;
    const double dL = height * height / (2.0 * juce::jmax (0.5, h.capoGapMm));
    const double lc = h.scaleLengthMm * std::pow (2.0, -capoFret / 12.0);
    return kStrainCents * h.eaOverT[(size_t) juce::jlimit (0, kMaxStrings - 1, s)] * dL / lc;
}

double StabilityModel::floatingK (const TuningHardware& h) noexcept
{
    using B = WhammyEngine::BridgeType;

    switch (h.bridge)
    {
        case B::FloydRose:   return 3.0;
        case B::VintageTrem: return 2.0;
        case B::Bigsby:      return 1.0;
        default:             return 0.0;   // TransTrem holds its intervals by design
    }
}

double StabilityModel::creepK (const TuningHardware& h) noexcept
{
    using B = WhammyEngine::BridgeType;

    if (h.acoustic)
        return 0.2;

    switch (h.bridge)
    {
        case B::VintageTrem: return 0.3;
        case B::FloydRose:   return 0.2;
        case B::Bigsby:      return 0.4;
        case B::TransTrem:   return 0.1;
        default:             return 0.05;   // hardtail
    }
}

double StabilityModel::returnK (const TuningHardware& h) noexcept
{
    using B = WhammyEngine::BridgeType;

    switch (h.bridge)
    {
        case B::VintageTrem: return 0.8;
        case B::Bigsby:      return 0.6;
        case B::FloydRose:   return 0.15;
        case B::TransTrem:   return 0.3;
        default:             return 0.0;
    }
}

//==============================================================================
void StabilityModel::reset() noexcept
{
    for (auto& st : strings)
    {
        const double committed = st.sigmaCommitted, comp = st.capoComp;
        st = StringState {};
        st.sigma = st.sigmaCommitted = committed;
        st.capoComp = comp;
        st.written = std::numeric_limits<double>::quiet_NaN();   // write on the next advance
    }

    excursionIndex = 0;
    whammyPeak = 0.0;
    whammyActive = false;
    idleSeconds = 0.0;
    idleFired = false;
    lastPlaying = false;
}

void StabilityModel::setStringAge (StringAge age) noexcept
{
    const double sigma = sigmaForAge (age);

    for (auto& st : strings)
    {
        st.sigma = st.sigmaCommitted = sigma;
        st.w = 0.0;
    }
}

void StabilityModel::beginPresetLoad (StringAge age) noexcept
{
    suppressTuningEvents.store (true, std::memory_order_release);
    pendingMask.store (0, std::memory_order_release);

    // Sigma and capoComp now, so a session restore right after (setState
    // loads the preset, then its "stability" block) wins; the offsets clear
    // at the next block, on the audio thread.
    const double sigma = sigmaForAge (age);

    for (auto& st : strings)
    {
        st.sigma = st.sigmaCommitted = sigma;
        st.capoComp = 0.0;
    }

    pendingPresetAge.store ((int) age, std::memory_order_release);
}

//==============================================================================
void StabilityModel::onTuningChanged (int s, double oldHz, double newHz) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings) || oldHz <= 0.0 || newHz <= 0.0
        || suppressTuningEvents.load (std::memory_order_acquire))
        return;

    const juce::uint32 bit = 1u << s;

    // The first old pitch of a burst is kept, the latest new one wins.
    if ((pendingMask.load (std::memory_order_acquire) & bit) == 0)
        pendingOld[(size_t) s].store (oldHz, std::memory_order_relaxed);

    pendingNew[(size_t) s].store (newHz, std::memory_order_relaxed);
    pendingMask.fetch_or (bit, std::memory_order_release);
}

void StabilityModel::onPluck (int s, double velocity) noexcept
{
    if (settings.amount <= 0.0 || ! juce::isPositiveAndBelow (s, kMaxStrings))
        return;

    auto& st = strings[(size_t) s];
    const auto index = ++st.pluckIndex;
    const double v = juce::jlimit (0.0, 1.0, velocity);

    // 2.2: the ping. A later pluck lets the bind go with p = 0.25 v (1 - mu).
    if (std::abs (st.stuck) > 1.0e-6 && st.stuckRampLeft <= 0.0)
    {
        const double p = 0.25 * v * (1.0 - hw.effectiveNutFriction());

        if (noiseUniform (seed, (juce::uint32) s * 65536u + index) < p)
        {
            st.stuckRampLeft = kPingSeconds;
            st.pingAtPluck = (int) index;
        }
    }

    // 2.3: a hard pluck is a tension excursion.
    if (st.backlashArmed && v >= 0.9)
        startBacklash (s);
}

void StabilityModel::startBacklash (int s) noexcept
{
    auto& st = strings[(size_t) s];
    st.backlashArmed = false;
    st.backlashTarget = st.backlash - backlashCents (hw, s);
    st.backlashRampLeft = kBacklashSeconds;
}

double StabilityModel::totalTension() const noexcept
{
    double t = 0.0;

    for (int s = 0; s < hw.numStrings; ++s)
        t += hw.tensionN[(size_t) s];

    return juce::jmax (1.0, t);
}

void StabilityModel::applyTuningChange (int s, double oldHz, double newHz) noexcept
{
    if (std::abs (newHz / oldHz - 1.0) < 1.0e-9)
        return;

    auto& st = strings[(size_t) s];

    // 2.3: brought to pitch from above - the gear is not loaded.
    if (newHz < oldHz)
        st.backlashArmed = true;

    const double tNew = juce::jmax (1.0, hw.tensionN[(size_t) s]);
    const double tOld = tNew * (oldHz / newHz) * (oldHz / newHz);
    const double dT = tNew - tOld;

    // 2.4.1: the springs balance the total tension; every other string moves.
    const double kf = floatingK (hw);

    if (kf > 0.0)
        for (int i = 0; i < hw.numStrings; ++i)
            if (i != s)
                strings[(size_t) i].equil += -kf * 100.0 * dT / totalTension();

    // 2.4.2: and this string creeps toward its new equilibrium.
    st.creepTarget = -creepK (hw) * 100.0 * std::abs (dT) / tNew;
}

double StabilityModel::scaledTotal (int s, const int* capoFret) const noexcept
{
    const auto& st = strings[(size_t) s];
    const auto& k = settings;

    const double settleC = -kSettleCents * settlingMaterialFactor (hw.material) * st.sigma * (1.0 - std::exp (-st.w / kW0));
    const int capo = capoFret != nullptr ? capoFret[s] : 0;
    const double bias = capoBiasCents (hw, s, capo);

    const double sum = k.amount * (k.settling * settleC
                                   + k.nutBinding * st.stuck
                                   + k.backlash * st.backlash
                                   + k.saddleCreep * (st.creep + st.equil + st.returnErr)
                                   + k.bendMemory * st.bendMem
                                   + k.capoBias * bias)
                     + st.capoComp;

    const double limit = k.isAdvanced() ? 200.0 : 50.0;
    return juce::jlimit (-limit, limit, sum);
}

void StabilityModel::retuneString (int s, bool transportPlaying, const int* capoFret) noexcept
{
    auto& st = strings[(size_t) s];
    const double heard = scaledTotal (s, capoFret) + st.glide;

    // 3: on a floating bridge the correction itself moves every other string
    // (2.4.1): the retune changes this string's tension by the offset heard.
    const double kf = floatingK (hw);
    const double bridgeScale = settings.amount * settings.saddleCreep;

    if (kf > 0.0 && bridgeScale > 0.0)
    {
        const double t = juce::jmax (1.0, hw.tensionN[(size_t) s]);
        const double dT = t * (std::pow (2.0, -heard / 600.0) - 1.0);

        for (int i = 0; i < hw.numStrings; ++i)
            if (i != s)
                strings[(size_t) i].equil += (-kf * 100.0 * dT / totalTension()) / bridgeScale;
    }

    // 2.1: the stretch already taken out stays out. Committed only while the
    // host is stopped, so playback renders always start from the same sigma (5).
    st.sigma *= std::exp (-st.w / kW0);
    st.w = 0.0;

    if (! transportPlaying)
        st.sigmaCommitted = st.sigma;

    st.stuck = st.stuckRampLeft = 0.0;
    st.backlash = st.backlashTarget = st.backlashRampLeft = 0.0;
    st.backlashArmed = false;       // a retune always comes up from below
    st.bendMem = 0.0;
    st.creep = st.creepTarget = 0.0;
    st.returnErr = 0.0;
    st.equil = 0.0;

    // 2.6: tuned with the capo on, the string is tuned to its bias.
    st.capoComp = 0.0;
    const int capo = capoFret != nullptr ? capoFret[s] : 0;

    if (capo > 0)
        st.capoComp = -settings.amount * settings.capoBias * capoBiasCents (hw, s, capo);

    // 3: a ringing string moves to pitch over 250 ms; a silent one snaps.
    const double after = scaledTotal (s, capoFret);

    if (st.lastLevel > kSilentLevel)
    {
        st.glide = heard - after;
        st.glideRemaining = kGlideSeconds;
    }
    else
    {
        st.glide = st.glideRemaining = 0.0;
    }
}

//==============================================================================
StabilityModel::BlockOutput StabilityModel::advance (const BlockInput& in, TuningEngine& tuning) noexcept
{
    BlockOutput out;
    const int n = juce::jlimit (1, kMaxStrings, hw.numStrings);
    blockCount.fetch_add (1, std::memory_order_relaxed);

    // ---- a preset load (7) ------------------------------------------------------
    {
        const int age = pendingPresetAge.exchange (-1, std::memory_order_acq_rel);

        if (age >= 0)
            reset();
    }

    // ---- the retune requests (3) ------------------------------------------------
    const juce::uint32 mask = retuneMask.exchange (0, std::memory_order_acq_rel);
    const bool all = retuneAll.exchange (false, std::memory_order_acq_rel);

    // ---- tuning-change events (5) ------------------------------------------------
    const juce::uint32 changed = pendingMask.exchange (0, std::memory_order_acq_rel);

    if (settings.amount <= 0.0)
    {
        // 5: amount 0 returns at once; stabilityCents is exactly 0.0. The
        // retune actions still reach the character engine and the drift.
        for (int s = 0; s < kMaxStrings; ++s)
        {
            auto& st = strings[(size_t) s];

            if (st.written != 0.0)
            {
                tuning.setStabilityCents (s, 0.0);
                st.written = 0.0;
            }

            for (auto& r : readout[(size_t) s])
                r.store (0.0f, std::memory_order_relaxed);
        }

        out.retunedAll = all;
        out.retunedMask = all ? (juce::uint32) ((1u << n) - 1) : mask;
        lastPlaying = in.transportPlaying;
        return out;
    }

    for (int s = 0; s < n; ++s)
        if ((changed >> s) & 1u)
            applyTuningChange (s, pendingOld[(size_t) s].load (std::memory_order_relaxed),
                               pendingNew[(size_t) s].load (std::memory_order_relaxed));

    const double dt = (double) juce::jmax (0, in.numSamples) / sr;
    const bool advanced = settings.isAdvanced();
    const double capScale = advanced ? 4.0 : 1.0;
    const double mu = hw.effectiveNutFriction();

    // ---- per string ---------------------------------------------------------------
    bool allSilent = true;

    for (int s = 0; s < n; ++s)
    {
        auto& st = strings[(size_t) s];
        const double level = in.levels != nullptr ? in.levels[s] : 0.0;
        st.lastLevel = level;
        allSilent = allSilent && level < kSilentLevel;

        // 2.1: playing time wears the strings in.
        st.w += level * level * dt;

        // 2.2, 2.5: bends, by their peak and release.
        const double bend = in.bendCents != nullptr ? in.bendCents[s] : 0.0;

        if (! st.bendActive && bend >= 20.0)
        {
            st.bendActive = true;
            st.bendPeak = bend;
        }

        if (st.bendActive)
        {
            st.bendPeak = juce::jmax (st.bendPeak, bend);

            if (st.backlashArmed && bend >= 50.0)
                startBacklash (s);

            if (bend < 0.25 * st.bendPeak)
            {
                st.bendActive = false;
                const double b = st.bendPeak;

                st.stuck = juce::jlimit (-6.0 * capScale, 6.0 * capScale, st.stuck + 0.03 * mu * b);
                st.stuckRampLeft = 0.0;
                st.w += 0.5 * (b / 100.0) * (b / 100.0);

                if (b >= 50.0)
                    st.bendMem = juce::jmax (-10.0 * capScale,
                                             st.bendMem - 0.04 * (b / 100.0) * (b / 100.0) * (1.0 + 2.0 * st.sigma)
                                                            * (hw.tunerLocking ? 0.3 : 1.0));
            }
        }

        // 2.2: a released bind ramps out over 20 ms.
        if (st.stuckRampLeft > 0.0)
        {
            st.stuck -= st.stuck * juce::jmin (1.0, dt / st.stuckRampLeft);
            st.stuckRampLeft -= dt;

            if (st.stuckRampLeft <= 0.0)
                st.stuck = st.stuckRampLeft = 0.0;
        }

        // 2.3: backlash gives way over 30 ms.
        if (st.backlashRampLeft > 0.0)
        {
            st.backlash += (st.backlashTarget - st.backlash) * juce::jmin (1.0, dt / st.backlashRampLeft);
            st.backlashRampLeft -= dt;

            if (st.backlashRampLeft <= 0.0)
                st.backlash = st.backlashTarget, st.backlashRampLeft = 0.0;
        }

        // 2.4.2: creep, tau 90 s.
        st.creep += (st.creepTarget - st.creep) * (1.0 - std::exp (-dt / kCreepSeconds));

        // 3: the retune glide.
        if (st.glideRemaining > 0.0)
        {
            st.glide -= st.glide * juce::jmin (1.0, dt / st.glideRemaining);
            st.glideRemaining -= dt;

            if (st.glideRemaining <= 0.0)
                st.glide = st.glideRemaining = 0.0;
        }
    }

    // ---- the whammy: dives, pull-ups and returns (2.2, 2.3, 2.4.3) ------------------
    {
        double w = 0.0;

        if (in.whammyCents != nullptr)
            for (int s = 0; s < n; ++s)
                if (std::abs (in.whammyCents[s]) > std::abs (w))
                    w = in.whammyCents[s];

        if (w >= 50.0)
            for (int s = 0; s < n; ++s)
                if (strings[(size_t) s].backlashArmed)
                    startBacklash (s);

        if (! whammyActive && std::abs (w) >= 100.0)
        {
            whammyActive = true;
            whammyPeak = w;
        }

        if (whammyActive)
        {
            if (std::abs (w) > std::abs (whammyPeak))
                whammyPeak = w;

            if (std::abs (w) < 5.0)
            {
                whammyActive = false;
                const double sign = noiseUniform (seed ^ 0x3E7u, ++excursionIndex) < 0.5 ? -1.0 : 1.0;
                const double err = sign * returnK (hw) * std::abs (whammyPeak) / 100.0;

                for (int s = 0; s < n; ++s)
                {
                    auto& st = strings[(size_t) s];
                    st.returnErr = juce::jlimit (-8.0 * capScale, 8.0 * capScale, st.returnErr + err);

                    // A dive drags the string back through the nut, flat.
                    if (whammyPeak < 0.0)
                        st.stuck = juce::jlimit (-6.0 * capScale, 6.0 * capScale,
                                                 st.stuck - 0.015 * mu * std::abs (whammyPeak));
                }
            }
        }
    }

    // ---- auto-retune (3) ---------------------------------------------------------------
    {
        bool anyOffset = false;

        for (int s = 0; s < n && ! anyOffset; ++s)
            anyOffset = std::abs (scaledTotal (s, in.capoFret)) > 1.0e-3;

        const auto mode = settings.autoRetune;
        bool fire = false;

        if (allSilent)
            idleSeconds += dt;
        else
            idleSeconds = 0.0, idleFired = false;

        if ((mode == AutoRetune::idle || mode == AutoRetune::idleAndStop)
            && idleSeconds >= kIdleSeconds && ! idleFired && anyOffset)
        {
            fire = true;
            idleFired = true;
        }

        if ((mode == AutoRetune::transportStop || mode == AutoRetune::idleAndStop)
            && lastPlaying && ! in.transportPlaying && anyOffset)
            fire = true;

        lastPlaying = in.transportPlaying;

        if (fire)
        {
            ++autoRetunes;
            retuneAll.store (true, std::memory_order_release);
        }
    }

    const bool allNow = all || retuneAll.exchange (false, std::memory_order_acq_rel);

    // ---- retune (3) -------------------------------------------------------------------------
    if (allNow)
    {
        // Lowest to highest pitch, each correction feeding the rest.
        std::array<int, kMaxStrings> order {};
        for (int s = 0; s < n; ++s) order[(size_t) s] = s;

        std::sort (order.begin(), order.begin() + n,
                   [this] (int a, int b) { return hw.openHz[(size_t) a] < hw.openHz[(size_t) b]; });

        for (int k = 0; k < n; ++k)
            retuneString (order[(size_t) k], in.transportPlaying, in.capoFret);

        out.retunedAll = true;
        out.retunedMask = (juce::uint32) ((1u << n) - 1);
    }
    else if (mask != 0)
    {
        for (int s = 0; s < n; ++s)
            if ((mask >> s) & 1u)
                retuneString (s, in.transportPlaying, in.capoFret);

        out.retunedMask = mask;
    }

    // ---- write (5) and the readouts ---------------------------------------------------------
    const auto& k = settings;

    for (int s = 0; s < kMaxStrings; ++s)
    {
        auto& st = strings[(size_t) s];
        const double total = s < n ? scaledTotal (s, in.capoFret) + st.glide : 0.0;

        if (! (std::abs (total - st.written) <= 1.0e-4))
        {
            tuning.setStabilityCents (s, total);
            st.written = total;
        }

        auto& r = readout[(size_t) s];
        const int capoHere = in.capoFret != nullptr && s < n ? in.capoFret[s] : 0;
        r[settle].store ((float) (k.amount * k.settling * -kSettleCents * settlingMaterialFactor (hw.material)
                                  * st.sigma * (1.0 - std::exp (-st.w / kW0))), std::memory_order_relaxed);
        r[nut].store ((float) (k.amount * k.nutBinding * st.stuck), std::memory_order_relaxed);
        r[backlash].store ((float) (k.amount * k.backlash * st.backlash), std::memory_order_relaxed);
        r[bridge].store ((float) (k.amount * k.saddleCreep * (st.creep + st.equil + st.returnErr)), std::memory_order_relaxed);
        r[memory].store ((float) (k.amount * k.bendMemory * st.bendMem), std::memory_order_relaxed);
        r[capo].store ((float) (k.amount * k.capoBias * capoBiasCents (hw, s, capoHere) + st.capoComp), std::memory_order_relaxed);
        r[numCauses].store ((float) total, std::memory_order_relaxed);
    }

    return out;
}

//==============================================================================
double StabilityModel::getTotalCents (int s) const noexcept
{
    return readout[(size_t) juce::jlimit (0, kMaxStrings - 1, s)][numCauses].load (std::memory_order_relaxed);
}

double StabilityModel::getCauseCents (int s, Cause c) const noexcept
{
    return readout[(size_t) juce::jlimit (0, kMaxStrings - 1, s)][(size_t) juce::jlimit (0, (int) numCauses - 1, (int) c)]
             .load (std::memory_order_relaxed);
}

//==============================================================================
juce::var StabilityModel::toVar() const
{
    juce::Array<juce::var> sigma, comp;

    // 17 significant digits as text: JSON's number formatting rounds, and
    // TS-15 asks for an exact round trip.
    auto exact = [] (double v) { return juce::var (juce::String::formatted ("%.17g", v)); };

    for (const auto& st : strings)
    {
        sigma.add (exact (st.sigmaCommitted));
        comp.add (exact (st.capoComp));
    }

    auto* object = new juce::DynamicObject();
    object->setProperty ("sigma", sigma);
    object->setProperty ("capo_comp", comp);
    return juce::var (object);
}

void StabilityModel::fromVar (const juce::var& v)
{
    if (auto* sigma = v.getProperty ("sigma", {}).getArray())
        for (int s = 0; s < juce::jmin (kMaxStrings, sigma->size()); ++s)
            strings[(size_t) s].sigma = strings[(size_t) s].sigmaCommitted = juce::jlimit (0.0, 1.0, (double) (*sigma)[s]);

    if (auto* comp = v.getProperty ("capo_comp", {}).getArray())
        for (int s = 0; s < juce::jmin (kMaxStrings, comp->size()); ++s)
            strings[(size_t) s].capoComp = juce::jlimit (-200.0, 200.0, (double) (*comp)[s]);
}

} // namespace luthier
