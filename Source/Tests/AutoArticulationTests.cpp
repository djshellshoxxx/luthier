/*  Performance Assist (auto-articulation.md 14, AA-01 - AA-36).

    The rule tests drive a MidiInterpreter with its own tuning, technique
    engine and voicer - the same wiring LuthierEngine::prepare makes - and read
    the typed events it emits, stamped with their absolute samples. The
    engine-level tests (lift, feed, attack, block sizes, capture, cost) drive a
    LuthierEngine or the processor. Humanize is off in the rule tests so a
    decision is the only thing that moves an event.
*/

#include "TestFramework.h"

#include "../Model/Playing/MidiInterpreter.h"
#include "../Model/Playing/AutoArticulator.h"
#include "../LuthierEngine.h"

#include <algorithm>
#include <random>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    juce::int64 ms (double m) { return (juce::int64) std::llround (m * 0.001 * kSr); }

    using M = juce::MidiMessage;

    struct Ev
    {
        juce::int64 sample;
        M message;
    };

    struct On
    {
        juce::int64 sample;
        NoteOnEvent e;
    };

    struct Off
    {
        juce::int64 sample;
        NoteOffEvent e;
    };

    //==============================================================================
    /** An interpreter wired as the engine wires it, with Assist settings. */
    struct Fixture
    {
        TuningEngine tuning;
        TechniqueEngine technique;
        RubricVoicer voicer;
        MidiInterpreter interp;

        AssistExplicitContext context;
        bool transportRunning = false;
        double bpm = 120.0;

        std::vector<On> ons;
        std::vector<Off> offs;

        explicit Fixture (PlayingMode mode, AssistStyle style = AssistStyle::cleanPop, bool on = true,
                          int rules = AssistRule::all, double amount = 0.6, int strings = 6,
                          TuningPreset preset = TuningPreset::Standard)
        {
            tuning.prepare (kSr);
            tuning.setNumStrings (strings);
            tuning.setTuningPreset (preset);
            technique.prepare (kSr, strings);
            voicer.prepare (&tuning, strings);

            interp.prepare (kSr, strings);
            interp.setEngines (&tuning, &technique, &voicer);
            interp.setNumStrings (strings);
            interp.setPlayingMode (mode);

            MidiInterpreter::Humanisation flat;
            flat.amount = 0.0;
            interp.setHumanisation (flat);

            auto strum = StrumSettings::guitarDefaults();
            strum.acceleration = 0.0;
            strum.missProbability = 0.0;
            strum.evenness = 1.0;
            strum.tilt = 0.0;
            interp.setStrumSettings (strum);

            AutoArticulationSettings s;
            s.enabled = on;
            s.style = (int) style;
            s.rules = rules;
            s.amount = amount;
            interp.setAutoArticulation (s);
        }

        AutoArticulator& aa() { return interp.getAutoArticulator(); }

        /** Plays the events in blocks of `block` (or the sizes cycling through
            `blocks`) up to `total`, collecting what came out. */
        void run (std::vector<Ev> events, juce::int64 total, std::vector<int> blocks = { 256 })
        {
            std::stable_sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.sample < b.sample; });

            auto queue = std::make_unique<PlayEventQueue>();
            size_t next = 0;
            size_t bi = 0;

            for (juce::int64 pos = 0; pos < total;)
            {
                const int n = (int) juce::jmin ((juce::int64) blocks[bi++ % blocks.size()], total - pos);
                juce::MidiBuffer midi;

                while (next < events.size() && events[next].sample < pos + n)
                {
                    midi.addEvent (events[next].message, (int) (events[next].sample - pos));
                    ++next;
                }

                AssistTransport t;
                t.playing = transportRunning;
                t.bpm = bpm;
                t.ppqAtBlockStart = (double) pos / kSr * bpm / 60.0;
                interp.setAssistContext (context, t, pos);

                interp.processBlock (midi, n, pos, *queue);

                for (int i = 0; i < queue->getNumNoteOns(); ++i)
                    ons.push_back ({ pos + queue->getNoteOn (i).sampleOffset, queue->getNoteOn (i) });

                for (int i = 0; i < queue->getNumNoteOffs(); ++i)
                    offs.push_back ({ pos + queue->getNoteOff (i).sampleOffset, queue->getNoteOff (i) });

                pos += n;
            }

            std::stable_sort (ons.begin(), ons.end(), [] (const On& a, const On& b) { return a.sample < b.sample; });
            std::stable_sort (offs.begin(), offs.end(), [] (const Off& a, const Off& b) { return a.sample < b.sample; });
        }

        const On* onFor (int midiNote, int nth = 0) const
        {
            for (const auto& o : ons)
                if (o.e.midiNote == midiNote && nth-- == 0)
                    return &o;

            return nullptr;
        }
    };

    Ev noteOn (double atMs, int note, int velocity = 100, int channel = 1)
    {
        return { ms (atMs), M::noteOn (channel, note, (juce::uint8) velocity) };
    }

    Ev noteOff (double atMs, int note, int channel = 1)
    {
        return { ms (atMs), M::noteOff (channel, note) };
    }

    Ev cc (double atMs, int number, int value)
    {
        return { ms (atMs), M::controllerEvent (1, number, value) };
    }

    /** A legato pair: `first` held until `second` arrives plus `overlapMs`. */
    std::vector<Ev> legatoPair (int first, int second, double ioiMs, double overlapMs, int v1 = 100, int v2 = 100)
    {
        return { noteOn (0, first, v1), noteOn (ioiMs, second, v2),
                 noteOff (ioiMs + overlapMs, first), noteOff (ioiMs + 400.0, second) };
    }

    juce::String describe (const std::vector<On>& ons)
    {
        juce::String s;

        for (const auto& o : ons)
            s << o.e.midiNote << "@" << (int) o.sample << " s" << o.e.stringIndex << " f" << o.e.fretPosition
              << " " << getTechniqueName (o.e.technique) << " r" << (int) o.e.autoRules << "; ";

        return s;
    }
}

