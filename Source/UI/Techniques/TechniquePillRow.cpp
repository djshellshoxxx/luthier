#include "TechniquePillRow.h"
#include "../../PluginProcessor.h"

namespace luthier
{

//==============================================================================
TechniquePopover::TechniquePopover (LuthierAudioProcessor& p, TechniqueSlot s)
    : slot (s), flow (p)
{
    setWantsKeyboardFocus (true);

    title.setText (juce::String (TechniqueTable::get (slot).name), juce::dontSendNotification);
    title.setFont (Fonts::sectionHeader());
    title.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (title);

    closeButton.setTooltip ("Close (Escape)");
    closeButton.onClick = [this]
    {
        // Deleting the popover from inside its own button's click is left to the next message.
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<TechniquePopover> (this)]
                                         { if (safe != nullptr) safe->close(); });
    };
    AccessibleSetup::configureButton (closeButton, "Close", "Closes the technique popover");
    addAndMakeVisible (closeButton);

    // gui-techniques-updates 2: "the top 3-5 controls for that technique".
    switch (slot)
    {
        case TechniqueSlot::scrape:
            flow.addChoice (ParamIDs::scrapeDirection, "Direction", "Bridge to nut, nut to bridge, or hold and sweep");
            flow.addChoice (ParamIDs::scrapeTool, "Tool", "Pick edge, fingernail or thumb");
            flow.addKnob (ParamIDs::scrapeDuration, "Duration", "How long a scrape takes");
            flow.addKnob (ParamIDs::scrapePressure, "Pressure", "How hard the edge is pressed in");
            break;

        case TechniqueSlot::slide:
            flow.addChoice (ParamIDs::slidePosSource, "Position source", "What moves the bar");
            flow.addChoice (ParamIDs::slidePosMode, "Position mode", "Absolute or relative to the fretted note");
            flow.addChoice (ParamIDs::slideContact, "Contact strings", "Which strings the bar touches");
            flow.addKnob (ParamIDs::slideSpeedLimit, "Speed limit", "Cents per second");
            break;

        case TechniqueSlot::slap:
            flow.addChoice (ParamIDs::slapType, "Slap type", "Thumb slap, finger pop, palm slap or body tap");
            flow.addChoice (ParamIDs::slapTrigger, "Trigger", "What makes a note a slap");
            flow.addKnob (ParamIDs::slapForce, "Force", "How hard the hand lands");
            flow.addToggle (ParamIDs::slapGhostMode, "Ghost mode", "Every slap a ghost");
            break;

        case TechniqueSlot::mute:
            flow.addChoice (ParamIDs::muteMasterMode, "Master mode", "Overrides every mute with this one");
            flow.addKnob (ParamIDs::mutePalmPosition, "Palm position", "mm from the bridge");
            flow.addKnob (ParamIDs::mutePalmPressure, "Palm pressure", "How hard the palm presses");
            flow.addKnob (ParamIDs::muteHumanise, "Humanise", "Chance a step flips open / palm mute");
            break;

        case TechniqueSlot::tap:
            flow.addChoice (ParamIDs::tapSource, "Tap trigger", "MIDI channel, keyswitch 19 or the fretboard");
            flow.addToggle (ParamIDs::tapAutoPullOff, "Auto pull-off", "Pluck on the way off a tap");
            flow.addKnob (ParamIDs::tapFlick, "Release flick", "The pull-off's flick");
            flow.addToggle (ParamIDs::tapFretSnap, "Fret snap", "Taps on fret centres");
            break;

        case TechniqueSlot::bend:
            flow.addKnob (ParamIDs::bendGlobalRange, "Bend range", "A full throw, in cents");
            flow.addChoice (ParamIDs::bendVibratoSource, "Vibrato", "LFO, aftertouch or MPE Z");
            flow.addKnob (ParamIDs::bendVibratoRate, "Rate", "Vibrato rate");
            flow.addKnob (ParamIDs::bendVibratoDepth, "Depth", "Vibrato depth, cents");
            flow.addChoice (ParamIDs::bendQuantise, "Quantise", "Pull bends to a grid");
            break;

        case TechniqueSlot::numSlots:
        default:
            break;
    }

    addAndMakeVisible (flow);
    setSize (kWidth, getPreferredHeight());
    AccessibleSetup::configureDescriptive (*this, juce::String (TechniqueTable::get (slot).spokenName) + " controls",
                                           "Escape closes");
}

