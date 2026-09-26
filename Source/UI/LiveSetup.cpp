#include "LiveSetup.h"

#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

#include <map>

namespace luthier
{

namespace
{
    void styleNote (juce::Label& label, juce::Colour colour = Palette::textMuted)
    {
        label.setFont (Fonts::ui (10.5f));
        label.setColour (juce::Label::textColourId, colour);
    }

    juce::String ccText (int cc)
    {
        return cc >= 0 ? "CC " + juce::String (cc) : juce::String ("not assigned");
    }
}

//==============================================================================
//  LiveActionButton
//==============================================================================
LiveActionButton::LiveActionButton (LuthierAudioProcessor& p, const juce::String& text)
    : juce::TextButton (text), processor (p), idleText (text)
{
    const juce::String tip ("Assign a footswitch or MIDI CC to a live action: snapshot "
                            "next / previous / by value, tap tempo, kill switch, panic, "
                            "setlist next / previous. Pick one, then press the switch.");
    setTooltip (tip);
    AccessibleSetup::configureButton (*this, "Live action controllers", tip);

    onClick = [this]
    {
        juce::Component::SafePointer<LiveActionButton> safe (this);

        buildMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                   [safe] (int result)
        {
            if (safe != nullptr)
                safe->applyMenuResult (result);
        });
    };

    refreshText();
    startTimerHz (10);
}

LiveActionButton::~LiveActionButton()
{
    stopTimer();
}

juce::PopupMenu LiveActionButton::buildMenu() const
{
    auto& map = processor.getLiveActions();
    const int learning = map.getLearningAction();

    juce::PopupMenu menu;
    menu.addSectionHeader (learning >= 0 ? "Press the footswitch or move the controller now"
                                         : "Choose an action, then press its switch");

    for (int i = 0; i < LiveActionMap::kNumActions; ++i)
    {
        const auto action = (LiveAction) i;
        const juce::String text = juce::String (getLiveActionName (action)) + "   ("
                                    + (learning == i ? juce::String ("learning...") : ccText (map.getCcFor (action)))
                                    + ")";

        menu.addItem (1 + i, text, true, learning == i);
    }

    juce::PopupMenu forget;

    for (int i = 0; i < LiveActionMap::kNumActions; ++i)
        if (map.getCcFor ((LiveAction) i) >= 0)
            forget.addItem (100 + i, getLiveActionName ((LiveAction) i));

    menu.addSeparator();
    menu.addSubMenu ("Forget", forget, forget.getNumItems() > 0);

    if (learning >= 0)
        menu.addItem (200, "Cancel learning");

    return menu;
}

void LiveActionButton::applyMenuResult (int result)
{
    auto& map = processor.getLiveActions();

    if (result >= 1 && result <= LiveActionMap::kNumActions)
    {
        if (map.getLearningAction() == result - 1)
            map.cancelLearning();
        else
            map.beginLearning ((LiveAction) (result - 1));
    }
    else if (result >= 100 && result < 100 + LiveActionMap::kNumActions)
    {
        map.clear ((LiveAction) (result - 100));
        map.save();
    }
    else if (result == 200)
    {
        map.cancelLearning();
    }

    refreshText();
}

void LiveActionButton::timerCallback()
{
    const int v = processor.getLiveActions().getVersion();

    if (v != lastVersion)
    {
        lastVersion = v;
        refreshText();
    }
}

void LiveActionButton::refreshText()
{
    const int learning = processor.getLiveActions().getLearningAction();

    setButtonText (learning >= 0 ? juce::String ("LEARN...") : idleText);
    setToggleState (learning >= 0, juce::dontSendNotification);
}

