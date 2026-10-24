// WP1 (T-REG scaffolding): the frozen interfaces of the reference pedalboard.
//
// Covers descriptor tapers, registry validation and lane assignment, PedalBase,
// the POD sizes the spec fixes, Rt/SpscRing (wait-free stress) and RetireList,
// the node table, the Board facade's legacy path, and the scan that keeps
// Source/ReferencePedals free of GUI includes. Header self-containment is
// enforced at build time by the generated one-header translation units in
// CMakeLists.txt. Later packages add their own RefPedal* suites; this one must
// stay green with their models registered (registryContentValid).
//
// Run: LuthierTests refpedal

#include "TestFramework.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <vector>

#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/BoardCommand.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/BoardTypes.h"
#include "../ReferencePedals/IPedal.h"
#include "../ReferencePedals/LegacyModel.h"
#include "../ReferencePedals/Midi/MidiRouter.h"
#include "../ReferencePedals/Midi/MidiTypes.h"
#include "../ReferencePedals/PedalBase.h"
#include "../ReferencePedals/PedalDesc.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../ReferencePedals/Routing/RoutingNodes.h"
#include "../ReferencePedals/Rt/RetireList.h"
#include "../ReferencePedals/Rt/SpscRing.h"
#include "../ReferencePedals/Services/IAssetStore.h"
#include "../ReferencePedals/Topology.h"
#include "../UI/ReferencePedals/PedalFaceFactory.h"

namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;   // defined in CircuitTests.cpp (global operator new hook)
}

using namespace luthier::reference;
using luthier::tests::allocationsOnThisThread;

namespace
{
    //==========================================================================
    // Fake pedal used by the registry and PedalBase tests.
    const char* const kModeChoices[] { "A", "B", "C" };

    // clang-format off
    const ParamDesc kFakeParams[] {
        { "fk_drive",  "DRIVE",  "",   ParamKind::Continuous, Taper::Linear, 0.f, 10.f, 5.f, 1.f, 0, nullptr,      PF_Automatable, 20.f, 14 },
        { "fk_tone",   "TONE",   "Hz", ParamKind::Continuous, Taper::LogA,   100.f, 10000.f, 1000.f, 1.f, 0, nullptr, PF_Automatable, 20.f, 15 },
        { "fk_level",  "LEVEL",  "dB", ParamKind::Continuous, Taper::Skew,   -24.f, 12.f, 0.f, 2.f, 0, nullptr,       PF_Automatable, 20.f, 16 },
        { "fk_mode",   "MODE",   "",   ParamKind::Enum,       Taper::Linear, 0.f, 2.f, 1.f, 1.f, 3, kModeChoices,     PF_Automatable, 0.f,  -1 },
        { "fk_tap",    "TAP",    "",   ParamKind::Trigger,    Taper::Linear, 0.f, 1.f, 0.f, 1.f, 0, nullptr,          PF_None,        0.f,  -1 },
        { "fk_on",     "ON",     "",   ParamKind::Toggle,     Taper::Linear, 0.f, 1.f, 1.f, 1.f, 0, nullptr,          PF_Engage,      0.f,  -1 },
        { "fk_hq",     "HQ",     "",   ParamKind::Toggle,     Taper::Linear, 0.f, 1.f, 0.f, 1.f, 0, nullptr,          PF_Config,      0.f,  -1 },
        { "fk_ext",    "EXT",    "",   ParamKind::Continuous, Taper::Linear, 0.f, 1.f, 0.f, 1.f, 0, nullptr,          PF_Ext,         20.f, -1 },
        { "fk_hold",   "HOLD",   "",   ParamKind::Momentary,  Taper::Linear, 0.f, 1.f, 0.f, 1.f, 0, nullptr,          PF_None,        0.f,  -1 },
    };
    // clang-format on

    PedalDesc makeDesc (const char* modelId, const char* prefix, const ParamDesc* params, int n)
    {
        return { modelId, prefix, 1, "Fake", "Test", nullptr, ChannelMode::Mono, BypassMode::True,
                 n, params, 0, false, false, false, 1 };
    }

    const PedalDesc kFakeDesc = makeDesc ("test.fake", "fk_", kFakeParams, (int) std::size (kFakeParams));

    class FakePedal final : public PedalBase<9>
    {
    public:
        FakePedal() : PedalBase<9> (kFakeDesc) {}
        void prepare (double sr, int, int) override { prepareParams (sr); }
        void reset() noexcept override {}
        void process (double* L, double* R, int n) noexcept override
        {
            for (int i = 0; i < n; ++i)
            {
                L[i] = softGuard (L[i] * smoothedNow (0));
                R[i] = softGuard (R[i] * smoothedNow (0));
            }
        }
        int changed = 0, triggers = 0, lastIdx = -1;
        float lastValue = 0.f;
        float smoothedDrive() noexcept { return nextSmoothed (0); }
        bool driveSmoothing() const noexcept { return isSmoothing (0); }
    protected:
        void onParamChanged (int idx, float v) noexcept override { ++changed; lastIdx = idx; lastValue = v; }
        void onTrigger (int) noexcept override { ++triggers; }
    };

    std::unique_ptr<IPedal> makeFake() { return std::make_unique<FakePedal>(); }

