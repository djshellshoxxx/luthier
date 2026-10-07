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

//==============================================================================
//  Engine and processor level.
//==============================================================================
#include "ComboHarness.h"
#include "../Capture/PerformanceCapture.h"
#include "../Notation/NotationExport.h"
#include "../Export/MidiProfiles.h"
#include "../Export/MidiImportTargets.h"
#include "../Support/Edition.h"
#include "../UI/PerformanceAssistUi.h"

namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;
}

namespace
{
    /** A bare engine with Assist set, rendering MIDI in blocks. */
    struct EngineRig
    {
        std::unique_ptr<LuthierEngine> engine = std::make_unique<LuthierEngine>();
        std::vector<float> out;

        EngineRig (AutoArticulationSettings s, PlayingMode mode = PlayingMode::Mono,
                   GuitarType type = GuitarType::Stratocaster, int block = 256)
        {
            engine->prepare (kSr, block);
            engine->setGuitarType (type);

            MidiInterpreter::Humanisation flat;
            flat.amount = 0.0;
            engine->getMidiInterpreter().setHumanisation (flat);
            engine->getMidiInterpreter().setPlayingMode (mode);
            engine->setAutoArticulation (s);
            engine->reset();
        }

        void render (std::vector<Ev> events, juce::int64 total, std::vector<int> blocks = { 256 },
                     std::function<void (juce::int64)> afterBlock = {})
        {
            std::stable_sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.sample < b.sample; });
            juce::AudioBuffer<float> buffer (2, 4096);
            size_t next = 0, bi = 0;

            for (juce::int64 pos = 0; pos < total;)
            {
                const int n = (int) juce::jmin ((juce::int64) blocks[bi++ % blocks.size()], total - pos);
                juce::MidiBuffer midi;

                while (next < events.size() && events[next].sample < pos + n)
                {
                    midi.addEvent (events[next].message, (int) (events[next].sample - pos));
                    ++next;
                }

                buffer.setSize (2, n, false, false, true);
                buffer.clear();
                engine->processBlock (buffer, midi);

                for (int i = 0; i < n; ++i)
                    out.push_back (buffer.getSample (0, i));

                pos += n;

                if (afterBlock)
                    afterBlock (pos);
            }
        }

        std::vector<AutoArticulationFeedEntry> feed()
        {
            std::vector<AutoArticulationFeedEntry> entries;
            engine->getAutoArticulator().getFeed().drain ([&] (const AutoArticulationFeedEntry& e) { entries.push_back (e); });
            return entries;
        }
    };

    AutoArticulationSettings assistOn (AssistStyle style, int rules = AssistRule::all, double amount = 0.6)
    {
        AutoArticulationSettings s;
        s.enabled = true;
        s.style = (int) style;
        s.rules = rules;
        s.amount = amount;
        return s;
    }

    std::vector<Ev> allComboPhrases()
    {
        std::vector<Ev> events;
        juce::int64 offset = 0;

        for (int ph = 0; ph < (int) combo::Phrase::numPhrases; ++ph)
        {
            int released = 0;

            for (const auto& e : combo::makePhrase ((combo::Phrase) ph, released))
                events.push_back ({ offset + e.sample, e.message });

            offset += released + (juce::int64) (1.5 * combo::kSr);
        }

        return events;
    }
}

