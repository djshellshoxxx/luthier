/*  Slap (string-slap-technique.md 7, and the slap parts of bass-techniques.md
    12 it depends on), and the technique layer's shared MIDI front.

    The Slap and TechniqueTriggers suites need only the new classes. The
    SlapWiring suite needs the LuthierEngine edits that wire them in
    (getSlapEngine, getTechniqueTriggers, setSlapSettings and the
    processSubBlock / triggerNote hooks), and SlapPresets the parameters.

    Measurements are of what the instrument does: the strings' own level
    followers for "pitched content", the pre-body sum and the output for what
    is heard, the pool's per-block trigger records for when and how loud each
    fret contact was.
*/

#include "TestFramework.h"

#include "../DSP/Slap/SlapEngine.h"
#include "../DSP/Noise/ScrapeEngine.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    // A four-string bass's strings, engine order: 0 G, 1 D, 2 A, 3 E.
    constexpr int kBassE = 3, kBassA = 2;

    std::unique_ptr<SlapEngine> makeSlap (bool bassFamily = true, int numStrings = 4)
    {
        auto slap = std::make_unique<SlapEngine>();
        slap->prepare (kSr);
        slap->setInstrument (numStrings, bassFamily ? 864.0 : 648.0, bassFamily ? 20 : 22, bassFamily);
        return slap;
    }

    NoteOnEvent note (int stringIndex, double velocity, Technique technique = Technique::Pluck, int channel = 1)
    {
        NoteOnEvent e;
        e.stringIndex = stringIndex;
        e.velocity = velocity;
        e.technique = technique;
        e.midiChannel = channel;
        return e;
    }

    std::unique_ptr<LuthierEngine> makeEngine (GuitarType type)
    {
        auto engine = std::make_unique<LuthierEngine>();
        engine->prepare (kSr, kBlock);
        engine->setGuitarType (type);
        return engine;
    }

    struct Contact
    {
        juce::int64 at = 0;
        int stringIndex = 0;
        float level = 0.0f;
        float durationMs = 0.0f;
    };

    struct Render
    {
        std::vector<double> preBody, output, noiseBus;
        std::vector<Contact> contacts;       ///< fret-buzz class triggers, absolute sample
        std::vector<double> stringPeak;      ///< each string's own level, highest seen
    };

    /** Renders blocks; `midiFor (block)` supplies each block's MIDI. */
    template <typename MidiFor>
    Render renderEngine (LuthierEngine& engine, int blocks, MidiFor midiFor)
    {
        Render r;
        r.stringPeak.assign ((size_t) engine.getNumStrings(), 0.0);
        juce::AudioBuffer<float> block (2, kBlock);

        for (int b = 0; b < blocks; ++b)
        {
            block.clear();
            juce::MidiBuffer midi = midiFor (b);
            engine.processBlock (block, midi);

            const double* sum = engine.getPreBodyBuffer();
            const double* bus = engine.getNoiseBusData();

            for (int i = 0; i < kBlock; ++i)
            {
                r.preBody.push_back (sum[i]);
                r.noiseBus.push_back (bus[i]);
                r.output.push_back (block.getSample (0, i));
            }

            const auto& pool = engine.getNoisePool();

            for (int i = 0; i < pool.getNumBlockTriggers(); ++i)
            {
                const auto& t = pool.getBlockTrigger (i);

                if (t.noiseClass == NoiseClass::fretBuzz)
                    r.contacts.push_back ({ (juce::int64) b * kBlock + t.offset, t.stringIndex, t.level, t.durationMs });
            }

            for (int s = 0; s < engine.getNumStrings(); ++s)
                r.stringPeak[(size_t) s] = juce::jmax (r.stringPeak[(size_t) s], engine.getStringLevel (s));
        }

        return r;
    }

    Render renderEngine (LuthierEngine& engine, int blocks, const juce::MidiBuffer& first = {})
    {
        return renderEngine (engine, blocks, [&first] (int b) { return b == 0 ? first : juce::MidiBuffer(); });
    }

    juce::MidiBuffer noteOn (int noteNumber, int velocity, int offset = 0, int channel = 1)
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (channel, noteNumber, (juce::uint8) velocity), offset);
        return m;
    }

    double rmsOf (const std::vector<double>& x, size_t from = 0, size_t to = 0)
    {
        to = to == 0 ? x.size() : juce::jmin (to, x.size());

        if (to <= from)
            return 0.0;

        return rms (x.data() + from, (int) (to - from));
    }

    double peakOf (const std::vector<double>& x, size_t from = 0, size_t to = 0)
    {
        to = to == 0 ? x.size() : juce::jmin (to, x.size());
        double m = 0.0;

        for (size_t i = from; i < to; ++i)
            m = juce::jmax (m, std::abs (x[i]));

        return m;
    }

    /** Fraction of a signal's power, Hann-windowed, between two frequencies. */
    double bandShare (const std::vector<double>& x, double loHz, double hiHz)
    {
        constexpr int order = 13;
        constexpr int size = 1 << order;

        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) size * 2, 0.0f);
        const int n = juce::jmin (size, (int) x.size());

        for (int i = 0; i < n; ++i)
            data[(size_t) i] = (float) (x[(size_t) i] * 0.5 * (1.0 - std::cos (constants::kTwoPi * i / (n - 1))));

        fft.performFrequencyOnlyForwardTransform (data.data());

        double band = 0.0, total = 0.0;

        for (int k = 1; k < size / 2; ++k)
        {
            const double hz = k * kSr / size;
            const double power = (double) data[(size_t) k] * (double) data[(size_t) k];
            total += power;

            if (hz >= loHz && hz < hiHz)
                band += power;
        }

        return total > 0.0 ? band / total : 0.0;
    }

    double centroidOf (const std::vector<double>& x)
    {
        constexpr int order = 13;
        constexpr int size = 1 << order;

        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) size * 2, 0.0f);
        const int n = juce::jmin (size, (int) x.size());

        for (int i = 0; i < n; ++i)
            data[(size_t) i] = (float) (x[(size_t) i] * 0.5 * (1.0 - std::cos (constants::kTwoPi * i / (n - 1))));

        fft.performFrequencyOnlyForwardTransform (data.data());

        double weighted = 0.0, total = 0.0;

        for (int k = 1; k < size / 2; ++k)
        {
            const double power = (double) data[(size_t) k] * (double) data[(size_t) k];
            weighted += power * k * kSr / size;
            total += power;
        }

        return total > 0.0 ? weighted / total : 0.0;
    }

    /** The slap's own clacks: fret contacts shorter than a sensed buzz's 25 ms. */
    std::vector<Contact> clacksOn (const Render& r, int stringIndex)
    {
        std::vector<Contact> out;

        for (const auto& c : r.contacts)
            if (c.stringIndex == stringIndex && c.durationMs < 20.0f)
                out.push_back (c);

        return out;
    }

    SlapSettings bassThumb (TriggerSource trigger = TriggerSource::velocityZone)
    {
        SlapSettings s;
        s.armed = true;
        s.type = SlapType::thumb;
        s.trigger = trigger;
        s.ghostAuto = false;
        return s;
    }
}

