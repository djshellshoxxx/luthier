#include "TuningEngine.h"

namespace luthier
{

namespace
{
    //==========================================================================
    // Ratios within one octave, relative to the temperament root.

    const double kJust[12] =
    {
        1.0, 16.0/15.0, 9.0/8.0, 6.0/5.0, 5.0/4.0, 4.0/3.0,
        45.0/32.0, 3.0/2.0, 8.0/5.0, 5.0/3.0, 9.0/5.0, 15.0/8.0
    };

    // Quarter-comma meantone.
    const double kMeantone[12] =
    {
        1.000000, 1.044907, 1.118034, 1.196279, 1.250000, 1.337481,
        1.397542, 1.495349, 1.562500, 1.671851, 1.788854, 1.869186
    };

    const double kWerckmeister3[12] =
    {
        1.000000, 1.053497, 1.117403, 1.185185, 1.252827, 1.333333,
        1.404663, 1.494927, 1.580246, 1.670436, 1.777778, 1.879240
    };

    const double kKirnberger3[12] =
    {
        1.000000, 1.053497, 1.118034, 1.185185, 1.250000, 1.333333,
        1.406250, 1.495349, 1.580246, 1.671851, 1.777778, 1.875000
    };

    const double kPythagorean[12] =
    {
        1.000000, 256.0/243.0, 9.0/8.0, 32.0/27.0, 81.0/64.0, 4.0/3.0,
        729.0/512.0, 3.0/2.0, 128.0/81.0, 27.0/16.0, 16.0/9.0, 243.0/128.0
    };

    //==========================================================================
    // Open-string frequencies per preset, string 0 = highest-pitched string.

    struct PresetDef { int count; double f[kMaxStrings]; };

    const PresetDef kPresets[(size_t) TuningPreset::NumPresets] =
    {
        /* Standard      */ { 6, { 329.628, 246.942, 195.998, 146.832, 110.000,  82.407 } },
        /* DropD         */ { 6, { 329.628, 246.942, 195.998, 146.832, 110.000,  73.416 } },
        /* DropC         */ { 6, { 293.665, 220.000, 174.614, 130.813,  97.999,  65.406 } },
        /* DropB         */ { 6, { 277.183, 207.652, 164.814, 123.471,  92.499,  61.735 } },
        /* DADGAD        */ { 6, { 293.665, 220.000, 195.998, 146.832, 110.000,  73.416 } },
        /* OpenG         */ { 6, { 293.665, 246.942, 195.998, 146.832,  97.999,  73.416 } },
        /* OpenD         */ { 6, { 293.665, 220.000, 184.997, 146.832, 110.000,  73.416 } },
        /* OpenE         */ { 6, { 329.628, 246.942, 207.652, 164.814, 123.471,  82.407 } },
        /* OpenC         */ { 6, { 329.628, 261.626, 195.998, 130.813,  97.999,  65.406 } },
        /* HalfStepDown  */ { 6, { 311.127, 233.082, 184.997, 138.591, 103.826,  77.782 } },
        /* FullStepDown  */ { 6, { 293.665, 220.000, 174.614, 130.813,  97.999,  73.416 } },
        /* Nashville     */ { 6, { 329.628, 246.942, 391.995, 293.665, 220.000, 164.814 } },
        /* SevenString   */ { 7, { 329.628, 246.942, 195.998, 146.832, 110.000,  82.407,  61.735 } },
        /* EightString   */ { 8, { 329.628, 246.942, 195.998, 146.832, 110.000,  82.407,  61.735, 46.249 } },
        /* BaritoneB     */ { 6, { 246.942, 184.997, 146.832, 110.000,  82.407,  61.735 } },
        /* BassStandard  */ { 4, {  97.999,  73.416,  55.000,  41.203 } },
        /* BassFiveString*/ { 5, {  97.999,  73.416,  55.000,  41.203,  30.868 } },
        /* Custom        */ { 6, { 329.628, 246.942, 195.998, 146.832, 110.000,  82.407 } }
    };

