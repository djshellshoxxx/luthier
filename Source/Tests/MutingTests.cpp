/*  Muting as rhythm (muting-rhythm.md section 9, and the parts of
    technique-cascade.md that muting answers for).

    Three layers, as in StrumGestureTests:

      - the rules on their own (Muting::dampingFor, resolve, humanise) and the
        string they damp (StringEngine's Muted mode, measured on a low E);
      - the rhythm engine, which stamps each note with its pattern step's mute,
        and MuteEngine, which gives every note its final one - the master mode,
        the chuka source, or the live grid for notes the rhythm engine did not
        write;
      - the plugin: the parameters, the preset round trip, the MUTE sub-tab,
        the RHYTHM tab's Mute Row and Easy mode's Mute button.
*/

#include "TestFramework.h"

#include "../Rhythm/Muting.h"
#include "../DSP/Techniques/MuteEngine.h"
#include "../UI/Techniques/TechniquesPanel.h"
#include "../UI/Techniques/TechniquePillRow.h"
#include "../Rhythm/RhythmEngine.h"
#include "../Rhythm/Patterns.h"
#include "../Model/Playing/RubricVoicer.h"
#include "../Model/Playing/TuningEngine.h"
#include "../DSP/String/StringEngine.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../UI/MuteGroup.h"
#include "../UI/RhythmPanel.h"
#include "../UI/EasyPanel.h"

#include <algorithm>
#include <vector>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    constexpr double kLowE = 82.407;

    //==========================================================================
    Excitation::Params pluck (double velocity = 0.8)
    {
        Excitation::Params p;
        p.material = Excitation::Material::PickCelluloid;
        p.kind = Excitation::Kind::Pluck;
        p.pluckPosition = 0.18;
        p.velocity = velocity;
        p.brightness = 0.5;
        return p;
    }

    /** A low E, plucked with `damping` applied first as triggerNote does, rendered. */
    std::vector<double> renderLowE (const MuteDamping& damping, double seconds, double velocity = 0.8)
    {
        StringEngine s;
        s.prepare (kSr, kBlock);

        StringEngine::Physical physical;
        physical.sustainSeconds = 6.0;
        physical.openBrightnessHz = 5000.0;
        s.setPhysical (physical);
        s.snapToFrequency (kLowE);

        if (damping.dampsAtStrike())
            s.setMutedDamping (damping.t60Seconds, damping.cutoffHz);

        s.excite (pluck (velocity * damping.velocityScale));

        std::vector<double> out ((size_t) (seconds * kSr), 0.0);

        for (auto& x : out)
            x = s.processSample (0.0);

        return out;
    }

    /*  T60 from the slope of the envelope between 10 and 40 dB under its peak.
        The envelope is RMS over one period of the low E, hopped by 1 ms, so
        the waveform's own shape does not read as decay.

        Measured on the note itself - a band around the low E's fundamental -
        because the string's output DC blocker (7 Hz, a 23 ms time constant)
        rings on its own after any short, damped strike, and that sub-audio
        tail is not the string: broadband, it read a 50 ms loop T60 as 112 ms. */
    double measureT60 (const std::vector<double>& raw)
    {
        std::vector<double> x (raw.size());
        Biquad band;
        band.setBandpass (kSr, kLowE, 1.5);

        for (size_t i = 0; i < raw.size(); ++i)
            x[i] = band.process (raw[i]);

        const int window = (int) (kSr / kLowE);
        const int hop = (int) (kSr * 0.001);

        std::vector<double> env;

        for (int i = 0; i + window <= (int) x.size(); i += hop)
            env.push_back (rms (x.data() + i, window));

        if (env.empty())
            return -1.0;

        const auto peakIt = std::max_element (env.begin(), env.end());
        const double peakDb = gainToDb (*peakIt);

        int at10 = -1, at40 = -1;

        for (auto it = peakIt; it != env.end(); ++it)
        {
            const double db = gainToDb (*it) - peakDb;
            const int index = (int) (it - env.begin());

            if (at10 < 0 && db <= -10.0) at10 = index;
            if (at40 < 0 && db <= -40.0) { at40 = index; break; }
        }

        if (at10 < 0 || at40 <= at10)
            return -1.0;

        return 60.0 * ((double) (at40 - at10) * 0.001) / 30.0;
    }

    //==========================================================================
    struct Hit
    {
        int64_t sample = 0;
        int stringIndex = 0;
        double velocity = 0.0;
        double chuck = 0.0;
        int muteType = 0;
        double mutePressure = -1.0;
    };

    struct Off
    {
        int64_t sample = 0;
        int stringIndex = 0;
    };

    struct MuteFixture
    {
        MuteFixture()
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

            auto strum = StrumSettings::guitarDefaults();
            strum.missProbability = 0.0;
            engine.setStrumSettings (strum);
        }

        void holdOpenE()
        {
            juce::MidiBuffer midi;

            for (int note : { 40, 47, 52, 56, 59, 64 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            engine.handleMidi (midi, 0);
        }

        void play (double seconds, std::vector<Hit>& hits, std::vector<Off>* offs = nullptr)
        {
            const double perBeat = 0.5 * kSr;   // 120 bpm
            const int blocks = (int) std::ceil (seconds * kSr / (double) kBlock);

            for (int b = 0; b < blocks; ++b)
            {
                auto out = std::make_unique<PlayEventQueue>();
                out->clear();

                RhythmTransport transport;
                transport.bpm = 120.0;
                transport.isPlaying = true;
                transport.ppqPosition = (double) b * (double) kBlock / perBeat;

                engine.processBlock (kBlock, transport, *out);
            mute.apply (*out, kSr, transport.bpm, transport.ppqPosition, true, true);

                for (int i = 0; i < out->getNumNoteOns(); ++i)
                {
                    const auto& e = out->getNoteOn (i);
                    hits.push_back ({ (int64_t) b * kBlock + e.sampleOffset, e.stringIndex, e.velocity,
                                      e.chuck, e.muteType, e.mutePressure });
                }

                if (offs != nullptr)
                    for (int i = 0; i < out->getNumNoteOffs(); ++i)
                        offs->push_back ({ (int64_t) b * kBlock + out->getNoteOff (i).sampleOffset,
                                           out->getNoteOff (i).stringIndex });
            }

            std::stable_sort (hits.begin(), hits.end(),
                              [] (const Hit& a, const Hit& c) { return a.sample < c.sample; });
        }

        void setMuteSettings (const MuteSettings& s) { mute.setSettings (s); }

        /** Played notes, the rhythm engine not writing them: the live grid's path. */
        void applyLiveMutes (PlayEventQueue& q, const RhythmTransport& t)
        {
            mute.apply (q, kSr, t.bpm, t.ppqPosition, t.isPlaying, false);
        }

        TuningEngine tuning;
        RubricVoicer voicer;
        RhythmEngine engine;
        MuteEngine mute;
    };

    /** Down-strums on the four beats of a 16-step bar. */
    RhythmPattern quarterDowns (double dynamic = 0.8)
    {
        RhythmPattern p;
        p.setName ("Test Mute Quarters");
        p.setKind (RhythmPattern::Kind::strum);
        p.setSubdivision (Subdivision::sixteenth);
        p.setLength (16);

        for (int i = 0; i < 16; i += 4)
        {
            StrumStep step;
            step.type = StrumType::down;
            step.dynamic = dynamic;
            step.stringMask = 0x0FFF;
            p.setStrumStep (i, step);
        }

        return p;
    }

    MuteSettings armedNeutral()
    {
        MuteSettings s;
        s.armed = true;
        s.masterMode = -1;
        s.chukaSource = ChukaSource::patternOnly;
        s.humanise = 0.0;
        return s;
    }

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
}

