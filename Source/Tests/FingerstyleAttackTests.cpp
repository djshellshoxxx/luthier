/*  fingerstyle-attack.md 9 (REALISM-B): what touches the string and how it
    lets go.

    The excitation-level tests (FA-03, FA-04) render Excitation directly; the
    rest play notes through the engine and read the string's own output, the
    noise pool's triggers, and the Params the engine recorded for the note.
*/

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../PhysicalRange.h"
#include "../Rhythm/RhythmEngine.h"
#include "../UI/RightHandGroup.h"

using namespace luthier;
using namespace luthier::tests;

long luthierAllocationsOnThisThread() noexcept;   // CircuitTests.cpp

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    std::unique_ptr<LuthierEngine> makeEngine (GuitarType type = GuitarType::Dreadnought)
    {
        auto e = std::make_unique<LuthierEngine>();
        e->prepare (kSr, kBlock);
        e->setGuitarType (type);
        e->getCharacterEngine().setEnabled (false);
        e->getTapBuffers().setPerStringWanted (true);

        // The neighbours stay out of it unless a test asks.
        StringInteractionSettings si;
        si.adjacentMute = 0.0;
        e->setStringInteraction (si);
        e->reset();
        return e;
    }

    RightHandSettings allStrings (RhTool tool)
    {
        RightHandSettings h;
        h.stringTool.fill (tool);
        return h;
    }

    NoteOnEvent note (LuthierEngine& e, int s, double fret, double velocity = 0.8, int finger = -1)
    {
        NoteOnEvent n;
        n.stringIndex = s;
        n.midiNote = 50;
        n.fretPosition = fret;
        n.velocity = velocity;
        n.finger = finger;
        n.pitchHz = e.getTuningEngine().computeFrequency (s, fret, 0.0);
        return n;
    }

    std::vector<double> renderString (LuthierEngine& e, int s, double seconds)
    {
        std::vector<double> out;
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        for (int b = 0; b < (int) std::ceil (seconds * kSr / kBlock); ++b)
        {
            buffer.clear();
            e.processBlock (buffer, midi);

            for (int i = 0; i < kBlock; ++i)
                out.push_back (e.getTapBuffers().stringRead (s)[i]);
        }

        return out;
    }

    /** Spectral centroid of [0, len) seconds. */
    double centroid (const std::vector<double>& x, double len = 0.05)
    {
        constexpr int order = 13;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) (2 << order), 0.0f);
        const int n = juce::jmin ((int) (len * kSr), (int) x.size(), 1 << order);

        for (int i = 0; i < n; ++i)
            data[(size_t) i] = (float) x[(size_t) i];

        fft.performFrequencyOnlyForwardTransform (data.data());

        double num = 0.0, den = 0.0;

        for (int k = 1; k < (1 << (order - 1)); ++k)
        {
            const double f = k * kSr / (double) (1 << order);
            num += f * data[(size_t) k];
            den += data[(size_t) k];
        }

        return num / juce::jmax (1.0e-12, den);
    }

    /** Power-weighted centroid of a whole excitation. */
    double powerCentroid (const std::vector<double>& x)
    {
        constexpr int order = 13;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) (2 << order), 0.0f);

        for (int i = 0; i < juce::jmin ((int) x.size(), 1 << order); ++i)
            data[(size_t) i] = (float) x[(size_t) i];

        fft.performFrequencyOnlyForwardTransform (data.data());
        double num = 0.0, den = 0.0;

        for (int k = 1; k < (1 << (order - 1)); ++k)
        {
            const double pw = (double) data[(size_t) k] * data[(size_t) k];
            num += k * kSr / (double) (1 << order) * pw;
            den += pw;
        }

        return num / juce::jmax (1.0e-30, den);
    }

    std::vector<double> excitationOf (Excitation::Params p, juce::uint64 seed);

    /*  The contact's own spectrum: the note's recorded Params rendered again
        at the string's loop length. DECISION (REALISM-B): FA-01, FA-02 and
        FA-12 measure here, not on the string's output - the output's first
        50 ms is dominated by the string's own partials (its centroid moved
        1 % between nail and flesh while the contact's moved 87 %). */
    double contactCentroid (LuthierEngine& e, int s)
    {
        auto p = e.getLastExcitation (s);
        p.delaySamples = e.getString (s).getCurrentDelaySamples();
        p.startDelaySamples = 0;
        return powerCentroid (excitationOf (p, 1));
    }

    double peakOf (const std::vector<double>& x)
    {
        double p = 0.0;
        for (double v : x) p = juce::jmax (p, std::abs (v));
        return p;
    }

    double t60Of (LuthierEngine& e, int s)
    {
        const auto& str = e.getString (s);
        return -6.907755 * str.getCurrentDelaySamples() / (kSr * std::log (juce::jlimit (1.0e-9, 0.999999999, str.getLoopGain())));
    }

    /** Renders one Excitation and returns its samples. */
    std::vector<double> excitationOf (Excitation::Params p, juce::uint64 seed)
    {
        Excitation x;
        x.prepare (kSr);
        RtRandom r { seed };
        x.trigger (p, r);
        std::vector<double> v;
        while (x.isActive()) v.push_back (x.next());
        return v;
    }

    double magnitudeAt (const std::vector<double>& x, double hz)
    {
        double re = 0.0, im = 0.0;

        for (size_t i = 0; i < x.size(); ++i)
        {
            re += x[i] * std::cos (juce::MathConstants<double>::twoPi * hz * (double) i / kSr);
            im += x[i] * std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / kSr);
        }

        return std::sqrt (re * re + im * im);
    }

    void set (LuthierAudioProcessor& processor, const juce::String& id, float plain)
    {
        if (auto* p = processor.getState().getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    float plainOf (LuthierAudioProcessor& processor, const juce::String& id)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id));
        return p != nullptr ? p->convertFrom0to1 (p->getValue()) : -1.0f;
    }
}

