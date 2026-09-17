#include "Excitation.h"

namespace luthier
{

//==============================================================================
const Excitation::MaterialSpec& Excitation::specFor (Material m) noexcept
{
    // lowpassHz, peakHz, peakDb, noiseScale, lengthScale
    static const MaterialSpec specs[(size_t) Material::NumMaterials] =
    {
        { 5000.0, 2600.0, 3.0, 0.10, 1.00 },  // PickNylon      - warm, slightly soft
        { 6000.0, 3000.0, 4.0, 0.12, 1.00 },  // PickCelluloid  - the reference pick
        { 6500.0, 3400.0, 4.5, 0.11, 0.94 },  // PickDelrin     - slick, a touch brighter
        { 9000.0, 5000.0, 6.0, 0.20, 0.80 },  // PickMetal      - hard, clangy, short contact
        { 4000.0, 2000.0, 2.0, 0.16, 1.10 },  // PickWood       - soft knock, woody
        {  900.0,  600.0, 1.0, 0.05, 1.60 },  // PickFelt       - almost no transient
        { 7000.0, 4200.0, 5.0, 0.14, 0.86 },  // Fingernail     - bright and sharp
        { 2200.0, 1200.0, 1.5, 0.07, 1.35 },  // Fingertip      - round and warm
        { 1100.0,  700.0, 1.0, 0.06, 1.60 },  // Thumb          - warmest
        { 4500.0, 2600.0, 3.0, 0.13, 1.05 },  // Thumbpick      - between thumb and pick
        { 3500.0, 1800.0, 1.5, 0.45, 1.50 },  // Brush          - mostly noise
        { 3000.0, 1500.0, 2.0, 0.30, 1.40 }   // Slide          - glass/metal on wound strings
    };

    const auto idx = (size_t) juce::jlimit (0, (int) Material::NumMaterials - 1, (int) m);
    return specs[idx];
}

//==============================================================================
void Excitation::prepare (double sampleRate)
{
    sr = sampleRate;

    // Worst case: the longest string we support, plucked at the midpoint, with the
    // comb tail (3x the pluck length) and the material length scale on top.
    const int maxPluck = (int) std::ceil (sr / constants::kMinStringHz * 0.5);
    const int maxLen = (int) (maxPluck * 3.4) + 64;

    buffer.assign ((size_t) maxLen, 0.0);
    scratch.assign ((size_t) maxLen, 0.0);

    dcTrim.prepare (sr);
    dcTrim.setCutoff (25.0);

    reset();
}

void Excitation::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0);
    std::fill (scratch.begin(), scratch.end(), 0.0);
    length = 0;
    readPos = 0;
    peak = 0.0;
    shaper.reset();
    resonator.reset();
    harmonicBand.reset();
    dcTrim.reset();
}