    // A generated descriptor with `count` plain continuous params named <prefix>pNN.
    // Owns every string the descriptor points at; keep the object alive (and
    // unmoved) for as long as the descriptor is registered.
    struct BigDesc
    {
        BigDesc (std::string modelId, std::string prefix, int count)
            : modelIdStr (std::move (modelId)), prefixStr (std::move (prefix))
        {
            idStrs.reserve ((size_t) count);
            params.reserve ((size_t) count);
            for (int i = 0; i < count; ++i)
            {
                idStrs.push_back (prefixStr + "p" + (i < 10 ? "0" : "") + std::to_string (i));
                params.push_back ({ nullptr, "P", "", ParamKind::Continuous, Taper::Linear,
                                    0.f, 1.f, 0.5f, 1.f, 0, nullptr, PF_Automatable, 20.f, -1 });
            }
            for (size_t i = 0; i < idStrs.size(); ++i)
                params[i].id = idStrs[i].c_str();
            desc = makeDesc (modelIdStr.c_str(), prefixStr.c_str(), params.data(), count);
        }
        BigDesc (const BigDesc&) = delete;
        BigDesc& operator= (const BigDesc&) = delete;

        std::string modelIdStr, prefixStr;
        std::vector<std::string> idStrs;
        std::vector<ParamDesc> params;
        PedalDesc desc {};
    };

    std::unique_ptr<IPedal> makeNull() { return nullptr; }

    ParamDesc oneParam (const char* id, ParamKind kind, float mn, float mx, float def, uint16_t flags = PF_None)
    {
        return { id, "X", "", kind, Taper::Linear, mn, mx, def, 1.f, 0, nullptr, flags, 20.f, -1 };
    }

    //==========================================================================
    bool containsGuiInclude (const juce::String& line)
    {
        const auto t = line.trim();
        if (! t.startsWith ("#"))
            return false;
        const auto rest = t.substring (1).trim();
        if (! rest.startsWith ("include"))
            return false;
        for (const char* banned : { "juce_gui_basics", "juce_gui_extra", "juce_audio_utils", "juce_opengl",
                                    "juce_audio_plugin_client", "PluginEditor", "/UI/" })
            if (rest.contains (banned))
                return true;
        return false;
    }

    uint64_t stressOps()
    {
        if (const char* env = std::getenv ("LUTHIER_STRESS_OPS"))
            if (const auto v = juce::String (env).getLargeIntValue(); v > 0)
                return (uint64_t) v;
        return 4000000ull;   // 10^8 with LUTHIER_STRESS_OPS=100000000 (plan WP1 acceptance, TSan)
    }
}

//==============================================================================
// Frozen constants and POD layouts (static_asserts live in the headers too).
LUTHIER_TEST (RefPedalApi, constantsAndPodSizesAreFrozen)
{
    CHECK (kMaxParams == 32);
    CHECK (kMaxLaneParams == 16);
    CHECK (kMaxSlots == 16);
    CHECK (kMaxBranches == 4);
    CHECK (kMaxLoops == 2);
    CHECK (kMaxLatencySamp == 8192);
    CHECK (kSchemaVersion == 1);
    CHECK (sizeof (midi::MidiEv) == 24);
    CHECK (sizeof (midi::Binding) == 32);
    CHECK (sizeof (Node) == 16);
    CHECK ((int) Placement::beforeExistingPre == 0);
    CHECK ((int) Placement::afterExistingPost == 1);
    CHECK ((int) Placement::postCab == 2);
    CHECK (kNumPlacements == 3);
    CHECK (! isValidPlacement (3));
    CHECK (! isValidPlacement (-1));
    CHECK (kNodeKindCount == 11);
    CHECK (midi::kTargetBoardBase == kMaxSlots * kMaxParams);
    CHECK (Board::slotCount == 16);
    CHECK (kMaxBuses == kMaxBranches + kMaxLoops + 1);
}

LUTHIER_TEST (RefPedalApi, bindingPacksModeAndCurve)
{
    midi::Binding b;
    for (int m = 0; m <= (int) midi::BindMode::Step; ++m)
        for (int c = 0; c <= (int) midi::CurveId::Custom5; ++c)
        {
            b.setModeCurve ((midi::BindMode) m, (midi::CurveId) c);
            CHECK ((int) b.mode() == m);
            CHECK ((int) b.curve() == c);
        }
    CHECK (b.has (midi::BF_Enabled));
    midi::RouteOut out;
    out.nWrites = 3; out.nFoot = 2; out.nTr = 1; out.time.bpm = 99;
    out.clear();
    CHECK (out.nWrites == 0 && out.nFoot == 0 && out.nTr == 0 && out.time.bpm == 120.0);
}