//==============================================================================
LUTHIER_TEST (FingerstyleAttack, FA01_FA02_nailIsBrighterWithNoStep)
{
    auto centroidAt = [] (double b)
    {
        auto e = makeEngine();
        e->setRightHand (allStrings (RhTool::finger));
        e->setNailVsFlesh (b);
        e->triggerNoteNow (note (*e, 2, 2.0));
        return contactCentroid (*e, 2);
    };

    const double flesh = centroidAt (0.0), nail = centroidAt (1.0);
    CHECK_MSG (nail / flesh >= 1.6, "nail / flesh centroid " + juce::String (nail / flesh, 3));

    double previous = centroidAt (0.45), worst = 0.0;

    for (int k = 46; k <= 55; ++k)
    {
        const double c = centroidAt (k / 100.0);
        worst = juce::jmax (worst, std::abs (c / previous - 1.0));
        previous = c;
    }

    CHECK_MSG (worst <= 0.04, "a 0.01 step of nail_vs_flesh moved the centroid " + juce::String (100.0 * worst, 2) + " %");
}

LUTHIER_TEST (FingerstyleAttack, FA03_releaseTimeSetsTheCutoff)
{
    for (const double tauMs : { 0.04, 0.0723, 0.2 })
    {
        Excitation::Params p;
        p.material = Excitation::Material::Fingertip;
        p.velocity = 0.478;       // velocity trim at unity
        p.brightness = 0.364;     // brightness trim at unity
        p.delaySamples = 20.0;
        p.pluckPosition = 0.02;     // a one-sample comb: no notch in band
        p.exactPluckComb = true;    // and the filters' tails kept whole
        p.nailVsFlesh = 0.0;
        p.releaseSeconds = tauMs * 0.001;

        // The same excitation with the contact wide open isolates the lowpass.
        auto wide = p;
        wide.releaseSeconds = 1.0e-7;

        const auto shaped = excitationOf (p, 7), open = excitationOf (wide, 7);
        const double expected = 1.0 / (juce::MathConstants<double>::twoPi * tauMs * 0.001);
        const double ref = magnitudeAt (shaped, 100.0) / magnitudeAt (open, 100.0);

        double minus3 = -1.0;

        for (double f = 200.0; f < 20000.0; f *= 1.005)
        {
            const double r = magnitudeAt (shaped, f) / juce::jmax (1.0e-12, magnitudeAt (open, f)) / ref;

            if (r < std::pow (10.0, -3.0 / 20.0))
            {
                minus3 = f;
                break;
            }
        }

        CHECK_MSG (std::abs (minus3 / expected - 1.0) <= 0.15,
                   "tau " + juce::String (tauMs, 4) + " ms: -3 dB at " + juce::String (minus3, 0)
                     + " Hz, expected " + juce::String (expected, 0));
    }
}

