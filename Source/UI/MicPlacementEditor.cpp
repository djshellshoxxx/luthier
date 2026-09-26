#include "MicPlacementEditor.h"
#include "Theme.h"
#include "../Accessibility/Localisation.h"
#include "../Presets/MicPlacementMigration.h"

namespace luthier
{

namespace
{
    // The side view's ruler (6.2): the marks, and log spacing with 0 at the grille.
    constexpr double kRulerMarks[] = { 0.0, 1.0, 2.5, 5.0, 10.0, 15.0, 30.0, 50.0, 100.0 };
    constexpr double kRulerOffsetCm = 1.0;   // log (cm + 1): 0 sits at the grille

    const char* acId (int mic, int which)
    {
        static const char* ids[2][4] = { { ParamIDs::acMicAlong, ParamIDs::acMicAcross, ParamIDs::acMicDist, ParamIDs::acMicAngle },
                                         { ParamIDs::acMicAlong2, ParamIDs::acMicAcross2, ParamIDs::acMicDist2, ParamIDs::acMicAngle2 } };
        return ids[juce::jlimit (0, 1, mic)][juce::jlimit (0, 3, which)];
    }
}

//==============================================================================
MicSideView::MicSideView (LuthierAudioProcessor& p, MicFace& f)
    : processor (p), face (f)
{
    setWantsKeyboardFocus (true);
    setTitle ("Mic side view");
    setTooltip ("Drag the mic for distance; drag its tail for angle");
    startTimerHz (30);
}

MicSideView::~MicSideView()
{
    stopTimer();
}

const char* MicSideView::distId() const
{
    const int mic = face.getFocusedMic();
    return MicUi::isAcoustic (processor) ? acId (mic, 2) : MicPlacementMigration::idsFor (mic).dist;
}

const char* MicSideView::angleId() const
{
    const int mic = face.getFocusedMic();
    return MicUi::isAcoustic (processor) ? acId (mic, 3) : MicPlacementMigration::idsFor (mic).angle;
}

float MicSideView::xForCm (double cm) const
{
    const auto area = getLocalBounds().toFloat().reduced (10.0f, 6.0f).withTrimmedLeft (36.0f);
    const double maxCm = MicUi::isAcoustic (processor) ? 300.0 : 200.0;
    const double t = std::log (juce::jmax (0.0, cm) + kRulerOffsetCm) / std::log (maxCm + kRulerOffsetCm);
    return area.getX() + (float) t * area.getWidth();
}

double MicSideView::cmForX (float x) const
{
    const auto area = getLocalBounds().toFloat().reduced (10.0f, 6.0f).withTrimmedLeft (36.0f);
    const double maxCm = MicUi::isAcoustic (processor) ? 300.0 : 200.0;
    const double t = juce::jlimit (0.0, 1.0, (double) ((x - area.getX()) / juce::jmax (1.0f, area.getWidth())));
    return std::pow (maxCm + kRulerOffsetCm, t) - kRulerOffsetCm;
}

juce::Point<float> MicSideView::micPoint() const
{
    return { xForCm (MicUi::get (processor, distId())), getHeight() * 0.45f };
}

void MicSideView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (area, 4.0f);

    // The cone in profile: a bracket at the grille.
    const float gx = xForCm (0.0);
    g.setColour (Palette::edgeBright);
    juce::Path cone;
    cone.startNewSubPath (gx - 22.0f, area.getY() + 8.0f);
    cone.quadraticTo (gx - 4.0f, area.getCentreY() * 0.9f, gx - 22.0f, area.getBottom() - 22.0f);
    g.strokePath (cone, juce::PathStrokeType (2.0f));
    g.drawVerticalLine ((int) gx, area.getY() + 4.0f, area.getBottom() - 18.0f);

    // The ruler.
    g.setFont (Fonts::ui (9.0f));
    const float rulerY = area.getBottom() - 14.0f;

    for (double cm : kRulerMarks)
    {
        const float x = xForCm (cm);
        g.setColour (Palette::edge);
        g.drawVerticalLine ((int) x, rulerY - 4.0f, rulerY);
        g.setColour (Palette::textMuted);
        g.drawText (juce::String (cm, cm < 10.0 && cm != std::floor (cm) ? 1 : 0),
                    juce::Rectangle<float> (28.0f, 12.0f).withCentre ({ x, rulerY + 6.0f }), juce::Justification::centred);
    }

