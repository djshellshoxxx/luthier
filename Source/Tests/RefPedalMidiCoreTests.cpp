// OWNER: WP5
#include "TestFramework.h"
#include "../ReferencePedals/Midi/MidiRouter.h"
#include "../ReferencePedals/Midi/MidiMap.h"
#include "../ReferencePedals/Midi/ClockSync.h"
#include <memory>
using namespace luthier::reference;
using namespace luthier::reference::midi;
LUTHIER_TEST (RefPedalMidiCore, mappedOffsetsAndReservedGuard)
{
    MidiRouter router; router.prepare (48000, 512);
    auto map = std::make_unique<MapSnap>();
    Binding b; b.srcNum = 20; b.target = 3; b.smoothMs = 0;
    CHECK (map->add (b));
    b.srcNum = 123; CHECK (! map->add (b));
    map->compile(); router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 100);
    RouteOut out; router.process (in, {}, 0, 512, out);
    CHECK (out.nWrites == 1); CHECK (out.writes[0].offset == 100);
    CHECK (out.writes[0].paramIdx == 3); CHECK (out.writes[0].norm == 1.f);
}
LUTHIER_TEST (RefPedalMidiCore, clockStartStopLoss)
{
    ClockSync clock; clock.prepare (48000);
    RouteOut out; clock.begin ({}, TempoSource::MidiClock, 120, 0, out);
    clock.event (0xfa, 0, 0, 0, out);
    for (int i = 0; i < 48; ++i) clock.event (0xf8, 0, 0, i * 1000, out);
    clock.finish (48000, out);
    CHECK (std::abs (out.time.bpm - 120.) < 0.1); CHECK (out.time.playing);
    clock.event (0xfc, 0, 0, 48000, out); CHECK (! clock.isRunning());
    clock.event (0xfb, 0, 0, 48000, out); CHECK (clock.isRunning());
    clock.finish (73000, out); CHECK (clock.isLost()); CHECK (! clock.isRunning());
}
LUTHIER_TEST (RefPedalMidiCore, boundedFloodProtectsClock)
{
    MidiRouter router; router.prepare (48000, 512);
    juce::MidiBuffer in;
    for (int i = 0; i < 800; ++i) in.addEvent (juce::MidiMessage::controllerEvent (1, 20, i % 128), 0);
    in.addEvent (juce::MidiMessage::midiStart(), 1);
    in.addEvent (juce::MidiMessage::midiClock(), 2);
    RouteOut out; router.process (in, {}, 0, 512, out);
    CHECK (router.droppedCount() > 0); CHECK (out.nTr >= 1);
}
LUTHIER_TEST (RefPedalMidiCore, learnNeedsDistinctValuesAndSuppresses)
{
    MidiRouter router; router.prepare (48000, 512); router.armLearn ({ 0, 0, true });
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 10), 0);
    RouteOut out; router.process (in, {}, 0, 512, out); LearnCapture c;
    CHECK (! router.pollLearn (c));
    in.clear(); in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 80), 4);
    router.process (in, {}, 512, 512, out); CHECK (router.pollLearn (c));
    CHECK (c.valid); CHECK (c.num == 20); CHECK (c.chan == 1);
}