// AA-01 (in-suite half; scripts/assist_off_golden_check.sh compares against the
// pre-feature build): off, Amount 0 and rules 0 render the same samples and
// the same events as a processor whose aa_* were never touched.
LUTHIER_TEST (AutoArticulationEngine, offAmountZeroAndNoRulesAreTheSameAsNothing)
{
    const auto events = allComboPhrases();
    const juce::int64 total = events.back().sample + (juce::int64) combo::kSr;

    for (const char* preset : { "Init", "Modern Metal Chug", "P-Bass Flatwound" })
    {
        std::vector<std::vector<float>> renders;

        for (int variant = 0; variant < (edition::isPro ? 4 : 3); ++variant)
        {
            combo::Rig rig;
            auto& presets = rig.p().getPresetManager();
            rig.p().resetEverything();
            CHECK (presets.loadPreset (presets.indexOfPreset (preset)));

            if (variant == 1) { rig.setIndex (ParamIDs::aaEnabled, 0); rig.setIndex (ParamIDs::aaStyle, 3); }
            if (variant == 2) { rig.setIndex (ParamIDs::aaEnabled, 1); rig.setPlain (ParamIDs::aaAmount, 0.0f); }
            // auto-articulation.md 11: in Free aa_rules is always effectively 511, so "no
            // rules" is a Pro-only configuration (Free runs 3 variants; its forcing is checked below).
            if (variant == 3) { rig.setIndex (ParamIDs::aaEnabled, 1); rig.setIndex (ParamIDs::aaRules, 0); }

            rig.apply();
            rig.processSilence (2);

            std::vector<combo::TimedMidi> timed;
            for (const auto& e : events)
                timed.push_back ({ (int) e.sample, e.message });

            renders.push_back (rig.renderEvents (timed, (int) total, 0.0).mono);
        }

        for (int v = 1; v < (int) renders.size(); ++v)
            CHECK_MSG (renders[(size_t) v] == renders[0], juce::String (preset) + " variant " + juce::String (v) + " differs");
    }

    if (! edition::isPro)
        CHECK (Parameters::effectiveAssistSettings (true, 0, 60.0f, 0, Edition::free).rules == AssistRule::all);
}

// AA-03
LUTHIER_TEST (AutoArticulationEngine, latencyDoesNotDependOnAssist)
{
    for (float window : { 0.0f, 2.0f, 20.0f })
    {
        int latency[2] = {};

        for (int on = 0; on < 2; ++on)
        {
            combo::Rig rig;
            rig.setIndex (ParamIDs::playingMode, (int) PlayingMode::Poly);
            rig.setPlain (ParamIDs::chordWindow, window);
            rig.setIndex (ParamIDs::aaEnabled, on);
            rig.apply();
            rig.processSilence (2);
            latency[on] = rig.p().getEngine().getLatencySamples();
        }

        CHECK_MSG (latency[0] == latency[1], "window " + juce::String (window) + ": " + juce::String (latency[0])
                                               + " vs " + juce::String (latency[1]));
    }
}

// AA-15
LUTHIER_TEST (AutoArticulationEngine, anAutoMuteLiftsWhenHeld)
{
    EngineRig rig (assistOn (AssistStyle::rock));
    const auto s = rig.engine->getNumStrings() - 1;
    StringEngine::Damping dampingAt[3] {};

    const juce::int64 lift = ms (125) + ms (220);   // max (1.5 x 125 ms, 220 ms) after the second note

    rig.render ({ noteOn (0, 40), noteOff (75, 40), noteOn (125, 40), noteOff (1200, 40) }, ms (1300), { 256 },
                [&] (juce::int64 pos)
                {
                    if (pos <= ms (200) && pos + 256 > ms (200))          dampingAt[0] = rig.engine->getString (s).getDamping();
                    if (pos <= lift + ms (60) + 256 && pos + 256 > lift + ms (60) + 256) dampingAt[1] = rig.engine->getString (s).getDamping();
                });

    bool lifted = false;

    for (const auto& e : rig.feed())
        if (e.label == (juce::uint8) AssistLabel::muteLift)
        {
            lifted = true;
            CHECK_NEAR ((double) e.sample, (double) lift, 1.0);
        }

    CHECK (lifted);
    CHECK (dampingAt[0] == StringEngine::Damping::PalmMute);
    CHECK (dampingAt[1] == StringEngine::Damping::Open);
}

