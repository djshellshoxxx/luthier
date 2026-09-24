#include "Metronome.h"

namespace luthier
{

//==============================================================================
const char* getClickSoundName (ClickSound sound) noexcept
{
    switch (sound)
    {
        case ClickSound::woodBlock:   return "Wood Block";
        case ClickSound::cowbell:     return "Cowbell";
        case ClickSound::digitalBlip: return "Digital Blip";
        case ClickSound::sideStick:   return "Side Stick";
        case ClickSound::shaker:      return "Shaker";
        case ClickSound::tap:         return "Tap";
        case ClickSound::numSounds:
        default:                      return "Wood Block";
    }
}

const char* getClickSubdivisionName (ClickSubdivision s) noexcept
{
    switch (s)
    {
        case ClickSubdivision::quarter:      return "Quarter";
        case ClickSubdivision::eighth:       return "Eighth";
        case ClickSubdivision::triplet:      return "Triplet";
        case ClickSubdivision::sixteenth:    return "Sixteenth";
        case ClickSubdivision::dottedEighth: return "Dotted Eighth";
        case ClickSubdivision::numSubdivisions:
        default:                             return "Quarter";
    }
}

double clicksPerBeat (ClickSubdivision s) noexcept
{
    switch (s)
    {
        case ClickSubdivision::quarter:      return 1.0;
        case ClickSubdivision::eighth:       return 2.0;
        case ClickSubdivision::triplet:      return 3.0;
        case ClickSubdivision::sixteenth:    return 4.0;

        // A dotted eighth is three sixteenths, so it fits four thirds of a time
        // into a beat. It deliberately does not line up with the bar, which is
        // the point of practising against one.
        case ClickSubdivision::dottedEighth: return 4.0 / 3.0;

        case ClickSubdivision::numSubdivisions:
        default:                             return 1.0;
    }
}

//==============================================================================
Metronome::Metronome()
{
    resetAccents();
}

void Metronome::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = juce::jmax (1.0, sampleRate);
    reset();
}

void Metronome::reset() noexcept
{
    clickPosition = 0.0;
    progressiveStartBar = 0.0;

    for (auto& voice : voices)
        voice = Voice {};

    currentBeat.store (0, std::memory_order_relaxed);
    currentBar.store (0, std::memory_order_relaxed);
    barIsSilent.store (false, std::memory_order_relaxed);
    beatPhase.store (0.0, std::memory_order_relaxed);
}

//==============================================================================
void Metronome::setEnabled (bool shouldBeEnabled) noexcept
{
    const bool was = enabled.exchange (shouldBeEnabled, std::memory_order_relaxed);

    // Starting the metronome starts it on beat one, which is the only place a
    // musician expects a count-in to begin.
    if (shouldBeEnabled && ! was)
        clickPosition = 0.0;
}

void Metronome::setTempo (double newBpm) noexcept
{
    bpm.store (juce::jlimit (20.0, 300.0, newBpm), std::memory_order_relaxed);
}

void Metronome::setTimeSignature (int n, int d) noexcept
{
    numerator.store (juce::jlimit (1, kMaxBeatsPerBar, n), std::memory_order_relaxed);

    // practice-tools 1: the denominator is 2, 4, 8 or 16.
    const int clamped = (d == 2 || d == 4 || d == 8 || d == 16) ? d : 4;
    denominator.store (clamped, std::memory_order_relaxed);
}

TimeSignature Metronome::getTimeSignature() const noexcept
{
    TimeSignature signature;
    signature.numerator = numerator.load (std::memory_order_relaxed);
    signature.denominator = denominator.load (std::memory_order_relaxed);
    return signature;
}

void Metronome::setSubdivision (ClickSubdivision s) noexcept
{
    subdivision.store (juce::jlimit (0, (int) ClickSubdivision::numSubdivisions - 1, (int) s),
                       std::memory_order_relaxed);
}

void Metronome::setLevelDb (double db) noexcept
{
    levelDb.store (juce::jlimit (-60.0, 6.0, db), std::memory_order_relaxed);
}

void Metronome::setSubdivisionLevelDb (double db) noexcept
{
    subdivisionLevelDb.store (juce::jlimit (-60.0, 6.0, db), std::memory_order_relaxed);
}

//==============================================================================
void Metronome::setBeatAccent (int beat, BeatAccent accent) noexcept
{
    if (juce::isPositiveAndBelow (beat, kMaxBeatsPerBar))
        accents[(size_t) beat].store ((int) accent, std::memory_order_relaxed);
}

BeatAccent Metronome::getBeatAccent (int beat) const noexcept
{
    return juce::isPositiveAndBelow (beat, kMaxBeatsPerBar)
             ? (BeatAccent) accents[(size_t) beat].load (std::memory_order_relaxed)
             : BeatAccent::normal;
}

