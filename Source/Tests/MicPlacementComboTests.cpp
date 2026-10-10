/*  mic-placement.md 13, the combination cases: MP-35 (a snapshot morph and a
    preset morph between two placements) and MP-36 (an LFO on the mic under
    the rhythm engine), through ComboHarness's real processor.

    A guitar's own output is full of high frequencies, so the click bound of
    MP-11 cannot be read off it. What the combination decides is the path the
    placement takes block by block; that path, recorded from the cabinet the
    processor drives, is then replayed through a cabinet with MP-11's 200 Hz
    sine, where the bound applies exactly as MP-11 states it.
*/

#include "ComboHarness.h"

#include "../DSP/Amp/MicPlacement.h"
#include "../Support/IrLibrary.h"

using namespace luthier;
using namespace luthier::tests;
using namespace luthier::combo;

namespace
{
    /** 8-16 kHz energy, loudest 1024-sample window, dBFS. */
    double worstHfDb (const std::vector<float>& y, double sr)
    {
        TptSvf hp[4], lp[4];

        for (int i = 0; i < 4; ++i)
        {
            const double q = i % 2 == 0 ? 0.5412 : 1.3066;
            hp[i].setHighpass (sr, 8000.0, q);
            lp[i].setLowpass (sr, 16000.0, q);
        }

        double worst = -300.0, acc = 0.0;
        int count = 0;

        for (size_t i = 0; i < y.size(); ++i)
        {
            double v = y[i];
            for (auto& f : hp) v = f.process (v);
            for (auto& f : lp) v = f.process (v);

            if (i < 2048)
                continue;

            acc += v * v;

            if (++count == 1024)
            {
                worst = juce::jmax (worst, 10.0 * std::log10 (acc / count + 1.0e-30));
                acc = 0.0;
                count = 0;
            }
        }

        return worst;
    }

    /** Replays a per-block placement path through a cabinet with a 200 Hz sine. */
    std::vector<float> replay (const std::vector<MicPlacement>& path, int block, double& cpuSeconds, bool withPlacement = true)
    {
        CabinetEngine cab;
        cab.prepare (kSr, block);
        cab.setConfigA ({});

        if (const auto file = IrLibrary::findCabIr (CabinetEngine::anchorConfig ({})); file.existsAsFile())
            cab.loadImpulseResponse (0, file);

        cab.setPlacementBypassed (0, ! withPlacement);
        cab.getPlacementStage (0).snapToTargets();

        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, block);
        double phase = 0.0;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();

