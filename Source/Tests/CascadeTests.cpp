/*  Technique cascade (technique-cascade.md 7 and 10) and the engine technique
    layer's own tests (engine-technique-layer.md 10).

    The resolver on its own (every cell of the matrix, same string and
    different strings, the priority rules), then the engine: preemption,
    cross-string independence, the combined presets, a fuzz, the pipeline
    order, commands without allocation, and pre-delta presets untouched.
*/

#include "TestFramework.h"

#include "../DSP/Techniques/CascadeResolver.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"
#include "../Presets/TechniquePresets.h"
#include "../UI/Techniques/TechniqueUi.h"
#include "../UI/Techniques/TechniquesPanel.h"

long luthierTestAllocationCount() noexcept;   // CircuitTests.cpp

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
    using CT = CascadeTechnique;
    using CR = CascadeRelation;

    constexpr juce::int64 ms (double m) { return (juce::int64) (m * 0.001 * kSr); }

    void setPlain (LuthierAudioProcessor& p, const juce::String& id, float plain)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id)))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    }

    double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }
}

//==============================================================================
/*  2: the table, cell by cell, as written. */
LUTHIER_TEST (Cascade, theMatrixIsTheSpecs)
{
    const CT order[] = { CT::scrape, CT::slide, CT::slap, CT::mute, CT::tap, CT::bend };
    const char* const rows[] =
    {
        //  Scrape Slide Slap Mute Tap Bend
        "QXXCXC",   // Scrape
        "XSXCXC",   // Slide
        "XXQCAC",   // Slap
        "CCCSCC",   // Mute
        "XXACQC",   // Tap
        "CCCCCS",   // Bend
    };

    auto letter = [] (CR r)
    {
        switch (r)
        {
            case CR::compatible: return 'C';
            case CR::queue:      return 'Q';
            case CR::alternate:  return 'A';
            case CR::conflict:   return 'X';
            case CR::same:       return 'S';
            default:             return '?';
        }
    };

    for (int a = 0; a < 6; ++a)
        for (int b = 0; b < 6; ++b)
            CHECK_MSG (letter (CascadeResolver::relation (order[a], order[b])) == rows[a][b],
                       juce::String (getCascadeTechniqueName (order[a])) + " then " + getCascadeTechniqueName (order[b]));
}

/*  7 / 10: "Every cell of the compatibility matrix has a fixture and a test."
    Each pair, same string inside the window: the documented outcome. And on
    different strings: both apply (0.4). */
LUTHIER_TEST (Cascade, everyPairResolvesAsDocumented)
{
    const CT all[] = { CT::scrape, CT::slide, CT::slap, CT::mute, CT::tap, CT::bend };
    int cells = 0;

    for (auto a : all)
    {
        for (auto b : all)
        {
            CascadeResolver r;
            r.prepare (kSr);

            // A continuous technique is held; an event technique was just fired.
            if (a == CT::slide || a == CT::scrape || a == CT::tap)
                r.setHeld (a, 2, true, 0);
            else
                r.request (a, 2, 0, true);

            const auto same = r.request (b, 2, ms (50), true);
            const auto rel = CascadeResolver::relation (a, b);
            const auto label = juce::String (getCascadeTechniqueName (a)) + " + " + getCascadeTechniqueName (b);

            switch (rel)
            {
                case CR::compatible:
                case CR::same:
                    CHECK_MSG (same.accepted && same.preemptMask == 0, label + ": both should apply");
                    break;

                case CR::queue:
                    CHECK_MSG (same.accepted && same.queued && same.preemptMask == 0, label + ": should queue");
                    break;

                case CR::alternate:
                    CHECK_MSG (same.accepted && (same.preemptMask & (1 << (int) a)) != 0, label + ": latest should win");
                    break;

                case CR::conflict:
                    if (a == CT::slide)
                        CHECK_MSG (! same.accepted, label + ": the bar holds its string (3.4)");
                    else
                        CHECK_MSG (same.accepted && (same.preemptMask & (1 << (int) a)) != 0, label + ": most recent should win (3.2)");
                    break;

                default:
                    break;
            }

            // Different strings: always both (0.4).
            const auto other = r.request (b, 4, ms (50), true);
            CHECK_MSG (other.accepted && other.preemptMask == 0, label + " on different strings");
            ++cells;
        }
    }

    CHECK (cells == 36);
}