//==============================================================================
// AA-04
LUTHIER_TEST (AutoArticulation, legatoPairIsHammerOnAndPullOff)
{
    {
        Fixture f (PlayingMode::Mono);
        f.run (legatoPair (60, 62, 150.0, 10.0), ms (800));

        const auto* c = f.onFor (60);
        const auto* d = f.onFor (62);
        CHECK (c != nullptr && d != nullptr);

        if (c != nullptr && d != nullptr)
        {
            CHECK_MSG (d->e.technique == Technique::HammerOn, describe (f.ons));
            CHECK (d->e.stringIndex == c->e.stringIndex);
            CHECK ((d->e.autoRules & AssistRule::legato) != 0);
        }
    }

    {
        Fixture f (PlayingMode::Mono);
        f.run (legatoPair (62, 60, 150.0, 10.0), ms (800));

        const auto* d = f.onFor (62);
        const auto* c = f.onFor (60);
        CHECK (c != nullptr && d != nullptr);

        if (c != nullptr && d != nullptr)
        {
            CHECK_MSG (c->e.technique == Technique::PullOff, describe (f.ons));
            CHECK (d->e.stringIndex == c->e.stringIndex);
        }
    }
}

// AA-05
LUTHIER_TEST (AutoArticulation, longOverlapIsASlideAndAGapIsAPluck)
{
    {
        Fixture f (PlayingMode::Mono);
        f.run (legatoPair (60, 62, 150.0, 60.0), ms (800));

        const auto* c = f.onFor (60);
        const auto* d = f.onFor (62);

        CHECK (c != nullptr && d != nullptr);

        if (c != nullptr && d != nullptr)
        {
            CHECK_MSG (d->e.technique == Technique::Slide, describe (f.ons));
            CHECK_NEAR (d->e.slideFromFret, c->e.fretPosition, 1.0e-9);
            CHECK (d->e.slideSeconds > 0.0);
        }
    }

    {
        Fixture f (PlayingMode::Mono);
        f.run ({ noteOn (0, 60), noteOff (150, 60), noteOn (300, 62), noteOff (600, 62) }, ms (800));

        const auto* d = f.onFor (62);
        CHECK (d != nullptr && d->e.technique == Technique::Pluck);
    }
}

// AA-06
LUTHIER_TEST (AutoArticulation, aHarderNoteIsRePicked)
{
    Fixture f (PlayingMode::Mono);
    f.run (legatoPair (60, 62, 150.0, 10.0, 70, 100), ms (800));

    const auto* d = f.onFor (62);
    CHECK_MSG (d != nullptr && d->e.technique == Technique::Pluck, describe (f.ons));
}

// AA-07
LUTHIER_TEST (AutoArticulation, pollyHandOverHasNoReleaseBeforeTheHammerOn)
{
    Fixture f (PlayingMode::Poly);
    f.interp.setChordWindowMs (2.0);

    // The source lets go 1 ms after the destination arrives: inside the window.
    f.run ({ noteOn (0, 60), noteOn (150, 62), noteOff (151, 60), noteOff (500, 62) }, ms (800));

    const auto* c = f.onFor (60);
    const auto* d = f.onFor (62);
    CHECK (c != nullptr && d != nullptr);

    if (c == nullptr || d == nullptr)
        return;

    CHECK_MSG (d->e.technique == Technique::HammerOn, describe (f.ons));
    CHECK (d->e.stringIndex == c->e.stringIndex);

    for (const auto& off : f.offs)
        CHECK_MSG (! (off.e.stringIndex == c->e.stringIndex && off.sample <= d->sample),
                   "a release at " + juce::String (off.sample) + " before the hammer-on at " + juce::String (d->sample));
}

