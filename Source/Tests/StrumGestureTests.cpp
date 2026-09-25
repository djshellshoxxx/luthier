/*  Strum gesture tests (strum-dynamics.md section 8, ambiguity-resolutions.md
    6.1).

    Three layers, in this order:

      - the gesture on its own (StrumGesture::plan and its rules), which is
        where timing, force and misses are decided;
      - the two schedulers that use it: RhythmEngine for pattern strums and
        MidiInterpreter for live chords, so the crossing velocity's sources
        (strum-dynamics 1.1) are proven where they are resolved;
      - the plugin: the engine's striker noise and chuck, the parameters, the
        family defaults, the legacy preset migration and the STRUM group.

    Every rhythm fixture here has humanisation off and misses at zero unless
    the test is about them, so a spacing is exactly a spacing.
*/

#include "TestFramework.h"

#include "../Rhythm/StrumGesture.h"
#include "../Rhythm/RhythmEngine.h"
#include "../Rhythm/Patterns.h"
#include "../Model/Playing/MidiInterpreter.h"
#include "../Model/Playing/TechniqueEngine.h"
#include "../Model/Playing/RubricVoicer.h"
#include "../Model/Playing/TuningEngine.h"
#include "../DSP/Noise/PlayingNoise.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../UI/StrumGroup.h"
#include "../UI/RhythmPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/NoiseGroups.h"

#include <algorithm>
#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    /** A six-string down-strum crosses the low E (index 5) first. */
    constexpr int kDownSix[] = { 5, 4, 3, 2, 1, 0 };
    constexpr int kUpSix[]   = { 0, 1, 2, 3, 4, 5 };

    StrumSettings flatSettings()
    {
        auto s = StrumSettings::guitarDefaults();
        s.acceleration = 0.0;
        s.missProbability = 0.0;
        return s;
    }

    StrumRequest requestFor (const int* strings, int n, bool down, double sps, juce::uint32 index = 0)
    {
        StrumRequest r;
        r.strings = strings;
        r.numStrings = n;
        r.down = down;
        r.sourceSps = sps;
        r.strumIndex = index;
        return r;
    }

    //==========================================================================
    struct Hit
    {
        int64_t sample = 0;
        int stringIndex = 0;
        double velocity = 0.0;
        double chuck = 0.0;
        int striker = -1;
    };

    /** A rhythm engine holding a chord, with humanisation off, misses off and
        acceleration 0, so the grid and the crossing are exactly as set. */
    struct StrumFixture
    {
        StrumFixture()
        {
            tuning.prepare (kSr);
            tuning.setNumStrings (6);
            tuning.setTuningPreset (TuningPreset::Standard);

            voicer.prepare (&tuning, 6);

            engine.prepare (kSr, kBlock, &tuning, &voicer);
            engine.setNumStrings (6);
            engine.setEnabled (true);

            RhythmHumanise flat;
            flat.timingMs = 0.0;
            flat.velocityPercent = 0.0;
            flat.missPercent = 0.0;
            flat.ghostPercent = 0.0;
            flat.amount = 0.0;
            engine.setHumanise (flat);

            engine.setStrumSettings (flatSettings());
        }

        void hold (std::initializer_list<int> notes)
        {
            juce::MidiBuffer midi;

            for (int note : notes)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            engine.handleMidi (midi, 0);
        }

        void holdOpenE() { hold ({ 40, 47, 52, 56, 59, 64 }); }

        /** Plays from ppq 0 and returns every note-on, in time order. */
        std::vector<Hit> play (double seconds, double bpm = 120.0)
        {
            std::vector<Hit> hits;

            const double perBeat = 60.0 / bpm * kSr;
            const int blocks = (int) std::ceil (seconds * kSr / (double) kBlock);

            for (int b = 0; b < blocks; ++b)
            {
                auto out = std::make_unique<PlayEventQueue>();
                out->clear();

                RhythmTransport transport;
                transport.bpm = bpm;
                transport.isPlaying = true;
                transport.ppqPosition = (double) b * (double) kBlock / perBeat;

                engine.processBlock (kBlock, transport, *out);

                for (int i = 0; i < out->getNumNoteOns(); ++i)
                {
                    const auto& e = out->getNoteOn (i);
                    hits.push_back ({ (int64_t) b * kBlock + e.sampleOffset, e.stringIndex,
                                      e.velocity, e.chuck, e.strikerMaterial });
                }
            }

            std::stable_sort (hits.begin(), hits.end(),
                              [] (const Hit& a, const Hit& b) { return a.sample < b.sample; });
            return hits;
        }

        TuningEngine tuning;
        RubricVoicer voicer;
        RhythmEngine engine;
    };

    /** One stroke on step `at` of a 16-step bar, rests elsewhere. */
    RhythmPattern oneStrum (StrumType type, double patternSps = 0.0, double stepSps = 0.0, int at = 0)
    {
        RhythmPattern p;
        p.setName ("Test One Strum");
        p.setKind (RhythmPattern::Kind::strum);
        p.setSubdivision (Subdivision::sixteenth);
        p.setLength (16);
        p.setSwing (0.5);
        p.setCrossingSps (patternSps);

        StrumStep step;
        step.type = type;
        step.dynamic = 0.8;
        step.stringMask = 0x0FFF;
        step.crossingSps = stepSps;
        p.setStrumStep (at, step);

        return p;
    }

    /** The first strum's hits: everything within 200 ms of the first. */
    std::vector<Hit> firstStrum (const std::vector<Hit>& hits)
    {
        std::vector<Hit> strum;

        if (hits.empty())
            return strum;

        for (const auto& h : hits)
            if (h.sample < hits.front().sample + (int64_t) (0.2 * kSr))
                strum.push_back (h);

        return strum;
    }

    double spanSamples (const std::vector<Hit>& strum)
    {
        return strum.size() < 2 ? 0.0 : (double) (strum.back().sample - strum.front().sample);
    }

    /** The largest error between consecutive hits and `expected` samples. */
    double worstGapError (const std::vector<Hit>& strum, double expected)
    {
        double worst = 0.0;

        for (size_t i = 1; i < strum.size(); ++i)
            worst = juce::jmax (worst, std::abs ((double) (strum[i].sample - strum[i - 1].sample) - expected));

        return worst;
    }

    //==========================================================================
    struct InterpreterFixture
    {
        InterpreterFixture()
        {
            tuning.prepare (kSr);
            tuning.setNumStrings (6);
            tuning.setTuningPreset (TuningPreset::Standard);

            voicer.prepare (&tuning, 6);
            technique.prepare (kSr, 6);

            interpreter.prepare (kSr, 6);
            interpreter.setEngines (&tuning, &technique, &voicer);
            interpreter.setNumStrings (6);
            interpreter.setPlayingMode (PlayingMode::Poly);

            MidiInterpreter::Humanisation flat;
            flat.amount = 0.0;
            interpreter.setHumanisation (flat);

            interpreter.setStrumSettings (flatSettings());
            interpreter.setStrumSpeedMs (5.0);   // 200 sps, the global default
        }

        /** Sends one block and returns the note-on offsets, sorted. */
        juce::Array<int> block (std::initializer_list<std::pair<int, int>> notesAt,
                                int64_t start, int numSamples, int channelBase = 1, bool channelPerNote = false)
        {
            juce::MidiBuffer midi;
            int channel = channelBase;

            for (const auto& n : notesAt)
            {
                midi.addEvent (juce::MidiMessage::noteOn (channel, n.second, 0.8f), n.first);

                if (channelPerNote)
                    ++channel;
            }

            auto out = std::make_unique<PlayEventQueue>();
            interpreter.processBlock (midi, numSamples, start, *out);

            juce::Array<int> offsets;

            for (int i = 0; i < out->getNumNoteOns(); ++i)
                offsets.add (out->getNoteOn (i).sampleOffset);

            offsets.sort();
            return offsets;
        }

        TuningEngine tuning;
        RubricVoicer voicer;
        TechniqueEngine technique;
        MidiInterpreter interpreter;
    };

    //==========================================================================
    float plainOf (LuthierAudioProcessor& p, const char* id)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id)))
            return param->convertFrom0to1 (param->getValue());

        return -1.0e9f;
    }

    void setPlain (LuthierAudioProcessor& p, const char* id, float plain)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id)))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    }

    //==========================================================================
    /** A whole engine playing the rhythm engine's pattern over a held chord. */
    std::vector<float> renderRhythm (LuthierEngine& engine, const RhythmPattern& pattern,
                                     std::initializer_list<int> held, double seconds)
    {
        auto& rhythm = engine.getRhythmEngine();
        rhythm.setEnabled (true);
        rhythm.setPattern (pattern);

        RhythmHumanise flat;
        flat.timingMs = 0.0;
        flat.velocityPercent = 0.0;
        flat.missPercent = 0.0;
        flat.ghostPercent = 0.0;
        flat.amount = 0.0;
        rhythm.setHumanise (flat);

        engine.setTempoBpm (120.0);

        const double perBeat = 0.5 * kSr;
        const int blocks = (int) std::ceil (seconds * kSr / (double) kBlock);

        juce::AudioBuffer<float> buffer (2, kBlock);
        std::vector<float> mono;
        mono.reserve ((size_t) (blocks * kBlock));

        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;

            if (b == 0)
                for (int note : held)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            engine.setTransportPosition ((double) b * (double) kBlock / perBeat, true);

            buffer.clear();
            engine.processBlock (buffer, midi);

            for (int i = 0; i < kBlock; ++i)
                mono.push_back (0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)));
        }

        return mono;
    }

    /** The strongest normalised autocorrelation over the lags of 70-340 Hz, in
        [start, start + length): how clearly anything in the E range repeats. */
    double pitchClarity (const std::vector<float>& x, int start, int length)
    {
        const int minLag = (int) (kSr / 340.0);
        const int maxLag = (int) (kSr / 70.0);
        double best = 0.0;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double num = 0.0, e1 = 0.0, e2 = 0.0;

            for (int i = start; i < start + length && i + lag < (int) x.size(); ++i)
            {
                const double a = x[(size_t) i], b = x[(size_t) (i + lag)];
                num += a * b;
                e1 += a * a;
                e2 += b * b;
            }

            if (e1 > 0.0 && e2 > 0.0)
                best = juce::jmax (best, num / std::sqrt (e1 * e2));
        }

        return best;
    }

    double rms (const std::vector<float>& x, int start, int length)
    {
        double sum = 0.0;
        int n = 0;

        for (int i = start; i < start + length && i < (int) x.size(); ++i, ++n)
            sum += (double) x[(size_t) i] * (double) x[(size_t) i];

        return n > 0 ? std::sqrt (sum / n) : 0.0;
    }

    double peak (const std::vector<float>& x, int start, int length)
    {
        double p = 0.0;

        for (int i = start; i < start + length && i < (int) x.size(); ++i)
            p = juce::jmax (p, (double) std::abs (x[(size_t) i]));

        return p;
    }
}

