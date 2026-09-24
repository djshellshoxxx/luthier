#include "StringAging.h"

namespace luthier
{

namespace
{
    /*  The old `kAgeEffects` rows (StringMaterials.cpp), which are the legacy
        anchors of section 3.3, plus PlayingNoise's roughness table (1.0 / 1.15
        / 1.4) for 3.5. Columns: sustain, brightness, detune, roughness, squeak. */
    constexpr double kAnchorHours[3] = { 0.0, 12.0, 120.0 };
    constexpr double kAnchors[5][3] =
    {
        { 1.00, 0.92, 0.66 },    // sustain
        { 1.00, 0.80, 0.48 },    // brightness
        { 0.00, 1.50, 5.00 },    // detune, cents
        { 1.00, 1.15, 1.40 },    // squeak roughness (string-squeak.md 10)
        { 1.20, 1.00, 0.62 }     // StringSpec.squeak's old age factor
    };

    constexpr double kSmoothSeconds = 0.2;   // 9: the hours glide

    double lerp (double a, double b, double t) noexcept { return a + (b - a) * t; }

    double finite (double x) noexcept { return std::isfinite (x) ? x : 0.0; }

    double saturate (double h, double tau) noexcept { return 1.0 - std::exp (-juce::jmax (0.0, h) / tau); }
}

//==============================================================================
StringAging::StringAging()
{
    for (int i = 0; i < kMaxStrings; ++i)
    {
        baseHours[(size_t) i].store (0.0);
        accruedHours[(size_t) i].store (0.0);
        publishedHours[(size_t) i].store (0.0);
        publishedBright[(size_t) i].store (1.0);
    }
}

void StringAging::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (1.0, sampleRate);
    reset();
}

void StringAging::reset() noexcept
{
    smoothedHours = target.hours;
    recompute();

    // Recomputed, but not yet pushed to the strings: the next advance does.
    dirty = true;
}

void StringAging::setNumStrings (int n) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
    dirty = true;
}

void StringAging::setStringInfo (int s, bool isWound, bool isCoatedMaterial) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return;

    if (wound[(size_t) s] != isWound || coatedMaterial[(size_t) s] != isCoatedMaterial)
        dirty = true;

    wound[(size_t) s] = isWound;
    coatedMaterial[(size_t) s] = isCoatedMaterial;
}

void StringAging::setJitter (int s, double bipolar) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return;

    const double j = juce::jlimit (-1.0, 1.0, bipolar);

    if (jitter[(size_t) s] != j)
        dirty = true;

    jitter[(size_t) s] = j;
}

void StringAging::setInputs (const Inputs& in) noexcept
{
    const double kRH = target.kRH;
    target = in;
    target.kRH = kRH;
    target.hours = juce::jlimit (0.0, 5000.0, std::isfinite (in.hours) ? in.hours : 0.0);
    target.corrosivity = juce::jlimit (0.0, 10.0, std::isfinite (in.corrosivity) ? in.corrosivity : 1.0);
    target.detail = juce::jlimit (0.0, 1.0, std::isfinite (in.detail) ? in.detail : 1.0);
}

//==============================================================================
double StringAging::coatingRate (StringCoating c) noexcept
{
    switch (c)
    {
        case StringCoating::thin:  return 0.33;
        case StringCoating::thick: return 0.25;
        case StringCoating::none:
        case StringCoating::numCoatings:
        default:                   return 1.0;
    }
}

double StringAging::coatingBrightness (StringCoating c) noexcept
{
    switch (c)
    {
        case StringCoating::thin:  return 0.97;
        case StringCoating::thick: return 0.92;
        case StringCoating::none:
        case StringCoating::numCoatings:
        default:                   return 1.0;
    }
}

double StringAging::legacy (int column, double hours) noexcept
{
    const auto& row = kAnchors[juce::jlimit (0, 4, column)];
    const double h = juce::jmax (0.0, hours);

    if (h >= kAnchorHours[2])
        return row[2];

    if (h >= kAnchorHours[1])
        return lerp (row[1], row[2], (h - kAnchorHours[1]) / (kAnchorHours[2] - kAnchorHours[1]));

    return lerp (row[0], row[1], h / kAnchorHours[1]);
}

double StringAging::detuneSign (int stringIndex) noexcept
{
    RtRandom r { 0xA6E0000ull + (uint64_t) juce::jmax (0, stringIndex) };
    return r.nextBipolar();
}

void StringAging::weightsFor (bool isWound, double j, double detail, double& wc, double& wk) noexcept
{
    // 3.1: per-string weights, scaled by the detail.
    const double d = juce::jlimit (0.0, 1.0, detail);
    const double jit = 1.0 + 0.10 * d * juce::jlimit (-1.0, 1.0, j);
    wc = lerp (1.0, isWound ? 1.4 : 0.6, d) * jit;
    wk = lerp (1.0, isWound ? 0.85 : 1.15, d) * jit;
}