//==============================================================================
//  MorphSetupPanel
//==============================================================================
MorphSetupPanel::MorphSetupPanel (LuthierAudioProcessor& p)
    : processor (p), ccButton (p, "Footswitch CCs...")
{
    // LP-16: the parameter, so automation, MIDI Learn and the mod matrix can
    // all drive the morph.
    position.attachTo (processor, ParamIDs::snapshotMorph,
                       "Morph position between the two snapshot slots. Automatable, "
                       "MIDI-learnable and a modulation destination (LFO, mod wheel, "
                       "expression pedal, sidechain follower).");
    addAndMakeVisible (position);

    // LP-18
    exclusionsButton.setTooltip ("Parameters that hold snapshot A's value for the whole morph.");
    AccessibleSetup::configureButton (exclusionsButton, "Morph exclusions", exclusionsButton.getTooltip());
    exclusionsButton.onClick = [this]
    {
        juce::Component::SafePointer<MorphSetupPanel> safe (this);

        buildExclusionMenu().showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&exclusionsButton),
                                            [safe] (int result)
        {
            if (safe != nullptr)
                safe->applyExclusionMenuResult (result);
        });
    };
    addAndMakeVisible (exclusionsButton);

    styleNote (exclusionsSummary, Palette::textDisabled);
    addAndMakeVisible (exclusionsSummary);

    // LP-19: the two inner control points of the cubic Bezier.
    styleNote (bezierLabel);
    bezierLabel.setText ("Bezier", juce::dontSendNotification);
    addChildComponent (bezierLabel);

    static const char* names[] = { "Bezier point 1 X", "Bezier point 1 Y",
                                   "Bezier point 2 X", "Bezier point 2 Y" };

    for (int i = 0; i < 4; ++i)
    {
        auto& s = bezier[(size_t) i];
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (0.0, 1.0, 0.01);
        s.setTooltip (juce::String (names[i]) + " of the morph curve.");
        AccessibleSetup::configureSlider (s, names[i]);

        s.onValueChange = [this]
        {
            if (updating)
                return;

            processor.getSnapshots().setBezierControlPoints (bezier[0].getValue(), bezier[1].getValue(),
                                                             bezier[2].getValue(), bezier[3].getValue());
        };

        addChildComponent (s);
    }

    // LP-11 on the setup surface too.
    addAndMakeVisible (ccButton);

    refresh();
    startTimerHz (6);
}

MorphSetupPanel::~MorphSetupPanel()
{
    stopTimer();
}

int MorphSetupPanel::getPreferredHeight() const
{
    const bool showBezier = processor.getSnapshots().getMorphCurve() == MorphCurve::bezier;
    return kRowHeight * (showBezier ? 4 : 3);
}

juce::PopupMenu MorphSetupPanel::buildExclusionMenu() const
{
    const auto& bank = processor.getSnapshots();

    // Grouped by the id's first word (amp_, pre1_, reverb_...), so four hundred
    // parameters are a short list of short lists.
    std::map<juce::String, std::pair<juce::PopupMenu, juce::PopupMenu>> groups;   // discrete, continuous

    const auto& parameters = processor.getParameters();

    for (int i = 0; i < parameters.size(); ++i)
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]);

        if (withId == nullptr || withId->paramID == ParamIDs::snapshotMorph)
            continue;

        const auto group = withId->paramID.upToFirstOccurrenceOf ("_", false, false);
        auto& target = SnapshotBank::isDiscrete (*withId) ? groups[group].first : groups[group].second;

        target.addItem (1 + i, withId->getName (64), true,
                        bank.isParameterExcludedFromMorph (withId->paramID));
    }

    juce::PopupMenu menu;
    menu.addSectionHeader ("Hold at snapshot A while morphing");

    for (auto& [name, pair] : groups)
    {
        juce::PopupMenu sub (pair.first);

        if (pair.first.getNumItems() > 0 && pair.second.getNumItems() > 0)
            sub.addSeparator();

        for (juce::PopupMenu::MenuItemIterator it (pair.second); it.next();)
            sub.addItem (it.getItem());

        menu.addSubMenu (name, sub);
    }

    return menu;
}

void MorphSetupPanel::applyExclusionMenuResult (int result)
{
    const auto& parameters = processor.getParameters();

    if (! juce::isPositiveAndBelow (result - 1, parameters.size()))
        return;

    if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[result - 1]))
    {
        auto& bank = processor.getSnapshots();
        bank.setParameterExcludedFromMorph (withId->paramID,
                                            ! bank.isParameterExcludedFromMorph (withId->paramID));
    }

    refresh();
}

void MorphSetupPanel::timerCallback()
{
    refresh();
}

void MorphSetupPanel::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    const auto& bank = processor.getSnapshots();

    const auto exclusions = bank.getMorphExclusions();
    exclusionsSummary.setText (exclusions.isEmpty() ? juce::String ("Every parameter morphs.")
                                                    : juce::String (exclusions.size()) + " held at A",
                               juce::dontSendNotification);

    position.setEnabled (bank.isMorphEnabled());
    exclusionsButton.setEnabled (bank.isMorphEnabled());

    const bool showBezier = bank.getMorphCurve() == MorphCurve::bezier;

    for (int i = 0; i < 4; ++i)
    {
        bezier[(size_t) i].setValue (bank.getBezierControlPoint (i), juce::dontSendNotification);
        bezier[(size_t) i].setVisible (showBezier);
    }

    bezierLabel.setVisible (showBezier);

    if (showBezier != lastBezierShown)
    {
        lastBezierShown = showBezier;
        resized();

        if (onLayoutChanged != nullptr)
            onLayoutChanged();
    }
}