void Metronome::resetAccents() noexcept
{
    for (int i = 0; i < kMaxBeatsPerBar; ++i)
        accents[(size_t) i].store ((int) (i == 0 ? BeatAccent::accent : BeatAccent::normal),
                                   std::memory_order_relaxed);
}

void Metronome::setSilentBarPeriod (int everyNBars) noexcept
{
    silentBarPeriod.store (juce::jlimit (0, kMaxSilentBarPeriod, everyNBars),
                           std::memory_order_relaxed);
}

//==============================================================================
void Metronome::startProgressiveTempo (double fromBpm, double toBpm, int overBars) noexcept
{
    progressiveFrom.store (juce::jlimit (20.0, 300.0, fromBpm), std::memory_order_relaxed);
    progressiveTo.store (juce::jlimit (20.0, 300.0, toBpm), std::memory_order_relaxed);
    progressiveBars.store (juce::jmax (1, overBars), std::memory_order_relaxed);

    const auto signature = getTimeSignature();
    const double clicksPerBar = signature.beatsPerBar()
                                  * clicksPerBeat (getSubdivision());

    progressiveStartBar = (clicksPerBar > 0.0) ? (clickPosition / clicksPerBar) : 0.0;

    setTempo (fromBpm);
    progressive.store (true, std::memory_order_relaxed);
}

void Metronome::stopProgressiveTempo() noexcept
{
    progressive.store (false, std::memory_order_relaxed);
}

//==============================================================================
void Metronome::triggerClick (double frequencyHz, double decaySeconds, double gain,
                              ClickSound clickSound) noexcept
{
    // Find a free voice, or steal the quietest one. Stealing rather than dropping
    // matters at fast subdivisions, where a dropped click reads as a missed beat.
    int chosen = -1;
    double quietest = 1.0e9;

    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (! voices[(size_t) i].active)
        {
            chosen = i;
            break;
        }

        const double loudness = voices[(size_t) i].envelope * voices[(size_t) i].gain;

        if (loudness < quietest)
        {
            quietest = loudness;
            chosen = i;
        }
    }

    if (chosen < 0)
        return;

    auto& voice = voices[(size_t) chosen];

    voice.active = true;
    voice.sound = clickSound;
    voice.phase = 0.0;
    voice.phaseIncrement = 2.0 * juce::MathConstants<double>::pi * frequencyHz / sr;
    voice.envelope = 1.0;

    // An exponential decay reaching -60 dB at the requested time.
    voice.envelopeDecay = std::exp (-6.9078 / juce::jmax (1.0, decaySeconds * sr));
    voice.gain = gain;
    voice.samplesRemaining = (int) (decaySeconds * sr * 1.5);
    voice.z1 = 0.0;
    voice.z2 = 0.0;
}

double Metronome::renderVoice (Voice& voice) noexcept
{
    if (! voice.active)
        return 0.0;

    double sample = 0.0;

    switch (voice.sound)
    {
        case ClickSound::woodBlock:
        {
            // A wood block is a short pitched knock with a hard edge: a sine
            // plus its own square, which puts odd harmonics on the attack.
            const double fundamental = std::sin (voice.phase);
            sample = fundamental * 0.7 + (fundamental > 0.0 ? 0.3 : -0.3) * voice.envelope;
            break;
        }

        case ClickSound::cowbell:
        {
            // Two detuned square-ish tones, which is what makes a cowbell a
            // cowbell rather than a beep.
            const double a = std::sin (voice.phase);
            const double b = std::sin (voice.phase * 1.4854);
            sample = (a + b) * 0.5;
            sample = juce::jlimit (-1.0, 1.0, sample * 1.6);
            break;
        }

        case ClickSound::digitalBlip:
            sample = std::sin (voice.phase);
            break;

        case ClickSound::sideStick:
        {
            // A rim click: a burst of noise through a sharp resonant filter.
            const double noise = rng.nextDouble() * 2.0 - 1.0;
            const double resonance = 0.986;

            voice.z1 = noise + resonance * 2.0 * std::cos (voice.phaseIncrement) * voice.z1
                         - resonance * resonance * voice.z2;
            voice.z2 = voice.z1 * 0.0 + voice.z2;

            // A one-pole difference keeps the filter stable without a full
            // biquad's state, which is all a click needs.
            sample = flushDenormal (voice.z1 * 0.08);
            voice.z2 = voice.z1;
            voice.z1 = sample * 12.5;
            break;
        }

        case ClickSound::shaker:
        {
            // Filtered noise with no pitch at all.
            const double noise = rng.nextDouble() * 2.0 - 1.0;
            voice.z1 = flushDenormal (voice.z1 * 0.55 + noise * 0.45);
            sample = (noise - voice.z1) * 0.8;
            break;
        }

        case ClickSound::tap:
        {
            // The quietest option: a single soft impulse, nearly a fingertip.
            sample = std::sin (voice.phase) * 0.6;
            break;
        }

        case ClickSound::numSounds:
        default:
            sample = std::sin (voice.phase);
            break;
    }

    sample *= voice.envelope * voice.gain;

    voice.phase += voice.phaseIncrement;

    if (voice.phase > 2.0 * juce::MathConstants<double>::pi)
        voice.phase -= 2.0 * juce::MathConstants<double>::pi;

    voice.envelope = flushDenormal (voice.envelope * voice.envelopeDecay);

    if (--voice.samplesRemaining <= 0 || voice.envelope < 1.0e-5)
        voice.active = false;

    return sanitise (sample);
}

