#include "PerformanceAssistUi.h"
#include "UiPreferences.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Support/Edition.h"

namespace luthier
{

namespace
{
    const juce::String kDot (juce::CharPointer_UTF8 (" \xc2\xb7 "));

    juce::RangedAudioParameter* param (LuthierAudioProcessor& p, const char* id)
    {
        return p.getState().getParameter (id);
    }

    float plain (LuthierAudioProcessor& p, const char* id)
    {
        if (auto* raw = p.getState().getRawParameterValue (id))
            return raw->load();

        return 0.0f;
    }

    void write (LuthierAudioProcessor& p, const char* id, float plainValue)
    {
        if (auto* prm = param (p, id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
            prm->endChangeGesture();
        }
    }

    /** One colour per string, distinct at a glance; the glyph carries the
        meaning, so monochrome palettes still read (7.3). */
    juce::Colour stringColour (int s)
    {
        return Palette::accent.withRotatedHue ((float) s * 0.09f).withMultipliedSaturation (0.9f);
    }
}

//==============================================================================
bool AssistUi::isEnabled (LuthierAudioProcessor& p)          { return plain (p, ParamIDs::aaEnabled) > 0.5f; }
int AssistUi::getRules (LuthierAudioProcessor& p)            { return (int) plain (p, ParamIDs::aaRules); }
int AssistUi::getStyle (LuthierAudioProcessor& p)            { return (int) plain (p, ParamIDs::aaStyle); }
int AssistUi::getAmountPercent (LuthierAudioProcessor& p)    { return juce::roundToInt (plain (p, ParamIDs::aaAmount)); }

void AssistUi::setEnabled (LuthierAudioProcessor& p, bool on)
{
    if (isEnabled (p) == on)
        return;

    const LuthierAudioProcessor::ScopedUndoAction undo (p, on ? "Turn on Performance Assist" : "Turn off Performance Assist");
    write (p, ParamIDs::aaEnabled, on ? 1.0f : 0.0f);
}

void AssistUi::toggle (LuthierAudioProcessor& p)
{
    setEnabled (p, ! isEnabled (p));
}

void AssistUi::setRule (LuthierAudioProcessor& p, int bitIndex, bool on)
{
    const int rules = getRules (p);
    const int bit = 1 << juce::jlimit (0, (int) AssistRule::numRules - 1, bitIndex);
    const int updated = on ? (rules | bit) : (rules & ~bit);

    if (updated == rules)
        return;

    const LuthierAudioProcessor::ScopedUndoAction undo (
        p, juce::String (on ? "Turn on " : "Turn off ") + AssistRule::getName (bitIndex) + " rule");
    write (p, ParamIDs::aaRules, (float) updated);
}

juce::String AssistUi::statusText (LuthierAudioProcessor& p)
{
    return juce::String (isEnabled (p) ? "ON" : "OFF") + kDot + AutoArticulationStyles::get (getStyle (p)).name
           + kDot + juce::String (getAmountPercent (p)) + "%";
}

juce::String AssistUi::accessibleSummary (LuthierAudioProcessor& p)
{
    return juce::String ("Performance Assist, ") + (isEnabled (p) ? "on" : "off") + ", style "
           + AutoArticulationStyles::get (getStyle (p)).name + ", amount " + juce::String (getAmountPercent (p)) + " percent";
}

juce::String AssistUi::noticeText (LuthierAudioProcessor& p)
{
    const bool controller = (int) plain (p, ParamIDs::playingMode) == (int) PlayingMode::GuitarController
                            || plain (p, ParamIDs::mpeEnabled) > 0.5f;

    if (controller)
        return "Off in Guitar Controller / MPE mode: your controller already articulates.";

    auto& rhythm = p.getEngine().getRhythmEngine();

    if (rhythm.isEnabled() && rhythm.isDriving())
        return "The rhythm engine is playing: its notes are not assisted. The Tune melody still is.";

    return {};
}

bool AssistUi::showLabels()           { return UiPreferences::get().getBool ("visualAids.assistLabels", true); }
void AssistUi::setShowLabels (bool on) { UiPreferences::get().setBool ("visualAids.assistLabels", on); }

void AssistUi::drain (LuthierAudioProcessor& p, bool force)
{
    p.getAssistLog().drainIfDue (p.getEngine().getAutoArticulator().getFeed(), p.getEngine().getSampleRate(), force);
}

void AssistUi::showLockedStyleNotice (int styleIndex, juce::Component* near)
{
    const auto& locked = AutoArticulationStyles::get (styleIndex);
    const auto& plays = AutoArticulationStyles::get (AutoArticulationStyles::nearestFreeStyle (styleIndex));

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Luthier Pro",
                                            juce::String (locked.name) + " is a Pro style. This edition plays it as "
                                              + plays.name + ", and your choice is kept for Pro.",
                                            "OK", near);
}

//==============================================================================
AssistLabelOverlay::AssistLabelOverlay (LuthierAudioProcessor& p, juce::Component& h, PositionFn position)
    : processor (p), host (h), positionOf (std::move (position))
{
    setInterceptsMouseClicks (false, false);
    setAccessible (false);
    host.addAndMakeVisible (this);
    host.addComponentListener (this);
    setBounds (host.getLocalBounds());
    motion.startTimerHz (*this, 30);   // gui-engine-dataflow: the fretboard's own 30 Hz; cpu-quality-modes 6
}

AssistLabelOverlay::~AssistLabelOverlay()
{
    stopTimer();
    host.removeComponentListener (this);
}

void AssistLabelOverlay::componentMovedOrResized (juce::Component&, bool, bool)
{
    setBounds (host.getLocalBounds());
    toFront (false);
}

bool AssistLabelOverlay::reducedMotion() const
{
    if (reducedMotionOverride >= 0)
        return reducedMotionOverride != 0;

    // cpu-quality-modes 6: Low quality (motion Off) draws the labels still, as
    // Reduced motion does.
    return AccessibilitySettings::get().isReducedMotion()
        || ! AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition);
}

