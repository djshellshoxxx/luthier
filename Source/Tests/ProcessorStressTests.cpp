/*  ProcessorStress: the owner-requested 30-second stress run of the FULL
    plugin processor (docs/perf/PROCESSOR_STRESS.md).

    A real LuthierAudioProcessor renders chords through several reference-board
    pedals and the legacy rack while the message thread (this test's main
    thread, which is JUCE's message thread in LuthierTests) automates host
    parameters, toggles pedal / rack / board bypass, adds, removes and reorders
    board pedals and switches presets. The audio thread is a separate thread
    that only ever calls processBlock, as a host's callback does; it is marked
    with ThreadProbe so blocking locks on it are counted (AudioThreadSafetyTests
    interposes pthread_mutex_lock on Linux), and the test binary's operator new
    counts its allocations.

    One pass per sample rate (44.1, 48, 88.2, 96, 192 kHz), each cycling every
    block size {1, 16, 37, 64, 128, 256, 481, 512, 1024, 2048} block by block,
    after a control pass at 48 kHz with the same rig and no edits. The audio
    thread is not paced: it renders as fast as the processor allows, so a pass
    covers more audio than its wall time and the timing numbers are the cost of
    each block, not a sleep.

    Env: LUTHIER_STRESS_SECONDS (default 30, clamped 30..1800),
         LUTHIER_STRESS_JSON (default ./stress-report.json),
         LUTHIER_PERF=1 turns the timing gates on (numbers are always recorded).
*/

#include "TestFramework.h"
#include "QualityTestSupport.h"

#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../DSP/Effects/Pedal.h"
#include "../DSP/Effects/EffectsChain.h"
#include "../Support/ThreadProbe.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>

#if JUCE_LINUX || JUCE_MAC
 #include <dlfcn.h>
 #include <execinfo.h>
 #define LUTHIER_STRESS_BACKTRACE 1
#endif

#if JUCE_MAC
 #include <CoreFoundation/CoreFoundation.h>
#else
 namespace juce::detail { bool dispatchNextMessageOnSystemQueue (bool returnIfNoPendingMessages); }
#endif

namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;
    void setAllocationObserverOnThisThread (void (*observer) (std::size_t) noexcept) noexcept;
}

using namespace luthier;
using namespace luthier::tests;
using namespace luthier::reference;