#include <thread>
#include <atomic>
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST (RefPedalMidiCore, noAllocationsDuringRouting)
{
    MidiRouter router; router.prepare (48000, 512);
    auto map = std::make_unique<MapSnap>(); Binding b; b.srcNum = 20; CHECK (map->add (b));
    router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 37);
    RouteOut out;
    const auto before = luthier::tests::allocationsOnThisThread();
    router.process (in, {}, 0, 512, out);
    const auto after = luthier::tests::allocationsOnThisThread();
    CHECK (after == before); CHECK (out.nWrites == 1);
}
LUTHIER_TEST (RefPedalMidiCore, concurrentPublicationAndReclamation)
{
    MidiRouter router; router.prepare (48000, 64);
    std::atomic<bool> done { false }, valid { true };
    std::thread audio ([&]
    {
        juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 37);
        RouteOut out; int64_t sample = 0;
        while (! done.load (std::memory_order_acquire))
        {
            router.process (in, {}, sample, 64, out); sample += 64;
            if (out.nWrites > 1 || (out.nWrites == 1 && (out.writes[0].norm < 0 || out.writes[0].norm > 1))) valid.store (false);
        }
    });
    for (int i = 0; i < 200; ++i)
    {
        auto map = std::make_unique<MapSnap>(); Binding b; b.srcNum = 20; b.target = (uint16_t) (i % 32); map->add (b);
        router.publishMap (std::move (map)); router.reclaim();
    }
    done.store (true, std::memory_order_release); audio.join();
    CHECK (valid.load());
}
LUTHIER_TEST (RefPedalMidiCore, boardTargetsRetainOffsets)
{
    MidiRouter router; router.prepare (48000, 512);
    auto map = std::make_unique<MapSnap>();
    Binding b; b.srcNum = 20; b.target = kTargetBoardBase + (uint16_t) BoardTarget::Mix; CHECK (map->add (b));
    router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 37);
    RouteOut out; router.process (in, {}, 0, 512, out);
    CHECK (out.nWrites == 1); CHECK (out.writes[0].slot == kBoardTargetSlot);
    CHECK (out.writes[0].paramIdx == (uint16_t) BoardTarget::Mix); CHECK (out.writes[0].offset == 37);
}
LUTHIER_TEST (RefPedalMidiCore, softTakeoverAndExactChannelPrecedence)
{
    MidiRouter router; router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>(); Binding b; b.srcNum = 20; b.flags |= BF_SoftTakeover;
    CHECK (map->add (b)); map->initialValues[0] = .5f;
    router.publishMap (std::move (map));
    juce::MidiBuffer in; RouteOut out;
    in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 10), 0);
    router.process (in, {}, 0, 128, out); CHECK (out.nWrites == 0);
    in.clear(); in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 100), 0);
    router.process (in, {}, 128, 128, out); CHECK (out.nWrites == 1);
    auto exact = std::make_unique<MapSnap>(); b.flags = BF_Enabled; b.target = 1; exact->add (b);
    b.chan = 2; b.target = 2; exact->add (b); router.publishMap (std::move (exact));
    in.clear(); in.addEvent (juce::MidiMessage::controllerEvent (2, 20, 100), 0);
    router.process (in, {}, 256, 128, out); CHECK (out.nWrites == 1); CHECK (out.writes[0].paramIdx == 2);
}
LUTHIER_TEST (RefPedalMidiCore, heldNotesReleaseOriginalSlotAcrossReorder)
{
    MidiRouter router; router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>(); map->noteToSlot[36] = 5; router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 20), 37);
    RouteOut out; router.process (in, {}, 0, 128, out);
    CHECK (out.nFoot == 1); CHECK (out.footSlot[0] == 5); CHECK (out.foot[0].press);
    auto moved = std::make_unique<MapSnap>(); moved->noteToSlot[36] = 7; router.publishMap (std::move (moved));
    in.clear(); in.addEvent (juce::MidiMessage::noteOff (1, 36), 44);
    router.process (in, {}, 128, 128, out);
    CHECK (out.nFoot == 1); CHECK (out.footSlot[0] == 5); CHECK (! out.foot[0].press); CHECK (out.foot[0].offset == 44);
}
LUTHIER_TEST (RefPedalMidiCore, disabledBoardOnlyAllowsEnableTarget)
{
    MidiRouter router; router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>(); map->boardEnabled = false; map->noteToSlot[36] = 0;
    Binding b; b.srcNum = 20; b.target = 1; map->add (b);
    b.srcNum = 21; b.target = kTargetBoardBase + (uint16_t) BoardTarget::Enabled; map->add (b);
    router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 80), 0);
    in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 1);
    in.addEvent (juce::MidiMessage::controllerEvent (1, 21, 127), 37);
    in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 38);
    RouteOut out; router.process (in, {}, 0, 128, out);
    CHECK (out.nFoot == 0); CHECK (out.nWrites == 2);
    CHECK (out.writes[0].slot == kBoardTargetSlot); CHECK (out.writes[0].offset == 37);
    CHECK (out.writes[1].slot == 0); CHECK (out.writes[1].offset == 38);
}
LUTHIER_TEST (RefPedalMidiCore, hostTransportPriorityAndStopEdge)
{
    ClockSync clock; clock.prepare (48000); RouteOut out;
    HostTransport host; host.valid = true; host.playing = true; host.bpm = 143; host.ppq = 0;
    clock.begin (host, TempoSource::Auto, 120, 0, out); clock.finish (64, out);
    CHECK (out.nTr == 1); CHECK (out.tr[0] == TransportEvent::Start); CHECK (out.time.bpm == 143);
    out.clear(); host.ppq = 64 * 143. / (60 * 48000);
    clock.begin (host, TempoSource::Auto, 120, 64, out); clock.finish (128, out); CHECK (out.nTr == 0);
    out.clear(); host.playing = false; clock.begin (host, TempoSource::Auto, 120, 128, out); clock.finish (192, out);
    CHECK (out.nTr == 1); CHECK (out.tr[0] == TransportEvent::Stop);
    out.clear(); clock.begin (host, TempoSource::Auto, 120, 192, out); CHECK (out.nTr == 0);
}
LUTHIER_TEST (RefPedalMidiCore, clockTempoIndependentOfRateAndBlockPartition)
{
    const double rates[] { 44100, 48000, 96000, 192000 };
    const int sizes[] { 1, 37, 64, 333, 2048 };
    for (double rate : rates)
        for (int block : sizes)
        {
            ClockSync clock; clock.prepare (rate); RouteOut out; int tick = 0;
            const int64_t end = (int64_t) rate;
            for (int64_t start = 0; start < end; start += block)
            {
                out.clear(); clock.begin ({}, TempoSource::MidiClock, 120, start, out);
                while ((int64_t) std::llround (tick * rate / 48) < std::min (end, start + block))
                { clock.event (0xf8, 0, 0, (int64_t) std::llround (tick * rate / 48), out); ++tick; }
                clock.finish (std::min (end, start + block), out);
            }
            CHECK (std::abs (out.time.bpm - 120.) < .1); CHECK (std::isfinite (out.time.ppq));
        }
}