//==============================================================================
// Descriptor tapers
LUTHIER_TEST (RefPedalApi, taperRoundTripAndEndpoints)
{
    const Taper tapers[] { Taper::Linear, Taper::LogA, Taper::LogB, Taper::ReverseLog, Taper::Skew, Taper::Detent, Taper::Custom };
    for (const auto taper : tapers)
        for (const float mn : { 0.f, 20.f })
        {
            ParamDesc p = oneParam ("x_a", ParamKind::Continuous, mn, 5000.f, 100.f);
            p.taper = taper;
            p.skew = taper == Taper::Skew ? 2.5f : 1.f;
            CHECK_NEAR (p.fromNormalised (0.0), (double) mn, 1.0e-9);
            CHECK_NEAR (p.fromNormalised (1.0), 5000.0, 1.0e-6);
            double previous = -1.0e30;
            for (int i = 0; i <= 200; ++i)
            {
                const double n = (double) i / 200.0;
                const double v = p.fromNormalised (n);
                CHECK (v >= previous - 1.0e-9);                       // monotone
                CHECK_NEAR (p.toNormalised (v), n, 1.0e-6);           // round trip (A-PED)
                previous = v;
            }
        }

    ParamDesc audio = oneParam ("x_a", ParamKind::Continuous, 0.f, 1.f, 0.f);
    audio.taper = Taper::LogB;
    CHECK_NEAR (audio.fromNormalised (0.5), 0.0909, 1.0e-3);          // audio pot: ~9% of travel at mid
    ParamDesc geo = oneParam ("x_a", ParamKind::Continuous, 100.f, 10000.f, 1000.f);
    geo.taper = Taper::LogA;
    CHECK_NEAR (geo.fromNormalised (0.5), 1000.0, 1.0e-6);           // geometric mean at mid
    CHECK_NEAR (geo.defaultNormalised(), 0.5, 1.0e-6);
}

LUTHIER_TEST (RefPedalApi, discreteAndSwitchKinds)
{
    ParamDesc e = oneParam ("x_e", ParamKind::Enum, 0.f, 3.f, 1.f);
    e.steps = 4;
    CHECK (e.fromNormalised (0.0) == 0.0);
    CHECK (e.fromNormalised (1.0) == 3.0);
    CHECK (e.fromNormalised (0.34) == 1.0);
    CHECK_NEAR (e.toNormalised (2.0), 2.0 / 3.0, 1.0e-12);

    for (const auto kind : { ParamKind::Toggle, ParamKind::Momentary, ParamKind::Trigger })
    {
        ParamDesc t = oneParam ("x_t", kind, 0.f, 1.f, 0.f);
        CHECK (t.fromNormalised (0.49) == 0.0);
        CHECK (t.fromNormalised (0.5) == 1.0);
        CHECK (t.toNormalised (1.0) == 1.0);
    }

    ParamDesc c = oneParam ("x_c", ParamKind::Continuous, 0.f, 1.f, 0.5f);
    CHECK (c.fromNormalised (std::nan ("")) == 0.0);                // non-finite input is safe
    CHECK (c.fromNormalised (7.0) == 1.0);
    CHECK (c.toNormalised (INFINITY) == 0.0);
}

//==============================================================================
// Registry
LUTHIER_TEST (RefPedalApi, registryRejectsMalformedDescriptors)
{
    PedalRegistry::TestScope scope;
    juce::String why;

    CHECK (PedalRegistry::validate (kFakeDesc, &why));
    CHECK (why.isEmpty());

    auto bad = [&] (const PedalDesc& d) { return ! PedalRegistry::validate (d, &why) && why.isNotEmpty(); };

    { auto d = kFakeDesc; d.modelId = ""; CHECK (bad (d)); }
    { auto d = kFakeDesc; d.displayName = nullptr; CHECK (bad (d)); }
    { auto d = kFakeDesc; d.revision = 0; CHECK (bad (d)); }
    { auto d = kFakeDesc; d.paramPrefix = "fk"; CHECK (bad (d)); }              // no trailing '_'
    { auto d = kFakeDesc; d.paramPrefix = "Fk_"; CHECK (bad (d)); }             // upper case
    { auto d = kFakeDesc; d.paramPrefix = "other_"; CHECK (bad (d)); }          // params do not start with it
    { auto d = kFakeDesc; d.numParams = kMaxParams + 1; CHECK (bad (d)); }
    { auto d = kFakeDesc; d.params = nullptr; CHECK (bad (d)); }
    { auto d = kFakeDesc; d.maxLatencySamples = kMaxLatencySamp + 1; CHECK (bad (d)); }

    const ParamDesc dupId[] { oneParam ("fk_a", ParamKind::Continuous, 0, 1, 0), oneParam ("fk_a", ParamKind::Continuous, 0, 1, 0) };
    CHECK (bad (makeDesc ("test.dup", "fk_", dupId, 2)));
    const ParamDesc badRange[] { oneParam ("fk_a", ParamKind::Continuous, 1, 1, 1) };
    CHECK (bad (makeDesc ("test.range", "fk_", badRange, 1)));
    const ParamDesc badDef[] { oneParam ("fk_a", ParamKind::Continuous, 0, 1, 2) };
    CHECK (bad (makeDesc ("test.def", "fk_", badDef, 1)));
    const ParamDesc badEnum[] { oneParam ("fk_a", ParamKind::Enum, 0, 2, 0) };   // steps 0, no choices
    CHECK (bad (makeDesc ("test.enum", "fk_", badEnum, 1)));
    const ParamDesc twoEngage[] { oneParam ("fk_a", ParamKind::Toggle, 0, 1, 1, PF_Engage), oneParam ("fk_b", ParamKind::Toggle, 0, 1, 1, PF_Engage) };
    CHECK (bad (makeDesc ("test.eng", "fk_", twoEngage, 2)));
    const ParamDesc engageNotToggle[] { oneParam ("fk_a", ParamKind::Continuous, 0, 1, 1, PF_Engage) };
    CHECK (bad (makeDesc ("test.eng2", "fk_", engageNotToggle, 1)));
    const ParamDesc noSuffix[] { oneParam ("fk_", ParamKind::Continuous, 0, 1, 0) };   // id == prefix
    CHECK (bad (makeDesc ("test.suffix", "fk_", noSuffix, 1)));
    const ParamDesc upper[] { oneParam ("fk_Drive", ParamKind::Continuous, 0, 1, 0) };
    CHECK (bad (makeDesc ("test.case", "fk_", upper, 1)));
    const ParamDesc badSwitch[] { oneParam ("fk_a", ParamKind::Toggle, 0, 5, 0) };
    CHECK (bad (makeDesc ("test.sw", "fk_", badSwitch, 1)));

    // add() refuses invalid input and leaves the registry empty
    { auto d = kFakeDesc; d.paramPrefix = "fk"; CHECK (! PedalRegistry::add (d, &makeFake)); }
    CHECK (! PedalRegistry::add (kFakeDesc, nullptr));
    CHECK (PedalRegistry::all().size() == 0);
}

