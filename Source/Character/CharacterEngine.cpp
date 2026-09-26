#include "CharacterEngine.h"

namespace luthier
{

//==============================================================================
const char* getTemperatureName (Temperature t) noexcept
{
    switch (t)
    {
        case Temperature::cold: return "Cold";
        case Temperature::room: return "Room";
        case Temperature::warm: return "Warm";
        case Temperature::numTemperatures:
        default:                return "Room";
    }
}

const char* getHumidityName (Humidity h) noexcept
{
    switch (h)
    {
        case Humidity::dry:    return "Dry";
        case Humidity::normal: return "Normal";
        case Humidity::humid:  return "Humid";
        case Humidity::numHumidities:
        default:               return "Normal";
    }
}

//==============================================================================
double DeadSpot::weightAt (double fretPosition) const noexcept
{
    const double distance = fretPosition - (double) fret;
    const double sigma = juce::jmax (0.5, width * 0.5);

    return std::exp (-(distance * distance) / (2.0 * sigma * sigma));
}

//==============================================================================
CharacterEngine::CharacterEngine()
{
    generate();
}

void CharacterEngine::prepare (double sampleRate, int strings) noexcept
{
    sr = juce::jmax (1.0, sampleRate);
    numStrings = juce::jlimit (1, kMaxStrings, strings);

    reset();
}

void CharacterEngine::reset() noexcept
{
    // A reset returns the instrument to how it was at the start of a session:
    // in tune, and with no dropout in progress. The instance's character - its
    // dead spots, its wear - is not touched, because that is what makes it this
    // instrument rather than a different one.
    retune();

    jackGain = 1.0;
    jackDropoutRemaining = 0.0;

    rng.setSeed (seed ^ 0xA5A5A5A5ull);
    jackSecondsToNext = 30.0 + rng.nextDouble() * 60.0;
}

void CharacterEngine::setNumStrings (int n) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
}

//==============================================================================
double CharacterEngine::hashed (int category, int a, int b) const noexcept
{
    /*  SplitMix64 over the seed and the coordinates.

        Mixing the coordinates into the seed rather than drawing from a running
        generator is what makes rule 1 hold: every value is a pure function of
        (seed, category, a, b), so it does not matter what order anything is
        asked for in, or whether some values are never asked for at all.
    */
    uint64_t x = seed;

    x ^= (uint64_t) (category + 1) * 0x9E3779B97F4A7C15ull;
    x ^= (uint64_t) (a + 1) * 0xBF58476D1CE4E5B9ull;
    x ^= (uint64_t) (b + 1) * 0x94D049BB133111EBull;

    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    x = x ^ (x >> 31);

    // The top 53 bits, which is what a double can hold exactly.
    return (double) (x >> 11) / (double) (1ull << 53);
}

//==============================================================================
void CharacterEngine::setSeed (uint64_t newSeed)
{
    seed = newSeed != 0 ? newSeed : 0x5EEDC0DEull;
    generate();
    reset();
}

void CharacterEngine::reroll()
{
    // A new instrument, from the clock. Saved into the preset by toVar, so the
    // roll is kept rather than repeated on every load.
    setSeed ((uint64_t) juce::Time::getHighResolutionTicks()
               ^ (uint64_t) juce::Random::getSystemRandom().nextInt64());
}