#include "../ReferencePedals/Midi/HostAutomationCapture.h"
LUTHIER_TEST (RefPedalMidiCore, hostCaptureUnitsAndThreadLocalEcho)
{
    HostAutomationCapture capture;
    CHECK (capture.write (2, .5f)); CHECK (capture.value (2) == 160.f);
    CHECK (capture.write (20, 0)); CHECK (capture.value (20) == -12.f);
    CHECK (capture.write (22, .5f)); CHECK (capture.value (22) == 8.f);
    std::atomic<bool> begin { false };
    std::thread host ([&] { while (! begin.load (std::memory_order_acquire)) {} for (int i = 0; i < 10000; ++i) capture.write (4, .75f); });
    {
        HostAutomationCapture::EchoScope echo (capture); begin.store (true, std::memory_order_release);
        for (int i = 0; i < 10000; ++i) capture.write (4, .1f);
        host.join(); CHECK (! capture.write (4, .2f));
    }
    const auto snapshot = capture.snapshot(); CHECK (snapshot.serials[4] == 10000); CHECK (snapshot.values[4] == .75f);
    CHECK (capture.write (4, .75f)); CHECK (capture.snapshot().serials[4] == 10001);
}
LUTHIER_TEST (RefPedalMidiCore, hostEnableGatesMidiWithoutModelEcho)
{
    MidiRouter router; router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>(); map->boardEnabled = false;
    Binding b; b.srcNum = 20; b.target = 1; map->add (b); router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 37);
    RouteOut out;
    router.setHostEnabled (true, 1); router.process (in, {}, 0, 128, out);
    CHECK (out.nWrites == 1); CHECK (out.writes[0].offset == 37);
    router.setHostEnabled (false, 2); router.process (in, {}, 128, 128, out); CHECK (out.nWrites == 0);
    auto changed = std::make_unique<MapSnap>(); changed->boardEnabled = true; changed->add (b);
    router.publishMap (std::move (changed)); router.setHostEnabled (false, 2);
    router.process (in, {}, 256, 128, out); CHECK (out.nWrites == 1);
    router.setHostEnabled (false, 3); router.process (in, {}, 384, 128, out); CHECK (out.nWrites == 0);
}
LUTHIER_TEST (RefPedalMidiCore, sameValueModelDisableDefeatsConsumedHostWrite)
{
    MidiRouter router; router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>(); map->boardEnabled = false;
    Binding b; b.srcNum = 20; b.target = 1; map->add (b); router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 37);
    RouteOut out;
    router.setHostEnabled (true, UINT32_MAX); router.process (in, {}, 0, 128, out); CHECK (out.nWrites == 1);
    router.setModelEnabled (false, 1); router.setHostEnabled (true, UINT32_MAX);
    router.process (in, {}, 128, 128, out); CHECK (out.nWrites == 0);
    router.setHostEnabled (true, 1); router.process (in, {}, 256, 128, out); CHECK (out.nWrites == 1);
    router.setModelEnabled (false, 2); router.process (in, {}, 384, 128, out); CHECK (out.nWrites == 0);
    router.prepare (96000, 128); router.setModelEnabled (false, 2); router.setHostEnabled (false, 2);
    router.process (in, {}, 0, 128, out); CHECK (out.nWrites == 0);
}
LUTHIER_TEST (RefPedalMidiCore, highResolutionCcIgnoresOrphanLsb)
{
    MidiRouter router; router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>(); Binding b; b.srcNum = 20; b.flags |= BF_HiRes; CHECK (map->add (b));
    b.srcNum = 7; CHECK (! map->add (b)); router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 52, 127), 0);
    RouteOut out; router.process (in, {}, 0, 128, out); CHECK (out.nWrites == 0);
    in.clear(); in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 64), 1);
    in.addEvent (juce::MidiMessage::controllerEvent (1, 52, 127), 2);
    router.process (in, {}, 128, 128, out); CHECK (out.nWrites == 2);
    CHECK_NEAR (out.writes[0].norm, 8192.f / 16383.f, 1e-6);
    CHECK_NEAR (out.writes[1].norm, 8319.f / 16383.f, 1e-6);
}

