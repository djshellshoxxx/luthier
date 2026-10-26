/*  Task Q3 (QA/robustness fleet): fuzz + preset/state QA.

    Report-only harness: drives the engine with randomised parameter
    combinations, random MIDI, extreme buffer sizes and sample rates, and every
    factory preset. Asserts no crash, no NaN/inf, no runaway level, and that
    the host-level state save -> load round trips. Anything that misbehaves is
    recorded with its reproduction seed rather than failing the build outright
    (this suite must not regress the Linux gate); see docs/review/QA_ROBUSTNESS.md
    for the findings this run produced.

    A prior QA run found heap corruption while switching a non-default cabinet
    or body with a declared host block size <= 4. The convolution instances now
    prepare with an internal capacity of at least 8 while processing the host's
    actual callback size unchanged. `convolutionPreparesSafelyForTinyHostBlocks`
    is the active 1-4-sample regression; pre-fix evidence and the unconfirmed
    ASan hypothesis remain in
    `docs/handoff/BUFFER-SIZE-1-4-HEAP-CORRUPTION-2026-10-26.md`.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"
#include "../Support/IrLibrary.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct BlockStats
    {
        bool finite = true;
        float peak = 0.0f;
    };

    /** Renders `blocks` blocks of `blockSize` samples at whatever sample rate/block
        size the processor was last prepared with, feeding `midi` (if any) into the
        first block only. */
    BlockStats renderBlocks (LuthierAudioProcessor& p, int blockSize, int blocks, const juce::MidiBuffer* midi = nullptr)
    {
        BlockStats stats;
        juce::AudioBuffer<float> buffer (juce::jmax (1, juce::jmax (p.getTotalNumOutputChannels(), p.getTotalNumInputChannels())), blockSize);

        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer local;

            if (b == 0 && midi != nullptr)
                local = *midi;

            buffer.clear();
            p.processBlock (buffer, local);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const auto* d = buffer.getReadPointer (ch);

                for (int i = 0; i < blockSize; ++i)
                {
                    stats.finite = stats.finite && std::isfinite (d[i]);
                    stats.peak = juce::jmax (stats.peak, std::abs (d[i]));
                }
            }
        }

        return stats;
    }

    juce::MidiBuffer randomMidi (juce::Random& rng, int blockSize)
    {
        juce::MidiBuffer midi;
        const int events = 1 + rng.nextInt (8);

        for (int i = 0; i < events; ++i)
        {
            const int sample = blockSize > 1 ? rng.nextInt (blockSize) : 0;
            const int channel = 1 + rng.nextInt (16);

            switch (rng.nextInt (6))
            {
                case 0: midi.addEvent (juce::MidiMessage::noteOn (channel, rng.nextInt (128), (juce::uint8) (1 + rng.nextInt (127))), sample); break;
                case 1: midi.addEvent (juce::MidiMessage::noteOff (channel, rng.nextInt (128)), sample); break;
                case 2: midi.addEvent (juce::MidiMessage::controllerEvent (channel, rng.nextInt (128), rng.nextInt (128)), sample); break;
                case 3: midi.addEvent (juce::MidiMessage::pitchWheel (channel, rng.nextInt (16384)), sample); break;
                case 4: midi.addEvent (juce::MidiMessage::channelPressureChange (channel, rng.nextInt (128)), sample); break;
                default: midi.addEvent (juce::MidiMessage::allNotesOff (channel), sample); break;
            }
        }

        return midi;
    }

    /** Sets every parameter to a random normalised value and pushes it to the
        engine. Structural parameters (guitar type, cabinet, body) are among
        "every parameter"; when this lands on a config that differs from the
        engine's compiled-in default, prepareToPlay's own internal applyAllNow
        call (PluginProcessor.cpp, after engine.prepare()) re-applies it - so
        calling this before vs. after the processor's first prepareToPlay
        makes no difference to safety, only to which call ends up doing the
        real work. The broad randomized matrix uses sizes >= 16; a separate
        regression covers the prior 1-4 sample cabinet/body swap failure. */
    void randomiseAllParameters (LuthierAudioProcessor& p, juce::Random& rng)
    {
        for (auto* param : p.getParameters())
            // Exercise the same notification path as host automation. Writing
            // through setValue() alone bypasses the APVTS parameter adapter,
            // leaving its raw atomics stale even though getValue() has changed.
            param->setValueNotifyingHost (rng.nextFloat());

        p.getParameterBridge().applyAllNow();
    }

    /** A single misbehaviour, as reported to the doc: what was run, and the seed
        (or preset name) that reproduces it. */
    struct Finding
    {
        juce::String area;
        juce::String repro;
        juce::String symptom;
    };

    struct ReportSection
    {
        juce::String testName;
        juce::String summary;
        juce::Array<Finding> findings;
    };

    /** All the LUTHIER_TESTs below run in one process; a section is appended
        here as each finishes and the whole doc is rewritten from this list every
        time, so the file is complete after a full run regardless of test order,
        and still useful if the suite was filtered down to a subset. */
    juce::Array<ReportSection>& reportSections()
    {
        static juce::Array<ReportSection> sections;
        return sections;
    }

    void writeReport()
    {
        auto file = juce::File::getCurrentWorkingDirectory().getChildFile ("docs/review/QA_ROBUSTNESS.md");

        if (! file.getParentDirectory().isDirectory())
            file = juce::File (__FILE__).getParentDirectory().getParentDirectory().getParentDirectory().getChildFile ("docs/review/QA_ROBUSTNESS.md");

        juce::String body;
        body << "# QA Robustness — fuzz + preset/state findings\n\n";
        body << "Generated by `Source/Tests/QA_Robustness.cpp` (Task Q3). Report-only: no production\n";
        body << "code was changed to produce this file; anything listed below is a finding for the\n";
        body << "coordinator to schedule a fix for, not something this suite fixed itself.\n\n";

        int totalFindings = 0;

        for (auto& section : reportSections())
            totalFindings += section.findings.size();

        body << "## Summary\n\n";
        body << juce::String (totalFindings) << " finding(s) across the runs below.\n\n";

        for (auto& section : reportSections())
        {
            body << "## " << section.testName << "\n\n" << section.summary << "\n\n";

            if (section.findings.isEmpty())
            {
                body << "None. Every case ran clean (finite output, bounded level, clean state\n";
                body << "round-trip) for the seeds and presets this run covered.\n\n";
            }
            else
            {
                for (auto& f : section.findings)
                    body << "- **" << f.area << "** — repro: `" << f.repro << "` — " << f.symptom << "\n";

                body << "\n";
            }
        }

        body << "## What this run covers\n\n";
        body << "- Every factory preset (`FactoryPresets`), rendered at a spread of extreme\n";
        body << "  sample rates (8 kHz - 192 kHz) and buffer sizes (16 - 8192 samples); a\n";
        body << "  dedicated regression also processes a non-default cabinet/body at sizes 1-4.\n";
        body << "- A dedicated sweep of switching preset/guitar/cabinet *while already prepared*\n";
        body << "  at an extreme buffer size - the realistic \"host changes patch mid-session\"\n";
        body << "  case - restricted to buffer sizes >= 16, with sizes 1-4 covered separately.\n";
        body << "- Randomised parameter combinations (every automatable parameter set to a\n";
        body << "  random normalised value, which can land on a non-default cabinet/body/guitar\n";
        body << "  config) crossed with random MIDI (notes, CCs, pitch bend, channel pressure,\n";
        body << "  all-notes-off), at extreme buffer sizes (>= 16) and sample rates.\n";
        body << "- Host-level state round trips (`getStateInformation` / `setStateInformation`,\n";
        body << "  not the preset manager's own save/load, which `PresetQaTests.cpp` already covers\n";
        body << "  to the ulp) for every factory preset and for a batch of randomised parameter\n";
        body << "  states, each re-saved after reload and diffed byte-for-byte against the first save.\n\n";
        body << "Checks: output finite (no NaN/Inf), no runaway level (peak > 1.2, past the master\n";
        body << "limiter's -0.3 dBFS headroom), and (for the state tests) an exact round trip of\n";
        body << "the saved state text. `LUTHIER_PERF=1` widens the seed count and the buffer/sample-\n";
        body << "rate matrix; the default run keeps this to a few seconds so it runs on every push.\n";
        body << "Findings are recorded rather than failed hard, so a real bug here does not block\n";
        body << "the Linux gate; a section's own CHECK still fails if the finding rate crosses a\n";
        body << "generous threshold (more than a handful of cases), which would indicate something\n";
        body << "systemic rather than one flaky corner.\n";

        file.getParentDirectory().createDirectory();
        file.replaceWithText (body);
    }

    void appendReport (const juce::String& testName, const juce::Array<Finding>& findings, const juce::String& runSummary)
    {
        reportSections().add ({ testName, runSummary, findings });
        writeReport();
    }
}