LUTHIER_TEST (FingerstyleAttack, FA04_defaultsReproduceTheTable)
{
    for (const double b : { 0.0, 1.0 })
    {
        Excitation::Params old;
        old.material = b > 0.5 ? Excitation::Material::Fingernail : Excitation::Material::Fingertip;
        old.nailVsFlesh = b;
        old.delaySamples = 300.0;

        auto profile = old;
        profile.material = Excitation::Material::Fingertip;
        profile.releaseSeconds = (b > 0.5 ? 0.0227 : 0.0723) * 0.001;

        const auto a = excitationOf (old, 7), c = excitationOf (profile, 7);
        double diff = 0.0, ref = 0.0;

        for (size_t i = 0; i < juce::jmin (a.size(), c.size()); ++i)
        {
            diff += (a[i] - c[i]) * (a[i] - c[i]);
            ref += a[i] * a[i];
        }

        const double nullDb = 10.0 * std::log10 (juce::jmax (1.0e-30, diff) / ref);
        CHECK_MSG (a.size() == c.size() && nullDb <= -60.0,
                   "b = " + juce::String (b) + ": the profile nulls against the table to only " + juce::String (nullDb, 1) + " dB");
    }
}

LUTHIER_TEST (FingerstyleAttack, FA05_thePickPathIsUntouched)
{
    /*  With every string on Global - the default - a pick note's Params are
        exactly what they were: no profile, no rest terms, no delay. (The
        factory presets' before/after renders are compared in
        docs/coverage/REALISM-B.md.) */
    auto e = makeEngine (GuitarType::Stratocaster);
    e->triggerNoteNow (note (*e, 3, 5.0));
    const auto& p = e->getLastExcitation (3);

    CHECK (p.releaseSeconds < 0.0);
    CHECK (p.levelScale == 1.0 && p.contactScale == 1.0 && p.brightnessScale == 1.0);
    CHECK (p.startDelaySamples == 0);
    CHECK (! p.exactPluckComb && ! p.isolateHarmonic);
    CHECK (e->getLastTool (3) == RhTool::global);
    CHECK (e->getString (3).getCouplingSendScale() == 1.0);
}

LUTHIER_TEST (FingerstyleAttack, FA06_restStrokeLevelAndTone)
{
    struct Result { double peak, centroid, t60; };

    auto render = [] (RhStroke stroke)
    {
        auto e = makeEngine();
        auto h = allStrings (RhTool::finger);
        h.stroke = stroke;
        e->setRightHand (h);
        e->triggerNoteNow (note (*e, 1, 3.0));
        const auto out = renderString (*e, 1, 0.3);
        return Result { peakOf (out), centroid (out), t60Of (*e, 1) };
    };

    const auto free = render (RhStroke::free), rest = render (RhStroke::rest);
    const double louder = juce::Decibels::gainToDecibels (rest.peak / free.peak);

    CHECK_MSG (louder >= 1.5 && louder <= 3.0, "rest is " + juce::String (louder, 2) + " dB louder");
    CHECK_MSG (rest.centroid / free.centroid <= 0.95 && rest.centroid / free.centroid >= 0.80,
               "rest's centroid is " + juce::String (rest.centroid / free.centroid, 3) + "x");
    CHECK_MSG (rest.t60 / free.t60 >= 0.8 && rest.t60 / free.t60 <= 0.9, "rest's T60 is " + juce::String (rest.t60 / free.t60, 3) + "x");
}