#include "../ReferencePedals/Midi/MidiState.h"
#include "../ReferencePedals/PedalRegistry.h"
namespace
{
const ParamDesc mapTestParams[] {
    { "map_test_gain", "Gain", "", ParamKind::Continuous, Taper::Linear, 0, 1, .5f, 1, 0, nullptr, PF_Automatable | PF_ExprTarget, 10, 14 },
    { "map_test_on", "On", "", ParamKind::Toggle, Taper::Linear, 0, 1, 1, 1, 0, nullptr, PF_Engage, 0, 85 }
};
const PedalDesc mapTestDesc { "test.midi-map", "map_test_", 1, "MIDI map test", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::DSP, 2, mapTestParams, 0, false, false, false, 1 };
std::unique_ptr<IPedal> makeMapTest() { return nullptr; }
}
LUTHIER_TEST (RefPedalMidiCore, stableUuidFocusAndLocalProfilePrecedence)
{
    PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (mapTestDesc, makeMapTest));
    std::vector<PedalInstance> instances (2);
    for (int i = 0; i < 2; ++i)
    {
        instances[(size_t) i].modelId = "test.midi-map"; instances[(size_t) i].slot = i == 0 ? 5 : 2;
        instances[(size_t) i].params[0] = .5f;
        instances[(size_t) i].extras = new juce::DynamicObject();
        juce::var local (new juce::DynamicObject()); local.getDynamicObject()->setProperty ("chan", 2);
        instances[(size_t) i].extras.getDynamicObject()->setProperty ("midi", local);
    }
    BoardSettings settings; settings.enabled = true; settings.midi = new juce::DynamicObject();
    settings.midi.getDynamicObject()->setProperty ("selectedInstance", instances[1].instanceId.toString());
    auto map = MidiState::buildMap (instances, settings);
    const auto& focus = map->find (MidiSrc::CC, 1, 70); CHECK (focus.count == 1);
    CHECK (map->bindings[focus.index[0]].target == 2 * kMaxParams);
    const auto& local = map->find (MidiSrc::CC, 2, 14); CHECK (local.count == 1);
    CHECK (map->bindings[local.index[0]].target == 5 * kMaxParams);
    CHECK (map->find (MidiSrc::CC, 1, 14).count == 0); CHECK (map->footCc[0][64] == 2);
    CHECK (map->footCc[1][85] == 5); CHECK (map->noteToSlot[36] == 5); CHECK (map->noteToSlot[37] == 2);
}
LUTHIER_TEST (RefPedalMidiCore, staleHostEchoMapCannotReenableBoard)
{
    MidiRouter router; router.prepare (48000, 128);
    auto map = std::make_unique<MapSnap>(); map->boardEnabled = false;
    Binding b; b.srcNum = 20; b.target = 1; map->add (b); router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 37);
    RouteOut out; router.setModelEnabled (false, 1); router.setHostEnabled (true, 1);
    router.process (in, {}, 0, 128, out); CHECK (out.nWrites == 1);
    router.setHostEnabled (false, 2); router.process (in, {}, 128, 128, out); CHECK (out.nWrites == 0);
    auto stale = std::make_unique<MapSnap>(); stale->boardEnabled = true; stale->add (b); router.publishMap (std::move (stale));
    router.setModelEnabled (true, 1); router.setHostEnabled (false, 2);
    router.process (in, {}, 256, 128, out); CHECK (out.nWrites == 0);
    router.setModelEnabled (true, 2); router.process (in, {}, 384, 128, out); CHECK (out.nWrites == 1);
}
LUTHIER_TEST (RefPedalMidiCore, adaptiveCurvesMeetAnalyticErrorBudget)
{
    for (auto id : { CurveId::Log, CurveId::Exp, CurveId::SCurve, CurveId::Linear })
        for (float p : { .5f, 2.f, 4.f })
        {
            CurveLut lut; buildCurve (lut, id, p); float error = 0, previous = -1; bool monotone = true;
            for (int i = 0; i <= 100000; ++i)
            {
                const float x = i / 100000.f, y = evalCurve (lut, x);
                error = std::max (error, std::abs (y - curveValue (id, x, p)));
                monotone &= y >= previous; previous = y;
            }
            CHECK_MSG (error <= 1e-4f, "curve error " + juce::String (error, 8)); CHECK (monotone);
            CHECK (evalCurve (lut, 0) == 0.f); CHECK (evalCurve (lut, 1) == 1.f);
        }
    CurveLut step; buildCurve (step, CurveId::Step, 16);
    CHECK_NEAR (evalCurve (step, .5f), 8.f / 15.f, 1e-7); CHECK_NEAR (evalCurve (step, .5f - 1e-5f), 7.f / 15.f, 1e-7);
}
LUTHIER_TEST (RefPedalMidiCore, customPchipPreservesMonotoneShape)
{
    CustomPoints points { CurvePoint {0,0}, {.01f,.3f}, {.2f,.4f}, {.8f,.9f}, {1,1} };
    CHECK (validPoints (points)); CurveLut lut; buildCurve (lut, CurveId::Custom5, 0, &points);
    const auto slopes = pchipSlopes (points); float previous = -1, error = 0; bool monotone = true;
    for (int i = 0; i <= 100000; ++i)
    {
        const float x = i / 100000.f, y = evalCurve (lut, x); monotone &= y >= previous; previous = y;
        error = std::max (error, std::abs (y - pchipValue (points, slopes, x)));
    }
    CHECK (monotone); CHECK_MSG (error <= 1e-4f, "PCHIP error " + juce::String (error, 8));
    CHECK (evalCurve (lut, 0) == 0); CHECK (evalCurve (lut, 1) == 1);
    points[2].y = .1f; CHECK (! validPoints (points)); CHECK (validPoints (points, true));
}

