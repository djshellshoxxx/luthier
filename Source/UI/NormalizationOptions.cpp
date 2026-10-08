#include "NormalizationOptions.h"
#include "UiPreferences.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"

namespace luthier
{

namespace
{
    /*  6: a new instance starts from the user's last choice. The processor
        cannot read UiPreferences (the renderer has no UI), so the UI installs
        the provider when this file is linked in - which it is in every build
        that has an editor. */
    struct DefaultsRegistrar
    {
        DefaultsRegistrar()
        {
            OutputNormalization::defaultsProvider() = []
            {
                OutputNormalization::Defaults d;
                auto& prefs = UiPreferences::get();
                d.enabled = prefs.getBool (OutputNormalization::kPrefDefaultEnabled, false);
                d.targetLufs = (double) prefs.getInt (OutputNormalization::kPrefDefaultTarget,
                                                      (int) OutputNormalization::kDefaultTarget);
                return d;
            };
        }
    };

    const DefaultsRegistrar registrar;

    void styleNote (juce::Label& label, juce::Colour colour, float size = 11.0f)
    {
        label.setFont (juce::Font (juce::FontOptions (size)));
        label.setColour (juce::Label::textColourId, colour);
        label.setJustificationType (juce::Justification::topLeft);
        label.setMinimumHorizontalScale (1.0f);
    }

    bool bannerPostedForFailure = false;
}

//==============================================================================
NormalizationOptionsGroup::NormalizationOptionsGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    toggle.setButtonText (tr ("options.audio.normalization.switch"));
    toggle.setComponentID (switchComponentId);

    // 9: the accessible description is the warning, heard whether it is on or off.
    AccessibleSetup::configureButton (toggle, tr ("options.audio.normalization.switch"),
                                      tr ("options.audio.normalization.caption"));
    toggle.setTooltip (tr ("options.audio.normalization.caption"));
    toggle.setDescription (tr ("options.audio.normalization.caption"));
    toggle.onClick = [this]
    {
        if (updating)
            return;

        auto& n = processor.getOutputNormalization();
        n.setEnabled (toggle.getToggleState(), true);   // fromUser: posts the banner (5.3)
        NormalizationUi::rememberAsDefault (n);
        refresh();
    };
    addAndMakeVisible (toggle);

    targetLabel.setText (tr ("options.audio.normalization.target"), juce::dontSendNotification);
    styleNote (targetLabel, Palette::textPrimary);
    targetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (targetLabel);

    for (int i = 0; i < OutputNormalization::kNumTargets; ++i)
        target.addItem (juce::String ((int) OutputNormalization::kTargets[i]) + " LUFS", i + 1);

    AccessibleSetup::configureComboBox (target, tr ("options.audio.normalization.target"));
    target.onChange = [this]
    {
        if (updating)
            return;

        const int index = target.getSelectedItemIndex();

        if (juce::isPositiveAndBelow (index, OutputNormalization::kNumTargets))
        {
            auto& n = processor.getOutputNormalization();
            n.setTargetLufs (OutputNormalization::kTargets[index]);
            NormalizationUi::rememberAsDefault (n);
        }
    };
    addAndMakeVisible (target);

    styleNote (readout, Palette::textPrimary);
    readout.setAccessible (true);
    addAndMakeVisible (readout);

    // 5.1 / 9: the "!" glyph and the words carry the warning, not the colour alone.
    styleNote (caption, Palette::warning);
    caption.setText ("! " + tr ("options.audio.normalization.caption"), juce::dontSendNotification);
    addAndMakeVisible (caption);

    styleNote (note, Palette::textMuted, 10.5f);
    note.setText (tr ("options.audio.normalization.note"), juce::dontSendNotification);
    addAndMakeVisible (note);

    setFocusContainerType (juce::Component::FocusContainerType::keyboardFocusContainer);

    refresh();
    motion.startTimerHz (*this, 10);   // 5.2: the readout refreshes at 10 Hz
}

