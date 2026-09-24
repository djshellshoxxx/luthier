/*  bass-techniques.md 12 - the tests SlapTests.cpp does not already make
    (MODEL-GAPS workstream): inert on a guitar, slap against a loud pluck, pop
    against slap, auto-ghosting, the rest stroke, finger alternation, the bass
    defaults on load, headroom, the step grid and BASS_TECH into the capture.

    Measurements are the strings' own (pre-body) sum unless the test is about
    what is heard, and each string's own level follower for "ringing".
*/

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Capture/PerformanceCapture.h"
#include "../Model/Guitar/BassDefaults.h"
#include "../Rhythm/BassStepGrid.h"
#include "../Rhythm/GenreKit.h"
#include "../UI/SlapGroup.h"

#include <set>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    // A four-string bass, engine order: 0 G, 1 D, 2 A, 3 E.
    std::unique_ptr<LuthierEngine> bassEngine (GuitarType type = GuitarType::JazzBass)
    {
        auto e = std::make_unique<LuthierEngine>();
        e->prepare (kSr, kBlock);
        e->setGuitarType (type);
        e->reset();
        return e;
    }

    struct Take
    {
        std::vector<double> preBody, output;
        std::vector<std::vector<double>> stringLevel;   ///< per block
    };

    template <typename MidiFor>
    Take render (LuthierEngine& engine, int blocks, MidiFor midiFor)
    {
        Take t;
        t.stringLevel.assign ((size_t) engine.getNumStrings(), {});
        juce::AudioBuffer<float> block (2, kBlock);

        for (int b = 0; b < blocks; ++b)
        {
            block.clear();
            juce::MidiBuffer midi = midiFor (b);
            engine.processBlock (block, midi);

            const double* sum = engine.getPreBodyBuffer();

            for (int i = 0; i < kBlock; ++i)
            {
                t.preBody.push_back (sum[i]);
                t.output.push_back (block.getSample (0, i));
            }

            for (int s = 0; s < engine.getNumStrings(); ++s)
                t.stringLevel[(size_t) s].push_back (engine.getStringLevel (s));
        }

        return t;
    }

    Take render (LuthierEngine& engine, int blocks)
    {
        return render (engine, blocks, [] (int) { return juce::MidiBuffer(); });
    }

    /** Spectral centroid of `n` samples from `from`, Hann-windowed, in Hz. */
    double centroid (const std::vector<double>& x, size_t from, int n)
    {
        constexpr int order = 12, size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) size * 2, 0.0f);

        for (int i = 0; i < n && i < size && from + (size_t) i < x.size(); ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / juce::jmax (1, n - 1));
            data[(size_t) i] = (float) (x[from + (size_t) i] * w);
        }

        fft.performFrequencyOnlyForwardTransform (data.data());

        double num = 0.0, den = 0.0;

        for (int k = 1; k < size / 2; ++k)
        {
            const double hz = k * kSr / size;
            const double m = data[(size_t) k];
            num += hz * m;
            den += m;
        }

        return den > 0.0 ? num / den : 0.0;
    }

    /** The first sample whose magnitude exceeds `fraction` of the peak. */
    size_t onsetOf (const std::vector<double>& x, double fraction = 0.01)
    {
        double peak = 0.0;

        for (double v : x)
            peak = juce::jmax (peak, std::abs (v));

        for (size_t i = 0; i < x.size(); ++i)
            if (std::abs (x[i]) > peak * fraction)
                return i;

        return 0;
    }

    /** Samples from the onset to the first sample at 90% of the peak of the first `window` samples. */
    double riseSamples (const std::vector<double>& x, int window)
    {
        const size_t start = onsetOf (x, 0.02);
        double peak = 0.0;

        for (int i = 0; i < window && start + (size_t) i < x.size(); ++i)
            peak = juce::jmax (peak, std::abs (x[start + (size_t) i]));

        for (int i = 0; i < window && start + (size_t) i < x.size(); ++i)
            if (std::abs (x[start + (size_t) i]) >= 0.9 * peak)
                return (double) i + 1.0;

        return (double) window;
    }

    /** Samples until the |signal|'s running peak envelope falls 20 dB under its maximum. */
    double decaySamples (const std::vector<double>& x)
    {
        double env = 0.0, peak = 0.0;
        const double release = std::exp (-1.0 / (0.005 * kSr));
        size_t peakAt = 0;
        std::vector<double> e (x.size());

        for (size_t i = 0; i < x.size(); ++i)
        {
            env = juce::jmax (std::abs (x[i]), env * release);
            e[i] = env;

            if (env > peak)
            {
                peak = env;
                peakAt = i;
            }
        }

        for (size_t i = peakAt; i < e.size(); ++i)
            if (e[i] < peak * 0.1)
                return (double) (i - peakAt);

        return (double) e.size();
    }

    NoteOnEvent bassNote (LuthierEngine& engine, int stringIndex, int fret, double velocity, int bassTechnique = -1)
    {
        NoteOnEvent e;
        e.stringIndex = stringIndex;
        e.fretPosition = fret;
        e.velocity = velocity;
        e.technique = Technique::Pluck;
        e.pitchHz = engine.getTuningEngine().computeFrequency (stringIndex, fret);
        e.midiNote = 40;
        e.bassTechnique = bassTechnique;
        return e;
    }

    bool loadType (LuthierAudioProcessor& processor, GuitarType type)
    {
        auto* p = processor.getState().getParameter (ParamIDs::guitarType);
        p->setValueNotifyingHost (p->convertTo0to1 ((float) type));
        processor.getParameterBridge().applyAllNow();
        return processor.getEngine().getGuitarType() == type;
    }

    std::vector<double> normalised (std::vector<double> x)
    {
        double peak = 0.0;

        for (double v : x)
            peak = juce::jmax (peak, std::abs (v));

        if (peak > 0.0)
            for (auto& v : x)
                v /= peak;

        return x;
    }

    std::vector<double> strike (int bassTechnique, double velocity)
    {
        auto engine = bassEngine();
        engine->triggerNoteNow (bassNote (*engine, 2, 5, velocity, bassTechnique));
        return normalised (render (*engine, 40).preBody);
    }
}