//==============================================================================
void Metronome::fireClickFor (int beatInBar, bool onBeat) noexcept
{
    fireClick (onBeat ? getBeatAccent (beatInBar) : BeatAccent::ghost);
}

void Metronome::fireClick (BeatAccent accent) noexcept
{
    if (accent == BeatAccent::silent)
        return;

    const auto clickSound = getSound();

    // The accented beat is pitched higher as well as louder, because a metronome
    // that only changes level is hard to follow in a loud room.
    double frequency = 0.0;
    double decay = 0.0;

    switch (clickSound)
    {
        case ClickSound::woodBlock:   frequency = 1200.0; decay = 0.030; break;
        case ClickSound::cowbell:     frequency = 540.0;  decay = 0.120; break;
        case ClickSound::digitalBlip: frequency = 1800.0; decay = 0.020; break;
        case ClickSound::sideStick:   frequency = 900.0;  decay = 0.035; break;
        case ClickSound::shaker:      frequency = 6000.0; decay = 0.040; break;
        case ClickSound::tap:         frequency = 700.0;  decay = 0.018; break;
        case ClickSound::numSounds:
        default:                      frequency = 1000.0; decay = 0.025; break;
    }

    double gain = dbToGain (getLevelDb());

    switch (accent)
    {
        case BeatAccent::accent:
            frequency *= 1.5;
            break;

        case BeatAccent::ghost:
            frequency *= 0.85;
            gain *= dbToGain (subdivisionLevelDb.load (std::memory_order_relaxed));
            break;

        case BeatAccent::normal:
            gain *= 0.72;
            break;

        case BeatAccent::silent:
        case BeatAccent::numLevels:
        default:
            return;
    }

    triggerClick (frequency, decay, gain, clickSound);
}

//==============================================================================
int Metronome::renderClicksAt (float* destination, int numSamples,
                               const int* offsets, const bool* downbeats, int count) noexcept
{
    if (destination == nullptr || numSamples <= 0)
        return 0;

    int next = 0, fired = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        // The offsets are in order; a stray one before this sample fires now.
        while (next < count && offsets[next] <= i)
        {
            fireClick (downbeats[next] ? BeatAccent::accent : BeatAccent::normal);
            ++next;
            ++fired;
        }

        double sample = 0.0;

        for (auto& voice : voices)
            sample += renderVoice (voice);

        destination[i] = (float) sanitise (sample);
    }

    return fired;
}

