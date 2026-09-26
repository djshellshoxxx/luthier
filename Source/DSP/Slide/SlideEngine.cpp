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
}

bool SlideEngine::noteOn (int s, int numStrings) noexcept
{
    landing = false;

    if (! settings.enabled || ! juce::isPositiveAndBelow (s, kMaxStrings))
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
    else if (anyUnder)
    {
        /*  SPEC-SWEEP SG-6 (slide-guitar.md 2): a bar covers only as many
            strings as its length reaches at the bar's string spacing. A note
            outside that span, from the strings already under it, cannot be
            under the same bar - it is fretted. */
        const int reach = juce::jmax (1, (int) std::floor (bar.lengthMm / kStringSpacingMm) + 1);
        int lowest = s, highest = s;

        for (int i = 0; i < juce::jmin (numStrings, kMaxStrings); ++i)
            if (underBar[(size_t) i])
                lowest = juce::jmin (lowest, i), highest = juce::jmax (highest, i);

        if (highest - lowest + 1 > reach)
            return false;
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
    // SPEC-SWEEP SG-6: a larger diameter is a flatter, softer contact that
    // absorbs a little less (22 mm, the default, leaves it as it was).
    const double curvature = std::pow (22.0 / juce::jlimit (5.0, 60.0, bar.diameterMm), 0.3);
    const double contact = 1.0 - material.damping * 0.5 * juce::jmin (2.0, massFactor) * curvature;

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
    // SPEC-SWEEP SG-6: a fatter bar clanks slightly lower.
    e.startHz = e.endHz = material.clankHz * std::pow (65.0 / juce::jlimit (5.0, 500.0, bar.massGrams), 0.25)
                          * std::pow (22.0 / juce::jlimit (5.0, 60.0, bar.diameterMm), 0.15);
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

} // namespace luthier
