#include "StringRoll.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"
#include "../Capture/PerformanceCapture.h"
#include "../Model/Playing/TuningEngine.h"

namespace luthier
{

namespace
{
    constexpr int kNameColumn = 30;
    constexpr int kFastHz = 30;         ///< gui-engine-dataflow 2: the compact per-string strip's rate
    constexpr int kSlowHz = 10;         ///< accessibility 5: reduced motion
    constexpr int kBarsInWindow = 4;
    constexpr double kFreeWindowSeconds = 8.0;
    constexpr int kFretRange = 12;      ///< a click reaches the octave; beyond that the fretboard is the tool
    constexpr int kMinLabelWidth = 16;
}

//==============================================================================
/*  One string's lane: the hit area for the mouse, the focus target for the
    keyboard, and the element a screen reader lands on. It paints only its
    focus ring; the owner paints the roll in one pass. */
class StringRollComponent::Lane : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    Lane (StringRollComponent& o, int i) : owner (o), index (i)
    {
        setWantsKeyboardFocus (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        if (hasKeyboardFocus (true))
        {
            g.setColour (Palette::accent.withAlpha (0.8f));
            g.drawRect (getLocalBounds(), 1);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        hoverFret = owner.fretAtY ((float) e.y, (float) getHeight());
        setTooltip (owner.getLaneTooltip (index, hoverFret));
        owner.setHover (index, hoverFret);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverFret = -1;
        owner.setHover (-1, -1);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;

        pressed = true;
        owner.pluck (index, owner.fretAtY ((float) e.y, (float) getHeight()),
                     owner.velocityAtX ((float) e.x, (float) getWidth()));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (pressed)
        {
            pressed = false;
            owner.release (index);
        }
    }

    // accessibility 2: Enter or Space plucks, at the fret last hovered (open
    // string when none), and the string is released when the key comes up.
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key != juce::KeyPress::returnKey && key != juce::KeyPress::spaceKey)
            return false;

        if (! keyHeld)
        {
            keyHeld = true;
            owner.pluck (index, juce::jmax (0, hoverFret), 0.8);
        }

        return true;
    }

    bool keyStateChanged (bool) override
    {
        if (keyHeld && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::returnKey)
                    && ! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
        {
            keyHeld = false;
            owner.release (index);
            return true;
        }

        return false;
    }

    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }

    // accessibility 1: a button named for its string; "press" plucks it open
    // and lets it ring out, since a screen reader has no key-up to release on.
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler> (
            *this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press,
                                                    [this] { owner.pluck (index, 0, 0.8); }));
    }

private:
    StringRollComponent& owner;
    const int index;
    int hoverFret = -1;
    bool pressed = false, keyHeld = false;
};

//==============================================================================
StringRollComponent::StringRollComponent (LuthierAudioProcessor& p)
    : processor (p)
{
    glow.fill (0.0f);
    setOpaque (false);

    rebuildLanes (juce::jlimit (1, kMaxLanes, processor.getEngine().getNumStrings()));
    refresh();

    runningHz = wantedRefreshHz();
    updateTimerState();
}

StringRollComponent::~StringRollComponent()
{
    motion.stopTimer();

    for (auto& ancestor : watchedAncestors)
        if (ancestor != nullptr)
            ancestor->removeComponentListener (this);
}

//==============================================================================
void StringRollComponent::visibilityChanged()
{
    updateTimerState();
}

void StringRollComponent::parentHierarchyChanged()
{
    watchAncestors();
    updateTimerState();
}

void StringRollComponent::componentVisibilityChanged (juce::Component&)
{
    updateTimerState();
}

void StringRollComponent::watchAncestors()
{
    for (auto& ancestor : watchedAncestors)
        if (ancestor != nullptr)
            ancestor->removeComponentListener (this);

    watchedAncestors.clear();

    for (auto* c = getParentComponent(); c != nullptr; c = c->getParentComponent())
    {
        c->addComponentListener (this);
        watchedAncestors.add (c);
    }
}

void StringRollComponent::updateTimerState()
{
    /*  Visible up the tree, rather than isShowing(): the latter also needs a
        window, which the tests do not have, and a roll in an editor about to
        be shown may as well be current on its first paint. */
    bool visible = true;

    for (auto* c = static_cast<const juce::Component*> (this); c != nullptr && visible; c = c->getParentComponent())
        visible = c->isVisible();

    if (visible && ! isTimerRunning())
    {
        runningHz = wantedRefreshHz();
        motion.startTimerHz (*this, runningHz);   // cpu-quality-modes 6
        refresh();   // what happened while it was hidden, now rather than a tick later
    }
    else if (! visible && isTimerRunning())
    {
        motion.stopTimer();
    }
}