    const char* const kNoteNamesSharp[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

//==============================================================================
TuningEngine::TuningEngine()
{
    for (int i = 0; i < 12; ++i)
        customRatios[(size_t) i] = std::pow (2.0, i / 12.0);

    driftTargets.fill (0.0);
    setTuningPreset (TuningPreset::Standard);
}

void TuningEngine::prepare (double sampleRate) noexcept
{
    sr = sampleRate;
    driftIntervalSamples = juce::jmax (1, (int) (sr * 30.0));
    driftCounter = 0;
}

void TuningEngine::reset() noexcept
{
    driftCounter = 0;
    driftTargets.fill (0.0);
    driftRng.setSeed (kDriftSeed);   // the same walk from every reset

    for (auto& s : strings)
    {
        s.driftCents = 0.0;
        s.characterDriftCents = 0.0;
    }
}

//==============================================================================
void TuningEngine::setNumStrings (int n) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
}

void TuningEngine::setTuningPreset (TuningPreset preset) noexcept
{
    currentPreset = preset;

    if (preset == TuningPreset::Custom)
        return;

    const auto& def = kPresets[(size_t) juce::jlimit (0, (int) TuningPreset::NumPresets - 1, (int) preset)];
    numStrings = def.count;

    for (int i = 0; i < numStrings; ++i)
        strings[(size_t) i].openFrequencyHz = def.f[i] * (concertA / 440.0);
}

void TuningEngine::setStringTuning (int stringIndex, const StringTuning& t) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
    {
        strings[(size_t) stringIndex] = t;
        currentPreset = TuningPreset::Custom;
    }
}

const TuningEngine::StringTuning& TuningEngine::getStringTuning (int stringIndex) const noexcept
{
    return strings[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)];
}

void TuningEngine::setOpenFrequency (int stringIndex, double hz) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
    {
        strings[(size_t) stringIndex].openFrequencyHz = juce::jlimit (12.0, 4000.0, hz);
        currentPreset = TuningPreset::Custom;
    }
}

void TuningEngine::setDetuneCents (int stringIndex, double cents) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        strings[(size_t) stringIndex].detuneCents = juce::jlimit (-100.0, 100.0, cents);
}

void TuningEngine::setCharacterDriftCents (int stringIndex, double cents) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        // environment.md 8: the room's offset rides here too, up to +-300 cents
        // at the advanced extremes.
        strings[(size_t) stringIndex].characterDriftCents = juce::jlimit (-350.0, 350.0, cents);
}

void TuningEngine::setFineTuneCents (int stringIndex, double cents) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        strings[(size_t) stringIndex].fineTuneCents = juce::jlimit (-50.0, 50.0, cents);
}

void TuningEngine::setIntonationSlope (int stringIndex, double centsPerFret) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        strings[(size_t) stringIndex].intonationSlope = juce::jlimit (-2.0, 2.0, centsPerFret);
}

void TuningEngine::setAgingIntonation (int stringIndex, double centsPerFret) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        strings[(size_t) stringIndex].agingIntonationSlope = juce::jlimit (0.0, 5.0, centsPerFret);
}

void TuningEngine::setMaxFrets (int stringIndex, int frets) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        strings[(size_t) stringIndex].maxFrets = juce::jlimit (5, 30, frets);
}

void TuningEngine::setCustomTemperament (const std::array<double, 12>& ratios) noexcept
{
    for (size_t i = 0; i < 12; ++i)
        customRatios[i] = juce::jlimit (0.5, 2.5, ratios[i]);
}

void TuningEngine::setConcertA (double hz) noexcept
{
    const double newA = juce::jlimit (392.0, 494.0, hz);
    const double scale = newA / concertA;
    concertA = newA;

    for (auto& s : strings)
        s.openFrequencyHz *= scale;
}

//==============================================================================
void TuningEngine::randomiseRealismDetune (double maxCents, uint64_t seed) noexcept
{
    RtRandom rng { seed };
    const double limit = juce::jlimit (0.0, 20.0, maxCents);

    for (int i = 0; i < kMaxStrings; ++i)
    {
        // Gaussian rather than uniform: most strings are close, one or two are
        // noticeably off, which is what a real guitar that was tuned five minutes
        // ago actually sounds like.
        double c = rng.nextGaussian() * limit * 0.45;
        strings[(size_t) i].realismDetuneCents = juce::jlimit (-limit, limit, c);
    }
}

