#include "SlapGroup.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    void writePlain (LuthierAudioProcessor& p, const char* id, float plain)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id)))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    }
}

//==============================================================================
SlapGroup::SlapGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    heading.setText ("SLAP", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    status.setFont (Fonts::ui (10.0f));
    status.setColour (juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible (status);

    armToggle.attachTo (processor, ParamIDs::slapArmed,
                        "Arms the slap: its keyswitches (C0 to F#0), CCs, zone or velocity zone, and the "
                        "STRIKE button. On a bass, quiet notes ghost whether or not this is on.");
    addAndMakeVisible (armToggle);

    strikeButton.setTooltip ("A slap now, on the type's strings, as the Playing strip's button fires it");
    strikeButton.onClick = [this] { strike(); };
    addAndMakeVisible (strikeButton);

    for (int i = 0; i < (int) SlapPreset::numPresets; ++i)
        presetBox.addItem (SlapSettings::getPresetName ((SlapPreset) i), i + 1);

    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setTooltip ("The factory slaps (string-slap-technique 4): writes every slap control at once");
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0)
            applyPreset ((SlapPreset) (presetBox.getSelectedId() - 1));
    };
    addAndMakeVisible (presetBox);

    type.attachTo (processor, ParamIDs::slapType,
                   "Thumb Slap drives the string into the frets; Finger Pop yanks it off them; Palm Slap is "
                   "the whole hand across muted strings; Body Tap knocks the body and leaves the strings alone.");
    trigger.attachTo (processor, ParamIDs::slapTrigger,
                      "What makes a note a slap: the keyswitch (D#0) held, the trigger CC over half, notes on the "
                      "MPE zone's channel, the STRIKE button only, or any note at or above the velocity zone.");
    bodyPart.attachTo (processor, ParamIDs::slapBodyPart,
                       "Where a body tap lands: the top rings its air mode and a bright knuckle click, the side is "
                       "mid-heavy, the back is the dull low one.");

    for (auto* c : { &type, &trigger, &bodyPart })
        addAndMakeVisible (*c);

    strings = std::make_unique<StringMaskSelector> (processor, ParamIDs::slapStringMask);
    strings->setTooltip ("The strings the slap takes. HELD is the type's default: the low two on a bass thumb, "
                         "the wound three on a guitar, every string for a palm slap.");
    addAndMakeVisible (*strings);

    auto attach = [this] (LuthierSlider& slider, const char* id, const char* tip)
    {
        slider.attachTo (processor, id, tip);
        addAndMakeVisible (slider);
    };

    auto attachToggle = [this] (LuthierToggle& toggle, const char* id, const char* tip)
    {
        toggle.attachTo (processor, id, tip);
        addAndMakeVisible (toggle);
    };

    attach (slapStrength, ParamIDs::slapStrength, "The thumb's contact force: louder, and deeper into the frets");
    attach (slapPosition, ParamIDs::slapPositionMm, "Where the thumb lands, in mm from the last fret toward the bridge");
    attach (thumbHardness, ParamIDs::slapThumbHardness, "A hard thumb is brighter and clacks higher and shorter");
    attach (fretContact, ParamIDs::slapFretContact,
            "How far the thumb drives the string into the frets: the clack. 0 is a thump with no clack.");
    attach (popStrength, ParamIDs::popStrength, "How hard the finger hooks the string before it snaps back");
    attach (popPosition, ParamIDs::popPositionMm, "Where the pop is pulled, in mm from the last fret");
    attach (force, ParamIDs::slapForce, "Contact force for a palm slap and a body tap");
    attach (palmPosition, ParamIDs::slapPalmPositionMm, "Where the palm lands, in mm from the last fret");
    attachToggle (doubleThump, ParamIDs::doubleThumpEnabled,
                  "The double thump: the thumb comes back up through the string after the rebound gap");
    attach (upRatio, ParamIDs::doubleThumpUpRatio, "The up-stroke's level next to the down-stroke's");
    attach (reboundGap, ParamIDs::slapReboundGap, "How long after the strike the thumb comes back up, in ms");
    attachToggle (ghostMode, ParamIDs::slapGhostMode,
                  "The fretting hand rests on the string before every strike: a thump with no clear pitch. "
                  "The E0 keyswitch or the ghost CC does the same while held.");
    attach (ghostLevel, ParamIDs::ghostLevel, "How loud a ghost is next to a full note");
    attach (ghostDamping, ParamIDs::ghostDamping, "How hard the resting hand stops the string; 0.94 ends it in about 15 ms");
    attachToggle (ghostAuto, ParamIDs::ghostAuto, "On a bass, notes under the threshold are ghosts without asking");
    attach (ghostThreshold, ParamIDs::ghostVelocityThreshold, "Auto ghost: MIDI velocity under this is a ghost");
    attach (snapBack, ParamIDs::slapSnapBack,
            "Bass only: how hard a popped string snaps back against the board. Higher is clackier and funkier.");

    startTimerHz (10);
}