std::vector<AssistLabelOverlay::Drawn> AssistLabelOverlay::computeLabels (double nowMs) const
{
    std::vector<Drawn> drawn;

    if (! AssistUi::showLabels() || positionOf == nullptr)
        return drawn;

    const bool still = reducedMotion();

    processor.getAssistLog().forEachLive (nowMs, [&] (const AssistDecisionLog::Decision& d, double age)
    {
        if (d.label == AssistLabel::muteLift || d.label == AssistLabel::none)
            return;

        Drawn l;
        l.opacity = still ? 0.9f : (float) (0.9 * (1.0 - age / AssistDecisionLog::kLabelMs));
        l.glyph = juce::String (juce::CharPointer_UTF8 (getAssistLabelGlyph (d.label)));

        if (d.string < 0)
        {
            // A strum: a vertical arrow over the struck strings, at the nut.
            int lo = -1, hi = -1;

            for (int s = 0; s < 16; ++s)
                if (((d.stringMask >> s) & 1) != 0)
                {
                    if (lo < 0) lo = s;
                    hi = s;
                }

            if (lo < 0)
                return;

            auto a = positionOf (lo, 0.0), b = positionOf (hi, 0.0);
            a.x -= 10.0f;
            b.x -= 10.0f;
            const bool up = d.label == AssistLabel::strumUp;
            l.strum = true;
            l.arrow = up ? juce::Line<float> (b, a) : juce::Line<float> (a, b);
            l.at = a;
        }
        else
        {
            l.stringIndex = d.string;
            l.fret = d.fret;
            l.at = positionOf (d.string, d.fret);
        }

        drawn.push_back (l);
    });

    return drawn;
}

void AssistLabelOverlay::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6
    const auto labels = computeLabels (processor.getAssistLog().nowMs());

    for (const auto& l : labels)
    {
        if (l.strum)
        {
            g.setColour (Palette::accent.withAlpha (l.opacity));
            juce::Path arrow;
            arrow.addArrow (l.arrow, 2.0f, 8.0f, 6.0f);
            g.fillPath (arrow);
            continue;
        }

        const auto font = Fonts::ui (10.0f, true);
        const float w = juce::jmax (14.0f, (float) juce::GlyphArrangement::getStringWidthInt (font, l.glyph) + 8.0f);
        const auto box = juce::Rectangle<float> (w, 14.0f).withCentre (l.at.translated (0.0f, -9.0f));

        g.setColour (stringColour (l.stringIndex).withAlpha (l.opacity));
        g.fillRoundedRectangle (box, 7.0f);
        g.setColour (Palette::backgroundDeep.withAlpha (l.opacity));
        g.setFont (font);
        g.drawText (l.glyph, box, juce::Justification::centred, false);
    }

    drewLast = ! labels.empty();
}

