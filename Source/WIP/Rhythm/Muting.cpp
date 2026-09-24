#include "Muting.h"

namespace luthier
{

//==============================================================================
const char* getMuteTypeId (MuteType type) noexcept
{
    switch (type)
    {
        case MuteType::palmLight:   return "palm_mute_light";
        case MuteType::palmHeavy:   return "palm_mute_heavy";
        case MuteType::palmExtreme: return "palm_mute_extreme";
        case MuteType::ghost:       return "ghost";
        case MuteType::chuka:       return "chuka";
        case MuteType::fretMute:    return "fret_mute";
        case MuteType::open:
        case MuteType::numTypes:
        default:                    return "open";
    }
}

const char* getMuteTypeName (MuteType type) noexcept
{
    switch (type)
    {
        case MuteType::palmLight:   return "Palm Mute Light";
        case MuteType::palmHeavy:   return "Palm Mute Heavy";
        case MuteType::palmExtreme: return "Palm Mute Extreme";
        case MuteType::ghost:       return "Ghost";
        case MuteType::chuka:       return "Chuka";
        case MuteType::fretMute:    return "Fret Mute";
        case MuteType::open:
        case MuteType::numTypes:
        default:                    return "Open";
    }
}

const char* getMuteTypeGlyph (MuteType type) noexcept
{
    switch (type)
    {
        case MuteType::palmLight:   return "PL";
        case MuteType::palmHeavy:   return "PH";
        case MuteType::palmExtreme: return "PX";
        case MuteType::ghost:       return "G";
        case MuteType::chuka:       return "C";
        case MuteType::fretMute:    return "F";
        case MuteType::open:
        case MuteType::numTypes:
        default:                    return "";
    }
}

MuteType muteTypeFromId (const juce::String& id) noexcept
{
    for (int t = 0; t < (int) MuteType::numTypes; ++t)
        if (id == getMuteTypeId ((MuteType) t))
            return (MuteType) t;

    return MuteType::open;
}

juce::StringArray getMuteMasterModeNames()
{
    juce::StringArray names { "Off" };

    for (int t = 0; t < (int) MuteType::numTypes; ++t)
        names.add (getMuteTypeName ((MuteType) t));

    return names;
}

//==============================================================================
namespace Muting
{
    double palmFactor (double pressure, double positionMm) noexcept
    {
        const double p = juce::jlimit (0.0, 1.0, pressure);
        const double mm = juce::jlimit (MuteSettings::kMinPositionMm, MuteSettings::kMaxPositionMm, positionMm);

        // Twice as long at no pressure, half at full; further from the saddle
        // the palm covers more of the string and stops it sooner.
        return std::pow (2.0, (0.5 - p) * 2.0) * std::sqrt (35.0 / mm);
    }

    MuteDamping dampingFor (MuteType type, const MuteSettings& settings,
                            double pressureOverride, double positionOverrideMm) noexcept
    {
        const double pressure = pressureOverride >= 0.0 ? pressureOverride : settings.palmPressure;
        const double position = positionOverrideMm >= 0.0 ? positionOverrideMm : settings.palmPositionMm;

        MuteDamping d;

        auto palm = [&d, pressure, position] (double t60, double cutoff)
        {
            const double f = palmFactor (pressure, position);
            d.t60Seconds = juce::jmax (0.01, t60 * f);
            d.cutoffHz = juce::jlimit (200.0, 8000.0, cutoff * std::sqrt (f));
        };

        switch (type)
        {
            case MuteType::palmLight:   palm (kPalmLightT60,   1500.0); break;
            case MuteType::palmHeavy:   palm (kPalmHeavyT60,    900.0); break;
            case MuteType::palmExtreme: palm (kPalmExtremeT60,  500.0); break;

            case MuteType::ghost:
                // The fretting hand, not the palm: position does not apply.
                d.t60Seconds = juce::jmax (0.01, kGhostT60 * palmFactor (pressure, 35.0));
                d.cutoffHz = 400.0;
                d.velocityScale = juce::jlimit (0.0, 1.0, settings.ghostVelocity);
                break;

            case MuteType::chuka:
                d.chuck = true;
                break;

            case MuteType::fretMute:
                d.releaseAfterSeconds = kFretMuteRingSeconds;
                d.releaseT60Seconds = kFretMuteStopT60;
                break;

            case MuteType::open:
            case MuteType::numTypes:
            default:
                break;
        }

        return d;
    }