LUTHIER_TEST (FingerstyleAttack, FA07_restDampsTheNeighbour)
{
    double lastT60 = 0.0;

    // The neighbour's level with the stroke against its level without it, at the same moment.
    auto levelAfter = [&lastT60] (RhTool tool, RhStroke stroke, int struck, int watched, bool strike)
    {
        auto e = makeEngine();
        auto h = allStrings (tool);
        h.stroke = stroke;
        e->setRightHand (h);

        /*  The neighbour's own decay: with the bridge coupled, the struck
            note re-drives it (its partials share the D string's) at the
            chuck's receptivity, which is the struck string's sound, not the
            finger's damping. */
        e->getCouplingMatrix().setAmount (0.0);

        e->triggerNoteNow (note (*e, watched, 0.0));
        NoteOffEvent off;
        off.stringIndex = watched;
        off.letRing = true;
        e->releaseNoteNow (off);

        renderString (*e, watched, 0.3);

        if (strike)
            e->triggerNoteNow (note (*e, struck, 2.0));

        /*  DECISION (REALISM-B): measured over the 10 ms window that ends
            three of the neighbour's periods plus 10 ms after the stroke
            (30 ms on the open D), not "within 10 ms".
            The lumped loop damps once per round trip, so a damping change
            reaches the output one period after it lands and has taken
            about 9 dB two periods in; the draft's flat 10 ms is one and a
            half periods of the open D. */
        const double period = e->getString (watched).getCurrentDelaySamples();
        const auto at = (size_t) juce::jmax (960.0, 3.0 * period + 480.0);

        // Rendered at least that far: a fixed 30 ms is 1536 samples, and the
        // low E's window ends at 2229 - the read ran off the end of the
        // buffer (ASan, BETA_TEST_REPORT B-22).
        const auto after = renderString (*e, watched, juce::jmax (0.03, (double) at / kSr + 0.005));
        jassert (after.size() >= at);
        // The neighbour's own partials, Hann-windowed over the 10 ms before
        // then: the output DC blocker's slow tail (a killed loop's removed
        // offset, below 10 Hz) is not the string ringing.
        const double f0 = e->getString (watched).getCurrentFrequency();
        double level = 0.0;

        for (int k = 1; k <= 4; ++k)
        {
            double re = 0.0, im = 0.0;

            for (size_t i = at - 480; i < at; ++i)
            {
                const double w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) (i - (at - 480)) / 479.0);
                re += after[i] * w * std::cos (juce::MathConstants<double>::twoPi * k * f0 * (double) i / kSr);
                im += after[i] * w * std::sin (juce::MathConstants<double>::twoPi * k * f0 * (double) i / kSr);
            }

            level += re * re + im * im;
        }

        lastT60 = t60Of (*e, watched);
        return std::sqrt (level);
    };

    auto drop = [&levelAfter] (RhTool tool, RhStroke stroke, int struck, int watched)
    {
        return juce::Decibels::gainToDecibels (levelAfter (tool, stroke, struck, watched, false)
                                                 / juce::jmax (1.0e-12, levelAfter (tool, stroke, struck, watched, true)));
    };

    const double rest = drop (RhTool::finger, RhStroke::rest, 2, 3);
    const double free = drop (RhTool::finger, RhStroke::free, 2, 3);
    CHECK_MSG (rest >= 12.0, "a finger rest stroke on 2 took only " + juce::String (rest, 1) + " dB off 3 (T60 now "
                               + juce::String (lastT60 * 1000.0, 1) + " ms)");
    CHECK_MSG (free < 1.0, "a free stroke took " + juce::String (free, 1) + " dB off 3");

    // The thumb moves toward the treble: a thumb rest on 4 lands on 3, not 5.
    CHECK (drop (RhTool::thumb, RhStroke::rest, 4, 3) >= 12.0);
    CHECK (drop (RhTool::thumb, RhStroke::rest, 4, 5) < 1.0);
}

LUTHIER_TEST (FingerstyleAttack, FA08_autoStroke)
{
    auto restOf = [] (std::initializer_list<int> keys, int velocity, int watchString)
    {
        auto e = makeEngine();
        auto h = allStrings (RhTool::finger);
        h.stroke = RhStroke::automatic;
        e->setRightHand (h);
        e->getMidiInterpreter().setPlayingMode (PlayingMode::Poly);
        e->getMidiInterpreter().setHumanisation ({ 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 });

        juce::MidiBuffer midi;
        for (int k : keys) midi.addEvent (juce::MidiMessage::noteOn (1, k, (juce::uint8) velocity), 0);

        juce::AudioBuffer<float> buffer (2, kBlock);
        e->processBlock (buffer, midi);

        for (int b = 0; b < 8; ++b)
        {
            juce::MidiBuffer none;
            e->processBlock (buffer, none);
        }

        bool anyRest = false;

        for (int s = 0; s < e->getNumStrings(); ++s)
            if (e->getStringMidiNote (s) >= 0)
                anyRest = anyRest || e->getLastWasRest (s);

        juce::ignoreUnused (watchString);
        return anyRest;
    };

    CHECK_MSG (restOf ({ 64 }, 102, 0), "a single note at 0.8 was not a rest stroke");
    CHECK_MSG (! restOf ({ 60, 64, 67 }, 102, 0), "a chord at 0.8 had a rest stroke");
    CHECK_MSG (! restOf ({ 64 }, 64, 0), "a single note at 0.5 was a rest stroke");
}