//==============================================================================
//  The rules and the string
//==============================================================================

/*  1 and 2: every type has a file id, and an id this build does not know
    plays open rather than failing. */
LUTHIER_TEST (Muting, typesRoundTripThroughTheirIds)
{
    for (int t = 0; t < (int) MuteType::numTypes; ++t)
        CHECK (muteTypeFromId (getMuteTypeId ((MuteType) t)) == (MuteType) t);

    CHECK (juce::String (getMuteTypeId (MuteType::palmHeavy)) == "palm_mute_heavy");
    CHECK (muteTypeFromId ("palm_mute_sideways") == MuteType::open);
    CHECK (getMuteMasterModeNames().size() == (int) MuteType::numTypes + 1);
    CHECK (getMuteMasterModeNames()[0] == "Off");
}

/*  1 and 3: each type's damping at the default palm, and how pressure and
    position move it. */
LUTHIER_TEST (Muting, eachTypeDampsAsDescribed)
{
    MuteSettings s;

    CHECK (! Muting::dampingFor (MuteType::open, s).dampsAtStrike());
    CHECK_NEAR (Muting::dampingFor (MuteType::palmLight, s).t60Seconds, 0.150, 1.0e-9);
    CHECK_NEAR (Muting::dampingFor (MuteType::palmHeavy, s).t60Seconds, 0.050, 1.0e-9);
    CHECK_NEAR (Muting::dampingFor (MuteType::palmExtreme, s).t60Seconds, 0.020, 1.0e-9);

    const auto ghost = Muting::dampingFor (MuteType::ghost, s);
    CHECK (ghost.dampsAtStrike() && ghost.t60Seconds < 0.02);
    CHECK_NEAR (ghost.velocityScale, 0.4, 1.0e-12);   // 3: ghost note velocity default 0.4

    CHECK (Muting::dampingFor (MuteType::chuka, s).chuck);

    const auto fret = Muting::dampingFor (MuteType::fretMute, s);
    CHECK (! fret.dampsAtStrike());                    // it rings first ...
    CHECK (fret.releaseAfterSeconds > 0.0 && fret.releaseT60Seconds > 0.0);   // ... then stops

    // Harder, or further from the saddle: shorter and darker.
    const auto base = Muting::dampingFor (MuteType::palmHeavy, s);
    const auto harder = Muting::dampingFor (MuteType::palmHeavy, s, 0.9);
    const auto further = Muting::dampingFor (MuteType::palmHeavy, s, -1.0, 70.0);

    CHECK (harder.t60Seconds < base.t60Seconds && harder.cutoffHz < base.cutoffHz);
    CHECK (further.t60Seconds < base.t60Seconds);
    CHECK_NEAR (Muting::palmFactor (0.5, 35.0), 1.0, 1.0e-12);
}