void AssistLabelOverlay::timerCallback()
{
    AssistUi::drain (processor);

    // Repaint while anything is live, and once more to clear the last frame.
    const bool any = ! computeLabels (processor.getAssistLog().nowMs()).empty();

    if (any || drewLast)
        repaint();

    if (! any)
        drewLast = false;
}

//==============================================================================
AssistStyleBox::AssistStyleBox (LuthierAudioProcessor& p)
    : LuthierChoice ("Style"), processor (p)
{
    attachTo (processor, ParamIDs::aaStyle,
              "Performance Assist's style: which slurs, vibrato, strums, mutes and ornaments it plays.");
    setLabelVisible (false);

    // Section 11: in Free the genre styles are Pro. They stay choosable - the
    // value is kept for Pro - and say what plays instead.
    if (! Editions::isPro())
        for (int i = 0; i < AutoArticulationStyles::kNumStyles; ++i)
            if (! AutoArticulationStyles::isInFree (i))
                getComboBox().changeItemText (i + 1, juce::String (AutoArticulationStyles::get (i).name) + " (Pro)");

    getComboBox().onChange = [this]
    {
        const int index = getComboBox().getSelectedItemIndex();

        if (! Editions::isPro() && index >= 0 && ! AutoArticulationStyles::isInFree (index))
            AssistUi::showLockedStyleNotice (index, this);
    };

    getComboBox().setTitle ("Assist style");
}

//==============================================================================
namespace
{
    /** The pill's popover: Amount, the style's one line, and the way to RHYTHM. */
    class AssistPopover : public juce::Component
    {
    public:
        AssistPopover (LuthierAudioProcessor& p, std::function<void()> openRhythm)
        {
            amount.attachTo (p, ParamIDs::aaAmount, "How much Performance Assist does: 0 is off, 100 is everything.");
            addAndMakeVisible (amount);

            description.setText (AutoArticulationStyles::get (AssistUi::getStyle (p)).description, juce::dontSendNotification);
            description.setFont (Fonts::ui (11.0f));
            description.setColour (juce::Label::textColourId, Palette::textMuted);
            description.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (description);

            more.setButtonText ("More in RHYTHM tab");
            more.onClick = [openRhythm]
            {
                if (openRhythm != nullptr)
                    openRhythm();
            };
            addAndMakeVisible (more);

            setSize (230, 110);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            amount.setBounds (r.removeFromLeft (70));
            r.removeFromLeft (6);
            more.setBounds (r.removeFromBottom (24));
            description.setBounds (r);
        }

    private:
        LuthierKnob amount { "Amount", LuthierKnob::Size::Small };
        juce::Label description;
        juce::TextButton more;
    };
}

AssistPill::AssistPill (LuthierAudioProcessor& p)
    : processor (p)
{
    setWantsKeyboardFocus (true);
    setTitle ("Performance Assist");
    setDescription (AssistUi::accessibleSummary (processor));
    setTooltip ("Performance Assist: turns plain MIDI into a guitar performance. Click to switch it on or off; "
                "hold (or press Down) for the Amount.");
    setSize (kWidth, kHeight);
    motion.startTimerHz (*this, 30);   // cpu-quality-modes 6
}

AssistPill::~AssistPill()
{
    stopTimer();
    closePopover();
}

bool AssistPill::isDotFlashing() const
{
    return nowMs() < flashUntilMs;
}