//==============================================================================
//  The gesture on its own
//==============================================================================

/*  8: "At 200 sps with acceleration 0, six per-string events are spaced 5 ms
    +- 1 sample." */
LUTHIER_TEST (StrumDynamics, crossingTimingIsExact)
{
    StrumGesture gesture;
    std::array<StrumStrike, kMaxStrings> strikes {};

    const int n = gesture.plan (flatSettings(), requestFor (kDownSix, 6, true, 200.0),
                                strikes.data(), (int) strikes.size());
    CHECK (n == 6);

    for (int i = 0; i < n; ++i)
    {
        CHECK (strikes[(size_t) i].stringIndex == kDownSix[i]);
        CHECK_NEAR (strikes[(size_t) i].timeSeconds * kSr, 240.0 * i, 1.0);
    }

    // 1's table: the six-string crossing at each named feel.
    CHECK_NEAR (StrumGesture::crossingSeconds (6, 20.0),  0.250, 1.0e-12);
    CHECK_NEAR (StrumGesture::crossingSeconds (6, 200.0), 0.025, 1.0e-12);
    CHECK_NEAR (StrumGesture::crossingSeconds (6, 800.0), 0.00625, 1.0e-12);
    CHECK (StrumGesture::crossingSeconds (1, 200.0) == 0.0);
}

/*  The same through the rhythm engine: the note-ons it writes are 240 samples
    apart at the global 200 sps. */
LUTHIER_TEST (StrumDynamics, rhythmEngineStrumsAreSpacedExactly)
{
    StrumFixture f;
    f.holdOpenE();
    f.engine.setPattern (oneStrum (StrumType::down));

    const auto strum = firstStrum (f.play (0.3));

    CHECK_MSG (strum.size() >= 4, "only " + juce::String ((int) strum.size()) + " strings were struck");
    CHECK_MSG (worstGapError (strum, kSr / 200.0) <= 1.0,
               "worst gap error " + juce::String (worstGapError (strum, kSr / 200.0), 2) + " samples");

    // Down crosses from the low strings, which carry the high indices.
    for (size_t i = 1; i < strum.size(); ++i)
        CHECK (strum[i].stringIndex < strum[i - 1].stringIndex);
}

/*  8: "At acceleration 1.0 the total crossing time is within 2 % of the
    acceleration-0 case, while the first gap is at least 1.6x the middle gap." */
LUTHIER_TEST (StrumDynamics, accelerationChangesSpacingNotDuration)
{
    StrumGesture gesture;
    std::array<StrumStrike, kMaxStrings> even {}, eased {};

    auto settings = flatSettings();
    gesture.plan (settings, requestFor (kDownSix, 6, true, 200.0), even.data(), 6);

    settings.acceleration = 1.0;
    gesture.plan (settings, requestFor (kDownSix, 6, true, 200.0), eased.data(), 6);

    const double totalEven = even[5].timeSeconds;
    const double totalEased = eased[5].timeSeconds;

    CHECK_MSG (std::abs (totalEased - totalEven) <= 0.02 * totalEven,
               "total " + juce::String (totalEased * 1000.0, 3) + " ms against "
                 + juce::String (totalEven * 1000.0, 3) + " ms");

    const double firstGap = eased[1].timeSeconds - eased[0].timeSeconds;
    const double middleGap = eased[3].timeSeconds - eased[2].timeSeconds;
    const double lastGap = eased[5].timeSeconds - eased[4].timeSeconds;

    CHECK_MSG (firstGap >= 1.6 * middleGap,
               "first gap " + juce::String (firstGap * 1000.0, 3) + " ms, middle "
                 + juce::String (middleGap * 1000.0, 3) + " ms");

    // 2: "the first and last gaps are roughly twice the middle ones".
    CHECK (lastGap >= 1.6 * middleGap);

    // The ease is monotonic and pinned at both ends, at every setting.
    for (double a : { 0.0, 0.35, 0.7, 1.0 })
    {
        CHECK_NEAR (StrumGesture::ease (0.0, a), 0.0, 1.0e-12);
        CHECK_NEAR (StrumGesture::ease (1.0, a), 1.0, 1.0e-12);

        double previous = -1.0;

        for (int i = 0; i <= 100; ++i)
        {
            const double t = StrumGesture::ease (i / 100.0, a);
            CHECK (t >= previous);
            previous = t;
        }
    }
}