AgingFactors StringAging::compute (double H, bool isWound, bool isCoatedMaterial, double j,
                                   int stringIndex, const Inputs& in) noexcept
{
    double wc = 1.0, wk = 1.0;
    weightsFor (isWound, j, in.detail, wc, wk);
    return computeWithWeights (H, wc, wk, isCoatedMaterial, stringIndex, in);
}

AgingFactors StringAging::computeWithWeights (double H, double wc, double wk, bool isCoatedMaterial,
                                              int stringIndex, const Inputs& in) noexcept
{
    AgingFactors f;

    H = juce::jmax (0.0, std::isfinite (H) ? H : 0.0);
    const double d = juce::jlimit (0.0, 1.0, in.detail);
    const double rho = juce::jmax (0.0, in.corrosivity);

    // 3.2: the coated material is at least Thin, and its darker table already
    // carries the fresh brightness.
    auto coating = in.coating;

    if (isCoatedMaterial && coating == StringCoating::none)
        coating = StringCoating::thin;

    const double rc = coatingRate (coating);
    const double b0 = isCoatedMaterial ? 1.0 : coatingBrightness (coating);

    const double hc = H * wc * rc * (0.5 + 0.5 * rho);
    const double hk = H * wk * rc * rho * in.kRH;
    const double hf = H;

    f.hours = H;
    f.contaminationHours = hc;
    f.corrosionHours = hk;
    f.fatigueHours = hf;

    // 3.2: the physical curves.
    const double bPhys = b0 * (1.0 - kBrightDrop * saturate (hc, kBrightTau));
    const double sPhys = 1.0 - kSustainDrop * saturate (0.5 * (hf + hk), kSustainTau);
    const double dPhys = kDetuneMax * saturate ((hc + hk + hf) / 3.0, kDetuneTau);
    const double pPhys = 1.0 + kDispersionRise * saturate (hk, kDispersionTau);
    const double cN = saturate (hc, kBrightTau) / saturate (kAnchorHours[2], kBrightTau);

    // 3.3: the blend. d = 0 is the legacy table exactly.
    f.brightness = lerp (legacy (1, H), bPhys, d);
    f.sustain = lerp (legacy (0, H), sPhys, d);
    f.dispersion = lerp (1.0, pPhys, d);

    const double detune = lerp (legacy (2, H), dPhys, d);
    f.detuneCents = detune * detuneSign (stringIndex);
    f.intonationCentsPerFret = d * detune * 0.5 / 12.0;

    // 3.5: grime is a louder, duller squeak; fresh edges a brighter one.
    f.roughness = lerp (legacy (3, H), 1.0 + 0.4 * cN, d);
    f.squeakCentroid = 1.0 - 0.30 * d * cN;

    // The in-loop slide noise keeps the old factor at d = 0; the physical model
    // leaves squeak to PlayingNoise, so at d = 1 it is the Broken In level.
    // The Fresh spec already carries 1.2, which is divided out.
    f.squeakScale = lerp (legacy (4, H), 1.0, d) / kAnchors[4][0];

    f.brightness = juce::jlimit (0.05, 2.0, finite (f.brightness));
    f.sustain = juce::jlimit (0.05, 2.0, finite (f.sustain));
    f.dispersion = juce::jlimit (0.5, 2.0, finite (f.dispersion));
    f.detuneCents = juce::jlimit (-50.0, 50.0, finite (f.detuneCents));
    f.intonationCentsPerFret = juce::jlimit (0.0, 5.0, finite (f.intonationCentsPerFret));
    return f;
}

AgingFactors StringAging::computeNow (int s) const noexcept
{
    s = juce::jlimit (0, kMaxStrings - 1, s);
    const double H = juce::jmax (0.0, target.hours - baseHours[(size_t) s].load (std::memory_order_relaxed))
                     + accruedHours[(size_t) s].load (std::memory_order_relaxed);
    return compute (H, wound[(size_t) s], coatedMaterial[(size_t) s], jitter[(size_t) s], s, target);
}

//==============================================================================
void StringAging::requestRestring (int stringIndex) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        restringMask.fetch_or (1u << (juce::uint32) stringIndex);
}

void StringAging::requestRestringAll() noexcept
{
    restringMask.fetch_or (0x80000000u);
}