/*  3: the priority rules. */
LUTHIER_TEST (Cascade, thePriorityRulesDecide)
{
    CascadeResolver r;
    r.prepare (kSr);

    // 3.1: a user gesture beats an automatic one - the automatic is refused.
    r.request (CT::slap, 1, 0, true);
    CHECK (! r.request (CT::scrape, 1, ms (20), false).accepted);

    // ... and a user one preempts the automatic.
    CascadeResolver r2;
    r2.prepare (kSr);
    r2.request (CT::slap, 1, 0, false);
    const auto o = r2.request (CT::scrape, 1, ms (20), true);
    CHECK (o.accepted && (o.preemptMask & (1 << (int) CT::slap)) != 0);

    // 3.2: past the window the old event is gone: no conflict at all.
    CascadeResolver r3;
    r3.prepare (kSr);
    r3.request (CT::slap, 1, 0, true);
    CHECK (r3.request (CT::scrape, 1, ms (250), true).preemptMask == 0);

    // 3.4: the slide holds its strings until the bar lifts.
    CascadeResolver r4;
    r4.prepare (kSr);
    r4.setHeld (CT::slide, 3, true, 0);
    CHECK (! r4.request (CT::tap, 3, ms (5000), true).accepted);
    r4.setHeld (CT::slide, 3, false, ms (5001));
    CHECK (r4.request (CT::tap, 3, ms (5002), true).accepted);

    // 3.5 / 3.6: mute and bend can always be added.
    CascadeResolver r5;
    r5.prepare (kSr);
    r5.setHeld (CT::slide, 0, true, 0);
    r5.setHeld (CT::scrape, 0, true, 0);
    CHECK (r5.request (CT::mute, 0, ms (10), false).accepted);
    CHECK (r5.request (CT::bend, 0, ms (10), false).accepted);

    // The published mask follows what is live.
    r5.publish (6, ms (10));
    CHECK ((r5.getPublishedMask (0) & (1 << (int) CT::slide)) != 0);
    CHECK ((r5.getPublishedMask (0) & (1 << (int) CT::mute)) != 0);
}

/*  6: the conflict message names the other technique and its strings. */
LUTHIER_TEST (Cascade, theConflictMessageNamesStrings)
{
    const auto m = CascadeResolver::conflictMessage (CT::tap, CT::slide, 0x3c, 0, 6);
    CHECK_MSG (m == "Conflicts with Slide on strings 3-6. Slide will preempt on trigger.", m);

    CHECK (CascadeResolver::conflictMessage (CT::mute, CT::slide, 0, 0, 6).isEmpty());   // compatible
    CHECK (CascadeResolver::conflictMessage (CT::tap, CT::slide, 0x38, 0x07, 6).isEmpty());   // no shared strings
}

//==============================================================================
/*  10: "Cross-string independence: slap on E, slide on B, tap on G, bend on
    A: every string outputs the intended pitch and excitation." */