        for (const auto& p : path)
        {
            cab.setMicPlacement (0, p);

            for (int i = 0; i < block; ++i)
            {
                const float v = (float) (0.5 * std::sin (phase));
                phase += constants::kTwoPi * 200.0 / kSr;
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }

            cab.processBlock (buf);
            out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + block);
        }

        cpuSeconds = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
        return out;
    }

    void setPlain (Rig& rig, const char* id, float v)
    {
        if (auto* p = rig.param (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
}

//==============================================================================
// MP-35: a morph is a mic move.
LUTHIER_TEST (Combo, micPlacementMorphsSmoothly)
{
    Rig rig;

    // Two placements, captured as snapshots.
    setPlain (rig, ParamIDs::micX, -1.0f);
    setPlain (rig, ParamIDs::micDist, 2.5f);
    rig.apply();
    CHECK (rig.p().getSnapshots().capture (0, "near"));

    setPlain (rig, ParamIDs::micX, 1.2f);
    setPlain (rig, ParamIDs::micDist, 40.0f);
    rig.apply();
    CHECK (rig.p().getSnapshots().capture (1, "far"));

    rig.p().getSnapshots().setCrossfadeMs (0.0);
    CHECK (rig.p().getSnapshots().recall (0));
    rig.processSilence (4);

    const auto before = rig.p().getEngine().getCabinetEngine().getMicPlacement (0);

    // The recall's crossfade carries mic_x from one to the other.
    auto& bank = rig.p().getSnapshots();
    bank.setCrossfadeMs (400.0);
    CHECK (bank.recall (1));

    std::vector<MicPlacement> path;
    juce::AudioBuffer<float> buffer (rig.bufferChannels(), kBlock);
    juce::MidiBuffer midi;

    for (int b = 0; b < (int) (0.6 * kSr / kBlock); ++b)
    {
        bank.noteAudioTime (kBlock / kSr);
        bank.advancePending();
        buffer.clear();
        rig.p().processBlock (buffer, midi);
        path.push_back (rig.p().getEngine().getCabinetEngine().getMicPlacement (0));
    }

    // Continuous: no block jumps more than a crossfade's share of the way.
    double worstStep = 0.0;

    for (size_t i = 1; i < path.size(); ++i)
        worstStep = juce::jmax (worstStep, std::abs (path[i].x - path[i - 1].x));

    CHECK_NEAR (before.x, -1.0, 1.0e-3);

    for (size_t i = 1; i < path.size(); ++i)
        CHECK_MSG (path[i].x >= path[i - 1].x - 1.0e-6, "the morph moved mic_x backwards");

    CHECK_NEAR (path.back().x, 1.2, 1.0e-3);
    CHECK_MSG (worstStep < 0.2, "the morph jumped mic_x by " + juce::String (worstStep, 3) + " in one block");

    double cpu = 0.0;
    const double hf = worstHfDb (replay (path, kBlock, cpu), kSr);
    CHECK_MSG (hf <= -80.0, "the morphed placement reached " + juce::String (hf, 1) + " dBFS at 8-16 kHz");

    // A preset morph between the same two placements: the slider is the move.
    auto& presets = rig.p().getPresetManager();
    auto& morph = rig.p().getPresetMorph();
    setPlain (rig, ParamIDs::micX, -1.0f);
    const auto a = presets.toVar ("A");
    setPlain (rig, ParamIDs::micX, 1.2f);
    const auto b = presets.toVar ("B");
    morph.setEnabled (true);
    morph.setSlot (PresetMorph::slotA, a, "A");
    morph.setSlot (PresetMorph::slotB, b, "B");

    double last = -10.0;

    for (int i = 0; i <= 20; ++i)
    {
        morph.apply (i / 20.0);
        const double x = rig.p().getState().getRawParameterValue (ParamIDs::micX)->load();
        CHECK_MSG (x >= last - 1.0e-6, "the preset morph moved mic_x backwards");
        last = x;
    }

    CHECK_NEAR (last, 1.2, 1.0e-3);
    morph.setEnabled (false);
}

//==============================================================================
// MP-36: an LFO on the mic while the rhythm engine strums.
LUTHIER_TEST (Combo, micPlacementUnderAnLfoWhileStrumming)
{
    Rig rig;
    auto& matrix = rig.p().getModMatrix();
    matrix.getLfo (0).setRateHz (5.0);

    ModRoute route;
    route.sourceId = modSourceIdForSlot (ModSourceSlots::lfoBase);
    route.destinationId = ParamIDs::micX;
    route.depth = 0.5f;
    CHECK (matrix.addRoute (route));
    rig.apply();

    rig.p().getEngine().getRhythmEngine().setEnabled (true);

    std::vector<MicPlacement> path;
    juce::AudioBuffer<float> buffer (rig.bufferChannels(), kBlock);
    const int blocks = (int) (4.0 * kSr / kBlock);
    bool finite = true;

    const auto t0 = juce::Time::getMillisecondCounterHiRes();

    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;

        if (b == 0)
            for (int note : { 40, 47, 52, 55, 59, 64 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

        // mic_dist automation: a host lane ramping 2.5 -> 60 cm over the render.
        setPlain (rig, ParamIDs::micDist, (float) (2.5 + 57.5 * b / (double) blocks));

        buffer.clear();
        rig.p().processBlock (buffer, midi);
        path.push_back (rig.p().getEngine().getCabinetEngine().getMicPlacement (0));

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < kBlock; ++i)
                finite = finite && std::isfinite (buffer.getSample (ch, i));
    }

    const double wall = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    CHECK (finite);

    // The LFO really moved the mic.
    double lo = 1.0e9, hi = -1.0e9;

    for (const auto& p : path)
    {
        lo = juce::jmin (lo, p.x);
        hi = juce::jmax (hi, p.x);
    }

    CHECK_MSG (hi - lo > 0.5, "the LFO moved mic_x only " + juce::String (hi - lo, 3));

    // MP-11's bound on that path.
    double cpuWith = 0.0, cpuPlain = 0.0;
    const double hf = worstHfDb (replay (path, kBlock, cpuWith), kSr);
    CHECK_MSG (hf <= -80.0, "the LFO'd placement reached " + juce::String (hf, 1) + " dBFS at 8-16 kHz");

    // MP-24's bound (its regression gate: the cabinet with a moving mic
    // against the same cabinet without placement). Machine-relative CPU-timing ratio and wall-clock
    // budget: measured and enforced only under LUTHIER_PERF=1 (the nightly).
    if (luthier::tests::perfRunRequested())
    {
        replay (path, kBlock, cpuPlain, false);
        CHECK_MSG (cpuWith <= juce::jmax (cpuPlain, 1.0e-4) * 1.70 * 1.20,
                   "placement under an LFO took the cabinet from " + juce::String (cpuPlain * 1000.0, 1) + " to "
                     + juce::String (cpuWith * 1000.0, 1) + " ms");
        CHECK (wall < 4.0 * 2.0);   // and the whole instrument stays inside real time
    }
}
