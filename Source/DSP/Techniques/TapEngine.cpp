#include "TapEngine.h"

namespace luthier
{

//==============================================================================
TechniqueTriggerConfig TapSettings::triggerConfig() const noexcept
{
    TechniqueTriggerConfig c;
    c.armed = armed;
    c.keyswitches = { TechniqueKeyswitch::tap, -1, -1, -1 };

    switch (source)
    {
        case TapSource::midiChannel:
            // 3: right-hand notes come in on a second channel. They are the
            // tap's and are taken out, so the interpreter never plucks them.
            c.source = TriggerSource::mpeZone;
            c.zoneChannel = juce::jlimit (1, 16, channel);
            c.consumeZoneNotes = true;
            break;

        case TapSource::keyswitch:
            c.source = TriggerSource::keyswitch;
            c.captureNotesWhileHeld = true;
            break;

        case TapSource::fretboard:
        case TapSource::numSources:
        default:
            c.source = TriggerSource::buttonOnly;
            break;
    }

    return c;
}

double TapSettings::strengthFor (double velocity) const noexcept
{
    // 3: "velocity-to-tap-strength mapping. Default linear". A positive curve
    // is a hard touch (strength arrives early), negative a soft one.
    const double v = juce::jlimit (0.0, 1.0, velocity);
    const double exponent = std::pow (3.0, -juce::jlimit (-1.0, 1.0, strengthCurve));
    return juce::jlimit (0.0, 1.0, std::pow (v, exponent));
}

//==============================================================================
void TapEngine::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (1.0, sampleRate);
    reset();
}

void TapEngine::reset() noexcept
{
    numTaps.fill (0);
    fretHandGone.fill (false);
    numEvents = 0;

    for (auto& s : requestState)
        s.store (0);

    for (int i = 0; i < kMaxMarkers; ++i)
    {
        markerFret[(size_t) i].store (-1.0f);
        markerString[(size_t) i].store (-1);
        markerReleased[(size_t) i].store (0);
    }
}

void TapEngine::setSettings (const TapSettings& s) noexcept
{
    const bool wasArmed = settings.armed;
    settings = s;
    settings.maxConcurrent = juce::jlimit (1, kMaxTapsPerString, s.maxConcurrent);
    settings.channel = juce::jlimit (1, 16, s.channel);

    // Disarming lifts every tap (from the next block, without a flick).
    if (wasArmed && ! settings.armed)
        for (int st = 0; st < kMaxStrings; ++st)
            for (int i = 0; i < numTaps[(size_t) st]; ++i)
                taps[(size_t) st][(size_t) i].remaining = 0.0;
}

void TapEngine::setInstrument (int n, const double* openMidiNotes, int frets) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
    maxFrets = juce::jlimit (1, 36, frets);

    for (int s = 0; s < numStrings; ++s)
        openNotes[(size_t) s] = openMidiNotes != nullptr ? openMidiNotes[s] : 40.0 + 5.0 * s;
}

//==============================================================================
void TapEngine::requestGesture (const TapGesture& g) noexcept
{
    for (int i = 0; i < kQueue; ++i)
    {
        int expected = 0;

        if (requestState[(size_t) i].compare_exchange_strong (expected, 1))
        {
            requests[(size_t) i] = g;
            requestOrder[(size_t) i].store (requestCounter.fetch_add (1));
            requestState[(size_t) i].store (2, std::memory_order_release);
            return;
        }
    }
}

void TapEngine::requestRelease (int stringIndex, double fret) noexcept
{
    TapGesture g;
    g.stringIndex = stringIndex;
    g.fret = fret;

    for (int i = 0; i < kQueue; ++i)
    {
        int expected = 0;

        if (requestState[(size_t) i].compare_exchange_strong (expected, 1))
        {
            requests[(size_t) i] = g;
            requestOrder[(size_t) i].store (requestCounter.fetch_add (1));
            requestState[(size_t) i].store (3, std::memory_order_release);
            return;
        }
    }
}

//==============================================================================
bool TapEngine::placeNote (int note, const double* lhFrets, const bool* lhHeld, int& string, double& fret) const noexcept
{
    /*  Which string a tapped pitch lands on. A two-hand tap sits above the
        fretting hand on the same string, so prefer a string with a fretted
        note below the tap, closest above it; else the string that reaches the
        note lowest on the neck (above the open string - a tap on fret 0 is
        not a tap). */
    int best = -1;
    double bestFret = 0.0, bestScore = 1.0e9;

    for (int s = 0; s < numStrings; ++s)
    {
        const double f = (double) note - openNotes[(size_t) s];

        if (f < 1.0 || f > (double) maxFrets)
            continue;

        double score;

        if (lhHeld != nullptr && lhHeld[s] && lhFrets != nullptr && f > lhFrets[s])
            score = f - lhFrets[s];            // 0-24: above the fretting hand
        else
            score = 100.0 + f;                 // anywhere else, lowest fret first

        if (score < bestScore)
        {
            bestScore = score;
            best = s;
            bestFret = f;
        }
    }

    if (best < 0)
        return false;

    string = best;
    fret = bestFret;
    return true;
}

