#include "JamBassVoice.h"
#include "../../Model/Guitar/StringMaterials.h"

namespace luthier
{

const char* getJamBassVoiceName (int kind) noexcept
{
    static const char* const names[] = { "Finger", "Pick", "Muted Pick", "Upright" };
    return names[juce::jlimit (0, 3, kind)];
}

//==============================================================================
void JamBassTone::prepare (double sampleRate)
{
    sr = sampleRate;

    // Room for the longest comb delay: 0.21 x 2^(7/12) of an E1 period at any rate.
    combLine.assign ((size_t) juce::jmax (64, (int) (sr * 0.02) + 8), 0.0);
    combWrite = 0;

    pickupResonance.setLowpass (sr, 4500.0, 1.2);
    bodyA.setPeaking (sr, 95.0, 4.0, 8.0);
    bodyB.setPeaking (sr, 180.0, 4.0, 6.0);
    dc.prepare (sr, 7.0);
    setTone (tone);
    reset();
}

void JamBassTone::reset() noexcept
{
    std::fill (combLine.begin(), combLine.end(), 0.0);
    combWrite = 0;

    for (auto* f : { &pickupResonance, &lowBand, &midBand, &highBand, &bodyA, &bodyB })
        f->reset();

    upStage.reset();
    downStage.reset();
    dc.reset();
}

void JamBassTone::setUpright (bool isUpright) noexcept
{
    upright = isUpright;
}

void JamBassTone::setTone (double tone01) noexcept
{
    tone = juce::jlimit (0.0, 1.0, tone01);

    // jam_bass_tone: 0 dark and round, 0.5 flat, 1 bright and scooped.
    const double t = tone - 0.5;
    lowBand.setLowShelf (sr, 120.0, 0.8, -4.0 * t);
    midBand.setPeaking (sr, 600.0, 0.8, -5.0 * t);
    highBand.setHighShelf (sr, 2200.0, 0.8, 16.0 * t);
}

void JamBassTone::setNote (double hz, int fret) noexcept
{
    const double period = sr / juce::jmax (20.0, hz);
    combDelay = juce::jlimit (1.0, (double) combLine.size() - 2.0, 0.21 * std::pow (2.0, fret / 12.0) * period);
}

double JamBassTone::process (double x) noexcept
{
    double y = x;

    if (upright)
    {
        // No pickup: the body's two lowest modes.
        y = bodyB.process (bodyA.process (y));
    }
    else
    {
        // The pickup's position comb, then its electrical resonance.
        const int size = (int) combLine.size();
        combLine[(size_t) combWrite] = y;

        const double read = (double) combWrite - combDelay;
        const double wrapped = read < 0.0 ? read + (double) size : read;
        const int i0 = (int) wrapped;
        const double frac = wrapped - (double) i0;
        const double delayed = combLine[(size_t) i0] * (1.0 - frac) + combLine[(size_t) ((i0 + 1) % size)] * frac;

        combWrite = (combWrite + 1) % size;
        y = pickupResonance.process (y - 0.85 * delayed);
    }

    y = highBand.process (midBand.process (lowBand.process (y)));

    // Tube saturation at 2x (engine.md 0.10): up, shape, down.
    double u0 = 0.0, u1 = 0.0;
    upStage.up (y, u0, u1);
    u0 = tubeShape (u0 * 1.2, 0.2) / 1.2;
    u1 = tubeShape (u1 * 1.2, 0.2) / 1.2;
    y = downStage.down (u0, u1);

    return juce::jlimit (-4.0, 4.0, sanitise (dc.process (y)));
}

//==============================================================================
constexpr int JamBassVoice::kOpenNotes[JamBassVoice::kNumStrings];

void JamBassVoice::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;

    for (int s = 0; s < kNumStrings; ++s)
    {
        // StringMaterials counts from the highest string: G is 0, E is 3.
        const int index = kNumStrings - 1 - s;
        const auto spec = StringMaterials::computeSpec (StringMaterial::NickelPlatedSteel, StringGauge::BassStandard,
                                                        StringAge::BrokenIn, index,
                                                        midiToHz ((double) kOpenNotes[s]), kScaleMm);
        physics[(size_t) s] = StringMaterials::toPhysical (spec, kScaleMm);
    }

    for (int i = 0; i < 2; ++i)
    {
        strings[(size_t) i].prepare (sr, maxBlockSize);
        strings[(size_t) i].setIndex (40 + i);
        strings[(size_t) i].setPhysical (physics[1]);
    }

    tone.prepare (sr);
    setVoice (voice);
    reset();
}