// AA-28 (the engine's feed)
LUTHIER_TEST (AutoArticulationEngine, theFeedDoesNotDependOnTheBlockSize)
{
    std::vector<Ev> events;

    for (int i = 0; i < 12; ++i)
    {
        events.push_back (noteOn (125.0 * i, 40 + (i % 3) * 5, 90 + (i % 4) * 10));
        events.push_back (noteOff (125.0 * i + 70.0, 40 + (i % 3) * 5));
    }

    events.push_back (noteOn (2000, 60));
    events.push_back (noteOn (2150, 62));
    events.push_back (noteOff (2160, 60));
    events.push_back (noteOff (2500, 62));

    std::vector<AutoArticulationFeedEntry> reference;

    for (auto blocks : std::vector<std::vector<int>> { { 256 }, { 32 }, { 64 }, { 512 }, { 1024 }, { 2048 }, { 100, 37, 511 } })
    {
        int maxBlock = 0;
        for (int b : blocks) maxBlock = juce::jmax (maxBlock, b);

        EngineRig rig (assistOn (AssistStyle::rock), PlayingMode::Mono, GuitarType::Stratocaster, maxBlock);
        rig.render (events, ms (3000), blocks);
        auto entries = rig.feed();

        if (reference.empty())
        {
            reference = entries;
            CHECK (reference.size() > 10);
            continue;
        }

        bool same = entries.size() == reference.size();

        for (size_t i = 0; same && i < entries.size(); ++i)
            same = entries[i].sample == reference[i].sample && entries[i].string == reference[i].string
                && entries[i].label == reference[i].label;

        CHECK_MSG (same, "block " + juce::String (blocks[0]) + ": " + juce::String ((int) entries.size())
                           + " entries vs " + juce::String ((int) reference.size()));
    }
}

// AA-29
LUTHIER_TEST (AutoArticulationEngine, realtimeAndOfflineRendersAreIdentical)
{
    const auto events = allComboPhrases();
    const juce::int64 total = juce::jmin ((juce::int64) (30.0 * kSr), events.back().sample + (juce::int64) kSr);
    std::vector<float> renders[2];

    for (int offline = 0; offline < 2; ++offline)
    {
        combo::Rig rig;
        rig.p().setNonRealtime (offline == 1);
        rig.setIndex (ParamIDs::aaEnabled, 1);
        rig.setIndex (ParamIDs::aaStyle, (int) AssistStyle::blues);
        rig.apply();
        rig.processSilence (2);

        std::vector<combo::TimedMidi> timed;
        for (const auto& e : events)
            timed.push_back ({ (int) e.sample, e.message });

        renders[offline] = rig.renderEvents (timed, (int) total, 0.0).mono;
    }

    CHECK (renders[0] == renders[1]);
}

// AA-30
LUTHIER_TEST (AutoArticulationEngine, noAllocationInProcessBlockInAnyStyle)
{
    const auto events = allComboPhrases();

    for (int style = 0; style < AutoArticulationStyles::kNumStyles; ++style)
    {
        EngineRig rig (assistOn ((AssistStyle) style), style % 2 == 0 ? PlayingMode::Mono : PlayingMode::Poly);
        rig.engine->getAutoArticulator().setProbabilityOverride (1.0);

        // MidiBuffers are built outside the measured call.
        std::vector<juce::MidiBuffer> blocks;
        const juce::int64 total = events.back().sample + (juce::int64) kSr;
        size_t next = 0;

        for (juce::int64 pos = 0; pos < total; pos += 256)
        {
            juce::MidiBuffer midi;
            midi.ensureSize (4096);

            while (next < events.size() && events[next].sample < pos + 256)
            {
                midi.addEvent (events[next].message, (int) (events[next].sample - pos));
                ++next;
            }

            blocks.push_back (std::move (midi));
        }

        juce::AudioBuffer<float> buffer (2, 256);
        long allocations = 0;

        for (auto& midi : blocks)
        {
            buffer.clear();
            const auto before = allocationsOnThisThread();
            rig.engine->processBlock (buffer, midi);
            allocations += allocationsOnThisThread() - before;
        }

        CHECK_MSG (allocations == 0, juce::String (AutoArticulationStyles::get (style).name) + ": "
                                       + juce::String (allocations) + " allocations");
    }
}

