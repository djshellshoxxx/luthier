#include "HarmonicsGroup.h"
#include "../PluginProcessor.h"
#include "../DSP/String/Harmonics.h"

namespace luthier
{

juce::String HarmonicsGroup::describeOffset (int choiceIndex)
{
    // 3: 12 -> 2, 7 -> 3, 5 -> 4, 4 (3.86) -> 5, 19 -> 3, 24 -> 4.
    static const int partials[] = { 2, 3, 4, 5, 3, 4 };
    const int i = juce::jlimit (0, 5, choiceIndex);
    return juce::String (harmonics::offsetFretsForChoice (i), 2) + " frets above the fretted note sounds partial "
           + juce::String (partials[i]);
}

HarmonicsGroup::HarmonicsGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    heading.setText ("PICK - HARMONICS", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    auto attach = [this] (LuthierSlider& s, const char* id, const juce::String& tip)
    {
        s.attachTo (processor, id, tip);
        addAndMakeVisible (s);
    };

    attach (pressure, ParamIDs::harmonicTouchPressure,
            "How firmly the finger rests on the node. At 0.6 every partial without a node there loses "
            "8 dB per round trip, which is why a clean 12th-fret harmonic has no fundamental.");
    attach (width, ParamIDs::harmonicFingerWidth,
            "How much string the finger pad covers. A wider pad forgives a touch a few millimetres off the node.");
    attach (touchTime, ParamIDs::harmonicTouchTime,
            "How long the finger stays on the string after the pluck. Fret 12 on the low E: 70 ms is 46 dB off the fundamental.");
    attach (graze, ParamIDs::harmonicBriefTouch,
            "How long a pinch's thumb grazes the string. A tapped harmonic stays for half a touch or 1.5 grazes.");
    attach (thumbOffset, ParamIDs::pinchThumbOffsetMm,
            "How far toward the neck the thumb lands behind the pick. Moving the pick (CC 70) sweeps the partial.");

    juce::StringArray offsets;
    for (int i = 0; i < 6; ++i)
        offsets.add (describeOffset (i));

    artificial.attachTo (processor, ParamIDs::artificialHarmonicOffset,
                         "Where the artificial harmonic is touched, above the fretted note (CC 103). " + offsets.joinIntoString ("; "));
    tapped.attachTo (processor, ParamIDs::tappedHarmonicOffset,
                     "Where a tapped harmonic lands, above the fretted note (CC 104). " + offsets.joinIntoString ("; "));
    mapping.attachTo (processor, ParamIDs::harmonicNoteMapping,
                      "Touch fret: the note names where the harmonic is touched, as tab writes <12>. "
                      "Sounding: the note names the pitch you hear, and the string and node are found for you.");

    for (auto* c : { &artificial, &tapped, &mapping })
        addAndMakeVisible (*c);
}

void HarmonicsGroup::resized()
{
    auto bounds = getLocalBounds();
    auto take = [&bounds] (int h) { auto r = bounds.removeFromTop (h); bounds.removeFromTop (gap); return r; };

    heading.setBounds (take (headingHeight));

    for (auto* s : { &pressure, &width, &touchTime, &graze, &thumbOffset })
        s->setBounds (take (rowHeight));

    {
        auto r = take (choiceHeight);
        artificial.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        tapped.setBounds (r);
    }

    mapping.setBounds (take (choiceHeight));
}

} // namespace luthier