bool TapEngine::isTapping (int s) const noexcept
{
    return juce::isPositiveAndBelow (s, kMaxStrings) && numTaps[(size_t) s] > 0;
}

double TapEngine::soundingFret (int s, double fretted) const noexcept
{
    if (! isTapping (s))
        return fretted;

    // 10: taps in series - the highest wins the pitch.
    double highest = -1.0;

    for (int i = 0; i < numTaps[(size_t) s]; ++i)
        highest = juce::jmax (highest, taps[(size_t) s][(size_t) i].fret);

    return juce::jmax (highest, fretted);
}

void TapEngine::fretHandReleased (int s) noexcept
{
    if (juce::isPositiveAndBelow (s, kMaxStrings))
        fretHandGone[(size_t) s] = true;
}

//==============================================================================
void TapEngine::push (const TapEvent& e) noexcept
{
    if (numEvents < kMaxEvents)
        events[(size_t) numEvents++] = e;
}

void TapEngine::tapOn (int s, double fret, double strength, int note, double durationSeconds, bool pullOff,
                       double target, int offset, const double* lhFrets, const bool* lhHeld, const bool* blocked) noexcept
{
    if (! juce::isPositiveAndBelow (s, numStrings))
        return;

    // technique-cascade.md 3.4: the slide holds its strings.
    if (blocked != nullptr && blocked[s])
        return;

    if (settings.fretSnap)
        fret = std::round (fret);

    fret = juce::jlimit (0.0, (double) maxFrets, fret);

    auto& list = taps[(size_t) s];
    auto& count = numTaps[(size_t) s];

    if (count == 0)
        fretHandGone[(size_t) s] = ! (lhHeld != nullptr && lhHeld[s]);

    // 3: the concurrency cap. A hand has so many fingers: the oldest lifts.
    if (count >= settings.maxConcurrent)
    {
        int oldest = 0;

        for (int i = 1; i < count; ++i)
            if (list[(size_t) i].order < list[(size_t) oldest].order)
                oldest = i;

        list[(size_t) oldest] = list[(size_t) (count - 1)];
        --count;
    }

    Tap t;
    t.fret = fret;
    t.strength = strength;
    t.note = note;
    t.remaining = durationSeconds;
    t.pullOff = pullOff;
    t.target = target;
    t.order = ++orderCounter;
    list[(size_t) count++] = t;

    TapEvent e;
    e.kind = TapEvent::Kind::tapOn;
    e.stringIndex = s;
    e.fret = soundingFret (s, (lhHeld != nullptr && lhHeld[s] && lhFrets != nullptr) ? lhFrets[s] : 0.0);
    e.strength = strength;
    e.offset = offset;
    push (e);

    fireCount.fetch_add (1, std::memory_order_relaxed);

    for (int m = 0; m < kMaxMarkers; ++m)
    {
        const auto released = markerReleased[(size_t) m].load();

        if (markerFret[(size_t) m].load() < 0.0f
            || (released != 0 && juce::Time::getMillisecondCounter() - released > 150))
        {
            markerString[(size_t) m].store (s);
            markerFret[(size_t) m].store ((float) fret);
            markerReleased[(size_t) m].store (0);
            break;
        }
    }
}

void TapEngine::tapOff (int s, int slot, int offset, const double* lhFrets, const bool* lhHeld) noexcept
{
    auto& list = taps[(size_t) s];
    auto& count = numTaps[(size_t) s];

    if (! juce::isPositiveAndBelow (slot, count))
        return;

    const auto lifted = list[(size_t) slot];
    list[(size_t) slot] = list[(size_t) (count - 1)];
    --count;

    // The marker fades (4: over 100 ms).
    const auto now = juce::jmax ((juce::uint32) 1, juce::Time::getMillisecondCounter());

    for (int m = 0; m < kMaxMarkers; ++m)
        if (markerString[(size_t) m].load() == s && markerReleased[(size_t) m].load() == 0
            && std::abs (markerFret[(size_t) m].load() - (float) lifted.fret) < 0.01f)
        {
            markerReleased[(size_t) m].store (now);
            break;
        }

    const bool handDown = ! fretHandGone[(size_t) s] && lhHeld != nullptr && lhHeld[s];
    double fretted = handDown && lhFrets != nullptr ? lhFrets[s] : -1.0;

    // 1: a scripted pull-off names the fret it reveals.
    if (lifted.target >= 0.0)
        fretted = lifted.target;

    TapEvent e;
    e.kind = TapEvent::Kind::tapOff;
    e.stringIndex = s;
    e.offset = offset;

    if (count > 0)
    {
        // Another tap is still down: the string falls to it (a pull-off onto a finger).
        e.revealFret = soundingFret (s, juce::jmax (0.0, fretted));
        e.strength = settings.autoPullOff ? settings.lateralFlick * lifted.strength : 0.0;
    }
    else if (fretted >= 0.0)
    {
        // 2: back to the fretted note. Auto pull-off flicks it; off, the pitch only falls.
        e.revealFret = fretted;
        e.strength = (settings.autoPullOff && lifted.pullOff) ? settings.lateralFlick * lifted.strength : 0.0;
    }
    else if (settings.autoPullOff && lifted.pullOff && settings.lateralFlick > 0.0)
    {
        // Nothing fretted: a pull-off to the open string rings it.
        e.revealFret = 0.0;
        e.strength = settings.lateralFlick * lifted.strength;
    }
    else
    {
        // Nothing fretted and no flick: the finger lifts and the string stops.
        e.revealFret = -1.0;
        e.strength = 0.0;
    }

    push (e);
}

