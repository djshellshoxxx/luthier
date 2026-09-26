#include "QualityOptions.h"
#include "Theme.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Localisation.h"

namespace luthier
{

namespace
{
    constexpr int kRow = 26, kGap = 4, kLabelWidth = 110, kStackBelow = 520;

    const char* describeKey (QualityChoice c)
    {
        switch (c)
        {
            case QualityChoice::Auto:   return "quality.describe.auto";
            case QualityChoice::Medium: return "quality.describe.medium";
            case QualityChoice::Low:    return "quality.describe.low";
            case QualityChoice::High:
            default:                    return "quality.describe.high";
        }
    }

    const char* nameKey (QualityChoice c)
    {
        switch (c)
        {
            case QualityChoice::Auto:   return "quality.level.auto";
            case QualityChoice::Medium: return "quality.level.medium";
            case QualityChoice::Low:    return "quality.level.low";
            case QualityChoice::High:
            default:                    return "quality.level.high";
        }
    }

    void muted (juce::Label& l, float size = 11.0f)
    {
        l.setColour (juce::Label::textColourId, Palette::textMuted);
        l.setFont (Fonts::ui (size));
        l.setJustificationType (juce::Justification::centredLeft);
    }
}

//==============================================================================
int QualityOptions::pillIndex (QualityChoice c) noexcept
{
    // Auto | High | Medium | Low, the spec's order.
    switch (c)
    {
        case QualityChoice::Auto:   return 0;
        case QualityChoice::High:   return 1;
        case QualityChoice::Medium: return 2;
        case QualityChoice::Low:
        default:                    return 3;
    }
}

QualityChoice QualityOptions::pillChoice (int index) noexcept
{
    const QualityChoice order[] = { QualityChoice::Auto, QualityChoice::High, QualityChoice::Medium, QualityChoice::Low };
    return order[juce::jlimit (0, 3, index)];
}

//==============================================================================
QualityOptions::QualityOptions (LuthierAudioProcessor& p)
    : processor (p), status (p)
{
    groupLabel.setText (tr ("quality.cpuQuality"), juce::dontSendNotification);
    muted (groupLabel, 12.0f);
    addAndMakeVisible (groupLabel);

    group.setTitle (tr ("quality.cpuQuality"));
    group.setComponentID ("cpuQualityGroup");
    addAndMakeVisible (group);

    for (int i = 0; i < 4; ++i)
    {
        const auto c = pillChoice (i);
        auto b = std::make_unique<juce::TextButton> (tr (nameKey (c)));
        b->setRadioGroupId (0x51A1);
        b->setClickingTogglesState (true);
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 3 ? juce::Button::ConnectedOnRight : 0));
        b->setTitle (tr (nameKey (c)));
        b->setDescription (tr (describeKey (c)));
        b->setTooltip (tr (describeKey (c)));
        b->onClick = [this, c] { if (! updating) choose (c); };
        group.addAndMakeVisible (*b);
        pills[(size_t) i] = std::move (b);
    }

    instanceLabel.setText (tr ("quality.thisInstance"), juce::dontSendNotification);
    muted (instanceLabel, 12.0f);
    addAndMakeVisible (instanceLabel);

    overrideBox.setTitle (tr ("quality.thisInstance"));
    overrideBox.onChange = [this]
    {
        if (updating)
            return;

        const auto o = (QualityOverride) juce::jlimit (0, 4, overrideBox.getSelectedId() - 1);
        processor.setQualityOverride (o);

        if (o == QualityOverride::Auto)
            processor.getQualityController().resumeAuto();

        refresh();
    };
    addAndMakeVisible (overrideBox);

    addAndMakeVisible (status);

    offlineToggle.setButtonText (tr ("quality.offlineAtHigh"));
    offlineToggle.onClick = [this] { if (! updating) PerformanceSettings::get().setOfflineAtHigh (offlineToggle.getToggleState()); };
    addAndMakeVisible (offlineToggle);

    notifyToggle.setButtonText (tr ("quality.autoNotify"));
    notifyToggle.onClick = [this] { if (! updating) PerformanceSettings::get().setAutoNotify (notifyToggle.getToggleState()); };
    addAndMakeVisible (notifyToggle);

    muted (oversamplingNote);
    oversamplingNote.setColour (juce::Label::textColourId, Palette::secondary);

    disclosureButton.setButtonText (tr ("quality.whatChanges") + juce::String::fromUTF8 (" \xe2\x80\xba"));
    disclosureButton.onClick = [this] { setDisclosureOpen (! disclosureOpen); };
    addAndMakeVisible (disclosureButton);

    muted (detailsLabel);
    detailsLabel.setJustificationType (juce::Justification::topLeft);
    detailsLabel.setText (tr ("quality.level.high") + ": " + tr ("quality.describe.high") + "\n"
                            + tr ("quality.level.medium") + ": " + tr ("quality.describe.medium") + "\n"
                            + tr ("quality.level.low") + ": " + tr ("quality.describe.low") + "\n"
                            + tr ("quality.level.auto") + ": " + tr ("quality.describe.auto") + "\n"
                            + tr ("quality.disclosure.never"),
                          juce::dontSendNotification);
    addChildComponent (detailsLabel);

    PerformanceSettings::get().addChangeListener (this);
    processor.getQualityController().addChangeListener (this);
    refresh();
}