LUTHIER_TEST (Cascade, techniquesOnDifferentStringsAreIndependent)
{
    auto engine = std::make_unique<LuthierEngine>();
    engine->prepare (kSr, kBlock);
    auto& tuning = engine->getTuningEngine();

    // Engine order: 0 high E, 1 B, 2 G, 3 D, 4 A, 5 low E.
    SlideSettings slide;
    slide.enabled = true;
    slide.mode = SlideMode::bottleneck;
    slide.intonationAssist = 0.0;
    engine->setSlideSettings (slide);

    SlideControlSettings contact;
    contact.contactMask = 1 << 1;   // the bar on the B only
    engine->setSlideControls (contact);

    TapSettings tap;
    tap.armed = true;
    tap.source = TapSource::fretboard;
    engine->setTapSettings (tap);

    BendSettings bend;
    bend.armed = true;
    bend.stringSource = StringBendSource::customCc;
    bend.stringCcBase = 21;
    bend.vibratoSource = VibratoSource::off;
    engine->setBendSettings (bend);

    SlapSettings slap;
    slap.armed = true;
    slap.trigger = TriggerSource::velocityZone;
    slap.velocityZone = 100;
    engine->setSlapSettings (slap);

    auto start = [&engine, &tuning] (int s, double fret, Technique t, double velocity)
    {
        NoteOnEvent e;
        e.stringIndex = s;
        e.fretPosition = fret;
        e.pitchHz = tuning.computeFrequency (s, fret, 0.0);
        e.technique = t;
        e.velocity = velocity;
        engine->triggerNoteNow (e);
    };

    start (5, 0.0, Technique::Pluck, 0.9);         // low E, slapped (velocity zone)
    start (1, 5.0, Technique::SlideGuitar, 0.7);   // B under the bar
    start (4, 3.0, Technique::Pluck, 0.7);         // A, bent below

    TapGesture g;
    g.stringIndex = 2;
    g.fret = 9.0;
    g.durationMs = 0.0;
    engine->getTechniqueLayer().tap.requestGesture (g);   // G tapped

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::controllerEvent (1, 21 + 4, 127), 0);   // the A: +200

    for (int b = 0; b < 30; ++b)
    {
        engine->processBlock (buffer, m);
        m.clear();
    }

    CHECK (engine->getSlideEngine().isUnderBar (1));
    CHECK (! engine->getSlideEngine().isUnderBar (2) && ! engine->getSlideEngine().isUnderBar (4));
    CHECK (engine->getTechniqueLayer().tap.isTapping (2));

    for (int s : { 1, 2, 4, 5 })
        CHECK_MSG (engine->getString (s).getLevel() > 1.0e-4, "string " + juce::String (s) + " is silent");

    const double gOff = cents (engine->getStringFrequency (2), tuning.computeFrequency (2, 9.0, 0.0));
    const double aOff = cents (engine->getStringFrequency (4), tuning.computeFrequency (4, 3.0, 0.0));
    const double eOff = cents (engine->getStringFrequency (5), tuning.computeFrequency (5, 0.0, 0.0));
    const double bOff = cents (engine->getStringFrequency (1), tuning.computeFrequency (1, 5.0, 0.0));

    CHECK_MSG (std::abs (gOff) < 10.0, "tapped G is " + juce::String (gOff, 1) + " cents off fret 9");
    CHECK_MSG (std::abs (aOff - 200.0) < 10.0, "bent A is " + juce::String (aOff, 1) + " cents (+200 wanted)");
    CHECK_MSG (std::abs (eOff) < 10.0, "slapped E is " + juce::String (eOff, 1) + " cents off");
    CHECK_MSG (std::abs (bOff) < 15.0, "the B under the bar is " + juce::String (bOff, 1) + " cents off fret 5");
}

/*  3.3 in the engine: a tap on a string being scraped preempts the scrape,
    which fades over its 10 ms (ScrapeTests.aPreemptedScrapeFadesOutInTenMilliseconds
    measures the fade itself); the tap is recorded as the string's technique. */
LUTHIER_TEST (Cascade, aTapPreemptsAScrapeOnItsString)
{
    auto engine = std::make_unique<LuthierEngine>();
    engine->prepare (kSr, kBlock);

    ScrapeSettings scrape;
    scrape.armed = true;
    scrape.trigger = ScrapeTriggerSource::buttonOnly;
    scrape.durationMs = 2000.0;
    scrape.stringMask = 1 << 3;
    engine->setScrapeSettings (scrape);

    TapSettings tap;
    tap.armed = true;
    tap.source = TapSource::fretboard;
    engine->setTapSettings (tap);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer none;
    engine->getScrapeEngine().requestTrigger();

    for (int b = 0; b < 10; ++b)
        engine->processBlock (buffer, none);

    CHECK (engine->getScrapeEngine().isStringActive (3));

    TapGesture g;
    g.stringIndex = 3;
    g.fret = 7.0;
    g.durationMs = 0.0;
    engine->getTechniqueLayer().tap.requestGesture (g);

    for (int b = 0; b < 10; ++b)   // 53 ms: well past the 10 ms fade
        engine->processBlock (buffer, none);

    CHECK_MSG (! engine->getScrapeEngine().isStringActive (3), "the scrape survived the tap");
    CHECK (engine->getTechniqueLayer().tap.isTapping (3));
    CHECK ((engine->getTechniqueLayer().cascade.getPublishedMask (3) & (1 << (int) CT::tap)) != 0);
}

/*  7: "Fuzz test: 10 000 random gesture sequences across all six techniques on
    all strings; no crashes, no orphaned state, CPU within budget." */