// AA-08
LUTHIER_TEST (AutoArticulation, theLegatoChainCapRePicks)
{
    Fixture f (PlayingMode::Mono);

    std::vector<Ev> events;
    const int notes[] = { 64, 65, 66, 67, 68, 69, 70, 71 };

    for (int i = 0; i < 8; ++i)
    {
        events.push_back (noteOn (100.0 * i, notes[i]));
        events.push_back (noteOff (100.0 * (i + 1) + 10.0, notes[i]));
    }

    f.run (events, ms (1200));

    CHECK (f.ons.size() == 8);

    if (f.ons.size() != 8)
        return;

    const Technique expected[] = { Technique::Pluck, Technique::HammerOn, Technique::HammerOn, Technique::HammerOn,
                                   Technique::Pluck, Technique::HammerOn, Technique::HammerOn, Technique::HammerOn };

    for (int i = 0; i < 8; ++i)
        CHECK_MSG (f.ons[(size_t) i].e.technique == expected[i], describe (f.ons));
}

// AA-09
LUTHIER_TEST (AutoArticulation, aScaleStaysInOneBox)
{
    Fixture f (PlayingMode::Mono);

    const int scale[] = { 57, 59, 61, 62, 64, 66, 68, 69 };
    std::vector<Ev> events;

    for (int i = 0; i < 8; ++i)
    {
        events.push_back (noteOn (400.0 * i, scale[i]));
        events.push_back (noteOff (400.0 * i + 300.0, scale[i]));
    }

    std::vector<int> hands;
    f.run (events, ms (3400));

    int lo = 100, hi = -1, lastString = -1, prevHand = -1, shifts = 0;
    juce::ignoreUnused (lastString, hands);

    for (const auto& o : f.ons)
    {
        const int fret = (int) std::lround (o.e.fretPosition);

        if (fret > 0)
        {
            lo = juce::jmin (lo, fret);
            hi = juce::jmax (hi, fret);
        }
    }

    // Replay note by note to count hand moves.
    {
        Fixture g (PlayingMode::Mono);

        for (int i = 0; i < 8; ++i)
        {
            g.run ({ noteOn (400.0 * i, scale[i]), noteOff (400.0 * i + 300.0, scale[i]) }, ms (400.0 * (i + 1)));
            const int h = g.aa().getHandPosition();

            if (prevHand >= 0 && h != prevHand)
                ++shifts;

            prevHand = h;
            g.ons.clear();
        }
    }

    CHECK (f.ons.size() == 8);
    CHECK_MSG (hi - lo <= 4, describe (f.ons));
    CHECK_MSG (shifts <= 1, "shifts " + juce::String (shifts));
}

// AA-10
LUTHIER_TEST (AutoArticulation, positionTiesAreDeterministic)
{
    // Ties go to the lower fret: with the hand at 5 in Rock (no open bias), E4
    // costs the same open on string 0 as at fret 5 on string 1 - and the open
    // string, the lower fret, wins.
    {
        Fixture f (PlayingMode::Mono, AssistStyle::rock);
        f.aa().setHandPositionFromVoicing (5);
        const auto p = f.aa().planSingle (64, 0.8, 0, false, 0);
        CHECK (p.note.valid);
        CHECK_MSG (p.note.stringIndex == 0 && p.note.fretPosition == 0.0,
                   "s" + juce::String (p.note.stringIndex) + " f" + juce::String (p.note.fretPosition));
    }

    std::vector<int> reference;
    std::mt19937 rng (7);

    std::vector<Ev> events;
    for (int i = 0; i < 40; ++i)
    {
        const int note = 45 + (int) (rng() % 30);
        events.push_back (noteOn (120.0 * i, note, 60 + (int) (rng() % 60)));
        events.push_back (noteOff (120.0 * i + 90.0 + (double) (rng() % 60), note));
    }

    for (int run = 0; run < 100; ++run)
    {
        Fixture f (PlayingMode::Mono);
        f.run (events, ms (5200));

        std::vector<int> strings;
        for (const auto& o : f.ons)
            strings.push_back (o.e.stringIndex * 100 + (int) o.e.fretPosition);

        if (run == 0)
            reference = strings;
        else if (strings != reference)
        {
            CHECK_MSG (false, "run " + juce::String (run) + " differs");
            break;
        }
    }
}