/*  9: "Palm mute heavy on low E: T60 measures 40-60 ms." On the string. */
LUTHIER_TEST (Muting, palmMuteHeavyOnLowEDecaysIn40To60Ms)
{
    MuteSettings s;

    const double heavy = measureT60 (renderLowE (Muting::dampingFor (MuteType::palmHeavy, s), 0.5));
    CHECK_MSG (heavy >= 0.040 && heavy <= 0.060,
               "palm mute heavy T60 " + juce::String (heavy * 1000.0, 1) + " ms");

    // The ladder holds: light rings longer than heavy, extreme shorter.
    const double light = measureT60 (renderLowE (Muting::dampingFor (MuteType::palmLight, s), 1.0));
    const double extreme = measureT60 (renderLowE (Muting::dampingFor (MuteType::palmExtreme, s), 0.5));

    CHECK_MSG (light > heavy && (extreme < heavy || extreme < 0.0),
               "light " + juce::String (light * 1000.0, 1) + " ms, heavy " + juce::String (heavy * 1000.0, 1)
                 + " ms, extreme " + juce::String (extreme * 1000.0, 1) + " ms");
}

/*  9: "Ghost note: output has < -30 dB pitched content." Measured against
    an open note after the strike, where only the pitch can be. Ground rule 2:
    the ghost is still heard - it has an attack. */
LUTHIER_TEST (Muting, aGhostNoteHasNoPitchedContent)
{
    MuteSettings s;

    const auto open = renderLowE (MuteDamping {}, 0.3);
    const auto ghost = renderLowE (Muting::dampingFor (MuteType::ghost, s), 0.3);

    const int start = (int) (0.030 * kSr);
    const int length = (int) (0.200 * kSr);

    const double relative = gainToDb (rms (ghost.data() + start, length) / juce::jmax (1.0e-12, rms (open.data() + start, length)));

    CHECK_MSG (relative < -30.0, "ghost's sustained content " + juce::String (relative, 1) + " dB under open");

    // The thump: the first 20 ms are not silent.
    CHECK (peak (ghost.data(), (int) (0.020 * kSr)) > 1.0e-3);
}

/*  3: random mute humanise. "Random humanise 0.5: over 100 loops, ~50 % of
    eligible steps shift." Deterministic, and ghost / chuka / fret mute never
    move. */
LUTHIER_TEST (Muting, humaniseShiftsAboutHalfTheEligibleSteps)
{
    int shifted = 0, eligible = 0;

    for (juce::uint32 loop = 0; loop < 100; ++loop)
    {
        for (int step = 0; step < 16; ++step)
        {
            const auto from = (step % 2 == 0) ? MuteType::open : MuteType::palmHeavy;
            const auto to = Muting::humanise (from, 0.5, 0xABCDull, loop, step);

            ++eligible;

            if (to != from)
            {
                ++shifted;
                CHECK (to == (from == MuteType::open ? MuteType::palmLight : MuteType::open));
            }

            CHECK (to == Muting::humanise (from, 0.5, 0xABCDull, loop, step));
        }
    }

    const double fraction = (double) shifted / (double) eligible;
    CHECK_MSG (std::abs (fraction - 0.5) <= 0.06, "shifted " + juce::String (fraction * 100.0, 1) + " %");

    for (auto fixed : { MuteType::ghost, MuteType::chuka, MuteType::fretMute })
        for (juce::uint32 loop = 0; loop < 100; ++loop)
            CHECK (Muting::humanise (fixed, 1.0, 1ull, loop, 3) == fixed);

    CHECK (Muting::humanise (MuteType::open, 0.0, 1ull, 0, 0) == MuteType::open);
}

/*  2 and 3: the order a strike's mute is chosen in. */
LUTHIER_TEST (Muting, resolutionOrderIsMasterThenStepThenChuka)
{
    MuteStep heavy;
    heavy.type = MuteType::palmHeavy;
    heavy.pressure = 0.8;

    // Disarmed: the pattern's own type, as written.
    MuteSettings off;
    CHECK (Muting::resolve (heavy, off, true, 0.1, 1, 0, 0).type == MuteType::palmHeavy);
    CHECK (Muting::resolve (MuteStep {}, off, true, 0.1, 1, 0, 0).type == MuteType::open);

    // Armed, master mode set: it overrides, and drops the step's own pressure.
    auto master = armedNeutral();
    master.masterMode = (int) MuteType::ghost;
    const auto r = Muting::resolve (heavy, master, true, 0.9, 1, 0, 0);
    CHECK (r.type == MuteType::ghost && r.pressure < 0.0);

    // Armed, soft strums: an open strum under 0.3 is a chuka; a fingerpick is not.
    auto soft = armedNeutral();
    soft.chukaSource = ChukaSource::softStrums;
    CHECK (Muting::resolve (MuteStep {}, soft, true, 0.2, 1, 0, 0).type == MuteType::chuka);
    CHECK (Muting::resolve (MuteStep {}, soft, true, 0.35, 1, 0, 0).type == MuteType::open);
    CHECK (Muting::resolve (MuteStep {}, soft, false, 0.2, 1, 0, 0).type == MuteType::open);
    CHECK (Muting::resolve (heavy, soft, true, 0.2, 1, 0, 0).type == MuteType::palmHeavy);
}

//==============================================================================
//  The rhythm engine
//==============================================================================

/*  4 / 9: "Existing patterns without mute_type play identically to before."
    Every factory strum pattern, disarmed and armed-but-neutral, writes the
    same notes, none of them muted. */