int StringRollComponent::wantedRefreshHz() const
{
    return AccessibilitySettings::get().isReducedMotion() ? kSlowHz : kFastHz;
}

int StringRollComponent::getRefreshHz() const noexcept
{
    const int interval = getTimerInterval();
    return interval > 0 ? juce::roundToInt (1000.0 / interval) : 0;
}

void StringRollComponent::rebuildLanes (int count)
{
    numStrings = count;
    lanes.clear();

    for (int s = 0; s < numStrings; ++s)
        addAndMakeVisible (lanes.add (new Lane (*this, s)));

    resized();
}

juce::Component& StringRollComponent::getLane (int stringIndex)
{
    return *lanes[juce::jlimit (0, lanes.size() - 1, stringIndex)];
}

//==============================================================================
void StringRollComponent::timerCallback()
{
    // Reduced motion can change while the roll is open (Options); follow it
    // the way the guitar illustration does, per tick.
    const int hz = wantedRefreshHz();

    if (hz != runningHz)
    {
        runningHz = hz;
        motion.startTimerHz (*this, hz);
    }

    refresh();
}

void StringRollComponent::refresh()
{
    auto& engine = processor.getEngine();
    auto& take = processor.getPerformanceCapture();
    bool changed = false;

    const int strings = juce::jlimit (1, kMaxLanes, engine.getNumStrings());

    if (strings != numStrings)
    {
        rebuildLanes (strings);
        changed = true;
    }

    // ---- names, from the tuning engine (the nut, as a guitarist labels them) ---
    const auto open = PerformanceCapture::getOpenNotes (engine.getTuningEngine(), numStrings);

    for (int s = 0; s < numStrings; ++s)
    {
        auto name = TuningEngine::noteName (open[(size_t) s]);

        if (name != names[(size_t) s])
        {
            names[(size_t) s] = name;
            lanes[s]->setTitle (tr ("stringRoll.lane.title", { { "n", juce::String (s + 1) }, { "note", name } }));
            lanes[s]->setHelpText (tr ("stringRoll.lane.help"));
            changed = true;
        }
    }

    // ---- the live edge ----------------------------------------------------------
    reducedMotion = AccessibilitySettings::get().isReducedMotion();

    for (int s = 0; s < numStrings; ++s)
    {
        // The fretboard's scaling: a level of about 0.055 is fully lit.
        const float target = (float) juce::jlimit (0.0, 1.0, engine.getStringLevel (s) * 18.0);
        auto& g = glow[(size_t) s];
        const float before = g;

        // Fast attack so the pluck shows at once, slower decay so it reads as a
        // ringing string; reduced motion hard-sets it (accessibility 5).
        g = reducedMotion ? target : g + (target - g) * (target > g ? 0.85f : 0.3f);

        if (g < 0.005f)
            g = 0.0f;

        if (std::abs (g - before) > 0.004f)
            changed = true;
    }

    // ---- the take ---------------------------------------------------------------
    const bool wasOff = captureOff;
    captureOff = take.getState() == CaptureState::off;
    changed = changed || wasOff != captureOff;

    const auto& notes = take.getNotes();
    sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;

    if (notes.empty())
    {
        anchored = false;
        nowSample = 0;
    }
    else
    {
        juce::int64 newest = 0;

        for (const auto& note : notes)
            newest = juce::jmax (newest, note.startSample, note.endSample);

        // Wall time carries the clock between drains; a record from beyond it
        // (we fell behind) re-anchors it. Nothing ever moves it backwards, so
        // the bars never jitter when a drain lands late.
        const double wall = juce::Time::getMillisecondCounterHiRes();
        auto estimate = anchored ? anchorSample + (juce::int64) ((wall - anchorWallMs) * 0.001 * sampleRate)
                                 : newest;

        if (! anchored || newest > estimate)
        {
            anchored = true;
            anchorSample = newest;
            anchorWallMs = wall;
            estimate = newest;
        }

        nowSample = estimate;
    }

    // The window: four bars at the host's tempo while the newest note was played
    // to a rolling transport, eight seconds of free play otherwise.
    musical = ! notes.empty() && notes.back().musical;
    bpm = juce::jlimit (20.0, 300.0, processor.getHostTempo());

    if (! take.getMeters().empty())
    {
        numerator = juce::jmax (1, take.getMeters().back().numerator);
        denominator = juce::jmax (1, take.getMeters().back().denominator);
    }

    const double beatSeconds = 60.0 / bpm * 4.0 / (double) denominator;
    windowSeconds = musical ? kBarsInWindow * numerator * beatSeconds : kFreeWindowSeconds;

    const auto windowStart = nowSample - (juce::int64) (windowSeconds * sampleRate);
    int count = 0;

    for (const auto& note : notes)
        if (note.startSample <= nowSample && (note.endSample < 0 || note.endSample >= windowStart))
            ++count;

    changed = changed || count != visibleNotes;
    visibleNotes = count;

    // Anything in the window is scrolling, so that alone is a reason to paint.
    if (changed || visibleNotes > 0)
        repaint();
}