//==============================================================================
//  The technique layer's MIDI front
//==============================================================================
LUTHIER_TEST (TechniqueTriggers, nothingArmedMeansNothingTaken)
{
    TechniqueTriggers front;
    juce::MidiBuffer filtered;
    filtered.ensureSize (4096);

    auto midi = noteOn (TechniqueKeyswitch::slap, 100);
    CHECK (&front.process (midi, filtered) == &midi);
    CHECK (front.getNumEvents() == 0);

    // A button pressed while disarmed is not held over for later.
    front.request (TechniqueId::slap, 0, true);
    front.process (midi, filtered);
    CHECK (front.getNumEvents() == 0);

    // The keyswitch table has no two techniques on one key.
    const int keys[] = { TechniqueKeyswitch::scrape, TechniqueKeyswitch::rakeDown, TechniqueKeyswitch::rakeUp,
                         TechniqueKeyswitch::slap, TechniqueKeyswitch::ghost, TechniqueKeyswitch::bodyTap,
                         TechniqueKeyswitch::palmSlap };

    for (size_t i = 0; i < std::size (keys); ++i)
        for (size_t j = i + 1; j < std::size (keys); ++j)
            CHECK (keys[i] != keys[j]);

    CHECK (TechniqueKeyswitch::scrape == ScrapeEngine::kScrapeKeyswitch);
}

LUTHIER_TEST (TechniqueTriggers, keyswitchesAreTakenAndBecomeEvents)
{
    TechniqueTriggers front;
    SlapSettings s;
    s.armed = true;
    s.trigger = TriggerSource::velocityZone;   // the main keyswitch is not listening
    front.configure (TechniqueId::slap, s.triggerConfig());

    juce::MidiBuffer filtered, midi;
    filtered.ensureSize (4096);
    midi.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::ghost, (juce::uint8) 90), 3);
    midi.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::slap, (juce::uint8) 90), 4);
    midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 90), 5);
    midi.addEvent (juce::MidiMessage::noteOff (1, TechniqueKeyswitch::ghost), 9);

    const auto& passed = front.process (midi, filtered);

    // Ghost (role 1) is always the slap's when armed; the main keyswitch only
    // with the keyswitch source, so note 15 passes on here with the ordinary note.
    CHECK (&passed == &filtered);
    CHECK (passed.getNumEvents() == 2);

    CHECK (front.getNumEvents() == 2);
    CHECK (front.getEvent (0).role == 1 && front.getEvent (0).on && front.getEvent (0).offset == 3);
    CHECK (front.getEvent (1).role == 1 && ! front.getEvent (1).on && front.getEvent (1).offset == 9);
    CHECK (! front.isHeld (TechniqueId::slap, 1));

    // With the keyswitch source, note 15 is the slap's too.
    s.trigger = TriggerSource::keyswitch;
    front.configure (TechniqueId::slap, s.triggerConfig());
    CHECK (front.process (midi, filtered).getNumEvents() == 1);
}

LUTHIER_TEST (TechniqueTriggers, zoneNotesAreTakenOnlyWhenTheyHaveNoPitch)
{
    TechniqueTriggers front;
    juce::MidiBuffer filtered;
    filtered.ensureSize (4096);

    SlapSettings s;
    s.armed = true;
    s.trigger = TriggerSource::mpeZone;
    front.configure (TechniqueId::slap, s.triggerConfig());

    auto midi = noteOn (40, 100, 0, s.zoneChannel);

    // A thumb slap on the zone still sounds...
    CHECK (&front.process (midi, filtered) == &midi);
    CHECK (front.getNumEvents() == 1);

    // ...a palm slap there has no note to sound.
    s.type = SlapType::palm;
    front.configure (TechniqueId::slap, s.triggerConfig());
    CHECK (front.process (midi, filtered).getNumEvents() == 0);
}