//==============================================================================
/*  12: "Inert on a guitar. With a non-bass guitar loaded, assert no bass
    technique produces any audible difference and the SLAP group is hidden." */
LUTHIER_TEST (BassTechniques, inertOnAGuitar)
{
    auto play = [] (bool extremes)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.setUseFingers (true);

        if (extremes)
        {
            auto slap = engine.getSlapEngine().getSettings();
            slap.slapStrength = 1.0;
            slap.fretContact = 0.0;
            slap.popStrength = 0.1;
            slap.ghostAuto = true;
            slap.ghostVelocityThreshold = 127;
            slap.doubleThump = true;
            engine.setSlapSettings (slap);

            BassFingerstyleSettings fingers;
            fingers.alternationVariation = 1.0;
            fingers.restStroke = true;
            engine.setBassFingerstyle (fingers);
        }
        else
        {
            BassFingerstyleSettings fingers;
            fingers.alternationVariation = 0.0;
            fingers.restStroke = false;
            engine.setBassFingerstyle (fingers);
        }

        engine.reset();

        // Two quiet notes and a hard one on neighbouring strings, and a step-grid thumb.
        return render (engine, 60, [&engine] (int b)
        {
            juce::MidiBuffer m;

            if (b == 0)  m.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 20), 0);
            if (b == 10) m.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 20), 3);
            if (b == 20) m.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 127), 7);

            if (b == 30)
                engine.triggerNoteNow ([&engine]
                {
                    NoteOnEvent e;
                    e.stringIndex = 4;
                    e.velocity = 0.9;
                    e.pitchHz = engine.getTuningEngine().computeFrequency (4, 3);
                    e.fretPosition = 3;
                    e.bassTechnique = 1;
                    return e;
                }());

            return m;
        }).output;
    };

    const auto plain = play (false);
    const auto extreme = play (true);

    CHECK (plain.size() == extreme.size());

    double maxDiff = 0.0, peak = 0.0;

    for (size_t i = 0; i < plain.size(); ++i)
    {
        maxDiff = juce::jmax (maxDiff, std::abs (plain[i] - extreme[i]));
        peak = juce::jmax (peak, std::abs (plain[i]));
    }

    CHECK_MSG (peak > 1.0e-3, "the guitar made no sound");
    CHECK_MSG (maxDiff == 0.0, "a bass technique changed a guitar's output by " + juce::String (maxDiff));
}

/*  12: "Slap is not a loud pluck. Compare a slap and a velocity-127 pluck at
    equal peak level; assert the slap's spectral centroid in the first 20 ms is
    at least 1.5x higher and its attack rise time is at least 3x faster." */
