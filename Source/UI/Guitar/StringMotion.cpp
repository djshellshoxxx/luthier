#include "StringMotion.h"

#include <cmath>

namespace luthier
{

namespace
{
    // StringEngine::Damping, as published (SoundingNotes.h).
    enum : uint8_t { dOpen = 0, dLightTouch, dPalmMute, dReleased, dChoked, dSilenced, dChuck, dPalmMuteBass };

    bool isStopping (uint8_t d) noexcept
    {
        return d == dReleased || d == dChoked || d == dSilenced || d == dChuck;
    }

    float finiteOr (float v, float fallback) noexcept
    {
        return std::isfinite (v) ? v : fallback;
    }

    float smoothstep (float e0, float e1, float x) noexcept
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0));
        return t * t * (3.0f - 2.0f * t);
    }

    constexpr float kPi = juce::MathConstants<float>::pi;
}

//==============================================================================
float StringMotionGeometry::fractionForFret (float fret) noexcept
{
    return 1.0f - std::pow (2.0f, -juce::jmax (0.0f, fret) / 12.0f);
}

juce::Point<float> StringMotionGeometry::pointAtFraction (int s, float x) const noexcept
{
    if (! juce::isPositiveAndBelow (s, numStrings))
        return {};

    const auto& g = strings[(size_t) s];
    return g.nut + (g.bridge - g.nut) * x;
}

juce::Point<float> StringMotionGeometry::pointAt (int s, float fret) const noexcept
{
    return pointAtFraction (s, fractionForFret (fret));
}

float StringMotionGeometry::spacingAt (int s, float x) const noexcept
{
    const auto p = pointAtFraction (s, x);
    float best = 0.0f;

    for (int n : { s - 1, s + 1 })
        if (juce::isPositiveAndBelow (n, numStrings))
        {
            const float d = p.getDistanceFrom (pointAtFraction (n, x));
            best = best <= 0.0f ? d : juce::jmin (best, d);
        }

    return best > 0.0f ? best : 8.0f;   // a lone string: a nominal spacing
}

juce::Point<float> StringMotionGeometry::towardBass (int s, float x) const noexcept
{
    const auto p = pointAtFraction (s, x);
    juce::Point<float> d;

    if (juce::isPositiveAndBelow (s + 1, numStrings))
        d = pointAtFraction (s + 1, x) - p;
    else if (juce::isPositiveAndBelow (s - 1, numStrings))
        d = p - pointAtFraction (s - 1, x);
    else
    {
        const auto& g = strings[(size_t) juce::jlimit (0, kMaxStrings - 1, s)];
        const auto a = g.bridge - g.nut;
        d = { -a.y, a.x };
    }

    const float len = d.getDistanceFromOrigin();
    return len > 1.0e-6f ? d / len : juce::Point<float> (0.0f, 1.0f);
}

//==============================================================================
float StringMotionFrame::String::amplitudeAt (float u) const noexcept
{
    if (numSamples < 2)
        return 0.0f;

    const float x = juce::jlimit (0.0f, 1.0f, u) * (float) (numSamples - 1);
    const int i = juce::jmin (numSamples - 2, (int) x);
    const float f = x - (float) i;
    return samples[(size_t) i] + (samples[(size_t) i + 1] - samples[(size_t) i]) * f;
}

int StringMotionFrame::getNumActive() const noexcept
{
    int n = 0;
    for (int s = 0; s < numStrings; ++s)
        n += strings[(size_t) s].active ? 1 : 0;
    return n;
}

//==============================================================================
void StringMotion::reset() noexcept
{
    previousSwept.fill ({});
    previousActive.fill (false);
    muteOnset.fill (-1.0);
}

float StringMotion::normaliseLevel (float level) noexcept
{
    return juce::jlimit (0.0f, 1.0f, finiteOr (level, 0.0f) * 4.0f);
}

float StringMotion::bendDisplacement (float spacingPx, float bendCents) noexcept
{
    const float cents = juce::jlimit (0.0f, 450.0f, finiteOr (bendCents, 0.0f));
    return spacingPx * std::sqrt (cents / 200.0f);
}

float StringMotion::dampingMask (uint8_t damping, float u) noexcept
{
    /*  2.5: the palm sits over the last 8% before the bridge and pins the string
        there. The spec's smoothstep(0, 0.08, 1-u) would put its ramp inside that
        zone and leave the string moving under the palm, which AS-09 (zero for
        u >= 0.92) rules out, so the ramp starts where the palm ends. */
    if (damping == dPalmMute || damping == dPalmMuteBass)
        return smoothstep (0.08f, 0.16f, 1.0f - u);

    return 1.0f;
}