namespace
{
    constexpr int kBlockSizes[] { 1, 16, 37, 64, 128, 256, 481, 512, 1024, 2048 };
    constexpr int kNumBlockSizes = (int) std::size (kBlockSizes);
    constexpr int kMaxBlock = 2048;
    constexpr double kSampleRates[] { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    constexpr unsigned kSeed = 0x51a7u;
    constexpr double kOutOfRange = 16.0;   // +24 dBFS, the RefPedalSoak bound

    int blockSizeIndex (int n) noexcept
    {
        for (int i = 0; i < kNumBlockSizes; ++i)
            if (kBlockSizes[i] == n) return i;
        return -1;
    }

    //==========================================================================
    /*  Log-spaced histogram (1 % bins): fixed storage, so the audio thread can
        record every block of an arbitrarily long run without allocating, and
        percentiles are within one bin (1 %) of exact. Mean and max are exact. */
    struct LogHistogram
    {
        static constexpr double kLow = 1.0e-9, kRatio = 1.01;
        static constexpr int kBins = 2800;   // 1e-9 .. ~1.3e3
        std::array<uint64_t, kBins> bins {};
        uint64_t count = 0;
        double sum = 0.0, maximum = 0.0;

        void clear() noexcept { bins.fill (0); count = 0; sum = 0.0; maximum = 0.0; }

        void add (double x) noexcept
        {
            ++count; sum += x; maximum = std::max (maximum, x);
            int bin = 0;
            if (x > kLow)
                bin = std::min (kBins - 1, (int) (std::log (x / kLow) / std::log (kRatio)));
            ++bins[(size_t) bin];
        }

        double mean() const noexcept { return count > 0 ? sum / (double) count : 0.0; }

        /** Nearest rank, reported as the upper edge of the bin (never optimistic). */
        double percentile (double q) const noexcept
        {
            if (count == 0) return 0.0;
            const auto rank = (uint64_t) std::ceil (q * (double) count);
            uint64_t seen = 0;
            for (int b = 0; b < kBins; ++b)
            {
                seen += bins[(size_t) b];
                if (seen >= std::max<uint64_t> (1, rank))
                    return std::min (maximum, kLow * std::pow (kRatio, b + 1));
            }
            return maximum;
        }
    };

    //==========================================================================
    /*  Allocation call sites on the measured thread. The test binary's
        operator new calls the observer; the first kMaxSites allocations of a
        pass store raw return addresses (backtrace() needs no heap once warmed
        up), symbolised later on the message thread. */
    struct AllocationSites
    {
        static constexpr int kMaxSites = 4, kMaxFrames = 28;
        struct Site { int frames = 0; std::size_t size = 0; uint64_t block = 0; void* pcs[kMaxFrames] {}; };
        std::array<Site, kMaxSites> sites {};
        std::atomic<int> captured { 0 };
        uint64_t currentBlock = 0;
    };

    thread_local AllocationSites* activeSites = nullptr;
    thread_local bool insideObserver = false;

    void observeAllocation (std::size_t size) noexcept
    {
        auto* sites = activeSites;
        if (sites == nullptr || insideObserver) return;
        const int index = sites->captured.load (std::memory_order_relaxed);
        if (index >= AllocationSites::kMaxSites) return;
        insideObserver = true;
        auto& site = sites->sites[(size_t) index];
        site.size = size; site.block = sites->currentBlock;
       #if LUTHIER_STRESS_BACKTRACE
        site.frames = backtrace (site.pcs, AllocationSites::kMaxFrames);
       #else
        site.frames = 0;
       #endif
        sites->captured.store (index + 1, std::memory_order_relaxed);
        insideObserver = false;
    }

    /** Call once on a thread before installing the observer: the first
        backtrace() loads the unwinder, which allocates. */
    void warmUpBacktrace() noexcept
    {
       #if LUTHIER_STRESS_BACKTRACE
        void* pcs[4];
        (void) backtrace (pcs, 4);
       #endif
    }

    juce::String run (const juce::StringArray& command)
    {
        juce::ChildProcess child;
        if (! child.start (command, juce::ChildProcess::wantStdOut)) return {};
        auto text = child.readAllProcessOutput();
        child.waitForProcessToFinish (5000);
        return text;
    }

    /** "module+0xoffset (function at file:line)" per frame, outermost last.
        Uses addr2line when present (the binary is not linked -rdynamic, so
        backtrace_symbols alone gives only offsets). */
    juce::StringArray symbolise (const AllocationSites::Site& site)
    {
        juce::StringArray lines;
       #if LUTHIER_STRESS_BACKTRACE
        const bool haveAddr2line = juce::File ("/usr/bin/addr2line").existsAsFile();
        for (int f = 1; f < site.frames; ++f)   // frame 0 is observeAllocation
        {
            Dl_info info {};
            juce::String module = "?", offset;
            if (dladdr (site.pcs[f], &info) != 0 && info.dli_fname != nullptr)
            {
                module = info.dli_fname;
                // The return address is one past the call: step back into it.
                const auto off = (uintptr_t) site.pcs[f] - (uintptr_t) info.dli_fbase - 1;
                offset = "0x" + juce::String::toHexString ((juce::int64) off);
            }
            juce::String where;
            if (haveAddr2line && offset.isNotEmpty())
                where = run ({ "/usr/bin/addr2line", "-C", "-f", "-p", "-e", module, offset }).trim();
            if (where.isEmpty() && info.dli_sname != nullptr)
                where = info.dli_sname;
            lines.add (juce::File (module).getFileName() + "+" + offset + "  " + where);
        }
       #else
        juce::ignoreUnused (site);
        lines.add ("(backtraces unavailable on this platform)");
       #endif
        return lines;
    }

    //==========================================================================
    /*  Click rule (documented in PROCESSOR_STRESS.md). A click is a sample
        whose second difference |x[n] - 2x[n-1] + x[n-2]| is (a) at least
        kFloor in absolute terms and (b) at least kRatio times the largest
        second difference of the preceding signal, held by a peak follower with
        a kHoldSeconds release. A step discontinuity of height h has a second
        difference of h; band-limited music has a second difference far below
        its amplitude and repeats its own sharpest features (pick attacks,
        clipped edges), which the peak hold remembers. After a click the
        detector is refractory for 2 ms so one step counts once. */
    struct ClickDetector
    {
        static constexpr double kFloor = 0.05, kRatio = 4.0, kHoldSeconds = 0.1, kRefractorySeconds = 0.002;
        double x1 = 0.0, x2 = 0.0, hold = 0.0, release = 0.0;
        int primed = 0, refractory = 0, refractorySamples = 0;

        void prepare (double sampleRate) noexcept
        {
            release = std::exp (-1.0 / (kHoldSeconds * sampleRate));
            refractorySamples = (int) std::ceil (kRefractorySeconds * sampleRate);
            reset();
        }

        void reset() noexcept { x1 = x2 = hold = 0.0; primed = 0; refractory = 0; }

        /** Returns the second difference when this sample is a click, else 0. */
        double process (double x) noexcept
        {
            if (! std::isfinite (x)) { reset(); return 0.0; }
            double clickAmount = 0.0;
            if (primed >= 2)
            {
                const double d2 = std::abs (x - 2.0 * x1 + x2);
                if (refractory > 0) --refractory;
                else if (d2 >= kFloor && d2 >= kRatio * hold)
                {
                    clickAmount = d2;
                    refractory = refractorySamples;
                }
                hold = std::max (hold * release, d2);
            }
            else
                ++primed;
            x2 = x1; x1 = x;
            return clickAmount;
        }
    };

    //==========================================================================
    enum EditKind
    {
        editNone, editParam, editBoardParam, editPedalBypass, editRackBypass, editBoardBypass,
        editReorder, editAdd, editRemove, editMix, editPreset, editRackType, numEditKinds
    };

    const char* editName (int kind)
    {
        static const char* names[] { "none", "param", "board_param", "pedal_bypass", "rack_bypass", "board_bypass",
                                     "reorder", "add", "remove", "mix", "preset", "rack_type" };
        return juce::isPositiveAndBelow (kind, (int) numEditKinds) ? names[kind] : "?";
    }

    /*  How long after an edit is first visible to the audio thread its
        audible consequences are allowed to be discontinuous (audio time).
        Bypass crossfades are 10 ms (Pedal.h SwitchCrossfade); a preset rebuilds
        the instrument; a rack type change is a structural rebuild. */
    double editWindowSeconds (int kind)
    {
        switch (kind)
        {
            case editPreset:   return 0.300;
            case editRackType: return 0.100;
            case editReorder: case editAdd: case editRemove: return 0.050;
            default:           return 0.030;
        }
    }

    constexpr double kNoteWindowSeconds = 0.100;   // pick attacks, strum spread, note-off damping
    constexpr double kWarmUpSeconds = 0.200;       // after prepareToPlay: first chord out of silence
    constexpr double kChordPeriodSeconds = 0.5;

    /** What the message thread tells the audio thread about its last edit. */
    struct EditSignal
    {
        std::atomic<uint32_t> serial { 0 };
        std::atomic<int> kind { editNone }, detail { -1 };

        void publish (int k, int d) noexcept
        {
            kind.store (k, std::memory_order_relaxed);
            detail.store (d, std::memory_order_relaxed);
            serial.fetch_add (1, std::memory_order_release);
        }
    };

    struct ClickEvent
    {
        uint64_t sample = 0;
        int channel = 0, blockSize = 0, editKind = editNone, editDetail = -1;
        double amount = 0.0, step = 0.0, secondsSinceEdit = -1.0, secondsSinceNote = -1.0;
        const char* zone = "steady";
    };

    //==========================================================================
    /*  Everything one pass measures. The audio-thread half (record*, scan)
        touches only preallocated storage; the self-test drives the same
        methods with injected faults. */
    struct PassMeter
    {
        double sampleRate = 48000.0;
        uint64_t blocks = 0, samples = 0, overruns = 0, nonFinite = 0, outOfRange = 0;
        uint64_t allocations = 0, allocatingBlocks = 0;
        std::array<uint64_t, kNumBlockSizes> blocksBySize {};
        std::array<LogHistogram, kNumBlockSizes> secondsBySize {};
        LogHistogram seconds, rtf, rtfLarge;   // rtfLarge: blocks >= 64 samples
        AllocationSites sites;

        static constexpr int kMaxEvents = 32;
        std::array<ClickEvent, kMaxEvents> events {};
        uint64_t clicksSteady = 0, clicksEdit = 0, clicksNote = 0, windowSamples = 0;
        std::array<uint64_t, numEditKinds> clicksAfterEdit {};
        int numEvents = 0;
        std::array<ClickDetector, 2> detectors;

        // Zone bookkeeping, in samples of this pass.
        uint64_t editWindowEnd = 0, noteWindowEnd = 0, warmUpEnd = 0, lastEditSample = 0, lastNoteSample = 0;
        bool sawEdit = false, sawNote = false;
        int lastEditKind = editNone, lastEditDetail = -1;

        void prepare (double rate)
        {
            sampleRate = rate;
            blocks = samples = overruns = nonFinite = outOfRange = allocations = allocatingBlocks = 0;
            blocksBySize.fill (0);
            for (auto& h : secondsBySize) h.clear();
            seconds.clear(); rtf.clear(); rtfLarge.clear();
            sites.captured.store (0); sites.currentBlock = 0;
            clicksSteady = clicksEdit = clicksNote = windowSamples = 0;
            clicksAfterEdit.fill (0);
            numEvents = 0;
            for (auto& d : detectors) d.prepare (rate);
            editWindowEnd = noteWindowEnd = lastEditSample = lastNoteSample = 0;
            warmUpEnd = (uint64_t) (kWarmUpSeconds * rate);
            sawEdit = sawNote = false; lastEditKind = editNone; lastEditDetail = -1;
        }

        uint64_t toSamples (double s) const noexcept { return (uint64_t) std::ceil (s * sampleRate); }

        void noteEdit (int kind, int detail) noexcept
        {
            sawEdit = true; lastEditKind = kind; lastEditDetail = detail; lastEditSample = samples;
            editWindowEnd = std::max (editWindowEnd, samples + toSamples (editWindowSeconds (kind)));
        }

        void noteMidi (uint64_t at) noexcept
        {
            sawNote = true; lastNoteSample = at;
            noteWindowEnd = std::max (noteWindowEnd, at + toSamples (kNoteWindowSeconds));
        }

        /** One processBlock's cost, and what operator new saw during it. */
        void recordBlock (int n, double elapsedSeconds, long allocationDelta) noexcept
        {
            const double duration = (double) n / sampleRate;
            const double ratio = elapsedSeconds / duration;
            seconds.add (elapsedSeconds);
            rtf.add (ratio);
            if (n >= 64)
            {
                rtfLarge.add (ratio);
                if (elapsedSeconds > duration) ++overruns;
            }
            if (const int s = blockSizeIndex (n); s >= 0)
            { ++blocksBySize[(size_t) s]; secondsBySize[(size_t) s].add (elapsedSeconds); }
            if (allocationDelta > 0) { allocations += (uint64_t) allocationDelta; ++allocatingBlocks; }
            ++blocks;
            sites.currentBlock = blocks;
        }

        /** Scans one rendered block (after recordBlock's sample count has not yet moved). */
        void scan (const float* const* channels, int numChannels, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                const uint64_t at = samples + (uint64_t) i;
                const bool inEdit = at < editWindowEnd, inNote = at < noteWindowEnd || at < warmUpEnd;
                if (inEdit || inNote) ++windowSamples;
                for (int ch = 0; ch < std::min (numChannels, 2); ++ch)
                {
                    const double x = channels[ch][i];
                    if (! std::isfinite (x)) { ++nonFinite; detectors[(size_t) ch].process (x); continue; }
                    if (std::abs (x) > kOutOfRange) ++outOfRange;
                    const double prev = detectors[(size_t) ch].x1;
                    const double amount = detectors[(size_t) ch].process (x);
                    if (amount <= 0.0) continue;
                    const char* zone = inEdit ? "edit" : (inNote ? "note" : "steady");
                    if (inEdit) { ++clicksEdit; ++clicksAfterEdit[(size_t) lastEditKind]; }
                    else if (inNote) ++clicksNote;
                    else ++clicksSteady;
                    if (numEvents < kMaxEvents)
                    {
                        auto& e = events[(size_t) numEvents++];
                        e.sample = at; e.channel = ch; e.blockSize = n; e.amount = amount; e.step = x - prev;
                        e.editKind = sawEdit ? lastEditKind : editNone; e.editDetail = sawEdit ? lastEditDetail : -1;
                        e.secondsSinceEdit = sawEdit ? (double) (at - lastEditSample) / sampleRate : -1.0;
                        e.secondsSinceNote = sawNote ? (double) (at >= lastNoteSample ? at - lastNoteSample : 0) / sampleRate : -1.0;
                        e.zone = zone;
                    }
                }
            }
            samples += (uint64_t) n;
        }

        uint64_t clicksTotal() const noexcept { return clicksSteady + clicksEdit + clicksNote; }
    };