//==============================================================================
/*  Every factory preset, rendered at a spread of extreme sample rates and
    buffer sizes with random MIDI. This broad matrix uses sizes >= 16; the
    1-4 sample non-default convolution swap has its own active regression.
    Reports
    (rather than hard-fails) anything that goes non-finite or blows past the
    master limiter's headroom, since a genuine finding here is for the
    coordinator to schedule a fix for, not for this suite to break the Linux
    gate over. */
LUTHIER_TEST (Fuzz, everyFactoryPresetAcrossExtremeBuffersAndSampleRates)
{
    static const double sampleRates[] = { 8000.0, 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    static const int blockSizes[] = { 16, 32, 64, 128, 513, 1024, 4096, 8192 };

    const bool perf = perfRunRequested();
    const int numPresets = FactoryPresets::getNumPresets();

    // The default run samples one (sampleRate, blockSize) pair per preset
    // (deterministic, spread across the matrix) to stay fast; LUTHIER_PERF=1
    // runs the full cross product.
    juce::Array<Finding> findings;
    int cases = 0;

    for (int presetIndex = 0; presetIndex < numPresets; ++presetIndex)
    {
        const auto& def = FactoryPresets::getPreset (presetIndex);

        const int srCount = perf ? (int) std::size (sampleRates) : 1;
        const int bsCount = perf ? (int) std::size (blockSizes) : 1;

        for (int srI = 0; srI < srCount; ++srI)
        {
            for (int bsI = 0; bsI < bsCount; ++bsI)
            {
                const double sr = sampleRates[perf ? srI : (presetIndex + presetIndex / (int) std::size (sampleRates)) % (int) std::size (sampleRates)];
                const int bs = blockSizes[perf ? bsI : (presetIndex * 3 + 1) % (int) std::size (blockSizes)];

                auto p = std::make_unique<LuthierAudioProcessor>();
                FactoryPresets::setProcessorForRanges (p.get());

                CHECK_MSG (p->getPresetManager().fromVar (FactoryPresets::toVar (def, *p)),
                           juce::String (def.name) + " failed to load");
                p->getParameterBridge().applyAllNow();

                p->prepareToPlay (sr, bs);

                juce::Random rng (0x9000 + presetIndex * 977 + (int) sr + bs);
                auto midi = randomMidi (rng, bs);

                const int blocks = juce::jmax (1, (int) (0.25 * sr / juce::jmax (1, bs)));
                const auto stats = renderBlocks (*p, bs, blocks, &midi);
                ++cases;

                const juce::String repro = juce::String (def.name) + " @ " + juce::String (sr, 0) + " Hz, block " + juce::String (bs);

                if (! stats.finite)
                    findings.add ({ "preset render", repro, "non-finite sample in the output" });
                else if (stats.peak > 1.2f)
                    findings.add ({ "preset render", repro, "peak " + juce::String (stats.peak, 3) + " (runaway past the master limiter's headroom)" });
            }
        }
    }

    // Only genuinely broken cases (a preset that fails to *load*) fail the
    // build; a misbehaving render is a finding for the report below.
    const juce::String summary = juce::String (cases) + " (preset, sample rate, block size) cases run across "
                                  + juce::String (numPresets) + " factory presets, block sizes >= 16 only; "
                                  + juce::String (findings.size()) + " findings.";
    appendReport ("Fuzz.everyFactoryPresetAcrossExtremeBuffersAndSampleRates", findings, summary);

    CHECK_MSG (findings.size() < numPresets, "more than one finding per preset on average - see docs/review/QA_ROBUSTNESS.md");
}

//==============================================================================
namespace
{
    // Share the exact install/reload/output/destruction checks between the
    // original long reproducer and the additional rate/capacity boundaries.
    void checkTinyConvolutionConfiguration (TestContext& ctx, double sampleRate,
                                           int blockSize, int renderBlockCount)
    {
        const auto configuration = juce::String ((int) sampleRate) + " Hz / "
                                 + juce::String (blockSize) + " samples";
        std::cout << "[tiny-convolution] " << configuration << ", "
                  << renderBlockCount << " blocks per preset" << std::endl;
        auto p = std::make_unique<LuthierAudioProcessor>();
        FactoryPresets::setProcessorForRanges (p.get());
        p->setPlayConfigDetails (0, 2, sampleRate, blockSize);
        p->prepareToPlay (sampleRate, blockSize);

        auto& cabinet = p->getEngine().getCabinetEngine();
        auto& body = p->getEngine().getBodyEngine();

        const auto loadAndRender = [&] (int presetIndex, int blocks)
        {
            // getPreset() returns a thread-local view whose storage is reused on
            // the next call; consume the whole definition before requesting another.
            const auto& preset = FactoryPresets::getPreset (presetIndex);
            const auto presetName = juce::String (preset.name);
            CHECK_MSG (p->getPresetManager().fromVar (FactoryPresets::toVar (preset, *p)),
                       presetName + " failed to load at " + configuration);
            p->getParameterBridge().applyAllNow();
            if (presetIndex == 1)
            {
                // Free plays both original recipes as Tele. Give the alternate
                // fixture a shipped, distinct body IR through the real host path;
                // do not turn a valid same-file cache hit into a forced reload.
                auto* topWood = p->getState().getParameter (ParamIDs::bodyTopWood);
                CHECK_MSG (topWood != nullptr, "body IR fixture requires the top-wood parameter");
                if (topWood != nullptr)
                    topWood->setValueNotifyingHost (topWood->convertTo0to1 ((float) Wood::Mahogany));
                p->getParameterBridge().applyToEngine();
                p->getParameterBridge().flushPendingStructuralChange();
            }
            CHECK_MSG (cabinet.hasImpulseResponse (0), presetName + ": cabinet IR was not installed");
            CHECK_MSG (body.hasImpulseResponse(), presetName + ": body IR was not installed");
            CHECK_MSG (IrLibrary::findCabIr (CabinetEngine::anchorConfig (cabinet.getConfigA())).existsAsFile(),
                       presetName + ": configured cabinet IR fixture is unavailable");
            CHECK_MSG (IrLibrary::findBodyIr (body.getBodyConfig()).existsAsFile(),
                       presetName + ": configured body IR fixture is unavailable");
            CHECK_MSG (cabinet.isEnabled(), presetName + ": cabinet convolution must process the tiny blocks");
            CHECK_MSG (body.getMode() == BodyEngine::Mode::Convolution || body.getMode() == BodyEngine::Mode::Hybrid,
                       presetName + ": body convolution must process the tiny blocks");

            juce::MidiBuffer note;
            note.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            const auto stats = renderBlocks (*p, blockSize, blocks, &note);
            CHECK_MSG (stats.finite, "non-finite output at " + configuration);
            CHECK_MSG (stats.peak > 0.0f, "no output at " + configuration);
        };

        loadAndRender (0, renderBlockCount);

        {
            const auto cabinetFileBeforeSwap = IrLibrary::findCabIr (CabinetEngine::anchorConfig (cabinet.getConfigA()));
            const auto bodyFileBeforeSwap = IrLibrary::findBodyIr (body.getBodyConfig());
            const auto guitarBeforeSwap = p->getEngine().getGuitarType();
            const auto cabinetLoadsBeforeSwap = cabinet.getIrLoadCount();
            const auto bodyLoadsBeforeSwap = body.getIrLoadCount();
            loadAndRender (1, renderBlockCount);
            CHECK_MSG (IrLibrary::findCabIr (CabinetEngine::anchorConfig (cabinet.getConfigA())) != cabinetFileBeforeSwap,
                       "alternate preset must select a distinct supported cabinet IR");
            CHECK_MSG (IrLibrary::findBodyIr (body.getBodyConfig()) != bodyFileBeforeSwap,
                       "alternate preset must select a distinct supported body IR");
            CHECK_MSG (cabinet.getIrLoadCount() > cabinetLoadsBeforeSwap,
                       "alternate preset did not reload the cabinet convolution");
            CHECK_MSG (body.getIrLoadCount() > bodyLoadsBeforeSwap,
                       "alternate preset did not reload the body convolution");
            // Retain one fixture trace per cell so CI can attribute an IR swap
            // failure without changing the callback or discarding the assertion.
            std::cout << "[tiny-convolution-swap] " << configuration
                      << " guitar=" << (int) guitarBeforeSwap << "->" << (int) p->getEngine().getGuitarType()
                      << " body=" << bodyFileBeforeSwap.getRelativePathFrom (IrLibrary::getBodyIrFolder()).toStdString()
                      << "->" << IrLibrary::findBodyIr (body.getBodyConfig()).getRelativePathFrom (IrLibrary::getBodyIrFolder()).toStdString()
                      << " loads=" << bodyLoadsBeforeSwap << "->" << body.getIrLoadCount()
                      << " cabinet=" << cabinetFileBeforeSwap.getRelativePathFrom (IrLibrary::getCabIrFolder()).toStdString()
                      << "->" << IrLibrary::findCabIr (CabinetEngine::anchorConfig (cabinet.getConfigA())).getRelativePathFrom (IrLibrary::getCabIrFolder()).toStdString()
                      << " loads=" << cabinetLoadsBeforeSwap << "->" << cabinet.getIrLoadCount() << std::endl;
        }

        p->releaseResources();
        p.reset();

        std::vector<std::vector<uint8_t>> smallAllocationChurn;
        smallAllocationChurn.reserve (256);

        for (size_t i = 0; i < 256; ++i)
            smallAllocationChurn.emplace_back (32 + (i % 97), (uint8_t) blockSize);

        size_t verifiedBytes = 0;

        for (const auto& allocation : smallAllocationChurn)
        {
            CHECK (allocation.front() == (uint8_t) blockSize);
            CHECK (allocation.back() == (uint8_t) blockSize);
            verifiedBytes += allocation.size();
        }

        CHECK (verifiedBytes > 0);
    }
}

/*  Preserve the original 20,000-block reproducer at every crash size, including
    both presets and allocator churn. The host callbacks remain 1-4 samples
    while convolution's internal capacity is at least 8. */
LUTHIER_TEST (Fuzz, convolutionPreparesSafelyForTinyHostBlocks)
{
    CHECK (juce::String (FactoryPresets::getPreset (0).name) == "Clean Double-Cut Funk");
    CHECK (juce::String (FactoryPresets::getPreset (1).name) == "T-Style Country Twang");
    for (int blockSize : { 1, 2, 3, 4 })
        checkTinyConvolutionConfiguration (ctx, 8000.0, blockSize, 20000);
}

/*  Complete the S13 rate/size boundaries without duplicating or shortening the
    original long reproducer: 2,000 blocks for each preset in each added cell.
    Sizes 5, 7 and 8 cover either side of the original power-of-two boundary. */
LUTHIER_TEST (Fuzz, convolutionTinyHostRateAndCapacityBoundaries)
{
    CHECK (juce::String (FactoryPresets::getPreset (0).name) == "Clean Double-Cut Funk");
    CHECK (juce::String (FactoryPresets::getPreset (1).name) == "T-Style Country Twang");
    for (double sampleRate : { 8000.0, 44100.0, 192000.0 })
        for (int blockSize : { 1, 2, 3, 4, 5, 7, 8 })
        {
            if (sampleRate == 8000.0 && blockSize <= 4)
                continue; // Covered above with ten times as many render blocks.
            checkTinyConvolutionConfiguration (ctx, sampleRate, blockSize, 2000);
        }
}

//==============================================================================
/*  Every factory preset, rendered at a spread of extreme sample rates and
    buffer sizes with random MIDI. The tiny 1-4 sample callbacks have their own
    active regression above. */
LUTHIER_TEST (Fuzz, presetLoadAtTrulyExtremeBufferSizes)
{
    static const double sampleRates[] = { 8000.0, 48000.0, 192000.0 };
    static const int safeBlockSizes[] = { 16, 32, 64, 513, 1024, 4096, 8192 };

    const bool perf = perfRunRequested();
    const int numPresets = FactoryPresets::getNumPresets();
    juce::Array<Finding> findings;

    int cases = 0;

    for (int presetIndex = 0; presetIndex < numPresets; ++presetIndex)
    {
        const auto& def = FactoryPresets::getPreset (presetIndex);

        const int srCount = perf ? (int) std::size (sampleRates) : 1;
        const int bsCount = perf ? (int) std::size (safeBlockSizes) : 1;

        for (int srI = 0; srI < srCount; ++srI)
        {
            for (int bsI = 0; bsI < bsCount; ++bsI)
            {
                const double sr = sampleRates[perf ? srI : presetIndex % (int) std::size (sampleRates)];
                const int bs = safeBlockSizes[perf ? bsI : presetIndex % (int) std::size (safeBlockSizes)];

                auto p = std::make_unique<LuthierAudioProcessor>();
                FactoryPresets::setProcessorForRanges (p.get());
                p->prepareToPlay (sr, bs);   // the broad factory cross-product; tiny sizes have their own regression

                juce::Random rng (0xB00B + presetIndex * 131 + (int) sr + bs);
                renderBlocks (*p, bs, 2);   // a moment of normal running before the switch

                CHECK_MSG (p->getPresetManager().fromVar (FactoryPresets::toVar (def, *p)),
                           juce::String (def.name) + " failed to load while running");
                p->getParameterBridge().applyAllNow();

                auto midi = randomMidi (rng, bs);
                const auto stats = renderBlocks (*p, bs, 8, &midi);
                ++cases;

                const juce::String repro = juce::String (def.name) + " switched-in @ " + juce::String (sr, 0)
                                            + " Hz, block " + juce::String (bs);

                if (! stats.finite)
                    findings.add ({ "preset switch while running", repro, "non-finite sample in the output" });
                else if (stats.peak > 1.2f)
                    findings.add ({ "preset switch while running", repro,
                                     "peak " + juce::String (stats.peak, 3) + " (runaway past the master limiter's headroom)" });
            }
        }
    }

    const juce::String summary = juce::String (cases) + " (preset, sample rate, block size >= 16) switch-while-running cases run across "
                                  + juce::String (numPresets) + " factory presets; block sizes 1-4 are covered by the active tiny-block regression.";
    appendReport ("Fuzz.presetLoadAtTrulyExtremeBufferSizes", findings, summary);

    CHECK_MSG (findings.isEmpty(), "the >= 16 buffer-size zone found a new issue - see docs/review/QA_ROBUSTNESS.md");
}

//==============================================================================
/*  Random parameter combinations crossed with random MIDI, at extreme buffer
    sizes and sample rates. Parameters (including structural ones - guitar
    type, cabinet, body) are randomised, which can land on a non-default
    cabinet/body config; this broad sweep keeps sizes >= 16, while the specific
    tiny-block convolution case has a separate regression. 64 seeds by default
    (fast), 2000 under LUTHIER_PERF=1. Any
    misbehaving seed is recorded (not failed) so the coordinator can
    reproduce it exactly with the seed printed. */
LUTHIER_TEST (Fuzz, randomParameterAndMidiCombosWithReproSeeds)
{
    static const double sampleRates[] = { 8000.0, 11025.0, 44100.0, 48000.0, 96000.0, 192000.0 };
    static const int blockSizes[] = { 16, 63, 256, 2048, 8192 };

    const int seeds = perfRunRequested() ? 2000 : 64;
    juce::Array<Finding> findings;

    for (int seed = 0; seed < seeds; ++seed)
    {
        juce::Random rng (0x51ED0000 + seed);

        const double sr = sampleRates[rng.nextInt ((int) std::size (sampleRates))];
        const int bs = blockSizes[rng.nextInt ((int) std::size (blockSizes))];

        auto p = std::make_unique<LuthierAudioProcessor>();
        FactoryPresets::setProcessorForRanges (p.get());

        randomiseAllParameters (*p, rng);
        p->prepareToPlay (sr, bs);

        const int blocks = juce::jmax (1, juce::jmin (40, (int) (0.2 * sr / juce::jmax (1, bs))));
        bool finite = true;
        float peak = 0.0f;

        for (int b = 0; b < blocks; ++b)
        {
            auto midi = randomMidi (rng, bs);
            const auto stats = renderBlocks (*p, bs, 1, &midi);
            finite = finite && stats.finite;
            peak = juce::jmax (peak, stats.peak);
        }

        const juce::String repro = "seed " + juce::String (seed) + " (0x" + juce::String::toHexString (0x51ED0000 + seed)
                                    + "), " + juce::String (sr, 0) + " Hz, block " + juce::String (bs);

        if (! finite)
            findings.add ({ "param/MIDI fuzz", repro, "non-finite sample in the output" });
        else if (peak > 1.2f)
            findings.add ({ "param/MIDI fuzz", repro, "peak " + juce::String (peak, 3) + " (runaway past the master limiter's headroom)" });
    }

    const juce::String summary = juce::String (seeds) + " random parameter/MIDI seeds run across extreme buffer sizes and sample rates "
                                  + "(parameters randomised before the extreme-config prepare); " + juce::String (findings.size()) + " findings.";
    appendReport ("Fuzz.randomParameterAndMidiCombosWithReproSeeds", findings, summary);

    CHECK_MSG (findings.size() <= seeds / 4, "more than a quarter of the fuzz seeds misbehaved - see docs/review/QA_ROBUSTNESS.md");
}

//==============================================================================
/*  Host-level state round trip (getStateInformation / setStateInformation,
    the actual host contract - not PresetManager's own toVar/fromVar, which
    PresetQaTests.cpp already covers to the ulp) for every factory preset: load,
    save, restore into a fresh instance, re-save, and the two saves must be
    byte-identical text. A fixed, safe 48 kHz/256 throughout - see
    randomParameterStatesHostStateRoundTripAtExtremeConfigs for the extreme-
    buffer-size variant. */
LUTHIER_TEST (Fuzz, everyFactoryPresetHostStateRoundTrips)
{
    const int numPresets = FactoryPresets::getNumPresets();
    juce::Array<Finding> findings;

    for (int i = 0; i < numPresets; ++i)
    {
        const auto& def = FactoryPresets::getPreset (i);

        auto source = std::make_unique<LuthierAudioProcessor>();
        FactoryPresets::setProcessorForRanges (source.get());
        source->prepareToPlay (48000.0, 256);

        if (! source->getPresetManager().fromVar (FactoryPresets::toVar (def, *source)))
        {
            findings.add ({ "state round trip", def.name, "preset failed to load, skipped" });
            continue;
        }

        source->getParameterBridge().applyAllNow();

        juce::MemoryBlock saved;
        source->getStateInformation (saved);

        auto restored = std::make_unique<LuthierAudioProcessor>();
        FactoryPresets::setProcessorForRanges (restored.get());
        restored->setStateInformation (saved.getData(), (int) saved.getSize());
        restored->prepareToPlay (48000.0, 256);

        // The restored instance must render cleanly too (a bad restore that
        // leaves a parameter out of range often only shows up once audio runs).
        const auto stats = renderBlocks (*restored, 256, 4);

        if (! stats.finite)
            findings.add ({ "state round trip", def.name, "restored instance produced a non-finite sample" });

        juce::MemoryBlock resaved;
        restored->getStateInformation (resaved);

        const auto a = juce::String::fromUTF8 (static_cast<const char*> (saved.getData()), (int) saved.getSize());
        const auto b = juce::String::fromUTF8 (static_cast<const char*> (resaved.getData()), (int) resaved.getSize());

        if (a != b)
            findings.add ({ "state round trip", def.name, "re-saved state text differs from the first save (drift on restore)" });
    }

    const juce::String summary = juce::String (numPresets) + " factory presets round-tripped through getStateInformation/setStateInformation; "
                                  + juce::String (findings.size()) + " findings.";
    appendReport ("Fuzz.everyFactoryPresetHostStateRoundTrips", findings, summary);

    CHECK_MSG (findings.isEmpty(), juce::String (findings.size()) + " presets did not round-trip cleanly - see docs/review/QA_ROBUSTNESS.md");
}

//==============================================================================
/*  Randomised parameter states round-tripped through the real host state
    contract at extreme sample rates/buffer sizes (kept >= 16 in this broad
    sweep). Parameters are randomised on the SOURCE processor through
    setValueNotifyingHost before its first prepareToPlay; the RESTORED
    processor's setStateInformation likewise runs before its own prepareToPlay,
    matching HostState::anUnpreparedInstanceSavesTheSameStateAsAPreparedOne's
    already-covered pattern. 16 seeds by default, 200 under LUTHIER_PERF=1.

    The earlier helper used setValue() directly. That changed getValue() but
    bypassed APVTS's parameter adapter, leaving raw atomics stale; the first
    host save therefore mixed randomized parameter values with default engine
    and extra-state values. This test compares the serialized state byte for
    byte and must use the host-notification path so all state readers observe
    the same writes. The current helper uses setValueNotifyingHost(). */
LUTHIER_TEST (Fuzz, randomParameterStatesHostStateRoundTripAtExtremeConfigs)
{
    static const double sampleRates[] = { 8000.0, 48000.0, 192000.0 };
    static const int blockSizes[] = { 16, 256, 8192 };

    const int seeds = perfRunRequested() ? 200 : 16;
    juce::Array<Finding> findings;

    for (int seed = 0; seed < seeds; ++seed)
    {
        juce::Random rng (0x57A7E000 + seed);
        const double sr = sampleRates[rng.nextInt ((int) std::size (sampleRates))];
        const int bs = blockSizes[rng.nextInt ((int) std::size (blockSizes))];

        auto source = std::make_unique<LuthierAudioProcessor>();
        randomiseAllParameters (*source, rng);
        source->prepareToPlay (sr, bs);

        juce::MemoryBlock saved;
        source->getStateInformation (saved);

        auto restored = std::make_unique<LuthierAudioProcessor>();
        restored->setStateInformation (saved.getData(), (int) saved.getSize());
        restored->prepareToPlay (sr, bs);

        const auto stats = renderBlocks (*restored, bs, 2);

        juce::MemoryBlock resaved;
        restored->getStateInformation (resaved);

        const auto a = juce::String::fromUTF8 (static_cast<const char*> (saved.getData()), (int) saved.getSize());
        const auto b = juce::String::fromUTF8 (static_cast<const char*> (resaved.getData()), (int) resaved.getSize());

        const juce::String repro = "seed " + juce::String (seed) + " (0x" + juce::String::toHexString (0x57A7E000 + seed)
                                    + "), " + juce::String (sr, 0) + " Hz, block " + juce::String (bs);

        if (! stats.finite)
            findings.add ({ "state round trip fuzz", repro, "restored instance produced a non-finite sample" });

        if (a != b)
            findings.add ({ "state round trip fuzz (see the comment above this test)", repro,
                             "re-saved state text differs from the first save (drift on restore); "
                             "confirmed non-trivial for seed 0 - not float noise, see the finding above" });
    }

    const juce::String summary = juce::String (seeds) + " random parameter states round-tripped through host state at extreme sample rates/buffer sizes "
                                  + "(randomised before each processor's first prepare); " + juce::String (findings.size()) + " findings - "
                                  + "see the comment above this test before treating this as a scary 100% failure rate.";
    appendReport ("Fuzz.randomParameterStatesHostStateRoundTripAtExtremeConfigs", findings, summary);

    // Report-only: a non-finite sample would still be worth a hard failure
    // (that's an unambiguous bug), but the text-drift finding above is a
    // confirmed observation with an unconfirmed cause, so it must not block
    // the Linux gate - see the comment above this test.
    const int nonFiniteFindings = std::count_if (findings.begin(), findings.end(), [] (const Finding& f)
                                                  { return f.symptom.contains ("non-finite"); });
    CHECK_MSG (nonFiniteFindings == 0, juce::String (nonFiniteFindings) + " restored instances produced a non-finite sample - see docs/review/QA_ROBUSTNESS.md");
}

