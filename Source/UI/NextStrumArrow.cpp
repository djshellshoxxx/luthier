#include "NextStrumArrow.h"

#include "../PluginProcessor.h"
#include "Theme.h"

namespace luthier
{

NextStrumArrow::NextStrumArrow (LuthierAudioProcessor& p)
    : processor (p)
{
    setInterceptsMouseClicks (false, false);
    setTitle ("Next strum");
    motion.startTimerHz (*this, kRefreshHz);   // merge: through cpu-quality-modes 6's motion switch
}

NextStrumArrow::~NextStrumArrow()
{
    motion.stopTimer();
}

void NextStrumArrow::timerCallback()
{
    tick (juce::Time::getMillisecondCounterHiRes());
}

void NextStrumArrow::tick (double nowMs)
{
    auto& rhythm = processor.getEngine().getRhythmEngine();

    const auto blocks = rhythm.getDrivenBlockCount();
    const auto strokes = rhythm.getStrokeCount();

    if (blocks != lastBlocks)
    {
        lastBlocks = blocks;
        lastReportMs = nowMs;
    }

    if (strokes != lastStrokes)
    {
        lastStrokes = strokes;
        flashUntilMs = nowMs + kFlashMs;
    }

    const auto next = rhythm.getNextStrumType();
    const bool nowShown = rhythm.isEnabled() && nowMs - lastReportMs <= kStaleMs;
    const bool nowFlashing = nowShown && nowMs < flashUntilMs;
    const bool nowDown = isDownStroke (next);

    if (nowShown != shown || nowFlashing != flashing || nowDown != down)
    {
        shown = nowShown;
        flashing = nowFlashing;
        down = nowDown;
        repaint();
    }
}

void NextStrumArrow::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

    if (! shown)
        return;

    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float w = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (w * 0.6f, w);

    juce::Path arrow;
    arrow.addArrow ({ r.getCentreX(), down ? r.getY() : r.getBottom(),
                      r.getCentreX(), down ? r.getBottom() : r.getY() },
                    w * 0.12f, w * 0.5f, w * 0.4f);

    g.setColour (flashing ? Palette::textPrimary : Palette::accent);
    g.fillPath (arrow);
}

} // namespace luthier