LUTHIER_TEST (TechniqueTriggers, aControllerFiresOnItsEdges)
{
    TechniqueTriggers front;
    juce::MidiBuffer filtered;

    SlapSettings s;
    s.armed = true;
    s.trigger = TriggerSource::controller;
    front.configure (TechniqueId::slap, s.triggerConfig());

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, s.triggerCc, 100), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, s.triggerCc, 110), 1);   // still down
    midi.addEvent (juce::MidiMessage::controllerEvent (1, s.ghostCc, 127), 2);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, s.triggerCc, 10), 3);

    CHECK (&front.process (midi, filtered) == &midi);   // controllers are never taken
    CHECK (front.getNumEvents() == 3);
    CHECK (front.isHeld (TechniqueId::slap, 1));
    CHECK (! front.isHeld (TechniqueId::slap, 0));
}

//==============================================================================
//  The SlapEngine on its own
//==============================================================================
LUTHIER_TEST (Slap, whatANoteBecomes)
{
    auto slap = makeSlap();

    // Disarmed on a bass: only the auto-ghost (bass-techniques 5) touches notes.
    SlapSettings s;
    slap->setSettings (s);
    CHECK (slap->classify (note (kBassE, 0.2), false).ghost);          // velocity 25 < 32
    CHECK (! slap->classify (note (kBassE, 0.2), false).strike);
    CHECK (! slap->classify (note (kBassE, 0.8), false).ghost);

    // ...and not at all on a guitar (bass-techniques 0.1).
    auto guitar = makeSlap (false, 6);
    guitar->setSettings (s);
    CHECK (! guitar->classify (note (5, 0.2), false).ghost);

    // Velocity zone: hard notes slap. Thumb on the mask (E and A by default),
    // pops on the rest.
    slap->setSettings (bassThumb());
    const auto thumb = slap->classify (note (kBassE, 1.0), false);
    const auto pop = slap->classify (note (0, 1.0), false);
    CHECK (thumb.strike && thumb.type == SlapType::thumb);
    CHECK (pop.strike && pop.type == SlapType::pop);
    CHECK (! slap->classify (note (kBassE, 0.5), false).strike);

    // Mute-then-slap is standard funk; a legato move, a tap or the bar is not slapped.
    CHECK (slap->classify (note (kBassE, 1.0, Technique::PalmMute), false).strike);
    CHECK (! slap->classify (note (kBassE, 1.0, Technique::HammerOn), false).strike);
    CHECK (! slap->classify (note (kBassE, 1.0, Technique::Tap), false).strike);
    CHECK (! slap->classify (note (kBassE, 1.0), true).strike);

    // The MPE zone: notes on its channel are slapped, others are not.
    auto zone = bassThumb (TriggerSource::mpeZone);
    slap->setSettings (zone);
    CHECK (slap->classify (note (kBassE, 0.6, Technique::Pluck, zone.zoneChannel), false).strike);
    CHECK (! slap->classify (note (kBassE, 0.6, Technique::Pluck, 1), false).strike);

    // Force: the thumb's is slap_strength, scaled a little by the note's dynamics.
    auto forceTest = bassThumb();
    forceTest.slapStrength = 0.6;
    slap->setSettings (forceTest);
    CHECK_NEAR (slap->classify (note (kBassE, 1.0), false).force, 0.6, 1.0e-12);
}

LUTHIER_TEST (Slap, theContactPointIsMeasuredFromTheLastFret)
{
    auto slap = makeSlap();   // 864 mm, 20 frets: the last fret is 272 mm from the saddle

    const double lastFret = 864.0 * std::pow (2.0, -20.0 / 12.0);
    CHECK_NEAR (slap->positionFraction (60.0, 0.0), (lastFret - 60.0) / 864.0, 1.0e-9);

    // Fretted higher, the same point is further along the shorter vibrating length.
    CHECK (slap->positionFraction (60.0, 5.0) > slap->positionFraction (60.0, 0.0));

    // A pop at 40 mm is nearer the neck end than a slap at 60.
    CHECK (slap->positionFraction (40.0, 0.0) > slap->positionFraction (60.0, 0.0));
}

LUTHIER_TEST (Slap, theClackIsTheFretBuzzGenerator)
{
    auto slap = makeSlap();
    FretBuzz buzz;

    auto settings = bassThumb();
    slap->setSettings (settings);
    const auto strike = slap->classify (note (kBassE, 1.0), false);

    // bass-techniques 12: fret contact 0.8 triggers FretBuzz, 0.0 does not.
    const auto clack = slap->makeContactBuzz (strike, true, 41.2, buzz);
    CHECK (clack.noiseClass == NoiseClass::fretBuzz);
    CHECK (clack.level > 0.0);

    settings.fretContact = 0.0;
    slap->setSettings (settings);
    CHECK (slap->makeContactBuzz (strike, true, 41.2, buzz).level == 0.0);

    // 7: a plain string is audible but clacks less.
    settings.fretContact = 0.8;
    slap->setSettings (settings);
    const double wound = slap->makeContactBuzz (strike, true, 41.2, buzz).level;
    const double plain = slap->makeContactBuzz (strike, false, 41.2, buzz).level;
    CHECK (plain > 0.0);
    CHECK_MSG (gainToDb (plain / wound) < -3.0, "plain string clack only " + juce::String (gainToDb (plain / wound), 1) + " dB");

    // Force scales it, until the contact is full.
    auto soft = strike;
    soft.force = 0.2;
    CHECK (slap->makeContactBuzz (soft, true, 41.2, buzz).level < wound);
}