// AA-31
LUTHIER_TEST (AutoArticulationEngine, assistCostsAlmostNothing)
{
    // A 16-voice stress: two 8-note chords a beat, every beat, for 4 s, Poly.
    std::vector<Ev> events;

    for (int beat = 0; beat < 8; ++beat)
        for (int n : { 40, 45, 50, 55, 59, 64, 67, 71 })
        {
            events.push_back (noteOn (500.0 * beat, n + (beat % 3)));
            events.push_back (noteOff (500.0 * beat + 450.0, n + (beat % 3)));
        }

    auto timeIt = [&] (bool on)
    {
        double best = 1.0e9;

        for (int run = 0; run < 3; ++run)
        {
            EngineRig rig (on ? assistOn (AssistStyle::rock) : AutoArticulationSettings {}, PlayingMode::Poly);
            const auto t0 = std::chrono::steady_clock::now();
            rig.render (events, ms (4500));
            best = juce::jmin (best, std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());
        }

        return best;
    };

    const double off = timeIt (false), on = timeIt (true);

    // performance-budget.md 9's unit is 1 % of one core at 48 kHz; 0.02 units
    // of a 4.5 s render is 0.9 ms. Timing noise on a shared runner is larger
    // than that, so the bound is 3 % of the render (recorded in the coverage
    // doc), and the per-note cost is also measured directly below.
    CHECK_MSG (on <= off * 1.03 + 0.002, "off " + juce::String (off, 4) + " s, on " + juce::String (on, 4) + " s");

    AutoArticulator aa;
    TuningEngine tuning;
    tuning.prepare (kSr);
    tuning.setNumStrings (12);
    aa.prepare (kSr, 12);
    aa.setTuning (&tuning);
    aa.setSettings (assistOn (AssistStyle::rock));

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 10000; ++i)
        juce::ignoreUnused (aa.planSingle (40 + i % 40, 0.8, (juce::int64) i * 2000, false, 0));
    const double perNoteUs = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count() / 10000.0;

    CHECK_MSG (perNoteUs < 3.0, juce::String (perNoteUs, 3) + " us per note-on");
}