//==============================================================================
juce::Rectangle<int> StringRollComponent::rollArea() const
{
    return getLocalBounds().reduced (0, 2).withTrimmedLeft (kNameColumn).withTrimmedRight (2);
}

juce::Rectangle<int> StringRollComponent::laneBounds (int stringIndex) const
{
    const auto roll = rollArea();
    const float h = (float) roll.getHeight() / (float) juce::jmax (1, numStrings);

    return juce::Rectangle<float> ((float) roll.getX(), (float) roll.getY() + h * (float) stringIndex,
                                   (float) roll.getWidth(), h).toNearestInt();
}

void StringRollComponent::resized()
{
    for (int s = 0; s < lanes.size(); ++s)
        lanes[s]->setBounds (laneBounds (s));
}

float StringRollComponent::xForSample (juce::int64 sample, juce::Rectangle<int> roll) const
{
    const double secondsAgo = (double) (nowSample - sample) / sampleRate;
    return (float) roll.getRight() - (float) (secondsAgo / juce::jmax (0.01, windowSeconds)) * (float) roll.getWidth();
}

int StringRollComponent::fretAtY (float y, float laneHeight) const
{
    // Up is up the neck, as pitch rises up a piano roll. Pixel centres, so the
    // lane's bottom row is the open string and its top row the twelfth fret.
    const float t = 1.0f - juce::jlimit (0.0f, 1.0f, (y + 0.5f) / juce::jmax (1.0f, laneHeight));
    return juce::jlimit (0, kFretRange, juce::roundToInt (t * (float) kFretRange));
}

double StringRollComponent::velocityAtX (float x, float laneWidth) const
{
    return juce::jlimit (0.3, 1.0, 0.35 + 0.65 * (double) (x / juce::jmax (1.0f, laneWidth)));
}

juce::String StringRollComponent::getLaneTooltip (int stringIndex, int fret) const
{
    const int s = juce::jlimit (0, numStrings - 1, stringIndex);

    return tr ("stringRoll.lane.tooltip", { { "n", juce::String (s + 1) },
                                            { "note", names[(size_t) s] },
                                            { "fret", juce::String (juce::jmax (0, fret)) } });
}

void StringRollComponent::setHover (int stringIndex, int fret)
{
    if (stringIndex != hoverString || fret != hoverFret)
    {
        hoverString = stringIndex;
        hoverFret = fret;
        repaint();
    }
}

//==============================================================================
void StringRollComponent::pluck (int stringIndex, int fret, double velocity)
{
    if (! juce::isPositiveAndBelow (stringIndex, numStrings))
        return;

    // A capo raises the floor, as the fretboard's click does.
    const int capo = processor.getEngine().getTuningEngine().getCapoFret();
    processor.triggerPreviewNote (stringIndex, (double) juce::jmax (fret, capo), velocity);
}

void StringRollComponent::release (int stringIndex)
{
    if (juce::isPositiveAndBelow (stringIndex, numStrings))
        processor.releasePreviewNote (stringIndex);
}