void TuningEngine::setDriftEnabled (bool enabled, double maxCents) noexcept
{
    driftEnabled = enabled;
    driftMaxCents = juce::jlimit (0.0, 25.0, maxCents);

    if (! enabled)
    {
        for (auto& s : strings)
            s.driftCents = 0.0;

        driftTargets.fill (0.0);
    }
}

void TuningEngine::advanceDrift (int numSamples) noexcept
{
    if (! driftEnabled || numSamples <= 0)
        return;

    driftCounter += numSamples;

    if (driftCounter >= driftIntervalSamples)
    {
        driftCounter = 0;

        for (int i = 0; i < numStrings; ++i)
        {
            // Random walk, bounded, so a long session goes gently out of tune
            // rather than wandering off into another key.
            double next = driftTargets[(size_t) i] + driftRng.nextGaussian() * driftMaxCents * 0.30;
            driftTargets[(size_t) i] = juce::jlimit (-driftMaxCents, driftMaxCents, next);
        }
    }

    // Glide toward the target so the drift is never a step.
    const double rate = (double) numSamples / juce::jmax (1.0, sr * 4.0);

    for (int i = 0; i < numStrings; ++i)
    {
        auto& s = strings[(size_t) i];
        s.driftCents += (driftTargets[(size_t) i] - s.driftCents) * juce::jmin (1.0, rate);
    }
}

//==============================================================================
double TuningEngine::temperamentRatio (double semitonesFromRoot) const noexcept
{
    if (temperament == Temperament::EqualTemp12)
        return std::pow (2.0, semitonesFromRoot / 12.0);

    const double* table = nullptr;

    switch (temperament)
    {
        case Temperament::JustIntonation: table = kJust;          break;
        case Temperament::Meantone:       table = kMeantone;      break;
        case Temperament::Werckmeister3:  table = kWerckmeister3; break;
        case Temperament::Kirnberger3:    table = kKirnberger3;   break;
        case Temperament::Pythagorean:    table = kPythagorean;   break;
        case Temperament::Custom:         table = customRatios.data(); break;
        case Temperament::EqualTemp12:
        case Temperament::NumTemperaments:
        default:
            return std::pow (2.0, semitonesFromRoot / 12.0);
    }

    // Split into whole octaves plus a fractional scale degree. The fraction is
    // interpolated between adjacent table entries so bends stay continuous even
    // in an unequal temperament (pitfall 13).
    const double octaves = std::floor (semitonesFromRoot / 12.0);
    double within = semitonesFromRoot - octaves * 12.0;

    const int lower = (int) std::floor (within);
    const double frac = within - (double) lower;

    const double a = table[(size_t) (lower % 12)];
    const double b = (lower >= 11) ? table[0] * 2.0 : table[(size_t) (lower + 1)];

    // Interpolate in the log domain: a linear blend of ratios would bend flat.
    const double ratio = std::exp (std::log (a) + (std::log (b) - std::log (a)) * frac);

    return ratio * std::pow (2.0, octaves);
}

//==============================================================================
double TuningEngine::getOpenFrequencyBeforeCapo (int stringIndex) const noexcept
{
    const auto& s = getStringTuning (stringIndex);
    const double cents = s.detuneCents + s.realismDetuneCents + s.driftCents
                           + s.fineTuneCents + s.characterDriftCents;
    return s.openFrequencyHz * centsToRatio (cents);
}

double TuningEngine::getEffectiveOpenFrequency (int stringIndex) const noexcept
{
    /*  4.5: "open strings are the capo'd notes". Everything that asks what a
        string sounds like open - the tuner, the string list, the headstock
        popover, the engine setting up the string - wants the note it actually
        sounds, so the capo is applied here rather than left for each caller to
        remember. */
    const int capo = getCapoFretFor (stringIndex);

    if (capo <= 0)
        return getOpenFrequencyBeforeCapo (stringIndex);

    return getOpenFrequencyBeforeCapo (stringIndex) * temperamentRatio ((double) capo);
}