    double uniform (juce::uint64 seed, juce::uint32 loop, int stepIndex) noexcept
    {
        // splitmix64, as StrumGesture: stateless, so a loop's flips are the
        // same however many loops were played before it.
        juce::uint64 z = seed ^ 0x6D757465ull
                       ^ ((juce::uint64) loop * 0x9E3779B97F4A7C15ull)
                       ^ ((juce::uint64) (juce::uint32) stepIndex * 0xC2B2AE3D27D4EB4Full);

        z += 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z = z ^ (z >> 31);

        return (double) (z >> 11) * (1.0 / 9007199254740992.0);
    }

    MuteType humanise (MuteType type, double probability,
                       juce::uint64 seed, juce::uint32 loop, int stepIndex) noexcept
    {
        const bool eligible = type == MuteType::open || isPalmMute (type);

        if (! eligible || probability <= 0.0
              || uniform (seed, loop, stepIndex) >= juce::jlimit (0.0, 1.0, probability))
            return type;

        return type == MuteType::open ? MuteType::palmLight : MuteType::open;
    }

    MuteStep resolve (const MuteStep& step, const MuteSettings& settings,
                      bool isStrum, double dynamic,
                      juce::uint64 seed, juce::uint32 loop, int stepIndex) noexcept
    {
        auto r = step;

        if (! juce::isPositiveAndBelow ((int) r.type, (int) MuteType::numTypes))
            r.type = MuteType::open;

        if (! settings.armed)
            return r;

        if (juce::isPositiveAndBelow (settings.masterMode, (int) MuteType::numTypes))
        {
            // 3: the master mode overrides the pattern's type, and with it the
            // step's own pressure and position, which belonged to that type.
            r = MuteStep {};
            r.type = (MuteType) settings.masterMode;
        }
        else if (r.isOpen() && isStrum && settings.chukaSource == ChukaSource::softStrums
                   && dynamic < MuteSettings::kChukaDynamic)
        {
            r.type = MuteType::chuka;
        }

        r.type = humanise (r.type, settings.humanise, seed, loop, stepIndex);
        return r;
    }
}

//==============================================================================
namespace
{
    // muting-rhythm 6. Sixteenths; beats fall on 0, 4, 8, 12.
    const MuteGridPreset presets[] =
    {
        // Palm mute heavy on every step.
        { "Metal Chug 16ths",   "hhhhhhhhhhhhhhhh" },
        // Open on the beats, ghosts on the ands, chuka on the other off-beats.
        { "Funk Chuka",         "ocgcocgcocgcocgc" },
        // Open on 2 and 4, a light palm everywhere else.
        { "Reggae Skank",       "llllolllllllolll" },
        // Boom (palm-muted bass) on 1 and 3, chick (chuka) on 2 and 4.
        { "Country Boom-Chick", "hooocooohooocooo" },
        // Heavy on the gallop's onsets: dotted eighth, sixteenth, eighth.
        { "Metal Gallop",       "hoohhooohoohhooo" },
        // A fret mute on every note, no palm.
        { "Classical Staccato", "ffffffffffffffff" }
    };
}

int getNumMuteGridPresets() noexcept
{
    return (int) (sizeof (presets) / sizeof (presets[0]));
}

const MuteGridPreset& getMuteGridPreset (int index) noexcept
{
    return presets[juce::jlimit (0, getNumMuteGridPresets() - 1, index)];
}

MuteType muteTypeFromLetter (char letter) noexcept
{
    switch (letter)
    {
        case 'l': return MuteType::palmLight;
        case 'h': return MuteType::palmHeavy;
        case 'x': return MuteType::palmExtreme;
        case 'g': return MuteType::ghost;
        case 'c': return MuteType::chuka;
        case 'f': return MuteType::fretMute;
        case 'o':
        default:  return MuteType::open;
    }
}

} // namespace luthier