    //==========================================================================
    struct Chords
    {
        // Open-position shapes inside every guitar's range (E2 = 40 up).
        static constexpr int kNotes[4][6] { { 40, 47, 52, 56, 59, 64 }, { 45, 52, 57, 61, 64, -1 },
                                             { 43, 47, 50, 55, 59, 67 }, { 50, 57, 62, 66, -1, -1 } };
        int current = -1;

        void addChange (juce::MidiBuffer& midi, int offset, int velocityStep)
        {
            if (current >= 0)
                for (int note : kNotes[current]) if (note > 0) midi.addEvent (juce::MidiMessage::noteOff (1, note), offset);
            current = (current + 1) % 4;
            for (int note : kNotes[current])
                if (note > 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) (90 + velocityStep % 30)), offset);
        }
    };

    //==========================================================================
    void pumpMessages()
    {
        for (int i = 0; i < 64; ++i)
        {
           #if JUCE_MAC
            if (CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0, true) != kCFRunLoopRunHandledSource) break;
           #else
            if (! juce::detail::dispatchNextMessageOnSystemQueue (true)) break;
           #endif
        }
    }

    struct PassResult
    {
        juce::String name;
        double sampleRate = 0.0;
        bool control = false;
        double wallSeconds = 0.0, prepareSeconds = 0.0, audioCpuSeconds = 0.0;
        double rssStart = -1, rssAfterWarmUp = -1, rssEnd = -1, rssPeak = -1;
        uint64_t locks = 0, fileAccesses = 0;
        std::array<uint64_t, numEditKinds> edits {};
        uint64_t rejected = 0;
        juce::StringArray rejectedReasons;
        std::vector<juce::StringArray> allocationStacks;
        std::vector<std::pair<std::size_t, uint64_t>> allocationSiteInfo;
        juce::StringArray boardModels;
        std::unique_ptr<PassMeter> meter;
    };

    //==========================================================================
    /*  The message-thread half: holds the parameter lists and the RNG, and
        performs one edit at a time through the same entry points a host and
        the editor use. */
    struct Editor
    {
        LuthierAudioProcessor& p;
        BoardModel& model;
        std::mt19937 random { kSeed };
        std::vector<juce::RangedAudioParameter*> floats;
        std::vector<juce::String> models;
        EditSignal& signal;
        PassResult* result = nullptr;
        int boardOffTicks = -1;
        int presetCount = 0;

        Editor (LuthierAudioProcessor& processor, EditSignal& s)
            : p (processor), model (processor.getEngine().getReferencePedals().model()), signal (s)
        {
            for (auto* parameter : p.getParameters())
                if (auto* f = dynamic_cast<juce::AudioParameterFloat*> (parameter); f != nullptr && f->isAutomatable())
                    floats.push_back (f);
            for (const auto* descriptor : PedalRegistry::all())
                if (! juce::String (descriptor->modelId).startsWith ("test."))
                    models.emplace_back (descriptor->modelId);
            presetCount = p.getNumPrograms();
        }

        float uniform() { return float (random() % 100001u) / 100000.f; }

        void count (int kind, bool accepted, const juce::String& reason = {})
        {
            ++result->edits[(size_t) kind];
            if (! accepted)
            {
                ++result->rejected;
                if (result->rejectedReasons.size() < 12)
                    result->rejectedReasons.addIfNotAlreadyThere (juce::String (editName (kind)) + ": " + reason);
            }
            else
                signal.publish (kind, -1);
        }

        void setHost (const juce::String& id, float value)
        {
            if (auto* parameter = p.getState().getParameter (id))
                parameter->setValueNotifyingHost (value);
        }

        /** The legacy rack: a compressor and drive before the amp, chorus,
            delay and reverb after it. A preset may replace them; switchPreset
            puts the drive and delay back. */
        void setUpRack()
        {
            const std::pair<std::pair<bool, int>, PedalType> rack[] {
                { { false, 0 }, PedalType::Compressor }, { { false, 1 }, PedalType::Overdrive },
                { { true, 0 }, PedalType::Chorus }, { { true, 1 }, PedalType::Delay }, { { true, 2 }, PedalType::Reverb } };
            for (const auto& [slot, type] : rack)
            {
                if (auto* t = p.getState().getParameter (ParamIDs::slotType (slot.first, slot.second)))
                    t->setValueNotifyingHost (t->convertTo0to1 ((float) (int) type));
                setHost (ParamIDs::slotBypass (slot.first, slot.second), 0.f);
            }
            p.getParameterBridge().applyAllNow();
        }

        void ensureBoard (int minimum)
        {
            juce::String reason;
            while (model.getInstanceCount() < minimum)
            {
                const auto& id = models[random() % models.size()];
                const bool ok = model.apply (cmd::Add { id.toStdString() }, &reason);
                if (result != nullptr) count (editAdd, ok, reason);
                if (! ok) break;
            }
            if (! model.getBoardSettings().enabled)
            {
                const bool ok = model.apply (cmd::SetBoardEnabled { true }, &reason);
                if (result != nullptr) count (editBoardBypass, ok, reason);
            }
            boardOffTicks = -1;
        }

        void switchPreset()
        {
            if (presetCount <= 1) return;
            const int index = (int) (random() % (unsigned) presetCount);
            p.setCurrentProgram (index);
            count (editPreset, p.getCurrentProgram() == index, "program " + juce::String (index) + " did not load");
            // Keep the legacy rack in the chain whatever the preset chose.
            if (auto* t = p.getState().getParameter (ParamIDs::slotType (false, 1)))
                t->setValueNotifyingHost (t->convertTo0to1 ((float) (int) PedalType::Overdrive));
            if (auto* t = p.getState().getParameter (ParamIDs::slotType (true, 1)))
                t->setValueNotifyingHost (t->convertTo0to1 ((float) (int) PedalType::Delay));
            count (editRackType, true);
        }

        /** One randomly chosen edit (200 per second of wall time). */
        void step()
        {
            if (boardOffTicks >= 0 && --boardOffTicks < 0)
            {
                juce::String reason;
                count (editBoardBypass, model.apply (cmd::SetBoardEnabled { true }, &reason), reason);
                return;
            }
            const auto order = model.getOrder();
            juce::String reason;
            const unsigned roll = random() % 100u;
            if (roll < 40)
            {
                // Host automation: a random walk with occasional jumps, as a
                // drawn automation lane or a fast knob turn produces.
                auto* parameter = floats[random() % floats.size()];
                float v = parameter->getValue();
                v = (random() % 10u) == 0 ? uniform() : juce::jlimit (0.f, 1.f, v + (uniform() - .5f) * .2f);
                parameter->setValueNotifyingHost (v);
                count (editParam, true);
            }
            else if (roll < 50)
            {
                if (order.empty()) { count (editBoardParam, false, "empty board"); return; }
                const auto id = order[random() % order.size()];
                const auto instance = model.getInstance (id);
                const auto* desc = instance ? PedalRegistry::find (instance->modelId.c_str()) : nullptr;
                if (desc == nullptr || desc->numParams <= 0) { count (editBoardParam, false, "no params"); return; }
                count (editBoardParam, model.apply (cmd::SetParam { id, (int) (random() % (unsigned) desc->numParams), uniform(), 0 }, &reason), reason);
            }
            else if (roll < 65)
            {
                if (order.empty()) { count (editPedalBypass, false, "empty board"); return; }
                const auto id = order[random() % order.size()];
                const auto instance = model.getInstance (id);
                count (editPedalBypass, model.apply (cmd::SetEnabled { id, instance ? ! instance->enabled : true }, &reason), reason);
            }
            else if (roll < 70)
            {
                const bool post = (random() & 1u) != 0;
                const int slot = (int) (random() % 3u);
                if (auto* b = p.getState().getParameter (ParamIDs::slotBypass (post, slot)))
                    b->setValueNotifyingHost (b->getValue() > .5f ? 0.f : 1.f);
                count (editRackBypass, true);
            }
            else if (roll < 73)
            {
                if (boardOffTicks >= 0) return;
                count (editBoardBypass, model.apply (cmd::SetBoardEnabled { false }, &reason), reason);
                boardOffTicks = 3;   // a footswitch tap: back on 15 ms later
            }
            else if (roll < 83)
            {
                if (order.size() < 2) { count (editReorder, false, "fewer than two pedals"); return; }
                count (editReorder, model.apply (cmd::Move { order[random() % order.size()], (int) (random() % order.size()) }, &reason), reason);
            }
            else if (roll < 89)
            {
                if (order.size() >= 8) { ensureBoard (3); return; }
                count (editAdd, model.apply (cmd::Add { models[random() % models.size()].toStdString() }, &reason), reason);
            }
            else if (roll < 95)
            {
                if (order.size() <= 3) { count (editRemove, false, "keeping three pedals"); return; }
                count (editRemove, model.apply (cmd::Remove { order[random() % order.size()] }, &reason), reason);
            }
            else
                count (editMix, model.apply (cmd::SetMix { .5f + uniform() * .5f }, &reason), reason);
        }
    };

    //==========================================================================
    double percentileMs (const LogHistogram& h, double q) { return h.percentile (q) * 1000.0; }

    juce::var histogramMs (const LogHistogram& h)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("mean", h.mean() * 1000.0);
        o->setProperty ("p50", percentileMs (h, .50));
        o->setProperty ("p99", percentileMs (h, .99));
        o->setProperty ("max", h.maximum * 1000.0);
        return o;
    }

    juce::var toVar (const PassResult& r)
    {
        const auto& m = *r.meter;
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", r.name);
        o->setProperty ("control", r.control);
        o->setProperty ("sample_rate", r.sampleRate);
        o->setProperty ("wall_s", r.wallSeconds);
        o->setProperty ("prepare_s", r.prepareSeconds);
        o->setProperty ("audio_thread_cpu_s", r.audioCpuSeconds);
        o->setProperty ("blocks", (juce::int64) m.blocks);
        o->setProperty ("samples", (juce::int64) m.samples);
        o->setProperty ("audio_s", (double) m.samples / m.sampleRate);
        auto* sizes = new juce::DynamicObject();
        auto* bySize = new juce::DynamicObject();
        for (int s = 0; s < kNumBlockSizes; ++s)
        {
            const auto key = juce::String (kBlockSizes[s]);
            sizes->setProperty (key, (juce::int64) m.blocksBySize[(size_t) s]);
            const auto& h = m.secondsBySize[(size_t) s];
            auto* entry = new juce::DynamicObject();
            const double budget = (double) kBlockSizes[s] / m.sampleRate;
            entry->setProperty ("mean_ms", h.mean() * 1000.0);
            entry->setProperty ("p99_ms", percentileMs (h, .99));
            entry->setProperty ("max_ms", h.maximum * 1000.0);
            entry->setProperty ("rtf_mean", h.mean() / budget);
            entry->setProperty ("rtf_p99", h.percentile (.99) / budget);
            bySize->setProperty (key, entry);
        }
        o->setProperty ("block_sizes", sizes);
        o->setProperty ("by_block_size", bySize);
        o->setProperty ("block_ms", histogramMs (m.seconds));
        o->setProperty ("budget_ms_min", 1000.0 / m.sampleRate);
        o->setProperty ("rtf_mean", m.rtf.mean());
        o->setProperty ("rtf_p99", m.rtf.percentile (.99));
        o->setProperty ("rtf_mean_ge64", m.rtfLarge.mean());
        o->setProperty ("rtf_p99_ge64", m.rtfLarge.percentile (.99));
        o->setProperty ("rtf_max_ge64", m.rtfLarge.maximum);
        o->setProperty ("overruns", (juce::int64) m.overruns);
        o->setProperty ("blocks_ge64", (juce::int64) m.rtfLarge.count);
        o->setProperty ("audio_thread_allocs", (juce::int64) m.allocations);
        o->setProperty ("audio_thread_allocating_blocks", (juce::int64) m.allocatingBlocks);
        juce::Array<juce::var> stacks;
        for (size_t i = 0; i < r.allocationStacks.size(); ++i)
        {
            auto* s = new juce::DynamicObject();
            s->setProperty ("size", (juce::int64) r.allocationSiteInfo[i].first);
            s->setProperty ("block", (juce::int64) r.allocationSiteInfo[i].second);
            juce::Array<juce::var> frames;
            for (const auto& f : r.allocationStacks[i]) frames.add (f);
            s->setProperty ("frames", frames);
            stacks.add (s);
        }
        o->setProperty ("audio_thread_alloc_sites", stacks);
        o->setProperty ("audio_thread_locks", (juce::int64) r.locks);
        o->setProperty ("audio_thread_file_accesses", (juce::int64) r.fileAccesses);
        o->setProperty ("non_finite", (juce::int64) m.nonFinite);
        o->setProperty ("out_of_range", (juce::int64) m.outOfRange);
        auto* clicks = new juce::DynamicObject();
        clicks->setProperty ("steady", (juce::int64) m.clicksSteady);
        clicks->setProperty ("edit_window", (juce::int64) m.clicksEdit);
        clicks->setProperty ("note_window", (juce::int64) m.clicksNote);
        clicks->setProperty ("windowed_fraction", m.samples > 0 ? (double) m.windowSamples / (double) m.samples : 0.0);
        auto* afterEdit = new juce::DynamicObject();
        for (int k = 1; k < numEditKinds; ++k)
            if (m.clicksAfterEdit[(size_t) k] > 0) afterEdit->setProperty (editName (k), (juce::int64) m.clicksAfterEdit[(size_t) k]);
        clicks->setProperty ("edit_window_by_kind", afterEdit);
        juce::Array<juce::var> events;
        for (int i = 0; i < m.numEvents; ++i)
        {
            const auto& e = m.events[(size_t) i];
            auto* ev = new juce::DynamicObject();
            ev->setProperty ("sample", (juce::int64) e.sample);
            ev->setProperty ("time_s", (double) e.sample / m.sampleRate);
            ev->setProperty ("zone", e.zone);
            ev->setProperty ("channel", e.channel);
            ev->setProperty ("block_size", e.blockSize);
            ev->setProperty ("second_difference", e.amount);
            ev->setProperty ("step", e.step);
            ev->setProperty ("preceding_edit", editName (e.editKind));
            ev->setProperty ("s_since_edit", e.secondsSinceEdit);
            ev->setProperty ("s_since_note", e.secondsSinceNote);
            events.add (ev);
        }
        clicks->setProperty ("events", events);
        o->setProperty ("clicks", clicks);
        auto* rss = new juce::DynamicObject();
        rss->setProperty ("start", r.rssStart);
        rss->setProperty ("after_warmup", r.rssAfterWarmUp);
        rss->setProperty ("end", r.rssEnd);
        rss->setProperty ("peak", r.rssPeak);
        rss->setProperty ("growth", r.rssEnd >= 0 && r.rssAfterWarmUp >= 0 ? r.rssEnd - r.rssAfterWarmUp : -1.0);
        o->setProperty ("rss_mb", rss);
        auto* edits = new juce::DynamicObject();
        uint64_t total = 0;
        for (int k = 1; k < numEditKinds; ++k) { edits->setProperty (editName (k), (juce::int64) r.edits[(size_t) k]); total += r.edits[(size_t) k]; }
        edits->setProperty ("total", (juce::int64) total);
        edits->setProperty ("rejected", (juce::int64) r.rejected);
        juce::Array<juce::var> reasons;
        for (const auto& s : r.rejectedReasons) reasons.add (s);
        edits->setProperty ("rejected_reasons", reasons);
        o->setProperty ("edits", edits);
        juce::Array<juce::var> board;
        for (const auto& s : r.boardModels) board.add (s);
        o->setProperty ("board_at_end", board);
        return o;
    }

    juce::File reportFile()
    {
        const auto path = juce::SystemStats::getEnvironmentVariable ("LUTHIER_STRESS_JSON", {});
        if (path.isNotEmpty()) return juce::File::getCurrentWorkingDirectory().getChildFile (path);
        return juce::File::getCurrentWorkingDirectory().getChildFile ("stress-report.json");
    }

    int requestedSeconds()
    {
        const auto text = juce::SystemStats::getEnvironmentVariable ("LUTHIER_STRESS_SECONDS", {});
        return text.isEmpty() ? 30 : juce::jlimit (30, 1800, text.getIntValue());
    }

    //==========================================================================
    /*  Gates. Hard invariants always; timing only under LUTHIER_PERF=1 (a quiet
        machine). Thresholds and their calibration: PROCESSOR_STRESS.md. */
    constexpr double kRssGrowthLimitMb = 64.0;     // per pass, after its warm-up second
    constexpr double kRtfP99Limit = 0.25;          // blocks >= 64, LUTHIER_PERF=1 only
    constexpr double kOverrunFractionLimit = 1.0e-3;

    juce::StringArray gate (const PassResult& r, bool timingGates)
    {
        const auto& m = *r.meter;
        juce::StringArray f;
        const auto tag = r.name + ": ";
        if (m.blocks < 100) f.add (tag + "only " + juce::String ((juce::int64) m.blocks) + " blocks rendered");
        for (int s = 0; s < kNumBlockSizes; ++s)
            if (m.blocksBySize[(size_t) s] == 0) f.add (tag + "block size " + juce::String (kBlockSizes[s]) + " never rendered");
        if (m.nonFinite != 0) f.add (tag + juce::String ((juce::int64) m.nonFinite) + " non-finite samples");
        if (m.outOfRange != 0) f.add (tag + juce::String ((juce::int64) m.outOfRange) + " samples beyond +/-16");
        if (m.allocations != 0) f.add (tag + juce::String ((juce::int64) m.allocations) + " audio-thread allocations in "
                                       + juce::String ((juce::int64) m.allocatingBlocks) + " blocks");
        if (r.locks != 0) f.add (tag + juce::String ((juce::int64) r.locks) + " blocking locks on the audio thread");
        if (m.clicksSteady != 0) f.add (tag + juce::String ((juce::int64) m.clicksSteady) + " clicks outside edit/note windows");
        if (r.control && m.clicksEdit != 0) f.add (tag + "control pass reported edit-window clicks");
        const double growth = r.rssEnd >= 0 && r.rssAfterWarmUp >= 0 ? r.rssEnd - r.rssAfterWarmUp : 0.0;
        if (growth > kRssGrowthLimitMb) f.add (tag + "RSS grew " + juce::String (growth, 1) + " MB after warm-up");
        if (! r.control)
        {
            uint64_t total = 0;
            for (auto e : r.edits) total += e;
            if (total < (uint64_t) (r.wallSeconds * 100.0)) f.add (tag + "only " + juce::String ((juce::int64) total) + " edits");
            if (r.edits[editPreset] == 0) f.add (tag + "no preset switch happened");
        }
        if (timingGates)
        {
            if (m.rtfLarge.percentile (.99) > kRtfP99Limit)
                f.add (tag + "rtf p99 (blocks >= 64) " + juce::String (m.rtfLarge.percentile (.99), 3) + " > " + juce::String (kRtfP99Limit));
            if ((double) m.overruns > kOverrunFractionLimit * (double) m.rtfLarge.count)
                f.add (tag + juce::String ((juce::int64) m.overruns) + " overruns in " + juce::String ((juce::int64) m.rtfLarge.count) + " blocks >= 64");
        }
        return f;
    }

    //==========================================================================
    void runPass (LuthierAudioProcessor& p, Editor& editor, EditSignal& signal, PassResult& result,
                  std::chrono::steady_clock::time_point deadline)
    {
        using clock = std::chrono::steady_clock;
        auto& meter = *result.meter;
        editor.result = &result;
        result.rssStart = residentMegabytes();
        result.rssPeak = result.rssStart;

        const auto prepareStart = clock::now();
        p.prepareToPlay (result.sampleRate, kMaxBlock);
        editor.ensureBoard (4);
        if (result.control) for (auto& e : result.edits) e = 0;   // setup, not stress
        pumpMessages();
        result.prepareSeconds = std::chrono::duration<double> (clock::now() - prepareStart).count();
        meter.prepare (result.sampleRate);

        const int numChannels = juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
        juce::AudioBuffer<float> storage (numChannels, kMaxBlock);
        juce::MidiBuffer midi;
        midi.ensureSize (16384);
        p.reserveMidiOutputBuffer (midi);

        std::atomic<bool> stop { false };
        std::atomic<int> locksSeen { 0 }, filesSeen { 0 };
        std::atomic<double> cpuSeconds { 0.0 };

        std::thread audio ([&]
        {
            warmUpBacktrace();
            Chords chords;
            int sizeCursor = 0, velocity = 0;
            uint32_t seenSerial = signal.serial.load (std::memory_order_acquire);
            const auto chordPeriod = (uint64_t) (kChordPeriodSeconds * result.sampleRate);
            uint64_t nextChord = (uint64_t) (0.01 * result.sampleRate);
            std::array<float*, 64> pointers {};
            ThreadProbe::markAsAudioThread();
            activeSites = &meter.sites;
            setAllocationObserverOnThisThread (observeAllocation);
            const int locksBefore = ThreadProbe::audioThreadLocks.load();
            const int filesBefore = ThreadProbe::audioThreadFileAccesses.load();
            const double cpuStart = threadCpuTimeSeconds();

            while (! stop.load (std::memory_order_relaxed))
            {
                const int n = kBlockSizes[sizeCursor++ % kNumBlockSizes];
                if (const auto serial = signal.serial.load (std::memory_order_acquire); serial != seenSerial)
                {
                    seenSerial = serial;
                    meter.noteEdit (signal.kind.load (std::memory_order_relaxed), signal.detail.load (std::memory_order_relaxed));
                }
                midi.clear();
                while (nextChord < meter.samples + (uint64_t) n)
                {
                    chords.addChange (midi, (int) (nextChord - meter.samples), velocity++);
                    meter.noteMidi (nextChord);
                    nextChord += chordPeriod;
                }
                storage.clear();
                for (int ch = 0; ch < numChannels && ch < (int) pointers.size(); ++ch)
                    pointers[(size_t) ch] = storage.getWritePointer (ch);
                juce::AudioBuffer<float> block (pointers.data(), numChannels, n);   // refers; no allocation

                const long allocationsBefore = allocationsOnThisThread();
                const auto t0 = clock::now();
                p.processBlock (block, midi);
                const auto t1 = clock::now();
                const long allocationDelta = allocationsOnThisThread() - allocationsBefore;

                meter.recordBlock (n, std::chrono::duration<double> (t1 - t0).count(), allocationDelta);
                meter.scan (block.getArrayOfReadPointers(), numChannels, n);
            }

            cpuSeconds.store (threadCpuTimeSeconds() - cpuStart);
            locksSeen.store (ThreadProbe::audioThreadLocks.load() - locksBefore);
            filesSeen.store (ThreadProbe::audioThreadFileAccesses.load() - filesBefore);
            setAllocationObserverOnThisThread (nullptr);
            activeSites = nullptr;
            ThreadProbe::markAsAudioThread (false);
        });

        // The message thread: edits at 200/s, a preset every 500 ms, the
        // processor's 30 Hz timer (board reclaim, lane service) via the queue.
        const auto start = clock::now();
        const auto warmUpAt = start + std::chrono::seconds (1);
        auto nextPreset = start + std::chrono::milliseconds (500);
        auto nextRss = start;
        uint64_t tick = 0;
        while (clock::now() < deadline)
        {
            if (! result.control)
            {
                if (clock::now() >= nextPreset) { editor.switchPreset(); nextPreset += std::chrono::milliseconds (500); }
                else editor.step();
            }
            pumpMessages();
            if (const auto now = clock::now(); now >= nextRss)
            {
                const double rss = residentMegabytes();
                result.rssPeak = std::max (result.rssPeak, rss);
                if (now >= warmUpAt && result.rssAfterWarmUp < 0) result.rssAfterWarmUp = rss;
                nextRss = now + std::chrono::milliseconds (100);
            }
            std::this_thread::sleep_until (start + std::chrono::milliseconds (5 * ++tick));
        }

        stop.store (true);
        audio.join();
        result.wallSeconds = std::chrono::duration<double> (clock::now() - start).count();
        result.audioCpuSeconds = cpuSeconds.load();
        result.locks = (uint64_t) locksSeen.load();
        result.fileAccesses = (uint64_t) filesSeen.load();
        pumpMessages();
        result.rssEnd = residentMegabytes();
        result.rssPeak = std::max (result.rssPeak, result.rssEnd);
        if (result.rssAfterWarmUp < 0) result.rssAfterWarmUp = result.rssEnd;

        const int captured = std::min (meter.sites.captured.load(), AllocationSites::kMaxSites);
        for (int i = 0; i < captured; ++i)
        {
            result.allocationStacks.push_back (symbolise (meter.sites.sites[(size_t) i]));
            result.allocationSiteInfo.emplace_back (meter.sites.sites[(size_t) i].size, meter.sites.sites[(size_t) i].block);
        }
        for (const auto& id : editor.model.getOrder())
            if (const auto instance = editor.model.getInstance (id))
                result.boardModels.add (juce::String (instance->modelId) + (instance->enabled ? "" : " (off)"));
        p.releaseResources();
    }

    void printPass (const PassResult& r)
    {
        const auto& m = *r.meter;
        uint64_t edits = 0;
        for (auto e : r.edits) edits += e;
        std::cout << "    " << r.name << ": blocks=" << m.blocks << " audio_s=" << juce::String ((double) m.samples / m.sampleRate, 2)
                  << " wall_s=" << juce::String (r.wallSeconds, 2) << " cpu_s=" << juce::String (r.audioCpuSeconds, 2)
                  << " ms(mean/p50/p99/max)=" << juce::String (m.seconds.mean() * 1e3, 4) << "/" << juce::String (percentileMs (m.seconds, .5), 4)
                  << "/" << juce::String (percentileMs (m.seconds, .99), 4) << "/" << juce::String (m.seconds.maximum * 1e3, 3)
                  << " rtf(mean/p99)=" << juce::String (m.rtf.mean(), 3) << "/" << juce::String (m.rtf.percentile (.99), 3)
                  << " rtf>=64(mean/p99)=" << juce::String (m.rtfLarge.mean(), 3) << "/" << juce::String (m.rtfLarge.percentile (.99), 3)
                  << " overruns=" << m.overruns << " allocs=" << m.allocations << "/" << m.allocatingBlocks << "blk"
                  << " locks=" << r.locks << " files=" << r.fileAccesses << " nonfinite=" << m.nonFinite << " oor=" << m.outOfRange
                  << " clicks(steady/edit/note)=" << m.clicksSteady << "/" << m.clicksEdit << "/" << m.clicksNote
                  << " rss(start/warm/end)=" << juce::String (r.rssStart, 0) << "/" << juce::String (r.rssAfterWarmUp, 0) << "/" << juce::String (r.rssEnd, 0)
                  << " edits=" << edits << " rejected=" << r.rejected << std::endl;
        for (int i = 0; i < m.numEvents && i < 6; ++i)
        {
            const auto& e = m.events[(size_t) i];
            std::cout << "      click t=" << juce::String ((double) e.sample / m.sampleRate, 5) << "s sample=" << e.sample << " ch=" << e.channel
                      << " zone=" << e.zone << " d2=" << juce::String (e.amount, 4) << " step=" << juce::String (e.step, 4)
                      << " after=" << editName (e.editKind) << " +" << juce::String (e.secondsSinceEdit * 1e3, 1) << "ms"
                      << " note+" << juce::String (e.secondsSinceNote * 1e3, 1) << "ms block=" << e.blockSize << std::endl;
        }
        for (size_t i = 0; i < r.allocationStacks.size(); ++i)
        {
            std::cout << "      alloc #" << i << " size=" << r.allocationSiteInfo[i].first << " block=" << r.allocationSiteInfo[i].second << std::endl;
            for (int f = 0; f < r.allocationStacks[i].size() && f < 14; ++f)
                std::cout << "        " << r.allocationStacks[i][f] << std::endl;
        }
    }
}

