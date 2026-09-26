#include "QualityController.h"
#include "../Accessibility/Localisation.h"

namespace luthier
{

namespace
{
    QualityLevel levelOf (QualityChoice c) noexcept
    {
        switch (c)
        {
            case QualityChoice::Medium: return QualityLevel::Medium;
            case QualityChoice::Low:    return QualityLevel::Low;
            case QualityChoice::High:
            case QualityChoice::Auto:
            default:                    return QualityLevel::High;
        }
    }

    QualityChoice choiceOf (QualityOverride o, QualityChoice global) noexcept
    {
        switch (o)
        {
            case QualityOverride::High:   return QualityChoice::High;
            case QualityOverride::Medium: return QualityChoice::Medium;
            case QualityOverride::Low:    return QualityChoice::Low;
            case QualityOverride::Auto:   return QualityChoice::Auto;
            case QualityOverride::Global:
            default:                      return global;
        }
    }
}

//==============================================================================
QualityController::QualityController (const CpuLoadMonitor& m)
    : monitor (m)
{
    autoLevel.store ((int) PerformanceSettings::get().getAutoLastLevel(), std::memory_order_relaxed);
    startTimerHz (10);
}

QualityController::~QualityController()
{
    stopTimer();
}

//==============================================================================
void QualityController::setOverride (QualityOverride o)
{
    if (o == getOverride())
        return;

    overrideChoice.store ((int) o, std::memory_order_relaxed);
    sendChangeMessage();
}

void QualityController::resumeAuto()
{
    autoHeld.store (false, std::memory_order_relaxed);
    downCount = 0;

    for (auto& t : downTimes)
        t = -1.0e12;

    sendChangeMessage();
}

void QualityController::restartAutoForTesting()
{
    autoLevel.store ((int) PerformanceSettings::get().getAutoLastLevel(), std::memory_order_relaxed);
    autoHeld.store (false, std::memory_order_relaxed);
    autoWasActive = false;
    lastStep = lastDown = lastBanner = -1.0e12;
    downCount = 0;

    for (auto& t : downTimes)
        t = -1.0e12;

    for (auto& l : measuredLoad)
        l = 0.0;

    relief.store (0, std::memory_order_relaxed);
    e2Since = -1.0;
    lastAnnouncement = -1.0e12;

    const juce::ScopedLock sl (noticeLock);
    notices.clear();
}

//==============================================================================
QualityChoice QualityController::getChoice() const noexcept
{
    return choiceOf (getOverride(), PerformanceSettings::get().getQuality());
}

QualityLevel QualityController::getLiveLevel() const noexcept
{
    const int f = forced.load (std::memory_order_relaxed);

    if (f >= 0)
        return (QualityLevel) juce::jlimit (0, 2, f);

    const auto choice = getChoice();

    return choice == QualityChoice::Auto ? getAutoLevel() : levelOf (choice);
}

QualityLevel QualityController::getEffectiveLevel() const noexcept
{
    // 2.6: offline is High when the option is on, and always for Auto (load is
    // meaningless offline and renders must be deterministic).
    if (isNonRealtime()
        && (PerformanceSettings::get().isOfflineAtHigh() || getChoice() == QualityChoice::Auto))
        return QualityLevel::High;

    return getLiveLevel();
}

//==============================================================================
void QualityController::setClockForTesting (std::function<double()> nowMs)  { clock = std::move (nowMs); }
void QualityController::setLoadFeedForTesting (std::function<LoadSnapshot()> feed) { loadFeed = std::move (feed); }

double QualityController::now() const
{
    return clock ? clock() : juce::Time::getMillisecondCounterHiRes();
}

QualityController::LoadSnapshot QualityController::readLoad() const
{
    if (loadFeed)
        return loadFeed();

    LoadSnapshot s;
    s.mean200ms = monitor.getMean200ms();
    s.mean2s = monitor.getMean2s();
    s.mean20s = monitor.getMean20s();
    s.p95_2s = monitor.getP95_2s();
    s.measuredSeconds = monitor.getMeasuredSeconds();
    return s;
}

//==============================================================================
void QualityController::pushNotice (Notice n)
{
    {
        const juce::ScopedLock sl (noticeLock);
        notices.add (std::move (n));
    }

    sendChangeMessage();
}

bool QualityController::popNotice (Notice& out)
{
    const juce::ScopedLock sl (noticeLock);

    if (notices.isEmpty())
        return false;

    out = notices.removeAndReturn (0);
    return true;
}

int QualityController::getPendingNoticeCount() const
{
    const juce::ScopedLock sl (noticeLock);
    return notices.size();
}

juce::String QualityController::levelName (QualityLevel l)
{
    switch (l)
    {
        case QualityLevel::Medium: return tr ("quality.level.medium");
        case QualityLevel::Low:    return tr ("quality.level.low");
        case QualityLevel::High:
        default:                   return tr ("quality.level.high");
    }
}

juce::String QualityController::shortLabel (QualityChoice choice, QualityLevel live)
{
    const char* const plain[] = { "quality.badge.high", "quality.badge.medium", "quality.badge.low" };
    const char* const autoKeys[] = { "quality.badge.autoHigh", "quality.badge.autoMedium", "quality.badge.autoLow" };
    const int i = juce::jlimit (0, 2, (int) live);

    return tr (choice == QualityChoice::Auto ? autoKeys[i] : plain[i]);
}

//==============================================================================
void QualityController::stepAuto (int newLevel, bool down, double t)
{
    autoLevel.store (newLevel, std::memory_order_relaxed);
    levelSince = t;
    lastStep = t;

    PerformanceSettings::get().setAutoLastLevel ((QualityLevel) newLevel);

    const auto name = levelName ((QualityLevel) newLevel);

    if (down)
    {
        lastDown = t;

        // Anti-oscillation: three downs inside ten minutes hold Auto where it is.
        for (int i = kHoldDowns - 1; i > 0; --i)
            downTimes[i] = downTimes[i - 1];

        downTimes[0] = t;
        downCount = juce::jmin (kHoldDowns, downCount + 1);

        if (downCount >= kHoldDowns && t - downTimes[kHoldDowns - 1] <= kHoldWindowMs)
            autoHeld.store (true, std::memory_order_relaxed);

        Notice n;
        n.announce = true;
        n.banner = PerformanceSettings::get().isAutoNotify() && (t - lastBanner >= kBannerGapMs);
        n.text = tr ("quality.notice.autoDown", { { "level", name } });

        if (n.banner)
            lastBanner = t;

        pushNotice (n);
    }
    else
    {
        Notice n;
        n.announce = true;
        n.text = tr ("quality.notice.autoUp", { { "level", name } });
        pushNotice (n);
    }
}

void QualityController::tick()
{
    const double t = now();
    const auto load = readLoad();
    const bool offline = isNonRealtime();

    // ---- E3 notices (the drop itself happened on the audio thread) ---------------
    if (const int drops = stringDrops.exchange (0, std::memory_order_relaxed); drops > 0)
    {
        Notice n;
        n.banner = true;
        n.announce = (t - lastAnnouncement) >= 5000.0;
        n.text = announcedFirstDrop ? tr ("quality.notice.stringDropped")
                                    : tr ("quality.notice.stringDroppedFirst");
        announcedFirstDrop = true;

        if (n.announce)
            lastAnnouncement = t;

        pushNotice (n);
    }

    // ---- governor E1 / E2 (display only; 7) --------------------------------------
    if (offline || (! isGovernorEnabledGlobally() && ! loadFeed))
    {
        relief.store (0, std::memory_order_relaxed);
        e2Since = -1.0;
    }
    else
    {
        int r = relief.load (std::memory_order_relaxed);
        const int before = r;

        if (r == 0 && load.mean200ms > kE1Enter)
            r = 1;

        if (r >= 1)
        {
            if (load.mean200ms > kE2Enter)
            {
                if (e2Since < 0.0)
                    e2Since = t;

                if (r == 1 && t - e2Since >= kE2HoldMs)
                    r = 2;
            }
            else
            {
                e2Since = -1.0;
            }

            if (load.mean2s < kELeave && load.mean200ms <= kE1Enter)
            {
                r = 0;
                e2Since = -1.0;
            }
        }

        if (r != before)
        {
            relief.store (r, std::memory_order_relaxed);
            sendChangeMessage();
        }
    }

    // ---- Auto (2.7) ---------------------------------------------------------------
    const bool autoActive = isAuto() && forced.load (std::memory_order_relaxed) < 0;

    if (! autoActive)
    {
        autoWasActive = false;
        return;
    }

    if (! autoWasActive)
    {
        // A fresh start: the level's windows begin now.
        autoWasActive = true;
        levelSince = t;
    }

    if (offline || isAutoHeld())
        return;

    const int level = (int) getAutoLevel();
    const double atLevel = t - levelSince;

    // What this level costs, for the step-up prediction.
    if (atLevel >= kDownWindowMs)
        measuredLoad[level] = load.mean2s;

    const bool downWanted = (load.mean2s > kDownMean || load.p95_2s > kDownP95);

    if (downWanted && level < (int) QualityLevel::Low
        && atLevel >= kDownWindowMs && t - lastStep >= kDwellMs)
    {
        stepAuto (level + 1, true, t);
        sendChangeMessage();
        return;
    }

    if (level > (int) QualityLevel::High
        && atLevel >= kUpWindowMs
        && t - lastDown >= kNoUpAfterDownMs
        && t - lastStep >= kDwellMs
        && load.mean20s < kUpMean)
    {
        const int higher = level - 1;
        double ratio = (higher == (int) QualityLevel::High) ? kDefaultRatioMediumToHigh
                                                            : kDefaultRatioLowToMedium;

        if (measuredLoad[higher] > 0.0 && measuredLoad[level] > 0.0)
            ratio = measuredLoad[higher] / measuredLoad[level];

        if (load.mean20s * ratio < kUpPredicted)
        {
            stepAuto (higher, false, t);
            sendChangeMessage();
        }
    }
}

} // namespace luthier