// AA-11
LUTHIER_TEST (AutoArticulation, delayedVibratoOnAHeldBluesNote)
{
    Fixture f (PlayingMode::Mono, AssistStyle::blues);
    f.run ({ noteOn (0, 67), noteOff (800, 67) }, ms (900));

    const auto* o = f.onFor (67);
    CHECK (o != nullptr);

    if (o == nullptr)
        return;

    const int s = o->e.stringIndex;
    const double scale = 0.5 + 0.6;

    CHECK_NEAR (f.aa().autoVibratoDepth (s, o->sample + ms (200)), 0.0, 1.0e-9);
    CHECK_NEAR (f.aa().autoVibratoDepth (s, o->sample + ms (520)), 30.0 * scale, 1.0);

    // With the mod wheel at 64 the controller owns it.
    Fixture g (PlayingMode::Mono, AssistStyle::blues);
    g.run ({ cc (0, 1, 64), noteOn (1, 67), noteOff (800, 67) }, ms (900));

    const auto* p = g.onFor (67);
    CHECK (p != nullptr && g.aa().autoVibratoDepth (p->e.stringIndex, p->sample + ms (600)) == 0.0);
}

// AA-12
LUTHIER_TEST (AutoArticulation, aHeldChordGetsNoVibrato)
{
    Fixture f (PlayingMode::Poly, AssistStyle::blues);
    f.run ({ noteOn (0, 48), noteOn (0, 52), noteOn (0, 55), noteOn (0, 60),
             noteOff (2000, 48), noteOff (2000, 52), noteOff (2000, 55), noteOff (2000, 60) }, ms (2100));

    CHECK (f.ons.size() == 4);

    for (int s = 0; s < 6; ++s)
        CHECK (f.aa().autoVibratoDepth (s, ms (1500)) == 0.0);
}

// AA-13
LUTHIER_TEST (AutoArticulation, anAccentBrightensWithoutChangingVelocity)
{
    Fixture f (PlayingMode::Mono, AssistStyle::rock, true, AssistRule::all & ~AssistRule::alternate);
    f.run ({ noteOn (0, 60, 120), noteOff (300, 60), noteOn (1000, 60, 90), noteOff (1300, 60) }, ms (1500));

    const auto* loud = f.onFor (60, 0);
    const auto* soft = f.onFor (60, 1);
    CHECK (loud != nullptr && soft != nullptr);

    if (loud == nullptr || soft == nullptr)
        return;

    const double k = 0.5 + 0.6;
    CHECK_NEAR (loud->e.attackBrightnessScale, 1.0 + 0.25 * k, 1.0e-9);
    CHECK_NEAR (loud->e.attackNoiseScale, 1.0 + 0.8 * k, 1.0e-9);
    CHECK_NEAR (soft->e.attackBrightnessScale, 1.0, 1.0e-9);
    CHECK_NEAR (loud->e.velocity, 120.0 / 127.0, 1.0e-6);
    CHECK (loud->e.autoAccent);
}

// AA-14
LUTHIER_TEST (AutoArticulation, rockChugsArePalmMuted)
{
    std::vector<Ev> events;

    for (int i = 0; i < 8; ++i)
    {
        events.push_back (noteOn (125.0 * i, 40, 100));
        events.push_back (noteOff (125.0 * i + 75.0, 40));
    }

    {
        Fixture f (PlayingMode::Mono, AssistStyle::rock);
        f.run (events, ms (1200));
        CHECK (f.ons.size() == 8);

        for (size_t i = 1; i < f.ons.size(); ++i)
        {
            CHECK_MSG (f.ons[i].e.technique == Technique::PalmMute, describe (f.ons));
            CHECK_NEAR (f.ons[i].e.palmMuteAmount, 0.55 * (0.5 + 0.6), 1.0e-9);
        }
    }

    {
        Fixture f (PlayingMode::Mono, AssistStyle::jazz);
        f.run (events, ms (1200));

        for (const auto& o : f.ons)
            CHECK (o.e.technique != Technique::PalmMute);
    }
}

// AA-16
LUTHIER_TEST (AutoArticulation, alternatePickingFollowsTheGridOrToggles)
{
    std::vector<Ev> events;

    for (int i = 0; i < 8; ++i)
    {
        events.push_back (noteOn (125.0 * i, 69, 100));
        events.push_back (noteOff (125.0 * i + 100.0, 69));
    }

    {
        Fixture f (PlayingMode::Mono);
        f.transportRunning = true;
        f.run (events, ms (1200));
        CHECK (f.ons.size() == 8);

        for (size_t i = 0; i < f.ons.size(); ++i)
            CHECK_MSG (f.ons[i].e.upStroke == ((i % 2) == 1), describe (f.ons));
    }

    {
        Fixture f (PlayingMode::Mono);
        f.run ({ noteOn (0, 69), noteOff (100, 69), noteOn (125, 69), noteOff (225, 69),
                 noteOn (700, 69), noteOff (800, 69) }, ms (1000));
        CHECK (f.ons.size() == 3);

        if (f.ons.size() == 3)
        {
            CHECK (! f.ons[0].e.upStroke);
            CHECK (f.ons[1].e.upStroke);
            CHECK (! f.ons[2].e.upStroke);   // a gap of more than 300 ms resets to down
        }
    }
}