void CharacterEngine::generate()
{
    // ---- dead spots (character-wear 2) ---------------------------------------------
    for (int s = 0; s < kMaxStrings; ++s)
    {
        // Zero to three per string.
        const int count = (int) (hashed (0, s) * 3.999);
        numDeadSpots[(size_t) s] = juce::jlimit (0, kMaxDeadSpotsPerString, count);

        for (int i = 0; i < kMaxDeadSpotsPerString; ++i)
        {
            auto& spot = deadSpots[(size_t) s][(size_t) i];

            /*  Frets 6 to 12, from a beta-ish distribution.

                character-wear 2 asks for a beta centred there, which is where
                dead spots cluster on a real neck - the neck's own first bending
                mode lands around the seventh fret on most scale lengths. The
                sum of two uniforms is a triangular distribution peaked at the
                middle, which is the same shape for this purpose and needs no
                special function.
            */
            const double u = (hashed (1, s, i) + hashed (2, s, i)) * 0.5;

            spot.fret = juce::jlimit (1, kMaxFrets, 4 + (int) (u * 10.0));
            spot.depth = 0.1 + hashed (3, s, i) * 0.6;       // 0.1 to 0.7
            spot.width = 2.0 + hashed (4, s, i) * 3.0;       // 2 to 5 frets
        }
    }

    // ---- fret wear (character-wear 3) -------------------------------------------------
    for (int fret = 0; fret <= kMaxFrets; ++fret)
    {
        /*  Heavier at frets 1-5 and 12-17, which is where hands actually go:
            open-position chords at one end, and the octave position at the
            other. Everything else gets the base rate. */
        double bias = 0.35;

        if (fret >= 1 && fret <= 5)
            bias = 0.85;
        else if (fret >= 12 && fret <= 17)
            bias = 0.7;
        else if (fret >= 6 && fret <= 11)
            bias = 0.5;

        fretWear[(size_t) fret] = juce::jlimit (0.0, 1.0, bias * hashed (5, fret));
    }

    // ---- tuner drift (character-wear 4) ------------------------------------------------
    for (int s = 0; s < kMaxStrings; ++s)
    {
        driftPhase[(size_t) s] = hashed (6, s) * juce::MathConstants<double>::twoPi;

        // A period between twenty and ninety seconds, as the spec asks.
        const double period = 20.0 + hashed (7, s) * 70.0;
        driftRate[(size_t) s] = juce::MathConstants<double>::twoPi / period;

        // Up to five cents at full looseness.
        driftAmplitude[(size_t) s] = 0.4 + hashed (8, s) * 0.6;

        driftCents[(size_t) s] = 0.0;
    }

    // ---- electronics (character-wear 5) -------------------------------------------------
    capacitorDrift = 1.0 + (hashed (9, 0) * 2.0 - 1.0) * capacitorDriftRange;

    // ---- balance (character-wear 5, 6, 7) -------------------------------------------------
    for (int s = 0; s < kMaxStrings; ++s)
    {
        for (int pickup = 0; pickup < 3; ++pickup)
        {
            // Plus or minus 1.5 dB, per character-wear 6.
            pickupBalanceDb[(size_t) s][(size_t) pickup] =
                (hashed (10, s, pickup) * 2.0 - 1.0) * 1.5;
        }

        // Plus or minus 2 dB per saddle, per character-wear 5.
        saddleBalanceDb[(size_t) s] = (hashed (11, s) * 2.0 - 1.0) * 2.0;

        // Nut slot wear: a small damping on the open string.
        nutDamping[(size_t) s] = hashed (12, s) * 0.08;

        // Plus or minus 0.2 mm, per character-wear 7.
        saddleHeightMm[(size_t) s] = (hashed (13, s) * 2.0 - 1.0) * 0.2;
    }
}

//==============================================================================
void CharacterEngine::setAmount (double newAmount) noexcept
{
    amount.store (juce::jlimit (0.0, 1.0, newAmount), std::memory_order_relaxed);
}

//==============================================================================
int CharacterEngine::getNumDeadSpots (int stringIndex) const noexcept
{
    return juce::isPositiveAndBelow (stringIndex, kMaxStrings)
             ? numDeadSpots[(size_t) stringIndex] : 0;
}

DeadSpot CharacterEngine::getDeadSpot (int stringIndex, int index) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings)
          || ! juce::isPositiveAndBelow (index, kMaxDeadSpotsPerString))
        return {};

    return deadSpots[(size_t) stringIndex][(size_t) index];
}

void CharacterEngine::setDeadSpot (int stringIndex, int index, const DeadSpot& spot)
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings)
          || ! juce::isPositiveAndBelow (index, kMaxDeadSpotsPerString))
        return;

    auto& target = deadSpots[(size_t) stringIndex][(size_t) index];

    target.fret = juce::jlimit (0, kMaxFrets, spot.fret);
    target.depth = juce::jlimit (0.0, 0.7, spot.depth);
    target.width = juce::jlimit (1.0, 8.0, spot.width);

    // Editing a spot beyond the generated count brings it into play, which is
    // what the panel's drag-to-add gesture means.
    numDeadSpots[(size_t) stringIndex] =
        juce::jmax (numDeadSpots[(size_t) stringIndex], index + 1);
}