void TuningEngine::setCapoFret (int fret) noexcept
{
    // A capo past the end of the neck is silly rather than illegal: the playable
    // span clamps to zero and the string plays one note.
    capoFret = juce::jlimit (0, 36, fret);
}

int TuningEngine::getHighestPlayableFret (int stringIndex) const noexcept
{
    return juce::jmax (0, getStringTuning (stringIndex).maxFrets - getCapoFretFor (stringIndex));
}

double TuningEngine::computeFrequency (int stringIndex, double fretPosition, double bendCents) const noexcept
{
    const auto& s = getStringTuning (stringIndex);

    /*  Fret positions are measured from the capo, so the fret actually being
        held is capoFret higher up the neck. Both the temperament and the
        intonation are asked about that absolute position:

        - the temperament, because the frets are at fixed places. A capo at 5
          gives exactly what fret 5 gives, which under an unequal temperament is
          not the open string shifted by a tempered fourth. Multiplying two
          ratios instead of taking one at the sum would be wrong here, and
          identical under equal temperament - which is what would have let it
          ship.
        - the intonation, because a capo is a fret: the string is stretched by
          the whole distance from the nut, not just the part above the capo.
    */
    const double absoluteFret = (double) getCapoFretFor (stringIndex) + fretPosition;

    const double openHz = getOpenFrequencyBeforeCapo (stringIndex);

    // Real guitars go progressively sharp up the neck: pressing the string down
    // stretches it. The slope is per-string and adjustable.
    const double intonation = (s.intonationSlope + s.agingIntonationSlope) * juce::jmax (0.0, absoluteFret);

    const double totalCents = bendCents + intonation;

    const double fretRatio = temperamentRatio (absoluteFret);

    const double hz = openHz * fretRatio * centsToRatio (totalCents);

    return juce::jlimit (constants::kMinStringHz, 12000.0, hz);
}

double TuningEngine::frequencyToFretPosition (int stringIndex, double hz) const noexcept
{
    /*  Solved in absolute frets - from the nut, ignoring the capo - because that
        is the coordinate computeFrequency works in, and inverting it in any other
        one would not round-trip under an unequal temperament. The capo is taken
        off at the end, so what comes back is a position on the capo'd neck, which
        is what every caller wants. */
    const double openHz = getOpenFrequencyBeforeCapo (stringIndex);

    if (openHz <= 0.0 || hz <= 0.0)
        return -1.0;

    double semis = 12.0 * std::log2 (hz / openHz);

    if (temperament != Temperament::EqualTemp12)
    {
        // Invert the temperament numerically: a handful of Newton steps is exact
        // enough for tuning work and far simpler than inverting each table.
        double guess = semis;

        for (int i = 0; i < 12; ++i)
        {
            const double f = openHz * temperamentRatio (guess);
            const double err = 12.0 * std::log2 (juce::jmax (1.0e-9, hz / juce::jmax (1.0e-9, f)));

            if (std::abs (err) < 1.0e-6)
                break;

            guess += err;
        }

        semis = guess;
    }

    // Undo the intonation error, which itself depends on the fret position.
    const auto& s = getStringTuning (stringIndex);
    const double slope = s.intonationSlope + s.agingIntonationSlope;

    if (std::abs (slope) > 1.0e-9)
        semis -= slope * juce::jmax (0.0, semis) / 100.0;

    return semis - (double) getCapoFretFor (stringIndex);
}

bool TuningEngine::canPlay (int stringIndex, double hz) const noexcept
{
    const double fret = frequencyToFretPosition (stringIndex, hz);

    /*  4.5's "raises effective minimum fret": below the capo is unreachable, and
        the top of the neck comes down to meet it. A note that was playable open
        is not playable with a capo on, which is the point of a capo. */
    return fret >= -0.01
             && fret <= (double) getHighestPlayableFret (stringIndex) + 0.01;
}

//==============================================================================
juce::String TuningEngine::noteName (int midiNote)
{
    const int pc = ((midiNote % 12) + 12) % 12;
    const int octave = (midiNote / 12) - 1;
    return juce::String (kNoteNamesSharp[(size_t) pc]) + juce::String (octave);
}