void AssistPill::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6
    const bool on = AssistUi::isEnabled (processor);
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float radius = r.getHeight() * 0.5f;

    if (on)
    {
        g.setColour (Palette::accent);
        g.fillRoundedRectangle (r, radius);
    }
    else
    {
        g.setColour (Palette::accent.withAlpha (0.8f));
        g.drawRoundedRectangle (r, radius, 1.2f);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::textPrimary.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (1.5f), radius - 1.5f, 1.0f);
    }

    const auto textColour = on ? Palette::backgroundDeep : Palette::accent;
    auto area = r.reduced (6.0f, 0.0f);
    const auto dot = area.removeFromRight (7.0f).withSizeKeepingCentre (6.0f, 6.0f);

    g.setColour (textColour);
    g.setFont (Fonts::ui (11.0f, true));
    g.drawText ("AUTO", area, juce::Justification::centred, false);

    // The state dot: lit while on, and flashing bright on every decision.
    const bool flash = isDotFlashing();
    g.setColour (flash ? Palette::textPrimary : (on ? textColour : Palette::accent.withAlpha (0.35f)));
    g.fillEllipse (dot.expanded (flash ? 1.0f : 0.0f));
}

void AssistPill::mouseDown (const juce::MouseEvent&)
{
    pressedAtMs = nowMs();
    holdFired = false;
}

void AssistPill::mouseUp (const juce::MouseEvent&)
{
    pressedAtMs = -1.0;

    if (! holdFired)
        AssistUi::toggle (processor);

    holdFired = false;
    repaint();
}

bool AssistPill::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        AssistUi::toggle (processor);
        repaint();
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        showPopover();
        return true;
    }

    return false;
}

void AssistPill::showPopover()
{
    if (isPopoverOpen())
        return;

    closePopover();

    popoverContent = std::make_unique<AssistPopover> (processor, [this]
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<AssistPill> (this)]
        {
            if (safe == nullptr)
                return;

            safe->closePopover();

            if (safe->onOpenRhythmTab != nullptr)
                safe->onOpenRhythmTab();
        });
    });

    auto* parent = getTopLevelComponent();
    const auto area = parent != nullptr && parent != this ? parent->getLocalArea (this, getLocalBounds())
                                                          : getLocalBounds();

    popoverBox = std::make_unique<juce::CallOutBox> (*popoverContent, area, parent != this ? parent : nullptr);
    popoverBox->setDismissalMouseClicksAreAlwaysConsumed (true);
    popoverBox->setVisible (true);
    popoverBox->toFront (true);
}

void AssistPill::closePopover()
{
    popoverBox.reset();
    popoverContent.reset();
}

void AssistPill::timerCallback()
{
    const double now = nowMs();

    // Hold: the popover opens while the button is still down.
    if (pressedAtMs >= 0.0 && ! holdFired && now - pressedAtMs >= kHoldMs)
    {
        holdFired = true;
        showPopover();
    }

    AssistUi::drain (processor);

    const double last = processor.getAssistLog().getLastDecisionMs();

    if (last > lastSeenDecisionMs)
    {
        lastSeenDecisionMs = last;
        flashUntilMs = now + kFlashMs;
    }

    const auto summary = AssistUi::accessibleSummary (processor);

    if (summary != getDescription())
        setDescription (summary);

    repaint();
}