LUTHIER_TEST (Muting, existingPatternsPlayIdentically)
{
    PatternLibrary library;

    for (int i = 0; i < library.getNumPatterns(); ++i)
    {
        const auto& pattern = library.getPattern (i);

        if (pattern.getKind() != RhythmPattern::Kind::strum || pattern.isEmpty())
            continue;

        std::vector<Hit> disarmed, neutral;

        {
            auto f = std::make_unique<MuteFixture>();
            f->holdOpenE();
            f->engine.setPattern (pattern);
            f->play (2.0, disarmed);
        }

        {
            auto f = std::make_unique<MuteFixture>();
            f->setMuteSettings (armedNeutral());
            f->holdOpenE();
            f->engine.setPattern (pattern);
            f->play (2.0, neutral);
        }

        CHECK_MSG (disarmed.size() == neutral.size(),
                   pattern.getName() + ": " + juce::String ((int) disarmed.size()) + " notes against "
                     + juce::String ((int) neutral.size()));

        for (size_t n = 0; n < juce::jmin (disarmed.size(), neutral.size()); ++n)
        {
            CHECK (disarmed[n].sample == neutral[n].sample);
            CHECK (disarmed[n].velocity == neutral[n].velocity);
            CHECK (disarmed[n].muteType == 0 && neutral[n].muteType == 0);
        }
    }
}

/*  2 and 4: a pattern's mute row reaches every note it strikes, with the
    step's own pressure. */
LUTHIER_TEST (Muting, aPatternsMuteRowReachesItsNotes)
{
    auto f = std::make_unique<MuteFixture>();
    f->holdOpenE();

    auto pattern = quarterDowns();
    MuteStep heavy;
    heavy.type = MuteType::palmHeavy;
    heavy.pressure = 0.7;
    pattern.setMuteStep (4, heavy);
    f->engine.setPattern (pattern);

    std::vector<Hit> hits;
    f->play (1.1, hits);   // beats 1, 2 and 3 of the first bar

    int mutedHits = 0, openHits = 0;

    for (const auto& h : hits)
    {
        const bool onBeat2 = (h.sample >= (int64_t) (0.5 * kSr) && h.sample < (int64_t) (0.6 * kSr));

        if (onBeat2)
        {
            ++mutedHits;
            CHECK (h.muteType == (int) MuteType::palmHeavy);
            CHECK_NEAR (h.mutePressure, 0.7, 1.0e-9);
        }
        else
        {
            ++openHits;
            CHECK (h.muteType == (int) MuteType::open);
        }
    }

    CHECK (mutedHits >= 4 && openHits >= 4);
}

/*  9: "Chuka: strum with dynamics 0.2 produces percussive event with no
    sustained pitch." Armed, the default chuka source turns the soft strum
    into strum-dynamics 6.1's chuck (whose pitch test is chuckKillsPitch). */
LUTHIER_TEST (Muting, aSoftStrumIsAChuka)
{
    auto f = std::make_unique<MuteFixture>();

    MuteSettings s;
    s.armed = true;   // chuka source defaults to soft strums
    f->setMuteSettings (s);

    f->holdOpenE();
    f->engine.setPattern (quarterDowns (0.2));

    std::vector<Hit> hits;
    f->play (0.3, hits);

    CHECK (hits.size() >= 4);

    for (const auto& h : hits)
    {
        CHECK (h.muteType == (int) MuteType::chuka);
        CHECK_NEAR (h.chuck, StrumSettings::guitarDefaults().chuckDamping, 1.0e-9);
    }

    // And the chuck's damping has no pitch left on the string: it is gone in
    // about a period of the low E.
    StringEngine::Physical physical;
    physical.sustainSeconds = 6.0;

    StringEngine string;
    string.prepare (kSr, kBlock);
    string.setPhysical (physical);
    string.snapToFrequency (kLowE);
    string.setDamping (StringEngine::Damping::Chuck, StrumSettings::guitarDefaults().chuckDamping);
    string.excite (pluck());

    std::vector<double> out ((size_t) (0.3 * kSr));

    for (auto& x : out)
        x = string.processSample (0.0);

    const double t60 = measureT60 (out);
    // "No sustained pitch": gone well inside an eighth note at any tempo. The
    // fundamental band's own ring (about 6 ms) bounds how short it can read.
    CHECK_MSG (t60 > 0.0 && t60 < 0.05, "chuka T60 " + juce::String (t60 * 1000.0, 1) + " ms");
}

/*  3: the master mode overrides every step, on the rhythm engine's notes and
    on played notes alike - with the transport stopped too, which is how Easy
    mode's Mute button works under a player's hands. */
LUTHIER_TEST (Muting, theMasterModeOverridesEverything)
{
    auto f = std::make_unique<MuteFixture>();

    auto s = armedNeutral();
    s.masterMode = (int) MuteType::palmExtreme;
    f->setMuteSettings (s);

    auto pattern = quarterDowns();
    MuteStep ghost;
    ghost.type = MuteType::ghost;
    pattern.setMuteStep (0, ghost);

    f->holdOpenE();
    f->engine.setPattern (pattern);

    std::vector<Hit> hits;
    f->play (1.0, hits);

    CHECK (! hits.empty());

    for (const auto& h : hits)
        CHECK (h.muteType == (int) MuteType::palmExtreme);

    // Played notes, transport stopped.
    auto events = std::make_unique<PlayEventQueue>();
    events->clear();

    NoteOnEvent played;
    played.stringIndex = 5;
    played.sampleOffset = 100;
    events->addNoteOn (played);

    RhythmTransport stopped;
    stopped.isPlaying = false;

    f->applyLiveMutes (*events, stopped);
    CHECK (events->getNoteOn (0).muteType == (int) MuteType::palmExtreme);
}