LUTHIER_TEST (Slap, theUpStrokeComesAtItsGapAndItsRatio)
{
    auto slap = makeSlap();
    auto settings = bassThumb();
    settings.doubleThump = true;
    settings.reboundGapMs = 60.0;
    settings.upRatio = 0.65;
    slap->setSettings (settings);

    const auto strike = slap->classify (note (kBassE, 1.0), false);
    slap->noteStruck (strike, 1000);

    const auto gap = (juce::int64) std::llround (0.060 * kSr);
    CHECK (slap->getNumQueued() == 1);
    CHECK (! slap->hasDue (1000 + gap - 1));
    CHECK (slap->hasDue (1000 + gap));

    SlapAction up;
    CHECK (slap->popDue (1000 + gap, up));
    CHECK (up.strike.rebound && up.strike.stringIndex == kBassE);

    // Excitation 8's level law, inverted: the up-stroke peaks at the ratio.
    auto level = [] (double v) { return 0.10 + 0.90 * std::pow (v, 1.45); };
    CHECK_NEAR (gainToDb (level (up.strike.velocity) / level (strike.velocity)), gainToDb (0.65), 0.5);

    // It clacks at the ratio too, and brighter.
    FretBuzz buzz;
    const auto down = slap->makeContactBuzz (strike, true, 41.2, buzz);
    const auto back = slap->makeContactBuzz (up.strike, true, 41.2, buzz);
    CHECK_NEAR (gainToDb (back.level / down.level), gainToDb (0.65), 0.5);
    CHECK (back.endHz > down.endHz);

    // A rebound does not rebound again.
    slap->noteStruck (up.strike, 1000 + gap);
    CHECK (slap->getNumQueued() == 0);
}

LUTHIER_TEST (Slap, anotherTechniqueOnTheStringDropsTheUpStroke)
{
    // technique-cascade.md 2: slap x scrape conflicts, slap x tap alternates -
    // either way the string is someone else's now. A new strike replaces it too.
    auto slap = makeSlap();
    auto settings = bassThumb();
    settings.doubleThump = true;
    slap->setSettings (settings);

    const auto strike = slap->classify (note (kBassE, 1.0), false);

    slap->noteStruck (strike, 0);
    slap->preempt (kBassA);
    CHECK (slap->getNumQueued() == 1);   // a different string is left alone
    slap->preempt (kBassE);
    CHECK (slap->getNumQueued() == 0);

    slap->noteStruck (strike, 0);
    slap->noteStruck (strike, 100);
    CHECK (slap->getNumQueued() == 1);
}

LUTHIER_TEST (Slap, theButtonAndTheKeyswitchesQueueTheirStrikes)
{
    auto slap = makeSlap();
    TechniqueTriggers front;
    juce::MidiBuffer filtered;
    filtered.ensureSize (4096);

    auto settings = bassThumb (TriggerSource::keyswitch);
    slap->setSettings (settings);
    front.configure (TechniqueId::slap, settings.triggerConfig());

    // The keyswitch held: the notes under it are slapped.
    auto midi = noteOn (TechniqueKeyswitch::slap, 100);
    front.process (midi, filtered);
    slap->processBlock (kBlock, 0, front);
    CHECK (slap->isModifierHeld());
    CHECK (slap->classify (note (kBassE, 0.5), false).strike);

    // The button strikes the mask's strings now; body tap and palm slap
    // keyswitches work whatever the type.
    front.request (TechniqueId::slap, 0, true);
    juce::MidiBuffer more;
    more.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::bodyTap, (juce::uint8) 127), 10);
    more.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::palmSlap, (juce::uint8) 127), 20);
    front.process (more, filtered);
    slap->processBlock (kBlock, 256, front);

    CHECK_MSG (slap->getNumQueued() == 4, "queued " + juce::String (slap->getNumQueued()));   // E and A, a tap, a palm

    int strikes = 0, taps = 0, palms = 0;
    SlapAction a;

    while (slap->popDue (256 + kBlock, a))
    {
        strikes += a.kind == SlapAction::Kind::strike ? 1 : 0;
        taps += a.kind == SlapAction::Kind::bodyTap ? 1 : 0;
        palms += a.kind == SlapAction::Kind::palmSlap ? 1 : 0;

        if (a.kind == SlapAction::Kind::palmSlap)
            CHECK (a.mask == 0x0f && a.due == 256 + 20);
    }

    CHECK (strikes == 2 && taps == 1 && palms == 1);
}

LUTHIER_TEST (Slap, theBodyPartWeightsTheKnock)
{
    auto slap = makeSlap();

    auto knock = [&slap] (BodyPart part)
    {
        slap->reset();
        slap->startBodyTap (0.6, part);

        std::vector<double> x;

        while (slap->isBodyTapSounding())
            x.push_back (slap->nextBodyDrive());

        return x;
    };

    const auto top = knock (BodyPart::top), side = knock (BodyPart::side), back = knock (BodyPart::back);

    CHECK (top.size() == (size_t) (SlapEngine::kBodyTapSeconds * kSr));
    CHECK (peakOf (top) > 0.05 && peakOf (top) < 1.0);
    CHECK_MSG (centroidOf (back) < centroidOf (side) && centroidOf (back) < centroidOf (top),
               "back " + juce::String (centroidOf (back), 0) + " Hz, side " + juce::String (centroidOf (side), 0)
                 + " Hz, top " + juce::String (centroidOf (top), 0) + " Hz");

    // It dies away rather than stopping dead.
    CHECK (peakOf (top, top.size() - 480) < 0.001 * peakOf (top));
}