/*  8: "Up is faster than down by strum_up_velocity_ratio within 1 %." */
LUTHIER_TEST (StrumDynamics, upIsFasterThanDownByTheRatio)
{
    StrumGesture gesture;
    std::array<StrumStrike, kMaxStrings> down {}, up {};

    for (double ratio : { 1.25, 0.8, 2.0 })
    {
        auto settings = StrumSettings::guitarDefaults();   // acceleration on: it must not matter
        settings.missProbability = 0.0;
        settings.upVelocityRatio = ratio;

        gesture.plan (settings, requestFor (kDownSix, 6, true, 200.0), down.data(), 6);
        gesture.plan (settings, requestFor (kUpSix, 6, false, 200.0), up.data(), 6);

        const double measured = down[5].timeSeconds / up[5].timeSeconds;
        CHECK_MSG (std::abs (measured - ratio) <= 0.01 * ratio,
                   "ratio " + juce::String (ratio) + " measured " + juce::String (measured, 4));
    }

    // And through the engine, at the default 1.25.
    auto downFixture = std::make_unique<StrumFixture>();
    auto upFixture = std::make_unique<StrumFixture>();
    downFixture->holdOpenE();
    upFixture->holdOpenE();
    downFixture->engine.setPattern (oneStrum (StrumType::down));
    upFixture->engine.setPattern (oneStrum (StrumType::up));

    const auto downStrum = firstStrum (downFixture->play (0.3));
    const auto upStrum = firstStrum (upFixture->play (0.3));

    CHECK (downStrum.size() == upStrum.size());

    if (downStrum.size() > 2 && downStrum.size() == upStrum.size())
    {
        const double measured = spanSamples (downStrum) / spanSamples (upStrum);
        CHECK_MSG (std::abs (measured - 1.25) <= 0.0125 + 2.0 / spanSamples (upStrum),
                   "engine up/down ratio " + juce::String (measured, 4));
    }

    // An up-strum starts at the highest-pitched string: the lowest index.
    for (size_t i = 1; i < upStrum.size(); ++i)
        CHECK (upStrum[i].stringIndex > upStrum[i - 1].stringIndex);
}

/*  8: "Over 10 000 strums the miss rate matches strum_miss_probability within
    5 % relative, the leading string misses about 3x as often, and a fixed
    seed reproduces the exact pattern." */
LUTHIER_TEST (StrumDynamics, missesAreWeightedAndDeterministic)
{
    constexpr int kStrums = 10000;

    auto settings = StrumSettings::guitarDefaults();
    settings.missProbability = 0.04;

    StrumGesture gesture;
    gesture.setSeed (0xA5A5F00Dull);

    std::array<StrumStrike, kMaxStrings> strikes {};
    std::array<int, 6> missesByOrder {};
    int total = 0;

    for (int s = 0; s < kStrums; ++s)
    {
        gesture.plan (settings, requestFor (kDownSix, 6, true, 200.0, (juce::uint32) s), strikes.data(), 6);

        for (int i = 0; i < 6; ++i)
            if (strikes[(size_t) i].missed)
            {
                ++missesByOrder[(size_t) i];
                ++total;
            }
    }

    const double rate = (double) total / (6.0 * kStrums);
    CHECK_MSG (std::abs (rate - 0.04) <= 0.05 * 0.04,
               "miss rate " + juce::String (rate, 5) + ", expected 0.04 +- 5 %");

    const double others = (missesByOrder[1] + missesByOrder[2] + missesByOrder[3]
                           + missesByOrder[4] + missesByOrder[5]) / 5.0;
    const double leadingRatio = missesByOrder[0] / juce::jmax (1.0, others);

    CHECK_MSG (leadingRatio > 2.5 && leadingRatio < 3.5,
               "leading string missed " + juce::String (leadingRatio, 2) + "x as often as the rest");

    // The rule itself: 3x, with the mean held at p.
    const double lead = StrumGesture::missChance (0, 6, 0.04);
    const double rest = StrumGesture::missChance (3, 6, 0.04);
    CHECK_NEAR (lead / rest, 3.0, 1.0e-9);
    CHECK_NEAR ((lead + 5.0 * rest) / 6.0, 0.04, 1.0e-12);

    // A fixed seed reproduces the pattern exactly; another seed does not.
    auto fingerprint = [&settings] (juce::uint64 seed)
    {
        StrumGesture g;
        g.setSeed (seed);
        std::array<StrumStrike, kMaxStrings> st {};
        juce::String pattern;

        for (int s = 0; s < 2000; ++s)
        {
            g.plan (settings, requestFor (kDownSix, 6, true, 200.0, (juce::uint32) s), st.data(), 6);

            for (int i = 0; i < 6; ++i)
                if (st[(size_t) i].missed)
                    pattern << s << ":" << i << " ";
        }

        return pattern;
    };

    CHECK (fingerprint (0x1234ull) == fingerprint (0x1234ull));
    CHECK (fingerprint (0x1234ull) != fingerprint (0x4321ull));

    // missScale 0 is a hand that never misses (the live interpreter's case).
    auto never = requestFor (kDownSix, 6, true, 200.0);
    never.missScale = 0.0;
    settings.missProbability = 1.0;

    gesture.plan (settings, never, strikes.data(), 6);

    for (int i = 0; i < 6; ++i)
        CHECK (! strikes[(size_t) i].missed);
}

/*  8: "At evenness 1.0 all per-string forces are equal; at 0 the spread is
    within +-40 %." */
LUTHIER_TEST (StrumDynamics, evennessBoundsTheVariation)
{
    double lowest = 10.0, highest = 0.0;

    for (juce::uint32 strum = 0; strum < 2000; ++strum)
    {
        for (int s = 0; s < 6; ++s)
        {
            CHECK (StrumGesture::evennessFactor (0x77ull, strum, s, 1.0) == 1.0);

            const double f = StrumGesture::evennessFactor (0x77ull, strum, s, 0.0);
            lowest = juce::jmin (lowest, f);
            highest = juce::jmax (highest, f);
        }
    }

    CHECK_MSG (lowest >= 0.6 && highest <= 1.4,
               "evenness 0 ranged " + juce::String (lowest, 3) + " to " + juce::String (highest, 3));

    // And the range is used, not just bounded.
    CHECK (lowest < 0.65 && highest > 1.35);

    // Through a plan with no tilt: at evenness 1 the only differences between
    // strings are the accent's.
    auto settings = flatSettings();
    settings.tilt = 0.0;
    settings.evenness = 1.0;

    StrumGesture gesture;
    std::array<StrumStrike, kMaxStrings> strikes {};
    gesture.plan (settings, requestFor (kDownSix, 6, true, 200.0, 17), strikes.data(), 6);

    for (int i = 1; i < 6; ++i)
        CHECK_NEAR (strikes[(size_t) i].force / StrumGesture::accentFactor (i),
                    strikes[0].force / StrumGesture::accentFactor (0), 1.0e-12);
}