LUTHIER_TEST (BassTechniques, aSlapIsNotALoudPluck)
{
    const auto slap = strike (1, 1.0);
    const auto pluck = strike (-1, 1.0);

    const int window = (int) (0.020 * kSr);
    const double slapCentroid = centroid (slap, onsetOf (slap), window);
    const double pluckCentroid = centroid (pluck, onsetOf (pluck), window);

    CHECK_MSG (slapCentroid >= 1.5 * pluckCentroid,
               "centroid: slap " + juce::String (slapCentroid, 0) + " Hz, pluck " + juce::String (pluckCentroid, 0) + " Hz");

    /*  The attack's rise is the excitation's - the force the string receives,
        which is what 2.1.1's "0.3 ms versus 2 ms" describes. Measured on the
        string's output the rise is the first quarter-period of the note, the
        same for both, so it is measured where the spec states it. */
    auto excitationRise = [] (int bassTechnique)
    {
        auto engine = bassEngine();
        const auto e = bassNote (*engine, 2, 5, 1.0, bassTechnique);
        const auto st = engine->getSlapEngine().classify (e, false);

        Excitation::Params p;
        p.material = Excitation::Material::Fingertip;
        p.velocity = 1.0;
        p.delaySamples = kSr / e.pitchHz;

        if (st.strike)
        {
            engine->getSlapEngine().shapeExcitation (st, 5.0, p);
            p.velocity = st.velocity;
        }

        Excitation ex;
        ex.prepare (kSr);
        RtRandom rng { 7 };
        ex.trigger (p, rng);

        std::vector<double> x;

        while (ex.isActive())
            x.push_back (ex.next());

        // The rise of the first lobe of the force pulse: from the onset to 90%
        // of that lobe's peak. (The comb's reflected image comes after, the
        // other way up, and is not the attack.)
        const size_t start = onsetOf (x, 0.02);
        const double sign = x[start] >= 0.0 ? 1.0 : -1.0;
        double lobePeak = 0.0;
        size_t end = start;

        while (end < x.size() && x[end] * sign > -1.0e-12)
            lobePeak = juce::jmax (lobePeak, std::abs (x[end++]));

        for (size_t i = start; i < end; ++i)
            if (std::abs (x[i]) >= 0.9 * lobePeak)
                return (double) (i - start) + 1.0;

        return (double) (end - start);
    };

    const double slapRise = excitationRise (1);
    const double pluckRise = excitationRise (-1);

    CHECK_MSG (slapRise * 3.0 <= pluckRise,
               "rise: slap " + juce::String (slapRise) + " samples, pluck " + juce::String (pluckRise));
}

/*  12: "Pop is brighter and shorter than slap by at least 20% on both measures." */
LUTHIER_TEST (BassTechniques, aPopIsBrighterAndShorterThanASlap)
{
    const auto slap = strike (1, 0.9);
    const auto pop = strike (2, 0.9);

    const int window = (int) (0.020 * kSr);
    const double slapCentroid = centroid (slap, onsetOf (slap), window);
    const double popCentroid = centroid (pop, onsetOf (pop), window);

    CHECK_MSG (popCentroid >= 1.2 * slapCentroid,
               "centroid: pop " + juce::String (popCentroid, 0) + " Hz, slap " + juce::String (slapCentroid, 0) + " Hz");

    const double slapDecay = decaySamples (slap);
    const double popDecay = decaySamples (pop);

    CHECK_MSG (popDecay <= 0.8 * slapDecay,
               "decay to -20 dB: pop " + juce::String (popDecay / kSr * 1000.0, 1) + " ms, slap "
                 + juce::String (slapDecay / kSr * 1000.0, 1) + " ms");
}

/*  12: "Auto-ghosting fires below threshold and not above it." */
LUTHIER_TEST (BassTechniques, autoGhostingFiresBelowTheThresholdOnly)
{
    SlapEngine slap;
    slap.prepare (kSr);
    slap.setInstrument (4, 864.0, 20, true);

    auto s = slap.getSettings();
    s.ghostAuto = true;
    s.ghostVelocityThreshold = 32;
    slap.setSettings (s);

    NoteOnEvent e;
    e.stringIndex = 2;

    for (int v = 1; v <= 127; ++v)
    {
        e.velocity = v / 127.0;
        const bool ghost = slap.classify (e, false).ghost;
        CHECK_MSG (ghost == (v < 32), "velocity " + juce::String (v) + (ghost ? " ghosted" : " not ghosted"));
    }

    // Off: nothing is ghosted.
    s.ghostAuto = false;
    slap.setSettings (s);
    e.velocity = 5 / 127.0;
    CHECK (! slap.classify (e, false).ghost);

    // Not a bass: nothing is ghosted.
    s.ghostAuto = true;
    slap.setSettings (s);
    slap.setInstrument (6, 648.0, 22, false);
    CHECK (! slap.classify (e, false).ghost);
}

/*  12: "Rest stroke damps the neighbour. With rest_stroke on, plucking string 2
    reduces string 3's ringing amplitude by at least 12 dB within 10 ms."
    Strings counted from the high one as the spec does: engine 1 (D) and 2 (A). */