LUTHIER_TEST (Slap, idleAndActiveStayInBudget)
{
    // 7: idle < 0.05 %; a slap event's own work < 0.6 %. The excitation itself
    // is the string's, and costs what a pluck costs.
    auto slap = makeSlap();
    TechniqueTriggers front;
    double idleBest = 1.0e9, activeBest = 1.0e9;

    for (int run = 0; run < 3; ++run)
    {
        const auto start = juce::Time::getHighResolutionTicks();

        for (juce::int64 b = 0; b < 20000; ++b)
        {
            slap->processBlock (512, b * 512, front);

            for (int i = 0; i < 512; ++i)
                if (slap->hasDue (b * 512 + i) || slap->isBodyTapSounding())
                    slap->nextBodyDrive();
        }

        idleBest = juce::jmin (idleBest, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start));
    }

    const double idleRealtime = 20000 * 512 / kSr;
    CHECK_MSG (idleBest / idleRealtime < 0.0005, "idle " + juce::String (100.0 * idleBest / idleRealtime, 4) + "%");

    // One second holding four slap events: strikes classified, shaped, clacked,
    // rebounded, and a body tap rung out.
    FretBuzz buzz;
    auto settings = bassThumb();
    settings.doubleThump = true;
    slap->setSettings (settings);

    for (int run = 0; run < 5; ++run)
    {
        slap->reset();
        const auto start = juce::Time::getHighResolutionTicks();

        for (int event = 0; event < 4; ++event)
        {
            const auto strike = slap->classify (note (kBassE, 1.0), false);
            Excitation::Params p;
            slap->shapeExcitation (strike, 0.0, p);
            juce::ignoreUnused (slap->makeContactBuzz (strike, true, 41.2, buzz));
            slap->noteStruck (strike, event * 12000);
        }

        slap->startBodyTap (0.6, BodyPart::top);

        SlapAction a;
        double sink = 0.0;

        for (juce::int64 i = 0; i < (juce::int64) kSr; ++i)
        {
            while (slap->popDue (i, a))
                sink += a.strike.velocity;

            if (slap->isBodyTapSounding())
                sink += slap->nextBodyDrive();
        }

        activeBest = juce::jmin (activeBest, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start));
        CHECK (std::isfinite (sink));
    }

    CHECK_MSG (activeBest < 0.006 * 4, "four slap events took " + juce::String (activeBest * 1000.0, 3) + " ms of a second");
}

LUTHIER_TEST (Slap, theFactorySlapsAreWhatSectionFourSays)
{
    using P = SlapPreset;

    const auto standard = SlapSettings::fromPreset (P::bassStandard, 4, true);
    CHECK (standard.armed && standard.type == SlapType::thumb && standard.stringMask == ((1 << 2) | (1 << 3)));
    CHECK (standard.slapStrength == SlapSettings().slapStrength && standard.popStrength == SlapSettings().popStrength);

    const auto aggressive = SlapSettings::fromPreset (P::bassAggressive, 4, true);
    CHECK (aggressive.slapStrength > standard.slapStrength && aggressive.snapBack > standard.snapBack);

    const auto palm = SlapSettings::fromPreset (P::funkGuitarPalm, 6, false);
    CHECK (palm.type == SlapType::palm);

    auto slap = makeSlap (false, 6);
    slap->setSettings (palm);
    CHECK (slap->effectiveMask() == 0x3f);   // every string

    const auto body = SlapSettings::fromPreset (P::acousticBodyTap, 6, false);
    CHECK (body.type == SlapType::bodyTap && body.bodyPart == BodyPart::top);

    const auto fingerstyle = SlapSettings::fromPreset (P::percussiveFingerstyle, 6, false);
    CHECK (fingerstyle.type == SlapType::thumb && fingerstyle.stringMask == ((1 << 3) | (1 << 4) | (1 << 5)));

    for (int p = 0; p < (int) P::numPresets; ++p)
        CHECK (juce::String (SlapSettings::getPresetName ((P) p)).isNotEmpty());
}