// AA-32
LUTHIER_TEST (AutoArticulationEngine, anAssistedPhraseExportsWithItsTechniques)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, 256);

    auto set = [&] (const char* id, float plain)
    {
        auto* p = processor->getState().getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };

    set (ParamIDs::aaEnabled, 1.0f);
    set (ParamIDs::aaStyle, (float) AssistStyle::rock);
    set (ParamIDs::playingMode, (float) PlayingMode::Mono);
    set (ParamIDs::macroHumanize, 0.0f);
    processor->getParameterBridge().applyAllNow();
    processor->getEngine().getAutoArticulator().setProbabilityOverride (1.0);

    // Palm-muted chugs with alternate strokes, a hammer-on, a pull-off, a slide,
    // an accented bend-into held for vibrato.
    std::vector<Ev> events;

    for (int i = 0; i < 4; ++i)
    {
        events.push_back (noteOn (125.0 * i, 40, 110));
        events.push_back (noteOff (125.0 * i + 70.0, 40));
    }

    for (const auto& e : legatoPair (60, 62, 150.0, 10.0, 90, 90)) events.push_back ({ e.sample + ms (1000), e.message });
    for (const auto& e : legatoPair (62, 60, 150.0, 10.0, 90, 90)) events.push_back ({ e.sample + ms (2000), e.message });
    for (const auto& e : legatoPair (60, 62, 150.0, 60.0, 90, 90)) events.push_back ({ e.sample + ms (3000), e.message });
    events.push_back (noteOn (4500, 69, 120));
    events.push_back (noteOff (5500, 69));

    std::stable_sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.sample < b.sample; });

    juce::AudioBuffer<float> buffer (2, 256);
    size_t next = 0;

    for (juce::int64 pos = 0; pos < ms (6200); pos += 256)
    {
        juce::MidiBuffer midi;

        while (next < events.size() && events[next].sample < pos + 256)
        {
            midi.addEvent (events[next].message, (int) (events[next].sample - pos));
            ++next;
        }

        buffer.clear();
        processor->processBlock (buffer, midi);
    }

    auto& capture = processor->getPerformanceCapture();
    capture.drain();

    PerformanceScore score;
    capture.toScore (score);

    const auto xml = NotationExporter().renderMusicXml (score);

    for (const char* element : { "<hammer-on", "<pull-off", "palm-mute", "<wavy-line", "<bend>",
                                 "<accent/>", "<up-bow/>" })
        CHECK_MSG (xml.contains (element), juce::String ("MusicXML has no ") + element);

    // A legato slide is MusicXML's glissando; a shift or a slide-in is <slide>.
    CHECK_MSG (xml.contains ("<slide") || xml.contains ("<glissando"), "MusicXML has no slide");

    // The Luthier profile carries aa= on the assisted NOTEs.
    const auto performance = capture.toPerformance (kSr);
    int tagged = 0;

    for (const auto& e : performance.getEvents())
        if (e.eventClass == LuthierEventClass::note && e.has ("aa"))
            ++tagged;

    CHECK_MSG (tagged >= 5, juce::String (tagged) + " NOTE events carry aa=");
}

// AA-33
LUTHIER_TEST (AutoArticulationEngine, aLuthierRoundTripIsNotArticulatedTwice)
{
    // A performance with NOTE events (the Luthier profile's) renders the same
    // with Assist on as with it off: its notes are pre-articulated.
    MidiPerformance performance (kSr);
    const int keys[] = { 60, 62, 64, 62, 60 };

    for (int i = 0; i < 5; ++i)
    {
        const auto at = (juce::int64) (i * 0.15 * kSr);
        performance.addMessage (at, juce::MidiMessage::noteOn (1, keys[i], (juce::uint8) 100));
        performance.addMessage (at + (juce::int64) (0.16 * kSr), juce::MidiMessage::noteOff (1, keys[i]));

        auto note = LuthierEvent::make (LuthierEventClass::note, at);
        note.setInt ("ch", 1).setInt ("key", keys[i]).setInt ("str", 1).setInt ("fret", keys[i] - 59);
        performance.addEvent (note);
    }

    const auto off = MidiImportTargets::render (performance, GuitarType::Stratocaster, kSr, 3.0);
    const auto on = MidiImportTargets::render (performance, GuitarType::Stratocaster, kSr, 3.0, assistOn (AssistStyle::blues));

    double diff = 0.0, ref = 0.0;

    for (int i = 0; i < off.getNumSamples(); ++i)
    {
        const double d = off.getSample (0, i) - on.getSample (0, i);
        diff += d * d;
        ref += (double) off.getSample (0, i) * off.getSample (0, i);
    }

    const double nullDb = 10.0 * std::log10 ((diff + 1.0e-30) / (ref + 1.0e-30));
    CHECK_MSG (nullDb <= -60.0, "null " + juce::String (nullDb, 1) + " dB");
    CHECK (ref > 0.0);
}