LUTHIER_TEST (BassTechniques, theRestStrokeDampsTheNextLowerString)
{
    auto dropDb = [] (bool restStroke)
    {
        auto engine = bassEngine();
        engine->setUseFingers (true);
        BassFingerstyleSettings fingers;
        fingers.restStroke = restStroke;
        engine->setBassFingerstyle (fingers);

        // The A string's own output, from the per-string taps (routing-io 3).
        engine->getTapBuffers().setPerStringWanted (true);
        std::vector<double> a;

        auto run = [&engine, &a] (int blocks)
        {
            juce::AudioBuffer<float> block (2, kBlock);

            for (int b = 0; b < blocks; ++b)
            {
                block.clear();
                juce::MidiBuffer none;
                engine->processBlock (block, none);
                const float* s = engine->getTapBuffers().stringRead (2);

                for (int i = 0; i < kBlock; ++i)
                    a.push_back (s != nullptr ? s[i] : 0.0);
            }
        };

        engine->triggerNoteNow (bassNote (*engine, 2, 0, 0.8));
        run (40);                                               // the A rings for ~210 ms
        const size_t pluckAt = a.size();

        engine->triggerNoteNow (bassNote (*engine, 1, 0, 0.8)); // the D, fingerstyle
        run (12);

        // Peak over one period of the A (18 ms) before, and from 10 ms after.
        auto peakIn = [&a] (size_t from, size_t count)
        {
            double p = 0.0;

            for (size_t i = from; i < from + count && i < a.size(); ++i)
                p = juce::jmax (p, std::abs (a[i]));

            return p;
        };

        const size_t period = (size_t) (0.020 * kSr);
        const double before = peakIn (pluckAt - period, period);
        const double after = peakIn (pluckAt + (size_t) (0.010 * kSr), period);

        return juce::Decibels::gainToDecibels (juce::jmax (1.0e-9, after))
             - juce::Decibels::gainToDecibels (juce::jmax (1.0e-9, before));
    };

    const double withRest = dropDb (true);
    const double without = dropDb (false);

    CHECK_MSG (withRest <= -12.0, "rest stroke: the A fell " + juce::String (-withRest, 1) + " dB in 10 ms");
    CHECK_MSG (without > -6.0, "no rest stroke: the A fell " + juce::String (-without, 1) + " dB");
}

/*  12: "Finger alternation varies. Over 100 consecutive fingerstyle notes, odd
    and even notes differ measurably in timing and centroid; with variation 0
    they do not." Timing from the engine's own string-activity stamps, tone
    from each note's first 30 ms. */
LUTHIER_TEST (BassTechniques, fingerAlternationVaries)
{
    struct Stats
    {
        double oddLate = 0.0, evenLate = 0.0, oddCentroid = 0.0, evenCentroid = 0.0;
        double centroidSe = 0.0;   ///< standard error of the odd-even difference in centroid
    };

    auto measure = [&ctx] (double variation)
    {
        auto engine = bassEngine();
        engine->setUseFingers (true);
        BassFingerstyleSettings fingers;
        fingers.alternationVariation = variation;
        fingers.restStroke = false;
        engine->setBassFingerstyle (fingers);

        // The player's own humanisation would blur what is being measured.
        auto h = engine->getMidiInterpreter().getHumanisation();
        h.amount = 0.0;
        engine->getMidiInterpreter().setHumanisation (h);

        constexpr int notes = 100, blocksPerNote = 24;  // 128 ms a note: each one gone before the next
        Stats st;
        std::vector<double> all;
        std::vector<juce::int64> onsets;
        std::set<int> strings;
        juce::AudioBuffer<float> block (2, kBlock);

        for (int b = 0; b < notes * blocksPerNote; ++b)
        {
            juce::MidiBuffer m;

            if (b % blocksPerNote == 0)
                m.addEvent (juce::MidiMessage::noteOn (1, 33, (juce::uint8) 96), 16);   // A1, the A string open

            if (b % blocksPerNote == blocksPerNote - 1)
                m.addEvent (juce::MidiMessage::noteOff (1, 33), 200);

            block.clear();
            engine->processBlock (block, m);

            const auto& activity = engine->getStringActivity();

            for (int i = 0; i < activity.size(); ++i)
                if (activity[i].isNoteOn)
                {
                    onsets.push_back ((juce::int64) b * kBlock + activity[i].sampleOffset);
                    strings.insert (activity[i].stringIndex);
                }

            const double* sum = engine->getPreBodyBuffer();
            all.insert (all.end(), sum, sum + kBlock);
        }

        CHECK_MSG ((int) onsets.size() == notes, juce::String ((int) onsets.size()) + " notes started, expected 100");
        CHECK_MSG (strings.size() == 1, juce::String ((int) strings.size()) + " strings used");
        std::vector<double> centroids;

        for (int n = 0; n < (int) onsets.size(); ++n)
        {
            const double late = (double) (onsets[(size_t) n] - ((juce::int64) n * blocksPerNote * kBlock + 16));
            const double c = centroid (all, (size_t) onsets[(size_t) n], (int) (0.030 * kSr));

            (n % 2 == 0 ? st.evenLate : st.oddLate) += late / (notes / 2);
            (n % 2 == 0 ? st.evenCentroid : st.oddCentroid) += c / (notes / 2);
            centroids.push_back (c);
        }

        // The excitation jitters every attack by itself (engine.md 5.5), so each
        // note's centroid scatters; "measurably" is beyond that scatter.
        double varOdd = 0.0, varEven = 0.0;

        for (size_t n = 0; n < centroids.size(); ++n)
        {
            const double d = centroids[n] - (n % 2 == 0 ? st.evenCentroid : st.oddCentroid);
            (n % 2 == 0 ? varEven : varOdd) += d * d / (notes / 2 - 1);
        }

        st.centroidSe = std::sqrt (varOdd / (notes / 2) + varEven / (notes / 2));
        return st;
    };

    const auto varied = measure (0.25);
    CHECK_MSG (std::abs (varied.oddLate - varied.evenLate) >= 0.001 * kSr,
               "timing: odd " + juce::String (varied.oddLate, 1) + ", even " + juce::String (varied.evenLate, 1) + " samples late");
    CHECK_MSG (std::abs (varied.oddCentroid - varied.evenCentroid) >= 3.0 * varied.centroidSe,
               "centroid: odd " + juce::String (varied.oddCentroid, 1) + " Hz, even " + juce::String (varied.evenCentroid, 1)
                 + " Hz, standard error " + juce::String (varied.centroidSe, 1));

    const auto same = measure (0.0);
    CHECK_MSG (std::abs (same.oddLate - same.evenLate) < 0.5,
               "variation 0: timing differs by " + juce::String (same.oddLate - same.evenLate, 2) + " samples");
    CHECK_MSG (std::abs (same.oddCentroid - same.evenCentroid) < 3.0 * same.centroidSe,
               "variation 0: odd " + juce::String (same.oddCentroid, 1) + " Hz, even " + juce::String (same.evenCentroid, 1)
                 + " Hz, standard error " + juce::String (same.centroidSe, 1));
}