//==============================================================================
//  Through the whole engine
//==============================================================================
LUTHIER_TEST (SlapWiring, aThumbSlapIsTheSameHoweverItIsFired)
{
    // 7: "bass thumb slap on low E at 60 mm, force 0.6 matches bass-techniques'
    // reference within 1 dB". The reference is the one slap both specs share:
    // the velocity zone, the keyswitch modifier and the button all land in the
    // same strike.
    auto viaPath = [] (int path)
    {
        auto engine = makeEngine (GuitarType::PrecisionBass);

        auto s = bassThumb (path == 0 ? TriggerSource::velocityZone
                                      : path == 1 ? TriggerSource::keyswitch : TriggerSource::buttonOnly);
        s.slapStrength = 0.6;
        s.slapPositionMm = 60.0;
        s.stringMask = 1 << kBassE;
        engine->setSlapSettings (s);

        juce::MidiBuffer midi;

        if (path == 1)
            midi.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::slap, (juce::uint8) 100), 0);

        if (path < 2)
            midi.addEvent (juce::MidiMessage::noteOn (1, 28, (juce::uint8) 127), 0);   // E1, the open low E
        else
            engine->getTechniqueTriggers().request (TechniqueId::slap, 0, true);

        return renderEngine (*engine, 60, midi);
    };

    const auto zone = viaPath (0), key = viaPath (1), button = viaPath (2);

    CHECK (rmsOf (zone.preBody) > 1.0e-3);
    CHECK_MSG (std::abs (gainToDb (rmsOf (key.preBody) / rmsOf (zone.preBody))) < 1.0,
               "keyswitch slap " + juce::String (gainToDb (rmsOf (key.preBody) / rmsOf (zone.preBody)), 2) + " dB off");
    CHECK_MSG (std::abs (gainToDb (rmsOf (button.preBody) / rmsOf (zone.preBody))) < 1.0,
               "button slap " + juce::String (gainToDb (rmsOf (button.preBody) / rmsOf (zone.preBody)), 2) + " dB off");

    // Each clacked once on the low E, and nothing played the keyswitch.
    CHECK (clacksOn (zone, kBassE).size() == 1);
    CHECK (clacksOn (key, kBassE).size() == 1);
}

LUTHIER_TEST (SlapWiring, theClackComesFromTheBuzzGenerator)
{
    // bass-techniques 12: fret contact 0.8 triggers the buzz generator in the
    // strike; 0.0 does not.
    auto clacks = [] (double contact)
    {
        auto engine = makeEngine (GuitarType::PrecisionBass);
        auto s = bassThumb();
        s.fretContact = contact;
        engine->setSlapSettings (s);

        // A few blocks, for humanised timing; the sensed buzz, which runs from
        // the next block, is told apart by its length.
        return clacksOn (renderEngine (*engine, 4, noteOn (28, 127, 30)), kBassE).size();
    };

    CHECK (clacks (0.8) == 1);
    CHECK (clacks (0.0) == 0);
}

LUTHIER_TEST (SlapWiring, aPalmSlapIsBroadbandAndPitchless)
{
    // 7: "palm slap on muted strings produces a broadband percussive event with
    // < -25 dB pitched content". The pitched content is the strings' own
    // vibration, which the hand stops; the event is the hand and the frets.
    auto engine = makeEngine (GuitarType::Stratocaster);
    SlapSettings s;
    s.armed = true;
    engine->setSlapSettings (s);

    const auto r = renderEngine (*engine, 30, noteOn (TechniqueKeyswitch::palmSlap, 110, 10));

    const double eventPeak = peakOf (r.preBody);
    double pitched = 0.0;

    for (double level : r.stringPeak)
        pitched = juce::jmax (pitched, level);

    CHECK_MSG (eventPeak > 1.0e-3, "the palm slap was silent");
    CHECK_MSG (gainToDb (juce::jmax (1.0e-12, pitched) / eventPeak) < -25.0,
               "pitched content " + juce::String (gainToDb (juce::jmax (1.0e-12, pitched) / eventPeak), 1) + " dB");

    // Broadband: real energy both under 400 Hz (the hand) and over 1 kHz (the frets).
    CHECK_MSG (bandShare (r.preBody, 20.0, 400.0) > dbToGain (-25.0), "no low thump");
    CHECK_MSG (bandShare (r.preBody, 1000.0, 12000.0) > dbToGain (-25.0), "no clack");

    // A clack on every string, at the keyswitch's sample.
    int strings = 0;

    for (int str = 0; str < engine->getNumStrings(); ++str)
        strings += clacksOn (r, str).empty() ? 0 : 1;

    CHECK (strings == engine->getNumStrings());

    // And over ringing strings it is the funk chuck: they stop.
    auto chuck = makeEngine (GuitarType::Stratocaster);
    chuck->setSlapSettings (s);

    juce::MidiBuffer chord;
    for (int n : { 40, 45, 50, 55, 59, 64 })
        chord.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);

    renderEngine (*chuck, 40, chord);

    double before = 0.0;
    for (int str = 0; str < chuck->getNumStrings(); ++str)
        before = juce::jmax (before, chuck->getStringLevel (str));

    // The level followers release over 60 ms, so give them 400 ms to show it.
    renderEngine (*chuck, 75, noteOn (TechniqueKeyswitch::palmSlap, 110, 0));

    double after = 0.0;
    for (int str = 0; str < chuck->getNumStrings(); ++str)
        after = juce::jmax (after, chuck->getStringLevel (str));

    CHECK_MSG (gainToDb (juce::jmax (1.0e-12, after) / before) < -40.0,
               "the strings under the palm are only " + juce::String (-gainToDb (juce::jmax (1.0e-12, after) / before), 1)
                 + " dB down after 400 ms");
}