NormalizationOptionsGroup::~NormalizationOptionsGroup()
{
    motion.stopTimer();
}

void NormalizationOptionsGroup::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    auto& n = processor.getOutputNormalization();
    const auto status = n.getStatus();
    const bool on = status.enabled;

    toggle.setToggleState (on, juce::dontSendNotification);

    for (int i = 0; i < OutputNormalization::kNumTargets; ++i)
        if (OutputNormalization::kTargets[i] == status.targetLufs && target.getSelectedItemIndex() != i)
            target.setSelectedItemIndex (i, juce::dontSendNotification);

    // 9: disabled while off, and says so.
    target.setEnabled (on);
    target.setTooltip (on ? juce::String() : tr ("options.audio.normalization.targetOff"));
    target.setDescription (on ? juce::String() : tr ("options.audio.normalization.targetOff"));

    // 5.1: the caption is always shown while on, and cannot be suppressed.
    caption.setVisible (on);
    note.setVisible (on);

    const auto text = OutputNormalization::readoutText (status);

    if (readout.getText() != text)
        readout.setText (text, juce::dontSendNotification);

    // 5.2: stale after 2 s shows the last value in the muted colour.
    const bool stale = on && status.state == OutputNormalization::State::measuring
                         && status.updatedMs != 0
                         && juce::Time::getMillisecondCounter() - status.updatedMs > 2000;
    readout.setColour (juce::Label::textColourId, stale || ! on ? Palette::textMuted : Palette::textPrimary);
    readout.setTitle (text);

    // 9: a polite announcement when the applied gain moves by 0.5 dB or more,
    // at most one every 2 s, at verbosity standard or verbose.
    if (on && (status.state == OutputNormalization::State::applied
                 || status.state == OutputNormalization::State::clamped
                 || status.state == OutputNormalization::State::estimate
                 || status.state == OutputNormalization::State::morphing))
    {
        const auto now = clock();
        const bool verboseEnough = AccessibilitySettings::get().getVerbosity() != AccessibilitySettings::Verbosity::minimal;

        if (verboseEnough && std::abs (status.gainDb - lastAnnouncedDb) >= 0.5
             && (announcements == 0 || now - lastAnnouncementMs >= 2000))
        {
            lastAnnouncedDb = status.gainDb;
            lastAnnouncementMs = now;
            ++announcements;

            juce::AccessibilityHandler::postAnnouncement (
                tr ("options.audio.normalization.announce",
                    { { "sign", status.gainDb < 0.0 ? "minus" : "plus" },
                      { "value", juce::String (std::abs (status.gainDb), 1) } }),
                juce::AccessibilityHandler::AnnouncementPriority::low);
        }
    }
}

void NormalizationOptionsGroup::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().removeFromTop (18);
    g.setColour (Palette::accent);
    g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    g.drawText (tr ("options.audio.normalization.heading"), bounds, juce::Justification::centredLeft, false);

    g.setColour (Palette::edge);
    g.drawHorizontalLine (bounds.getBottom(), 0.0f, (float) getWidth());
}

void NormalizationOptionsGroup::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (22);

    toggle.setBounds (bounds.removeFromTop (24).removeFromLeft (280));

    auto indented = bounds.withTrimmedLeft (24);

    {
        auto row = indented.removeFromTop (26);
        targetLabel.setBounds (row.removeFromLeft (140));
        target.setBounds (row.removeFromLeft (160).reduced (0, 2));
    }

    indented.removeFromTop (4);
    readout.setBounds (indented.removeFromTop (18));
    indented.removeFromTop (4);
    caption.setBounds (indented.removeFromTop (58));
    note.setBounds (indented.removeFromTop (34));
}