#include "../Support/HostMidiCapacity.h"
#include "../Support/BoundedMidi.h"
LUTHIER_TEST (RefPedalMidiCore, externalMidiCapacityIsExplicitAndBounded)
{
    luthier::HostMidiCapacity capacity; juce::MidiBuffer dest, source;
    CHECK (capacity.lowerBound (dest, luthier::MidiCapacityContract::Unknown) == 0);
    for (int i = 0; i < 100; ++i) source.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), i);
    source.addEvent (juce::MidiMessage::noteOff (1, 60), 101);
    capacity.reserveExternal (dest, 256);
    const auto* storage = dest.data.getRawDataPointer();
    const int bytes = capacity.lowerBound (dest, luthier::MidiCapacityContract::Unknown);
    int dropped = 0;
    const auto before = luthier::tests::allocationsOnThisThread();
    dropped = luthier::BoundedMidi::addEvents (dest, bytes, source, 0, -1, 0);
    const auto after = luthier::tests::allocationsOnThisThread(); CHECK (before == after);
    CHECK (dropped > 0); CHECK (dest.data.size() <= 256); CHECK (storage == dest.data.getRawDataPointer());
    bool release = false; for (const auto event : dest) release |= event.getMessage().isNoteOff(); CHECK (release);
    luthier::HostMidiCapacity unknown; CHECK (unknown.lowerBound (dest, luthier::MidiCapacityContract::Unknown) == dest.data.size()); dest.clear();
    CHECK (capacity.lowerBound (dest, luthier::MidiCapacityContract::Unknown) == 256);
    juce::MidiBuffer replacement; dest.swapWith (replacement);
    CHECK (capacity.lowerBound (dest, luthier::MidiCapacityContract::Unknown) == 0);
    CHECK (capacity.lowerBound (replacement, luthier::MidiCapacityContract::Unknown) == 0);
    CHECK (capacity.lowerBound (dest, luthier::MidiCapacityContract::JuceWrapper) == 2048);
}