void MorphSetupPanel::resized()
{
    auto bounds = getLocalBounds();

    position.setBounds (bounds.removeFromTop (kRowHeight));

    auto row = bounds.removeFromTop (kRowHeight).reduced (0, 2);
    exclusionsButton.setBounds (row.removeFromLeft (juce::jmin (110, row.getWidth() / 2)));
    row.removeFromLeft (6);
    exclusionsSummary.setBounds (row);

    if (lastBezierShown)
    {
        auto b = bounds.removeFromTop (kRowHeight).reduced (0, 2);
        bezierLabel.setBounds (b.removeFromLeft (48));
        const int w = b.getWidth() / 4;

        for (auto& s : bezier)
            s.setBounds (b.removeFromLeft (w).reduced (2, 0));
    }

    auto ccRow = bounds.removeFromTop (kRowHeight).reduced (0, 2);
    ccButton.setBounds (ccRow.removeFromLeft (juce::jmin (150, ccRow.getWidth())));
}

//==============================================================================
//  MonitorSetupPanel
//==============================================================================
MonitorSetupPanel::MonitorSetupPanel (LuthierAudioProcessor& p)
    : processor (p)
{
    struct Spec { const char* label; const char* name; double lo, hi, step, def; const char* suffix; };

    static const Spec specs[numControls] =
    {
        { "LEVEL", "Monitor level",    -60.0, 12.0, 0.1,  0.0, " dB" },
        { "PAN",   "Monitor pan",       -1.0,  1.0, 0.01, 0.0, "" },
        { "LOW",   "Monitor low EQ",   -12.0, 12.0, 0.1,  0.0, " dB" },
        { "MID",   "Monitor mid EQ",   -12.0, 12.0, 0.1,  0.0, " dB" },
        { "HIGH",  "Monitor high EQ",  -12.0, 12.0, 0.1,  0.0, " dB" },
    };

    for (int i = 0; i < numControls; ++i)
    {
        const auto& spec = specs[i];
        auto& s = sliders[(size_t) i];

        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (spec.lo, spec.hi, spec.step);
        s.setDoubleClickReturnValue (true, spec.def);
        s.setTextValueSuffix (spec.suffix);
        s.setTooltip (juce::String (spec.name) + ". The monitor mix goes to its own output "
                      "bus; the audience never hears these.");
        AccessibleSetup::configureSlider (s, spec.name, spec.suffix);

        s.onValueChange = [this, i]
        {
            if (updating)
                return;

            auto& mix = processor.getMonitorMix();
            const double v = sliders[(size_t) i].getValue();

            switch ((Control) i)
            {
                case level: mix.setLevelDb (v);  break;
                case pan:   mix.setPan (v);      break;
                case low:   mix.setEqLowDb (v);  break;
                case mid:   mix.setEqMidDb (v);  break;
                case high:  mix.setEqHighDb (v); break;
                case numControls:
                default:    break;
            }
        };

        addAndMakeVisible (s);

        auto& l = labels[(size_t) i];
        l.setText (spec.label, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        styleNote (l);
        addAndMakeVisible (l);
    }

    refresh();
    startTimerHz (6);
}

MonitorSetupPanel::~MonitorSetupPanel()
{
    stopTimer();
}

void MonitorSetupPanel::timerCallback()
{
    refresh();
}

void MonitorSetupPanel::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    const auto& mix = processor.getMonitorMix();

    sliders[level].setValue (mix.getLevelDb(), juce::dontSendNotification);
    sliders[pan].setValue (mix.getPan(), juce::dontSendNotification);
    sliders[low].setValue (mix.getEqLowDb(), juce::dontSendNotification);
    sliders[mid].setValue (mix.getEqMidDb(), juce::dontSendNotification);
    sliders[high].setValue (mix.getEqHighDb(), juce::dontSendNotification);
}

void MonitorSetupPanel::resized()
{
    auto bounds = getLocalBounds();
    const int w = bounds.getWidth() / numControls;

    for (int i = 0; i < numControls; ++i)
    {
        auto cell = bounds.removeFromLeft (w);
        labels[(size_t) i].setBounds (cell.removeFromBottom (14));
        sliders[(size_t) i].setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), cell.getHeight()),
                                                                   cell.getHeight()));
    }
}

} // namespace luthier