/*  7: "Palm muting is the bass pick technique ... shorter decay, more
    fundamental retained." The bass profile dies sooner and is darker than the
    guitar profile on the same string. */
LUTHIER_TEST (BassTechniques, aBassPalmMuteIsShorterAndDarker)
{
    auto run = [] (bool bassProfile)
    {
        StringEngine s;
        s.prepare (kSr, kBlock);
        StringEngine::Physical p;
        p.scaleLengthMm = 864.0;
        p.diameterMm = 2.54;
        p.linearDensity = 0.0189;
        p.wound = true;
        s.setPhysical (p);
        s.reset();
        s.snapToFrequency (55.0);
        s.setDamping (bassProfile ? StringEngine::Damping::PalmMuteBass : StringEngine::Damping::PalmMute, 0.8);

        Excitation::Params e;
        e.velocity = 0.8;
        s.excite (e);

        std::vector<double> out ((size_t) kSr);

        for (auto& v : out)
            v = s.processSample (0.0);

        return out;
    };

    const auto guitar = run (false);
    const auto bass = run (true);

    CHECK_MSG (decaySamples (bass) < decaySamples (guitar),
               "bass palm mute decays in " + juce::String (decaySamples (bass) / kSr * 1000.0, 1) + " ms, guitar "
                 + juce::String (decaySamples (guitar) / kSr * 1000.0, 1) + " ms");
    // "More fundamental retained": the share of the note's energy at the
    // fundamental, once the palm has had 40 ms to act.
    auto fundamentalShare = [] (const std::vector<double>& x)
    {
        constexpr int order = 13, size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) size * 2, 0.0f);
        const size_t from = (size_t) (0.040 * kSr);

        for (int i = 0; i < size && from + (size_t) i < x.size(); ++i)
            data[(size_t) i] = (float) (x[from + (size_t) i] * (0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / (size - 1))));

        fft.performFrequencyOnlyForwardTransform (data.data());
        double low = 0.0, all = 0.0;

        for (int k = 1; k < size / 2; ++k)
        {
            const double e = (double) data[(size_t) k] * data[(size_t) k];
            all += e;

            if (k * kSr / size < 80.0)
                low += e;
        }

        return all > 0.0 ? low / all : 0.0;
    };

    CHECK_MSG (fundamentalShare (bass) > fundamentalShare (guitar),
               "fundamental share: bass " + juce::String (fundamentalShare (bass), 3) + ", guitar "
                 + juce::String (fundamentalShare (guitar), 3));
}

/*  12: "Bass defaults apply on load. Loading each factory bass produces the
    section 8 values." From a guitar, through the processor's guitar load. */