int TechniquePopover::getPreferredHeight() const
{
    return 28 + flow.getHeightForWidth (kWidth - 16) + 8;
}

void TechniquePopover::close()
{
    if (onClose != nullptr)
        onClose();
}

bool TechniquePopover::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }

    return false;
}

void TechniquePopover::paint (juce::Graphics& g)
{
    g.setColour (Palette::shadow);
    g.fillRoundedRectangle (getLocalBounds().toFloat().translated (2.0f, 3.0f), Metrics::panelCorner);
    LuthierLookAndFeel::drawPanel (g, getLocalBounds().toFloat());
}

void TechniquePopover::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto top = area.removeFromTop (20);
    closeButton.setBounds (top.removeFromRight (22));
    title.setBounds (top);
    area.removeFromTop (4);
    flow.setBounds (area);
}

//==============================================================================
TechniquePillRow::TechniquePillRow (LuthierAudioProcessor& p)
    : processor (p), muteButton (p)
{
    for (int i = 0; i < TechniqueTable::count; ++i)
    {
        auto* pill = pills.add (new TechniquePill (p, (TechniqueSlot) i));
        pill->onHold = [this] (TechniqueSlot s) { openPopover (s); };
        pill->onOpenSubTab = [this] (TechniqueSlot s)
        {
            closePopover();

            if (onOpenSubTab != nullptr)
                onOpenSubTab ((int) s);
        };
        addAndMakeVisible (pill);
    }

    addAndMakeVisible (muteButton);
    setTitle ("Techniques");
    setFocusContainerType (juce::Component::FocusContainerType::keyboardFocusContainer);
}

TechniquePillRow::~TechniquePillRow()
{
    closePopover();
}

TechniquePopover* TechniquePillRow::openPopover (TechniqueSlot slot)
{
    closePopover();

    popover = std::make_unique<TechniquePopover> (processor, slot);
    popover->onClose = [this] { closePopover(); };

    auto* host = getTopLevelComponent() != nullptr ? getTopLevelComponent() : this;
    const auto& pill = *pills[(int) slot];
    auto anchor = host->getLocalArea (&pill, pill.getLocalBounds());

    int x = juce::jlimit (0, juce::jmax (0, host->getWidth() - TechniquePopover::kWidth), anchor.getX());
    int y = anchor.getY() - popover->getHeight() - 4;

    if (y < 0)
        y = anchor.getBottom() + 4;

    popover->setTopLeftPosition (x, y);
    host->addAndMakeVisible (*popover);

    if (popover->isShowing())
        popover->grabKeyboardFocus();

    return popover.get();
}

void TechniquePillRow::closePopover()
{
    if (popover == nullptr)
        return;

    if (auto* parent = popover->getParentComponent())
        parent->removeChildComponent (popover.get());

    popover.reset();
}

void TechniquePillRow::resized()
{
    auto area = getLocalBounds();
    const int pillW = juce::jmin (TechniquePill::preferredWidth, (area.getWidth() - 110) / TechniqueTable::count);

    for (auto* pill : pills)
    {
        pill->setBounds (area.removeFromLeft (pillW).withSizeKeepingCentre (pillW - 2, TechniquePill::preferredHeight));
        area.removeFromLeft (2);
    }

    area.removeFromLeft (Metrics::grid);
    muteButton.setBounds (area.removeFromLeft (juce::jmin (110, area.getWidth())).withSizeKeepingCentre (juce::jmin (110, area.getWidth()), TechniquePill::preferredHeight));
}

} // namespace luthier