void StringMotion::envelope (float pluckPosition, double noteSeconds, int harmonicPartial,
                             StringAnimationQuality quality, int numSamples, float* out) noexcept
{
    numSamples = juce::jlimit (2, StringMotionFrame::kMaxSamples, numSamples);
    float peak = 0.0f;

    // p is measured from the bridge (Excitation::Params) and u runs from the
    // stop (0) to the bridge (1), so the pick sits at u = 1 - p.
    const float pickU = 1.0f - juce::jlimit (0.02f, 0.98f, finiteOr (pluckPosition, 0.16f));
    const int K = quality == StringAnimationQuality::high ? 4 : 1;
    const double t = juce::jmax (0.0, std::isfinite (noteSeconds) ? noteSeconds : 1.0e3);

    float c[4] = {};
    for (int k = 1; k <= K; ++k)
        c[k - 1] = std::sin ((float) k * kPi * pickU) / (float) (k * k)
                   * (float) std::exp (-(double) (k - 1) * t / 0.12);

    for (int i = 0; i < numSamples; ++i)
    {
        const float u = (float) i / (float) (numSamples - 1);
        float e = 0.0f;

        if (harmonicPartial >= 2)
        {
            e = std::abs (std::sin ((float) harmonicPartial * kPi * u));
        }
        else
        {
            for (int k = 1; k <= K; ++k)
                e += c[k - 1] * std::sin ((float) k * kPi * u);
            e = std::abs (e);
        }

        // The two ends are nodes exactly, not to float rounding of sin (k pi).
        out[i] = (i == 0 || i == numSamples - 1) ? 0.0f : e;
        peak = juce::jmax (peak, out[i]);
    }

    const float scale = peak > 1.0e-9f ? 1.0f / peak : 0.0f;

    for (int i = 0; i < numSamples; ++i)
        out[i] *= scale;
}

//==============================================================================
float StringMotion::levelFor (int s, const SoundingNotes::Motion& rec, double nowSeconds, float gain) const noexcept
{
    float ln = normaliseLevel (rec.level) * juce::jlimit (0.0f, 1.0f, finiteOr (gain, 0.0f));

    // 2.5: a stopped string dies visibly within 25 ms of the stop, even while the
    // follower's own release is still on its way down.
    if (isStopping (rec.damping))
    {
        const double onset = muteOnset[(size_t) s];
        if (onset >= 0.0)
            ln *= (float) std::exp (-juce::jmax (0.0, nowSeconds - onset) / kMuteDecaySeconds);
    }

    return ln;
}

bool StringMotion::anyAboveFloor (const SoundingNotes::Frame& snapshot, const StringMotionGeometry& geometry,
                                  double nowSeconds, float gain) const noexcept
{
    const int n = juce::jmin (geometry.numStrings, snapshot.numStrings, StringMotionFrame::kMaxStrings);

    for (int s = 0; s < n; ++s)
    {
        const auto& rec = snapshot.motion[(size_t) s];
        const float ln = levelFor (s, rec, nowSeconds, gain);

        if (ln <= 0.0f)
            continue;

        // The largest A_max the string can have: the widest spacing along it.
        const float stopX = StringMotionGeometry::fractionForFret (juce::jlimit (0.0f, geometry.numFrets, finiteOr (rec.stopFret, 0.0f)));
        const float spacing = juce::jmax (geometry.spacingAt (s, stopX), geometry.spacingAt (s, 1.0f));
        float aMax = juce::jmax (1.5f, 0.40f * spacing);

        if (rec.damping == dLightTouch)
            aMax *= 0.5f;

        if (aMax * ln >= kFloorPx)
            return true;
    }

    return false;
}

