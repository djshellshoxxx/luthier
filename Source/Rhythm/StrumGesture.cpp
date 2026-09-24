#include "StrumGesture.h"

namespace luthier
{

//==============================================================================
const char* getStrikerName (Striker striker) noexcept
{
    switch (striker)
    {
        case Striker::pick:        return "Pick";
        case Striker::thumb:       return "Thumb";
        case Striker::fingernails: return "Fingernails";
        case Striker::flesh:       return "Flesh";
        case Striker::thumbpick:   return "Thumbpick";
        case Striker::brush:       return "Brush";
        case Striker::numStrikers:
        default:                   return "Pick";
    }
}

juce::StringArray getStrikerNames()
{
    juce::StringArray names;

    for (int i = 0; i < (int) Striker::numStrikers; ++i)
        names.add (getStrikerName ((Striker) i));

    return names;
}

double getStrikerCrossingFactor (Striker striker) noexcept
{
    // strum-dynamics 5's crossing column.
    switch (striker)
    {
        case Striker::thumb:       return 0.6;
        case Striker::fingernails: return 1.3;
        case Striker::flesh:       return 0.7;
        case Striker::thumbpick:   return 0.9;
        case Striker::brush:       return 0.4;
        case Striker::pick:
        case Striker::numStrikers:
        default:                   return 1.0;
    }
}

int getStrikerMaterial (Striker striker) noexcept
{
    using M = Excitation::Material;

    switch (striker)
    {
        case Striker::thumb:       return (int) M::Thumb;
        case Striker::fingernails: return (int) M::Fingernail;
        case Striker::flesh:       return (int) M::Fingertip;
        case Striker::thumbpick:   return (int) M::Thumbpick;
        case Striker::brush:       return (int) M::Brush;
        case Striker::pick:
        case Striker::numStrikers:
        default:                   return -1;
    }
}

//==============================================================================
StrumSettings StrumSettings::guitarDefaults() noexcept
{
    return {};
}

StrumSettings StrumSettings::bassDefaults() noexcept
{
    // strum-dynamics 4's bass column; bass-techniques.md 8 repeats the first two.
    StrumSettings s;
    s.crossingSps = 100.0;
    s.acceleration = 0.25;
    s.upVelocityRatio = 1.15;
    s.tilt = 0.10;
    s.evenness = 0.85;
    s.missProbability = 0.01;
    return s;
}

StrumSettings StrumSettings::retargetDefaults (const StrumSettings& current,
                                               bool fromBass, bool toBass) noexcept
{
    if (fromBass == toBass)
        return current;

    const auto from = defaultsFor (fromBass);
    const auto to = defaultsFor (toBass);

    // Parameters travel as floats, so "still at the default" allows for that.
    auto follow = [] (double value, double oldDefault, double newDefault)
    {
        return std::abs (value - oldDefault) <= 1.0e-4 * juce::jmax (1.0, std::abs (oldDefault))
                 ? newDefault : value;
    };

    auto s = current;
    s.crossingSps     = follow (current.crossingSps,     from.crossingSps,     to.crossingSps);
    s.acceleration    = follow (current.acceleration,    from.acceleration,    to.acceleration);
    s.upVelocityRatio = follow (current.upVelocityRatio, from.upVelocityRatio, to.upVelocityRatio);
    s.tilt            = follow (current.tilt,            from.tilt,            to.tilt);
    s.evenness        = follow (current.evenness,        from.evenness,        to.evenness);
    s.missProbability = follow (current.missProbability, from.missProbability, to.missProbability);
    s.chuckAmount     = follow (current.chuckAmount,     from.chuckAmount,     to.chuckAmount);
    s.chuckDamping    = follow (current.chuckDamping,    from.chuckDamping,    to.chuckDamping);

    if (current.strikerDown == from.strikerDown) s.strikerDown = to.strikerDown;
    if (current.strikerUp == from.strikerUp)     s.strikerUp = to.strikerUp;

    return s;
}

StrumSettings StrumSettings::clamped() const noexcept
{
    auto validStriker = [] (Striker s)
    {
        return juce::isPositiveAndBelow ((int) s, (int) Striker::numStrikers) ? s : Striker::pick;
    };

    // strum-dynamics 7's ranges.
    StrumSettings s;
    s.crossingSps     = juce::jlimit (20.0, 800.0, crossingSps);
    s.acceleration    = juce::jlimit (0.0, 1.0, acceleration);
    s.upVelocityRatio = juce::jlimit (0.5, 2.0, upVelocityRatio);
    s.tilt            = juce::jlimit (-1.0, 1.0, tilt);
    s.evenness        = juce::jlimit (0.0, 1.0, evenness);
    s.missProbability = juce::jlimit (0.0, 1.0, missProbability);
    s.strikerDown     = validStriker (strikerDown);
    s.strikerUp       = validStriker (strikerUp);
    s.chuckAmount     = juce::jlimit (0.0, 1.0, chuckAmount);
    s.chuckDamping    = juce::jlimit (0.0, 1.0, chuckDamping);
    return s;
}

//==============================================================================
namespace StrumFeel
{
    namespace
    {
        double throughThree (double feel, double atZero, double atHalf, double atOne) noexcept
        {
            const double f = juce::jlimit (0.0, 1.0, feel);

            return f <= 0.5 ? atZero + (atHalf - atZero) * (f / 0.5)
                            : atHalf + (atOne - atHalf) * ((f - 0.5) / 0.5);
        }
    }

