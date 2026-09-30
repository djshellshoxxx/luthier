#include "TechniqueMirrors.h"
#include "../../PluginProcessor.h"

namespace luthier
{

TechniqueMirrors::TechniqueMirrors (LuthierAudioProcessor& p)
    : ControlFlow (p), slideFlow (p)
{
    // two-hand-tapping.md 6: CHARACTER Right Hand gains a Tapping section.
    addHeading ("TAPPING (RIGHT HAND)");
    addKnob (ParamIDs::tapStrengthCurve, "Strength curve", "Velocity to tap strength (TECHNIQUES > TAP)");
    addKnob (ParamIDs::tapFlick, "Release flick", "The pull-off's lateral flick (TECHNIQUES > TAP)");
    addToggle (ParamIDs::tapAutoPullOff, "Auto pull-off", "Pluck on the way off a tap (TECHNIQUES > TAP)");

    // microtonal-bends.md 5: CHARACTER PLAYING gains a Microtonal section.
    addHeading ("MICROTONAL (PLAYING)");
    addKnob (ParamIDs::bendGlobalRange, "Bend range", "Global bend range, cents (TECHNIQUES > BEND)");
    addChoice (ParamIDs::bendVibratoSource, "Vibrato", "Vibrato source (TECHNIQUES > BEND)");
    addKnob (ParamIDs::bendVibratoRate, "Rate", "Vibrato rate");
    addKnob (ParamIDs::bendVibratoDepth, "Depth", "Vibrato depth, cents");
    addChoice (ParamIDs::bendQuantise, "Quantise", "Bend quantise (TECHNIQUES > BEND)");

    // slide-technique-controls.md 4: the SLIDE group's expandable section.
    slideFlow.addChoice (ParamIDs::slidePosSource, "Position source", "What moves the bar (TECHNIQUES > SLIDE)");
    slideFlow.addChoice (ParamIDs::slidePosMode, "Position mode", "Absolute or relative");
    slideFlow.addChoice (ParamIDs::slideContact, "Contact strings", "Which strings the bar touches");
    slideFlow.addKnob (ParamIDs::slideSpeedLimit, "Speed limit", "Cents per second");
    slideFlow.addToggle (ParamIDs::slideAutoVibrato, "Auto-vibrato", "Vibrato when the bar holds still");

    slideExpander.setClickingTogglesState (true);
    slideExpander.setTooltip ("The slide's technique controls: what moves the bar and how");
    AccessibleSetup::configureButton (slideExpander, "Slide controls", "Shows or hides the slide technique controls");
    slideExpander.onClick = [this]
    {
        const bool open = slideExpander.getToggleState();
        slideExpander.setButtonText (open ? "SLIDE CONTROLS  -" : "SLIDE CONTROLS  +");
        slideFlow.setVisible (open);
        resized();

        if (onHeightChanged != nullptr)
            onHeightChanged();
    };

    addWide (slideExpander, Metrics::buttonHeight);
    addWide (slideFlow, 1);   // re-measured in resized()
    slideFlow.setVisible (false);
}

} // namespace luthier