//==============================================================================
PerformanceAssistGroup::PerformanceAssistGroup (LuthierAudioProcessor& p)
    : processor (p), style (p)
{
    heading.setText ("PLAYING", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    status.setFont (Fonts::ui (11.0f, true));
    status.setColour (juce::Label::textColourId, Palette::textPrimary);
    status.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (status);

    collapse.setTooltip ("Collapse or expand the PLAYING group");
    collapse.setTitle ("Collapse Playing group");
    collapse.onClick = [this] { setCollapsed (! isCollapsed()); };
    addAndMakeVisible (collapse);

    help.setTooltip ("Help: Performance Assist");
    help.setTitle ("Performance Assist help");
    help.onClick = [this]
    {
        if (onHelp != nullptr)
            onHelp();
    };
    addAndMakeVisible (help);

    // 7.2: a mirror of the playing mode; the Easy strip stays canonical.
    mode.attachTo (processor, ParamIDs::playingMode,
                   "Mono routes every note to one string; Poly voices chords; Guitar Controller maps channels to strings.");
    mode.setLabelVisible (false);
    addAndMakeVisible (mode);

    enable.setTitle ("Performance Assist");
    enable.setTooltip ("Turns plain MIDI into a guitar performance: positions, slurs, vibrato, strums, "
                       "palm mutes and ornaments. Shortcut: A.");
    enable.onClick = [this] { AssistUi::setEnabled (processor, enable.getToggleState()); };
    addAndMakeVisible (enable);

    addAndMakeVisible (style);

    amount.attachTo (processor, ParamIDs::aaAmount, "How much Performance Assist does: 0 is off, 100 is everything.");
    addAndMakeVisible (amount);

    rulesLabel.setText ("Rules:", juce::dontSendNotification);
    recentLabel.setText ("Recent:", juce::dontSendNotification);

    for (auto* l : { &rulesLabel, &recentLabel })
    {
        l->setFont (Fonts::ui (11.0f));
        l->setColour (juce::Label::textColourId, Palette::textMuted);
        addAndMakeVisible (*l);
    }

    const bool pro = Editions::isPro();

    for (int bit = 0; bit < (int) AssistRule::numRules; ++bit)
    {
        auto t = std::make_unique<juce::ToggleButton> (AssistRule::getName (bit));
        t->setTitle (AssistRule::getName (bit));
        t->setDescription (AssistRule::getDescription (bit));
        t->setTooltip (juce::String (AssistRule::getDescription (bit)) + (pro ? "" : " The per-rule switches are Pro."));
        t->setEnabled (pro);
        t->onClick = [this, bit] { AssistUi::setRule (processor, bit, rules[(size_t) bit]->getToggleState()); };
        addAndMakeVisible (*t);
        rules[(size_t) bit] = std::move (t);
    }

    list.setModel (this);
    list.setRowHeight (16);
    list.setTitle ("Performance Assist decisions");
    list.setDescription ("What Performance Assist decided, newest first");
    list.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    addAndMakeVisible (list);

    notice.setFont (Fonts::ui (11.0f));
    notice.setColour (juce::Label::textColourId, Palette::accentBright);
    addAndMakeVisible (notice);

    // 8: tab order - mode, toggle, style, amount, rules in reading order, list.
    int order = 1;

    for (auto* c : getFocusOrder())
        c->setExplicitFocusOrder (order++);

    setCollapsed (processor.getUiState().playingGroupCollapsed);
    refresh();
    motion.startTimerHz (*this, 30);   // cpu-quality-modes 6
}

PerformanceAssistGroup::~PerformanceAssistGroup()
{
    stopTimer();
    list.setModel (nullptr);
}

std::vector<juce::Component*> PerformanceAssistGroup::getFocusOrder()
{
    std::vector<juce::Component*> order { &mode, &enable, &style, &amount };

    for (auto& r : rules)
        order.push_back (r.get());

    order.push_back (&list);
    return order;
}

bool PerformanceAssistGroup::isCollapsed() const noexcept
{
    return processor.getUiState().playingGroupCollapsed;
}

void PerformanceAssistGroup::setCollapsed (bool shouldCollapse)
{
    processor.getUiState().playingGroupCollapsed = shouldCollapse;
    collapse.setButtonText (shouldCollapse ? ">" : "v");

    for (juce::Component* c : { (juce::Component*) &mode, (juce::Component*) &enable, (juce::Component*) &style,
                                (juce::Component*) &amount, (juce::Component*) &rulesLabel,
                                (juce::Component*) &recentLabel, (juce::Component*) &list, (juce::Component*) &notice })
        c->setVisible (! shouldCollapse);

    for (auto& r : rules)
        r->setVisible (! shouldCollapse);

    resized();

    if (onLayoutChanged != nullptr)
        onLayoutChanged();
}

int PerformanceAssistGroup::preferredHeight() const
{
    return 22 + (isCollapsed() ? 0 : 4 + 40 + 4 + 2 * 20 + 4 + 14 + 6 * 16 + 4 + 16 + 4);
}

void PerformanceAssistGroup::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6
    g.setColour (Palette::panelRaised);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    g.setColour (Palette::edge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);

    // The heading's rule, as the other groups draw it.
    const auto h = heading.getBounds();
    g.setColour (Palette::edge);
    g.drawHorizontalLine (h.getCentreY(), (float) h.getRight() + 4.0f, (float) status.getX() - 4.0f);
}