int TuningEngine::parseNoteName (const juce::String& name)
{
    auto trimmed = name.trim();

    if (trimmed.isEmpty())
        return -1;

    const juce::String letters = "CDEFGAB";
    const int semitoneOf[7] = { 0, 2, 4, 5, 7, 9, 11 };

    const int letterIndex = letters.indexOfChar (juce::CharacterFunctions::toUpperCase (trimmed[0]));

    if (letterIndex < 0)
        return -1;

    int semitone = semitoneOf[letterIndex];
    int pos = 1;

    while (pos < trimmed.length() && (trimmed[pos] == '#' || trimmed[pos] == 'b'
                                      || trimmed[pos] == 's' || trimmed[pos] == 'S'))
    {
        semitone += (trimmed[pos] == 'b') ? -1 : 1;
        ++pos;
    }

    const auto octaveText = trimmed.substring (pos).trim();

    if (octaveText.isEmpty() || ! octaveText.containsOnly ("-0123456789"))
        return -1;

    const int octave = octaveText.getIntValue();

    return (octave + 1) * 12 + semitone;
}

juce::String TuningEngine::describeFrequency (double hz, double concertAHz)
{
    if (hz <= 0.0)
        return "--";

    const double midi = hzToMidi (hz, concertAHz);
    const int nearest = (int) std::round (midi);
    const double cents = (midi - (double) nearest) * 100.0;

    juce::String out = noteName (nearest);

    if (std::abs (cents) >= 0.5)
        out += (cents > 0.0 ? " +" : " ") + juce::String (cents, 1) + "c";

    return out;
}

//==============================================================================
const char* TuningEngine::getTuningPresetName (TuningPreset p) noexcept
{
    switch (p)
    {
        case TuningPreset::Standard:       return "Standard E";
        case TuningPreset::DropD:          return "Drop D";
        case TuningPreset::DropC:          return "Drop C";
        case TuningPreset::DropB:          return "Drop B";
        case TuningPreset::DADGAD:         return "DADGAD";
        case TuningPreset::OpenG:          return "Open G";
        case TuningPreset::OpenD:          return "Open D";
        case TuningPreset::OpenE:          return "Open E";
        case TuningPreset::OpenC:          return "Open C";
        case TuningPreset::HalfStepDown:   return "Half Step Down";
        case TuningPreset::FullStepDown:   return "Full Step Down";
        case TuningPreset::Nashville:      return "Nashville";
        case TuningPreset::SevenString:    return "7-String B";
        case TuningPreset::EightString:    return "8-String F#";
        case TuningPreset::BaritoneB:      return "Baritone B";
        case TuningPreset::BassStandard:   return "Bass Standard";
        case TuningPreset::BassFiveString: return "Bass 5-String";
        case TuningPreset::Custom:         return "Custom";
        case TuningPreset::NumPresets:
        default:                           return "Standard E";
    }
}

const char* TuningEngine::getTemperamentName (Temperament t) noexcept
{
    switch (t)
    {
        case Temperament::EqualTemp12:    return "12-TET";
        case Temperament::JustIntonation: return "Just";
        case Temperament::Meantone:       return "Meantone 1/4";
        case Temperament::Werckmeister3:  return "Werckmeister III";
        case Temperament::Kirnberger3:    return "Kirnberger III";
        case Temperament::Pythagorean:    return "Pythagorean";
        case Temperament::Custom:         return "Custom";
        case Temperament::NumTemperaments:
        default:                          return "12-TET";
    }
}

int TuningEngine::getPresetStringCount (TuningPreset p) noexcept
{
    return kPresets[(size_t) juce::jlimit (0, (int) TuningPreset::NumPresets - 1, (int) p)].count;
}

void TuningEngine::getPresetFrequencies (TuningPreset p, double* dest, int maxCount) noexcept
{
    if (dest == nullptr)
        return;

    const auto& def = kPresets[(size_t) juce::jlimit (0, (int) TuningPreset::NumPresets - 1, (int) p)];

    for (int i = 0; i < juce::jmin (maxCount, def.count); ++i)
        dest[i] = def.f[i];
}

} // namespace luthier