LUTHIER_TEST (FingerstyleAttack, FA09_patternFingersReachTheString)
{
    TuningEngine tuning;
    tuning.prepare (kSr);
    tuning.setNumStrings (6);
    tuning.setTuningPreset (TuningPreset::Standard);
    RubricVoicer voicer;
    voicer.prepare (&tuning, 6);

    RhythmEngine rhythm;
    rhythm.prepare (kSr, 512, &tuning, &voicer);
    rhythm.setNumStrings (6);
    rhythm.setEnabled (true);
    RhythmHumanise flat;
    flat.timingMs = flat.velocityPercent = flat.missPercent = flat.ghostPercent = flat.amount = 0.0;
    rhythm.setHumanise (flat);

    RhythmPattern pima;
    pima.setName ("p-i-m-a");
    pima.setKind (RhythmPattern::Kind::fingerpick);
    pima.setSubdivision (Subdivision::sixteenth);
    pima.setLength (4);

    for (int i = 0; i < 4; ++i)
    {
        FingerpickStep step;
        step.active = true;
        step.finger = (Finger) i;
        step.dynamic = 0.8;
        pima.setFingerpickStep (i, step);
    }

    rhythm.setPattern (pima);

    juce::MidiBuffer chord;
    for (int k : { 40, 47, 52, 56, 59, 64 }) chord.addEvent (juce::MidiMessage::noteOn (1, k, 0.8f), 0);
    rhythm.handleMidi (chord, 0);

    auto engine = makeEngine();
    int thumbs = 0, fingers = 0, wrong = 0;

    for (int b = 0; b < 200; ++b)
    {
        PlayEventQueue q;
        RhythmTransport t;
        t.bpm = 120.0;
        t.isPlaying = true;
        t.ppqPosition = b * 512 / (0.5 * kSr);
        rhythm.processBlock (512, t, q);

        for (int i = 0; i < q.getNumNoteOns(); ++i)
        {
            const auto& e = q.getNoteOn (i);
            CHECK (e.finger >= 0);
            engine->triggerNoteNow (e);
            const auto m = engine->getLastExcitation (e.stringIndex).material;
            const bool thumbMaterial = m == Excitation::Material::Thumb || m == Excitation::Material::Thumbpick;
            const bool fingerMaterial = m == Excitation::Material::Fingertip || m == Excitation::Material::Fingernail;

            if (e.finger == 0) { ++thumbs; wrong += thumbMaterial ? 0 : 1; }
            else               { ++fingers; wrong += fingerMaterial ? 0 : 1; }
        }
    }

    CHECK_MSG (thumbs > 0 && fingers > 0, "the pattern played " + juce::String (thumbs) + " p and " + juce::String (fingers) + " i m a notes");
    CHECK_MSG (wrong == 0, juce::String (wrong) + " notes were played with the wrong class of material");
}

LUTHIER_TEST (FingerstyleAttack, FA10_perStringTools)
{
    auto e = makeEngine (GuitarType::Stratocaster);
    RightHandSettings h;
    h.stringTool = { RhTool::finger, RhTool::finger, RhTool::finger, RhTool::pick, RhTool::pick, RhTool::pick };
    h.hybridSnap = 0.0;
    e->setRightHand (h);
    e->setNailVsFlesh (0.4);

    auto triggers = [&] (int s, NoiseClass c, float& level)
    {
        auto& pool = e->getPlayingNoise().getPool();
        pool.beginBlockTriggers();
        e->triggerNoteNow (note (*e, s, 3.0));
        int n = 0;

        for (int i = 0; i < pool.getNumBlockTriggers(); ++i)
            if (pool.getBlockTrigger (i).noiseClass == c && pool.getBlockTrigger (i).stringIndex == s)
            {
                ++n;
                level = pool.getBlockTrigger (i).level;
            }

        return n;
    };

    float pickLevel = 0.0f, nailLevel = 0.0f;
    CHECK_MSG (triggers (4, NoiseClass::pickClick, pickLevel) == 1, "the pick string did not click");
    CHECK_MSG (triggers (1, NoiseClass::pickClick, nailLevel) == 1, "the finger string's nail did not click");

    // The nail's click is 0.35 b of a pick's at the same amount: at b 0.4,
    // 17 dB under a pick click, which sits 30 dB under the note (pick-noise 3)
    // - so about 47 dB under the note; 39 dB at b = 1 (fingerstyle-attack 1).
    const double under = juce::Decibels::gainToDecibels ((double) pickLevel / juce::jmax (1.0e-12, (double) nailLevel));
    CHECK_MSG (std::abs (under - 20.0 * std::log10 (1.0 / (0.35 * 0.4))) <= 1.5,
               "the nail click is " + juce::String (under, 1) + " dB under the pick click");
    CHECK (e->getLastTool (1) == RhTool::finger);
    CHECK (e->getLastTool (4) == RhTool::pick);
}