    // The focused mic: a body, and its tail showing the angle.
    const int mic = face.getFocusedMic();
    const auto at = micPoint();
    const double angle = MicUi::get (processor, angleId());
    const float len = 34.0f;
    const float rad = juce::degreesToRadians ((float) angle);
    const juce::Point<float> tail (at.x + len * std::cos (rad), at.y - len * std::sin (rad));

    g.setColour (MicUi::colourFor (mic));
    g.drawLine ({ at, tail }, 5.0f);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (at));
    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (10.0f, true));
    g.drawText (juce::String (mic + 1), juce::Rectangle<float> (12.0f, 12.0f).withCentre (at), juce::Justification::centred);

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (10.0f));
    g.drawText (juce::String (MicUi::get (processor, distId()), 1) + " cm, " + juce::String (juce::roundToInt (angle)) + " deg",
                area.reduced (6.0f).removeFromTop (14.0f), juce::Justification::topRight);

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accentBright);
        g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
    }
}

void MicSideView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto at = micPoint();
    draggingTail = e.position.getDistanceFrom (at) > 10.0f && e.position.x > at.x;
    const int mic = face.getFocusedMic();
    drag = std::make_unique<MicEdit> (processor, (draggingTail ? "Angle Mic " : "Distance Mic ") + juce::String (mic + 1),
                                      juce::StringArray { draggingTail ? angleId() : distId() });
    mouseDrag (e);
}

void MicSideView::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == nullptr)
        return;

    if (draggingTail)
    {
        const auto at = micPoint();
        const float angle = juce::radiansToDegrees (std::atan2 (at.y - e.position.y, juce::jmax (0.001f, e.position.x - at.x)));
        drag->set (angleId(), juce::jlimit (0.0f, 180.0f, angle));
    }
    else
    {
        drag->set (distId(), (float) cmForX (e.position.x));
    }

    repaint();
}

void MicSideView::mouseUp (const juce::MouseEvent&)
{
    drag.reset();
}

bool MicSideView::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const float step = key.getModifiers().isShiftDown() ? 0.1f : 1.0f;

    if (code == juce::KeyPress::rightKey || code == juce::KeyPress::leftKey)
    {
        MicEdit edit (processor, "Distance", { distId() });
        edit.set (distId(), MicUi::get (processor, distId()) + (code == juce::KeyPress::rightKey ? step : -step));
        return true;
    }

    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        MicEdit edit (processor, "Angle", { angleId() });
        edit.set (angleId(), MicUi::get (processor, angleId()) + (code == juce::KeyPress::upKey ? step : -step));
        return true;
    }

    return false;
}

//==============================================================================
MicPlacementEditor::MicPlacementEditor (LuthierAudioProcessor& p)
    : processor (p), front (p, true)
{
    setWantsKeyboardFocus (true);
    setTitle (MicUi::text ("mic.editor.title"));

    addAndMakeVisible (front);
    addAndMakeVisible (side);
    addAndMakeVisible (plot);

    grilleButton.setClickingTogglesState (true);
    grilleButton.setToggleState (processor.getUiState().micGrilleVisible, juce::dontSendNotification);
    grilleButton.setButtonText (MicUi::text ("mic.editor.grille"));
    grilleButton.onClick = [this]
    {
        processor.getUiState().micGrilleVisible = grilleButton.getToggleState();
        front.setGrilleVisible (grilleButton.getToggleState());
    };

    resetButton.setButtonText (MicUi::text ("mic.editor.reset"));
    resetButton.setTooltip ("Returns the focused mic to its defaults; Shift-click for both");
    resetButton.onClick = [this] { resetMics (juce::ModifierKeys::currentModifiers.isShiftDown()); };

    closeButton.setTooltip ("Close (Escape)");
    closeButton.onClick = [this] { if (onClose) onClose(); };

    for (auto* b : { &grilleButton, &resetButton, &closeButton })
        addAndMakeVisible (b);

    front.onPlacementChanged = [this] { plot.update(); side.repaint(); };

    // The mic cards (6.2): model, rear, speaker and the readouts.
    for (int m = 0; m < 2; ++m)
    {
        auto& c = cards[(size_t) m];
        const auto& ids = MicPlacementMigration::idsFor (m);
        const auto n = juce::String (m + 1);

        c.micType = std::make_unique<LuthierChoice> ("Mic " + n);
        c.micType->attachTo (p, m == 0 ? ParamIDs::micType : ParamIDs::micType2, "Mic " + n + " model");
        c.rear = std::make_unique<LuthierToggle> ("Rear " + n);
        c.rear->attachTo (p, ids.rear, "Mic " + n + " behind the cabinet");

        const auto slider = [&p] (std::unique_ptr<LuthierSlider>& s, const juce::String& label, const char* id, const juce::String& tip)
        {
            s = std::make_unique<LuthierSlider> (label);
            s->attachTo (p, id, tip);
        };

        slider (c.speaker, "Speaker " + n, ids.speaker, "Which speaker mic " + n + " is on");
        slider (c.x, "u X " + n, ids.x, "Across the cone, in landmark units");
        slider (c.y, "u Y " + n, ids.y, "Up the cone, in landmark units");
        slider (c.dist, "cm " + n, ids.dist, "Distance from the grille");
        slider (c.angle, "deg " + n, ids.angle, "Angle off-axis");
        slider (c.along, "Along " + n, acId (m, 0), "0 tail, 1 lower bout, 2 bridge, 3 soundhole, 4 12th fret");
        slider (c.across, "Across " + n, acId (m, 1), "Across the body, + the treble side");
        slider (c.acDist, "cm " + n, acId (m, 2), "Distance from the guitar");
        slider (c.acAngle, "deg " + n, acId (m, 3), "Angle off-axis");

        for (juce::Component* comp : std::initializer_list<juce::Component*> {
                 c.micType.get(), c.rear.get(), c.speaker.get(), c.x.get(), c.y.get(), c.dist.get(), c.angle.get(),
                 c.along.get(), c.across.get(), c.acDist.get(), c.acAngle.get() })
            addChildComponent (comp);
    }

    // Tab order: mic 1, mic 2, the side view, then the cards (section 8).
    front.getHandle (0).setExplicitFocusOrder (1);
    front.getHandle (1).setExplicitFocusOrder (2);
    side.setExplicitFocusOrder (3);
    int order = 4;

    for (auto& c : cards)
        for (juce::Component* comp : std::initializer_list<juce::Component*> {
                 c.micType.get(), c.rear.get(), c.speaker.get(), c.x.get(), c.y.get(), c.dist.get(), c.angle.get() })
            comp->setExplicitFocusOrder (order++);

    applyFamily();
    startTimerHz (10);
}