    double crossingSpsFor (double feel) noexcept { return throughThree (feel, 60.0, 200.0, 400.0); }
    double evennessFor (double feel) noexcept    { return throughThree (feel, 0.45, 0.75, 0.95); }
}

//==============================================================================
double StrumGesture::effectiveCrossingSps (const StrumSettings& settings,
                                           double sourceSps, bool down) noexcept
{
    // 2.1: the return stroke is faster; 5: a thumb is slower than a pick.
    const double direction = down ? 1.0 : juce::jlimit (0.5, 2.0, settings.upVelocityRatio);
    const auto striker = down ? settings.strikerDown : settings.strikerUp;

    // Not clamped to the parameter's 20-800: a kit or a test may ask for a
    // near-instant strum, and the parameter's range is the UI's business.
    return juce::jmax (1.0, sourceSps) * direction * getStrikerCrossingFactor (striker);
}

double StrumGesture::crossingSeconds (int numStrings, double sps) noexcept
{
    if (numStrings <= 1)
        return 0.0;

    return (double) (numStrings - 1) / juce::jmax (1.0, sps);
}

double StrumGesture::ease (double u, double acceleration) noexcept
{
    u = juce::jlimit (0.0, 1.0, u);
    const double a = juce::jlimit (0.0, 1.0, acceleration);

    // The inverse of smoothstep s(t) = t^2 (3 - 2t): the time at which a hand
    // moving along the smoothstep reaches position u.
    const double inverse = 0.5 - std::sin (std::asin (juce::jlimit (-1.0, 1.0, 1.0 - 2.0 * u)) / 3.0);

    return (1.0 - a) * u + a * inverse;
}

double StrumGesture::tiltFactor (int order, int numStrings, double tilt, bool down) noexcept
{
    if (numStrings <= 1)
        return 1.0;

    const double u = (double) order / (double) (numStrings - 1);
    const double t = juce::jlimit (-1.0, 1.0, tilt) * (down ? 1.0 : -1.0);

    // +-50 % across the strum at full tilt; the default 0.15 is +-7.5 %.
    return 1.0 + t * (u - 0.5);
}

double StrumGesture::evennessFactor (juce::uint64 seed, juce::uint32 strumIndex,
                                     int stringIndex, double evenness) noexcept
{
    const double spread = (1.0 - juce::jlimit (0.0, 1.0, evenness)) * kMaxUnevenness;

    if (spread <= 0.0)
        return 1.0;

    return 1.0 + spread * (2.0 * uniform (seed, strumIndex, stringIndex, 0xE7E7u) - 1.0);
}

double StrumGesture::accentFactor (int order) noexcept
{
    return order < 2 ? dbToGain (kAccentDb) : 1.0;
}

double StrumGesture::missChance (int order, int numStrings, double probability) noexcept
{
    const double p = juce::jlimit (0.0, 1.0, probability);

    if (numStrings <= 0 || p <= 0.0)
        return 0.0;

    // Leading string at w q, the others at q, averaging p over n strings:
    // (w + n - 1) q = n p.
    const double n = (double) numStrings;
    const double q = p * n / (kLeadingMissWeight + n - 1.0);

    return juce::jlimit (0.0, 1.0, order == 0 ? kLeadingMissWeight * q : q);
}

double StrumGesture::chuckFor (const StrumSettings& settings, bool isChuckStep) noexcept
{
    const double blend = isChuckStep ? 1.0 : juce::jlimit (0.0, 1.0, settings.chuckAmount);
    return blend * juce::jlimit (0.0, 1.0, settings.chuckDamping);
}

double StrumGesture::uniform (juce::uint64 seed, juce::uint32 strumIndex,
                              int stringIndex, juce::uint32 salt) noexcept
{
    // splitmix64 over the four inputs: stateless, so a strum's variation is the
    // same however many strums were planned before it.
    juce::uint64 z = seed
                   ^ ((juce::uint64) strumIndex * 0x9E3779B97F4A7C15ull)
                   ^ ((juce::uint64) (juce::uint32) stringIndex * 0xC2B2AE3D27D4EB4Full)
                   ^ ((juce::uint64) salt << 32);

    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z = z ^ (z >> 31);

    return (double) (z >> 11) * (1.0 / 9007199254740992.0);
}

//==============================================================================
int StrumGesture::plan (const StrumSettings& settings, const StrumRequest& request,
                        StrumStrike* out, int maxOut) const noexcept
{
    if (out == nullptr || request.strings == nullptr || request.numStrings <= 0 || maxOut <= 0)
        return 0;

    const int n = juce::jmin (request.numStrings, maxOut, kMaxStrings);
    const bool down = request.down;

    const double sps = effectiveCrossingSps (settings, request.sourceSps, down);
    const double total = crossingSeconds (n, sps);

    /*  3: force = base x tilt x evenness x accent. "base" is taken as the
        level of the strongest string before evenness, so the loudest string
        of a dynamic-1.0 strum is at full velocity rather than over it:
        velocities stop at 1, and without this the accent and the upper half
        of the tilt would be clipped away on exactly the strums that carry
        them. The shape across the strings is the spec's. */
    double nominalMax = 0.0;

    for (int i = 0; i < n; ++i)
        nominalMax = juce::jmax (nominalMax, tiltFactor (i, n, settings.tilt, down) * accentFactor (i));

    const double norm = (nominalMax > 0.0 ? 1.0 / nominalMax : 1.0)
                      * (down ? 1.0 : kUpStrokeForce);
    const double missScale = juce::jmax (0.0, request.missScale);

    for (int i = 0; i < n; ++i)
    {
        const int s = request.strings[i];
        const double u = n > 1 ? (double) i / (double) (n - 1) : 0.0;

        auto& strike = out[i];
        strike.stringIndex = s;
        strike.timeSeconds = total * ease (u, settings.acceleration);
        strike.force = tiltFactor (i, n, settings.tilt, down)
                     * evennessFactor (seed, request.strumIndex, s, settings.evenness)
                     * accentFactor (i)
                     * norm;

        const double chance = missChance (i, n, settings.missProbability) * missScale;
        strike.missed = chance > 0.0
                          && uniform (seed, request.strumIndex, s, 0x5A15u) < chance;
    }

    return n;
}

} // namespace luthier
