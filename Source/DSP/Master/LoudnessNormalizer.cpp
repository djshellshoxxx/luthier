#include "LoudnessNormalizer.h"

#include <algorithm>
#include <climits>

namespace luthier
{

//==============================================================================
std::int32_t LoudnessNormalizer::gainForMeasurement (double targetLufs, double measuredLufs, std::uint8_t& flags) noexcept
{
    const double wanted = targetLufs - measuredLufs;
    auto centi = (std::int32_t) std::lround (wanted * 100.0);

    if (centi > kMaxGainCentiDb)
    {
        centi = kMaxGainCentiDb;
        flags |= flagClampedHigh;
    }
    else if (centi < kMinGainCentiDb)
    {
        centi = kMinGainCentiDb;
        flags |= flagClampedLow;
    }

    return centi;
}

std::uint64_t LoudnessNormalizer::pack (std::uint32_t serial, std::int32_t gainCentiDb, std::uint8_t flags) noexcept
{
    const auto gain24 = (std::uint64_t) ((std::uint32_t) gainCentiDb & 0xFFFFFFu);
    return ((std::uint64_t) serial << 32) | (gain24 << 8) | (std::uint64_t) flags;
}

void LoudnessNormalizer::unpack (std::uint64_t word, std::uint32_t& serial, std::int32_t& gainCentiDb, std::uint8_t& flags) noexcept
{
    serial = (std::uint32_t) (word >> 32);
    flags = (std::uint8_t) (word & 0xFFu);

    auto gain24 = (std::uint32_t) ((word >> 8) & 0xFFFFFFu);

    if ((gain24 & 0x800000u) != 0)
        gain24 |= 0xFF000000u;   // sign-extend

    gainCentiDb = (std::int32_t) gain24;
}

std::uint32_t LoudnessNormalizer::getPublishedSerial() const noexcept
{
    std::uint32_t s; std::int32_t g; std::uint8_t f;
    unpack (getPublishedWord(), s, g, f);
    return s;
}

//==============================================================================
void LoudnessNormalizer::prepare (double sampleRate) noexcept
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    glideLength = std::max<std::int64_t> (1, (std::int64_t) std::llround (kGlideSeconds * sr));
    reset();
}

void LoudnessNormalizer::reset() noexcept
{
    startDb = endDb;
    glideStart = timeline - glideLength;
    gliding = false;
    segStart = INT64_MIN;
    currentDb.store (endDb, std::memory_order_relaxed);
    active = enabled.load (std::memory_order_acquire) || endDb != 0.0;
}

void LoudnessNormalizer::publishResult (std::uint32_t serial, std::int32_t gainCentiDb, std::uint8_t flags) noexcept
{
    published.store (pack (serial, gainCentiDb, flags), std::memory_order_release);
}

void LoudnessNormalizer::applyLoadGain (std::int32_t gainCentiDb) noexcept
{
    pendingLoad.store (std::clamp (gainCentiDb, kMinGainCentiDb, kMaxGainCentiDb), std::memory_order_release);
}

//==============================================================================
double LoudnessNormalizer::dbAt (std::int64_t t) const noexcept
{
    if (t <= glideStart)
        return startDb;

    if (t >= glideStart + glideLength)
        return endDb;

    return startDb + (endDb - startDb) * (double) (t - glideStart) / (double) glideLength;
}

void LoudnessNormalizer::startGlide (double toDb, std::int64_t at) noexcept
{
    const double from = dbAt (at);

    if (from == toDb && ! gliding && endDb == toDb)
        return;

    startDb = from;
    endDb = toDb;
    glideStart = at;
    gliding = true;
    segStart = INT64_MIN;   // re-evaluate the segment under the new curve
}

void LoudnessNormalizer::loadSegment (std::int64_t segmentStart) noexcept
{
    segStart = segmentStart;
    segGainA = std::pow (10.0, dbAt (segmentStart) / 20.0);
    segGainB = std::pow (10.0, dbAt (segmentStart + kSegment) / 20.0);
}

void LoudnessNormalizer::beginBlock (std::int64_t timelineStart, std::int64_t resultStartSample) noexcept
{
    timeline = timelineStart;

    // A cached preset load snaps rather than glides (2.2).
    const auto load = pendingLoad.exchange (INT32_MIN, std::memory_order_acq_rel);

    if (load != INT32_MIN)
    {
        calibratedDb = (double) load / 100.0;

        if (enabled.load (std::memory_order_acquire))
        {
            startDb = endDb = calibratedDb;
            glideStart = timeline - glideLength;
            gliding = false;
            segStart = INT64_MIN;
        }
    }

    // A new calibration result.
    {
        std::uint32_t serial; std::int32_t gain; std::uint8_t flags;
        unpack (published.load (std::memory_order_acquire), serial, gain, flags);

        if (serial != 0 && serial != appliedSerial)
        {
            appliedSerial = serial;

            // Unmeasurable or failed-without-estimate: keep the gain (12).
            if ((flags & (flagUnmeasurable | flagFailed)) == 0 || (flags & flagEstimate) != 0)
            {
                calibratedDb = (double) gain / 100.0;

                if (enabled.load (std::memory_order_acquire))
                {
                    if (snapNext)
                    {
                        startDb = endDb = calibratedDb;
                        glideStart = timeline - glideLength;
                        gliding = false;
                        segStart = INT64_MIN;
                    }
                    else
                    {
                        startGlide (calibratedDb, resultStartSample >= 0 ? resultStartSample : timeline);
                    }
                }
            }
        }
    }

    snapNext = false;

    // The switch.
    const bool on = enabled.load (std::memory_order_acquire);

    if (on != wasEnabled)
    {
        wasEnabled = on;
        startGlide (on ? calibratedDb : 0.0, timeline);
    }

    if (gliding && timeline >= glideStart + glideLength + kSegment)
        gliding = false;

    active = on || gliding || endDb != 0.0;

    if (! active)
        currentDb.store (0.0, std::memory_order_relaxed);
}

double LoudnessNormalizer::next() noexcept
{
    const std::int64_t t = timeline++;
    const std::int64_t s = t - (((t % kSegment) + kSegment) % kSegment);

    if (s != segStart)
        loadSegment (s);

    const double frac = (double) (t - s) / (double) kSegment;
    const double g = segGainA + (segGainB - segGainA) * frac;

    if ((t & 63) == 0)
        currentDb.store (20.0 * std::log10 (std::max (1.0e-9, g)), std::memory_order_relaxed);

    return g;
}

} // namespace luthier