namespace
{
    std::vector<Ev> chordAt (double atMs, double lengthMs, std::initializer_list<int> notes, int velocity = 100)
    {
        std::vector<Ev> e;

        for (int n : notes)
            e.push_back (noteOn (atMs, n, velocity));

        for (int n : notes)
            e.push_back (noteOff (atMs + lengthMs, n));

        return e;
    }

    template <typename... V>
    std::vector<Ev> joinEvents (V... parts)
    {
        std::vector<Ev> all;
        (all.insert (all.end(), parts.begin(), parts.end()), ...);
        return all;
    }

    std::vector<On> chordStrikes (const std::vector<On>& ons, juce::int64 from, juce::int64 to)
    {
        std::vector<On> out;

        for (const auto& o : ons)
            if (o.sample >= from && o.sample < to)
                out.push_back (o);

        return out;
    }
}

// AA-17
LUTHIER_TEST (AutoArticulation, chordsStrumDownOnTheBeatAndUpOffIt)
{
    Fixture f (PlayingMode::Poly);
    f.transportRunning = true;

    // 120 bpm: beat 1 at 0, its "and" at 250 ms.
    f.run (joinEvents (chordAt (0, 200, { 40, 45, 50, 55, 59, 64 }, 100),
                       chordAt (250, 200, { 40, 45, 50, 55, 59, 64 }, 100)), ms (700));

    const auto first = chordStrikes (f.ons, 0, ms (240));
    const auto second = chordStrikes (f.ons, ms (240), ms (600));

    CHECK_MSG (first.size() == 6 && second.size() == 6, describe (f.ons));

    if (first.size() != 6 || second.size() != 6)
        return;

    // Down: the low strings (high indices) first.
    CHECK_MSG (first.front().e.stringIndex > first.back().e.stringIndex, describe (first));
    CHECK (! first.front().e.upStroke);
    CHECK_MSG (second.front().e.stringIndex < second.back().e.stringIndex, describe (second));
    CHECK (second.front().e.upStroke);

    // 220 sps x (0.75 + 0.5 v), evenly spaced with the acceleration off.
    const double sps = 220.0 * (0.75 + 0.5 * (100.0 / 127.0));
    const double spacing = kSr / sps;

    for (size_t i = 1; i < first.size(); ++i)
        CHECK_NEAR ((double) (first[i].sample - first[i - 1].sample), spacing, 1.0);
}

// AA-18
LUTHIER_TEST (AutoArticulation, metalDownstrokesAndFingerstylePinchRoll)
{
    {
        Fixture f (PlayingMode::Poly, AssistStyle::metal);
        f.transportRunning = true;
        f.run (joinEvents (chordAt (0, 200, { 40, 47, 52 }), chordAt (250, 200, { 40, 47, 52 })), ms (700));

        for (const auto& o : f.ons)
            CHECK (! o.e.upStroke);
    }

    {
        Fixture f (PlayingMode::Poly, AssistStyle::fingerstyle);
        f.run (chordAt (0, 400, { 48, 55, 60, 64 }), ms (600));
        CHECK (f.ons.size() == 4);

        if (f.ons.size() == 4)
        {
            CHECK_MSG (f.ons.front().e.midiNote == 48, describe (f.ons));

            for (size_t i = 1; i < f.ons.size(); ++i)
            {
                CHECK (f.ons[i].e.midiNote > f.ons[i - 1].e.midiNote);
                CHECK (f.ons[i].sample - f.ons.front().sample <= ms (15) + 1);
            }
        }
    }
}

// AA-19
LUTHIER_TEST (AutoArticulation, rolledChordsKeepTheirTimingAndTheStrumControllerWins)
{
    {
        Fixture f (PlayingMode::Poly);
        f.interp.setChordWindowMs (20.0);
        f.run ({ noteOn (0, 48), noteOn (4, 52), noteOn (8, 55), noteOn (12, 60),
                 noteOff (400, 48), noteOff (400, 52), noteOff (400, 55), noteOff (400, 60) }, ms (600));

        CHECK (f.ons.size() == 4);

        if (f.ons.size() == 4)
            for (size_t i = 0; i < 4; ++i)
                CHECK_NEAR ((double) (f.ons[i].sample - f.ons[0].sample), (double) ms (4.0 * (double) i), 1.0);
    }

    {
        // CC 77 in its Up zone: the controller's direction wins over the grid's down.
        Fixture f (PlayingMode::Poly);
        f.transportRunning = true;
        f.run (joinEvents (std::vector<Ev> { cc (0, 77, 64) }, chordAt (1, 200, { 40, 45, 50, 55 })), ms (400));
        CHECK (f.ons.size() == 4);

        if (f.ons.size() == 4)
            CHECK_MSG (f.ons.front().e.stringIndex < f.ons.back().e.stringIndex, describe (f.ons));
    }
}