// AA-34
LUTHIER_TEST (AutoArticulationEngine, theFourParametersAreLastAndInOrder)
{
    combo::Rig rig;
    juce::StringArray ids;

    for (auto* p : rig.p().getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            ids.add (r->getParameterID());

    CHECK (ids.size() >= 4);
    const juce::StringArray expected { "aa_enabled", "aa_style", "aa_amount", "aa_rules" };

    // Other workstreams append after these; the four keep their order and are
    // together, after every parameter that existed before them.
    const int first = ids.indexOf ("aa_enabled");
    CHECK (first >= 0);

    for (int i = 0; i < 4; ++i)
        CHECK_MSG (ids[first + i] == expected[i], ids[first + i]);

    CHECK_MSG (first > ids.indexOf ("aux1_pre_circuit"), "aa_* is not after aux1_pre_circuit");

    // The defaults (6).
    auto* enabled = rig.param ("aa_enabled");
    auto* amount = rig.param ("aa_amount");
    auto* rules = rig.param ("aa_rules");
    CHECK (enabled->getDefaultValue() < 0.5f);
    CHECK_NEAR (amount->convertFrom0to1 (amount->getDefaultValue()), 60.0, 1.0e-3);
    CHECK_NEAR (rules->convertFrom0to1 (rules->getDefaultValue()), 511.0, 1.0e-3);
    CHECK (rules->isDiscrete());
}

// AA-35
LUTHIER_TEST (AutoArticulationEngine, presetSnapshotAndHostStateKeepTheFourValues)
{
    combo::Rig rig;
    rig.setIndex (ParamIDs::aaEnabled, 1);
    rig.setIndex (ParamIDs::aaStyle, (int) AssistStyle::jazz);
    rig.setPlain (ParamIDs::aaAmount, 35.0f);
    rig.setIndex (ParamIDs::aaRules, 0b101010101);

    auto check = [&] (combo::Rig& r, const juce::String& where)
    {
        CHECK_MSG (r.param ("aa_enabled")->getValue() > 0.5f, where);
        CHECK_MSG ((int) std::lround (r.param ("aa_style")->convertFrom0to1 (r.param ("aa_style")->getValue())) == (int) AssistStyle::jazz, where);
        CHECK_NEAR (r.param ("aa_amount")->convertFrom0to1 (r.param ("aa_amount")->getValue()), 35.0, 0.01);
        CHECK_MSG ((int) std::lround (r.param ("aa_rules")->convertFrom0to1 (r.param ("aa_rules")->getValue())) == 0b101010101, where);
    };

    // Preset.
    const auto preset = rig.p().getPresetManager().toVar ("AA-35");
    {
        combo::Rig other;
        CHECK (other.p().getPresetManager().fromVar (preset));
        check (other, "preset");
    }

    // Host state.
    juce::MemoryBlock state;
    rig.p().getStateInformation (state);
    {
        combo::Rig other;
        other.p().setStateInformation (state.getData(), (int) state.getSize());
        check (other, "host state");
    }

    // A preset without aa_* loads as off.
    {
        auto stripped = juce::JSON::parse (juce::JSON::toString (preset));

        if (auto* params = stripped.getProperty ("parameters", {}).getDynamicObject())
            for (const char* id : { "aa_enabled", "aa_style", "aa_amount", "aa_rules" })
                params->removeProperty (id);

        combo::Rig other;
        other.setIndex (ParamIDs::aaEnabled, 1);
        CHECK (other.p().getPresetManager().fromVar (stripped));
        CHECK_MSG (other.param ("aa_enabled")->getValue() < 0.5f, "an old preset left Assist on");
    }

    // Snapshots: switches at the midpoint, Amount interpolates (6).
    {
        auto& bank = rig.p().getSnapshots();
        bank.capture (0);
        rig.setIndex (ParamIDs::aaEnabled, 0);
        rig.setIndex (ParamIDs::aaStyle, (int) AssistStyle::rock);
        rig.setPlain (ParamIDs::aaAmount, 85.0f);
        rig.setIndex (ParamIDs::aaRules, 511);
        bank.capture (1);

        bank.setMorphSlots (0, 1);
        bank.setMorphEnabled (true);
        bank.setMorphPosition (0.25);

        CHECK (rig.param ("aa_enabled")->getValue() > 0.5f);
        CHECK_NEAR (rig.param ("aa_amount")->convertFrom0to1 (rig.param ("aa_amount")->getValue()), 35.0 + 0.25 * 50.0, 0.5);
        CHECK ((int) std::lround (rig.param ("aa_rules")->convertFrom0to1 (rig.param ("aa_rules")->getValue())) == 0b101010101);

        bank.setMorphPosition (0.75);
        CHECK (rig.param ("aa_enabled")->getValue() < 0.5f);
        CHECK ((int) std::lround (rig.param ("aa_rules")->convertFrom0to1 (rig.param ("aa_rules")->getValue())) == 511);
    }
}

// AA-36
LUTHIER_TEST (AutoArticulationEngine, freePlaysMetalAsRockAndKeepsMetal)
{
    // The effective settings.
    const auto free = Parameters::effectiveAssistSettings (true, (int) AssistStyle::metal, 60.0f, 3, Edition::free);
    CHECK (free.style == (int) AssistStyle::rock);
    CHECK (free.rules == AssistRule::all);
    CHECK (Parameters::effectiveAssistSettings (true, (int) AssistStyle::jazz, 60.0f, 3, Edition::free).style == (int) AssistStyle::cleanPop);
    CHECK (Parameters::effectiveAssistSettings (true, (int) AssistStyle::metal, 60.0f, 3, Edition::pro).style == (int) AssistStyle::metal);

    // The decision feed of Metal-in-Free equals Rock's.
    std::vector<Ev> events;
    for (int i = 0; i < 8; ++i)
    {
        events.push_back (noteOn (125.0 * i, 40 + (i % 2) * 7, 100));
        events.push_back (noteOff (125.0 * i + 70.0, 40 + (i % 2) * 7));
    }

    std::vector<AutoArticulationFeedEntry> feeds[2];

    for (int variant = 0; variant < 2; ++variant)
    {
        Editions::set (variant == 0 ? Edition::free : Edition::pro);
        combo::Rig rig;
        rig.setIndex (ParamIDs::aaEnabled, 1);
        rig.setIndex (ParamIDs::aaStyle, variant == 0 ? (int) AssistStyle::metal : (int) AssistStyle::rock);
        rig.setIndex (ParamIDs::playingMode, (int) PlayingMode::Mono);
        rig.setPlain (ParamIDs::macroHumanize, 0.0f);
        rig.apply();

        std::vector<combo::TimedMidi> timed;
        for (const auto& e : events)
            timed.push_back ({ (int) e.sample, e.message });

        rig.renderEvents (timed, (int) ms (1500), 0.0);
        rig.p().getEngine().getAutoArticulator().getFeed().drain ([&] (const AutoArticulationFeedEntry& e) { feeds[variant].push_back (e); });

        if (variant == 0)
        {
            // Saving writes Metal back unchanged; aa_rules is not automatable.
            const auto saved = rig.p().getPresetManager().toVar ("free");
            const auto style = saved.getProperty ("parameters", {}).getProperty ("aa_style", -1.0);
            CHECK_MSG ((int) std::lround (rig.param ("aa_style")->convertFrom0to1 ((float) (double) style)) == (int) AssistStyle::metal,
                       "saved style " + style.toString());
            CHECK ((int) std::lround (rig.param ("aa_style")->convertFrom0to1 (rig.param ("aa_style")->getValue())) == (int) AssistStyle::metal);
            CHECK (! rig.param ("aa_rules")->isAutomatable());
            CHECK (rig.param ("aa_rules")->getName (64).endsWith ("(Pro)"));
        }
    }

    Editions::set (Edition::pro);

    CHECK (! feeds[1].empty());
    CHECK (feeds[0].size() == feeds[1].size());

    for (size_t i = 0; i < juce::jmin (feeds[0].size(), feeds[1].size()); ++i)
        CHECK (feeds[0][i].sample == feeds[1][i].sample && feeds[0][i].label == feeds[1][i].label
               && feeds[0][i].string == feeds[1][i].string);
}