/*  9: "Grid painting: painting palm-mute on step N in the live overlay
    applies within one bar." Notes played along with the host, the rhythm
    engine not driving. */
LUTHIER_TEST (Muting, paintingTheLiveGridAppliesWithinABar)
{
    auto f = std::make_unique<MuteFixture>();
    f->setMuteSettings (armedNeutral());

    const double samplesPerSixteenth = 0.125 * kSr;   // 120 bpm

    // A note on every sixteenth of one bar, stamped a block at a time.
    auto playBar = [&f, samplesPerSixteenth] (int bar)
    {
        std::array<int, kLiveMuteSteps> types {};

        for (int step = 0; step < kLiveMuteSteps; ++step)
        {
            const double ppq = (double) bar * 4.0 + (double) step * 0.25;

            RhythmTransport t;
            t.bpm = 120.0;
            t.isPlaying = true;
            t.ppqPosition = ppq;

            auto events = std::make_unique<PlayEventQueue>();
            events->clear();

            NoteOnEvent e;
            e.stringIndex = 2;
            e.sampleOffset = 10;   // just after the step's start
            events->addNoteOn (e);

            f->applyLiveMutes (*events, t);
            types[(size_t) step] = events->getNoteOn (0).muteType;
        }

        juce::ignoreUnused (samplesPerSixteenth);
        return types;
    };

    auto before = playBar (0);

    for (int t : before)
        CHECK (t == (int) MuteType::open);

    // Paint step 6 mid-bar; the very next bar has it.
    f->mute.setLiveStep (6, MuteType::palmHeavy);

    const auto after = playBar (1);

    for (int step = 0; step < kLiveMuteSteps; ++step)
        CHECK_MSG (after[(size_t) step] == (step == 6 ? (int) MuteType::palmHeavy : (int) MuteType::open),
                   "step " + juce::String (step) + " came out as " + juce::String (after[(size_t) step]));

    // Disarmed, the live grid leaves played notes alone.
    f->setMuteSettings (MuteSettings {});
    const auto disarmed = playBar (2);
    CHECK (disarmed[6] == (int) MuteType::open);
}

/*  3: fretting-hand style. Rock spread: the spare fingers deaden the strings
    a muted strike does not play. Classical fingertip: they ring on. Through
    the whole engine: an open E chord rings, then a palm-muted strike on the
    top string alone. */
LUTHIER_TEST (Muting, rockSpreadDeadensTheStringsAMutedStrumMisses)
{
    auto lowELevelAfter = [] (FrettingMuteStyle style)
    {
        auto engine = std::make_unique<LuthierEngine>();
        engine->prepare (kSr, kBlock);

        MuteSettings s;
        s.armed = true;
        s.frettingStyle = style;
        engine->setMuteSettings (s);

        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer none;

        for (int string = 0; string < 6; ++string)
        {
            NoteOnEvent e;
            e.stringIndex = string;
            e.fretPosition = 0.0;
            e.pitchHz = engine->getTuningEngine().computeFrequency (string, 0.0, 0.0);
            e.velocity = 0.8;
            engine->triggerNoteNow (e);
        }

        for (int b = 0; b < 10; ++b)
            engine->processBlock (buffer, none);

        NoteOnEvent muted;
        muted.stringIndex = 0;
        muted.pitchHz = engine->getTuningEngine().computeFrequency (0, 0.0, 0.0);
        muted.velocity = 0.8;
        muted.muteType = (int) MuteType::palmHeavy;
        engine->triggerNoteNow (muted);

        for (int b = 0; b < (int) (0.3 * kSr / kBlock); ++b)
            engine->processBlock (buffer, none);

        return engine->getString (5).getLevel();
    };

    const double spread = lowELevelAfter (FrettingMuteStyle::rockSpread);
    const double classical = lowELevelAfter (FrettingMuteStyle::classicalFingertip);

    CHECK_MSG (gainToDb (spread / juce::jmax (1.0e-12, classical)) < -30.0,
               "rock spread left the low E at " + juce::String (gainToDb (spread / juce::jmax (1.0e-12, classical)), 1)
                 + " dB against classical");
    CHECK_MSG (classical > 1.0e-4, "classical fingertip deadened the low E");
}

/*  technique-cascade.md 2-3: muting is additive and can always be added. A
    mute is stamped on any note, whatever its technique, and the grid's
    override reaches it. (The slap's own note path is the slap integration's;
    this checks the stamp it will read.) */
LUTHIER_TEST (Muting, aMuteIsStampedOnAnyTechnique)
{
    auto f = std::make_unique<MuteFixture>();

    auto s = armedNeutral();
    s.masterMode = (int) MuteType::palmHeavy;
    f->setMuteSettings (s);

    auto events = std::make_unique<PlayEventQueue>();
    events->clear();

    for (auto technique : { Technique::Pluck, Technique::HammerOn, Technique::Tap,
                            Technique::PalmMute, Technique::Slide })
    {
        NoteOnEvent e;
        e.technique = technique;
        events->addNoteOn (e);
    }

    RhythmTransport t;
    f->applyLiveMutes (*events, t);

    for (int i = 0; i < events->getNumNoteOns(); ++i)
        CHECK (events->getNoteOn (i).muteType == (int) MuteType::palmHeavy);
}