// AA-20
LUTHIER_TEST (AutoArticulation, aLateJoinLandsOnAFreeStringAtItsOwnTime)
{
    Fixture f (PlayingMode::Poly);
    f.run (joinEvents (chordAt (0, 500, { 48, 52, 55 }), std::vector<Ev> { noteOn (30, 64), noteOff (500, 64) }),
           ms (700));

    CHECK (f.ons.size() == 4);

    const auto* late = f.onFor (64);

    if (late == nullptr)
        return;

    for (const auto& o : f.ons)
        if (&o != late)
            CHECK (o.e.stringIndex != late->e.stringIndex);

    // One window late, like every note in Poly.
    CHECK_NEAR ((double) late->sample, (double) (ms (30) + ms (2)), 2.0);
}

// AA-21
LUTHIER_TEST (AutoArticulation, bendIntoStartsBelowAndRisesToPitch)
{
    Fixture f (PlayingMode::Mono, AssistStyle::blues);
    f.aa().setProbabilityOverride (1.0);
    // Rendered only to 400 ms: the note is still held, so no fall has replaced the curve.
    f.run ({ noteOn (0, 69, 100), noteOff (600, 69) }, ms (400));

    const auto* o = f.onFor (69);
    CHECK (o != nullptr);

    if (o == nullptr)
        return;

    const int k = o->e.autoOrnament == 2 ? 2 : 1;
    CHECK (o->e.autoOrnament == 1 || o->e.autoOrnament == 2);

    // Played k frets down, then bent up k semitones within the rise plus a block.
    const double natural = f.tuning.frequencyToFretPosition (o->e.stringIndex, midiToHz (69.0, 440.0));
    CHECK_NEAR (o->e.fretPosition, std::round (natural) - k, 1.0e-9);
    CHECK_NEAR (f.aa().autoPitchCents (o->e.stringIndex, o->sample + ms (110) + 256), 100.0 * k, 5.0);
    CHECK_NEAR (f.aa().autoPitchCents (o->e.stringIndex, o->sample), 0.0, 1.0e-9);
}

// AA-22
LUTHIER_TEST (AutoArticulation, aFallDefersTheReleaseAndGlidesDown)
{
    Fixture f (PlayingMode::Mono, AssistStyle::blues, true, AssistRule::all);
    f.aa().setProbabilityOverride (1.0);
    f.run ({ noteOn (0, 69, 70), noteOff (600, 69) }, ms (900));

    CHECK (f.offs.size() == 1);

    if (f.offs.size() == 1)
    {
        CHECK_NEAR ((double) f.offs[0].sample, (double) (ms (600) + ms (120)), 1.0);
        const int s = f.offs[0].e.stringIndex;
        const double before = f.aa().autoPitchCents (s, ms (600));
        CHECK_NEAR (f.aa().autoPitchCents (s, ms (720)) - before, -200.0, 10.0);
    }

    // A note on another string during the fall does not cancel it...
    {
        Fixture g (PlayingMode::Poly, AssistStyle::blues);
        g.aa().setProbabilityOverride (1.0);
        g.run ({ noteOn (0, 69, 70), noteOff (600, 69), noteOn (650, 45, 70), noteOff (900, 45) }, ms (1000));

        const auto* first = g.onFor (69);
        bool released = false;

        for (const auto& off : g.offs)
            if (first != nullptr && off.e.stringIndex == first->e.stringIndex)
                released = std::abs ((double) (off.sample - ms (720))) <= (double) ms (2) + 1.0;

        CHECK_MSG (released, describe (g.ons));
    }

    // ...one on the same string does: it takes the string.
    {
        Fixture g (PlayingMode::Mono, AssistStyle::blues);
        g.aa().setProbabilityOverride (1.0);
        g.run ({ noteOn (0, 69, 70), noteOff (600, 69), noteOn (650, 69, 70), noteOff (900, 69) }, ms (1100));

        const auto* second = g.onFor (69, 1);
        CHECK (second != nullptr);

        for (const auto& off : g.offs)
            CHECK_MSG (! (off.sample >= ms (600) && off.sample <= ms (720) + 1), "the fall still released the string");
    }
}

