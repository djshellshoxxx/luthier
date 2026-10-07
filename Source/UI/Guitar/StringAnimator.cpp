#include "StringAnimator.h"
#include "../UiPreferences.h"
#include "../../Accessibility/Accessibility.h"
#include "../../Support/ErrorLog.h"

namespace luthier
{

//==============================================================================
bool StringAnimationSettings::isEnabled()
{
    return UiPreferences::get().getBool (enabledKey, false);
}

void StringAnimationSettings::setEnabled (bool shouldAnimate)
{
    UiPreferences::get().setBool (enabledKey, shouldAnimate);
}

StringAnimationQuality StringAnimationSettings::getQuality()
{
    return UiPreferences::get().getString (qualityKey, "high") == "low" ? StringAnimationQuality::low
                                                                         : StringAnimationQuality::high;
}

void StringAnimationSettings::setQuality (StringAnimationQuality q)
{
    UiPreferences::get().setString (qualityKey, q == StringAnimationQuality::low ? "low" : "high");
}

//==============================================================================
StringAnimator::StringAnimator (juce::Component& ownerIn, const SoundingNotes& sourceIn, GeometryProvider provider)
    : owner (ownerIn), source (sourceIn), geometryProvider (std::move (provider))
{
}

StringAnimator::~StringAnimator()
{
    stopClock();
}

double StringAnimator::now() const
{
    return testClock ? testClock() : juce::Time::getMillisecondCounterHiRes() * 0.001;
}

void StringAnimator::setClockForTesting (std::function<double()> clock)
{
    testClock = std::move (clock);
}

bool StringAnimator::isRunning() const noexcept
{
    return vblank != nullptr || isTimerRunning();
}

bool StringAnimator::isEffectivelyShowing (const juce::Component& c)
{
    if (c.getPeer() != nullptr)
        return AnimationPolicy::isShowingFast (c);

    // No window (tests, or a component not yet on screen): visible within its
    // own hierarchy.
    for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        if (! p->isVisible())
            return false;

    return true;
}

StringAnimationQuality StringAnimator::getEffectiveQuality() const noexcept
{
    // cpu-quality-modes 6: Limited motion (Medium, relief 1) forces the Low style.
    if (reliefLevel >= 1 || droppedToLow || AnimationPolicy::get().getStringsStyle() == AnimationPolicy::StringsStyle::LowStyle)
        return StringAnimationQuality::low;

    return StringAnimationSettings::getQuality();
}

void StringAnimator::setReliefLevel (int level)
{
    reliefLevel = juce::jmax (0, level);
    poll();
}

void StringAnimator::resetMotion()
{
    motion.reset();
    frame = {};
    drawing = false;
}

//==============================================================================
bool StringAnimator::readSnapshot (double t)
{
    source.read (snapshot);   // 12: a failed read keeps the previous snapshot

    if (snapshot.sequence != lastSequence)
    {
        lastSequence = snapshot.sequence;
        lastSequenceChange = t;
    }

    stale = snapshot.sequence == 0 || (t - lastSequenceChange) > kStaleSeconds;
    return ! stale;
}

float StringAnimator::staleGain (double t) const noexcept
{
    if (! stale)
        return 1.0f;

    // 2.6: stale for 250 ms, then every string eases to rest over 120 ms.
    const double since = t - (lastSequenceChange + kStaleSeconds);
    return (float) juce::jlimit (0.0, 1.0, 1.0 - since / kEaseSeconds);
}

//==============================================================================
void StringAnimator::poll()
{
    const double t = now();

    // Motion Off (Reduced motion, CPU quality Low, relief >= 2: cpu-quality-modes 6)
    // shows the static overlay, as Reduced motion always did.
    enabledByUser = StringAnimationSettings::isEnabled()
                    && AnimationPolicy::get().getStringsStyle() != AnimationPolicy::StringsStyle::Off;

    const bool showing = isEffectivelyShowing (owner);
    const bool haveScene = enabledByUser && showing && geometryProvider && geometryProvider (geometry);

    motionActive = enabledByUser && showing && reliefLevel < 2 && haveScene;

    if (! motionActive)
    {
        if (drawing || isRunning())
            goToRest();

        return;
    }

    readSnapshot (t);

    if (isRunning())
    {
        // The quality changed under a running clock (relief, the preference):
        // restart it on the right source.
        const bool wantVBlank = getEffectiveQuality() == StringAnimationQuality::high && owner.getPeer() != nullptr;

        if (wantVBlank != (vblank != nullptr))
        {
            stopClock();
            startClock();
        }

        return;
    }

    if (motion.anyAboveFloor (snapshot, geometry, t, staleGain (t)))
        startClock();
    else if (drawing)
        frameAt (t);   // the final frame back to rest
}

void StringAnimator::goToRest()
{
    stopClock();

    // Clean up what was drawn, unless nobody can see it (a hidden view repaints
    // whole when it shows again).
    if (drawing && isEffectivelyShowing (owner))
    {
        for (int s = 0; s < frame.numStrings; ++s)
        {
            const auto& str = frame.strings[(size_t) s];

            if (str.active && ! str.swept.isEmpty())
            {
                owner.repaint (str.swept.getSmallestIntegerContainer().getIntersection (owner.getLocalBounds()));
                ++repaintCalls;
            }
        }
    }

    motion.reset();
    frame = {};
    drawing = false;
}

void StringAnimator::startClock()
{
    if (isRunning())
        return;

    lastFrameTime = -1.0e9;

    if (getEffectiveQuality() == StringAnimationQuality::high && owner.getPeer() != nullptr)
        vblank = std::make_unique<juce::VBlankAttachment> (&owner, [this] (double ts) { onVBlank (ts); });
    else
        motionRegistration.startTimerHz (*this, 30);
}

void StringAnimator::stopClock()
{
    vblank.reset();
    motionRegistration.stopTimer();
}

void StringAnimator::timerCallback()
{
    frameAt (now());
}

void StringAnimator::onVBlank (double timestampSeconds)
{
    simulateVBlankForTesting (timestampSeconds);
}

bool StringAnimator::simulateVBlankForTesting (double timestampSeconds)
{
    // 2.6: at least 15 ms apart at High (so 120/144 Hz displays cap at 60), 30 at Low.
    const double minInterval = getEffectiveQuality() == StringAnimationQuality::high ? kHighMinInterval
                                                                                      : kLowMinInterval;

    if (timestampSeconds - lastFrameTime < minInterval - 1.0e-4)
        return false;

    lastFrameTime = timestampSeconds;
    frameAt (now());
    return true;
}

void StringAnimator::stepFrameForTesting()
{
    poll();

    if (isRunning())
        frameAt (now());
}

//==============================================================================
void StringAnimator::frameAt (double t)
{
    const auto startTicks = juce::Time::getHighResolutionTicks();

    ++frames;
    readSnapshot (t);

    if (! geometryProvider || ! geometryProvider (geometry))
    {
        goToRest();
        return;
    }

    motion.update (snapshot, geometry, getEffectiveQuality(), t, staleGain (t), frame);

    lastDirtyRects.clear();
    lastDirtyUnion = {};
    lastDirtyArea = 0;

    const auto bounds = owner.getLocalBounds();
    int sumOfAreas = 0, numDirty = 0;

    for (int s = 0; s < StringMotionFrame::kMaxStrings; ++s)
    {
        const auto& str = frame.strings[(size_t) s];

        if (! str.hasDirty)
            continue;

        const auto r = str.dirty.getIntersection (bounds);

        if (r.isEmpty())
            continue;

        dirtyScratch[(size_t) numDirty++] = r;
        sumOfAreas += r.getWidth() * r.getHeight();
        lastDirtyUnion = lastDirtyUnion.isEmpty() ? r : lastDirtyUnion.getUnion (r);
    }

    /*  At most one repaint(rect) per moving string, never the whole owner (4.3,
        11). Neighbouring strings' rects mostly overlap, and a region of many
        overlapping rectangles is slow to clip against, so when their bounding
        rect is barely larger than the rects themselves it goes as one call. */
    const int unionArea = lastDirtyUnion.getWidth() * lastDirtyUnion.getHeight();

    if (numDirty > 1 && (float) unionArea <= 1.25f * (float) sumOfAreas)
    {
        owner.repaint (lastDirtyUnion);
        ++repaintCalls;
        lastDirtyRects.add (lastDirtyUnion);
    }
    else
    {
        for (int i = 0; i < numDirty; ++i)
        {
            owner.repaint (dirtyScratch[(size_t) i]);
            ++repaintCalls;
            lastDirtyRects.add (dirtyScratch[(size_t) i]);
        }
    }

    lastDirtyRects.consolidate();

    for (const auto& r : lastDirtyRects)
        lastDirtyArea += r.getWidth() * r.getHeight();

    drawing = frame.getNumActive() > 0;

    if (! drawing)
        stopClock();

    // 12: ten frames in a row over budget drop the animator to Low for the session.
    const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - startTicks) * 1000.0
                      + lastPaintMs;

    overBudgetFrames = ms > kFrameBudgetMs ? overBudgetFrames + 1 : 0;

    if (overBudgetFrames >= 10 && ! droppedToLow)
    {
        droppedToLow = true;
        ErrorLog::write (ErrorLog::Severity::warn, "StringAnimator", "STRING_ANIMATION_OVER_BUDGET",
                         "The string animation went over its frame budget for 10 frames and dropped to Low quality.");
    }
}

} // namespace luthier