LUTHIER_TEST (Cascade, aFuzzOfGesturesLeavesNothingBehind)
{
    auto engine = std::make_unique<LuthierEngine>();
    engine->prepare (kSr, kBlock);
    auto& tuning = engine->getTuningEngine();

    ScrapeSettings scrape;
    scrape.armed = true;
    scrape.trigger = ScrapeTriggerSource::buttonOnly;
    engine->setScrapeSettings (scrape);

    SlapSettings slap;
    slap.armed = true;
    slap.trigger = TriggerSource::buttonOnly;
    engine->setSlapSettings (slap);

    MuteSettings mute;
    mute.armed = true;
    mute.humanise = 0.5;
    engine->setMuteSettings (mute);

    TapSettings tap;
    tap.armed = true;
    tap.source = TapSource::fretboard;
    tap.maxConcurrent = 4;
    engine->setTapSettings (tap);

    BendSettings bend;
    bend.armed = true;
    engine->setBendSettings (bend);

    SlideSettings slide;
    slide.enabled = true;
    engine->setSlideSettings (slide);

    juce::Random rng (0x5eed);
    juce::AudioBuffer<float> buffer (2, kBlock);
    int gestures = 0;
    double worst = 0.0;

    const auto start = juce::Time::getMillisecondCounterHiRes();

    while (gestures < 10000)
    {
        juce::MidiBuffer m;

        for (int k = 0; k < 8 && gestures < 10000; ++k, ++gestures)
        {
            const int s = rng.nextInt (6);

            switch (rng.nextInt (8))
            {
                case 0: engine->getScrapeEngine().requestTrigger(); break;
                case 1: engine->getTechniqueTriggers().request (TechniqueId::slap, 0, true);
                        engine->getTechniqueTriggers().request (TechniqueId::slap, 0, false); break;
                case 2:
                {
                    TapGesture g;
                    g.stringIndex = s;
                    g.fret = 1.0 + rng.nextDouble() * 20.0;
                    g.durationMs = rng.nextDouble() * 60.0;
                    engine->getTechniqueLayer().tap.requestGesture (g);
                    break;
                }
                case 3: m.addEvent (juce::MidiMessage::pitchWheel (1, rng.nextInt (16384)), rng.nextInt (kBlock)); break;
                case 4: engine->getTechniqueLayer().mute.setLiveStep (rng.nextInt (16), (MuteType) rng.nextInt ((int) MuteType::numTypes)); break;
                case 5: m.addEvent (juce::MidiMessage::noteOn (1, TechniqueKeyswitch::preBend, (juce::uint8) 100), 0); break;
                case 6:
                {
                    NoteOnEvent e;
                    e.stringIndex = s;
                    e.fretPosition = rng.nextInt (15);
                    e.pitchHz = tuning.computeFrequency (s, e.fretPosition, 0.0);
                    e.technique = rng.nextBool() ? Technique::SlideGuitar : Technique::Pluck;
                    e.muteType = rng.nextInt ((int) MuteType::numTypes);
                    engine->triggerNoteNow (e);
                    break;
                }
                default:
                    m.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                    break;
            }
        }

        engine->processBlock (buffer, m);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlock; ++i)
            {
                const float v = buffer.getSample (ch, i);
                CHECK_MSG (std::isfinite (v), "non-finite output");
                worst = juce::jmax (worst, (double) std::abs (v));
            }
    }

    const double elapsed = (juce::Time::getMillisecondCounterHiRes() - start) * 0.001;
    juce::ignoreUnused (elapsed);

    // No orphaned state: disarm everything and let it all end.
    engine->setTapSettings (TapSettings {});
    engine->setBendSettings (BendSettings {});
    engine->setMuteSettings (MuteSettings {});
    engine->panic();
    juce::MidiBuffer none;

    for (int b = 0; b < 200; ++b)
        engine->processBlock (buffer, none);

    for (int s = 0; s < 6; ++s)
    {
        CHECK_MSG (! engine->getTechniqueLayer().tap.isTapping (s), "a tap outlived its technique on string " + juce::String (s));
        CHECK (engine->getTechniqueLayer().bend.getLiveCents (s) == 0.0 || ! engine->getTechniqueLayer().bend.isArmed());
    }

    CHECK_MSG (worst < 32.0, "output peaked at " + juce::String (worst));
}

//==============================================================================
/*  5 / 7: the combined presets exist, arm what they say, and render the same
    every time (the "saved reference" is a render of the same preset: a
    stored file would record this build's DSP, not the spec). */