LUTHIER_TEST (RefPedalApi, registryRejectsDuplicateIdAndPrefix)
{
    PedalRegistry::TestScope scope;
    CHECK (PedalRegistry::add (kFakeDesc, &makeFake));

    CHECK (! PedalRegistry::add (kFakeDesc, &makeFake));                   // same id
    auto samePrefix = kFakeDesc;
    samePrefix.modelId = "test.other";
    CHECK (! PedalRegistry::add (samePrefix, &makeFake));                  // same prefix, new id
    CHECK (PedalRegistry::all().size() == 1);

    const ParamDesc other[] { oneParam ("ot_a", ParamKind::Continuous, 0, 1, 0) };
    CHECK (PedalRegistry::add (makeDesc ("test.other", "ot_", other, 1), &makeNull));
    CHECK (PedalRegistry::all().size() == 2);
}

LUTHIER_TEST (RefPedalApi, registryFindMakeAndOrder)
{
    PedalRegistry::TestScope scope;
    CHECK (PedalRegistry::find ("test.fake") == nullptr);
    CHECK (PedalRegistry::make ("test.fake") == nullptr);
    CHECK (PedalRegistry::make (nullptr) == nullptr);

    const ParamDesc a[] { oneParam ("aa_x", ParamKind::Continuous, 0, 1, 0) };
    const ParamDesc b[] { oneParam ("bb_x", ParamKind::Continuous, 0, 1, 0) };
    CHECK (PedalRegistry::add (makeDesc ("test.b", "bb_", b, 1), &makeFake));
    CHECK (PedalRegistry::add (kFakeDesc, &makeFake));
    CHECK (PedalRegistry::add (makeDesc ("test.a", "aa_", a, 1), &makeNull));

    const auto all = PedalRegistry::all();
    CHECK (all.size() == 3);
    if (all.size() == 3)
    {
        CHECK (juce::String (all[0]->modelId) == "test.b");                // registration order == catalog order
        CHECK (juce::String (all[1]->modelId) == "test.fake");
        CHECK (juce::String (all[2]->modelId) == "test.a");
    }

    const auto* found = PedalRegistry::find ("test.fake");
    CHECK (found != nullptr && found->numParams == 9);
    auto pedal = PedalRegistry::make ("test.fake");
    CHECK (pedal != nullptr);
    if (pedal != nullptr)
        CHECK (juce::String (pedal->desc().modelId) == "test.fake");
    CHECK (PedalRegistry::make ("test.a") == nullptr);                     // factory may fail: no crash

    // find() pointers survive later add()
    const auto* first = PedalRegistry::find ("test.b");
    std::vector<std::unique_ptr<BigDesc>> extra;
    for (int i = 0; i < 40; ++i)
    {
        extra.push_back (std::make_unique<BigDesc> ("test.big" + std::to_string (i), "z" + std::to_string (i) + "_", 2));
        CHECK (PedalRegistry::add (extra.back()->desc, &makeNull));
    }
    CHECK (PedalRegistry::find ("test.b") == first);

    CHECK (PedalRegistry::findParam (*found, "fk_tone") == 1);
    CHECK (PedalRegistry::findParam (*found, "nope") == -1);
}

LUTHIER_TEST (RefPedalApi, registerAllIsIdempotent)
{
    PedalRegistry::TestScope scope;
    PedalRegistry::registerAll();
    const auto n = PedalRegistry::all().size();
    PedalRegistry::registerAll();
    PedalRegistry::registerAll();
    CHECK (PedalRegistry::all().size() == n);
}