QualityOptions::~QualityOptions()
{
    PerformanceSettings::get().removeChangeListener (this);
    processor.getQualityController().removeChangeListener (this);
}

void QualityOptions::choose (QualityChoice c)
{
    auto& controller = processor.getQualityController();

    // 3: the pills set the machine's setting; an instance override is the
    // combo's job, so picking a pill while overridden sets the override too.
    if (controller.getOverride() != QualityOverride::Global)
        processor.setQualityOverride ((QualityOverride) ((int) c + 1));
    else
        PerformanceSettings::get().setQuality (c);

    // 2.7: choosing Auto again clears a hold.
    if (c == QualityChoice::Auto)
        controller.resumeAuto();

    refresh();
}

juce::String QualityOptions::oversamplingNoteFor (LuthierAudioProcessor& p)
{
    auto& engine = p.getEngine();
    const int nominal = engine.getOversamplingFactor();
    const int amp = engine.getEffectiveAmpOversampling();

    if (amp >= nominal)
        return {};

    return tr ("quality.osCapped", { { "factor", juce::String (amp) },
                                     { "level", QualityController::levelName (p.getAppliedQualityLevel()) } });
}

void QualityOptions::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    auto& controller = processor.getQualityController();
    auto& settings = PerformanceSettings::get();

    const auto choice = controller.getOverride() == QualityOverride::Global ? settings.getQuality() : controller.getChoice();

    for (int i = 0; i < 4; ++i)
        pills[(size_t) i]->setToggleState (pillChoice (i) == choice, juce::dontSendNotification);

    overrideBox.clear (juce::dontSendNotification);
    overrideBox.addItem (tr ("quality.override.global", { { "level", tr (nameKey (settings.getQuality())) } }), 1);
    overrideBox.addItem (tr ("quality.override.high"), 2);
    overrideBox.addItem (tr ("quality.override.medium"), 3);
    overrideBox.addItem (tr ("quality.override.low"), 4);
    overrideBox.addItem (tr ("quality.override.auto"), 5);
    overrideBox.setSelectedId ((int) controller.getOverride() + 1, juce::dontSendNotification);

    offlineToggle.setToggleState (settings.isOfflineAtHigh(), juce::dontSendNotification);
    notifyToggle.setToggleState (settings.isAutoNotify(), juce::dontSendNotification);

    const auto note = oversamplingNoteFor (processor);
    oversamplingNote.setText (note, juce::dontSendNotification);
    oversamplingNote.setVisible (note.isNotEmpty());

    status.update();
}