LUTHIER_TEST (BassTechniques, bassDefaultsApplyOnLoad)
{
    auto plain = [] (LuthierAudioProcessor& p, const juce::String& id)
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
        return param != nullptr ? (double) param->convertFrom0to1 (param->getValue()) : -1.0;
    };

    const auto bass = BassFamilyDefaults::forFamily (true);

    for (auto type : { GuitarType::PrecisionBass, GuitarType::JazzBass, GuitarType::Rickenbacker,
                       GuitarType::FiveStringBass, GuitarType::FretlessBass })
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, kBlock);
        CHECK (loadType (processor, GuitarType::Stratocaster));
        CHECK (loadType (processor, type));

        const juce::String name (GuitarLibrary::get (type).name);

        CHECK_MSG (std::abs (plain (processor, ParamIDs::strumCrossingSps) - 100.0) < 0.5, name + ": strum crossing");
        CHECK_MSG (std::abs (plain (processor, ParamIDs::strumMissProbability) - 0.01) < 1.0e-3, name + ": strum misses");
        CHECK_MSG (std::abs (plain (processor, ParamIDs::squeakPressure) - 0.35) < 1.0e-3, name + ": squeak pressure");
        CHECK_MSG (juce::roundToInt (plain (processor, ParamIDs::setupStyle)) == 0, name + ": setup style");
        CHECK_MSG (std::abs (Parameters::pickThicknessMm (plain (processor, ParamIDs::pickThickness)) - 1.14) < 0.01,
                   name + ": pick " + juce::String (Parameters::pickThicknessMm (plain (processor, ParamIDs::pickThickness)), 3) + " mm");
        CHECK_MSG (juce::roundToInt (plain (processor, ParamIDs::slotType (false, 0))) == (int) PedalType::Compressor,
                   name + ": compressor");
        CHECK_MSG (std::abs (plain (processor, ParamIDs::pluckPosition) - bass.pluckPosition) < 1.0e-3, name + ": pluck position");
        CHECK_MSG (std::abs (processor.getEngine().getGuitarSpec().scaleLengthMm - 864.0) < 1.0, name + ": scale "
                     + juce::String (processor.getEngine().getGuitarSpec().scaleLengthMm, 1) + " mm");

        // And back: a guitar gets the guitar's defaults again, the compressor out.
        CHECK (loadType (processor, GuitarType::Stratocaster));
        CHECK (std::abs (plain (processor, ParamIDs::squeakPressure) - 0.5) < 1.0e-3);
        CHECK (juce::roundToInt (plain (processor, ParamIDs::setupStyle)) == kDefaultSetupStyle);
        CHECK (juce::roundToInt (plain (processor, ParamIDs::slotType (false, 0))) == (int) PedalType::None);
    }
}

/*  8: "These are defaults, not constraints." What the user set stays. */
LUTHIER_TEST (BassTechniques, aUserSettingSurvivesTheFamilyChange)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    CHECK (loadType (processor, GuitarType::Stratocaster));

    auto* squeak = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::squeakPressure));
    squeak->setValueNotifyingHost (squeak->convertTo0to1 (0.8f));

    CHECK (loadType (processor, GuitarType::JazzBass));
    CHECK_NEAR (squeak->convertFrom0to1 (squeak->getValue()), 0.8, 1.0e-3);
}

/*  12: "Headroom. A full-strength slap on every string simultaneously peaks
    below 0 dBFS with the limiter bypassed." */
LUTHIER_TEST (BassTechniques, aFullSlapOnEveryStringHasHeadroom)
{
    for (auto type : { GuitarType::PrecisionBass, GuitarType::JazzBass, GuitarType::FiveStringBass })
    {
        auto engine = bassEngine (type);
        engine->getMasterBus().setLimiterEnabled (false);

        auto s = engine->getSlapEngine().getSettings();
        s.slapStrength = 1.0;
        s.fretContact = 1.0;
        s.thumbHardness = 1.0;
        engine->setSlapSettings (s);

        for (int str = 0; str < engine->getNumStrings(); ++str)
            engine->triggerNoteNow (bassNote (*engine, str, 0, 1.0, 1));

        const auto take = render (*engine, 80);
        double peak = 0.0;

        for (double v : take.output)
            peak = juce::jmax (peak, std::abs (v));

        CHECK_MSG (peak < 1.0, juce::String (GuitarLibrary::get (type).name) + ": peak "
                                 + juce::String (juce::Decibels::gainToDecibels (peak), 2) + " dBFS");
    }
}

