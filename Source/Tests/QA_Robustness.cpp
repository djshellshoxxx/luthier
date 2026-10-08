/*  Task Q3 (QA/robustness fleet): fuzz + preset/state QA.

    Report-only harness: drives the engine with randomised parameter
    combinations, random MIDI, extreme buffer sizes and sample rates, and every
    factory preset. Asserts no crash, no NaN/inf, no runaway level, and that
    the host-level state save -> load round trips. Anything that misbehaves is
    recorded with its reproduction seed rather than failing the build outright
    (this suite must not regress the Linux gate); see docs/review/QA_ROBUSTNESS.md
    for the findings this run produced.

    One configuration below is deliberately NOT exercised at its crashing
    values: preparing at a very small buffer size (empirically bisected to
    <= 4 samples; 5 and up is clean) with any preset/config whose cabinet or
    body differs from the engine's compiled-in default reliably aborts the
    process with heap corruption (glibc "corrupted size vs. prev_size" /
    "munmap_chunk(): invalid pointer" / "free(): invalid pointer" - the abort
    site and message vary between runs, itself a heap-corruption signature,
    and at least once the corruption was only detected several thousand
    render blocks and one destructor later, i.e. it is not always immediate).
    This was bisected empirically (see presetLoadAtTrulyExtremeBufferSizes
    below and docs/review/QA_ROBUSTNESS.md for the full writeup and repro); a
    byte-precise root cause would need ASan or valgrind, which this session
    attempted but could not complete in the time available (a from-scratch
    ASan rebuild of the whole engine + test suite is slow on this box). A hard
    process abort cannot be caught by CHECK/CHECK_MSG, so running the known-bad
    configuration here would take down the entire Linux test gate rather than
    just this suite; it is recorded as a permanent, unconditional finding in
    the report instead.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"

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
        real work. See the file header and presetLoadAtTrulyExtremeBufferSizes:
        the only thing that actually avoids the crash is keeping the buffer
        size out of the confirmed-bad range whenever the config might differ
        from default. */
    void randomiseAllParameters (LuthierAudioProcessor& p, juce::Random& rng)
    {
        for (auto* param : p.getParameters())
            param->setValue (rng.nextFloat());

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
        body << "  sample rates (8 kHz - 192 kHz) and buffer sizes (16 - 8192 samples; see the\n";
        body << "  critical finding below for why buffer sizes under 16 are excluded here).\n";
        body << "- A dedicated sweep of switching preset/guitar/cabinet *while already prepared*\n";
        body << "  at an extreme buffer size - the realistic \"host changes patch mid-session\"\n";
        body << "  case - restricted to buffer sizes >= 16 for the same reason, plus a permanent,\n";
        body << "  always-present finding recording the confirmed-bad zone (buffer size <= 4) with\n";
        body << "  its empirical bisection, a manual repro, and what was ruled out.\n";
        body << "- Randomised parameter combinations (every automatable parameter set to a\n";
        body << "  random normalised value, which can land on a non-default cabinet/body/guitar\n";
        body << "  config) crossed with random MIDI (notes, CCs, pitch bend, channel pressure,\n";
        body << "  all-notes-off), at extreme buffer sizes (>= 16, same reason) and sample rates.\n";
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
    buffer sizes with random MIDI. Buffer sizes are kept to the confirmed-safe
    zone (>= 16; see the file header and presetLoadAtTrulyExtremeBufferSizes
    below for the buffer sizes deliberately left out and why). Reports
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
/*  CRITICAL FINDING (found by this harness; not fixed - see the file header
    and this task's HARD LIMIT): preparing at a very small buffer size with
    any preset/config whose cabinet or body differs from the engine's
    compiled-in default (Cab4x12/Vintage30/SM57/OnAxisCapEdge/Close, per
    CabinetConfig's defaults in Source/DSP/Amp/CabinetEngine.h) reliably
    corrupts the heap. Confirmed reproducible; NOT a one-off flake.

    Bisected empirically (repeated runs, not a single observation):
      - buffer size 1, 2, 3 or 4: crashes. Manual repro: construct
        LuthierAudioProcessor, prepareToPlay(8000.0, 3 or 4), then load
        factory preset "Clean Double-Cut Funk" (FactoryPresets::getPreset(0)
        at the time of this run) via PresetManager::fromVar +
        ParameterBridge::applyAllNow. Sometimes aborts inside that
        prepareToPlay/applyAllNow call itself (bs=3, 4); sometimes only after
        thousands of clean-looking render blocks, when a later, unrelated
        allocation trips glibc's corruption check (bs=1) - itself a classic
        heap-corruption signature (silent overwrite, delayed detection). The
        abort message varies between runs: "corrupted size vs. prev_size",
        "munmap_chunk(): invalid pointer", "free(): invalid pointer".
      - buffer size 5 and up: clean in every trial run (including a 20 000-
        block soak at buffer size 5, not just the first few blocks).
      - The order of operations does NOT matter (loading the preset before
        vs. after the first prepareToPlay makes no difference): PluginProcessor::
        prepareToPlay (Source/PluginProcessor.cpp:224 engine.prepare(), then
        :293 bridge.applyAllNow()) unconditionally re-applies the full
        structural state - including any already-set, non-default cabinet/
        body config - AFTER engine.prepare() has already switched the engine
        to the new (small) buffer size. There is no call sequence a caller can
        use to dodge it other than avoiding that buffer-size range outright.
      - The bisected boundary (bad at <= 4, clean at >= 5) lines up with
        `juce::nextPowerOfTwo(maxBlockSize)`, which is 4 for maxBlockSize
        3 or 4 and 8 for maxBlockSize 5-8 - suggesting the fault is in how
        JUCE's juce::dsp::Convolution (or this codebase's use of it via
        Source/DSP/Amp/CabinetEngine.cpp / Source/DSP/Body/BodyEngine.cpp /
        Source/DSP/Common/ConvolutionInstaller.h, all of which reload a
        cabinet or body impulse response through it) handles an internal
        FFT/partition size that collapses to its practical minimum. That is
        a lead, not a confirmed byte-level cause: pinning the exact
        overflowing buffer needs ASan or valgrind. This session built an
        ASan tree (Debug, -fsanitize=address) to get one, but could not
        finish a from-scratch rebuild of the whole engine + test suite in
        the time available on this box (4 cores; still under 40% built
        after the time this investigation could spend on it) and does not
        want to leave an incomplete, uncommitted rebuild blocking the report.

    A hard process abort cannot be caught by CHECK/CHECK_MSG, so running the
    known-bad configuration here would take down the entire Linux test gate
    rather than just this suite. This test itself only sweeps the confirmed-
    safe zone (buffer size >= 16, well clear of the bisected boundary at 5) so
    the Linux gate stays green; the excluded zone is recorded as a permanent
    finding below every run, independent of what the safe sweep finds. */
LUTHIER_TEST (Fuzz, presetLoadAtTrulyExtremeBufferSizes)
{
    static const double sampleRates[] = { 8000.0, 48000.0, 192000.0 };
    static const int safeBlockSizes[] = { 16, 32, 64, 513, 1024, 4096, 8192 };   // <= 4 is the finding above; 5-15 also tested clean but kept out of the automated sweep for margin

    const bool perf = perfRunRequested();
    const int numPresets = FactoryPresets::getNumPresets();
    juce::Array<Finding> findings;

    // The permanent, unconditional finding: not discovered by the loop below,
    // since the loop deliberately never runs the crashing configuration.
    findings.add ({ "CRITICAL: heap corruption",
                     "prepareToPlay(sr, bs) with bs <= 4, with any preset/config whose cabinet or body "
                     "differs from the compiled-in default (e.g. \"Clean Double-Cut Funk\" @ 8000 Hz, block 3 or 4) "
                     "- order of preset-load vs. prepare does not matter",
                     "process aborts (glibc \"corrupted size vs. prev_size\" / \"munmap_chunk(): invalid pointer\" / "
                     "\"free(): invalid pointer\", varies between runs) from heap corruption; sometimes only detected "
                     "thousands of render blocks later. Bisected clean at buffer size 5 and up (soak-tested to 20000 "
                     "blocks at size 5) - see the comment above this test for the full empirical writeup, what was "
                     "ruled out, and why a byte-precise root cause needs ASan/valgrind this session could not finish" });

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
                p->prepareToPlay (sr, bs);   // the risky order (prepare, then switch), kept at a safe size here

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

    const juce::String summary = juce::String (cases) + " (preset, sample rate, safe block size >= 16) switch-while-running cases run across "
                                  + juce::String (numPresets) + " factory presets, plus the permanent critical finding below for block sizes <= 4.";
    appendReport ("Fuzz.presetLoadAtTrulyExtremeBufferSizes", findings, summary);

    // Only the one, permanent, always-present finding is expected; more than
    // that means the safe (>= 16) zone found something new.
    CHECK_MSG (findings.size() <= 1, "the safe (>= 16) buffer-size zone found more than the known critical bug - see docs/review/QA_ROBUSTNESS.md");
}

//==============================================================================
/*  Random parameter combinations crossed with random MIDI, at extreme buffer
    sizes and sample rates. Parameters (including structural ones - guitar
    type, cabinet, body) are randomised, which can land on a non-default
    cabinet/body config; buffer sizes are kept >= 16 (see the file header and
    presetLoadAtTrulyExtremeBufferSizes for the confirmed-bad zone this stays
    clear of). 64 seeds by default (fast), 2000 under LUTHIER_PERF=1. Any
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
    contract at extreme sample rates/buffer sizes (kept >= 16 - see the file
    header and presetLoadAtTrulyExtremeBufferSizes for why). Parameters are
    randomised on the SOURCE processor before its first prepareToPlay; the
    RESTORED processor's setStateInformation likewise runs before its own
    prepareToPlay, matching HostState::anUnpreparedInstanceSavesTheSameStateAsAPreparedOne's
    already-covered pattern. 16 seeds by default, 200 under LUTHIER_PERF=1.

    FINDING (every seed in this run, worth reading before assuming a scary
    100% failure rate): the resaved text differs from the first save in every
    one of the 16 seeds tried, unlike everyFactoryPresetHostStateRoundTrips
    above (0 findings across 36 real factory presets). Inspecting one failing
    case (seed 0) showed the differences are not float noise - concrete
    examples: "doubler_on" reads back 0.0 after restore though it was saved as
    0.514864...; a six-entry array (looks like per-string detune/frequency
    data) with substantially different values on both sides, not an ulp apart.
    "doubler_on"/"doubler_amount" are called out in Source/Parameters.cpp:1665
    as deprecated ("kept for saved automation... no longer wired to the
    engine"), which is one plausible, non-bug explanation for that one field;
    the per-string array's difference is not explained by that comment and
    could be a real bug (missed restoration path) or could be re-derived from
    other randomised parameters at apply time in a way that is not stable
    across a save/restore/resave cycle - this harness cannot tell those apart
    without a per-field, per-parameter diff (which PresetQaTests.cpp's own
    ulp-comparison approach does for the preset-manager path, but this test
    exercises the separate getStateInformation/setStateInformation contract).
    A human should treat this as: confirmed observation, unconfirmed cause;
    see docs/review/QA_ROBUSTNESS.md for the repro. Recorded rather than
    failed hard (this is a report-only harness; see the file header). */
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