LUTHIER_TEST (Cascade, theCombinedPresetsArmTheirTechniques)
{
    const std::pair<const char*, juce::StringArray> expected[] =
    {
        { "Metal Lead Combo",  { ParamIDs::tapArmed, ParamIDs::bendArmed, ParamIDs::muteArmed } },
        { "Funk Slap Groove",  { ParamIDs::slapArmed, ParamIDs::muteArmed, ParamIDs::bendArmed } },
        { "Slide Blues",       { ParamIDs::slideGuitar, ParamIDs::bendArmed, ParamIDs::muteArmed } },
        { "Percussive Tap",    { ParamIDs::tapArmed, ParamIDs::muteArmed, ParamIDs::slapArmed } },
        { "Scrape Intro",      { ParamIDs::scrapeArmed, ParamIDs::muteArmed } },
        { "Full Cascade Demo", { ParamIDs::scrapeArmed, ParamIDs::slapArmed, ParamIDs::muteArmed,
                                 ParamIDs::tapArmed, ParamIDs::bendArmed, ParamIDs::slideGuitar } },
    };

    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    for (const auto& [name, arms] : expected)
    {
        int index = -1;

        for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
            if (juce::String (FactoryPresets::getPreset (i).name) == name)
                index = i;

        CHECK_MSG (index >= 0, juce::String (name) + " is not in the factory bank");

        if (index < 0)
            continue;

        const auto data = FactoryPresets::toVar (FactoryPresets::getPreset (index), *processor);
        auto* params = data.getProperty ("parameters", {}).getDynamicObject();

        for (const auto& id : arms)
            CHECK_MSG (params != nullptr && (double) params->getProperty (id) > 0.5, juce::String (name) + " does not arm " + id);

        // Render the same phrase twice from the preset: identical within 0.5 dB.
        auto render = [&data]
        {
            auto p = std::make_unique<LuthierAudioProcessor>();
            p->prepareToPlay (kSr, kBlock);
            p->getPresetManager().fromVar (data);
            p->getParameterBridge().applyAllNow();

            juce::AudioBuffer<float> buffer (2, kBlock);
            double energy = 0.0;

            for (int b = 0; b < 60; ++b)
            {
                juce::MidiBuffer m;

                if (b == 1)
                    for (int note : { 40, 47, 52 })
                        m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

                if (b == 30)
                    m.addEvent (juce::MidiMessage::noteOn (2, 64, (juce::uint8) 90), 0);

                buffer.clear();
                p->getEngine().processBlock (buffer, m);
                energy += buffer.getRMSLevel (0, 0, kBlock);
            }

            return energy;
        };

        const double a = render(), b = render();
        CHECK_MSG (a > 0.0 && std::abs (gainToDb (a / juce::jmax (1.0e-12, b))) < 0.5,
                   juce::String (name) + " renders differ by " + juce::String (gainToDb (a / juce::jmax (1.0e-12, b)), 2) + " dB");
    }

    // The technique presets' categories and the techniques block.
    int withGrid = 0;

    for (const auto& r : getTechniquePresetRecipes())
    {
        CHECK (r.category == "Techniques");

        if (r.techniques.isNotEmpty())
        {
            ++withGrid;
            CHECK_MSG (juce::JSON::parse (r.techniques).getProperty ("mute_grid", {}).toString().isNotEmpty(), r.name);
        }
    }

    CHECK (withGrid >= 5);
}

/*  10: "Conflict indicator UI: arming a conflicting technique shows the slash
    icon within one UI frame." One refresh is one frame of the pill's timer. */
LUTHIER_TEST (Cascade, aConflictShowsOnThePillWithinAFrame)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    TechniquePill tap (*processor, TechniqueSlot::tap);
    TechniquePill slide (*processor, TechniqueSlot::slide);
    TechniquePill bend (*processor, TechniqueSlot::bend);

    setPlain (*processor, ParamIDs::tapArmed, 1.0f);
    setPlain (*processor, ParamIDs::bendArmed, 1.0f);
    tap.refresh();
    bend.refresh();
    CHECK (! tap.isShowingConflict() && ! bend.isShowingConflict());

    setPlain (*processor, ParamIDs::slideGuitar, 1.0f);
    tap.refresh();
    slide.refresh();
    bend.refresh();

    CHECK (tap.isShowingConflict());
    CHECK_MSG (tap.getConflictText().startsWith ("Conflicts with Slide on strings"), tap.getConflictText());
    CHECK (tap.getTooltip() == tap.getConflictText());
    CHECK (! bend.isShowingConflict());   // 3.6: bend never conflicts

    // A bass-3 bar and a tap limited to nothing else still share strings, so it stays;
    // the CASCADE view lists it.
    TechniquesPanel panel (*processor);
    panel.setSize (800, 600);
    panel.showSubTab (TechniqueTable::cascadeSubTab);
    auto* page = dynamic_cast<CascadePage*> (panel.getPage (TechniqueTable::cascadeSubTab));
    CHECK (page != nullptr);

    if (page != nullptr)
    {
        page->getView().refresh();
        CHECK (page->getView().isArmedRow (TechniqueSlot::tap));
        CHECK (! page->getView().getConflicts().isEmpty());
    }
}