//==============================================================================
//  Round trips, parameters and the UI
//==============================================================================

/*  9: "Preset save / restore round-trips the mute grid." The pattern's row
    through .luthierpattern JSON; the live grid through a .luthierpreset file
    (its techniques block) and the plugin's saved state. */
LUTHIER_TEST (Muting, theMuteGridsRoundTrip)
{
    auto pattern = quarterDowns();

    MuteStep light;
    light.type = MuteType::palmLight;
    light.pressure = 0.3;
    light.positionMm = 50.0;
    pattern.setMuteStep (0, light);

    MuteStep chuka;
    chuka.type = MuteType::chuka;
    pattern.setMuteStep (8, chuka);

    MuteStep onRest;
    onRest.type = MuteType::ghost;
    pattern.setMuteStep (2, onRest);   // a rest step's mute is the row's too

    const auto json = juce::JSON::toString (pattern.toVar(), false);
    CHECK (json.contains ("\"mute_type\""));
    CHECK (json.contains ("palm_mute_light"));

    const auto back = RhythmPattern::fromVar (juce::JSON::parse (json));
    CHECK (back.getMuteStep (0).type == MuteType::palmLight);
    CHECK_NEAR (back.getMuteStep (0).pressure, 0.3, 1.0e-9);
    CHECK_NEAR (back.getMuteStep (0).positionMm, 50.0, 1.0e-9);
    CHECK (back.getMuteStep (8).type == MuteType::chuka);
    CHECK (back.getMuteStep (2).type == MuteType::ghost);
    CHECK (back.getMuteStep (4).type == MuteType::open);
    CHECK (back.getMuteStep (4).pressure < 0.0);

    // A pattern with no mutes writes no mute fields: files as they always were.
    CHECK (! juce::JSON::toString (quarterDowns().toVar(), false).contains ("mute"));

    // The live grid and the parameters through a preset file.
    auto source = std::make_unique<LuthierAudioProcessor>();
    source->prepareToPlay (kSr, kBlock);

    for (int i = 0; i < kLiveMuteSteps; ++i)
        source->getEngine().getTechniqueLayer().mute.setLiveStep (i, muteTypeFromLetter (getMuteGridPreset (1).cells[i]));

    setPlain (*source, ParamIDs::mutePalmPressure, 0.8f);
    setPlain (*source, ParamIDs::muteArmed, 1.0f);

    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier_mute_roundtrip.luthierpreset");
    file.replaceWithText (juce::JSON::toString (source->getPresetManager().toVar ("Mute Round Trip"), false));

    auto restored = std::make_unique<LuthierAudioProcessor>();
    restored->prepareToPlay (kSr, kBlock);
    CHECK (restored->getPresetManager().loadPreset (file));

    for (int i = 0; i < kLiveMuteSteps; ++i)
        CHECK (restored->getEngine().getTechniqueLayer().mute.getLiveStep (i)
               == source->getEngine().getTechniqueLayer().mute.getLiveStep (i));

    CHECK_NEAR (plainOf (*restored, ParamIDs::mutePalmPressure), 0.8f, 1.0e-3f);
    CHECK (plainOf (*restored, ParamIDs::muteArmed) > 0.5f);
    file.deleteFile();

    // And through the plugin's saved state, the pattern's row with it.
    auto pattern2 = quarterDowns();
    pattern2.setMuteStep (12, light);
    source->getEngine().getRhythmEngine().setPattern (pattern2);

    juce::MemoryBlock state;
    source->getStateInformation (state);

    auto again = std::make_unique<LuthierAudioProcessor>();
    again->prepareToPlay (kSr, kBlock);
    again->setStateInformation (state.getData(), (int) state.getSize());

    CHECK (again->getEngine().getTechniqueLayer().mute.getLiveStep (2) == muteTypeFromLetter (getMuteGridPreset (1).cells[2]));
    CHECK (again->getEngine().getRhythmEngine().getPattern().getMuteStep (12).type == MuteType::palmLight);

    // engine-technique-layer 6: a preset without the block loads with a clear grid.
    auto defaults = std::make_unique<LuthierAudioProcessor>();
    defaults->prepareToPlay (kSr, kBlock);
    defaults->getPresetManager().resetToDefaults();

    for (int i = 0; i < kLiveMuteSteps; ++i)
        CHECK (defaults->getEngine().getTechniqueLayer().mute.getLiveStep (i) == MuteType::open);
}

/*  3 / engine-technique-layer 5: the parameters, with their defaults, reach
    the mute engine through the bridge. */