//==============================================================================
void StringRollComponent::paint (juce::Graphics& g)
{
    const auto roll = rollArea();

    if (roll.getWidth() < 8 || roll.getHeight() < 8)
        return;

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

    // ---- lanes and names --------------------------------------------------------
    for (int s = 0; s < numStrings; ++s)
    {
        const auto lane = laneBounds (s);

        g.setColour (s == hoverString ? Palette::accent.withAlpha (0.10f)
                                      : Palette::panel.withAlpha (s % 2 == 0 ? 0.35f : 0.15f));
        g.fillRect (lane);

        g.setColour (Palette::edge.withAlpha (0.6f));
        g.drawHorizontalLine (lane.getBottom() - 1, (float) roll.getX(), (float) roll.getRight());

        g.setColour (Palette::textMuted);
        g.setFont (Fonts::mono (9.0f));
        g.drawText (names[(size_t) s], 2, lane.getY(), kNameColumn - 4, lane.getHeight(),
                    juce::Justification::centredRight, false);
    }

    const auto& notes = processor.getPerformanceCapture().getNotes();
    const auto windowStart = nowSample - (juce::int64) (windowSeconds * sampleRate);

    // ---- the grid: bars and beats to a rolling transport, seconds in free play --
    if (musical && ! notes.empty())
    {
        const auto& anchor = notes.back();
        const double beat = 4.0 / (double) denominator;
        const double bar = beat * numerator;
        const double samplesPerPpq = 60.0 / bpm * sampleRate;

        auto ppqAt = [&] (juce::int64 sample) { return anchor.startPpq + (double) (sample - anchor.startSample) / samplesPerPpq; };
        const double firstBar = std::floor (ppqAt (windowStart) / bar) * bar;
        const double lastPpq = ppqAt (nowSample);

        for (double ppq = firstBar; ppq <= lastPpq; ppq += beat)
        {
            const bool onBar = std::abs (std::fmod (ppq - firstBar, bar)) < 1.0e-6;
            const auto x = xForSample (anchor.startSample + (juce::int64) ((ppq - anchor.startPpq) * samplesPerPpq), roll);

            if (x < (float) roll.getX())
                continue;

            g.setColour (Palette::edgeBright.withAlpha (onBar ? 0.7f : 0.25f));
            g.drawVerticalLine ((int) x, (float) roll.getY(), (float) roll.getBottom());
        }
    }
    else if (visibleNotes > 0)
    {
        const auto firstSecond = (juce::int64) std::ceil ((double) windowStart / sampleRate);

        for (auto sec = firstSecond; sec * sampleRate <= (double) nowSample; ++sec)
        {
            g.setColour (Palette::edgeBright.withAlpha (0.25f));
            g.drawVerticalLine ((int) xForSample ((juce::int64) ((double) sec * sampleRate), roll),
                                (float) roll.getY(), (float) roll.getBottom());
        }
    }

    // ---- the notes ---------------------------------------------------------------
    g.setFont (Fonts::mono (9.0f));

    for (const auto& note : notes)
    {
        if (note.startSample > nowSample || (note.endSample >= 0 && note.endSample < windowStart))
            continue;

        if (! juce::isPositiveAndBelow (note.stringIndex, numStrings))
            continue;

        const auto lane = laneBounds (note.stringIndex).toFloat().reduced (0.0f, 3.0f);
        const float x0 = juce::jmax ((float) roll.getX(), xForSample (note.startSample, roll));
        const float x1 = note.isSounding() ? (float) roll.getRight() : xForSample (note.endSample, roll);
        const auto bar = juce::Rectangle<float> (x0, lane.getY(), juce::jmax (3.0f, x1 - x0), lane.getHeight());

        const float alpha = (float) juce::jlimit (0.35, 1.0, 0.35 + 0.65 * note.velocity);
        g.setColour ((note.isSounding() ? Palette::accentBright : Palette::accent).withAlpha (alpha));
        g.fillRoundedRectangle (bar, juce::jmin (3.0f, bar.getHeight() * 0.5f));

        // The fret, like tab, when the bar can carry it.
        if (bar.getWidth() >= (float) kMinLabelWidth && bar.getHeight() >= 9.0f)
        {
            g.setColour (Palette::backgroundDeep);
            g.drawText (juce::String (juce::roundToInt (note.fret)), bar.toNearestInt(),
                        juce::Justification::centred, false);
        }
    }

    // ---- the live edge -----------------------------------------------------------
    for (int s = 0; s < numStrings; ++s)
    {
        const float level = glow[(size_t) s];

        if (level <= 0.0f)
            continue;

        auto lane = laneBounds (s).toFloat().reduced (0.0f, 2.0f);

        if (reducedMotion)
        {
            // A static marker: the feedback without the animation (accessibility 5).
            g.setColour (Palette::accentBright.withAlpha (level));
            g.fillRect (lane.removeFromRight (4.0f));
        }
        else
        {
            auto edge = lane.removeFromRight (16.0f);
            g.setGradientFill (juce::ColourGradient (Palette::accentBright.withAlpha (0.0f), edge.getX(), edge.getY(),
                                                     Palette::accentBright.withAlpha (level), edge.getRight(), edge.getY(),
                                                     false));
            g.fillRect (edge);
        }
    }

    // ---- gui-integration 14: the empty state says what fills it ------------------
    if (visibleNotes == 0)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (11.0f));
        g.drawFittedText (tr (captureOff ? "stringRoll.empty.captureOff" : "stringRoll.empty.play"),
                          roll.reduced (Metrics::grid, 0), juce::Justification::centred, 2);
    }
}

} // namespace luthier