/*  3: the tilt favours the treble in both directions, and the accent lands
    on the first two strings crossed - the bass two down, the treble two up. */
LUTHIER_TEST (StrumDynamics, tiltAndAccentShapeTheForce)
{
    CHECK_NEAR (dbToGain (StrumGesture::kAccentDb), StrumGesture::accentFactor (0), 1.0e-12);
    CHECK (StrumGesture::accentFactor (1) > 1.0);
    CHECK (StrumGesture::accentFactor (2) == 1.0);

    // Down: later strings are treble, and positive tilt raises them.
    CHECK (StrumGesture::tiltFactor (5, 6, 0.15, true) > StrumGesture::tiltFactor (0, 6, 0.15, true));

    // Up: later strings are bass, and the same setting now lowers them.
    CHECK (StrumGesture::tiltFactor (5, 6, 0.15, false) < StrumGesture::tiltFactor (0, 6, 0.15, false));

    // The loudest string of a down-strum is at the base force (see plan), never over it.
    StrumGesture gesture;
    std::array<StrumStrike, kMaxStrings> strikes {};
    auto settings = flatSettings();
    settings.evenness = 1.0;

    gesture.plan (settings, requestFor (kDownSix, 6, true, 200.0), strikes.data(), 6);

    double loudest = 0.0;

    for (int i = 0; i < 6; ++i)
        loudest = juce::jmax (loudest, strikes[(size_t) i].force);

    CHECK_NEAR (loudest, 1.0, 1.0e-9);
}

/*  2.1: the up-stroke "hits at a shallower angle: softer". */
LUTHIER_TEST (StrumDynamics, upStrokesAreSofter)
{
    auto settings = flatSettings();
    settings.evenness = 1.0;
    settings.tilt = 0.0;

    StrumGesture gesture;
    std::array<StrumStrike, kMaxStrings> down {}, up {};

    gesture.plan (settings, requestFor (kDownSix, 6, true, 200.0), down.data(), 6);
    gesture.plan (settings, requestFor (kUpSix, 6, false, 200.0), up.data(), 6);

    for (int i = 0; i < 6; ++i)
        CHECK_NEAR (up[(size_t) i].force, down[(size_t) i].force * StrumGesture::kUpStrokeForce, 1.0e-12);

    CHECK (StrumGesture::kUpStrokeForce < 1.0);
}

/*  8: "Feel knob maps as specified. Feel 0, 0.5 and 1.0 produce the three
    documented crossing velocities within 1 %." */
LUTHIER_TEST (StrumDynamics, feelMapsAsSpecified)
{
    CHECK_NEAR (StrumFeel::crossingSpsFor (0.0), 60.0,  0.6);
    CHECK_NEAR (StrumFeel::crossingSpsFor (0.5), 200.0, 2.0);
    CHECK_NEAR (StrumFeel::crossingSpsFor (1.0), 400.0, 4.0);

    CHECK_NEAR (StrumFeel::evennessFor (0.0), 0.45, 1.0e-9);
    CHECK_NEAR (StrumFeel::evennessFor (0.5), 0.75, 1.0e-9);
    CHECK_NEAR (StrumFeel::evennessFor (1.0), 0.95, 1.0e-9);

    // Faster and tighter as it goes up, everywhere.
    for (int i = 1; i <= 20; ++i)
    {
        CHECK (StrumFeel::crossingSpsFor (i / 20.0) > StrumFeel::crossingSpsFor ((i - 1) / 20.0));
        CHECK (StrumFeel::evennessFor (i / 20.0) > StrumFeel::evennessFor ((i - 1) / 20.0));
    }

    // Through the rhythm engine, with the global source at its default.
    const std::pair<double, double> cases[] = { { 0.0, 60.0 }, { 0.5, 200.0 }, { 1.0, 400.0 } };

    for (const auto& [feel, sps] : cases)
    {
        auto f = std::make_unique<StrumFixture>();
        f->holdOpenE();
        f->engine.setPattern (oneStrum (StrumType::down));
        f->engine.setStrumFeel (feel);

        const auto strum = firstStrum (f->play (0.3));
        CHECK (strum.size() >= 4);

        if (strum.size() >= 2)
        {
            const double measuredSps = kSr * (double) (strum.size() - 1) / spanSamples (strum);
            CHECK_MSG (std::abs (measuredSps - sps) <= 0.01 * sps + 0.5,
                       "feel " + juce::String (feel) + " crossed at " + juce::String (measuredSps, 2) + " sps");
        }
    }
}

//==============================================================================
//  Where the crossing velocity comes from (ambiguity-resolutions 6)
//==============================================================================

/*  ambiguity-resolutions 6.1: "Rhythm engine voicing a chord from a held root
    produces per-string events spaced per the active pattern's crossing_sps
    within 1 sample." Held as a single root, as the resolution describes. */
LUTHIER_TEST (StrumDynamics, rhythmEngineSuppliesVelocityFromThePattern)
{
    StrumFixture f;
    f.hold ({ 40 });
    f.engine.setPattern (oneStrum (StrumType::down, 150.0));

    const auto strum = firstStrum (f.play (0.3));

    CHECK_MSG (strum.size() >= 2,
               "a held root voiced to " + juce::String ((int) strum.size())
                 + " string(s); nothing to space");
    CHECK_MSG (worstGapError (strum, kSr / 150.0) <= 1.0,
               "spacing off the pattern's 150 sps by " + juce::String (worstGapError (strum, kSr / 150.0), 2)
                 + " samples");

    // And the same with a full chord held.
    StrumFixture chord;
    chord.holdOpenE();
    chord.engine.setPattern (oneStrum (StrumType::down, 150.0));

    const auto chordStrum = firstStrum (chord.play (0.3));
    CHECK (chordStrum.size() >= 4);
    CHECK (worstGapError (chordStrum, kSr / 150.0) <= 1.0);
}

/*  ambiguity-resolutions 6.1: "Chord progression looper input at 120 bpm with
    a pattern crossing_sps = 220 produces the documented 22.7 ms spread." The
    spread is the six-string crossing, so acceleration (on here, at its
    default) does not change it. */
LUTHIER_TEST (StrumDynamics, patternCrossingGivesTheDocumentedSpread)
{
    StrumFixture f;

    auto settings = StrumSettings::guitarDefaults();
    settings.missProbability = 0.0;
    f.engine.setStrumSettings (settings);

    f.holdOpenE();
    f.engine.setPattern (oneStrum (StrumType::down, 220.0));

    const auto strum = firstStrum (f.play (0.3, 120.0));

    CHECK_MSG (strum.size() == 6, "the open E voiced to " + juce::String ((int) strum.size()) + " strings");
    CHECK_MSG (std::abs (spanSamples (strum) - 5.0 / 220.0 * kSr) <= 1.0,
               "spread " + juce::String (spanSamples (strum) / kSr * 1000.0, 3) + " ms, expected 22.727 ms");
}