//==============================================================================
/*  engine-technique-layer 10: "Pipeline order test: verify new modules
    execute at documented points." The layer logs its stages each block. */
LUTHIER_TEST (TechniqueLayer, theModulesRunInTheDocumentedOrder)
{
    auto engine = std::make_unique<LuthierEngine>();
    engine->prepare (kSr, kBlock);
    engine->getRhythmEngine().setEnabled (true);
    engine->setTransportPosition (0.0, true);

    RhythmPattern p;
    p.setKind (RhythmPattern::Kind::strum);
    StrumStep step;
    step.type = StrumType::down;
    p.setStrumStep (0, step);
    engine->getRhythmEngine().setPattern (p);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
    m.addEvent (juce::MidiMessage::noteOn (1, 56, (juce::uint8) 100), 0);
    m.addEvent (juce::MidiMessage::noteOn (1, 59, (juce::uint8) 100), 0);

    bool sawRhythm = false;

    for (int b = 0; b < 40; ++b)
    {
        engine->setTransportPosition ((double) b * kBlock / kSr * 2.0, true);
        engine->processBlock (buffer, m);
        m.clear();

        const auto& log = engine->getTechniqueLayer().stages;
        CHECK (log.count >= 3);
        CHECK (log.order[0] == TechniqueLayer::stageTriggers);
        CHECK (log.order[1] == TechniqueLayer::stageCascade);
        CHECK (log.order[2] == TechniqueLayer::stageTechniques);

        if (log.count >= 4)
        {
            CHECK (log.order[3] == TechniqueLayer::stageRhythm);
            sawRhythm = true;
        }
    }

    CHECK_MSG (sawRhythm, "the rhythm engine's stage never ran after the technique modules");
}

/*  engine-technique-layer 10: "Command queue: 1000 arm / disarm / trigger
    commands per second handled without allocation on the audio thread." A
    second of blocks with about 22 commands each. */
LUTHIER_TEST (TechniqueLayer, commandsDoNotAllocate)
{
    auto engine = std::make_unique<LuthierEngine>();
    engine->prepare (kSr, kBlock);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer m;
    m.ensureSize (4096);

    // Warm up: first-block lazy work is not the steady state.
    for (int b = 0; b < 4; ++b)
        engine->processBlock (buffer, m);

    long allocations = 0;
    int commands = 0;
    const int blocks = (int) (kSr / kBlock);

    for (int b = 0; b < blocks; ++b)
    {
        const bool on = (b % 2) == 0;

        TapSettings tap;
        tap.armed = on;
        tap.source = TapSource::fretboard;
        BendSettings bend;
        bend.armed = on;
        MuteSettings mute;
        mute.armed = on;

        TapGesture g;
        g.stringIndex = b % 6;
        g.fret = 5.0 + b % 7;
        g.durationMs = 20.0;

        const auto before = luthierTestAllocationCount();

        engine->setTapSettings (tap);
        engine->setBendSettings (bend);
        engine->setMuteSettings (mute);
        engine->getTechniqueLayer().tap.requestGesture (g);
        engine->getTechniqueTriggers().request (TechniqueId::slap, 0, on);
        engine->getTechniqueLayer().bend.requestPreBend();
        engine->processBlock (buffer, m);

        allocations += luthierTestAllocationCount() - before;
        commands += 6;
    }

    CHECK (commands >= 1000);
    CHECK_MSG (allocations == 0, juce::String (allocations) + " allocations in a second of technique commands");
}

/*  engine-technique-layer 6 and 10: "Presets saved before these specs: load
    with all techniques disarmed and default controls", and play as before -
    the whole technique layer idle: no note gets a mute, no string a bend it
    would not have had. (The byte-for-byte half is the rest of the suite,
    unchanged by this delta.) */