//==============================================================================
LUTHIER_TEST (ProcessorStress, selfTestDetectsInjectedFaults)
{
    // Clean signal: a 440 Hz sine at -10 dBFS, 0.5 s.
    auto render = [] (PassMeter& meter, int stepAt, double stepHeight, int nanAt, int hugeAt)
    {
        std::array<float, 256> l {}, r {};
        const float* channels[] { l.data(), r.data() };
        int cursor = 0;
        while (cursor < 24000)
        {
            for (int i = 0; i < 256; ++i)
            {
                const int at = cursor + i;
                double x = .3 * std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * at / 48000.0);
                if (stepAt >= 0 && at >= stepAt) x += stepHeight;
                l[(size_t) i] = r[(size_t) i] = (float) x;
                if (at == nanAt) l[(size_t) i] = std::numeric_limits<float>::quiet_NaN();
                if (at == hugeAt) r[(size_t) i] = 20.f;
            }
            meter.recordBlock (256, 1.0e-4, 0);
            meter.scan (channels, 2, 256);
            cursor += 256;
        }
    };

    {
        auto meter = std::make_unique<PassMeter>(); meter->prepare (48000.0);
        render (*meter, -1, 0.0, -1, -1);
        CHECK (meter->clicksTotal() == 0);
        CHECK (meter->nonFinite == 0 && meter->outOfRange == 0 && meter->overruns == 0);
    }
    {
        // A +0.2 step at 0.25 s, outside every window: one steady click on each channel.
        auto meter = std::make_unique<PassMeter>(); meter->prepare (48000.0);
        render (*meter, 12000, .2, -1, -1);
        CHECK (meter->clicksSteady == 2);
        CHECK (meter->numEvents == 2 && meter->events[0].sample == 12000);
    }
    {
        // The same step just after an edit is attributed to it, not gated.
        auto meter = std::make_unique<PassMeter>(); meter->prepare (48000.0);
        std::array<float, 256> l {}, r {};
        const float* channels[] { l.data(), r.data() };
        for (int cursor = 0; cursor < 24000; cursor += 256)
        {
            if (cursor == 11776) meter->noteEdit (editReorder, -1);
            for (int i = 0; i < 256; ++i)
                l[(size_t) i] = r[(size_t) i] = (float) (.3 * std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * (cursor + i) / 48000.0)
                                                         + (cursor + i >= 12000 ? .2 : 0.0));
            meter->recordBlock (256, 1.0e-4, 0);
            meter->scan (channels, 2, 256);
        }
        CHECK (meter->clicksSteady == 0 && meter->clicksEdit == 2);
        CHECK (meter->clicksAfterEdit[editReorder] == 2);
    }
    {
        // NaN and +20 (out of range).
        auto meter = std::make_unique<PassMeter>(); meter->prepare (48000.0);
        render (*meter, -1, 0.0, 5000, 7000);
        CHECK (meter->nonFinite == 1);
        CHECK (meter->outOfRange == 1);
    }
    {
        // An overrun: a 256-sample block (5.33 ms at 48 kHz) that took 8 ms;
        // a 16-sample block that overran is not an overrun by definition.
        auto meter = std::make_unique<PassMeter>(); meter->prepare (48000.0);
        meter->recordBlock (256, 0.008, 0);
        meter->recordBlock (256, 0.001, 0);
        meter->recordBlock (16, 0.008, 0);
        CHECK (meter->overruns == 1);
        CHECK (meter->rtfLarge.count == 2);
        CHECK_NEAR (meter->rtfLarge.maximum, 0.008 / (256.0 / 48000.0), 1e-9);
        CHECK (meter->rtfLarge.percentile (.99) >= 1.0);
    }
    {
        // Percentiles of a known distribution are within the 1 % bin width.
        LogHistogram h;
        for (int i = 1; i <= 1000; ++i) h.add (i * 1.0e-6);
        CHECK (h.percentile (.5) >= 500e-6 && h.percentile (.5) <= 500e-6 * 1.011);
        CHECK (h.percentile (.99) >= 990e-6 && h.percentile (.99) <= 990e-6 * 1.011);
        CHECK_NEAR (h.mean(), 500.5e-6, 1e-12);
        CHECK_NEAR (h.maximum, 1000e-6, 1e-12);
    }
    {
        // An allocation inside the measured region is counted and its call site captured.
        auto meter = std::make_unique<PassMeter>(); meter->prepare (48000.0);
        long delta = 0;
        std::thread worker ([&]
        {
            warmUpBacktrace();
            activeSites = &meter->sites;
            setAllocationObserverOnThisThread (observeAllocation);
            const long before = allocationsOnThisThread();
            static std::atomic<int*> sink { nullptr };
            sink.store (new int[16]);
            delta = allocationsOnThisThread() - before;
            setAllocationObserverOnThisThread (nullptr);
            activeSites = nullptr;
            delete[] sink.exchange (nullptr);
        });
        worker.join();
        meter->recordBlock (64, 1.0e-5, delta);
        CHECK (meter->allocations == 1 && meter->allocatingBlocks == 1);
        CHECK (meter->sites.captured.load() == 1);
        CHECK (meter->sites.sites[0].size == 16 * sizeof (int));
       #if LUTHIER_STRESS_BACKTRACE
        CHECK (meter->sites.sites[0].frames > 2);
        CHECK (! symbolise (meter->sites.sites[0]).isEmpty());
       #endif
    }
    {
        // A failing pass is reported by the gate, with each kind of fault named.
        PassResult r; r.name = "injected"; r.meter = std::make_unique<PassMeter>(); r.meter->prepare (48000.0);
        render (*r.meter, 12000, .2, 5000, 7000);
        r.meter->recordBlock (256, 0.008, 1);
        r.locks = 1; r.rssAfterWarmUp = 100; r.rssEnd = 100 + kRssGrowthLimitMb + 1; r.control = true;
        const auto failures = gate (r, true).joinIntoString ("\n");
        for (const char* expected : { "non-finite", "beyond +/-16", "allocations", "blocking locks", "clicks outside", "RSS grew", "overruns" })
            CHECK_MSG (failures.contains (expected), juce::String ("gate did not report: ") + expected);
    }
}