//==============================================================================
/*  9: the bass step grid. A grid on a bass plays its techniques on the root. */
LUTHIER_TEST (BassTechniques, theStepGridPlaysItsTechniquesOnABass)
{
    RubricVoicer voicer;
    TuningEngine tuning;
    tuning.prepare (kSr);
    tuning.setNumStrings (4);
    tuning.setTuningPreset (TuningPreset::BassStandard);
    voicer.prepare (&tuning, 4);

    RhythmEngine rhythm;
    rhythm.prepare (kSr, kBlock, &tuning, &voicer);
    rhythm.setNumStrings (4);
    rhythm.setEnabled (true);
    rhythm.setVoicingStyle (VoicingStyle::bass);

    BassStepGrid grid;
    grid.setSubdivision (Subdivision::sixteenth);
    grid.setLength (4);
    grid.setStep (0, { BassStepType::thumb, 0.9, BassStepNote::root });
    grid.setStep (1, { BassStepType::ghost, 0.5, BassStepNote::root });
    grid.setStep (2, { BassStepType::pop, 0.8, BassStepNote::octave });
    grid.setStep (3, { BassStepType::dead, 0.6, BassStepNote::root });
    rhythm.setBassGrid (grid);

    auto collect = [&rhythm] (bool bassFamily)
    {
        rhythm.setBassFamily (bassFamily);
        rhythm.reset();

        // A held A (A1): the grid plays under it.
        juce::MidiBuffer held;
        held.addEvent (juce::MidiMessage::noteOn (1, 33, (juce::uint8) 100), 0);
        rhythm.handleMidi (held, 0);

        std::vector<NoteOnEvent> ons;
        PlayEventQueue out;
        RhythmTransport t;
        t.bpm = 120.0;
        t.isPlaying = true;

        // 120 bpm: a beat is 24000 samples; four sixteenths.
        for (int b = 0; b < 24000 / kBlock; ++b)
        {
            out.clear();
            t.ppqPosition = (double) b * kBlock / 24000.0;
            rhythm.processBlock (kBlock, t, out);

            for (int i = 0; i < out.getNumNoteOns(); ++i)
                ons.push_back (out.getNoteOn (i));
        }

        return ons;
    };

    const auto ons = collect (true);
    CHECK_MSG (ons.size() == 4, juce::String ((int) ons.size()) + " grid notes, expected 4");

    if (ons.size() == 4)
    {
        CHECK (ons[0].bassTechnique == (int) BassStepType::thumb);
        CHECK (ons[1].bassTechnique == (int) BassStepType::ghost);
        CHECK (ons[2].bassTechnique == (int) BassStepType::pop);
        CHECK (ons[3].bassTechnique == (int) BassStepType::dead);
        CHECK (ons[3].technique == Technique::MutedPick);
        CHECK (ons[0].midiNote % 12 == 9);                        // A
        CHECK (ons[2].midiNote == ons[0].midiNote + 12);          // the octave
        CHECK_NEAR (ons[0].velocity, 0.9, 1.0e-9);
    }

    // Not a bass: the strum pattern (empty here) is in charge and nothing plays from the grid.
    for (const auto& e : collect (false))
        CHECK (e.bassTechnique < 0);
}

LUTHIER_TEST (BassTechniques, theStepGridRoundTripsAndEmptyLeavesThePattern)
{
    for (int f = 0; f < (int) BassStepGrid::Factory::numFactory; ++f)
    {
        const auto grid = BassStepGrid::factory ((BassStepGrid::Factory) f);
        CHECK (! grid.isEmpty());
        CHECK (BassStepGrid::fromVar (juce::JSON::parse (juce::JSON::toString (grid.toVar()))) == grid);
    }

    // Through the rhythm engine's saved state.
    RhythmEngine a, b;
    a.setBassGrid (BassStepGrid::factory (BassStepGrid::Factory::slapFunk));
    a.setBassPattern (RubricBassPattern::rootFifth);
    b.fromVar (juce::JSON::parse (juce::JSON::toString (a.toVar())));
    CHECK (b.getBassGrid() == a.getBassGrid());
    CHECK (b.getBassPattern() == RubricBassPattern::rootFifth);

    // An empty grid does not take over.
    RhythmEngine c;
    c.setBassFamily (true);
    CHECK (! c.isBassGridActive());
    c.setBassGrid (BassStepGrid::factory (BassStepGrid::Factory::fingerstyleGroove));
    CHECK (c.isBassGridActive());
}

/*  The grid's technique reaches the slap: a thumb step is a strike with the
    buzz generator's clack, a ghost is damped, on a bass only. */
LUTHIER_TEST (BassTechniques, aGridTechniqueIsAStrikeOnABassOnly)
{
    SlapEngine slap;
    slap.prepare (kSr);
    slap.setInstrument (4, 864.0, 20, true);

    NoteOnEvent e;
    e.stringIndex = 3;
    e.velocity = 0.8;

    e.bassTechnique = (int) BassStepType::thumb;
    auto st = slap.classify (e, false);
    CHECK (st.strike && st.type == SlapType::thumb);

    e.bassTechnique = (int) BassStepType::pop;
    st = slap.classify (e, false);
    CHECK (st.strike && st.type == SlapType::pop);

    e.bassTechnique = (int) BassStepType::ghost;
    st = slap.classify (e, false);
    CHECK (st.ghost && ! st.strike);

    e.bassTechnique = (int) BassStepType::finger;
    st = slap.classify (e, false);
    CHECK (! st.ghost && ! st.strike);

    slap.setInstrument (6, 648.0, 22, false);
    e.bassTechnique = (int) BassStepType::thumb;
    CHECK (! slap.classify (e, false).strike);
}

