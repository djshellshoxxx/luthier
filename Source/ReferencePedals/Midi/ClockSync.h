#pragma once
#include "MidiTypes.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace luthier::reference::midi
{
class ClockSync
{
public:
    void prepare (double rate) noexcept { *this = ClockSync {}; sr = rate > 0 ? rate : 48000; }
    void begin (const HostTransport& h, TempoSource s, double internal, int64_t start, RouteOut& out) noexcept
    {
        host = h; source = s; blockStart = start;
        internalBpm = std::isfinite (internal) ? std::clamp (internal, 20., 300.) : 120.;
        const bool valid = h.valid && std::isfinite (h.bpm) && h.bpm > 0 && std::isfinite (h.ppq);
        useHost = valid && (s == TempoSource::Host || (s == TempoSource::Auto && h.playing));
        if (previousHostPlaying && (! useHost || ! h.playing)) emit (TransportEvent::Stop, start, out);
        if (useHost)
        {
            if (h.playing && ! previousHostPlaying) emit (std::abs (h.ppq) < 1e-9 ? TransportEvent::Start : TransportEvent::Continue, start, out);
            else if (h.playing && previousHostValid && std::abs (h.ppq - expectedHostPpq) > .05) emit (TransportEvent::Locate, start, out);
            expectedHostPpq = h.ppq;
        }
        previousHostPlaying = useHost && h.playing; previousHostValid = useHost;
        finish (start, out);
    }
    void event (uint8_t status, uint8_t d1, uint8_t d2, int64_t t, RouteOut& out) noexcept
    {
        if (status == 0xfa) { armed = true; running = false; ticks = 0; lost = false; }
        else if (status == 0xfb) { armed = false; running = true; if (! useHost) emit (TransportEvent::Continue, t, out); }
        else if (status == 0xfc) { armed = false; running = false; if (! useHost) emit (TransportEvent::Stop, t, out); }
        else if (status == 0xf2 && ! running) ticks = ((int64_t) d1 + ((int64_t) d2 << 7)) * 6;
        else if (status == 0xf8)
        {
            if (lastTick >= 0 && t > lastTick)
            {
                const double d = (double) (t - lastTick);
                if (d >= sr * 60 / (300 * 24) * .5 && d <= sr * 60 / (20 * 24) * 2)
                {
                    intervals[(size_t) (intervalCount % 5)] = d; ++intervalCount;
                    auto sorted = intervals; const int count = std::min (intervalCount, 5);
                    std::sort (sorted.begin(), sorted.begin() + count);
                    const double median = sorted[(size_t) (count / 2)];
                    period = period <= 0 ? median : period + (intervalCount < 24 ? .3 : .05) * (median - period);
                    midiBpm = std::clamp (60 * sr / (24 * period), 20., 300.);
                }
            }
            lastTick = t; lost = false;
            if (armed) { running = true; armed = false; ticks = 0; if (! useHost) emit (TransportEvent::Start, t, out); }
            else if (running) ++ticks;
        }
    }
    void finish (int64_t end, RouteOut& out) noexcept
    {
        if (lastTick >= 0 && end - lastTick >= (int64_t) (sr * .5)) { lost = true; running = false; }
        const bool midiValid = ! lost && intervalCount >= 1;
        const bool useMidi = ! useHost && midiValid && (source == TempoSource::Auto || source == TempoSource::MidiClock);
        out.time.bpm = useHost ? host.bpm : useMidi ? midiBpm : source == TempoSource::MidiClock && lost ? midiBpm : internalBpm;
        out.time.ppq = useHost ? host.ppq : (double) ticks / 24.;
        out.time.playing = useHost ? host.playing : useMidi && running;
        out.time.valid = useHost || useMidi;
        out.time.clockLocked = useMidi && intervalCount >= 24;
        out.time.sample = blockStart;
        if (useHost) expectedHostPpq = host.ppq + (double) (end - blockStart) * host.bpm / (60 * sr);
    }
    bool isRunning() const noexcept { return running; }
    bool isLost() const noexcept { return lost; }
    static double division (int i) noexcept
    {
        constexpr double d[] { 4,2,4./3,3,1,2./3,1.5,.5,1./3,.75,.25,1./6,.375,.125,8,.75 };
        return d[std::clamp (i, 0, 15)];
    }
    double tap (int64_t t) noexcept
    {
        if (lastTap < 0 || t - lastTap > sr * 2) { tapCount = 0; lastTap = t; return internalBpm; }
        const double d = (double) (t - lastTap); lastTap = t;
        if (d < sr * .2 || d > sr * 3) return internalBpm;
        if (tapCount == 3) { taps[0] = taps[1]; taps[1] = taps[2]; tapCount = 2; }
        taps[(size_t) tapCount++] = d;
        double sum = 0, weights = 0;
        for (int i = 0; i < tapCount; ++i) { sum += taps[(size_t) i] * (i + 1); weights += i + 1; }
        internalBpm = std::clamp (60 * sr * weights / sum, 20., 300.); return internalBpm;
    }
private:
    void emit (TransportEvent e, int64_t t, RouteOut& out) noexcept
    { if (out.nTr < kMaxRouteTransport) { out.tr[out.nTr] = e; out.trOff[out.nTr++] = (int) std::max (int64_t (0), t - blockStart); } }
    double sr = 48000, internalBpm = 120, midiBpm = 120, period = 0, expectedHostPpq = 0;
    int64_t blockStart = 0, lastTick = -1, ticks = 0, lastTap = -1;
    int intervalCount = 0, tapCount = 0;
    std::array<double, 5> intervals {};
    std::array<double, 3> taps {};
    bool armed = false, running = false, lost = false, useHost = false, previousHostPlaying = false, previousHostValid = false;
    HostTransport host;
    TempoSource source = TempoSource::Auto;
};
}