/*  ambiguity-resolutions 6: a step's own crossing_sps, then the pattern's,
    then the genre kit's default, then the plugin-global parameter. */
LUTHIER_TEST (StrumDynamics, crossingSourcesResolveInOrder)
{
    StrumFixture f;

    auto settings = flatSettings();
    settings.crossingSps = 333.0;
    f.engine.setStrumSettings (settings);

    StrumStep step;
    step.type = StrumType::down;

    RhythmPattern pattern;
    RhythmEngine::CrossingSource source {};

    CHECK_NEAR (f.engine.resolveCrossingSps (step, pattern, &source), 333.0, 1.0e-9);
    CHECK (source == RhythmEngine::CrossingSource::global);

    // A kit's six-string crossing time: 20 ms is 5 gaps at 250 sps.
    f.engine.setStrumDurationMs (20.0);
    CHECK_NEAR (f.engine.resolveCrossingSps (step, pattern, &source), 250.0, 1.0e-9);
    CHECK (source == RhythmEngine::CrossingSource::kit);

    pattern.setCrossingSps (180.0);
    CHECK_NEAR (f.engine.resolveCrossingSps (step, pattern, &source), 180.0, 1.0e-9);
    CHECK (source == RhythmEngine::CrossingSource::pattern);

    step.crossingSps = 400.0;
    CHECK_NEAR (f.engine.resolveCrossingSps (step, pattern, &source), 400.0, 1.0e-9);
    CHECK (source == RhythmEngine::CrossingSource::step);

    // Clearing the kit hands the strum back to the parameter.
    f.engine.setStrumDurationMs (0.0);
    CHECK (f.engine.getStrumDurationMs() == 0.0);
    CHECK_NEAR (f.engine.resolveCrossingSps (StrumStep {}, RhythmPattern {}, &source), 333.0, 1.0e-9);

    // A step override is heard, not just resolved.
    f.holdOpenE();
    f.engine.setPattern (oneStrum (StrumType::down, 150.0, 400.0));

    const auto strum = firstStrum (f.play (0.3));
    CHECK (strum.size() >= 4);
    CHECK (worstGapError (strum, kSr / 400.0) <= 1.0);
}

/*  1.1 and ground rule 1: humanised timing moves the whole gesture, not each
    string, so the spacing survives it. */
LUTHIER_TEST (StrumDynamics, humanisedTimingMovesTheWholeGesture)
{
    StrumFixture f;
    f.holdOpenE();

    // On beat 2, so a gesture pulled early is not clamped against sample 0.
    f.engine.setPattern (oneStrum (StrumType::down, 0.0, 0.0, 4));
    f.engine.setSeed (0xBEEFull);

    RhythmHumanise loose;
    loose.timingMs = 15.0;
    loose.velocityPercent = 0.0;
    loose.missPercent = 0.0;
    loose.ghostPercent = 0.0;
    loose.amount = 1.0;
    f.engine.setHumanise (loose);

    // Misses would remove strings from the spacing; the setting stays at 0.
    const auto strum = firstStrum (f.play (0.9));

    CHECK_MSG (! strum.empty() && std::abs ((double) strum.front().sample - 0.5 * kSr) > 1.0,
               "the humanised strum landed exactly on the grid; timing humanise did nothing");

    CHECK (strum.size() >= 4);
    CHECK_MSG (worstGapError (strum, kSr / 200.0) <= 1.0,
               "humanise broke the spacing by " + juce::String (worstGapError (strum, kSr / 200.0), 2));
}

/*  8: "Live spread wins. A keyboard chord with 40 ms of note spread produces a
    crossing of 40 ms, not the pattern's." With the chord window wide enough to
    group it and with a 2 ms window that splits it, the notes sound at their
    own arrival, one window late. */
LUTHIER_TEST (StrumDynamics, liveSpreadWins)
{
    const int step = (int) (0.008 * kSr);   // 8 ms between notes, 40 ms in all

    for (double windowMs : { 50.0, 2.0 })
    {
        InterpreterFixture f;
        f.interpreter.setChordWindowMs (windowMs);

        const auto offsets = f.block ({ { 0 * step, 40 }, { 1 * step, 47 }, { 2 * step, 52 },
                                        { 3 * step, 56 }, { 4 * step, 59 }, { 5 * step, 64 } },
                                      0, 4096);

        CHECK_MSG (offsets.size() == 6,
                   "window " + juce::String (windowMs) + " ms: " + juce::String (offsets.size()) + " note-ons");

        if (offsets.size() == 6)
            CHECK_MSG (std::abs ((offsets.getLast() - offsets.getFirst()) - 5 * step) <= 1,
                       "window " + juce::String (windowMs) + " ms: crossing "
                         + juce::String ((offsets.getLast() - offsets.getFirst()) / kSr * 1000.0, 2)
                         + " ms, expected 40 ms");
    }

    // A chord that arrives all at once is strummed at the global crossing
    // instead: 25 ms for six strings at 200 sps.
    InterpreterFixture f;
    f.interpreter.setChordWindowMs (2.0);

    const auto offsets = f.block ({ { 10, 40 }, { 10, 47 }, { 10, 52 }, { 10, 56 }, { 10, 59 }, { 10, 64 } },
                                  0, 4096);

    CHECK (offsets.size() == 6);

    if (offsets.size() == 6)
        CHECK_NEAR (offsets.getLast() - offsets.getFirst(), 0.025 * kSr, 1.0);
}

/*  8: "MPE passes through. Per-string events on separate channels arrive in
    their original timing with no strum synthesis." */
LUTHIER_TEST (StrumDynamics, mpePassesThrough)
{
    // MPE in Poly mode: six notes at once on six channels stay at once.
    {
        InterpreterFixture f;
        f.interpreter.setChordWindowMs (2.0);
        f.interpreter.setMpeEnabled (true);

        const auto offsets = f.block ({ { 10, 40 }, { 10, 47 }, { 10, 52 }, { 10, 56 }, { 10, 59 }, { 10, 64 } },
                                      0, 4096, 2, true);

        CHECK (offsets.size() == 6);

        if (offsets.size() == 6)
            CHECK_MSG (offsets.getFirst() == offsets.getLast(),
                       "MPE notes played together were spread over "
                         + juce::String (offsets.getLast() - offsets.getFirst()) + " samples");
    }

    // Hex routing (GuitarController): each string at exactly its own sample.
    {
        InterpreterFixture f;
        f.interpreter.setPlayingMode (PlayingMode::GuitarController);

        const auto offsets = f.block ({ { 3, 40 }, { 50, 45 }, { 97, 50 }, { 180, 55 }, { 181, 59 }, { 300, 64 } },
                                      0, 512, 1, true);

        CHECK (offsets.size() == 6);

        const int expected[] = { 3, 50, 97, 180, 181, 300 };

        for (int i = 0; i < juce::jmin (6, offsets.size()); ++i)
            CHECK_MSG (offsets[i] == expected[i],
                       "hex note " + juce::String (i) + " at " + juce::String (offsets[i])
                         + ", played at " + juce::String (expected[i]));
    }
}

//==============================================================================
//  Strikers and chucks
//==============================================================================