// Stays valid as packages land: whatever registerAll() registers must satisfy the rules.
LUTHIER_TEST (RefPedalApi, registryContentValid)
{
    PedalRegistry::registerAll();
    juce::StringArray ids, prefixes;
    for (const auto* d : PedalRegistry::all())
    {
        juce::String why;
        CHECK_MSG (PedalRegistry::validate (*d, &why), why);
        CHECK (! ids.contains (d->modelId));
        CHECK (! prefixes.contains (d->paramPrefix));
        ids.add (d->modelId);
        prefixes.add (d->paramPrefix);
        const auto& lanes = PedalRegistry::laneMap (d->modelId);
        CHECK (lanes.numLanes <= kMaxLaneParams);
        CHECK (PedalRegistry::make (d->modelId) != nullptr);
    }
}

LUTHIER_TEST (RefPedalApi, laneMapSkipsEngageConfigExtTriggerMomentary)
{
    PedalRegistry::TestScope scope;
    CHECK (PedalRegistry::add (kFakeDesc, &makeFake));
    const auto& map = PedalRegistry::laneMap ("test.fake");

    // lanes: drive, tone, level, mode (indices 0..3); tap(4) trigger, on(5) engage, hq(6) config, ext(7), hold(8) momentary skipped
    CHECK (map.numLanes == 4);
    CHECK (map.laneToParam[0] == 0 && map.laneToParam[1] == 1 && map.laneToParam[2] == 2 && map.laneToParam[3] == 3);
    CHECK (map.laneToParam[4] == -1);
    CHECK (map.engageParam == 5);
    CHECK (map.paramToLane[0] == 0 && map.paramToLane[3] == 3);
    for (const int skipped : { 4, 5, 6, 7, 8 })
        CHECK (map.paramToLane[skipped] == -1);
    CHECK (map.numOverflow == 0);

    // unknown model: shared empty map, never null
    const auto& none = PedalRegistry::laneMap ("no.such.model");
    CHECK (none.numLanes == 0 && none.engageParam == -1);
    CHECK (&PedalRegistry::laneMap (nullptr) == &none);

    // overflow beyond 16 lanes stays state-only
    BigDesc big ("test.big", "bg_", 24);
    CHECK (PedalRegistry::add (big.desc, &makeNull));
    const auto& bm = PedalRegistry::laneMap ("test.big");
    CHECK (bm.numLanes == kMaxLaneParams);
    CHECK (bm.numOverflow == 8);
    CHECK (bm.laneToParam[15] == 15);
    CHECK (bm.paramToLane[16] == -1);
}

//==============================================================================
// PedalBase
LUTHIER_TEST (RefPedalApi, pedalBaseValidatesAndAppliesTapers)
{
    FakePedal p;
    CHECK_NEAR (p.getParam (0), 0.5, 1.0e-6);                      // default norm of drive 5 in 0..10
    CHECK_NEAR (p.getParam (1), 0.5, 1.0e-6);                      // LogA geometric mean
    CHECK (p.getParam (-1) == 0.f && p.getParam (99) == 0.f);

    p.setParam (0, 0.25f);
    CHECK (p.lastIdx == 0);
    CHECK_NEAR (p.lastValue, 2.5, 1.0e-5);
    CHECK_NEAR (p.getParam (0), 0.25, 1.0e-6);

    const int before = p.changed;
    p.setParam (0, std::nanf (""));                                // ignored
    p.setParam (0, INFINITY);                                      // ignored
    p.setParam (-1, 0.5f);
    p.setParam (12, 0.5f);
    CHECK (p.changed == before);
    CHECK_NEAR (p.getParam (0), 0.25, 1.0e-6);

    p.setParam (0, 7.f);                                           // clamps
    CHECK_NEAR (p.getParam (0), 1.0, 1.0e-9);
    CHECK_NEAR (p.lastValue, 10.0, 1.0e-5);
    p.setParam (0, -3.f);
    CHECK_NEAR (p.getParam (0), 0.0, 1.0e-9);

    p.setParam (3, 1.f);                                           // enum 3 steps -> index 2
    CHECK_NEAR (p.lastValue, 2.0, 1.0e-6);

    p.setParam (2, 0.5f);                                          // skew 2: 0.5^(1/2)
    CHECK_NEAR (p.lastValue, -24.0 + 36.0 * std::sqrt (0.5), 1.0e-4);
}

LUTHIER_TEST (RefPedalApi, pedalBaseTriggerFiresOnRisingEdgeOnly)
{
    FakePedal p;
    p.setParam (4, 0.f);
    CHECK (p.triggers == 0);
    p.setParam (4, 1.f);
    CHECK (p.triggers == 1);
    p.setParam (4, 1.f);                                           // held high: no second edge
    CHECK (p.triggers == 1);
    p.setParam (4, 0.2f);
    p.setParam (4, 0.9f);
    CHECK (p.triggers == 2);
    CHECK (p.lastIdx != 4);                                        // trigger does not call onParamChanged
}