void PerformanceAssistGroup::resized()
{
    auto r = getLocalBounds().reduced (4, 2);

    {
        auto top = r.removeFromTop (20);
        heading.setBounds (top.removeFromLeft (70));
        help.setBounds (top.removeFromRight (20).reduced (1));
        collapse.setBounds (top.removeFromRight (20).reduced (1));
        status.setBounds (top.removeFromRight (juce::jmin (150, top.getWidth())));
    }

    if (isCollapsed())
        return;

    r.removeFromTop (4);

    {
        auto row = r.removeFromTop (40);
        amount.setBounds (row.removeFromRight (56));
        auto line = row.withSizeKeepingCentre (row.getWidth(), 26);
        mode.setBounds (line.removeFromLeft (juce::jmax (80, line.getWidth() / 4)));
        line.removeFromLeft (4);
        enable.setBounds (line.removeFromLeft (juce::jmax (130, line.getWidth() / 2)));
        line.removeFromLeft (4);
        style.setBounds (line);
    }

    r.removeFromTop (4);

    {
        auto area = r.removeFromTop (40);
        rulesLabel.setBounds (area.removeFromLeft (44).removeFromTop (20));
        const int perRow = 5;
        const int w = area.getWidth() / perRow;

        for (int bit = 0; bit < (int) AssistRule::numRules; ++bit)
        {
            const int rowIndex = bit / perRow, column = bit % perRow;
            rules[(size_t) bit]->setBounds (area.getX() + column * w, area.getY() + rowIndex * 20, w, 20);
        }
    }

    r.removeFromTop (4);
    recentLabel.setBounds (r.removeFromTop (14));
    list.setBounds (r.removeFromTop (6 * 16));
    r.removeFromTop (4);
    notice.setBounds (r.removeFromTop (16));
}

void PerformanceAssistGroup::timerCallback()
{
    status.setText (AssistUi::statusText (processor), juce::dontSendNotification);

    const auto noticeText = AssistUi::noticeText (processor);

    if (notice.getText() != noticeText)
        notice.setText (noticeText, juce::dontSendNotification);

    enable.setToggleState (AssistUi::isEnabled (processor), juce::dontSendNotification);

    const int bits = AssistUi::getRules (processor);

    for (int bit = 0; bit < (int) AssistRule::numRules; ++bit)
        rules[(size_t) bit]->setToggleState (((bits >> bit) & 1) != 0, juce::dontSendNotification);

    // 10: the list repaints only when the feed delivered.
    AssistUi::drain (processor);
    const auto count = processor.getAssistLog().getDecisionCount();

    if (count != listedCount)
    {
        listedCount = count;
        list.updateContent();
        list.repaint();
    }
}

int PerformanceAssistGroup::getNumRows()
{
    return juce::jmax (1, processor.getAssistLog().getNumListed());
}

juce::String PerformanceAssistGroup::getRowText (int row) const
{
    auto& log = processor.getAssistLog();

    if (log.getNumListed() == 0)
        return row == 0 ? juce::String (AssistUi::kEmptyListText) : juce::String();

    return juce::isPositiveAndBelow (row, log.getNumListed()) ? log.describe (log.getListed (row), false) : juce::String();
}

juce::String PerformanceAssistGroup::getNameForRow (int row)
{
    auto& log = processor.getAssistLog();

    if (log.getNumListed() == 0)
        return AssistUi::kEmptyListText;

    return juce::isPositiveAndBelow (row, log.getNumListed()) ? log.describe (log.getListed (row), true) : juce::String();
}

void PerformanceAssistGroup::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (selected)
        g.fillAll (Palette::accent.withAlpha (0.25f));

    const bool empty = processor.getAssistLog().getNumListed() == 0;
    g.setColour (empty ? Palette::textMuted : Palette::textPrimary);
    g.setFont (Fonts::ui (11.0f));
    g.drawText (getRowText (row), 4, 0, width - 8, height, juce::Justification::centredLeft, true);
}

} // namespace luthier