void StringMotion::update (const SoundingNotes::Frame& snapshot, const StringMotionGeometry& geometry,
                           StringAnimationQuality quality, double nowSeconds, float gain,
                           StringMotionFrame& frame) noexcept
{
    const int n = juce::jlimit (0, StringMotionFrame::kMaxStrings, juce::jmin (geometry.numStrings, snapshot.numStrings));
    const int numSamples = (quality == StringAnimationQuality::high ? 32 : 12) + 1;
    const double sr = snapshot.sampleRate > 0.0 ? snapshot.sampleRate : 44100.0;

    frame.numStrings = n;
    frame.quality = quality;

    for (int s = 0; s < StringMotionFrame::kMaxStrings; ++s)
    {
        auto& out = frame.strings[(size_t) s];
        out.hasDirty = false;
        out.dirty = {};

        if (s >= n)
        {
            // A string that no longer exists (a family switch to fewer strings):
            // clear whatever it last drew, once.
            if (previousActive[(size_t) s])
            {
                out.dirty = previousSwept[(size_t) s].getSmallestIntegerContainer();
                out.hasDirty = true;
            }

            out.active = false;
            out.swept = {};
            previousActive[(size_t) s] = false;
            previousSwept[(size_t) s] = {};
            continue;
        }

        const auto& rec = snapshot.motion[(size_t) s];
        const auto& g = geometry.strings[(size_t) s];

        // ---- 2.5's onset clock ----------------------------------------------------
        if (isStopping (rec.damping))
        {
            if (muteOnset[(size_t) s] < 0.0)
                muteOnset[(size_t) s] = nowSeconds;
        }
        else
        {
            muteOnset[(size_t) s] = -1.0;
        }

        const float ln = levelFor (s, rec, nowSeconds, gain);

        // ---- 2.1: stop, bridge, and 2.4's push -------------------------------------
        const float stopFret = juce::jlimit (0.0f, geometry.numFrets, finiteOr (rec.stopFret, 0.0f));
        const float stopX = StringMotionGeometry::fractionForFret (stopFret);

        out.nut = g.nut;
        out.bridge = g.bridge;
        out.stop = geometry.pointAtFraction (s, stopX);

        const float spacingAtStop = geometry.spacingAt (s, stopX);
        out.displacementPx = bendDisplacement (spacingAtStop, rec.pushCents);

        // The treble half is pushed toward the bass, the bass half pulled toward
        // the treble; on odd counts the middle string goes toward the bass.
        const bool trebleHalf = (float) s < (float) n * 0.5f;
        const auto bass = geometry.towardBass (s, stopX);
        out.displacedStop = out.stop + (trebleHalf ? bass : -bass) * out.displacementPx;

        auto axis = out.bridge - out.displacedStop;
        const float length = axis.getDistanceFromOrigin();
        out.normal = length > 1.0e-6f ? juce::Point<float> (-axis.y, axis.x) / length : juce::Point<float> (0.0f, 1.0f);

        // ---- 2.2: the envelope ---------------------------------------------------
        const double noteSeconds = rec.exciteSample >= 0 ? (double) (snapshot.samplePosition - rec.exciteSample) / sr : 1.0e3;
        envelope (rec.pluckPosition, noteSeconds, rec.harmonicPartial, quality, numSamples, scratch.data());

        int peakIndex = numSamples / 2;
        float peakShape = -1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float u = (float) i / (float) (numSamples - 1);
            scratch[(size_t) i] *= dampingMask (rec.damping, u);

            if (scratch[(size_t) i] > peakShape)
            {
                peakShape = scratch[(size_t) i];
                peakIndex = i;
            }
        }

        // A_max: 0.40 of the local spacing at the envelope's peak, floored at 1.5 px.
        const float peakU = (float) peakIndex / (float) (numSamples - 1);
        float aMax = juce::jmax (1.5f, 0.40f * geometry.spacingAt (s, stopX + (1.0f - stopX) * peakU));

        if (rec.damping == dLightTouch)
            aMax *= 0.5f;

        out.amplitudeMaxPx = aMax;
        out.levelNorm = ln;

        const float a = aMax * ln;
        out.active = a >= kFloorPx && length > 1.0f;
        out.numSamples = numSamples;
        out.peakPx = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            out.samples[(size_t) i] = out.active ? a * scratch[(size_t) i] : 0.0f;
            out.peakPx = juce::jmax (out.peakPx, out.samples[(size_t) i]);
        }

        // ---- 4.2: this frame's swept bounds and the dirty rect ----------------------
        const float margin = 2.0f * g.strokeWidthPx + 2.0f;

        if (out.active)
        {
            juce::Rectangle<float> r (out.nut, out.bridge);
            r = r.getUnion (juce::Rectangle<float> (out.displacedStop, out.displacedStop));

            for (int i = 0; i < numSamples; ++i)
            {
                const float u = (float) i / (float) (numSamples - 1);
                const float w = out.samples[(size_t) i];
                const auto p0 = out.pointAt (u, w), p1 = out.pointAt (u, -w);
                r = r.getUnion (juce::Rectangle<float> (p0, p1));
            }

            out.swept = r.expanded (margin);
        }
        else
        {
            out.swept = {};
        }

        const bool was = previousActive[(size_t) s];

        if (out.active || was)
        {
            // Last frame's and this frame's, so what moved away is cleaned up; on
            // the way down to rest, the rest line is drawn once more (2.2 "Floor").
            auto area = previousSwept[(size_t) s];
            const auto now = out.active ? out.swept : juce::Rectangle<float> (out.nut, out.bridge).expanded (margin);
            area = area.isEmpty() ? now : area.getUnion (now);

            if (! geometry.clip.isEmpty())
                area = area.getIntersection (geometry.clip.expanded (margin));

            out.dirty = area.getSmallestIntegerContainer();
            out.hasDirty = ! out.dirty.isEmpty();
        }

        previousActive[(size_t) s] = out.active;
        previousSwept[(size_t) s] = out.swept;
    }
}

} // namespace luthier