//==============================================================================
LUTHIER_TEST (ProcessorStress, thirtySecondFullProcessorMatrix)
{
    using clock = std::chrono::steady_clock;
    PedalRegistry::registerAll();
    const int seconds = requestedSeconds();
    const bool timingGates = perfRunRequested();
    const auto file = reportFile();
    const auto runStart = clock::now();

    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& p = *processor;
    EditSignal signal;
    Editor editor (p, signal);
    CHECK (! editor.floats.empty());
    CHECK (editor.models.size() >= 4);
    CHECK (editor.presetCount > 1);
    if (editor.floats.empty() || editor.models.size() < 4) return;
    editor.setUpRack();

    std::vector<PassResult> results;
    juce::StringArray failures;

    auto writeReport = [&] (bool finished)
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("schema", 1);
        root->setProperty ("suite", "ProcessorStress.thirtySecondFullProcessorMatrix");
        root->setProperty ("seed", (juce::int64) kSeed);
        root->setProperty ("duration_s", seconds);
        root->setProperty ("wall_s", std::chrono::duration<double> (clock::now() - runStart).count());
        root->setProperty ("finished", finished);
        root->setProperty ("timing_gates", timingGates);
        auto* machine = new juce::DynamicObject();
        machine->setProperty ("cpu", juce::SystemStats::getCpuModel());
        machine->setProperty ("logical_cpus", juce::SystemStats::getNumCpus());
        machine->setProperty ("memory_mb", juce::SystemStats::getMemorySizeInMegabytes());
        machine->setProperty ("os", juce::SystemStats::getOperatingSystemName());
        root->setProperty ("machine", machine);
        auto* build = new juce::DynamicObject();
        build->setProperty ("git", juce::SystemStats::getEnvironmentVariable ("LUTHIER_GIT_SHA", "unknown"));
       #if JUCE_DEBUG
        build->setProperty ("config", "Debug");
       #else
        build->setProperty ("config", "Release");
       #endif
        build->setProperty ("edition", LUTHIER_PRO ? "Pro" : "Free");
        root->setProperty ("build", build);
        auto* thresholds = new juce::DynamicObject();
        thresholds->setProperty ("out_of_range_abs", kOutOfRange);
        thresholds->setProperty ("rss_growth_mb_per_pass", kRssGrowthLimitMb);
        thresholds->setProperty ("rtf_p99_ge64", kRtfP99Limit);
        thresholds->setProperty ("overrun_fraction_ge64", kOverrunFractionLimit);
        thresholds->setProperty ("click_floor", ClickDetector::kFloor);
        thresholds->setProperty ("click_ratio", ClickDetector::kRatio);
        thresholds->setProperty ("click_hold_s", ClickDetector::kHoldSeconds);
        root->setProperty ("thresholds", thresholds);
        juce::Array<juce::var> passes;
        for (const auto& r : results) passes.add (toVar (r));
        root->setProperty ("passes", passes);
        juce::Array<juce::var> failed;
        for (const auto& f : failures) failed.add (f);
        root->setProperty ("failures", failed);
        root->setProperty ("pass", finished && failures.isEmpty());
        file.replaceWithText (juce::JSON::toString (juce::var (root)));
    };

    // Time split: control 10 %, each of the five rates 18 % (prepare included).
    struct Plan { double rate; bool control; double share; };
    std::vector<Plan> plan { { 48000.0, true, .10 } };
    for (double rate : kSampleRates) plan.push_back ({ rate, false, .18 });

    const auto budget = std::chrono::duration<double> (seconds);
    double elapsedShare = 0.0;
    for (const auto& step : plan)
    {
        elapsedShare += step.share;
        const auto deadline = runStart + std::chrono::duration_cast<clock::duration> (budget * elapsedShare);
        results.emplace_back();
        auto& r = results.back();
        r.sampleRate = step.rate; r.control = step.control;
        r.name = (step.control ? "control@" : "stress@") + juce::String ((int) step.rate);
        r.meter = std::make_unique<PassMeter>();
        runPass (p, editor, signal, r, deadline);
        printPass (r);
        failures.addArray (gate (r, timingGates));
        writeReport (false);
    }

    // Whole-run summary.
    uint64_t blocks = 0, samples = 0, allocs = 0, locks = 0, nonFinite = 0, oor = 0, steady = 0, edits = 0, rejected = 0, overruns = 0;
    double worstRtf = 0.0, rssGrowth = 0.0;
    for (const auto& r : results)
    {
        const auto& m = *r.meter;
        blocks += m.blocks; samples += m.samples; allocs += m.allocations; locks += r.locks; nonFinite += m.nonFinite;
        oor += m.outOfRange; steady += m.clicksSteady; overruns += m.overruns; rejected += r.rejected;
        for (auto e : r.edits) edits += e;
        worstRtf = std::max (worstRtf, m.rtfLarge.percentile (.99));
        rssGrowth = std::max (rssGrowth, r.rssEnd - r.rssAfterWarmUp);
    }
    writeReport (true);
    std::cout << "    ProcessorStress " << (failures.isEmpty() ? "PASS" : "FAIL") << " seconds=" << seconds << " passes=" << results.size()
              << " blocks=" << blocks << " samples=" << samples << " worst_rtf_p99_ge64=" << juce::String (worstRtf, 3)
              << " overruns_ge64=" << overruns << " allocs=" << allocs << " locks=" << locks << " nonfinite=" << nonFinite
              << " oor=" << oor << " steady_clicks=" << steady << " max_rss_growth_mb=" << juce::String (rssGrowth, 1)
              << " edits=" << edits << " rejected=" << rejected << " timing_gates=" << (timingGates ? "on" : "off")
              << " json=" << file.getFullPathName() << std::endl;

    for (const auto& f : failures)
        CHECK_MSG (false, f);
    CHECK (file.existsAsFile());
}