LUTHIER_TEST (Muting, parametersReachTheEngine)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    CHECK_NEAR (plainOf (*processor, ParamIDs::muteArmed), 0.0f, 1.0e-6f);
    CHECK_NEAR (plainOf (*processor, ParamIDs::muteMasterMode), 0.0f, 1.0e-6f);
    CHECK_NEAR (plainOf (*processor, ParamIDs::mutePalmPosition), 35.0f, 0.01f);
    CHECK_NEAR (plainOf (*processor, ParamIDs::mutePalmPressure), 0.5f, 1.0e-4f);
    CHECK_NEAR (plainOf (*processor, ParamIDs::muteFrettingStyle), 0.0f, 1.0e-6f);
    CHECK_NEAR (plainOf (*processor, ParamIDs::muteChukaSource), 1.0f, 1.0e-6f);
    CHECK_NEAR (plainOf (*processor, ParamIDs::muteHumanise), 0.0f, 1.0e-6f);
    CHECK_NEAR (plainOf (*processor, ParamIDs::muteGhostVelocity), 0.4f, 1.0e-4f);

    setPlain (*processor, ParamIDs::muteArmed, 1.0f);
    setPlain (*processor, ParamIDs::muteMasterMode, (float) ((int) MuteType::palmHeavy + 1));
    setPlain (*processor, ParamIDs::mutePalmPosition, 60.0f);
    setPlain (*processor, ParamIDs::mutePalmPressure, 0.7f);
    setPlain (*processor, ParamIDs::muteFrettingStyle, 1.0f);
    setPlain (*processor, ParamIDs::muteChukaSource, 0.0f);
    setPlain (*processor, ParamIDs::muteHumanise, 0.25f);
    setPlain (*processor, ParamIDs::muteGhostVelocity, 0.6f);

    processor->getParameterBridge().applyAllNow();

    const auto s = processor->getEngine().getTechniqueLayer().mute.getSettings();

    CHECK (s.armed);
    CHECK (s.masterMode == (int) MuteType::palmHeavy);
    CHECK_NEAR (s.palmPositionMm, 60.0, 0.01);
    CHECK_NEAR (s.palmPressure, 0.7, 1.0e-4);
    CHECK (s.frettingStyle == FrettingMuteStyle::classicalFingertip);
    CHECK (s.chukaSource == ChukaSource::patternOnly);
    CHECK_NEAR (s.humanise, 0.25, 1.0e-4);
    CHECK_NEAR (s.ghostVelocity, 0.6, 1.0e-4);
}

/*  7 / gui-techniques-updates 1 and 6: the MUTE sub-tab edits what it shows,
    the RHYTHM tab carries the pattern's Mute Row, and the presets write the
    live grid - each paint an undoable mute-grid-paint. */
LUTHIER_TEST (Muting, theMuteControlsDriveTheModel)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    auto& mute = processor->getEngine().getTechniqueLayer().mute;

    MuteGroup group (*processor);
    group.setSize (360, MuteGroup::preferredHeight);

    // Arm.
    group.getArmToggle().getButton().setToggleState (true, juce::sendNotificationSync);
    CHECK (plainOf (*processor, ParamIDs::muteArmed) > 0.5f);

    // Master mode.
    group.getMasterModeControl().getComboBox().setSelectedItemIndex ((int) MuteType::ghost + 1,
                                                                     juce::sendNotificationSync);
    CHECK_NEAR (plainOf (*processor, ParamIDs::muteMasterMode), (float) ((int) MuteType::ghost + 1), 1.0e-3f);

    // Paint the live grid with the brush: one undo entry.
    TechniqueUndo::resetMergeWindow();
    const int before = processor->getNumUndoSteps();
    group.getBrushBox().setSelectedId ((int) MuteType::palmExtreme + 1, juce::sendNotificationSync);
    group.getLiveGrid().paintCell (9);
    CHECK (mute.getLiveStep (9) == MuteType::palmExtreme);
    CHECK (processor->getNumUndoSteps() == before + 1);

    processor->undo();
    CHECK (processor->getEngine().getTechniqueLayer().mute.getLiveStep (9) == MuteType::open);

    // 6: a preset writes all sixteen.
    group.getPresetBox().setSelectedId (1, juce::sendNotificationSync);   // Metal Chug 16ths

    for (int i = 0; i < kLiveMuteSteps; ++i)
        CHECK (processor->getEngine().getTechniqueLayer().mute.getLiveStep (i) == MuteType::palmHeavy);

    // The RHYTHM tab's Mute Row edits the pattern.
    auto& rhythm = processor->getEngine().getRhythmEngine();
    rhythm.setPattern (quarterDowns());

    RhythmPanel panel (*processor);
    panel.setSize (400, panel.preferredHeight());

    MuteGridEditor* muteRow = nullptr;

    for (auto* child : panel.getChildren())
        if (auto* row = dynamic_cast<MuteGridEditor*> (child))
            muteRow = row;

    CHECK_MSG (muteRow != nullptr, "the RHYTHM panel has no Mute Row");

    if (muteRow != nullptr)
    {
        CHECK (muteRow->getNumCells != nullptr && muteRow->getNumCells() == 16);

        // All open for a pattern without mute_type.
        for (int i = 0; i < 16; ++i)
            CHECK (muteRow->getCell (i) == MuteType::open);

        muteRow->setBrush (MuteType::fretMute);
        muteRow->paintCell (4);

        CHECK (rhythm.getPattern().getMuteStep (4).type == MuteType::fretMute);
        CHECK (muteRow->getCell (4) == MuteType::fretMute);
    }

    // The MUTE sub-tab of TECHNIQUES holds the group.
    TechniquesPanel techniques (*processor);
    techniques.setSize (700, 600);
    techniques.showTechnique (TechniqueSlot::mute);
    CHECK (dynamic_cast<MutePage*> (techniques.getPage ((int) TechniqueSlot::mute)) != nullptr);
}