LUTHIER_TEST (SlapWiring, aBodyTapLeavesTheStringsAlone)
{
    // 7: "body tap drives body modes without exciting strings (< -60 dB on
    // string outputs when strings are damped)".
    // The internal mic hears the body; the engine's own default is all piezo,
    // which hears the saddle (the bridge's string sum) and not a tap on the top.
    auto baseline = makeEngine (GuitarType::Dreadnought);
    baseline->getPickupEngine().setPiezoMicBlend (0.6);
    const auto quiet = renderEngine (*baseline, 40);

    auto engine = makeEngine (GuitarType::Dreadnought);
    engine->getPickupEngine().setPiezoMicBlend (0.6);
    SlapSettings s;
    s.armed = true;
    s.type = SlapType::bodyTap;
    engine->setSlapSettings (s);

    const auto tapped = renderEngine (*engine, 40, noteOn (TechniqueKeyswitch::slap, 110, 0));

    std::vector<double> heard (tapped.output.size());

    for (size_t i = 0; i < heard.size(); ++i)
        heard[i] = tapped.output[i] - quiet.output[i];

    const double tapPeak = peakOf (heard);
    double strings = 0.0;

    for (double level : tapped.stringPeak)
        strings = juce::jmax (strings, level);

    CHECK_MSG (tapPeak > 1.0e-5, "the body tap did not reach the output");
    CHECK_MSG (gainToDb (juce::jmax (1.0e-12, strings) / tapPeak) < -60.0, "the body tap moved a string");
    CHECK_MSG (peakOf (tapped.noiseBus) > 0.0, "the body tap is not on Aux 8");

    // The pre-body sum is the strings: untouched.
    CHECK (peakOf (tapped.preBody) == peakOf (quiet.preBody));
}

LUTHIER_TEST (SlapWiring, aGhostIsAThumpWithNoPitch)
{
    // 7 and bass-techniques 5: with the fretting hand resting on the string
    // the strike is over almost as soon as it lands; a slap rings on.
    auto slapWith = [] (bool ghostMode)
    {
        auto engine = makeEngine (GuitarType::PrecisionBass);
        auto s = bassThumb();
        s.ghostMode = ghostMode;
        s.fretContact = 0.0;   // the string alone, without the clack
        engine->setSlapSettings (s);

        return renderEngine (*engine, 60, noteOn (33, 127, 0));   // A1, the open A
    };

    auto ghost = slapWith (true), slap = slapWith (false);

    // "No clear pitch" is about what is heard: the string's 7 Hz output DC
    // blocker leaves a sub-audio tail (a 23 ms time constant) behind any note
    // stopped this fast, so the sum is high-passed at 20 Hz before measuring.
    auto highPass = [] (std::vector<double>& x)
    {
        const double r = 1.0 - constants::kTwoPi * 20.0 / kSr;
        double x1 = 0.0, y1 = 0.0;

        for (auto& v : x)
        {
            const double y = v - x1 + r * y1;
            x1 = v;
            y1 = y;
            v = y;
        }
    };

    highPass (ghost.preBody);
    highPass (slap.preBody);

    // The note lands after the interpreter's chord window and humanised
    // timing, so the windows are measured from where the strike starts.
    auto onsetOf = [] (const std::vector<double>& x)
    {
        for (size_t i = 0; i < x.size(); ++i)
            if (std::abs (x[i]) > 1.0e-4)
                return i;

        return x.size();
    };

    const size_t ghostOn = onsetOf (ghost.preBody), slapOn = onsetOf (slap.preBody);
    const size_t attack = (size_t) (0.03 * kSr), from = (size_t) (0.15 * kSr), to = (size_t) (0.2 * kSr);

    CHECK_MSG (ghostOn < ghost.preBody.size() && slapOn < slap.preBody.size(), "a strike never sounded");
    CHECK_MSG (ghostOn + to <= ghost.preBody.size(), "the ghost landed too late to measure");

    const double ghostTail = gainToDb (juce::jmax (1.0e-12, rmsOf (ghost.preBody, ghostOn + from, ghostOn + to))
                                       / juce::jmax (1.0e-12, peakOf (ghost.preBody, ghostOn, ghostOn + attack)));
    const double slapTail = gainToDb (juce::jmax (1.0e-12, rmsOf (slap.preBody, slapOn + from, slapOn + to))
                                      / juce::jmax (1.0e-12, peakOf (slap.preBody, slapOn, slapOn + attack)));

    CHECK_MSG (peakOf (ghost.preBody, ghostOn, ghostOn + attack) > 1.0e-4, "the ghost made no thump");
    CHECK_MSG (ghostTail < -40.0, "a ghost still sounds " + juce::String (ghostTail, 1) + " dB at 150 ms");
    CHECK_MSG (slapTail > -25.0, "an ordinary slap has died to " + juce::String (slapTail, 1) + " dB at 150 ms");
}

LUTHIER_TEST (SlapWiring, theDoubleThumpComesBackAtItsGap)
{
    // 7: "rebound gap matches configured value +- 3 ms"; bass-techniques 12:
    // "second at the ratio's level within 0.5 dB". Both read off the clacks.
    auto engine = makeEngine (GuitarType::PrecisionBass);
    auto s = bassThumb();
    s.doubleThump = true;
    s.reboundGapMs = 60.0;
    s.upRatio = 0.65;
    engine->setSlapSettings (s);

    const auto r = renderEngine (*engine, 30, noteOn (28, 127, 17));
    const auto clacks = clacksOn (r, kBassE);

    CHECK_MSG (clacks.size() == 2, juce::String ((int) clacks.size()) + " clacks for a double thump");

    if (clacks.size() == 2)
    {
        const double gapMs = 1000.0 * (double) (clacks[1].at - clacks[0].at) / kSr;
        CHECK_MSG (std::abs (gapMs - 60.0) <= 3.0, "gap " + juce::String (gapMs, 2) + " ms");
        CHECK_NEAR (gainToDb (clacks[1].level / clacks[0].level), gainToDb (0.65), 0.5);
    }
}