// AA-23
LUTHIER_TEST (AutoArticulation, neverAHarmonicTapSlideGuitarOrMutedPick)
{
    std::mt19937 rng (0xAA23);
    int notes = 0;

    for (int phrase = 0; phrase < 10000; ++phrase)
    {
        const auto style = (AssistStyle) (phrase % AutoArticulationStyles::kNumStyles);
        Fixture f ((phrase / 8) % 2 == 0 ? PlayingMode::Mono : PlayingMode::Poly, style, true, AssistRule::all,
                   (double) (rng() % 101) / 100.0);
        f.aa().setProbabilityOverride ((phrase % 3) == 0 ? 1.0 : -1.0);

        std::vector<Ev> events;
        double t = 0.0;

        for (int i = 0; i < 6; ++i)
        {
            const int note = 38 + (int) (rng() % 40);
            const double len = 20.0 + (double) (rng() % 500);
            events.push_back (noteOn (t, note, 20 + (int) (rng() % 107)));
            events.push_back (noteOff (t + len, note));
            t += (double) (rng() % 300);
        }

        f.run (events, ms (t + 800.0), { 64, 512, 100 });

        for (const auto& o : f.ons)
        {
            ++notes;
            const auto tech = o.e.technique;
            CHECK (tech != Technique::NaturalHarmonic && tech != Technique::PinchHarmonic
                   && tech != Technique::ArtificialHarmonic && tech != Technique::Tap
                   && tech != Technique::SlideGuitar && tech != Technique::MutedPick);
            CHECK (std::isfinite (o.e.pitchHz) && o.e.fretPosition >= 0.0);
        }
    }

    CHECK (notes > 30000);
}

// AA-24
LUTHIER_TEST (AutoArticulation, explicitTechniquesWin)
{
    {
        Fixture f (PlayingMode::Mono, AssistStyle::rock);
        auto events = joinEvents (std::vector<Ev> { cc (0, 67, 127) }, legatoPair (60, 62, 150.0, 60.0));
        f.run (events, ms (800));

        for (const auto& o : f.ons)
        {
            CHECK (o.e.technique == Technique::PalmMute);
            CHECK (o.e.palmMuteAmount < 0.0);   // the controller's amount
            CHECK ((o.e.autoRules & (AssistRule::legato | AssistRule::slide | AssistRule::palmMute)) == 0);
        }
    }

    {
        Fixture f (PlayingMode::Mono, AssistStyle::rock);
        f.run ({ cc (0, 72, 127), noteOn (1, 60, 120), noteOff (300, 60) }, ms (500));
        const auto* o = f.onFor (60);
        CHECK (o != nullptr && o->e.technique == Technique::PinchHarmonic && o->e.autoRules == 0);
    }
}

namespace
{
    /** Two runs, Assist on and off, and the typed events compared field by field. */
    bool sameEvents (const Fixture& a, const Fixture& b, juce::String& why)
    {
        if (a.ons.size() != b.ons.size() || a.offs.size() != b.offs.size())
        {
            why = "counts differ";
            return false;
        }

        for (size_t i = 0; i < a.ons.size(); ++i)
        {
            const auto& x = a.ons[i].e;
            const auto& y = b.ons[i].e;

            if (a.ons[i].sample != b.ons[i].sample || x.stringIndex != y.stringIndex || x.midiNote != y.midiNote
                || x.fretPosition != y.fretPosition || x.velocity != y.velocity || x.technique != y.technique
                || x.pitchHz != y.pitchHz || x.slideFromFret != y.slideFromFret || x.autoRules != y.autoRules)
            {
                why = "note-on " + juce::String ((int) i);
                return false;
            }
        }

        for (size_t i = 0; i < a.offs.size(); ++i)
            if (a.offs[i].sample != b.offs[i].sample || a.offs[i].e.stringIndex != b.offs[i].e.stringIndex)
            {
                why = "note-off " + juce::String ((int) i);
                return false;
            }

        return true;
    }

    std::vector<Ev> mixedPhrase()
    {
        return { noteOn (0, 60), noteOn (150, 62), noteOff (160, 60), noteOff (400, 62),
                 noteOn (500, 40), noteOn (500, 47), noteOn (500, 52), noteOff (900, 40), noteOff (900, 47),
                 noteOff (900, 52), noteOn (1000, 69, 120), noteOff (1700, 69) };
    }
}