void TapEngine::processBlock (int numSamples, const TechniqueTriggers& triggers,
                              const double* lhFrets, const bool* lhHeld, const bool* blocked) noexcept
{
    numEvents = 0;
    const double blockSeconds = (double) numSamples / sr;

    // ---- the fretboard and scripts ---------------------------------------------------
    for (;;)
    {
        int next = -1;
        juce::uint32 lowest = 0;

        for (int i = 0; i < kQueue; ++i)
        {
            const int st = requestState[(size_t) i].load (std::memory_order_acquire);

            if ((st == 2 || st == 3) && (next < 0 || requestOrder[(size_t) i].load() - lowest > 0x80000000u))
            {
                next = i;
                lowest = requestOrder[(size_t) i].load();
            }
        }

        if (next < 0)
            break;

        const auto g = requests[(size_t) next];
        const bool release = requestState[(size_t) next].load() == 3;
        requestState[(size_t) next].store (0, std::memory_order_release);

        if (! settings.armed)
            continue;

        const int s = juce::jlimit (0, numStrings - 1, g.stringIndex);

        if (release)
        {
            for (int i = numTaps[(size_t) s] - 1; i >= 0; --i)
                if (std::abs (taps[(size_t) s][(size_t) i].fret - (settings.fretSnap ? std::round (g.fret) : g.fret)) < 0.51)
                {
                    tapOff (s, i, 0, lhFrets, lhHeld);
                    break;
                }

            continue;
        }

        const double durationMs = g.durationMs < 0.0 ? settings.defaultDurationMs : g.durationMs;

        tapOn (s, g.fret, juce::jlimit (0.0, 1.0, g.strength), -1,
               durationMs > 0.0 ? durationMs * 0.001 : -1.0,
               g.pullOffAfter, g.pullOffTargetFret, 0, lhFrets, lhHeld, blocked);
    }

    // ---- MIDI: the tap channel, or the notes under keyswitch 19 ------------------------
    if (settings.armed)
    {
        for (int i = 0; i < triggers.getNumEvents(); ++i)
        {
            const auto& g = triggers.getEvent (i);

            if (g.technique != TechniqueId::tap || g.number < 0)
                continue;

            const bool isNote = (settings.source == TapSource::midiChannel && g.role == 0)
                                  || (settings.source == TapSource::keyswitch && g.role == TechniqueTriggers::kCaptureRole);

            if (! isNote)
                continue;

            if (g.on)
            {
                int s = 0;
                double fret = 0.0;

                if (placeNote (g.number, lhFrets, lhHeld, s, fret))
                    tapOn (s, fret, settings.strengthFor (g.value), g.number, -1.0, true, -1.0,
                           g.offset, lhFrets, lhHeld, blocked);
            }
            else
            {
                for (int s = 0; s < numStrings; ++s)
                    for (int t = numTaps[(size_t) s] - 1; t >= 0; --t)
                        if (taps[(size_t) s][(size_t) t].note == g.number)
                            tapOff (s, t, g.offset, lhFrets, lhHeld);
            }
        }
    }

    // ---- timed taps (1: duration_ms) --------------------------------------------------
    for (int s = 0; s < numStrings; ++s)
    {
        for (int t = numTaps[(size_t) s] - 1; t >= 0; --t)
        {
            auto& tap = taps[(size_t) s][(size_t) t];

            if (tap.remaining < 0.0)
                continue;

            tap.remaining -= blockSeconds;

            if (tap.remaining <= 0.0)
                tapOff (s, t, juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                            (int) std::round ((tap.remaining + blockSeconds) * sr)),
                        lhFrets, lhHeld);
        }
    }
}

TapEngine::Marker TapEngine::getMarker (int i) const noexcept
{
    Marker m;

    if (! juce::isPositiveAndBelow (i, kMaxMarkers))
        return m;

    m.fret = markerFret[(size_t) i].load();
    m.string = markerString[(size_t) i].load();
    m.releasedMs = markerReleased[(size_t) i].load();
    m.held = m.fret >= 0.0f && m.releasedMs == 0;
    return m;
}

} // namespace luthier