double CharacterEngine::getSustainMultiplier (int stringIndex, double fretPosition,
                                              double noteHz, double bodyResonanceHz) const noexcept
{
    if (! isEnabled() || ! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return 1.0;

    const double intensity = getAmount();

    if (intensity <= 0.0)
        return 1.0;

    double loss = 0.0;

    for (int i = 0; i < numDeadSpots[(size_t) stringIndex]; ++i)
    {
        const auto& spot = deadSpots[(size_t) stringIndex][(size_t) i];

        loss = juce::jmax (loss, spot.depth * spot.weightAt (fretPosition));
    }

    /*  character-wear 2: dead spots interact with the body.

        A dead spot is the neck's bending mode taking energy out of the string,
        and the neck is coupled to the body. A note close to the body's air
        resonance therefore loses more, because there is somewhere for the energy
        to go. Notes far from it lose the base amount.
    */
    if (noteHz > 0.0 && bodyResonanceHz > 0.0)
    {
        const double octaves = std::abs (std::log2 (noteHz / bodyResonanceHz));

        // Full effect at the resonance, falling to two thirds an octave away.
        const double proximity = std::exp (-octaves * octaves * 1.2);

        loss *= 0.66 + 0.34 * proximity;
    }

    return juce::jlimit (0.2, 1.0, 1.0 - loss * intensity);
}

//==============================================================================
double CharacterEngine::getFretWear (int fret) const noexcept
{
    return juce::isPositiveAndBelow (fret, kMaxFrets + 1)
             ? fretWear[(size_t) fret] : 0.0;
}

void CharacterEngine::setFretWear (int fret, double wear)
{
    if (juce::isPositiveAndBelow (fret, kMaxFrets + 1))
        fretWear[(size_t) fret] = juce::jlimit (0.0, 1.0, wear);
}

void CharacterEngine::refret()
{
    fretWear.fill (0.0);
}

double CharacterEngine::getFretDetuneCents (double fretPosition) const noexcept
{
    if (! isEnabled())
        return 0.0;

    const int fret = juce::jlimit (0, kMaxFrets, (int) std::round (fretPosition));

    // A worn crown is flatter, so the string is stopped a fraction further from
    // the nut than it should be, and the note goes very slightly flat.
    return -getFretWear (fret) * 3.0 * getAmount();
}

double CharacterEngine::getFretBuzzMultiplier (double fretPosition) const noexcept
{
    if (! isEnabled())
        return 1.0;

    const int fret = juce::jlimit (0, kMaxFrets, (int) std::round (fretPosition));

    // A worn fret sits lower than its neighbours, so the string is closer to the
    // one in front of it.
    return 1.0 + getFretWear (fret) * 1.5 * getAmount();
}

double CharacterEngine::getFretSustainMultiplier (double fretPosition) const noexcept
{
    if (! isEnabled())
        return 1.0;

    const int fret = juce::jlimit (0, kMaxFrets, (int) std::round (fretPosition));

    return juce::jlimit (0.85, 1.0, 1.0 - getFretWear (fret) * 0.12 * getAmount());
}

//==============================================================================
void CharacterEngine::setTunerLooseness (double percent) noexcept
{
    tunerLooseness.store (juce::jlimit (0.0, 100.0, percent), std::memory_order_relaxed);
}

void CharacterEngine::advance (double secondsElapsed) noexcept
{
    if (secondsElapsed <= 0.0)
        return;

    sessionSeconds += secondsElapsed;

    if (! isEnabled())
        return;

    const double looseness = getTunerLooseness() * 0.01;
    const double intensity = getAmount();

    // character-wear 4: up to five cents at full looseness.
    const double scale = looseness * intensity * 5.0;

    for (int s = 0; s < numStrings; ++s)
    {
        driftPhase[(size_t) s] += driftRate[(size_t) s] * secondsElapsed;

        if (driftPhase[(size_t) s] > juce::MathConstants<double>::twoPi)
            driftPhase[(size_t) s] -= juce::MathConstants<double>::twoPi;

        /*  character-wear 9: the environment envelopes the drift over minutes.

            A guitar left under stage lights keeps going out of tune for as long
            as it is warming up, and then settles. The envelope rises over the
            first few minutes of a session and levels off, which is what that
            looks like.
        */
        const double settling = 1.0 - 0.4 * std::exp (-sessionSeconds / 240.0);

        driftCents[(size_t) s] = std::sin (driftPhase[(size_t) s])
                                   * driftAmplitude[(size_t) s] * scale * settling;
    }

    // ---- the intermittent jack (character-wear 5) ---------------------------------
    if (jackIntermittent.load (std::memory_order_relaxed))
    {
        if (jackDropoutRemaining > 0.0)
        {
            jackDropoutRemaining -= secondsElapsed;

            if (jackDropoutRemaining <= 0.0)
            {
                jackGain = 1.0;
                jackSecondsToNext = 30.0 + rng.nextDouble() * 90.0;
            }
        }
        else
        {
            jackSecondsToNext -= secondsElapsed;

            if (jackSecondsToNext <= 0.0)
            {
                // 20 to 100 ms, per the spec.
                jackDropoutRemaining = 0.02 + rng.nextDouble() * 0.08;
                jackGain = rng.nextDouble() * 0.15;
            }
        }
    }
    else
    {
        jackGain = 1.0;
        jackDropoutRemaining = 0.0;
    }
}

double CharacterEngine::getTunerDriftCents (int stringIndex) const noexcept
{
    if (! isEnabled() || ! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return 0.0;

    // environment.md 1: the temperature offset is EnvironmentModel's now.
    return driftCents[(size_t) stringIndex];
}

void CharacterEngine::retune() noexcept
{
    driftCents.fill (0.0);
    sessionSeconds = 0.0;

    // The phases are reset to where the sine is zero, so the drift starts from
    // in-tune rather than jumping there and then immediately moving.
    for (int s = 0; s < kMaxStrings; ++s)
        driftPhase[(size_t) s] = 0.0;
}

void CharacterEngine::retuneString (int stringIndex) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
    {
        driftCents[(size_t) stringIndex] = 0.0;
        driftPhase[(size_t) stringIndex] = 0.0;
    }
}

//==============================================================================
void CharacterEngine::setPotLinearityAmount (double newAmount) noexcept
{
    potLinearity.store (juce::jlimit (0.0, 1.0, newAmount), std::memory_order_relaxed);
}

double CharacterEngine::applyPotTaper (double position) const noexcept
{
    const double p = juce::jlimit (0.0, 1.0, position);

    if (! isEnabled())
        return p;

    const double strength = getPotLinearityAmount() * getAmount();

    if (strength <= 0.0)
        return p;

    /*  An aged pot's taper (character-wear 5).

        A carbon track wears where the wiper spends most of its life, which on a
        guitar volume control is the top of the sweep. The result is a curve that
        is close to nominal at the ends - the track is not worn where the wiper
        rarely goes - and sags in the upper middle, so the control gets most of
        its range in the last part of its travel.
    */
    const double sag = std::sin (p * juce::MathConstants<double>::pi) * p;

    return juce::jlimit (0.0, 1.0, p - sag * 0.18 * strength);
}

void CharacterEngine::setCapacitorDriftRange (double fraction) noexcept
{
    capacitorDriftRange = juce::jlimit (0.0, 0.25, fraction);

    // The drift is a function of the seed and the range, so changing the range
    // re-derives it rather than rolling a new one.
    capacitorDrift = 1.0 + (hashed (9, 0) * 2.0 - 1.0) * capacitorDriftRange;
}

//==============================================================================
double CharacterEngine::getPickupBalanceDb (int stringIndex, int pickupSlot) const noexcept
{
    if (! isEnabled() || ! juce::isPositiveAndBelow (stringIndex, kMaxStrings)
          || ! juce::isPositiveAndBelow (pickupSlot, 3))
        return 0.0;

    return pickupBalanceDb[(size_t) stringIndex][(size_t) pickupSlot] * getAmount();
}

double CharacterEngine::getSaddleBalanceDb (int stringIndex) const noexcept
{
    if (! isEnabled() || ! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return 0.0;

    return saddleBalanceDb[(size_t) stringIndex] * getAmount();
}

double CharacterEngine::getNutDamping (int stringIndex) const noexcept
{
    if (! isEnabled() || ! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return 0.0;

    return nutDamping[(size_t) stringIndex] * getAmount();
}

double CharacterEngine::getSaddleHeightOffsetMm (int stringIndex) const noexcept
{
    if (! isEnabled() || ! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return 0.0;

    return saddleHeightMm[(size_t) stringIndex] * getAmount();
}

double CharacterEngine::getNutMaterialDamping() const noexcept
{
    // Bone is harder than any synthetic and damps the string less, which is
    // most of why players change them.
    return isBoneNut() ? 0.94 : 1.06;
}

//==============================================================================
void CharacterEngine::setBodyAge (double age) noexcept
{
    bodyAge.store (juce::jlimit (0.0, 100.0, age), std::memory_order_relaxed);
}

double CharacterEngine::getBodyQMultiplier() const noexcept
{
    if (! isEnabled())
        return 1.0;

    // character-wear 8: the Qs rise slightly as the body breaks in.
    return 1.0 + (getBodyAge() * 0.01) * 0.15 * getAmount();
}

double CharacterEngine::getAirResonanceMultiplier() const noexcept
{
    if (! isEnabled())
        return 1.0;

    // character-wear 8: the air resonance drops 3 to 8 percent at full age.
    return 1.0 - (getBodyAge() * 0.01) * 0.055 * getAmount();
}

double CharacterEngine::getBodyHfDampingMultiplier() const noexcept
{
    if (! isEnabled())
        return 1.0;

    // character-wear 8: high-frequency damping falls 5 to 10 percent - the body
    // "opens up".
    return 1.0 - (getBodyAge() * 0.01) * 0.075 * getAmount();
}

//==============================================================================
void CharacterEngine::setTemperature (Temperature t) noexcept
{
    temperature.store (juce::jlimit (0, (int) Temperature::numTemperatures - 1, (int) t),
                       std::memory_order_relaxed);
}

void CharacterEngine::setHumidity (Humidity h) noexcept
{
    humidity.store (juce::jlimit (0, (int) Humidity::numHumidities - 1, (int) h),
                    std::memory_order_relaxed);
}

//==============================================================================
void CharacterEngine::setAllFresh()
{
    setAmount (0.0);
    refret();
    setTunerLooseness (0.0);
    setPotLinearityAmount (0.0);
    setCapacitorDriftRange (0.0);
    setJackIntermittentEnabled (false);
    setBodyAge (0.0);
    setTemperature (Temperature::room);
    setHumidity (Humidity::normal);
    retune();
}

void CharacterEngine::setAllOld()
{
    setAmount (1.0);
    setTunerLooseness (85.0);
    setPotLinearityAmount (0.9);
    setCapacitorDriftRange (0.2);
    setBodyAge (95.0);

    // The jack stays off even here: character-wear 5 is explicit that it
    // surprises people, and "all old" is an audition preset rather than a
    // request to have the signal cut out.
    setJackIntermittentEnabled (false);

    // Worn frets, following the same pattern the generator uses.
    for (int fret = 0; fret <= kMaxFrets; ++fret)
    {
        double bias = 0.5;

        if (fret >= 1 && fret <= 5)
            bias = 0.95;
        else if (fret >= 12 && fret <= 17)
            bias = 0.85;

        fretWear[(size_t) fret] = juce::jlimit (0.0, 1.0, bias);
    }
}

//==============================================================================
juce::var CharacterEngine::toVar() const
{
    auto* root = new juce::DynamicObject();

    // The seed is the important part: everything generated is recoverable from
    // it, so only what the user has overridden needs storing alongside.
    root->setProperty ("seed", juce::String (seed));
    root->setProperty ("enabled", isEnabled());
    root->setProperty ("amount", getAmount());
    root->setProperty ("tunerLooseness", getTunerLooseness());
    root->setProperty ("potLinearity", getPotLinearityAmount());
    root->setProperty ("capacitorDriftRange", capacitorDriftRange);
    root->setProperty ("jackIntermittent", isJackIntermittentEnabled());
    root->setProperty ("boneNut", isBoneNut());
    root->setProperty ("bodyAge", getBodyAge());
    // environment.md 6: temperature and humidity are env_* parameters now;
    // the old keys are only read, for presets saved before.

    // The fret wear map and the dead spots are stored because the panel lets the
    // user nudge them, and an edit has to survive a reload.
    juce::Array<juce::var> wearArray;

    for (int fret = 0; fret <= kMaxFrets; ++fret)
        wearArray.add (fretWear[(size_t) fret]);

    root->setProperty ("fretWear", wearArray);

    juce::Array<juce::var> spotArray;

    for (int s = 0; s < kMaxStrings; ++s)
    {
        juce::Array<juce::var> perString;

        for (int i = 0; i < numDeadSpots[(size_t) s]; ++i)
        {
            const auto& spot = deadSpots[(size_t) s][(size_t) i];

            auto* entry = new juce::DynamicObject();
            entry->setProperty ("fret", spot.fret);
            entry->setProperty ("depth", spot.depth);
            entry->setProperty ("width", spot.width);

            perString.add (juce::var (entry));
        }

        spotArray.add (juce::var (perString));
    }

    root->setProperty ("deadSpots", spotArray);

    return { root };
}

void CharacterEngine::fromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    // The seed first, because setting it regenerates everything; the stored
    // overrides are then applied on top.
    const auto seedText = root->getProperty ("seed").toString();

    if (seedText.isNotEmpty())
    {
        seed = (uint64_t) seedText.getLargeIntValue();

        if (seed == 0)
            seed = 0x5EEDC0DEull;
    }

    generate();

    setEnabled (root->hasProperty ("enabled") ? (bool) root->getProperty ("enabled") : true);
    setAmount ((double) root->getProperty ("amount"));
    setTunerLooseness ((double) root->getProperty ("tunerLooseness"));
    setPotLinearityAmount ((double) root->getProperty ("potLinearity"));

    if (root->hasProperty ("capacitorDriftRange"))
        setCapacitorDriftRange ((double) root->getProperty ("capacitorDriftRange"));

    setJackIntermittentEnabled ((bool) root->getProperty ("jackIntermittent"));
    setBoneNut (root->hasProperty ("boneNut") ? (bool) root->getProperty ("boneNut") : true);
    setBodyAge ((double) root->getProperty ("bodyAge"));

    // environment.md 6: legacy keys, read for one schema; absent is room / normal.
    setTemperature (root->hasProperty ("temperature")
                      ? (Temperature) juce::jlimit (0, (int) Temperature::numTemperatures - 1,
                                                    (int) root->getProperty ("temperature"))
                      : Temperature::room);

    setHumidity (root->hasProperty ("humidity")
                   ? (Humidity) juce::jlimit (0, (int) Humidity::numHumidities - 1,
                                              (int) root->getProperty ("humidity"))
                   : Humidity::normal);

    if (const auto* wearArray = root->getProperty ("fretWear").getArray())
        for (int fret = 0; fret <= juce::jmin (kMaxFrets, wearArray->size() - 1); ++fret)
            fretWear[(size_t) fret] = juce::jlimit (0.0, 1.0, (double) (*wearArray)[fret]);

    if (const auto* spotArray = root->getProperty ("deadSpots").getArray())
    {
        for (int s = 0; s < juce::jmin (kMaxStrings, spotArray->size()); ++s)
        {
            const auto* perString = (*spotArray)[s].getArray();

            if (perString == nullptr)
                continue;

            numDeadSpots[(size_t) s] = juce::jmin (kMaxDeadSpotsPerString, perString->size());

            for (int i = 0; i < numDeadSpots[(size_t) s]; ++i)
            {
                auto* entry = (*perString)[i].getDynamicObject();

                if (entry == nullptr)
                    continue;

                auto& spot = deadSpots[(size_t) s][(size_t) i];

                spot.fret = juce::jlimit (0, kMaxFrets, (int) entry->getProperty ("fret"));
                spot.depth = juce::jlimit (0.0, 0.7, (double) entry->getProperty ("depth"));
                spot.width = juce::jlimit (1.0, 8.0, (double) entry->getProperty ("width"));
            }
        }
    }

    reset();
}

} // namespace luthier