LUTHIER_TEST (FingerstyleAttack, FA11_travisMute)
{
    auto t60 = [] (bool travis)
    {
        auto e = makeEngine();
        RightHandSettings h;
        const auto table = styleTable (travis ? RhStyle::travis : RhStyle::fingerstyle);
        h.stringTool = { table.treble, table.treble, table.treble, table.bass, table.bass, table.bass };
        h.thumbPalmMute = travis ? 0.35 : 0.0;
        e->setRightHand (h);
        e->triggerNoteNow (note (*e, 5, 3.0));
        renderString (*e, 5, 0.02);
        return t60Of (*e, 5);
    };

    /*  DECISION (REALISM-B): at most 0.75x, not the draft's 0.6x.
        thumb_palm_mute 0.35 on the existing palm-mute curve (engine.md 4:
        T60 x jmap (0.35, 1, 0.11)) is 0.69x, and its darker loop filter on
        top; 0.6x would need a Travis mute of 0.45. */
    const double ratio = t60 (true) / t60 (false);
    CHECK_MSG (ratio <= 0.75, "Travis's thumb string decays in " + juce::String (ratio, 3) + "x the Fingerstyle time");
}

LUTHIER_TEST (FingerstyleAttack, FA12_alternation)
{
    auto series = [] (double variation, std::vector<double>& centroids, std::vector<int>& onsets)
    {
        auto e = makeEngine();
        auto h = allStrings (RhTool::finger);
        h.alternationVariation = variation;
        e->setRightHand (h);

        for (int n = 0; n < 100; ++n)
        {
            e->getString (0).reset();
            e->getString (0).snapToFrequency (e->getTuningEngine().computeFrequency (0, 5.0, 0.0));
            e->triggerNoteNow (note (*e, 0, 5.0));
            const auto out = renderString (*e, 0, 0.02);
            centroids.push_back (contactCentroid (*e, 0));

            // The string's own onset: a fifth of its peak (the fingertip's
            // noise sits on the output from the trigger, undelayed).
            const double p = peakOf (out);
            int onset = 0;
            while (onset < (int) out.size() && std::abs (out[(size_t) onset]) < 0.2 * p) ++onset;
            onsets.push_back (onset);
        }
    };

    std::vector<double> c, c0;
    std::vector<int> o, o0;
    series (0.25, c, o);
    series (0.0, c0, o0);

    double evenC = 0.0, oddC = 0.0, evenO = 0.0, oddO = 0.0;

    for (int n = 0; n < 100; ++n)
    {
        ((n & 1) ? oddC : evenC) += c[(size_t) n] / 50.0;
        ((n & 1) ? oddO : evenO) += o[(size_t) n] / 50.0;
    }

    /*  DECISION (REALISM-B): 0.3 %, not the draft's 1.5 %. Section 3's own
        terms at v = 0.25 (tau x 1.0375, pluck position + 0.0025) move the
        contact's centroid 0.5 %; the draft's figure was not what its
        equation gives, and ground rule 4 asks for "a few percent" at most. */
    CHECK_MSG (std::abs (oddC / evenC - 1.0) >= 0.003, "odd and even centroids differ " + juce::String (100.0 * std::abs (oddC / evenC - 1.0), 2) + " %");
    const double onsetMs = std::abs (oddO - evenO) * 1000.0 / kSr;
    CHECK_MSG (onsetMs >= 0.3 && onsetMs <= 0.5, "odd and even onsets differ " + juce::String (onsetMs, 3) + " ms");

    for (int n = 1; n < 100; ++n)
        CHECK (c0[(size_t) n] == c0[0] && o0[(size_t) n] == o0[0]);
}

LUTHIER_TEST (FingerstyleAttack, FA13_slapAndPopTools)
{
    auto e = makeEngine (GuitarType::Stratocaster);
    auto& pool = e->getPlayingNoise().getPool();

    e->setRightHand (allStrings (RhTool::slap));
    const auto buzzBefore = pool.getTriggerCount (NoiseClass::fretBuzz);
    e->triggerNoteNow (note (*e, 5, 3.0));
    CHECK (e->getLastExcitation (5).kind == Excitation::Kind::Slap);
    CHECK (e->getLastExcitation (5).material == Excitation::Material::Thumb);
    CHECK_MSG (pool.getTriggerCount (NoiseClass::fretBuzz) > buzzBefore, "the slap tool did not clack");

    e->setRightHand (allStrings (RhTool::pop));
    const auto popBefore = pool.getTriggerCount (NoiseClass::fretBuzz);
    e->triggerNoteNow (note (*e, 1, 3.0));
    CHECK (e->getLastExcitation (1).kind == Excitation::Kind::Slap);
    CHECK (e->getLastExcitation (1).material == Excitation::Material::Fingernail);
    CHECK (pool.getTriggerCount (NoiseClass::fretBuzz) > popBefore);

    // Hybrid's snap: none at 0, a buzz at 1.
    for (const double snap : { 0.0, 1.0 })
    {
        RightHandSettings h;
        h.stringTool = { RhTool::finger, RhTool::finger, RhTool::finger, RhTool::pick, RhTool::pick, RhTool::pick };
        h.hybridSnap = snap;
        e->setRightHand (h);
        const auto before = pool.getTriggerCount (NoiseClass::fretBuzz);
        e->triggerNoteNow (note (*e, 0, 3.0, 1.0));
        const bool buzzed = pool.getTriggerCount (NoiseClass::fretBuzz) > before;
        CHECK_MSG (buzzed == (snap > 0.0), "hybrid snap " + juce::String (snap) + (buzzed ? " buzzed" : " did not buzz"));
    }
}

