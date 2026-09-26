#include "PlayingNoise.h"

namespace luthier
{

//==============================================================================
StringNoiseInfo StringNoiseInfo::fromSpec (const StringSpec& spec, StringMaterial material, StringAge age) noexcept
{
    StringNoiseInfo info;
    info.material = material;
    info.wound = spec.wound;

    if (spec.wound)
    {
        // The wrap wire is what the finger and the pick ride over: its
        // diameter is half the difference between the outer and core.
        const double wrapMm = juce::jmax (0.05, (spec.diameterMm - spec.coreDiameterMm) * 0.5);

        info.windingPitchPerMm = 1.0 / wrapMm;
        info.windingDepth = juce::jlimit (0.2, 1.0, wrapMm / 0.35);
    }

    // Old strings squeak more: the winding is dirtier (string-squeak.md 10).
    info.ageRoughness = age == StringAge::Old ? 1.4 : (age == StringAge::BrokenIn ? 1.15 : 1.0);
    return info;
}

StringNoiseInfo StringNoiseInfo::fromSpec (const StringSpec& spec, StringMaterial material,
                                           double ageRoughness, double squeakCentroid) noexcept
{
    auto info = fromSpec (spec, material, StringAge::Fresh);
    info.ageRoughness = juce::jlimit (0.5, 3.0, ageRoughness);
    info.squeakCentroid = juce::jlimit (0.3, 1.5, squeakCentroid);
    return info;
}

//==============================================================================
PlayingNoise::PickMaterialProperties PlayingNoise::getPickMaterial (Excitation::Material m) noexcept
{
    using M = Excitation::Material;

    switch (m)
    {
        case M::PickNylon:     return { 1.15, 0.55, 0.30, NoiseTexture::fine,   true };
        case M::PickCelluloid: return { 1.40, 0.35, 0.50, NoiseTexture::fine,   true };
        case M::PickDelrin:    return { 1.41, 0.30, 0.20, NoiseTexture::smooth, true };
        case M::PickMetal:     return { 8.00, 0.05, 0.80, NoiseTexture::coarse, true };
        case M::PickWood:      return { 0.70, 0.60, 0.40, NoiseTexture::medium, true };

        // Felt is not in pick-noise.md 2.1; it is the softest thing in the
        // build's list, so it gets the table's softest numbers pushed further.
        case M::PickFelt:      return { 0.35, 0.85, 0.10, NoiseTexture::smooth, true };

        // A thumbpick is a celluloid pick on a ring.
        case M::Thumbpick:     return { 1.40, 0.35, 0.50, NoiseTexture::fine,   true };

        case M::Fingernail:
        case M::Fingertip:
        case M::Thumb:
        case M::Brush:
        case M::Slide:
        case M::NumMaterials:
        default:               return { 1.0, 0.5, 0.2, NoiseTexture::fine, false };
    }
}

double PlayingNoise::windingBrightness (StringMaterial m) noexcept
{
    switch (m)
    {
        case StringMaterial::PhosphorBronze:    return 0.75;
        case StringMaterial::Bronze8020:        return 0.85;
        case StringMaterial::NickelPlatedSteel: return 0.55;
        case StringMaterial::PureNickel:        return 0.45;
        case StringMaterial::StainlessSteel:    return 0.90;
        case StringMaterial::Flatwound:         return 0.10;
        case StringMaterial::Halfwound:         return 0.35;
        case StringMaterial::Coated:            return 0.40;

        // Not in string-squeak.md 4's table; placed by what their windings are.
        case StringMaterial::Cobalt:            return 0.60;
        case StringMaterial::SilkAndSteel:      return 0.50;
        case StringMaterial::Nylon:             return 0.50;   // silver-plated copper basses
        case StringMaterial::Fluorocarbon:      return 0.50;
        case StringMaterial::NumMaterials:
        default:                                return 0.55;
    }
}

NoiseTexture PlayingNoise::windingTexture (StringMaterial m) noexcept
{
    switch (m)
    {
        case StringMaterial::PhosphorBronze:  return NoiseTexture::medium;
        case StringMaterial::Bronze8020:
        case StringMaterial::StainlessSteel:  return NoiseTexture::coarse;
        case StringMaterial::Flatwound:       return NoiseTexture::smooth;
        case StringMaterial::NickelPlatedSteel:
        case StringMaterial::PureNickel:
        case StringMaterial::Halfwound:
        case StringMaterial::Coated:
        case StringMaterial::Cobalt:
        case StringMaterial::SilkAndSteel:
        case StringMaterial::Nylon:
        case StringMaterial::Fluorocarbon:
        case StringMaterial::NumMaterials:
        default:                              return NoiseTexture::fine;
    }
}

double PlayingNoise::fretDistanceMm (double scaleLengthMm, double fromFret, double toFret) noexcept
{
    auto position = [scaleLengthMm] (double fret) { return scaleLengthMm * (1.0 - std::pow (2.0, -fret / 12.0)); };
    return std::abs (position (toFret) - position (fromFret));
}

//==============================================================================
NoiseEvent PlayingNoise::makeClick (const PickSettings& pick, int stringIndex, double velocity) noexcept
{
    NoiseEvent e;
    e.noiseClass = NoiseClass::pickClick;
    e.stringIndex = stringIndex;

    const auto material = getPickMaterial (pick.material);

    if (pick.fingers || ! material.isPick || pick.clickAmount <= 0.0)
        return e;   // level 0: nothing clicks

    const double thickness = juce::jmax (0.05, pick.thicknessMm);
    const double angle = juce::degreesToRadians (juce::jlimit (0.0, 89.0, pick.angleDegrees));

    // Thicker is stiffer, and a stiffer pick hits harder.
    const double stiffness = std::sqrt (thickness / 0.73);

    // 3: amount x velocity^0.7 x stiffness x cos(angle)^1.5, with the nominal
    // (amount 0.5, velocity 100) 30 dB under the note. clickAmount is 0.5 there,
    // so the reference is 2x the -30 dB level.
    const double nominalVelocity = std::pow (100.0 / 127.0, 0.7);
    const double reference = 2.0 * kNoteReference * dbToGain (-30.0 + kClickInjectionGainDb) / nominalVelocity;

    e.level = reference * pick.clickAmount * std::pow (juce::jlimit (0.0, 1.0, velocity), 0.7)
              * stiffness * std::pow (std::cos (angle), 1.5);

    // f = 1800 (1.4 / density)^0.5 (0.73 / thickness)^0.4
    e.startHz = e.endHz = 1800.0 * std::sqrt (1.4 / material.density) * std::pow (0.73 / thickness, 0.4);

    // Brighter near the bridge, the same pluck position the string uses.
    e.brightness = juce::jlimit (0.0, 1.0, 0.15 + 0.6 * (1.0 - juce::jlimit (0.0, 0.5, pick.pluckPosition) * 2.0));

    // 3-15 ms: shorter with damping and with a sharp tip; wear lengthens it.
    const double tip = juce::jlimit (0.2, 1.0, std::sqrt (pick.tipRadiusMm / 4.0) + 0.2);
    e.decayMs = juce::jlimit (3.0, 15.0, (15.0 - 11.0 * material.damping) * tip * (1.0 + 0.3 * pick.wear));
    e.attackMs = 0.15 + 0.4 * pick.bevel;   // a bevel releases the string more gradually
    e.q = 2.5 + (1.0 - material.damping) * 5.0;

    // Wear rounds the tip: a second, lower resonance.
    e.subResonanceRatio = pick.wear > 0.0 ? 0.62 : 0.0;
    e.subResonanceLevel = 0.6 * pick.wear;

    e.texture = NoiseTexture::smooth;
    return e;
}

NoiseEvent PlayingNoise::makeChirp (const PickSettings& pick, const StringNoiseInfo& string,
                                    int stringIndex, double velocity) noexcept
{
    NoiseEvent e;
    e.noiseClass = NoiseClass::pickChirp;
    e.stringIndex = stringIndex;

    const auto material = getPickMaterial (pick.material);

    // Plain strings have no winding for the pick to cross.
    if (! string.wound || pick.fingers || ! material.isPick || pick.chirpAmount <= 0.0)
        return e;

    const double angle = juce::degreesToRadians (juce::jlimit (0.0, 89.0, pick.angleDegrees));
    const double roughness = material.roughness * (0.7 + 0.6 * pick.wear) * string.ageRoughness;

    // 4: amount x sin(angle) x roughness x windingDepth. At 0 degrees the pick
    // meets the string square-on and leaves without dragging across it.
    e.level = kNoteReference * dbToGain (-24.0) * pick.chirpAmount * std::sin (angle)
              * roughness * string.windingDepth;

    // The band sits at windingPitch x pickSpeed, and rises as the pick
    // accelerates across the winding.
    const double pickSpeedMmPerSecond = 250.0 + 900.0 * juce::jlimit (0.0, 1.0, velocity);
    const double centre = string.windingPitchPerMm * pickSpeedMmPerSecond;

    e.startHz = centre * 0.7;
    e.endHz = centre * 1.25;
    e.q = 3.0;
    e.brightness = 0.35;
    e.texture = material.texture;

    // 8-40 ms: a soft, slow stroke drags longer.
    e.attackMs = 1.0;
    e.holdMs = 6.0 + 24.0 * (1.0 - juce::jlimit (0.0, 1.0, velocity));
    e.decayMs = 8.0;
    return e;
}

NoiseEvent PlayingNoise::makeFingertipNoise (const PickSettings& pick, const StringNoiseInfo& string,
                                             int stringIndex, double velocity) noexcept
{
    // pick-noise.md 6: a much softer chirp, about 12 dB under the pick's. The
    // pick's chirp amount is the level control; with a pick the other amounts
    // mean nothing on fingers.
    auto asPick = pick;
    asPick.fingers = false;
    asPick.material = Excitation::Material::PickCelluloid;
    asPick.angleDegrees = 30.0;

    auto e = makeChirp (asPick, string, stringIndex, velocity);
    e.level *= dbToGain (-12.0);
    e.texture = NoiseTexture::smooth;

    if (! pick.fingers)
        e.level = 0.0;

    return e;
}

NoiseEvent PlayingNoise::makeSqueak (const SqueakSettings& s, const StringNoiseInfo& string,
                                     int stringIndex, double travelMm, double seconds,
                                     double travelFrets) noexcept
{
    NoiseEvent e;
    e.noiseClass = NoiseClass::squeak;
    e.stringIndex = stringIndex;

    if (! string.wound || s.slideMode || s.amount <= 0.0 || travelFrets < s.minTravelFrets)
        return e;

    const double duration = juce::jmax (0.02, seconds);
    const double speed = travelMm / duration;          // mm/s, average over the shift

    const double brightness = windingBrightness (string.material);

    // Dry fingers catch; damp ones slide (6). Pressure is felt as ^1.3 (3).
    const double roughness = juce::jlimit (0.0, 1.5, (1.3 - s.moisture) * string.ageRoughness);

    e.level = kNoteReference * dbToGain (-22.0) * s.amount * string.windingDepth
              * std::pow (juce::jlimit (0.0, 1.0, s.pressure), 1.3) * roughness
              * juce::jmin (1.0, speed / 300.0) * brightness / 0.75;

    // f = speed x windingPitch. The hand accelerates into a shift, so the
    // squeak glides up to that from about half of it.
    const double peak = speed * string.windingPitchPerMm * string.squeakCentroid;   // string-aging.md 3.5
    e.startHz = peak * 0.5;
    e.endHz = peak;

    // Brightness sets the harmonic balance; moisture dulls it.
    e.brightness = juce::jlimit (0.0, 1.0, brightness * (1.2 - 0.6 * s.moisture));
    e.q = 6.0 + 4.0 * (1.0 - s.pressure);       // pressure coarsens the texture
    e.texture = windingTexture (string.material);

    e.attackMs = 3.0;
    e.holdMs = duration * 1000.0;
    e.decayMs = 25.0;
    return e;
}

//==============================================================================
void PlayingNoise::prepare (double sampleRate)
{
    sr = juce::jmax (8000.0, sampleRate);
    pool.prepare (sr);
    reset();
}

void PlayingNoise::reset() noexcept
{
    pool.reset();
    scrape = {};
}

void PlayingNoise::onPluck (int stringIndex, const StringNoiseInfo& string, double velocity) noexcept
{
    if (pick.fingers)
    {
        pool.trigger (makeFingertipNoise (pick, string, stringIndex, velocity));
        return;
    }

    pool.trigger (makeClick (pick, stringIndex, velocity));
    pool.trigger (makeChirp (pick, string, stringIndex, velocity));
}

bool PlayingNoise::onShift (int stringIndex, const StringNoiseInfo& string, double scaleLengthMm,
                            double fromFret, double toFret, double seconds, juce::uint32 shiftIndex) noexcept
{
    const double travelFrets = std::abs (toFret - fromFret);
    const double travelMm = fretDistanceMm (scaleLengthMm, fromFret, toFret);

    auto event = makeSqueak (squeak, string, stringIndex, travelMm, seconds, travelFrets);

    if (event.level <= 0.0)
        return false;

    // 6: not every shift squeaks. The roll is deterministic per seed and
    // shift, and moisture lowers the odds as well as the brightness.
    const double chance = juce::jlimit (0.0, 1.0, squeak.probability * (1.35 - squeak.moisture));

    if (noiseUniform (seed32 ^ 0x51ea4u, shiftIndex) >= chance)
        return false;

    return pool.trigger (event) >= 0;
}

void PlayingNoise::startScrape (double seconds, bool downward, const bool* wound, int numStrings) noexcept
{
    if (pick.scrapeAmount <= 0.0 || pick.fingers || wound == nullptr)
        return;

    scrape = {};

    for (int i = 0; i < juce::jmin (numStrings, (int) scrape.order.size()); ++i)
    {
        // String 0 is the highest here, so a downstroke (toward the floor)
        // starts at the other end.
        const int s = downward ? numStrings - 1 - i : i;

        if (wound[s])
            scrape.order[(size_t) scrape.crossings++] = s;
    }

    if (scrape.crossings == 0)
        return;

    const double duration = juce::jlimit (0.1, 3.0, seconds);
    scrape.samplesPerCrossing = duration * sr / scrape.crossings;
    scrape.counter = scrape.samplesPerCrossing;   // the first crossing is immediate
    scrape.active = true;
}

double PlayingNoise::processSample (double* excitationNoise, double* surfaceNoise, int numStrings) noexcept
{
    if (scrape.active)
    {
        scrape.counter += 1.0;

        if (scrape.counter >= scrape.samplesPerCrossing)
        {
            scrape.counter -= scrape.samplesPerCrossing;

            const int s = scrape.order[(size_t) scrape.next++];

            // Each crossing is a chirp-like band dragged for the whole time
            // the pick is on that string, over a broadband scratch.
            NoiseEvent e;
            e.noiseClass = NoiseClass::pickScrape;
            e.stringIndex = s;
            e.level = kNoteReference * dbToGain (-18.0) * pick.scrapeAmount
                      * getPickMaterial (pick.material).roughness * 2.0;
            e.startHz = 1400.0 + 250.0 * s;
            e.endHz = e.startHz * 1.6;
            e.q = 1.4;
            e.brightness = 0.8;
            e.texture = NoiseTexture::coarse;
            e.attackMs = 2.0;
            e.holdMs = 1000.0 * scrape.samplesPerCrossing / sr;
            e.decayMs = 30.0;

            pool.trigger (e);

            if (scrape.next >= scrape.crossings)
                scrape.active = false;
        }
    }

    if (pool.isIdle())
    {
        for (int s = 0; s < numStrings; ++s)
            excitationNoise[s] = surfaceNoise[s] = 0.0;

        return 0.0;
    }

    return pool.processSample (excitationNoise, surfaceNoise, numStrings);
}

} // namespace luthier