/*  5: the striker's crossing factor, and its material. */
LUTHIER_TEST (StrumDynamics, strikersCrossAtTheirOwnSpeed)
{
    CHECK (getStrikerCrossingFactor (Striker::pick) == 1.0);
    CHECK (getStrikerCrossingFactor (Striker::thumb) == 0.6);
    CHECK (getStrikerCrossingFactor (Striker::fingernails) == 1.3);
    CHECK (getStrikerCrossingFactor (Striker::flesh) == 0.7);
    CHECK (getStrikerCrossingFactor (Striker::thumbpick) == 0.9);
    CHECK (getStrikerCrossingFactor (Striker::brush) == 0.4);

    auto settings = flatSettings();
    settings.strikerDown = Striker::thumb;

    CHECK_NEAR (StrumGesture::effectiveCrossingSps (settings, 200.0, true), 120.0, 1.0e-9);
    CHECK_NEAR (StrumGesture::effectiveCrossingSps (settings, 200.0, false), 250.0, 1.0e-9);

    CHECK (getStrikerMaterial (Striker::pick) == -1);
    CHECK (getStrikerMaterial (Striker::thumb) == (int) Excitation::Material::Thumb);
    CHECK (getStrikerMaterial (Striker::fingernails) == (int) Excitation::Material::Fingernail);
    CHECK (getStrikerMaterial (Striker::flesh) == (int) Excitation::Material::Fingertip);
    CHECK (getStrikerMaterial (Striker::thumbpick) == (int) Excitation::Material::Thumbpick);
    CHECK (getStrikerMaterial (Striker::brush) == (int) Excitation::Material::Brush);

    CHECK (getStrikerNames().size() == (int) Striker::numStrikers);

    // The rhythm engine carries the striker to each note-on.
    StrumFixture f;
    auto s = flatSettings();
    s.strikerUp = Striker::fingernails;
    f.engine.setStrumSettings (s);
    f.holdOpenE();
    f.engine.setPattern (oneStrum (StrumType::up));

    const auto strum = firstStrum (f.play (0.3));
    CHECK (! strum.empty());

    for (const auto& h : strum)
        CHECK (h.striker == (int) Excitation::Material::Fingernail);
}

/*  8: "Striker selects the noise. A thumb strum triggers no pick-click
    generator; a pick strum does." */
LUTHIER_TEST (StrumDynamics, strikerSelectsTheNoise)
{
    // The rule, at the noise model.
    PickSettings pick;
    pick.clickAmount = 0.5;
    pick.material = Excitation::Material::PickCelluloid;
    CHECK (PlayingNoise::makeClick (pick, 0, 0.8).level > 0.0);

    pick.material = (Excitation::Material) getStrikerMaterial (Striker::thumb);
    CHECK (PlayingNoise::makeClick (pick, 0, 0.8).level == 0.0);

    // And through the whole engine.
    auto clicksFor = [] (Striker striker)
    {
        auto engine = std::make_unique<LuthierEngine>();
        engine->prepare (kSr, kBlock);
        engine->setPickMaterialAndFingers (Excitation::Material::PickCelluloid, false);

        auto s = flatSettings();
        s.strikerDown = striker;
        engine->getRhythmEngine().setStrumSettings (s);

        const auto before = engine->getPlayingNoise().getPool().getTriggerCount (NoiseClass::pickClick);
        renderRhythm (*engine, oneStrum (StrumType::down), { 40, 47, 52, 56, 59, 64 }, 0.3);

        return engine->getPlayingNoise().getPool().getTriggerCount (NoiseClass::pickClick) - before;
    };

    const auto pickClicks = clicksFor (Striker::pick);
    const auto thumbClicks = clicksFor (Striker::thumb);

    CHECK_MSG (pickClicks > 0, "a pick strum triggered no pick click");
    CHECK_MSG (thumbClicks == 0,
               "a thumb strum triggered " + juce::String ((int) thumbClicks) + " pick clicks");
}

/*  6.1: a chuck step is a full chuck; chuck_amount blends every other strum
    toward one; the amount a note-on carries is scaled by chuck_damping. */
LUTHIER_TEST (StrumDynamics, chuckStepsCarryTheChuck)
{
    auto settings = flatSettings();

    CHECK_NEAR (StrumGesture::chuckFor (settings, true), 0.92, 1.0e-12);
    CHECK (StrumGesture::chuckFor (settings, false) == 0.0);

    settings.chuckAmount = 0.5;
    CHECK_NEAR (StrumGesture::chuckFor (settings, false), 0.46, 1.0e-12);

    StrumFixture f;
    f.holdOpenE();
    f.engine.setPattern (oneStrum (StrumType::chuck));

    const auto chuck = firstStrum (f.play (0.3));
    CHECK (chuck.size() >= 4);

    for (const auto& h : chuck)
        CHECK_NEAR (h.chuck, 0.92, 1.0e-9);

    // A chuck is a down-stroke: low strings first.
    for (size_t i = 1; i < chuck.size(); ++i)
        CHECK (chuck[i].stringIndex < chuck[i - 1].stringIndex);

    StrumFixture plain;
    plain.holdOpenE();
    plain.engine.setPattern (oneStrum (StrumType::down));

    for (const auto& h : firstStrum (plain.play (0.3)))
        CHECK (h.chuck == 0.0);
}

/*  8: "Chuck kills pitch. A chuck at chuck_amount 1.0 produces no detectable
    f0 above the noise floor, while retaining the body's resonance." Rendered
    through the whole engine; the comparison is a normal strum of the same
    chord. */
LUTHIER_TEST (StrumDynamics, chuckKillsPitch)
{
    auto renderWith = [] (StrumType type, double chuckAmount)
    {
        auto engine = std::make_unique<LuthierEngine>();
        engine->prepare (kSr, kBlock);
        engine->setAmpBuzzAmount (0.0);   // mains hum repeats in the E range

        auto s = flatSettings();
        s.chuckAmount = chuckAmount;
        engine->getRhythmEngine().setStrumSettings (s);

        return renderRhythm (*engine, oneStrum (type), { 40 }, 0.25);
    };

    const auto normal = renderWith (StrumType::down, 0.0);
    const auto chuck = renderWith (StrumType::down, 1.0);
    const auto chuckStep = renderWith (StrumType::chuck, 0.0);

    const int start = (int) (0.040 * kSr);
    const int length = (int) (0.060 * kSr);

    const double normalClarity = pitchClarity (normal, start, length);
    CHECK_MSG (normalClarity > 0.8,
               "the reference strum is not clearly pitched (" + juce::String (normalClarity, 3) + ")");

    for (const auto* rendered : { &chuck, &chuckStep })
    {
        const double clarity = pitchClarity (*rendered, start, length);
        const double level = rms (*rendered, start, length) / juce::jmax (1.0e-12, rms (normal, start, length));

        // No f0: either nothing repeats, or what is left is under the floor.
        CHECK_MSG (clarity < 0.5 || level < 1.0e-3,
                   "chuck still pitched: clarity " + juce::String (clarity, 3)
                     + " at " + juce::String (gainToDb (level + 1.0e-12), 1) + " dB against the strum");

        // The strike itself is still there, through the body.
        const double attack = peak (*rendered, 0, (int) (0.030 * kSr));
        CHECK_MSG (attack > 0.02 * peak (normal, 0, (int) (0.030 * kSr)),
                   "the chuck's attack is gone: " + juce::String (gainToDb (attack + 1.0e-12), 1) + " dBFS");
    }

    // 6.2: a palm-muted strum is a different thing - it keeps its pitch.
    const auto palm = renderWith (StrumType::downMute, 0.0);
    const double palmClarity = pitchClarity (palm, start, length);

    CHECK_MSG (palmClarity > 0.5,
               "a palm-muted strum lost its pitch (" + juce::String (palmClarity, 3) + ")");
}