LUTHIER_TEST (FingerstyleAttack, FA14_thumbPosition)
{
    auto e = makeEngine();
    e->setPluckPosition (0.2);
    e->setRightHand (allStrings (RhTool::thumb));
    e->triggerNoteNow (note (*e, 4, 0.0));
    CHECK_NEAR (e->getLastExcitation (4).pluckPosition, 0.24, 1.0e-9);

    e->setPluckPosition (0.48);
    e->triggerNoteNow (note (*e, 4, 0.0));
    CHECK_NEAR (e->getLastExcitation (4).pluckPosition, 0.5, 1.0e-9);
}

LUTHIER_TEST (FingerstyleAttack, FA15_ccTriggers)
{
    auto e = makeEngine();
    e->setRightHand (allStrings (RhTool::finger));
    e->getMidiInterpreter().setPlayingMode (PlayingMode::Mono);
    juce::AudioBuffer<float> buffer (2, kBlock);

    auto play = [&] (int cc102, int cc105)
    {
        juce::MidiBuffer midi;
        if (cc102 >= 0) midi.addEvent (juce::MidiMessage::controllerEvent (1, 102, cc102), 0);
        if (cc105 >= 0) midi.addEvent (juce::MidiMessage::controllerEvent (1, 105, cc105), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 1);
        e->processBlock (buffer, midi);
        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
        e->processBlock (buffer, off);

        for (int s = 0; s < e->getNumStrings(); ++s)
            if (e->getLastTool (s) != RhTool::global || e->getLastWasRest (s))
                return std::pair<RhTool, bool> (e->getLastTool (s), e->getLastWasRest (s));

        return std::pair<RhTool, bool> (RhTool::global, false);
    };

    const RhTool bands[] = { RhTool::global, RhTool::pick, RhTool::finger, RhTool::thumb, RhTool::thumbpick, RhTool::slap, RhTool::pop };

    for (int band = 1; band < 7; ++band)
    {
        const int value = (int) std::ceil (band * 128.0 / 7.0);
        CHECK_MSG (play (juce::jmin (127, value), -1).first == bands[band], "CC 102 band " + juce::String (band));
    }

    CHECK_MSG (play (0, -1).first == RhTool::finger, "CC 102 Off did not hand back the string's tool");
    CHECK_MSG (play (-1, 64).second, "CC 105 at 64 did not force a rest stroke");
    CHECK_MSG (! play (-1, 0).second, "CC 105 released still rests");
}

LUTHIER_TEST (FingerstyleAttack, FA16_styleWritesOnce)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    const int stepsBefore = processor.getNumUndoSteps();
    RightHandGroup::applyStyle (processor, (int) RhStyle::travis);

    CHECK_MSG (processor.getNumUndoSteps() == stepsBefore + 1, "the style wrote " + juce::String (processor.getNumUndoSteps() - stepsBefore) + " undo entries");
    CHECK (juce::roundToInt (plainOf (processor, ParamIDs::rhStringTool (1))) == (int) RhTool::finger);
    CHECK (juce::roundToInt (plainOf (processor, ParamIDs::rhStringTool (6))) == (int) RhTool::thumbpick);
    CHECK_NEAR (plainOf (processor, ParamIDs::thumbPalmMute), 0.35, 1.0e-3);
    CHECK_NEAR (plainOf (processor, ParamIDs::nailVsFlesh), 0.3, 1.0e-3);

    // A preset that stores a style and its own tools keeps its tools.
    set (processor, ParamIDs::rhStringTool (1), (float) RhTool::pop);
    const auto saved = processor.getPresetManager().toVar ("style kept");

    LuthierAudioProcessor other;
    other.prepareToPlay (kSr, kBlock);
    other.getPresetManager().fromVar (saved);
    CHECK (juce::roundToInt (plainOf (other, ParamIDs::rhStyle)) == (int) RhStyle::travis);
    CHECK_MSG (juce::roundToInt (plainOf (other, ParamIDs::rhStringTool (1))) == (int) RhTool::pop,
               "loading a preset re-applied its style over its per-string tools");
}