SlapGroup::~SlapGroup()
{
    stopTimer();
}

void SlapGroup::strike()
{
    auto& front = processor.getEngine().getTechniqueTriggers();
    front.request (TechniqueId::slap, 0, true);
    front.request (TechniqueId::slap, 0, false);
}

void SlapGroup::applyPreset (SlapPreset preset)
{
    auto& engine = processor.getEngine();
    const auto s = SlapSettings::fromPreset (preset, engine.getNumStrings(),
                                            engine.getGuitarSpec().category == GuitarCategory::Bass);

    writePlain (processor, ParamIDs::slapArmed, s.armed ? 1.0f : 0.0f);
    writePlain (processor, ParamIDs::slapType, (float) (int) s.type);
    writePlain (processor, ParamIDs::slapTrigger, (float) (int) s.trigger);
    writePlain (processor, ParamIDs::slapVelocityZone, (float) s.velocityZone);
    writePlain (processor, ParamIDs::slapStringMask, (float) s.stringMask);
    writePlain (processor, ParamIDs::slapStrength, (float) s.slapStrength);
    writePlain (processor, ParamIDs::slapPositionMm, (float) s.slapPositionMm);
    writePlain (processor, ParamIDs::slapThumbHardness, (float) s.thumbHardness);
    writePlain (processor, ParamIDs::slapFretContact, (float) s.fretContact);
    writePlain (processor, ParamIDs::popStrength, (float) s.popStrength);
    writePlain (processor, ParamIDs::popPositionMm, (float) s.popPositionMm);
    writePlain (processor, ParamIDs::doubleThumpEnabled, s.doubleThump ? 1.0f : 0.0f);
    writePlain (processor, ParamIDs::slapForce, (float) s.force);
    writePlain (processor, ParamIDs::slapPalmPositionMm, (float) s.palmPositionMm);
    writePlain (processor, ParamIDs::slapSnapBack, (float) s.snapBack);
    writePlain (processor, ParamIDs::slapBodyPart, (float) (int) s.bodyPart);
}

void SlapGroup::timerCallback()
{
    // The hand's state, for the sub-tab's indicators (gui-techniques 2).
    const auto& slap = processor.getEngine().getSlapEngine();
    const auto fired = slap.getFiredCount();

    juce::String text;

    if (! slap.getSettings().armed)
        text = "Disarmed";
    else if (slap.isModifierHeld())
        text = "Modifier held: the notes under it are slapped";
    else if (slap.isGhostHeld())
        text = "Ghost held";
    else
        text = juce::String (fired) + (fired == 1 ? " slap" : " slaps");

    if (fired != lastFired)
        lastFired = fired;

    if (text != status.getText())
        status.setText (text, juce::dontSendNotification);
}

void SlapGroup::resized()
{
    auto bounds = getLocalBounds();

    auto take = [&bounds] (int h)
    {
        auto r = bounds.removeFromTop (h);
        bounds.removeFromTop (gap);
        return r;
    };

    {
        auto r = take (headingHeight);
        heading.setBounds (r.removeFromLeft (80));
        status.setBounds (r);
    }

    {
        auto r = take (Metrics::buttonHeight);
        armToggle.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        strikeButton.setBounds (r);
    }

    presetBox.setBounds (take (rowHeight));
    type.setBounds (take (choiceHeight));
    trigger.setBounds (take (choiceHeight));
    strings->setBounds (take (maskHeight));

    for (auto* s : { &slapStrength, &slapPosition, &thumbHardness, &fretContact, &popStrength, &popPosition,
                     &force, &palmPosition })
        s->setBounds (take (rowHeight));

    doubleThump.setBounds (take (rowHeight));

    for (auto* s : { &upRatio, &reboundGap })
        s->setBounds (take (rowHeight));

    ghostMode.setBounds (take (rowHeight));

    for (auto* s : { &ghostLevel, &ghostDamping })
        s->setBounds (take (rowHeight));

    ghostAuto.setBounds (take (rowHeight));
    ghostThreshold.setBounds (take (rowHeight));
    snapBack.setBounds (take (rowHeight));
    bodyPart.setBounds (take (choiceHeight));
}

} // namespace luthier