//==============================================================================
//  Patterns, parameters, family defaults, presets and the UI
//==============================================================================

/*  ambiguity-resolutions 6 / rhythm-engine 6: crossing_sps on the pattern and
    on a step, and the chuck step, round-trip through .luthierpattern JSON. */
LUTHIER_TEST (StrumDynamics, patternCrossingAndChuckRoundTrip)
{
    auto pattern = oneStrum (StrumType::chuck, 220.0);

    StrumStep override;
    override.type = StrumType::up;
    override.crossingSps = 400.0;
    pattern.setStrumStep (8, override);

    const auto json = juce::JSON::toString (pattern.toVar(), false);
    CHECK (json.contains ("crossing_sps"));
    CHECK (json.contains ("\"Chuck\""));

    const auto reloaded = RhythmPattern::fromVar (juce::JSON::parse (json));

    CHECK_NEAR (reloaded.getCrossingSps(), 220.0, 1.0e-9);
    CHECK (reloaded.getStrumStep (0).type == StrumType::chuck);
    CHECK (reloaded.getStrumStep (0).crossingSps == 0.0);
    CHECK_NEAR (reloaded.getStrumStep (8).crossingSps, 400.0, 1.0e-9);

    // A pattern file with no crossing_sps names none: the kit or global applies.
    const auto plain = RhythmPattern::fromVar (juce::JSON::parse (
        juce::JSON::toString (oneStrum (StrumType::down).toVar(), false)));
    CHECK (plain.getCrossingSps() == 0.0);

    CHECK (isDownStroke (StrumType::chuck));
    CHECK (juce::String (getStrumTypeName (StrumType::chuck)) == "Chuck");
}

/*  7: the nine new parameters, with their ranges and defaults, reach the
    rhythm engine through the bridge. */
LUTHIER_TEST (StrumDynamics, parametersReachTheEngine)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    struct Expect { const char* id; float min, max, def; };

    for (const auto& e : { Expect { ParamIDs::strumCrossingSps,     20.0f, 800.0f, 200.0f },
                           Expect { ParamIDs::strumAcceleration,     0.0f,   1.0f, 0.35f },
                           Expect { ParamIDs::strumUpVelocityRatio,  0.5f,   2.0f, 1.25f },
                           Expect { ParamIDs::strumTilt,            -1.0f,   1.0f, 0.15f },
                           Expect { ParamIDs::strumMissProbability,  0.0f,   1.0f, 0.04f },
                           Expect { ParamIDs::chuckAmount,           0.0f,   1.0f, 0.0f },
                           Expect { ParamIDs::chuckDamping,          0.0f,   1.0f, 0.92f } })
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor->getState().getParameter (e.id));
        CHECK_MSG (p != nullptr, juce::String (e.id) + " is not a parameter");

        if (p == nullptr)
            continue;

        CHECK_NEAR (p->getNormalisableRange().start, e.min, 1.0e-6);
        CHECK_NEAR (p->getNormalisableRange().end, e.max, 1.0e-6);
        CHECK_NEAR (p->convertFrom0to1 (p->getDefaultValue()), e.def, 1.0e-3);
    }

    for (const char* id : { ParamIDs::strumStrikerDown, ParamIDs::strumStrikerUp })
    {
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor->getState().getParameter (id));
        CHECK_MSG (choice != nullptr, juce::String (id) + " is not a choice");

        if (choice != nullptr)
        {
            CHECK (choice->choices == getStrikerNames());
            CHECK (choice->getIndex() == (int) Striker::pick);
        }
    }

    setPlain (*processor, ParamIDs::strumCrossingSps, 320.0f);
    setPlain (*processor, ParamIDs::strumAcceleration, 0.6f);
    setPlain (*processor, ParamIDs::strumUpVelocityRatio, 1.5f);
    setPlain (*processor, ParamIDs::strumTilt, -0.4f);
    setPlain (*processor, ParamIDs::strumMissProbability, 0.1f);
    setPlain (*processor, ParamIDs::strumStrikerDown, (float) (int) Striker::thumb);
    setPlain (*processor, ParamIDs::strumStrikerUp, (float) (int) Striker::fingernails);
    setPlain (*processor, ParamIDs::chuckAmount, 0.3f);
    setPlain (*processor, ParamIDs::chuckDamping, 0.8f);

    processor->getParameterBridge().applyAllNow();

    const auto s = processor->getEngine().getRhythmEngine().getStrumSettings();

    CHECK_NEAR (s.crossingSps, 320.0, 0.01);
    CHECK_NEAR (s.acceleration, 0.6, 1.0e-4);
    CHECK_NEAR (s.upVelocityRatio, 1.5, 1.0e-4);
    CHECK_NEAR (s.tilt, -0.4, 1.0e-4);
    CHECK_NEAR (s.missProbability, 0.1, 1.0e-4);
    CHECK (s.strikerDown == Striker::thumb);
    CHECK (s.strikerUp == Striker::fingernails);
    CHECK_NEAR (s.chuckAmount, 0.3, 1.0e-4);
    CHECK_NEAR (s.chuckDamping, 0.8, 1.0e-4);

    // Live play takes the same global crossing (1.1 source 4).
    CHECK_NEAR (processor->getEngine().getMidiInterpreter().getStrumSpeedMs(), 1000.0 / 320.0, 1.0e-3);
}

/*  8: "Bass defaults apply when the loaded guitar's family is bass." And
    bass-techniques 8: they are defaults, not constraints. */
LUTHIER_TEST (StrumDynamics, bassDefaultsApply)
{
    const auto bass = StrumSettings::bassDefaults();

    CHECK (bass.crossingSps == 100.0);
    CHECK (bass.acceleration == 0.25);
    CHECK (bass.upVelocityRatio == 1.15);
    CHECK (bass.tilt == 0.10);
    CHECK (bass.evenness == 0.85);
    CHECK (bass.missProbability == 0.01);

    // Moving to bass moves only what is still on the guitar default.
    auto mine = StrumSettings::guitarDefaults();
    mine.tilt = 0.5;
    const auto moved = StrumSettings::retargetDefaults (mine, false, true);

    CHECK (moved.crossingSps == 100.0);
    CHECK (moved.missProbability == 0.01);
    CHECK (moved.tilt == 0.5);

    // And back again.
    const auto back = StrumSettings::retargetDefaults (moved, true, false);
    CHECK (back.crossingSps == 200.0);
    CHECK (back.tilt == 0.5);

    // Through the plugin: a family switch to bass.
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    CHECK_NEAR (plainOf (*processor, ParamIDs::strumCrossingSps), 200.0f, 0.01f);
    setPlain (*processor, ParamIDs::strumTilt, 0.5f);

    // Evenness follows only if nothing (a kit, say) has moved it off the default.
    const double evennessBefore = processor->getEngine().getRhythmEngine().getStrumEvenness();
    const double evennessExpected = std::abs (evennessBefore - 0.75) < 1.0e-4 ? 0.85 : evennessBefore;

    CHECK (processor->switchGuitarFamily ("bass"));
    processor->getParameterBridge().applyAllNow();

    CHECK_NEAR (plainOf (*processor, ParamIDs::strumCrossingSps), 100.0f, 0.05f);
    CHECK_NEAR (plainOf (*processor, ParamIDs::strumAcceleration), 0.25f, 1.0e-3f);
    CHECK_NEAR (plainOf (*processor, ParamIDs::strumUpVelocityRatio), 1.15f, 1.0e-3f);
    CHECK_NEAR (plainOf (*processor, ParamIDs::strumMissProbability), 0.01f, 1.0e-3f);
    CHECK_NEAR (processor->getEngine().getRhythmEngine().getStrumEvenness(), evennessExpected, 1.0e-3);
    CHECK_MSG (std::abs (plainOf (*processor, ParamIDs::strumTilt) - 0.5f) < 1.0e-3f,
               "the family switch overwrote a tilt the user had set");

    CHECK_NEAR (processor->getEngine().getRhythmEngine().getStrumSettings().crossingSps, 100.0, 0.05);
}