MicPlacementEditor::~MicPlacementEditor()
{
    stopTimer();
}

juce::String MicPlacementEditor::getTitleText() const
{
    if (MicUi::isAcoustic (processor))
        return MicUi::text ("mic.editor.title") + "  [" + processor.getEngine().getGuitarSpec().name + "]";

    return MicUi::text ("mic.editor.title") + "  [" + CabinetEngine::getCabinetName (MicUi::cabinetOf (processor))
           + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "))
           + CabinetEngine::getSpeakerName ((SpeakerType) juce::roundToInt (MicUi::get (processor, ParamIDs::cabSpeaker))) + "]";
}

void MicPlacementEditor::applyFamily()
{
    shownAcoustic = MicUi::isAcoustic (processor);

    for (auto& c : cards)
    {
        for (juce::Component* comp : std::initializer_list<juce::Component*> { c.rear.get(), c.speaker.get(), c.x.get(), c.y.get(), c.dist.get(), c.angle.get() })
            comp->setVisible (! shownAcoustic);

        for (juce::Component* comp : std::initializer_list<juce::Component*> { c.along.get(), c.across.get(), c.acDist.get(), c.acAngle.get() })
            comp->setVisible (shownAcoustic);

        c.micType->setVisible (true);
    }

    grilleButton.setVisible (! shownAcoustic);
    front.refresh();
    resized();
}

void MicPlacementEditor::timerCallback()
{
    if (MicUi::isAcoustic (processor) != shownAcoustic)
        applyFamily();

    repaint (getLocalBounds().removeFromTop (kHeader));
}

void MicPlacementEditor::resetMics (bool both)
{
    const bool acoustic = MicUi::isAcoustic (processor);
    juce::StringArray ids;

    for (int m = 0; m < 2; ++m)
    {
        if (! both && m != front.getFocusedMic())
            continue;

        if (acoustic)
        {
            for (int i = 0; i < 4; ++i)
                ids.add (acId (m, i));
        }
        else
        {
            const auto& e = MicPlacementMigration::idsFor (m);
            ids.addArray ({ e.x, e.y, e.dist, e.angle, e.speaker, e.rear });
        }
    }

    // One multi-target entry (section 8).
    MicEdit edit (processor, both ? "Reset both mics" : "Reset Mic " + juce::String (front.getFocusedMic() + 1), ids);

    for (const auto& id : ids)
        if (auto* prm = processor.getState().getParameter (id))
            prm->setValueNotifyingHost (prm->getDefaultValue());

    front.refresh();
    plot.update();
}

void MicPlacementEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);
    auto header = getLocalBounds().removeFromTop (kHeader).reduced (Metrics::grid, 0);
    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::display (18.0f));
    g.drawText (getTitleText().toUpperCase(), header, juce::Justification::centredLeft);
}

void MicPlacementEditor::resized()
{
    // A family switch caught by a layout pass before the timer's.
    if (MicUi::isAcoustic (processor) != shownAcoustic)
    {
        applyFamily();
        return;
    }

    front.refresh();
    auto area = getLocalBounds();
    auto header = area.removeFromTop (kHeader).reduced (Metrics::gridHalf);
    closeButton.setBounds (header.removeFromRight (28));
    header.removeFromRight (Metrics::gridHalf);
    resetButton.setBounds (header.removeFromRight (64));
    header.removeFromRight (Metrics::gridHalf);
    grilleButton.setBounds (header.removeFromRight (72));

    area.reduce (Metrics::grid, Metrics::gridHalf);

    auto bottom = area.removeFromBottom (juce::jmax (120, area.getHeight() / 4));
    plot.setBounds (bottom);
    area.removeFromBottom (Metrics::grid);

    auto right = area.removeFromRight (juce::jmax (260, area.getWidth() * 2 / 5));
    area.removeFromRight (Metrics::grid);
    front.setBounds (area);

    side.setBounds (right.removeFromTop (juce::jlimit (80, 140, right.getHeight() / 4)));
    right.removeFromTop (Metrics::gridHalf);

    // Two cards side by side, or stacked when narrow.
    const bool sideBySide = right.getWidth() >= 520;
    const int rows = shownAcoustic ? 4 : 6;
    const int rowH = juce::jlimit (18, 26, ((sideBySide ? right.getHeight() : right.getHeight() / 2) - 36) / rows);

    for (int m = 0; m < 2; ++m)
    {
        auto card = sideBySide ? (m == 0 ? right.removeFromLeft (right.getWidth() / 2) : right)
                               : right.removeFromTop (right.getHeight() / (2 - m));
        auto& c = cards[(size_t) m];
        c.micType->setBounds (card.removeFromTop (36));

        if (shownAcoustic)
        {
            for (auto* s : { c.along.get(), c.across.get(), c.acDist.get(), c.acAngle.get() })
                s->setBounds (card.removeFromTop (rowH));
        }
        else
        {
            c.rear->setBounds (card.removeFromTop (rowH).reduced (0, 1));

            for (auto* s : { c.speaker.get(), c.x.get(), c.y.get(), c.dist.get(), c.angle.get() })
                s->setBounds (card.removeFromTop (rowH));
        }
    }
}

bool MicPlacementEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose)
            onClose();

        return true;
    }

    return false;
}

//==============================================================================
MicPad::MicPad (LuthierAudioProcessor& p)
    : processor (p)
{
    setWantsKeyboardFocus (true);
    setTitle (MicUi::text ("mic.pad.label"));
    setTooltip ("Across: bright to warm. Down: close to far. Double-click for the full editor.");
    startTimerHz (15);
}

MicPad::~MicPad()
{
    stopTimer();
}

namespace
{
    // Vertical: 1 to 100 cm, log; the top is Close.
    float padYForCm (double cm) { return (float) (std::log10 (juce::jlimit (1.0, 100.0, cm)) / 2.0); }
    double cmForPadY (float y)   { return std::pow (10.0, 2.0 * juce::jlimit (0.0f, 1.0f, y)); }
}

juce::Point<float> MicPad::padPositionOf (int mic) const
{
    if (MicUi::isAcoustic (processor))
    {
        const auto a = MicUi::acousticPlacementOf (processor, mic);
        return { juce::jlimit (0.0f, 1.0f, (float) ((4.0 - a.along) / 3.0)), padYForCm (a.distCm) };
    }

    const auto m = MicUi::placementOf (processor, mic);
    return { juce::jlimit (0.0f, 1.0f, (float) (m.radius() / 0.9)), padYForCm (m.distCm) };
}

void MicPad::setFromPad (juce::Point<float> n)
{
    n = { juce::jlimit (0.0f, 1.0f, n.x), juce::jlimit (0.0f, 1.0f, n.y) };
    const double cm = cmForPadY (n.y);

    if (MicUi::isAcoustic (processor))
    {
        MicUi::set (processor, ParamIDs::acMicAlong, (float) (4.0 - 3.0 * n.x));
        MicUi::set (processor, ParamIDs::acMicDist, (float) cm);
        return;
    }

    // Keep the handle's current direction on the cone (6.3).
    const auto m = MicUi::placementOf (processor, 0);
    const double u = m.radius();
    const double dirX = u > 1.0e-6 ? m.x / u : 1.0, dirY = u > 1.0e-6 ? m.y / u : 0.0;
    const double target = 0.9 * n.x;
    MicUi::set (processor, ParamIDs::micX, (float) (dirX * target));
    MicUi::set (processor, ParamIDs::micY, (float) (dirY * target));
    MicUi::set (processor, ParamIDs::micDist, (float) cm);
}