// AA-25
LUTHIER_TEST (AutoArticulation, guitarControllerAndMpeAreBypassed)
{
    for (int variant = 0; variant < 2; ++variant)
    {
        Fixture on (variant == 0 ? PlayingMode::GuitarController : PlayingMode::Poly, AssistStyle::rock, true);
        Fixture off (variant == 0 ? PlayingMode::GuitarController : PlayingMode::Poly, AssistStyle::rock, false);

        if (variant == 1)
        {
            on.interp.setMpeEnabled (true);
            off.interp.setMpeEnabled (true);
        }

        on.run (mixedPhrase(), ms (2000));
        off.run (mixedPhrase(), ms (2000));

        juce::String why;
        CHECK_MSG (sameEvents (on, off, why), why);
        CHECK (on.interp.getAssistBypass() == AssistBypass::guitarControllerOrMpe);
    }
}

// AA-26
LUTHIER_TEST (AutoArticulation, rhythmDrivenPassIsNotAssisted)
{
    Fixture on (PlayingMode::Poly, AssistStyle::rock, true);
    Fixture off (PlayingMode::Poly, AssistStyle::rock, false);
    on.context.rhythmDriving = true;

    on.run (mixedPhrase(), ms (2000));
    off.run (mixedPhrase(), ms (2000));

    juce::String why;
    CHECK_MSG (sameEvents (on, off, why), why);

    // The direct pass (the Tune melody) is.
    Fixture direct (PlayingMode::Poly, AssistStyle::rock, true);
    direct.run (mixedPhrase(), ms (2000));

    int assisted = 0;
    for (const auto& o : direct.ons)
        assisted += o.e.autoRules != 0 ? 1 : 0;

    CHECK (assisted > 0);
}

// AA-27
LUTHIER_TEST (AutoArticulation, slapZoneAndTapArmedAreNotDecorated)
{
    for (int variant = 0; variant < 2; ++variant)
    {
        Fixture f (PlayingMode::Mono, AssistStyle::rock);

        if (variant == 0)
        {
            f.context.slapVelocityZone = true;
            f.context.slapZoneVelocity = 100;
        }
        else
        {
            f.context.tapArmed = true;
        }

        f.run ({ noteOn (0, 40, 110), noteOff (75, 40), noteOn (125, 40, 110), noteOff (200, 40) }, ms (400));

        for (const auto& o : f.ons)
            CHECK (o.e.autoRules == 0 && o.e.technique == Technique::Pluck);
    }
}

// AA-28
LUTHIER_TEST (AutoArticulation, decisionsDoNotDependOnTheBlockSize)
{
    std::vector<Ev> events = mixedPhrase();

    for (int i = 0; i < 12; ++i)
    {
        events.push_back (noteOn (2000.0 + 125.0 * i, 40 + (i % 3) * 5, 90 + (i % 4) * 10));
        events.push_back (noteOff (2000.0 + 125.0 * i + 70.0, 40 + (i % 3) * 5));
    }

    const std::vector<std::vector<int>> sizes = { { 32 }, { 64 }, { 256 }, { 512 }, { 1024 }, { 2048 }, { 100, 37, 511, 3 } };
    std::unique_ptr<Fixture> reference;

    for (const auto& blocks : sizes)
    {
        auto f = std::make_unique<Fixture> (PlayingMode::Mono, AssistStyle::rock);
        f->transportRunning = true;
        f->run (events, ms (4000), blocks);

        if (reference == nullptr)
        {
            reference = std::move (f);
            continue;
        }

        juce::String why;
        CHECK_MSG (sameEvents (*reference, *f, why), "block " + juce::String (blocks[0]) + ": " + why);
    }
}

// AA-02
LUTHIER_TEST (AutoArticulation, assistDrawsNothingFromTheInterpretersRandom)
{
    // With Humanize on, the jitter each note gets is the RtRandom's sequence.
    // Assist on and off must see the same draws in the same order: the note
    // count, and each note's detune (pitch against its fret), are compared.
    auto humanised = [] (Fixture& f)
    {
        MidiInterpreter::Humanisation h;
        f.interp.setHumanisation (h);
    };

    Fixture on (PlayingMode::Poly, AssistStyle::rock, true);
    Fixture off (PlayingMode::Poly, AssistStyle::rock, false);
    humanised (on);
    humanised (off);

    on.run (mixedPhrase(), ms (2000));
    off.run (mixedPhrase(), ms (2000));

    CHECK (on.ons.size() == off.ons.size());

    // The draws, as the detunes they became. A strum orders its strings by
    // time, so the comparison is of the values, not of which note got which.
    auto detunes = [] (Fixture& f)
    {
        std::vector<double> d;

        for (const auto& o : f.ons)
            d.push_back (1200.0 * std::log2 (o.e.pitchHz / f.tuning.computeFrequency (o.e.stringIndex, o.e.fretPosition, 0.0)));

        std::sort (d.begin(), d.end());
        return d;
    };

    const auto a = detunes (on), b = detunes (off);

    for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
        CHECK_NEAR (a[i], b[i], 1.0e-6);
}