int Metronome::processBlock (float* destination, int numSamples) noexcept
{
    if (destination == nullptr || numSamples <= 0)
        return 0;

    juce::FloatVectorOperations::clear (destination, numSamples);

    // practice-tools 0.1: a closed practice tool costs nothing. A disabled
    // metronome still has to render any click already in flight, or switching it
    // off mid-click would produce the pop the whole design avoids.
    const bool running = enabled.load (std::memory_order_relaxed);

    const auto signature = getTimeSignature();
    const auto division = getSubdivision();

    const double perBeat = clicksPerBeat (division);
    const double clicksPerBar = signature.beatsPerBar() * perBeat;

    if (running && clicksPerBar > 0.0)
    {
        // ---- progressive tempo ----------------------------------------------------
        if (progressive.load (std::memory_order_relaxed))
        {
            const double barNow = clickPosition / clicksPerBar;
            const double barsElapsed = barNow - progressiveStartBar;
            const double overBars = (double) progressiveBars.load (std::memory_order_relaxed);

            const double t = juce::jlimit (0.0, 1.0, barsElapsed / juce::jmax (1.0, overBars));

            const double from = progressiveFrom.load (std::memory_order_relaxed);
            const double to = progressiveTo.load (std::memory_order_relaxed);

            setTempo (from + (to - from) * t);

            if (t >= 1.0)
                progressive.store (false, std::memory_order_relaxed);
        }

        // ---- the grid --------------------------------------------------------------
        const double tempo = getTempo();

        // Clicks per second, which is the only rate this needs.
        const double clicksPerSecond = tempo / 60.0 * perBeat;
        const double increment = clicksPerSecond / sr;

        const int period = silentBarPeriod.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            const double before = clickPosition;
            clickPosition += increment;

            // A click fires when the position crosses an integer. Comparing the
            // floors either side of the step catches it exactly once, at the
            // sample it actually lands on.
            if (std::floor (clickPosition) > std::floor (before))
            {
                const double clickIndex = std::floor (clickPosition);

                const int bar = (int) std::floor (clickIndex / clicksPerBar);
                const double intoBar = clickIndex - (double) bar * clicksPerBar;

                // Is this click on a beat, or between them?
                const double beatFloat = intoBar / perBeat;
                const int beat = (int) std::floor (beatFloat + 1.0e-9);
                const bool onBeat = std::abs (beatFloat - (double) beat) < 1.0e-6;

                /*  practice-tools 1: every Nth bar is silent, so the player has
                    to keep time themselves. The bar still advances and the
                    indicator still moves - it is the click that stops, not the
                    metronome. */
                const bool silentBar = (period > 1) && (((bar + 1) % period) == 0);

                barIsSilent.store (silentBar, std::memory_order_relaxed);
                currentBar.store (bar, std::memory_order_relaxed);
                currentBeat.store (juce::jlimit (0, kMaxBeatsPerBar - 1, beat),
                                   std::memory_order_relaxed);

                if (! silentBar)
                    fireClickFor (beat, onBeat);
            }

            double sample = 0.0;

            for (auto& voice : voices)
                sample += renderVoice (voice);

            destination[i] = (float) sanitise (sample);
        }

        // Where the beat indicator sits, for the UI.
        {
            const double clickIntoBeat = std::fmod (clickPosition, perBeat);
            beatPhase.store (juce::jlimit (0.0, 1.0, clickIntoBeat / perBeat),
                             std::memory_order_relaxed);
        }
    }
    else
    {
        // Not running: render whatever is still decaying and nothing else.
        for (int i = 0; i < numSamples; ++i)
        {
            double sample = 0.0;

            for (auto& voice : voices)
                sample += renderVoice (voice);

            destination[i] = (float) sanitise (sample);
        }
    }

    int firing = 0;

    for (const auto& voice : voices)
        if (voice.active)
            ++firing;

    return firing;
}

//==============================================================================
juce::var Metronome::toVar() const
{
    auto* root = new juce::DynamicObject();

    const auto signature = getTimeSignature();

    root->setProperty ("enabled", isEnabled());
    root->setProperty ("bpm", getTempo());
    root->setProperty ("numerator", signature.numerator);
    root->setProperty ("denominator", signature.denominator);
    root->setProperty ("subdivision", (int) getSubdivision());
    root->setProperty ("sound", (int) getSound());
    root->setProperty ("levelDb", getLevelDb());
    root->setProperty ("subdivisionLevelDb", subdivisionLevelDb.load (std::memory_order_relaxed));
    root->setProperty ("silentBarPeriod", getSilentBarPeriod());

    juce::Array<juce::var> accentArray;

    for (int i = 0; i < kMaxBeatsPerBar; ++i)
        accentArray.add ((int) getBeatAccent (i));

    root->setProperty ("accents", accentArray);

    return { root };
}

void Metronome::fromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    setEnabled ((bool) root->getProperty ("enabled"));
    setTempo ((double) root->getProperty ("bpm"));
    setTimeSignature ((int) root->getProperty ("numerator"),
                      (int) root->getProperty ("denominator"));

    setSubdivision ((ClickSubdivision) juce::jlimit (
        0, (int) ClickSubdivision::numSubdivisions - 1, (int) root->getProperty ("subdivision")));

    sound.store (juce::jlimit (0, (int) ClickSound::numSounds - 1,
                               (int) root->getProperty ("sound")),
                 std::memory_order_relaxed);

    setLevelDb ((double) root->getProperty ("levelDb"));

    if (root->hasProperty ("subdivisionLevelDb"))
        setSubdivisionLevelDb ((double) root->getProperty ("subdivisionLevelDb"));

    setSilentBarPeriod ((int) root->getProperty ("silentBarPeriod"));

    if (const auto* accentArray = root->getProperty ("accents").getArray())
        for (int i = 0; i < juce::jmin (kMaxBeatsPerBar, accentArray->size()); ++i)
            setBeatAccent (i, (BeatAccent) juce::jlimit (
                0, (int) BeatAccent::numLevels - 1, (int) (*accentArray)[i]));
}

} // namespace luthier