LUTHIER_TEST (SlapWiring, slapAndScrapeTakeTheStringFromEachOther)
{
    // technique-cascade.md 2: scrape x slap on one string conflict, the latest wins.
    auto engine = makeEngine (GuitarType::Stratocaster);

    ScrapeGesture g;
    g.stringIndex = 5;
    g.durationMs = 800.0;
    engine->getScrapeEngine().trigger (g);
    renderEngine (*engine, 10);
    CHECK (engine->getScrapeEngine().isStringMoving (5));

    SlapSettings s;
    s.armed = true;
    s.trigger = TriggerSource::buttonOnly;
    s.stringMask = 1 << 5;
    s.doubleThump = true;
    engine->setSlapSettings (s);
    engine->getTechniqueTriggers().request (TechniqueId::slap, 0, true);
    renderEngine (*engine, 4);

    CHECK_MSG (! engine->getScrapeEngine().isStringActive (5), "the slap left the scrape running");
    CHECK (engine->getSlapEngine().getNumQueued() == 1);   // the up-stroke

    // A scrape arriving before the up-stroke takes the string back.
    engine->getScrapeEngine().trigger (g);
    renderEngine (*engine, 1);
    CHECK_MSG (engine->getSlapEngine().getNumQueued() == 0, "the scrape left the slap's up-stroke queued");

    // And a tap drops it too (slap x tap alternate): strike again, and tap
    // before the up-stroke is due.
    engine->getTechniqueTriggers().request (TechniqueId::slap, 0, true);
    renderEngine (*engine, 2);
    CHECK (engine->getSlapEngine().getNumQueued() == 1);

    NoteOnEvent tap;
    tap.stringIndex = 5;
    tap.technique = Technique::Tap;
    tap.pitchHz = 110.0;
    engine->triggerNoteNow (tap);
    CHECK (engine->getSlapEngine().getNumQueued() == 0);
}

LUTHIER_TEST (SlapWiring, thePlainHighEIsAudibleButClacksLess)
{
    // 7: "slap on plain string: audible but with reduced buzz component".
    auto strikeString = [] (int stringIndex)
    {
        auto engine = makeEngine (GuitarType::Stratocaster);
        SlapSettings s;
        s.armed = true;
        s.trigger = TriggerSource::buttonOnly;
        s.stringMask = 1 << stringIndex;
        engine->setSlapSettings (s);
        engine->getTechniqueTriggers().request (TechniqueId::slap, 0, true);
        return renderEngine (*engine, 20);
    };

    const auto plain = strikeString (0), wound = strikeString (5);
    const auto plainClack = clacksOn (plain, 0), woundClack = clacksOn (wound, 5);

    CHECK (peakOf (plain.preBody) > 1.0e-3);
    CHECK (plainClack.size() == 1 && woundClack.size() == 1);

    if (plainClack.size() == 1 && woundClack.size() == 1)
        CHECK_MSG (plainClack[0].level < woundClack[0].level * dbToGain (-3.0),
                   "plain " + juce::String (plainClack[0].level, 4) + ", wound " + juce::String (woundClack[0].level, 4));
}

//==============================================================================
//  Presets
//==============================================================================
LUTHIER_TEST (SlapPresets, everySlapFieldRoundTrips)
{
    // 7: "preset save / restore round-trips every added field".
    const char* ids[] = {
        ParamIDs::slapStrength, ParamIDs::slapPositionMm, ParamIDs::slapThumbHardness, ParamIDs::slapFretContact,
        ParamIDs::popStrength, ParamIDs::popPositionMm, ParamIDs::doubleThumpEnabled, ParamIDs::doubleThumpUpRatio,
        ParamIDs::ghostLevel, ParamIDs::ghostDamping, ParamIDs::ghostAuto, ParamIDs::ghostVelocityThreshold,
        ParamIDs::slapArmed, ParamIDs::slapType, ParamIDs::slapTrigger, ParamIDs::slapVelocityZone,
        ParamIDs::slapTriggerCc, ParamIDs::slapGhostCc, ParamIDs::slapForce, ParamIDs::slapPalmPositionMm,
        ParamIDs::slapStringMask, ParamIDs::slapGhostMode, ParamIDs::slapReboundGap, ParamIDs::slapSnapBack,
        ParamIDs::slapBodyPart
    };

    auto from = std::make_unique<LuthierAudioProcessor>();
    auto to = std::make_unique<LuthierAudioProcessor>();

    std::vector<float> written;

    for (auto* id : ids)
    {
        auto* p = from->getState().getParameter (id);
        CHECK_MSG (p != nullptr, juce::String ("no parameter ") + id);

        if (p == nullptr)
        {
            written.push_back (0.0f);
            continue;
        }

        // Somewhere that is not the default, on the parameter's own grid.
        const float target = p->convertTo0to1 (p->convertFrom0to1 (p->getDefaultValue() < 0.5f ? 0.8f : 0.2f));
        p->setValueNotifyingHost (target);
        written.push_back (p->getValue());
        CHECK_MSG (std::abs (written.back() - p->getDefaultValue()) > 1.0e-4f, juce::String (id) + " did not move");
    }

    to->getPresetManager().fromVar (from->getPresetManager().toVar ("slap round trip"));

    for (size_t i = 0; i < std::size (ids); ++i)
        if (auto* p = to->getState().getParameter (ids[i]))
            CHECK_MSG (std::abs (p->getValue() - written[i]) < 1.0e-4f, juce::String (ids[i]) + " did not round-trip");
}