//==============================================================================
void Excitation::trigger (const Params& p, RtRandom& rng) noexcept
{
    if (buffer.empty())
        return;

    const auto& ms = specFor (p.material);
    const int capacity = (int) buffer.size();

    const double vel = juce::jlimit (0.0, 1.0, p.velocity);

    // ---- 1. Pluck length ----------------------------------------------------
    // The triangle spans the distance from the pluck point to the nearer end, so
    // it scales with both the string length and where the hand is.
    const double pos = juce::jlimit (0.02, 0.5, p.pluckPosition);
    int pluckLen = (int) std::round (p.delaySamples * pos * ms.lengthScale);

    // A thicker pick and a more angled attack both lengthen the contact.
    const double contactStretch = 1.0 + p.pickThickness * 0.45 + p.pickAngle * 0.30;
    pluckLen = (int) std::round (pluckLen * contactStretch);

    // Hammer-ons, pull-offs and taps are not plucks: the contact is shorter and
    // the string is already moving, so the injected shape is smaller.
    double kindGain = 1.0;
    switch (p.kind)
    {
        case Kind::HammerOn:      pluckLen = (int) (pluckLen * 0.55); kindGain = 0.28; break;
        case Kind::PullOff:       pluckLen = (int) (pluckLen * 0.45); kindGain = 0.24; break;
        case Kind::Tap:           pluckLen = (int) (pluckLen * 0.50); kindGain = 0.42; break;
        case Kind::Harmonic:      pluckLen = (int) (pluckLen * 0.80); kindGain = 0.75; break;
        case Kind::PinchHarmonic: pluckLen = (int) (pluckLen * 0.35); kindGain = 0.90; break;
        case Kind::Scrape:        pluckLen = (int) (pluckLen * 2.20); kindGain = 0.55; break;
        case Kind::Slap:          pluckLen = (int) (pluckLen * 0.40); kindGain = 1.25; break;
        case Kind::Pluck:
        case Kind::NumKinds:
        default: break;
    }

    // Human variation in attack time: 0.5 to 2 ms of jitter (engine spec 5.5).
    const double jitterMs = 0.5 + rng.nextDouble() * 1.5;
    pluckLen += (int) (jitterMs * 0.001 * sr * (rng.nextBipolar() * 0.35));

    // Clamp BEFORE deriving anything from it. A very high note gives a pluck only
    // a few samples long, and the jitter above can then take it negative - which
    // would make the comb delay and the total length negative too.
    pluckLen = juce::jlimit (3, juce::jmax (3, (capacity - 16) / 3), pluckLen);

    const int combDelay = 2 * pluckLen;
    const int total = juce::jlimit (1, capacity - 1, pluckLen + combDelay + 8);

    // ---- 2. Triangle displacement -------------------------------------------
    // Apex position follows the pick angle: a parallel attack gives a sharp,
    // near-symmetric corner, an angled attack skews and softens it.
    const double apex = juce::jlimit (0.15, 0.85, 0.5 - p.pickAngle * 0.28);
    const int apexIdx = juce::jmax (1, (int) (pluckLen * apex));

    std::fill (scratch.begin(), scratch.begin() + total, 0.0);

    for (int i = 0; i < pluckLen; ++i)
    {
        const double t = (double) i;
        const double v = (i < apexIdx) ? (t / (double) apexIdx)
                                       : (1.0 - (t - apexIdx) / (double) juce::jmax (1, pluckLen - apexIdx));
        scratch[(size_t) i] = juce::jlimit (0.0, 1.0, v);
    }

    // Percussive kinds get a steep front edge rather than a ramp.
    if (p.kind == Kind::Slap || p.kind == Kind::PinchHarmonic)
    {
        const int edge = juce::jmax (1, pluckLen / 8);
        for (int i = 0; i < edge; ++i)
            scratch[(size_t) i] = 1.0;
    }

    // ---- 3. Contact noise ----------------------------------------------------
    // Real contact is not silent: nail, plectrum edge and wound-string texture all
    // add broadband grit under the impulse. Scrapes are almost entirely this.
    const double noiseGain = juce::jlimit (0.0, 1.0, p.noiseAmount) * ms.noiseScale
                             * (p.kind == Kind::Scrape ? 6.0 : 1.0);

    if (noiseGain > 1.0e-4)
    {
        for (int i = 0; i < pluckLen; ++i)
        {
            const double env = 1.0 - (double) i / (double) pluckLen;
            scratch[(size_t) i] += rng.nextBipolar() * noiseGain * env * env;
        }
    }

    // ---- 4. Comb-filter at twice the pluck length ---------------------------
    // This is the spectral notch a real pluck position creates: the partial whose
    // node sits under the finger cannot be excited.
    for (int i = 0; i < total; ++i)
    {
        const double delayed = (i >= combDelay) ? scratch[(size_t) (i - combDelay)] : 0.0;
        buffer[(size_t) i] = scratch[(size_t) i] - delayed;
    }

    // ---- 5. Material and velocity shaping ------------------------------------
    // Harder playing is brighter: the contact corner sharpens, so the effective
    // contact bandwidth rises. This is identity rule 5.
    // A hard pluck deforms the string into a much sharper corner than a soft
    // one, so the contact bandwidth moves by well over an octave across the
    // velocity range. A narrower mapping makes velocity read as volume alone.
    const double velBright = 0.45 + 1.15 * vel;
    const double brightTrim = 0.60 + 1.10 * juce::jlimit (0.0, 1.0, p.brightness);
    const double thicknessTrim = 1.25 - 0.55 * juce::jlimit (0.0, 1.0, p.pickThickness);
    const double angleTrim = 1.15 - 0.45 * juce::jlimit (0.0, 1.0, p.pickAngle);

    double cutoff = ms.lowpassHz * velBright * brightTrim * thicknessTrim * angleTrim;

    // Fingerstyle blends between flesh and nail rather than switching.
    if (p.material == Material::Fingertip || p.material == Material::Fingernail)
    {
        const auto& flesh = specFor (Material::Fingertip);
        const auto& nail  = specFor (Material::Fingernail);
        const double b = juce::jlimit (0.0, 1.0, p.nailVsFlesh);
        cutoff = (flesh.lowpassHz + (nail.lowpassHz - flesh.lowpassHz) * b)
                 * velBright * brightTrim;
    }

    cutoff = juce::jlimit (150.0, sr * 0.47, cutoff);

    shaper.reset();
    resonator.reset();
    shaper.setLowpass (sr, cutoff, 0.62);
    resonator.setPeaking (sr, juce::jlimit (100.0, sr * 0.45, ms.peakHz * (0.85 + 0.30 * vel)),
                          1.1, ms.peakDb * (0.5 + 0.5 * vel));

    for (int i = 0; i < total; ++i)
        buffer[(size_t) i] = resonator.process (shaper.process (buffer[(size_t) i]));

    // ---- 6. Harmonic isolation ----------------------------------------------
    // Touching a node kills every partial that does not have a node there. We
    // approximate that by band-limiting the excitation around the target partial.
    if ((p.kind == Kind::Harmonic || p.kind == Kind::PinchHarmonic) && p.harmonicNumber > 1)
    {
        const double f0 = sr / juce::jmax (1.0, p.delaySamples);
        const double target = juce::jlimit (40.0, sr * 0.45, f0 * (double) p.harmonicNumber);

        harmonicBand.reset();
        harmonicBand.setBandpass (sr, target, p.kind == Kind::Harmonic ? 3.2 : 2.2);

        // Two passes gives a steeper skirt, so neighbouring partials stay silent.
        for (int pass = 0; pass < 2; ++pass)
        {
            harmonicBand.reset();
            harmonicBand.setBandpass (sr, target, p.kind == Kind::Harmonic ? 3.2 : 2.2);
            for (int i = 0; i < total; ++i)
                buffer[(size_t) i] = harmonicBand.process (buffer[(size_t) i]);
        }

        // Band-passing costs a lot of level; put it back so harmonics stay audible.
        const double bandCompensation = 2.0 + 0.35 * (double) p.harmonicNumber;
        for (int i = 0; i < total; ++i)
            buffer[(size_t) i] *= bandCompensation;
    }

    // ---- 7. Remove DC so the waveguide never receives an offset --------------
    dcTrim.reset();
    for (int i = 0; i < total; ++i)
        buffer[(size_t) i] = dcTrim.process (buffer[(size_t) i]);

    // ---- 8. Normalise to a predictable peak, then apply velocity -------------
    double maxAbs = 0.0;
    for (int i = 0; i < total; ++i)
        maxAbs = juce::jmax (maxAbs, std::abs (buffer[(size_t) i]));

    // Loudness follows velocity with a mild curve; guitars are not linear in
    // velocity and a squared law feels far too steep under the fingers.
    const double amplitude = kindGain * (0.10 + 0.90 * std::pow (vel, 1.45));
    const double norm = (maxAbs > 1.0e-9) ? (amplitude / maxAbs) : 0.0;

    for (int i = 0; i < total; ++i)
        buffer[(size_t) i] = sanitise (buffer[(size_t) i] * norm);

    peak = amplitude;
    length = total;
    readPos = 0;
}

} // namespace luthier