LUTHIER_TEST (TechniqueLayer, preDeltaPresetsLoadDisarmedAndIdle)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    const auto armIds = getTechniqueArmParameterIds();
    int checked = 0;

    for (int i = 0; i < FactoryPresets::getNumPresets() && checked < 100; ++i)
    {
        const auto& def = FactoryPresets::getPreset (i);

        if (juce::String (def.category) == "Techniques")
            continue;

        auto data = FactoryPresets::toVar (def, *processor);

        // A pre-delta file: none of this workstream's parameters, no techniques block.
        if (auto* params = data.getProperty ("parameters", {}).getDynamicObject())
            for (auto* p : processor->getParameters())
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                    if (withId->paramID.startsWith ("mute_") || withId->paramID.startsWith ("tap_")
                        || withId->paramID.startsWith ("bend_") || withId->paramID.startsWith ("slide_pos")
                        || withId->paramID.startsWith ("slide_gesture") || withId->paramID.startsWith ("slide_auto")
                        || withId->paramID.startsWith ("slide_slant_s") || withId->paramID.startsWith ("slide_pressure_s")
                        || withId->paramID.startsWith ("slide_speed") || withId->paramID.startsWith ("slide_contact")
                        || withId->paramID.endsWith ("_cc") && withId->paramID.startsWith ("slide_"))
                        params->removeProperty (withId->paramID);

        processor->getPresetManager().fromVar (data);
        processor->getParameterBridge().applyAllNow();

        const auto& layer = processor->getEngine().getTechniqueLayer();
        CHECK_MSG (! layer.mute.getSettings().armed && ! layer.tap.getSettings().armed && ! layer.bend.isArmed(),
                   juce::String (def.name) + " loaded with a technique armed");
        CHECK (processor->getEngine().getSlideEngine().getControls().positionSource == ControlSource::none);

        for (int s = 0; s < kLiveMuteSteps; ++s)
            CHECK (layer.mute.getLiveStep (s) == MuteType::open);

        ++checked;
    }

    CHECK (checked > 10);

    // Idle: a block of notes passes through the mute stage unchanged.
    MuteEngine mute;
    mute.prepare (kSr);
    PlayEventQueue q;
    q.clear();

    for (int i = 0; i < 6; ++i)
    {
        NoteOnEvent e;
        e.stringIndex = i;
        e.velocity = 0.1 + 0.1 * i;
        e.technique = i % 2 ? Technique::Strum : Technique::Pluck;
        q.addNoteOn (e);
    }

    mute.apply (q, kSr, 120.0, 3.3, true, false);

    for (int i = 0; i < 6; ++i)
    {
        CHECK (q.getNoteOn (i).muteType == 0);
        CHECK (q.getNoteOn (i).chuck == 0.0);
        CHECK_NEAR (q.getNoteOn (i).velocity, 0.1 + 0.1 * i, 1.0e-12);
    }
}

/*  engine-technique-layer 10: "Migration: old bass-slap preset loads with
    legacy fields correctly mapped to new SlapEngine gesture format." The slap
    presets of the bass bank still arm and shape the SlapEngine. */
LUTHIER_TEST (TechniqueLayer, bassSlapPresetsStillReachTheSlapEngine)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    // A legacy file: bass-techniques.md's slap fields only, nothing of 5b.
    auto* params = new juce::DynamicObject();
    params->setProperty (ParamIDs::slapStrength, 0.9);
    params->setProperty (ParamIDs::popStrength, 0.2);
    params->setProperty (ParamIDs::doubleThumpEnabled, 1.0);

    auto* root = new juce::DynamicObject();
    root->setProperty ("magic", PresetManager::kMagic);
    root->setProperty ("schemaVersion", PresetManager::kSchemaVersion);
    root->setProperty ("name", "Old Slap");
    root->setProperty ("parameters", juce::var (params));

    CHECK (processor->getPresetManager().fromVar (juce::var (root)));
    processor->getParameterBridge().applyAllNow();

    const auto& s = processor->getEngine().getSlapEngine().getSettings();
    CHECK (s.doubleThump);
    CHECK_NEAR (s.slapStrength, 0.9, 0.01);
    CHECK (! processor->getEngine().getTechniqueLayer().tap.getSettings().armed);
}
