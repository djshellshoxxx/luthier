#include "RightHandGroup.h"
#include "../PluginProcessor.h"
#include "../Model/Playing/RightHand.h"

namespace luthier
{

namespace
{
    juce::RangedAudioParameter* parameter (LuthierAudioProcessor& p, const juce::String& id)
    {
        return dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
    }

    double plain (LuthierAudioProcessor& p, const juce::String& id)
    {
        if (auto* param = parameter (p, id))
            return param->convertFrom0to1 (param->getValue());

        return 0.0;
    }

    void write (LuthierAudioProcessor& p, const juce::String& id, double value)
    {
        if (auto* param = parameter (p, id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) value));
            param->endChangeGesture();
        }
    }
}

//==============================================================================
StringToolCell::StringToolCell (LuthierAudioProcessor& p, int number)
    : processor (p), stringNumber (number)
{
    setTooltip ("String " + juce::String (number) + (number == 1 ? " (high E)" : "")
                + ": what plays it. Click to step through the tools, right-click to pick one. "
                  "Global is the PICK group's pick or fingers.");
    setTitle ("String " + juce::String (number) + " tool");
    setWantsKeyboardFocus (true);
}

juce::String StringToolCell::glyphFor (int tool)
{
    static const char* glyphs[] = { "G", "P", "F", "T", "TP", "S", "Po" };
    return glyphs[juce::jlimit (0, 6, tool)];
}

int StringToolCell::getTool() const
{
    return juce::roundToInt (plain (processor, ParamIDs::rhStringTool (stringNumber)));
}

void StringToolCell::setTool (int tool)
{
    write (processor, ParamIDs::rhStringTool (stringNumber), (double) juce::jlimit (0, (int) RhTool::numTools - 1, tool));
    repaint();
}

void StringToolCell::paint (juce::Graphics& g)
{
    const int tool = getTool();
    auto r = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (tool == 0 ? Palette::panelSunken : Palette::accent.withAlpha (0.25f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (hasKeyboardFocus (false) ? Palette::accent : Palette::edge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (9.0f));
    g.drawText (juce::String (stringNumber), r.removeFromTop (10.0f), juce::Justification::centred);

    g.setColour (tool == 0 ? Palette::textMuted : Palette::textPrimary);
    g.setFont (Fonts::ui (12.0f));
    g.drawText (glyphFor (tool), r, juce::Justification::centred);
}

void StringToolCell::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        const auto names = rhToolNames();

        for (int i = 0; i < names.size(); ++i)
            menu.addItem (i + 1, names[i], true, i == getTool());

        juce::Component::SafePointer<StringToolCell> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
        {
            if (safe != nullptr && result > 0)
                safe->setTool (result - 1);
        });
        return;
    }

    setTool ((getTool() + 1) % (int) RhTool::numTools);
}