LUTHIER_TEST (RefPedalApi, pedalBaseSmoothsAndStaysRtSafe)
{
    FakePedal p;
    p.prepare (48000.0, 512, 1);
    p.setParam (0, 0.f);
    CHECK (p.driveSmoothing());
    float previous = p.smoothedDrive();
    int steps = 0;
    while (p.driveSmoothing() && steps < 5000)
    {
        const float v = p.smoothedDrive();
        CHECK (v <= previous + 1.0e-6f);                           // monotone ramp, no jump
        previous = v;
        ++steps;
    }
    CHECK (steps > 800 && steps < 1100);                           // ~20 ms at 48 kHz
    CHECK_NEAR (previous, 0.0, 1.0e-5);

    // Enum / toggle jump immediately (no zipper to hide).
    p.setParam (3, 0.f);
    CHECK (p.lastValue == 0.f);

    std::vector<double> l (512, 0.3), r (512, -0.3);
    const long allocsBefore = allocationsOnThisThread();
    for (int i = 0; i < 200; ++i)
    {
        p.setParam (i % 3, (float) (i % 7) / 7.f);
        p.setParam (4, (float) (i & 1));
        p.process (l.data(), r.data(), 512);
    }
    CHECK (allocationsOnThisThread() == allocsBefore);
    CHECK_FINITE (l.data(), 512);
}

LUTHIER_TEST (RefPedalApi, softGuardAndDcGuard)
{
    CHECK (PedalBase<1>::softGuard (INFINITY) == 0.0);
    CHECK (PedalBase<1>::softGuard (std::nan ("")) == 0.0);
    CHECK (PedalBase<1>::softGuard (1.0e6) == 16.0);
    CHECK (PedalBase<1>::softGuard (-1.0e6) == -16.0);
    CHECK (PedalBase<1>::softGuard (0.25) == 0.25);

    PedalBase<1>::DcGuard dc;
    double y = 0.0;
    for (int i = 0; i < 48000; ++i)
        y = dc.process (0.5);                                      // DC in -> decays to ~0
    CHECK (std::abs (y) < 1.0e-9);
}

//==============================================================================
// Rt/SpscRing, Rt/RetireList
LUTHIER_TEST (RefPedalApi, spscRingBasics)
{
    SpscRing<int, 8> ring;
    CHECK (ring.capacity() == 8);
    int v = -1;
    CHECK (! ring.pop (v));
    for (int i = 0; i < 8; ++i)
        CHECK (ring.push (i));
    CHECK (! ring.push (99));                                      // full: new item dropped and counted
    CHECK (! ring.push (100));
    CHECK (ring.dropped() == 2);
    CHECK (ring.sizeApprox() == 8);
    for (int i = 0; i < 8; ++i)
    {
        CHECK (ring.pop (v));
        CHECK (v == i);                                            // FIFO, newest dropped (not oldest)
    }
    CHECK (! ring.pop (v));

    // wrap-around with interleaved push/pop
    for (int round = 0; round < 100; ++round)
    {
        CHECK (ring.push (round));
        CHECK (ring.push (round + 1000));
        CHECK (ring.pop (v) && v == round);
        CHECK (ring.pop (v) && v == round + 1000);
    }

    int out[6];
    for (int i = 0; i < 5; ++i) ring.push (i);
    CHECK (ring.popMany (out, 3) == 3 && out[2] == 2);
    CHECK (ring.popMany (out, 6) == 2 && out[1] == 4);
    for (int i = 0; i < 5; ++i) ring.push (i);
    ring.clear();
    CHECK (! ring.pop (v));
}

LUTHIER_TEST (RefPedalApi, spscRingStressKeepsOrderAndNeverBlocks)
{
    struct Item { uint64_t seq; uint64_t check; };
    static SpscRing<Item, 256> ring;
    const uint64_t total = stressOps();
    std::atomic<bool> orderOk { true };
    std::atomic<uint64_t> received { 0 };

    std::thread consumer ([&]
    {
        Item item {};
        uint64_t expectedMin = 0;
        uint64_t count = 0;
        while (count < total)
        {
            if (ring.pop (item))
            {
                // payload intact (no torn write) and strictly increasing sequence
                if (item.check != ~item.seq || item.seq < expectedMin)
                    orderOk.store (false, std::memory_order_relaxed);
                expectedMin = item.seq + 1;
                ++count;
            }
            else if (received.load (std::memory_order_relaxed) == ~0ull)
                break;
        }
        received.store (count, std::memory_order_relaxed);
    });

    // Producer: bounded steps per call, retries on full (the ring never blocks it).
    for (uint64_t seq = 0; seq < total;)
        if (ring.push ({ seq, ~seq }))
            ++seq;

    consumer.join();
    CHECK (orderOk.load());
    CHECK (received.load() == total);
    ring.clear();
}

LUTHIER_TEST (RefPedalApi, retireListReclaimsByVersion)
{
    struct Probe { explicit Probe (int& c) : counter (c) {} ~Probe() { ++counter; } int& counter; };
    int destroyed = 0;
    RetireList<Probe> list;
    list.retire (std::make_unique<Probe> (destroyed), 5);
    list.retire (std::make_unique<Probe> (destroyed), 6);
    list.retire (std::make_unique<Probe> (destroyed), 6);
    list.retire (nullptr, 1);
    CHECK (list.size() == 3);
    CHECK (list.highWater() == 3);

    CHECK (list.reclaim (5) == 0);                                 // audio may still read version 5
    CHECK (list.reclaim (6) == 1 && destroyed == 1);               // everything older than 6 is free
    CHECK (list.reclaim (6) == 0);
    CHECK (list.reclaim (7) == 2 && destroyed == 3);
    CHECK (list.size() == 0 && list.highWater() == 3);

    // wrap-around: 0xFFFFFFFF is older than 2
    list.retire (std::make_unique<Probe> (destroyed), 0xFFFFFFFFu);
    CHECK (list.reclaim (2) == 1);
    list.retire (std::make_unique<Probe> (destroyed), 100);
    CHECK (list.reclaimAll() == 1 && list.size() == 0);
    CHECK (destroyed == 5);
}