LUTHIER_TEST (RefPedalMidiCore, malformedDataAndSensingCannotStarveClock)
{
    MidiRouter router; router.prepare (48000, 512);
    auto map = std::make_unique<MapSnap>(); map->tempoSource = TempoSource::MidiClock;
    map->keyboardSlots = 16; Binding b; b.srcNum = 20; b.target = 0; map->add (b); router.publishMap (std::move (map));
    for (int i = 0; i < 256; ++i) router.pushUiEvent ({0,0x90,255,255});
    juce::MidiBuffer input; for (int i = 0; i < 600; ++i) input.addEvent (juce::MidiMessage (0xfe), 0);
    input.addEvent (juce::MidiMessage::midiStart(), 1); input.addEvent (juce::MidiMessage::midiClock(), 10);
    RouteOut out; router.process (input, {}, 0, 512, out);
    CHECK (out.nFoot == 0); CHECK (out.nWrites == 0); CHECK (out.nTr > 0); CHECK (out.tr[0] == TransportEvent::Start);
    CHECK (router.droppedCount() >= 256);
}
LUTHIER_TEST (RefPedalMidiCore, prepareClearsHighResolutionControllerHistory)
{
    MidiRouter router; router.prepare (48000, 64); auto map = std::make_unique<MapSnap>();
    Binding b; b.srcType = (uint8_t) MidiSrc::CC14; b.srcNum = 1; b.target = 0; map->add (b); router.publishMap (std::move (map));
    juce::MidiBuffer in; in.addEvent (juce::MidiMessage::controllerEvent (1,1,127), 0); RouteOut out;
    router.process (in, {}, 0,64,out); CHECK (out.nWrites == 1);
    router.prepare (96000,64); in.clear(); in.addEvent (juce::MidiMessage::controllerEvent (1,33,127),0);
    router.process (in, {},0,64,out); CHECK (out.nWrites == 0);
}