//==============================================================================
RightHandGroup::RightHandGroup (LuthierAudioProcessor& p)
    : processor (p)
{
    heading.setText ("RIGHT HAND", juce::dontSendNotification);
    heading.setFont (Fonts::sectionHeader());
    heading.setColour (juce::Label::textColourId, Palette::accent);
    addAndMakeVisible (heading);

    styleLabel.setText ("Style", juce::dontSendNotification);
    styleLabel.setFont (Fonts::ui (11.0f));
    styleLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible (styleLabel);

    // Not an attachment: choosing a style writes its whole table (4), which an
    // attachment would not, and the box shows "(modified)" when the tools drift.
    styleBox.addItemList (rhStyleNames(), 1);
    styleBox.setTooltip ("Sets the per-string tools and the controls that go with them, as one undo step. "
                         "Custom writes nothing. Loading a preset never re-applies a style.");
    styleBox.setTitle ("Right-hand style");
    styleBox.onChange = [this]
    {
        if (! updating && styleBox.getSelectedItemIndex() >= 0)
            applyStyle (processor, styleBox.getSelectedItemIndex());
    };
    addAndMakeVisible (styleBox);

    for (int i = 0; i < 6; ++i)
    {
        cells[(size_t) i] = std::make_unique<StringToolCell> (processor, i + 1);
        addAndMakeVisible (*cells[(size_t) i]);
    }

    stroke.attachTo (processor, ParamIDs::rhStroke,
                     "Free: the finger pulls across and clears the next string. Rest: it pushes toward the top and lands "
                     "on the next string - louder, fuller, a shorter ring, and that string is damped. Auto: melody notes "
                     "played firmly are rest strokes, chords are free (CC 105 forces rest).");
    addAndMakeVisible (stroke);

    auto attach = [this] (LuthierSlider& s, const char* id, const char* tip)
    {
        s.attachTo (processor, id, tip);
        addAndMakeVisible (s);
    };

    attach (flesh, ParamIDs::fingerFleshReleaseMs, "How long the fingertip pad takes to let go. Longer is rounder: 0.072 ms is 2.2 kHz.");
    attach (nail, ParamIDs::fingerNailReleaseMs, "How long the nail takes to let go. 0.023 ms is 7 kHz.");
    attach (thumbPosition, ParamIDs::thumbPositionOffset, "How much nearer the neck the thumb strikes than the fingers");
    attach (restDamping, ParamIDs::restStrokeDamping, "How firmly a rest stroke's finger lands on the next string");
    attach (travisMute, ParamIDs::thumbPalmMute, "The heel of the hand lightly muting the thumb's strings (Travis picking)");
    attach (hybridSnap, ParamIDs::hybridSnap, "Hybrid picking: how hard the fingers pull up and snap the string against the frets");
    attach (nailVsFlesh, ParamIDs::nailVsFlesh, "Mirror of the PICK group: how much of the nail meets the string");

    fingers = std::make_unique<LuthierToggle> ("Fingers");
    fingers->attachTo (processor, ParamIDs::useFingers, "Mirror of the PICK group: the Global tool plays with fingers");
    addAndMakeVisible (*fingers);

    // bass-techniques.md 11's variation is reused, not duplicated (6).
    hasAlternation = processor.getState().getParameter (ParamIDs::fingerAlternationVariation) != nullptr;

    if (hasAlternation)
        attach (alternation, ParamIDs::fingerAlternationVariation, "How different alternating i and m strokes are");

    alternationNote.setText (hasAlternation ? "" : "Finger alternation: consecutive finger notes alternate i and m",
                             juce::dontSendNotification);

    for (auto* l : { &cutoffNote, &alternationNote })
    {
        l->setFont (Fonts::ui (10.0f));
        l->setColour (juce::Label::textColourId, Palette::textMuted);
        addAndMakeVisible (*l);
    }

    motion.startTimerHz (*this, 4);
    timerCallback();
}

RightHandGroup::~RightHandGroup()
{
    motion.stopTimer();
}

void RightHandGroup::applyStyle (LuthierAudioProcessor& p, int styleIndex)
{
    const auto style = (RhStyle) juce::jlimit (0, (int) RhStyle::numStyles - 1, styleIndex);
    const LuthierAudioProcessor::ScopedUndoAction undo (p, "Right-hand style " + rhStyleNames()[(int) style]);

    write (p, ParamIDs::rhStyle, (double) style);

    const auto table = styleTable (style);

    if (! table.writes)
        return;

    for (int n = 1; n <= 6; ++n)
        write (p, ParamIDs::rhStringTool (n), (double) (n <= 3 ? table.treble : table.bass));

    switch (style)
    {
        case RhStyle::pick:        write (p, ParamIDs::useFingers, 0.0); break;
        case RhStyle::fingerstyle: write (p, ParamIDs::rhStroke, (double) RhStroke::free);      write (p, ParamIDs::nailVsFlesh, 0.4); break;
        case RhStyle::classical:   write (p, ParamIDs::rhStroke, (double) RhStroke::automatic); write (p, ParamIDs::nailVsFlesh, 0.7); break;
        case RhStyle::travis:      write (p, ParamIDs::thumbPalmMute, 0.35); write (p, ParamIDs::nailVsFlesh, 0.3); break;
        case RhStyle::hybrid:      write (p, ParamIDs::hybridSnap, 0.3); write (p, ParamIDs::pickMaterial, 1.0); break;   // Celluloid
        case RhStyle::slapPop:
        case RhStyle::custom:
        case RhStyle::numStyles:
        default: break;
    }
}

juce::String RightHandGroup::describeStyle (LuthierAudioProcessor& p)
{
    const int index = juce::roundToInt (plain (p, ParamIDs::rhStyle));
    const auto table = styleTable ((RhStyle) index);
    bool matches = true;

    if (table.writes)
        for (int n = 1; n <= 6; ++n)
            matches = matches && juce::roundToInt (plain (p, ParamIDs::rhStringTool (n))) == (int) (n <= 3 ? table.treble : table.bass);

    return rhStyleNames()[index] + (matches ? "" : " (modified)");
}