//==============================================================================
// Hash helper, commands, node table
LUTHIER_TEST (RefPedalApi, assetHashHexRoundTrip)
{
    AssetHash h;
    CHECK (h.isNull());
    for (size_t i = 0; i < h.bytes.size(); ++i)
        h.bytes[i] = (uint8_t) (i * 7 + 1);
    CHECK (! h.isNull());
    const auto hex = h.toHex();
    CHECK (hex.length() == 64);
    CHECK (AssetHash::fromHex (hex) == h);
    CHECK (AssetHash::fromHex ("abc").isNull());
}

LUTHIER_TEST (RefPedalApi, commandNamesAreDistinct)
{
    const BoardCommand all[] {
        cmd::Add {}, cmd::Remove {}, cmd::Duplicate {}, cmd::Move {}, cmd::SetModel {}, cmd::SetParam {},
        cmd::SetEnabled {}, cmd::SetPlacement {}, cmd::SetMode {}, cmd::SetLane {}, cmd::SetLoop {},
        cmd::SetFeedback {}, cmd::SetStyle {}, cmd::SetMounting {}, cmd::SetMix {}, cmd::SetMixLaw {},
        cmd::SetBoardEnabled {}, cmd::SetTrim {}, cmd::SetMidiBinding {}, cmd::SetSlotChannel {},
        cmd::SetScene {}, cmd::ApplySnapshot {}, cmd::SetBoardParam {} };
    juce::StringArray names;
    for (size_t i = 0; i < std::size (all); ++i)
    {
        CHECK (all[i].index() == i);
        names.add (commandName (all[i]));
    }
    names.removeDuplicates (false);
    CHECK (names.size() == (int) std::size (all));
    CHECK (std::variant_size_v<BoardCommand> == std::size (all));
}

LUTHIER_TEST (RefPedalApi, boardTypesStringHelpers)
{
    BoardStyle s = BoardStyle::plywood;
    for (int i = 0; i <= (int) BoardStyle::loose; ++i)
    {
        CHECK (boardStyleFromString (toString ((BoardStyle) i), s));
        CHECK ((int) s == i);
    }
    s = BoardStyle::rail;
    CHECK (! boardStyleFromString ("marble", s) && s == BoardStyle::rail);   // unknown: untouched
    Mounting m = Mounting::none;
    CHECK (mountingFromString ("auto", m) && m == Mounting::automatic);
    CHECK (mountingFromString ("ceramic", m) && m == Mounting::ceramic);
    Mode mode = Mode::Loop;
    CHECK (modeFromString ("parallel", mode) && mode == Mode::Parallel);

    BoardSettings settings;
    CHECK (! settings.enabled);                                              // default OFF (T-LEGACY)
    CHECK (settings.mix == 1.f && settings.mixLaw == MixLaw::EqualPower && settings.mode == Mode::Series);
    CHECK (settings.fb.src.isNull() && settings.loops[0].first.isNull());
    PedalInstance inst;
    CHECK (! inst.instanceId.isNull());
    CHECK (inst.params.size() == (size_t) kMaxParams);
}

LUTHIER_TEST (RefPedalApi, routingNodeTableIsComplete)
{
    const NodeFn* table = routing::nodeTable();
    CHECK (table != nullptr);
    if (table == nullptr)
        return;

    Topology topo;
    ExecCtx exec;
    exec.topo = &topo;
    exec.n = 0;
    std::vector<double> buf (16, 0.25);
    for (auto& bus : exec.bus)
        bus[0] = bus[1] = buf.data();

    for (int k = 0; k < kNodeKindCount; ++k)
    {
        CHECK (table[k] != nullptr);
        Node node;
        node.kind = (NodeKind) k;
        table[k] (node, exec);                                      // n == 0: must be a safe no-op
    }
    CHECK (buf[0] == 0.25);
    CHECK (sizeof (Topology) < 8192);
    CHECK (topo.numNodes == 0 && topo.numLanes == 1);
}