//==============================================================================
NormalizationCaption::NormalizationCaption (LuthierAudioProcessor& p, const juce::String& localeKey)
    : processor (p)
{
    setText (tr (localeKey), juce::dontSendNotification);
    setFont (juce::Font (juce::FontOptions (10.5f)));
    setColour (juce::Label::textColourId, Palette::textMuted);
    setJustificationType (juce::Justification::centredLeft);
    setVisible (processor.getOutputNormalization().isEnabled());
    startTimerHz (4);
}

NormalizationCaption::~NormalizationCaption()
{
    stopTimer();
}

void NormalizationCaption::update()
{
    const bool on = processor.getOutputNormalization().isEnabled();

    if (on != isVisible())
    {
        setVisible (on);

        // The owner gives the line its space only while it shows.
        if (auto* parent = getParentComponent())
            parent->resized();
    }
}

//==============================================================================
void NormalizationUi::postEnabledBanner (NotificationCentre& centre, std::function<void()> openOptions)
{
    if (UiPreferences::get().getBool (OutputNormalization::kPrefBannerSuppressed, false))
        return;

    Notification n;
    n.id = "normalization.on";
    n.level = Notification::Level::info;
    n.message = tr ("banner.normalization.on");
    n.actionText = tr ("banner.normalization.options");
    n.action = std::move (openOptions);
    n.secondaryActionText = tr ("banner.normalization.dontShow");
    n.secondaryAction = []
    {
        auto& prefs = UiPreferences::get();
        prefs.setBool (OutputNormalization::kPrefBannerSuppressed, true);
        prefs.save();
    };

    centre.post (std::move (n));
}

void NormalizationUi::postFailureBanner (NotificationCentre& centre)
{
    if (bannerPostedForFailure)
        return;

    bannerPostedForFailure = true;

    Notification n;
    n.id = "normalization.failed";
    n.level = Notification::Level::warning;
    n.message = tr ("banner.normalization.failed");
    centre.post (std::move (n));
}

bool NormalizationUi::focusSwitchIn (juce::Component& root)
{
    std::function<juce::Component* (juce::Component&)> find = [&find] (juce::Component& c) -> juce::Component*
    {
        if (c.getComponentID() == NormalizationOptionsGroup::switchComponentId)
            return &c;

        for (auto* child : c.getChildren())
            if (auto* found = find (*child))
                return found;

        return nullptr;
    };

    if (auto* sw = find (root))
    {
        if (sw->isShowing())
            sw->grabKeyboardFocus();

        return true;
    }

    return false;
}

juce::StringArray NormalizationUi::diagnosticsLines (const LuthierAudioProcessor& processor)
{
    const auto s = processor.getNormalizationStatus();
    juce::StringArray lines;

    if (! s.enabled)
    {
        lines.add ("Output normalization: off");
        return lines;
    }

    lines.add ("Output normalization: " + juce::String (s.appliedGainDb, 2) + " dB applied, "
                 + juce::String (s.gainDb, 2) + " dB calibrated, target " + juce::String (s.targetLufs, 0) + " LUFS");
    lines.add ("  hash " + (s.hash.isNotEmpty() ? s.hash.substring (0, 16) : juce::String ("-"))
                 + ", measured " + juce::String (s.measuredLufs, 2) + " LUFS, source "
                 + NormalizationCalibrator::sourceName (s.source));
    lines.add ("  true-peak gain reduction " + juce::String (s.truePeakReductionDb, 2) + " dB, requests "
                 + juce::String (s.requests));
    return lines;
}

void NormalizationUi::rememberAsDefault (const OutputNormalization& normalization)
{
    auto& prefs = UiPreferences::get();
    prefs.setBool (OutputNormalization::kPrefDefaultEnabled, normalization.isEnabled());
    prefs.setInt (OutputNormalization::kPrefDefaultTarget, (int) normalization.getTargetLufs());
    prefs.save();
}

void NormalizationUi::toggleFromCommand (LuthierAudioProcessor& processor)
{
    auto& n = processor.getOutputNormalization();
    n.setEnabled (! n.isEnabled(), true);
    rememberAsDefault (n);
}

} // namespace luthier