void RightHandGroup::timerCallback()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    const int index = juce::roundToInt (plain (processor, ParamIDs::rhStyle));
    const auto text = describeStyle (processor);

    if (styleBox.getSelectedItemIndex() != index)
        styleBox.setSelectedItemIndex (index, juce::dontSendNotification);

    if (styleBox.getText() != text)
        styleBox.setText (text, juce::dontSendNotification);

    for (auto& c : cells)
        c->repaint();

    const double fleshMs = plain (processor, ParamIDs::fingerFleshReleaseMs);
    const double nailMs = plain (processor, ParamIDs::fingerNailReleaseMs);
    const auto khz = [] (double ms) { return juce::String (Excitation::cutoffForRelease (ms * 0.001) / 1000.0, 2) + " kHz"; };
    const auto note = "Flesh " + khz (fleshMs) + "   Nail " + khz (nailMs) + "   Thumb " + khz (2.0 * fleshMs);

    if (cutoffNote.getText() != note)
        cutoffNote.setText (note, juce::dontSendNotification);
}

int RightHandGroup::preferredHeight() const
{
    return headingHeight + gap + rowHeight + gap + cellHeight + gap + choiceHeight + gap
         + 2 * (rowHeight + gap) + noteHeight + gap
         + 5 * (rowHeight + gap) + (rowHeight + gap)
         + (hasAlternation ? rowHeight : noteHeight) + gap + 4;
}

void RightHandGroup::resized()
{
    auto bounds = getLocalBounds();
    auto take = [&bounds] (int h) { auto r = bounds.removeFromTop (h); bounds.removeFromTop (gap); return r; };

    heading.setBounds (take (headingHeight));

    {
        auto r = take (rowHeight);
        styleLabel.setBounds (r.removeFromLeft (40));
        styleBox.setBounds (r);
    }

    {
        auto r = take (cellHeight);
        const int w = r.getWidth() / 6;

        for (auto& c : cells)
            c->setBounds (r.removeFromLeft (w).reduced (1, 0));
    }

    stroke.setBounds (take (choiceHeight));
    flesh.setBounds (take (rowHeight));
    nail.setBounds (take (rowHeight));
    cutoffNote.setBounds (take (noteHeight));

    for (auto* s : { &thumbPosition, &restDamping, &travisMute, &hybridSnap, &nailVsFlesh })
        s->setBounds (take (rowHeight));

    fingers->setBounds (take (rowHeight).removeFromLeft (100));

    if (hasAlternation)
        alternation.setBounds (take (rowHeight));
    else
        alternationNote.setBounds (take (noteHeight));
}

//==============================================================================
RightHandToolSelector::RightHandToolSelector (LuthierAudioProcessor& p)
    : processor (p)
{
    static const char* tips[] =
    {
        "Mixed: the per-string tools as they are (CHARACTER -> RIGHT HAND)",
        "Pick: every string with the pick",
        "Fingerstyle: fingers on the treble, thumb on the bass, free strokes",
        "Classical: fingers and thumb, rest strokes on firm melody notes",
        "Travis: fingers on the treble, a thumbpick on a lightly muted bass",
        "Hybrid: the pick on the bass, fingers snapping the treble",
        "Slap & Pop: thumb slaps the bass strings, fingers pop the treble"
    };

    for (int i = 0; i < 7; ++i)
    {
        auto b = std::make_unique<juce::TextButton> (labelFor (i));
        b->setClickingTogglesState (false);
        b->setTooltip (tips[i]);
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 6 ? juce::Button::ConnectedOnRight : 0));
        b->onClick = [this, i] { RightHandGroup::applyStyle (processor, i); timerCallback(); };
        addAndMakeVisible (*b);
        segments[(size_t) i] = std::move (b);
    }

    setTitle ("Right-hand tool");
    startTimerHz (4);
    timerCallback();
}

RightHandToolSelector::~RightHandToolSelector()
{
    stopTimer();
}

juce::String RightHandToolSelector::labelFor (int styleIndex)
{
    static const char* labels[] = { "Mixed", "Pick", "Fingers", "Classical", "Travis", "Hybrid", "Slap" };
    return labels[juce::jlimit (0, 6, styleIndex)];
}

void RightHandToolSelector::timerCallback()
{
    const auto* raw = processor.getState().getRawParameterValue (ParamIDs::rhStyle);
    const int now = raw != nullptr ? juce::roundToInt (raw->load()) : 0;

    if (now == selected)
        return;

    selected = now;

    for (int i = 0; i < 7; ++i)
        segments[(size_t) i]->setToggleState (i == selected, juce::dontSendNotification);
}

void RightHandToolSelector::paint (juce::Graphics& g)
{
    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (10.0f));
    g.drawText ("Tool", getLocalBounds().removeFromTop (12), juce::Justification::centredLeft);
}

void RightHandToolSelector::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (12);
    r = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 24));

    const int w = r.getWidth() / 7;

    for (auto& b : segments)
        b->setBounds (r.removeFromLeft (w));
}

} // namespace luthier
