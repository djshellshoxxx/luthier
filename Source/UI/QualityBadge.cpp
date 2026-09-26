#include "QualityBadge.h"
#include "Notifications.h"
#include "QualityOptions.h"
#include "Theme.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Localisation.h"

namespace luthier
{

//==============================================================================
QualityBadge::QualityBadge (LuthierAudioProcessor& p)
    : processor (p)
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle (tr ("quality.badge.name"));
    refresh();
    motion.startTimerHz (*this, 4);
}

QualityBadge::~QualityBadge()
{
    stopTimer();
}

juce::String QualityBadge::getLabelText() const { return label; }
juce::String QualityBadge::getShareText() const { return share; }

void QualityBadge::refresh()
{
    auto& controller = processor.getQualityController();
    const auto choice = controller.getChoice();
    const auto live = controller.getLiveLevel();

    label = (choice == QualityChoice::Auto && controller.isAutoHeld())
              ? tr ("quality.badge.autoHeld")
              : QualityController::shortLabel (choice, live);

    // 5: "-" before playback and once the figure is 5 s stale.
    const auto& monitor = processor.getCpuLoadMonitor();
    const auto blocks = monitor.getTotalBlocks();
    const double now = juce::Time::getMillisecondCounterHiRes();

    if (blocks != lastBlocks)
    {
        lastBlocks = blocks;
        lastBlocksChangedMs = now;
    }

    stale = blocks <= 0 || now - lastBlocksChangedMs > 5000.0;
    load = stale ? 0.0 : monitor.getMean200ms();
    share = stale ? tr ("quality.badge.stale")
                  : tr ("quality.share", { { "percent", juce::String (juce::roundToInt (load * 100.0)) } });

    const auto levelName = choice == QualityChoice::Auto
                             ? tr ("quality.level.auto") + " (" + QualityController::levelName (live) + ")"
                             : QualityController::levelName (live);

    const auto tip = stale ? tr ("quality.badge.tooltipIdle", { { "level", levelName } })
                           : tr ("quality.badge.tooltip", { { "level", levelName },
                                                            { "percent", juce::String (juce::roundToInt (load * 100.0)) } });
    setTooltip (tip);
    setDescription (tip);
    repaint();
}

void QualityBadge::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);

    auto area = getLocalBounds().toFloat().reduced (0.5f);
    const bool notHigh = processor.getQualityController().getChoice() != QualityChoice::High
                         || processor.getQualityController().getLiveLevel() != QualityLevel::High;

    // The label plate: the secondary accent when not High.
    auto plate = area.removeFromLeft (46.0f);
    g.setColour (notHigh ? Palette::secondaryDim : Palette::panelSunken);
    g.fillRoundedRectangle (plate, 3.0f);
    g.setColour (notHigh ? Palette::secondary : Palette::edge);
    g.drawRoundedRectangle (plate, 3.0f, 1.0f);
    g.setColour (notHigh ? Palette::textPrimary : Palette::textMuted);
    g.setFont (Fonts::mono (9.0f));
    g.drawFittedText (label, plate.toNearestInt(), juce::Justification::centred, 1, 0.7f);

    area.removeFromLeft (4.0f);

    // Five cells, zoned: < 50 % normal, 50-80 % warning, > 80 % error, plus a
    // glyph so the zone reads in monochrome too.
    const auto zoneColour = load > 0.8 ? Palette::clip : load >= 0.5 ? Palette::warning : Palette::secondary;
    auto bar = area.removeFromLeft (40.0f).reduced (0.0f, 4.0f);
    const int lit = stale ? 0 : juce::jlimit (0, 5, (int) std::ceil (load * 5.0 - 1.0e-9));

    for (int i = 0; i < 5; ++i)
    {
        auto cell = bar.removeFromLeft (8.0f).reduced (1.0f, 0.0f);
        g.setColour (i < lit ? zoneColour : Palette::edge);
        g.fillRect (cell);
    }

    area.removeFromLeft (4.0f);
    g.setColour (stale ? Palette::textDisabled : Palette::textMuted);
    g.setFont (Fonts::mono (9.0f));
    const auto glyph = stale ? juce::String() : load > 0.8 ? juce::String ("!!") : load >= 0.5 ? juce::String ("!") : juce::String();
    g.drawText (share + glyph, area.toNearestInt(), juce::Justification::centredLeft, false);

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accent);
        g.drawRect (getLocalBounds(), 1);
    }
}

void QualityBadge::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && onOpen)
        onOpen();
}

bool QualityBadge::keyPressed (const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey) && onOpen)
    {
        onOpen();
        return true;
    }

    return false;
}

std::unique_ptr<juce::AccessibilityHandler> QualityBadge::createAccessibilityHandler()
{
    // accessibility 1 / cpu-quality-modes 9: a focusable button.
    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::button,
        juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press,
                                                [this] { if (onOpen) onOpen(); }));
}

//==============================================================================
QualityEditorLink::QualityEditorLink (LuthierAudioProcessor& p, NotificationCentre& n)
    : processor (p), notifications (n)
{
    // 3: another process may have changed the file since this one read it.
    PerformanceSettings::get().reloadIfChanged();
    PerformanceSettings::get().addChangeListener (this);
    processor.getQualityController().addChangeListener (this);
    update();
    startTimerHz (4);
}

QualityEditorLink::~QualityEditorLink()
{
    stopTimer();
    PerformanceSettings::get().removeChangeListener (this);
    processor.getQualityController().removeChangeListener (this);
    AnimationPolicy::get().removeSource (this);
}

void QualityEditorLink::update()
{
    auto& controller = processor.getQualityController();

    // 6: this editor's level and relief; the most restrictive open editor wins.
    AnimationPolicy::get().setSource (this, controller.getLiveLevel(), controller.getReliefLevel());

    if (onOversamplingNote != nullptr)
    {
        const auto note = QualityOptions::oversamplingNoteFor (processor);

        if (note != lastOversamplingNote)
        {
            lastOversamplingNote = note;
            onOversamplingNote (note);
        }
    }

    QualityController::Notice notice;

    while (controller.popNotice (notice))
    {
        if (notice.banner)
            notifications.post ({ "cpu-quality", notice.text, Notification::Level::info });

        if (notice.announce)
            pendingAnnouncement = notice.text;   // the latest wins
    }

    // 9: polite announcements, at most one per 5 s.
    const double now = juce::Time::getMillisecondCounterHiRes();

    if (pendingAnnouncement.isNotEmpty() && now - lastAnnouncementMs >= kAnnouncementGapMs)
    {
        juce::AccessibilityHandler::postAnnouncement (pendingAnnouncement,
                                                      juce::AccessibilityHandler::AnnouncementPriority::low);
        pendingAnnouncement.clear();
        lastAnnouncementMs = now;
        ++announcements;
    }
}

void QualityEditorLink::cycleQuality()
{
    auto& controller = processor.getQualityController();
    const auto o = controller.getOverride();

    if (o == QualityOverride::Global)
    {
        const auto next = (QualityChoice) (((int) PerformanceSettings::get().getQuality() + 1) % 4);
        PerformanceSettings::get().setQuality (next);

        if (next == QualityChoice::Auto)
            controller.resumeAuto();
    }
    else
    {
        // High(1) -> Medium(2) -> Low(3) -> Auto(4) -> High(1)
        const auto next = (QualityOverride) (((int) o % 4) + 1);
        processor.setQualityOverride (next);

        if (next == QualityOverride::Auto)
            controller.resumeAuto();
    }

    update();
}

} // namespace luthier