/*  midi-export 9 / notation-export 6.1: a slap strike reaches the capture as a
    BASS_TECH event, alongside the note with its string and fret. */
LUTHIER_TEST (BassTechniques, aStrikeIsCapturedAsBassTech)
{
    auto engine = bassEngine();
    PerformanceCapture capture;
    capture.prepare (kSr);
    engine->setPerformanceCapture (&capture);

    CaptureClock clock;
    clock.sampleRate = kSr;
    capture.beginBlock (clock);

    engine->triggerNoteNow (bassNote (*engine, 3, 5, 0.9, (int) BassStepType::thumb));
    engine->triggerNoteNow (bassNote (*engine, 1, 7, 0.9, (int) BassStepType::pop));
    engine->triggerNoteNow (bassNote (*engine, 2, 3, 0.4, (int) BassStepType::ghost));
    render (*engine, 2);
    engine->setPerformanceCapture (nullptr);
    capture.drain();

    juce::StringArray kinds;

    for (const auto& ev : capture.getEvents())
        kinds.add (ev.event.get ("tech"));

    CHECK_MSG (kinds.contains ("slap") && kinds.contains ("pop") && kinds.contains ("ghost"),
               "captured BASS_TECH: " + kinds.joinIntoString (","));

    CHECK (capture.getNotes().size() == 3);

    if (capture.getNotes().size() == 3)
    {
        CHECK (capture.getNotes()[0].stringIndex == 3);
        CHECK_NEAR (capture.getNotes()[0].fret, 5.0, 1.0e-9);
    }
}

//==============================================================================
/*  9 / gui-integration 0.7: the SLAP group is on a bass and nowhere else, and
    carries every bass amount. */
LUTHIER_TEST (BassTechniques, theSlapGroupIsShownOnlyOnABass)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    CHECK (loadType (processor, GuitarType::Stratocaster));

    SlapGroup group (processor);
    group.refresh();
    CHECK (! group.isVisible());
    CHECK (group.preferredHeight() == 0);

    CHECK (loadType (processor, GuitarType::JazzBass));
    group.refresh();
    CHECK (group.isVisible());
    CHECK (group.preferredHeight() > 0);

    const auto ids = group.getAttachedParameterIds();

    for (const char* id : { ParamIDs::slapStrength, ParamIDs::slapPositionMm, ParamIDs::slapThumbHardness,
                            ParamIDs::slapFretContact, ParamIDs::popStrength, ParamIDs::popPositionMm,
                            ParamIDs::doubleThumpEnabled, ParamIDs::doubleThumpUpRatio, ParamIDs::ghostLevel,
                            ParamIDs::ghostDamping, ParamIDs::ghostAuto, ParamIDs::ghostVelocityThreshold,
                            ParamIDs::fingerAlternationVariation, ParamIDs::restStroke })
        CHECK_MSG (ids.contains (id), juce::String (id) + " is not in the SLAP group");

    CHECK (juce::String (SlapGroup::kInactiveMessage) == "Bass techniques are inactive. Load a bass to use them.");
}

/*  bass-techniques 9: "factory-content.md's genre kits gain bass kits that use it." */
LUTHIER_TEST (BassTechniques, theBassKitsInstallTheirGrids)
{
    GenreKitLibrary kits;
    PatternLibrary patterns;
    RhythmEngine engine;
    int bassKits = 0;

    for (int i = 0; i < kits.getNumKits(); ++i)
    {
        const auto& kit = kits.getKit (i);

        if (kit.bassGrid.isEmpty())
            continue;

        ++bassKits;
        CHECK (kit.voicingStyle == VoicingStyle::bass);
        GenreKitLibrary::apply (kit, engine, patterns);
        CHECK_MSG (! engine.getBassGrid().isEmpty(), kit.name + " installed no grid");
        CHECK (engine.getBassGrid().getName() == kit.bassGrid);

        // The grid survives the kit's file round trip.
        CHECK (GenreKit::fromVar (kit.toVar()).bassGrid == kit.bassGrid);
    }

    CHECK_MSG (bassKits >= 4, juce::String (bassKits) + " bass kits");

    // A guitar kit clears the grid, so the strum pattern plays again.
    GenreKitLibrary::apply (kits.getKit (0), engine, patterns);
    CHECK (engine.getBassGrid().isEmpty());
}