void JamBassVoice::reset() noexcept
{
    for (auto& s : strings)
        s.reset();

    tone.reset();
    active = 0;
    currentNote = -1;
    lastString = 1;
    lastFret = 0;
    sounding = false;
    fadeGain = 1.0;
    fading = false;
}

void JamBassVoice::setVoice (JamBassVoiceKind kind) noexcept
{
    voice = kind;
    tone.setUpright (kind == JamBassVoiceKind::upright);

    // Upright: higher loop damping - a flesh pluck on gut or flats dies sooner.
    for (auto& s : strings)
        s.setSustainScale (kind == JamBassVoiceKind::upright ? 0.35 : 1.0);
}

void JamBassVoice::chooseString (int note, int previousFret, int& string, int& fret) noexcept
{
    int bestString = -1, bestFret = 0, bestDistance = 1 << 20;

    // The lowest fret at or below 7 nearest the previous position.
    for (int s = 0; s < kNumStrings; ++s)
    {
        const int f = note - kOpenNotes[s];

        if (f < 0 || f > 7)
            continue;

        const int distance = std::abs (f - previousFret);

        if (distance < bestDistance || (distance == bestDistance && f < bestFret))
        {
            bestString = s;
            bestFret = f;
            bestDistance = distance;
        }
    }

    if (bestString < 0)
    {
        // Nothing at or below the seventh fret: the lowest fret there is.
        bestString = 0;
        bestFret = juce::jmax (0, note - kOpenNotes[0]);

        for (int s = kNumStrings - 1; s >= 0; --s)
        {
            if (note - kOpenNotes[s] >= 0)
            {
                bestString = s;
                bestFret = note - kOpenNotes[s];
                break;
            }
        }
    }

    string = bestString;
    fret = bestFret;
}

void JamBassVoice::noteOn (int midiNote, double velocity, bool ghost) noexcept
{
    velocity = juce::jlimit (0.0, 1.0, velocity);

    int s = 0, f = 0;
    chooseString (midiNote, lastFret, s, f);

    const int idle = 1 - active;
    auto& next = strings[(size_t) idle];

    // The previous note: the finger lifts.
    if (sounding)
        strings[(size_t) active].setDamping (StringEngine::Damping::Released, 1.0);

    const double hz = midiToHz ((double) midiNote);

    next.setPhysical (physics[(size_t) s]);
    next.snapToFrequency (hz);

    const bool muted = ghost || voice == JamBassVoiceKind::mutedPick;
    next.setDamping (muted ? StringEngine::Damping::PalmMuteBass : StringEngine::Damping::Open,
                     ghost ? 1.0 : 0.8);

    Excitation::Params p;
    p.kind = Excitation::Kind::Pluck;
    p.velocity = ghost ? velocity * 0.4 : velocity;
    p.delaySamples = sr / hz;

    switch (voice)
    {
        case JamBassVoiceKind::finger:    p.material = Excitation::Material::Fingertip;     p.pluckPosition = 0.20; p.brightness = 0.4; break;
        case JamBassVoiceKind::pick:
        case JamBassVoiceKind::mutedPick: p.material = Excitation::Material::PickCelluloid; p.pluckPosition = 0.12; p.brightness = 0.6; break;
        case JamBassVoiceKind::upright:   p.material = Excitation::Material::Fingertip;     p.pluckPosition = 0.18; p.brightness = 0.25; break;
        case JamBassVoiceKind::numKinds:  break;
    }

    next.excite (p);
    tone.setNote (hz, f);

    active = idle;
    currentNote = midiNote;
    lastString = s;
    lastFret = f;
    sounding = true;
    fading = false;
    fadeGain = 1.0;
}

void JamBassVoice::noteOff() noexcept
{
    if (! sounding)
        return;

    strings[(size_t) active].setDamping (StringEngine::Damping::Released, 1.0);
    sounding = false;
}

void JamBassVoice::choke (double seconds) noexcept
{
    fadeStep = 1.0 / juce::jmax (1.0, seconds * sr);
    fading = true;
    sounding = false;
}

bool JamBassVoice::isSounding() const noexcept
{
    return sounding;
}

void JamBassVoice::render (double* out, int n) noexcept
{
    double peak = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double s = strings[0].processSample (0.0) + strings[1].processSample (0.0);
        double y = tone.process (s * 0.6);

        if (fading)
        {
            fadeGain -= fadeStep;

            if (fadeGain <= 0.0)
            {
                for (auto& st : strings)
                    st.reset();

                tone.reset();
                fadeGain = 1.0;
                fading = false;
                y = 0.0;
            }
            else
            {
                y *= fadeGain;
            }
        }

        out[i] = y;
        peak = juce::jmax (peak, std::abs (y));
    }

    lastPeak = peak;
}

} // namespace luthier