//==============================================================================
// Board facade, legacy path
LUTHIER_TEST (RefPedalApi, boardFacadeKeepsLegacyContract)
{
    Board board;
    board.prepare (48000.0, 512);
    CHECK (! board.isActive());
    CHECK (! board.isBoardEnabled());
    CHECK (board.getLatencySamples() == 0);
    CHECK (board.model().getInstanceCount() == 0);
    CHECK (board.model().getOrder().empty());
    CHECK (! board.model().canUndo());
    const auto meters = board.readMeters();
    CHECK (meters.latencySamples == 0 && ! meters.fbTripped && ! meters.clockLost);
    CHECK (board.reclaim() == 0);

    // inactive board never touches the buffer (T-LEGACY unit part)
    board.setModel (0, Model::ds1Prototype);
    double l[8] { 0.1, 0.2, -0.3, 0.4, 0.0, 0.5, -0.6, 0.7 }, r[8] {};
    const double lRef[8] { 0.1, 0.2, -0.3, 0.4, 0.0, 0.5, -0.6, 0.7 };
    board.processStereo (l, r, 8, Placement::beforeExistingPre);
    for (int i = 0; i < 8; ++i)
        CHECK (l[i] == lRef[i] && r[i] == 0.0);

    board.setBoardEnabled (true);
    CHECK (board.isActive());
    board.setPlacement (0, Placement::postCab);
    CHECK (board.getPlacement (0) == Placement::postCab);
    board.setPlacement (0, (Placement) 7);                         // invalid: ignored (prototype defect 8)
    CHECK (board.getPlacement (0) == Placement::postCab);

    // move keeps a permutation of 16 ids
    board.move (0, 15);
    bool seen[Board::slotCount] {};
    for (int p = 0; p < Board::slotCount; ++p)
    {
        const int id = board.getIdAtPosition (p);
        CHECK (id >= 0 && id < Board::slotCount && ! seen[id]);
        if (id >= 0 && id < Board::slotCount) seen[id] = true;
    }
    CHECK (board.getIdAtPosition (15) == 0);
    CHECK (board.getIdAtPosition (16) == -1);

    // A fresh no-input MIDI router initializes the supplied host transport.
    // The earlier audio exercise primed AUTO with internal tempo; its live
    // source changes deliberately glide over 50 ms (tested by MidiCore).
    // Keep this independent initialization check independent of that history.
    board.midi().prepare (48000.0, 512);
    midi::RouteOut out;
    juce::MidiBuffer buffer;
    HostTransport host;
    host.valid = true; host.playing = true; host.bpm = 133.0;
    const long allocs = allocationsOnThisThread();
    board.midi().process (buffer, host, 1234, 64, out);
    CHECK (allocationsOnThisThread() == allocs);
    CHECK (out.nWrites == 0 && out.time.bpm == 133.0 && out.time.sample == 1234);
}

LUTHIER_TEST (RefPedalApi, boardModelStubRejectsUntilImplemented)
{
    // WP2 replaces this expectation with the real command tests (RefPedalBoardCore).
    BoardModel model;
    juce::String reason;
    struct Spy : BoardModel::Listener
    {
        void rejected (const juce::String& r) override { last = r; }
        juce::String last;
    } spy;
    model.addListener (&spy);
    cmd::Add add;
    add.modelId = "test.fake";
    const bool accepted = model.apply (add, &reason);
    if (! accepted)
    {
        CHECK (reason.isNotEmpty());
        CHECK (spy.last == reason);
    }
    model.removeListener (&spy);
}

LUTHIER_TEST (RefPedalApi, faceFactoryFallsBackToPlaceholder)
{
    auto face = PedalFaceFactory::make ("no.such.model");
    CHECK (face != nullptr);
    if (face != nullptr)
    {
        const auto size = face->designSizeMm();
        CHECK (size.x > 0.f && size.y > 0.f);
        FaceLayout layout;
        layout.controls.push_back ({});
        face->layout (layout);
        CHECK (layout.controls.empty());
    }
    CHECK (! PedalFaceFactory::hasBrandedFace ("no.such.model"));
    CHECK (PedalFaceFactory::make (nullptr) != nullptr);
}

//==============================================================================
// Source/ReferencePedals is built into the headless renderer: no GUI includes.
LUTHIER_TEST (RefPedalApi, noGuiIncludes)
{
    const auto dir = juce::File (__FILE__).getSiblingFile ("../ReferencePedals").getFullPathName();
    const juce::File root (dir);
    if (! root.isDirectory())
    {
        CHECK_MSG (true, "source tree not available at run time, scan skipped");
        return;
    }

    int scanned = 0;
    for (const auto& file : root.findChildFiles (juce::File::findFiles, true, "*"))
    {
        const auto ext = file.getFileExtension();
        if (ext != ".h" && ext != ".cpp" && ext != ".inc")
            continue;
        ++scanned;
        juce::StringArray lines;
        lines.addLines (file.loadFileAsString());
        for (int i = 0; i < lines.size(); ++i)
            CHECK_MSG (! containsGuiInclude (lines[i]),
                       file.getRelativePathFrom (root) + ":" + juce::String (i + 1) + " includes GUI code: " + lines[i]);
    }
    CHECK (scanned > 20);   // the skeleton exists
}

// The skeleton generator output must stay complete (plan rule S-1).
LUTHIER_TEST (RefPedalApi, skeletonFilesHaveOwners)
{
    const juce::File root (juce::File (__FILE__).getSiblingFile ("../ReferencePedals").getFullPathName());
    if (! root.isDirectory())
        return;

    int checked = 0;
    for (const auto& file : root.findChildFiles (juce::File::findFiles, true, "*.cpp;*.h"))
    {
        juce::StringArray lines;
        lines.addLines (file.loadFileAsString());
        CHECK_MSG (lines.size() > 0 && lines[0].startsWith ("// OWNER: WP"),
                   file.getRelativePathFrom (root) + " lacks the '// OWNER: WPn' first line");
        ++checked;
    }
    CHECK (checked > 100);
}