bool MicPad::isShowingGhost() const
{
    return MicUi::dualMicOn (processor);
}

void MicPad::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (area, 3.0f);

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (8.0f));
    g.drawText ("bright", area.reduced (3.0f), juce::Justification::bottomLeft);
    g.drawText ("warm", area.reduced (3.0f), juce::Justification::bottomRight);
    g.drawText ("Close", area.reduced (3.0f), juce::Justification::topLeft);

    const auto toPixel = [&area] (juce::Point<float> n)
    {
        return juce::Point<float> (area.getX() + 6.0f + n.x * (area.getWidth() - 12.0f),
                                   area.getY() + 6.0f + n.y * (area.getHeight() - 12.0f));
    };

    if (isShowingGhost())
    {
        g.setColour (MicUi::colourFor (1));
        g.drawEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (toPixel (padPositionOf (1))), 1.4f);
    }

    const auto at = toPixel (padPositionOf (0));
    g.setColour (MicUi::colourFor (0));
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (at));
    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (9.0f, true));
    g.drawText ("1", juce::Rectangle<float> (12.0f, 12.0f).withCentre (at), juce::Justification::centred);

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accentBright);
        g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 1.0f);
    }
}

void MicPad::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const bool acoustic = MicUi::isAcoustic (processor);
    juce::StringArray ids = acoustic ? juce::StringArray { ParamIDs::acMicAlong, ParamIDs::acMicDist }
                                     : juce::StringArray { ParamIDs::micX, ParamIDs::micY, ParamIDs::micDist };
    drag = std::make_unique<MicEdit> (processor, "Move Mic 1", ids);
    ++MicUi::activeDrags();
    mouseDrag (e);
}

void MicPad::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == nullptr)
        return;

    const auto area = getLocalBounds().toFloat().reduced (6.0f);
    setFromPad ({ (e.position.x - area.getX()) / juce::jmax (1.0f, area.getWidth()),
                  (e.position.y - area.getY()) / juce::jmax (1.0f, area.getHeight()) });
    repaint();
}

void MicPad::mouseUp (const juce::MouseEvent&)
{
    if (drag != nullptr)
        --MicUi::activeDrags();

    drag.reset();
}

void MicPad::mouseDoubleClick (const juce::MouseEvent&)
{
    drag.reset();

    if (onOpenEditor)
        onOpenEditor();
}

bool MicPad::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    const float step = key.getModifiers().isShiftDown() ? 0.01f : 0.05f;
    auto at = padPositionOf (0);

    if      (code == juce::KeyPress::rightKey) at.x += step;
    else if (code == juce::KeyPress::leftKey)  at.x -= step;
    else if (code == juce::KeyPress::downKey)  at.y += step;
    else if (code == juce::KeyPress::upKey)    at.y -= step;
    else if (code == juce::KeyPress::returnKey) { if (onOpenEditor) onOpenEditor(); return true; }
    else return false;

    const bool acoustic = MicUi::isAcoustic (processor);
    MicEdit edit (processor, "Move Mic 1", acoustic ? juce::StringArray { ParamIDs::acMicAlong, ParamIDs::acMicDist }
                                                    : juce::StringArray { ParamIDs::micX, ParamIDs::micY, ParamIDs::micDist });
    setFromPad (at);
    repaint();
    return true;
}

std::unique_ptr<juce::AccessibilityHandler> MicPad::createAccessibilityHandler()
{
    struct Value : public juce::AccessibilityValueInterface
    {
        explicit Value (MicPad& p) : pad (p) {}
        bool isReadOnly() const override { return true; }
        double getCurrentValue() const override { return 0.0; }
        void setValue (double) override {}
        juce::String getCurrentValueAsString() const override { return MicUi::describe (pad.processor, 0); }
        void setValueAsString (const juce::String&) override {}
        AccessibleValueRange getRange() const override { return {}; }
        MicPad& pad;
    };

    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::slider, juce::AccessibilityActions(),
        juce::AccessibilityHandler::Interfaces { std::make_unique<Value> (*this) });
}

} // namespace luthier