void StringAging::applyRestrings() noexcept
{
    const auto mask = restringMask.exchange (0);

    if (mask == 0)
        return;

    for (int s = 0; s < kMaxStrings; ++s)
    {
        if ((mask & 0x80000000u) != 0)
        {
            // Restring all: the set's hours go to 0 as a parameter gesture; the
            // per-string state clears here.
            baseHours[(size_t) s].store (0.0, std::memory_order_relaxed);
            accruedHours[(size_t) s].store (0.0, std::memory_order_relaxed);
        }
        else if ((mask & (1u << (juce::uint32) s)) != 0)
        {
            // Only this string is new: it is as old as the set was when it went on.
            baseHours[(size_t) s].store (target.hours, std::memory_order_relaxed);
            accruedHours[(size_t) s].store (0.0, std::memory_order_relaxed);
        }
    }

    dirty = true;
}

bool StringAging::advance (double seconds, const double* levels, int numLevels) noexcept
{
    applyRestrings();

    if (stateLoaded.exchange (false))
        dirty = true;

    seconds = juce::jmax (0.0, seconds);

    // 9: a 200 ms glide on the hours, at block rate.
    const double a = 1.0 - std::exp (-seconds / kSmoothSeconds);
    const double before = smoothedHours;
    smoothedHours += (target.hours - smoothedHours) * a;

    if (std::abs (target.hours - smoothedHours) < 1.0e-6)
        smoothedHours = target.hours;

    // 6: played time, not wall-clock time.
    double rate = 0.0;

    switch (target.accrual)
    {
        case AgeAccrual::realTime: rate = 1.0; break;
        case AgeAccrual::x10:      rate = 10.0; break;
        case AgeAccrual::x100:     rate = 100.0; break;
        case AgeAccrual::off:
        case AgeAccrual::numRates:
        default:                   break;
    }

    bool accrued = false;

    if (rate > 0.0 && levels != nullptr)
    {
        const double add = seconds * rate / 3600.0;

        for (int s = 0; s < juce::jmin (numStrings, numLevels); ++s)
        {
            if (levels[s] > kLevelThreshold)
            {
                auto& acc = accruedHours[(size_t) s];
                acc.store (acc.load (std::memory_order_relaxed) + add, std::memory_order_relaxed);
                accrued = true;
            }
        }
    }

    const bool inputsMoved = ! (target.corrosivity == applied.corrosivity && target.detail == applied.detail
                                && target.coating == applied.coating && target.kRH == applied.kRH);
    const bool hoursMoved = std::abs (smoothedHours - applied.hours) > 1.0e-4
                            || (smoothedHours != before && smoothedHours == target.hours);

    if (! (dirty || inputsMoved || hoursMoved || accrued))
        return false;

    recompute();
    return true;
}

void StringAging::recompute() noexcept
{
    applied = target;
    applied.hours = smoothedHours;
    dirty = false;

    for (int s = 0; s < kMaxStrings; ++s)
    {
        const double H = juce::jmax (0.0, smoothedHours - baseHours[(size_t) s].load (std::memory_order_relaxed))
                         + accruedHours[(size_t) s].load (std::memory_order_relaxed);

        factors[(size_t) s] = compute (H, wound[(size_t) s], coatedMaterial[(size_t) s],
                                       jitter[(size_t) s], s, applied);

        publishedHours[(size_t) s].store (H, std::memory_order_relaxed);
        publishedBright[(size_t) s].store (factors[(size_t) s].brightness, std::memory_order_relaxed);
    }

    publishSerial.fetch_add (1, std::memory_order_relaxed);
}

//==============================================================================
juce::var StringAging::toVar() const
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> base, acc;

    for (int s = 0; s < kMaxStrings; ++s)
    {
        base.add (getBaseHours (s));
        acc.add (getAccruedHours (s));
    }

    o->setProperty ("base_hours", base);
    o->setProperty ("accrued_hours", acc);
    return juce::var (o);
}

void StringAging::fromVar (const juce::var& v)
{
    // An absent block means all zeros (section 8).
    for (int s = 0; s < kMaxStrings; ++s)
    {
        baseHours[(size_t) s].store (0.0, std::memory_order_relaxed);
        accruedHours[(size_t) s].store (0.0, std::memory_order_relaxed);
    }

    if (auto* o = v.getDynamicObject())
    {
        auto read = [] (const juce::var& a, std::array<std::atomic<double>, kMaxStrings>& dest)
        {
            if (auto* arr = a.getArray())
                for (int s = 0; s < juce::jmin (arr->size(), kMaxStrings); ++s)
                {
                    const double x = (double) (*arr)[s];
                    dest[(size_t) s].store (std::isfinite (x) ? juce::jmax (0.0, x) : 0.0, std::memory_order_relaxed);
                }
        };

        read (o->getProperty ("base_hours"), baseHours);
        read (o->getProperty ("accrued_hours"), accruedHours);
    }

    stateLoaded.store (true);
}

} // namespace luthier