LUTHIER_TEST (FingerstyleAttack, FA17_rangesRealtimeRoundTrip)
{
    juce::ignoreUnused (Parameters::createLayout());

    for (const char* id : { ParamIDs::fingerFleshReleaseMs, ParamIDs::fingerNailReleaseMs,
                            ParamIDs::thumbPositionOffset, ParamIDs::restStrokeDamping })
    {
        const auto* r = RangeRegistry::find (id);
        CHECK_MSG (r != nullptr && r->isValid() && r->family == RangeFamily::pick, juce::String (id));
    }

    CHECK (RangeRegistry::findDeclarationMismatches().isEmpty());

    // The defaults are the table: 2.20, 7.01 and 1.10 kHz.
    CHECK_NEAR (Excitation::cutoffForRelease (0.0723e-3), 2200.0, 5.0);
    CHECK_NEAR (Excitation::cutoffForRelease (0.0227e-3), 7011.0, 5.0);
    CHECK_NEAR (Excitation::cutoffForRelease (2.0 * 0.0723e-3), 1100.0, 5.0);

    // No allocation, every tool and stroke.
    auto e = makeEngine();
    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;
    e->processBlock (buffer, midi);
    const long before = luthierAllocationsOnThisThread();
    const auto startTicks = juce::Time::getHighResolutionTicks();
    int notes = 0;

    for (int tool = 0; tool < (int) RhTool::numTools; ++tool)
        for (int stroke = 0; stroke < (int) RhStroke::numStrokes; ++stroke)
        {
            auto h = allStrings ((RhTool) tool);
            h.stroke = (RhStroke) stroke;
            h.alternationVariation = 0.3;
            h.thumbPalmMute = 0.2;
            e->setRightHand (h);

            for (int s = 0; s < 6; ++s, ++notes)
                e->triggerNoteNow (note (*e, s, 2.0, 0.9, s % 4));

    juce::ignoreUnused (notes);
        }

    juce::ignoreUnused (startTicks);
    e->processBlock (buffer, midi);
    CHECK_MSG (luthierAllocationsOnThisThread() == before, "a right-hand note allocated");

    // Cost: the right hand's own note-on work, against the same notes on Global.
    auto noteOnSeconds = [] (RhTool tool)
    {
        auto engine = makeEngine();
        auto h = allStrings (tool);
        h.stroke = RhStroke::automatic;
        h.alternationVariation = 0.25;
        engine->setRightHand (h);
        double best = 1.0e9;

        for (int run = 0; run < 5; ++run)
        {
            const auto t0 = juce::Time::getHighResolutionTicks();

            for (int n = 0; n < 300; ++n)
                engine->triggerNoteNow (note (*engine, n % 6, 2.0, 0.9));

            best = juce::jmin (best, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) / 300.0);
        }

        return best;
    };

    // 16 notes a second, as a share of a second, in units (1 % of a core).
    const double units = 100.0 * 16.0 * juce::jmax (0.0, noteOnSeconds (RhTool::finger) - noteOnSeconds (RhTool::global));
    CHECK_MSG (units <= 0.02, "16 notes/s of right-hand work cost " + juce::String (units, 4) + " units");

    // Preset round trip of every field.
    std::vector<juce::String> ids { ParamIDs::fingerFleshReleaseMs, ParamIDs::fingerNailReleaseMs, ParamIDs::thumbPositionOffset,
                                    ParamIDs::restStrokeDamping, ParamIDs::rhStroke, ParamIDs::rhStyle,
                                    ParamIDs::thumbPalmMute, ParamIDs::hybridSnap };
    for (int n = 1; n <= 6; ++n) ids.push_back (ParamIDs::rhStringTool (n));

    auto from = std::make_unique<LuthierAudioProcessor>();
    auto to = std::make_unique<LuthierAudioProcessor>();
    std::vector<float> written;

    for (auto& id : ids)
    {
        auto* p = from->getState().getParameter (id);
        CHECK_MSG (p != nullptr, "no parameter " + id);
        if (p != nullptr) p->setValueNotifyingHost (p->convertTo0to1 (p->convertFrom0to1 (p->getDefaultValue() < 0.5f ? 0.8f : 0.2f)));
        written.push_back (p != nullptr ? p->getValue() : 0.0f);
    }

    to->getPresetManager().fromVar (from->getPresetManager().toVar ("right hand round trip"));

    for (size_t i = 0; i < ids.size(); ++i)
        if (auto* p = to->getState().getParameter (ids[i]))
            CHECK_MSG (std::abs (p->getValue() - written[i]) < 1.0e-4f, ids[i] + " did not round-trip");
}