void QualityOptions::focusGroup()
{
    const auto choice = processor.getQualityController().getChoice();
    pills[(size_t) pillIndex (choice)]->grabKeyboardFocus();
}

juce::String QualityOptions::getStatusText() const { return status.getText(); }

void QualityOptions::setDisclosureOpen (bool open)
{
    disclosureOpen = open;
    detailsLabel.setVisible (open);
    disclosureButton.setButtonText (tr ("quality.whatChanges")
                                    + juce::String::fromUTF8 (open ? " \xe2\x8c\x84" : " \xe2\x80\xba"));

    if (onLayoutChanged)
        onLayoutChanged();

    resized();
}

int QualityOptions::getPreferredHeight (int width) const
{
    const bool stacked = width < kStackBelow;
    int h = 0;
    h += stacked ? 2 * kRow : kRow;   // label + pills
    h += kGap;
    h += stacked ? 2 * kRow : kRow;   // this instance
    h += kGap + 20 + kGap;             // status
    h += stacked ? 2 * 24 : 24;        // toggles
    h += kGap + 22;                    // disclosure
    h += disclosureOpen ? 96 : 0;
    return h;
}

void QualityOptions::resized()
{
    auto bounds = getLocalBounds();
    const bool stacked = getWidth() < kStackBelow;

    auto labelled = [&] (juce::Label& label, juce::Component& control, int controlWidth)
    {
        if (stacked)
        {
            label.setBounds (bounds.removeFromTop (kRow));
            control.setBounds (bounds.removeFromTop (kRow).withWidth (juce::jmin (controlWidth, getWidth())));
        }
        else
        {
            auto row = bounds.removeFromTop (kRow);
            label.setBounds (row.removeFromLeft (kLabelWidth));
            control.setBounds (row.removeFromLeft (controlWidth));
        }

        bounds.removeFromTop (kGap);
    };

    labelled (groupLabel, group, 320);

    {
        auto pillArea = group.getLocalBounds();
        const int w = pillArea.getWidth() / 4;

        for (int i = 0; i < 4; ++i)
            pills[(size_t) i]->setBounds (i < 3 ? pillArea.removeFromLeft (w) : pillArea);
    }

    labelled (instanceLabel, overrideBox, 280);

    status.setBounds (bounds.removeFromTop (20));
    bounds.removeFromTop (kGap);

    if (stacked)
    {
        offlineToggle.setBounds (bounds.removeFromTop (24));
        notifyToggle.setBounds (bounds.removeFromTop (24));
    }
    else
    {
        auto row = bounds.removeFromTop (24);
        offlineToggle.setBounds (row.removeFromLeft (row.getWidth() / 2));
        notifyToggle.setBounds (row);
    }

    bounds.removeFromTop (kGap);
    disclosureButton.setBounds (bounds.removeFromTop (22).removeFromLeft (220));

    if (disclosureOpen)
        detailsLabel.setBounds (bounds.removeFromTop (96));
}

void QualityOptions::paint (juce::Graphics&) {}

//==============================================================================
bool QualityOptions::Group::keyPressed (const juce::KeyPress& key)
{
    // 9: the arrow keys move within the group, and choose.
    const int code = key.getKeyCode();
    int step = 0;

    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::upKey)    step = -1;
    if (code == juce::KeyPress::rightKey || code == juce::KeyPress::downKey) step = 1;

    if (step == 0)
        return false;

    int current = 0;

    for (int i = 0; i < 4; ++i)
        if (owner.pills[(size_t) i]->getToggleState())
            current = i;

    const int next = (current + step + 4) % 4;
    owner.pills[(size_t) next]->setToggleState (true, juce::dontSendNotification);
    owner.choose (pillChoice (next));
    owner.pills[(size_t) next]->grabKeyboardFocus();
    return true;
}

std::unique_ptr<juce::AccessibilityHandler> QualityOptions::Group::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::group);
}