/*  file-formats / error-recovery: a preset from before strum_crossing_sps set
    the live strum with strum_speed, in ms per string. It loads as the same
    speed. */
LUTHIER_TEST (StrumDynamics, olderPresetsKeepTheirStrumSpeed)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    auto& presets = processor->getPresetManager();
    auto data = presets.toVar ("Old strum", "Test");

    auto* params = data.getProperty ("parameters", {}).getDynamicObject();
    CHECK (params != nullptr);

    auto* oldSpeed = dynamic_cast<juce::RangedAudioParameter*> (processor->getState().getParameter (ParamIDs::strumSpeed));
    CHECK (oldSpeed != nullptr);

    if (params == nullptr || oldSpeed == nullptr)
        return;

    params->removeProperty (ParamIDs::strumCrossingSps);
    params->setProperty (ParamIDs::strumSpeed, oldSpeed->convertTo0to1 (14.0f));

    CHECK (presets.fromVar (data));
    CHECK_NEAR (plainOf (*processor, ParamIDs::strumCrossingSps), 1000.0f / 14.0f, 0.2f);

    // A preset that names both keeps its crossing: the new field wins.
    params->setProperty (ParamIDs::strumCrossingSps,
                         processor->getState().getParameter (ParamIDs::strumCrossingSps)->convertTo0to1 (300.0f));

    CHECK (presets.fromVar (data));
    CHECK_NEAR (plainOf (*processor, ParamIDs::strumCrossingSps), 300.0f, 0.2f);
}

/*  gui-integration 4.4 / strum-dynamics 6.3: the STRUM group drives what it
    shows, says which source sets the crossing, and sits on the RHYTHM tab. */
LUTHIER_TEST (StrumDynamics, theStrumGroupDrivesTheModel)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    auto& rhythm = processor->getEngine().getRhythmEngine();
    rhythm.setStrumDurationMs (0.0);
    rhythm.setPattern (oneStrum (StrumType::down));

    StrumGroup group (*processor);
    group.setSize (360, StrumGroup::preferredHeight);

    // The crossing control is the parameter.
    group.getCrossingControl().getSlider().setValue (300.0, juce::sendNotificationSync);
    CHECK_NEAR (plainOf (*processor, ParamIDs::strumCrossingSps), 300.0f, 1.0f);

    // Evenness is the engine's own.
    group.getEvennessControl().getSlider().setValue (0.3, juce::sendNotificationSync);
    CHECK_NEAR (rhythm.getStrumEvenness(), 0.3, 1.0e-6);

    // The striker dropdowns are the parameters.
    group.getStrikerDownControl().getComboBox().setSelectedItemIndex ((int) Striker::thumb, juce::sendNotificationSync);
    CHECK_NEAR (plainOf (*processor, ParamIDs::strumStrikerDown), (float) (int) Striker::thumb, 1.0e-3f);

    // Which source is in charge, said out loud.
    group.refresh();
    CHECK (group.getSourceText().contains ("control"));
    CHECK (! group.getFollowKnobButton().isVisible());

    rhythm.setStrumDurationMs (20.0);
    group.refresh();
    CHECK_MSG (group.getSourceText().contains ("kit") && group.getSourceText().contains ("250"),
               "source reads \"" + group.getSourceText() + "\"");
    CHECK (group.getFollowKnobButton().isVisible());

    // USE KNOB hands the strum back.
    group.getFollowKnobButton().onClick();
    CHECK (rhythm.getStrumDurationMs() == 0.0);
    CHECK (group.getSourceText().contains ("control"));

    rhythm.setPattern (oneStrum (StrumType::down, 180.0));
    group.refresh();
    CHECK (group.getSourceText().contains ("pattern") && group.getSourceText().contains ("180"));

    // A kit's evenness shows up without a click.
    rhythm.setStrumEvenness (0.62);
    group.refresh();
    CHECK_NEAR (group.getEvennessControl().getSlider().getValue(), 0.62, 1.0e-6);

    // And the RHYTHM tab carries it.
    RhythmPanel panel (*processor);
    int groups = 0;

    for (auto* child : panel.getChildren())
        if (dynamic_cast<StrumGroup*> (child) != nullptr)
            ++groups;

    CHECK_MSG (groups == 1, "the RHYTHM panel holds " + juce::String (groups) + " STRUM groups");

    // gui-integration 19's index: the strikers are mirrored in CHARACTER's PICK group.
    NoiseGroups noise (*processor);
    juce::StringArray mirrored;

    for (auto* child : noise.getChildren())
        if (auto* choice = dynamic_cast<LuthierChoice*> (child))
            mirrored.add (choice->getLearnParameterId());

    CHECK (mirrored.contains (ParamIDs::strumStrikerDown));
    CHECK (mirrored.contains (ParamIDs::strumStrikerUp));
}

/*  gui-integration 3.5 / strum-dynamics 6.3: Easy mode's Feel knob scales the
    strum. Its centre is feel 0.5, the defaults. */
LUTHIER_TEST (StrumDynamics, theEasyFeelKnobScalesTheStrum)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    EasyPanel easy (*processor);
    auto& knob = easy.getRhythmFeelSlider();
    auto& rhythm = processor->getEngine().getRhythmEngine();

    knob.setValue (0.0, juce::sendNotificationSync);
    CHECK_NEAR (rhythm.getStrumFeel(), 0.0, 1.0e-9);

    knob.setValue (knob.getMaximum(), juce::sendNotificationSync);
    CHECK_NEAR (rhythm.getStrumFeel(), 1.0, 1.0e-9);

    knob.setValue (0.5 * (knob.getMinimum() + knob.getMaximum()), juce::sendNotificationSync);
    CHECK_NEAR (rhythm.getStrumFeel(), 0.5, 1.0e-9);

    // A genre kit is written for feel 0.5.
    rhythm.setStrumFeel (0.9);
    processor->applyGenreKit (0);
    CHECK_NEAR (rhythm.getStrumFeel(), 0.5, 1.0e-9);
}