#include "../ReferencePedals/Midi/ExtendedHostAutomationCapture.h"
LUTHIER_TEST (RefPedalMidiCore, extendedAutomationPhysicalUnitsAndEchoIsolation)
{
    ExtendedHostAutomationCapture capture;
    CHECK (capture.write ((int) ExtendedHostField::FeedbackLpHz, .5f));
    CHECK (capture.write ((int) ExtendedHostField::Mode, .55f));
    auto snapshot = capture.snapshot();
    CHECK_NEAR (snapshot.values[(size_t) ExtendedHostField::FeedbackLpHz], 10010, 1e-3);
    CHECK (snapshot.values[(size_t) ExtendedHostField::Mode] == 2);
    { ExtendedHostAutomationCapture::EchoScope echo (capture); CHECK (! capture.write (0, 1)); }
    CHECK (capture.serial (0) == 0); CHECK (capture.write (0, 1)); CHECK (capture.value (0) == 2);
    CHECK (! capture.write (50, 1)); CHECK (! capture.write (0, std::numeric_limits<float>::infinity()));
}
LUTHIER_TEST (RefPedalMidiCore, virtualExpressionDeliversRealMappedWrites)
{
    MidiRouter router; router.prepare (48000, 128);
    auto makeMap = []
    {
        auto map = std::make_unique<MapSnap>();
        Binding b; b.srcType = (uint8_t) MidiSrc::VirtualExpression; b.srcNum = 1; b.target = 3; map->add (b);
        b.id = 1; b.target = kMaxParams + 2; map->add (b); return map;
    };
    router.publishMap (makeMap()); router.setVirtualExpression (1, .73f, 1);
    juce::MidiBuffer in; RouteOut out;
    const auto before = luthier::tests::allocationsOnThisThread(); router.process (in, {}, 0, 128, out);
    const auto after = luthier::tests::allocationsOnThisThread(); CHECK (before == after);
    CHECK (out.nWrites == 2); CHECK_NEAR (out.writes[0].norm, .73, 1e-6); CHECK (out.writes[0].offset == 0);
    router.process (in, {}, 128, 128, out); CHECK (out.nWrites == 0);
    in.addEvent (juce::MidiMessage::controllerEvent (1, 11, 127), 31); router.process (in, {}, 256, 128, out);
    CHECK (out.nWrites == 2); CHECK (out.writes[0].norm == 1.f); CHECK (out.writes[0].offset == 31);
    in.clear(); router.publishMap (makeMap()); router.process (in, {}, 384, 128, out);
    CHECK (out.nWrites == 2); CHECK (out.writes[0].norm == 1.f); // map reload preserves latest MIDI intent
    router.setVirtualExpression (1, .2f, UINT32_MAX); router.process (in, {}, 512,128,out); CHECK_NEAR (out.writes[0].norm,.2,1e-6);
    router.setVirtualExpression (1, .4f, 1); router.process (in, {},640,128,out); CHECK_NEAR (out.writes[0].norm,.4,1e-6);
    router.setHostEnabled (false, 1); router.setVirtualExpression (1, .6f, 2); router.process (in, {},768,128,out); CHECK (out.nWrites == 0);
    router.setHostEnabled (true, 2); router.process (in, {},896,128,out); CHECK (out.nWrites == 2); CHECK_NEAR (out.writes[0].norm,.6,1e-6);
}