//==============================================================================
QualityOptions::Status::Status (LuthierAudioProcessor& p)
    : processor (p)
{
    update();
    motion.startTimerHz (*this, 4);
}

void QualityOptions::Status::update()
{
    auto& controller = processor.getQualityController();
    const auto& monitor = processor.getCpuLoadMonitor();
    const auto live = controller.getLiveLevel();

    if (controller.isAuto() && controller.isAutoHeld())
    {
        text = tr ("quality.held", { { "level", QualityController::levelName (live) } });
        showBar = false;
    }
    else if (monitor.getTotalBlocks() <= 0)
    {
        text = tr ("quality.notPlaying");
        showBar = false;
    }
    else
    {
        text = tr (controller.isAuto() ? "quality.nowRunningAuto" : "quality.nowRunning",
                   { { "level", QualityController::levelName (live) } });
        load = monitor.getMean200ms();
        showBar = true;
    }

    setTitle (text);
    repaint();
}

void QualityOptions::Status::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);

    auto area = getLocalBounds();
    g.setFont (Fonts::ui (12.0f));
    g.setColour (Palette::textPrimary);
    g.drawText (text, area.removeFromLeft (showBar ? 260 : area.getWidth()), juce::Justification::centredLeft, true);

    if (! showBar)
        return;

    auto bar = area.removeFromLeft (100).reduced (0, 5).toFloat();
    g.setColour (Palette::panelSunken);
    g.fillRect (bar);

    const auto zone = load > 0.8 ? Palette::clip : load >= 0.5 ? Palette::warning : Palette::secondary;
    const int cells = juce::jlimit (0, 10, (int) std::ceil (load * 10.0 - 1.0e-9));   // stepped

    for (int i = 0; i < 10; ++i)
    {
        auto cell = bar.withWidth (bar.getWidth() / 10.0f).withX (bar.getX() + (float) i * bar.getWidth() / 10.0f).reduced (1.0f, 0.0f);
        g.setColour (i < cells ? zone : Palette::edge);
        g.fillRect (cell);
    }

    area.removeFromLeft (6);
    g.setColour (Palette::textMuted);
    g.setFont (Fonts::mono (11.0f));
    g.drawText (tr ("quality.share", { { "percent", juce::String (juce::roundToInt (load * 100.0)) } }),
                area, juce::Justification::centredLeft, false);
}

//==============================================================================
juce::String QualityDiagnostics::describe (LuthierAudioProcessor& p)
{
    auto& engine = p.getEngine();
    const auto level = p.getAppliedQualityLevel();
    const auto& body = engine.getBodyEngine();
    const auto& cab = engine.getCabinetEngine();
    const int l = (int) level;

    juce::String text;
    text << "  " << tr ("quality.diag.level", { { "level", QualityController::levelName (level)
                                                     + (p.getQualityController().isAuto() ? " (Auto)" : "") } }) << "\n"
         << "  " << tr ("quality.diag.oversampling", { { "amp", juce::String (engine.getEffectiveAmpOversampling()) },
                                                        { "drive", juce::String (engine.getEffectiveDriveOversampling()) },
                                                        { "nominal", juce::String (engine.getOversamplingFactor()) } }) << "\n"
         << "  " << tr ("quality.diag.irs", { { "body", juce::String (body.getIrVariants().getSecondsForLevel (l), 2) },
                                               { "cabA", juce::String (cab.getIrVariants (0).getSecondsForLevel (l), 2) },
                                               { "cabB", juce::String (cab.getIrVariants (1).getSecondsForLevel (l), 2) } }) << "\n"
         << "  " << tr ("quality.diag.modes", { { "n", juce::String (body.getRunningModeCount()) + " / " + juce::String (body.getNumModes()) } }) << "\n"
         << "  " << tr ("quality.diag.sleeping", { { "n", juce::String (engine.getSleepingStringCount()) } }) << "\n";
    return text;
}

} // namespace luthier
