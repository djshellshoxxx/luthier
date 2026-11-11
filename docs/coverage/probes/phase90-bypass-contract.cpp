// Luthier acceptance audit, 2026-10-26. Adapted from the retained in-project
// 2026-10-25 phase90-bypass-contract probe; provenance is in the QA handoff.
// Uses the real model, runtime and node; does not register a production model.
// The two conflicting written fade laws and literal 24-bit criterion remain
// separate assertions. A RED exit is intentional evidence, never a test pass.
#include "Source/ReferencePedals/Models/mxr_phase90_script.h"
#include "Source/ReferencePedals/BoardEngine.h"
#include "Source/ReferencePedals/Routing/RoutingNodes.h"
#include <array>
#include <cstdio>

using namespace luthier::reference;
using models::MxrPhase90Script;
namespace
{
constexpr double pi = 3.1415926535897932384626433832795;
constexpr double lsb24 = 1.0 / 8388608.0; // Signed full-scale 24-bit PCM.
double equalPower(double dry, double wet, double amount)
{
    if (amount <= 0) return dry;
    if (amount >= 1) return wet;
    return dry * std::cos(amount * pi / 2) + wet * std::sin(amount * pi / 2);
}
struct Result
{
    double directEqualError = 0, nodeEqualError = 0, nodeSingleLinearError = 0;
    double directLinearError = 0, nodeSquaredLinearError = 0;
    double settledNull = 0, settledWetError = 0, rawDelta = 0, dryRawDelta = 0;
    double idealEqualGainStep = 0, actualAddedGainStep = 0;
    double preEventOracleError = 0;
    double phaseBefore = 0, phaseAfter = 0;
    int endpoint = -1;
    bool finite = true, exactNull = true;
};
Result audit(double rate, int hostBlock, bool enabling)
{
    MxrPhase90Script direct, wet;
    PedalRuntime runtime;
    runtime.pedal = std::make_unique<MxrPhase90Script>();
    auto& model = static_cast<MxrPhase90Script&>(*runtime.pedal);
    for (auto* p : { &direct, &wet, &model })
    {
        // Zero sweep removes clock differences from the independent wet oracle.
        // All four actual nonlinear stages and stereo/filter histories remain.
        p->setEnabled(p == &wet || !enabling);
        p->setParam(3, 0); p->setParam(5, 0);
        p->prepare(rate, 2048, 1);
    }
    runtime.enabled = !enabling;
    runtime.enableMix = enabling ? 0 : 1;
    runtime.bypassSamples = rate * .005;
    for (auto& line : runtime.dry) line.prepare(0);
    Topology topology;
    PedalNodeState state; state.runtime = &runtime;
    ExecCtx execution;
    execution.topo = &topology; execution.state[0] = &state;
    execution.sampleRate = rate;
    Node node; node.lane = 0; node.slot = 0;
    std::array<std::array<double, kSubBlock>, 2> input{}, d{}, w{}, b{};
    Result result;
    double previousOutput[2]{}, previousDry[2]{};
    const int event = int(rate / 4), fadeSamples = int(std::ceil(rate * .005));
    const int end = event + fadeSamples + 64;
    int frame = 0;
    while (frame < end)
    {
        if (frame == event)
        {
            result.phaseBefore = model.modulationPhase();
            direct.setEnabled(enabling); model.setEnabled(enabling);
            runtime.enabled = enabling;
        }
        // BoardEngine splits host blocks into <=64-sample executions and at
        // scheduled events. Exercise that actual node boundary for each size.
        int n = std::min({ kSubBlock, hostBlock - frame % hostBlock, end - frame });
        if (frame < event) n = std::min(n, event - frame);
        for (int i = 0; i < n; ++i)
        {
            input[0][i] = .1 * std::sin(2 * pi * 1000 * (frame+i) / rate + .37);
            input[1][i] = .15 * std::cos(2 * pi * 731 * (frame+i) / rate + .13);
            for (int c = 0; c < 2; ++c) d[c][i] = w[c][i] = b[c][i] = input[c][i];
        }
        direct.process(d[0].data(), d[1].data(), n);
        wet.process(w[0].data(), w[1].data(), n);
        execution.n = n; execution.bus[0][0] = b[0].data(); execution.bus[0][1] = b[1].data();
        routing::nodePedal(node, execution);
        for (int i = 0; i < n; ++i)
        {
            const int since = frame + i - event;
            if (since >= 0)
            {
                const double t = std::min(1., (since+1) / (rate * .005));
                const double priorT = std::min(1., since / (rate * .005));
                const double amount = enabling ? t : 1-t, priorAmount = enabling ? priorT : 1-priorT;
                for (int c = 0; c < 2; ++c)
                {
                    const double dry = input[c][i], active = w[c][i];
                    const double expected = equalPower(dry, active, amount);
                    result.directEqualError = std::max(result.directEqualError, std::abs(d[c][i]-expected));
                    result.nodeEqualError = std::max(result.nodeEqualError, std::abs(b[c][i]-expected));
                    result.nodeSingleLinearError = std::max(result.nodeSingleLinearError, std::abs(b[c][i]-(dry+amount*(active-dry))));
                    // Diagnostic hypotheses, never substitutes for equal-power.
                    result.directLinearError = std::max(result.directLinearError, std::abs(d[c][i]-(dry+amount*(active-dry))));
                    result.nodeSquaredLinearError = std::max(result.nodeSquaredLinearError, std::abs(b[c][i]-(dry+amount*amount*(active-dry))));
                    result.rawDelta = std::max(result.rawDelta, std::abs(b[c][i]-previousOutput[c]));
                    result.dryRawDelta = std::max(result.dryRawDelta, std::abs(dry-previousDry[c]));
                    result.idealEqualGainStep = std::max(result.idealEqualGainStep,
                        std::abs(expected-equalPower(dry, active, priorAmount)));
                    result.actualAddedGainStep = std::max(result.actualAddedGainStep,
                        std::abs((amount*amount-priorAmount*priorAmount)*(active-dry)));
                    if (!enabling && since >= fadeSamples+1)
                    {
                        result.settledNull = std::max({ result.settledNull, std::abs(b[c][i]-dry), std::abs(d[c][i]-dry) });
                        result.exactNull &= b[c][i] == dry && d[c][i] == dry;
                    }
                    if (enabling && since >= fadeSamples+1)
                        result.settledWetError = std::max({ result.settledWetError, std::abs(b[c][i]-active), std::abs(d[c][i]-active) });
                    result.finite &= std::isfinite(d[c][i]) && std::isfinite(active) && std::isfinite(b[c][i]);
                }
            }
            else
                for (int c = 0; c < 2; ++c)
                {
                    const double expected = enabling ? input[c][i] : w[c][i];
                    result.preEventOracleError = std::max({ result.preEventOracleError,
                        std::abs(d[c][i]-expected), std::abs(b[c][i]-expected) });
                }
            for (int c = 0; c < 2; ++c)
            { previousOutput[c] = b[c][i]; previousDry[c] = input[c][i]; }
        }
        // Observe the real runtime endpoint; do not infer timing from a fitted law.
        if (frame+n > event && result.endpoint < 0 && runtime.enableMix == (enabling ? 1 : 0))
            result.endpoint = frame+n-event;
        frame += n;
    }
    result.phaseAfter = model.modulationPhase();
    return result;
}
}
int main()
{
    int cells = 0, equalPowerFails = 0, singleLinearFails = 0, deltaFails = 0, idealDeltaFails = 0, safetyFails = 0;
    for (double rate : { 44100., 48000., 88200., 96000., 192000. })
        for (int block : { 1, 4, 16, 64, 512, 2048 })
            for (bool enabling : { false, true })
            {
                const auto r = audit(rate, block, enabling); ++cells;
                const bool eq = r.nodeEqualError < 1e-14 && r.directEqualError < 1e-14;
                const bool linear = r.nodeSingleLinearError < 1e-14;
                const bool delta = r.rawDelta < lsb24;
                const bool idealDelta = r.idealEqualGainStep < lsb24;
                const bool safe = r.finite && r.exactNull && r.settledNull < 1e-7 && r.settledWetError < 1e-14
                    && r.preEventOracleError < 1e-14
                    && (enabling || r.phaseBefore == r.phaseAfter);
                equalPowerFails += !eq; singleLinearFails += !linear;
                deltaFails += !delta; idealDeltaFails += !idealDelta; safetyFails += !safe;
                std::printf("{\"rate\":%.0f,\"host_block\":%d,\"enabling\":%s,\"direct_equal_error\":%.17g,\"node_equal_error\":%.17g,"
                    "\"node_single_linear_error\":%.17g,\"direct_linear_error\":%.17g,\"node_squared_linear_error\":%.17g,"
                    "\"raw_peak_delta\":%.17g,\"dry_sine_peak_delta\":%.17g,\"ideal_equal_gain_step\":%.17g,"
                    "\"actual_added_gain_step\":%.17g,\"lsb24\":%.17g,\"settled_null\":%.17g,\"settled_wet_error\":%.17g,"
                    "\"pre_event_oracle_error\":%.17g,"
                    "\"exact_null\":%s,\"finite\":%s,\"frozen_phase\":%s,\"endpoint_observed_by_sample\":%d,\"safety_pass\":%s}\n",
                    rate, block, enabling?"true":"false", r.directEqualError, r.nodeEqualError, r.nodeSingleLinearError,
                    r.directLinearError, r.nodeSquaredLinearError, r.rawDelta, r.dryRawDelta, r.idealEqualGainStep,
                    r.actualAddedGainStep, lsb24, r.settledNull, r.settledWetError, r.preEventOracleError,
                    r.exactNull?"true":"false", r.finite?"true":"false", r.phaseBefore==r.phaseAfter?"true":"false", r.endpoint, safe?"true":"false");
            }
    std::printf("{\"summary\":true,\"cells\":%d,\"equal_power_failures\":%d,\"single_linear_failures\":%d,"
        "\"literal_delta_failures\":%d,\"ideal_equal_gain_step_failures\":%d,\"safety_failures\":%d}\n",
        cells, equalPowerFails, singleLinearFails, deltaFails, idealDeltaFails, safetyFails);
    return equalPowerFails || singleLinearFails || deltaFails || idealDeltaFails || safetyFails ? 1 : 0;
}