/*  7: Easy mode's Mute button: Off, Light, Heavy, Extreme, and back to Off. */
LUTHIER_TEST (Muting, theEasyMuteButtonCyclesFourWays)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    EasyMuteButton button (*processor);
    CHECK (button.getState() == 0);

    const MuteType expected[] = { MuteType::palmLight, MuteType::palmHeavy, MuteType::palmExtreme };

    for (int i = 0; i < 3; ++i)
    {
        button.onClick();
        CHECK (button.getState() == i + 1);
        CHECK (plainOf (*processor, ParamIDs::muteArmed) > 0.5f);
        CHECK_NEAR (plainOf (*processor, ParamIDs::muteMasterMode), (float) ((int) expected[i] + 1), 1.0e-3f);
    }

    button.onClick();
    CHECK (button.getState() == 0);
    CHECK (plainOf (*processor, ParamIDs::muteArmed) < 0.5f);

    // It is on Easy mode's playing strip, in the pill row.
    EasyPanel easy (*processor);
    CHECK (easy.getTechniquePills().getMuteButton().isVisible());
    CHECK (easy.getTechniquePills().getParentComponent() == &easy);
}

/*  1: a fret mute rings, then stops. Through the whole engine: armed with the
    master mode on Fret Mute, a played note is pitched for its first tens of
    milliseconds and gone long before an open one. */
LUTHIER_TEST (Muting, aFretMuteRingsThenStops)
{
    /*  The struck string's own level, early and late. Not the output: the
        body, the room and the other open strings (ringing in sympathy through
        the bridge) carry on after the finger lets go, as they would. Read at
        half a second, because the level follower itself releases over 60 ms. */
    auto levels = [] (bool fretMute, double& early, double& late)
    {
        auto engine = std::make_unique<LuthierEngine>();
        engine->prepare (kSr, kBlock);
        engine->setAmpBuzzAmount (0.0);

        MuteSettings s;

        if (fretMute)
        {
            s.armed = true;
            s.masterMode = (int) MuteType::fretMute;
        }

        engine->setMuteSettings (s);

        juce::AudioBuffer<float> buffer (2, kBlock);
        int string = -1;

        for (int b = 0; b < (int) (0.5 * kSr / kBlock); ++b)
        {
            juce::MidiBuffer midi;

            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 45, 0.8f), 0);

            buffer.clear();
            engine->processBlock (buffer, midi);

            for (int i = 0; i < engine->getNumStrings() && string < 0; ++i)
                if (engine->getString (i).hasSounded())
                    string = i;

            const double t = (double) (b + 1) * kBlock / kSr;

            if (string >= 0 && t >= 0.040 && t < 0.040 + (double) kBlock / kSr)
                early = engine->getString (string).getLevel();
        }

        late = string >= 0 ? engine->getString (string).getLevel() : 0.0;
    };

    double openEarly = 0.0, openLate = 0.0, muteEarly = 0.0, muteLate = 0.0;
    levels (false, openEarly, openLate);
    levels (true, muteEarly, muteLate);

    const double earlyDb = gainToDb (muteEarly / juce::jmax (1.0e-12, openEarly));
    const double lateDb = gainToDb (muteLate / juce::jmax (1.0e-12, openLate));

    CHECK_MSG (earlyDb > -6.0, "the fret mute did not ring first: " + juce::String (earlyDb, 1) + " dB");
    CHECK_MSG (lateDb < -30.0, "the fret mute did not stop: " + juce::String (lateDb, 1) + " dB");
}

/*  9: "Cascade with slap: slap event carries its own mute_type; grid override
    propagates correctly." A slapped note (velocity zone) under a palm-heavy
    master mode is stamped and damped like any other strike: slap + palm mute
    is standard funk (5, technique-cascade.md 2). */
LUTHIER_TEST (Muting, aSlappedNoteCarriesItsMute)
{
    auto levelAfter = [] (bool muted)
    {
        auto engine = std::make_unique<LuthierEngine>();
        engine->prepare (kSr, kBlock);
        engine->setGuitarType (GuitarType::PrecisionBass);

        SlapSettings slap;
        slap.armed = true;
        slap.trigger = TriggerSource::velocityZone;
        slap.velocityZone = 100;
        engine->setSlapSettings (slap);

        MuteSettings s;
        s.armed = muted;
        s.masterMode = muted ? (int) MuteType::palmHeavy : -1;
        engine->setMuteSettings (s);

        juce::AudioBuffer<float> buffer (2, kBlock);
        double level = 0.0;
        int string = -1;

        for (int b = 0; b < (int) (0.25 * kSr / kBlock); ++b)
        {
            juce::MidiBuffer midi;

            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 115), 0);

            engine->processBlock (buffer, midi);

            if (string < 0)
                for (int i = 0; i < engine->getNumStrings(); ++i)
                    if (engine->getString (i).hasSounded())
                        string = i;
        }

        if (string >= 0)
            level = engine->getString (string).getLevel();

        return level;
    };

    const double open = levelAfter (false);
    const double muted = levelAfter (true);

    CHECK_MSG (open > 1.0e-4, "the slapped note did not sound");
    CHECK_MSG (gainToDb (muted / juce::jmax (1.0e-12, open)) < -20.0,
               "palm-muted slap is only " + juce::String (gainToDb (muted / juce::jmax (1.0e-12, open)), 1) + " dB under the open one");
}
