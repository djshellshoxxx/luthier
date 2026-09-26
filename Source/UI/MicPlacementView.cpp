#include "MicPlacementView.h"
#include <cstring>
#include "Theme.h"
#include "UiPreferences.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"
#include "../Presets/MicPlacementMigration.h"
#include "../Support/IrLibrary.h"
#include "../UI/Guitar/BodyOutlines.h"

namespace luthier
{

//==============================================================================
namespace MicUi
{
    namespace
    {
        // How many handle drags are in flight, so the plot can wait for the
        // release under reduced motion (mic-placement.md 8).
        int dragsInFlight = 0;
    }

    int& activeDrags() { return dragsInFlight; }

    const std::vector<Ring>& rings()
    {
        static const std::vector<Ring> r
        {
            { 0.0,  "mic.ring.cap",     "Cap" },
            { 0.35, "mic.ring.capEdge", "Cap Edge" },
            { 0.62, "mic.ring.cone",    "Cone" },
            { 0.90, "mic.ring.edge",    "Edge" }
        };
        return r;
    }

    const std::vector<Landmark>& landmarks()
    {
        static const std::vector<Landmark> l
        {
            { 0.0, "Tail" }, { 1.0, "Lower Bout" }, { 2.0, "Bridge" }, { 3.0, "Soundhole" },
            { 3.5, "Upper Bout" }, { 4.0, "12th Fret" }
        };
        return l;
    }

    juce::String text (const char* key)
    {
        return Localisation::get().translate (key);
    }

    float get (LuthierAudioProcessor& p, const char* id)
    {
        if (auto* raw = p.getState().getRawParameterValue (id))
            return raw->load();

        return 0.0f;
    }

    void set (LuthierAudioProcessor& p, const char* id, float plain)
    {
        if (auto* prm = p.getState().getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
    }

    bool isAcoustic (LuthierAudioProcessor& p)
    {
        return p.getEngine().getGuitarSpec().category == GuitarCategory::Acoustic;
    }

    CabinetType cabinetOf (LuthierAudioProcessor& p)
    {
        return (CabinetType) juce::jlimit (0, (int) CabinetType::NumCabinets - 1, juce::roundToInt (get (p, ParamIDs::cabType)));
    }

    MicType micTypeOf (LuthierAudioProcessor& p, int mic)
    {
        return (MicType) juce::jlimit (0, (int) MicType::NumMics - 1,
                                       juce::roundToInt (get (p, mic == 0 ? ParamIDs::micType : ParamIDs::micType2)));
    }

    bool dualMicOn (LuthierAudioProcessor& p)
    {
        return isAcoustic (p) ? get (p, ParamIDs::acMic2On) > 0.5f : get (p, ParamIDs::dualMic) > 0.5f;
    }

    MicPlacement placementOf (LuthierAudioProcessor& p, int mic)
    {
        const auto& ids = MicPlacementMigration::idsFor (mic);
        MicPlacement m;
        m.x = get (p, ids.x);
        m.y = get (p, ids.y);
        m.distCm = get (p, ids.dist);
        m.angleDeg = get (p, ids.angle);
        m.speaker = juce::roundToInt (get (p, ids.speaker));
        m.rear = get (p, ids.rear) > 0.5f;
        return m;
    }

    AcousticMicPlacement acousticPlacementOf (LuthierAudioProcessor& p, int mic)
    {
        if (mic == 0)
            return { get (p, ParamIDs::acMicAlong), get (p, ParamIDs::acMicAcross),
                     get (p, ParamIDs::acMicDist), get (p, ParamIDs::acMicAngle) };

        return { get (p, ParamIDs::acMicAlong2), get (p, ParamIDs::acMicAcross2),
                 get (p, ParamIDs::acMicDist2), get (p, ParamIDs::acMicAngle2) };
    }

    MicPlacementModel::Input inputFor (LuthierAudioProcessor& p, int mic)
    {
        const auto place = placementOf (p, mic);
        MicPlacementModel::Input in;
        in.cabinet = cabinetOf (p);
        in.speaker = (SpeakerType) juce::jlimit (0, (int) SpeakerType::NumSpeakers - 1, juce::roundToInt (get (p, ParamIDs::cabSpeaker)));
        in.mic = micTypeOf (p, mic);
        in.x = place.x;
        in.y = place.y;
        in.distCm = place.distCm;
        in.angleDeg = place.angleDeg;
        in.rearAmount = place.rear ? 1.0 : 0.0;

        const int s = resolveSpeaker (in.cabinet, place.speaker);
        MicPlacementModel::speakerVariation (in.cabinet, s, in.variation);
        in.speakerHeightM = speakerHeightM (in.cabinet, s);
        in.levelMatch = get (p, ParamIDs::micLevelMatch) > 0.5f;
        in.floorRho = MicPlacementModel::floorRhoFor (juce::roundToInt (get (p, ParamIDs::roomMaterial)), get (p, ParamIDs::roomOn) > 0.5f);
        in.invertRearPolarity = get (p, ParamIDs::micTofMode) > 0.5f;
        return in;
    }

    juce::String nearestLandmark (double u)
    {
        const auto& r = rings();
        const Ring* best = &r.front();

        for (const auto& ring : r)
            if (std::abs (ring.u - u) < std::abs (best->u - u))
                best = &ring;

        if (u > 1.12)
            return "Baffle";

        if (u > 1.0)
            return "Surround";

        return (std::abs (best->u - u) < 0.005 ? juce::String() : juce::String ("near ")) + best->label;
    }

    juce::String nearestAcousticLandmark (double along)
    {
        const Landmark* best = &landmarks().front();

        for (const auto& l : landmarks())
            if (std::abs (l.along - along) < std::abs (best->along - along))
                best = &l;

        return (std::abs (best->along - along) < 0.005 ? juce::String() : juce::String ("near ")) + best->label;
    }

    juce::String describe (LuthierAudioProcessor& p, int mic)
    {
        const auto micName = juce::String (CabinetEngine::getMicName (micTypeOf (p, mic)));
        const auto number = juce::String (mic + 1);

        if (isAcoustic (p))
        {
            const auto a = acousticPlacementOf (p, mic);
            return Localisation::get().translate ("mic.a11y.acoustic",
                   { { "n", number }, { "mic", micName }, { "where", nearestAcousticLandmark (a.along) },
                     { "cm", juce::String (a.distCm, 1) }, { "deg", juce::String (juce::roundToInt (a.angleDeg)) } });
        }

        const auto m = placementOf (p, mic);
        const auto cab = cabinetOf (p);
        return Localisation::get().translate ("mic.a11y.value",
               { { "n", number }, { "mic", micName }, { "where", nearestLandmark (m.radius()) },
                 { "cm", juce::String (m.distCm, 1) }, { "deg", juce::String (juce::roundToInt (m.angleDeg)) },
                 { "speaker", juce::String (resolveSpeaker (cab, m.speaker)) },
                 { "of", juce::String (cabGeometry (cab).numSpeakers) } });
    }

    juce::Colour colourFor (int mic)
    {
        return mic == 0 ? Palette::accent : Palette::secondary;
    }

    bool isInNull (LuthierAudioProcessor& p, int mic)
    {
        const auto type = micTypeOf (p, mic);
        const double angle = isAcoustic (p) ? acousticPlacementOf (p, mic).angleDeg : placementOf (p, mic).angleDeg;
        const bool figure8 = micPolar (type).a == 0.0;
        return figure8 ? std::abs (angle - 90.0) < 10.0 : angle > 150.0;
    }

    juce::StringArray statusMessages (LuthierAudioProcessor& p)
    {
        juce::StringArray messages;
        const bool acoustic = isAcoustic (p);

        if (acoustic)
        {
            if (get (p, ParamIDs::acMicMix) <= 0.0f)
                messages.add (text ("mic.status.acousticSilent"));
        }
        else
        {
            if (get (p, ParamIDs::cabOn) < 0.5f)
                messages.add (text ("mic.status.cabinetOff"));

            if (cabinetOf (p) == CabinetType::AcousticDI)
                messages.add (text ("mic.status.acousticDi"));

            for (int slot = 0; slot < 2; ++slot)
                if (p.getCabIrSlot (slot).isEngaged())
                    messages.add (text ("mic.status.bakedIr"));
        }

        for (int mic = 0; mic < (dualMicOn (p) ? 2 : 1); ++mic)
            if (isInNull (p, mic))
                messages.add (text ("mic.chip.null"));

        messages.removeDuplicates (false);
        return messages;
    }
}

//==============================================================================
MicEdit::MicEdit (LuthierAudioProcessor& p, const juce::String& description, juce::StringArray idsIn)
    : processor (p), ids (std::move (idsIn))
{
    undo = std::make_unique<LuthierAudioProcessor::ScopedUndoAction> (processor, description);

    for (const auto& id : ids)
        if (auto* prm = processor.getState().getParameter (id))
            prm->beginChangeGesture();
}

MicEdit::~MicEdit()
{
    for (const auto& id : ids)
        if (auto* prm = processor.getState().getParameter (id))
            prm->endChangeGesture();

    undo.reset();
}

void MicEdit::set (const char* id, float plain)
{
    MicUi::set (processor, id, plain);
}

//==============================================================================
MicHandle::MicHandle (MicFace& owner, int m)
    : face (owner), mic (m)
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTitle (Localisation::get().translate ("mic.a11y.group", { { "n", juce::String (mic + 1) } }));

    for (auto& proxy : proxies)
    {
        proxy = std::make_unique<ProxySlider>();
        addAndMakeVisible (*proxy);
    }

    rebind();
}

MicHandle::~MicHandle()
{
    stopTimer();
    nudgeGroup.reset();
    drag.reset();

    for (auto& a : proxyAttachments)
        a.reset();
}

void MicHandle::rebind()
{
    auto& p = face.getProcessor();
    const bool acoustic = MicUi::isAcoustic (p);

    if (acoustic == boundAcoustic && proxyAttachments[0] != nullptr)
        return;

    boundAcoustic = acoustic;
    const auto& ids = MicPlacementMigration::idsFor (mic);

    const char* acIds[2][4] = { { ParamIDs::acMicAlong, ParamIDs::acMicAcross, ParamIDs::acMicDist, ParamIDs::acMicAngle },
                                { ParamIDs::acMicAlong2, ParamIDs::acMicAcross2, ParamIDs::acMicDist2, ParamIDs::acMicAngle2 } };
    const char* elIds[4] = { ids.x, ids.y, ids.dist, ids.angle };
    const char* names[2][4] = { { "X", "Y", "Distance", "Angle" }, { "Along", "Across", "Distance", "Angle" } };

    for (int i = 0; i < 4; ++i)
    {
        proxyAttachments[(size_t) i].reset();
        const char* id = acoustic ? acIds[mic][i] : elIds[i];

        if (auto* prm = p.getState().getParameter (id))
            proxyAttachments[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.getState(), id, *proxies[(size_t) i]);

        proxies[(size_t) i]->setTitle ("Mic " + juce::String (mic + 1) + " " + names[acoustic ? 1 : 0][i]);
    }
}

void MicHandle::resized()
{
    // The proxies sit inside the handle, invisible; they exist for assistive
    // technology (and are what a screen reader's slider commands move).
    for (int i = 0; i < 4; ++i)
        proxies[(size_t) i]->setBounds (1 + i, 1, 1, 1);
}

std::unique_ptr<juce::AccessibilityHandler> MicHandle::createAccessibilityHandler()
{
    struct Value : public juce::AccessibilityValueInterface
    {
        explicit Value (MicHandle& h) : handle (h) {}
        bool isReadOnly() const override { return true; }
        double getCurrentValue() const override { return 0.0; }
        void setValue (double) override {}
        juce::String getCurrentValueAsString() const override { return MicUi::describe (handle.face.getProcessor(), handle.mic); }
        void setValueAsString (const juce::String&) override {}
        AccessibleValueRange getRange() const override { return {}; }
        MicHandle& handle;
    };

    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::group,
        juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { grabKeyboardFocus(); }),
        juce::AccessibilityHandler::Interfaces { std::make_unique<Value> (*this) });
}

void MicHandle::paint (juce::Graphics& g)
{
    auto& p = face.getProcessor();
    const auto colour = MicUi::colourFor (mic);
    const bool acoustic = MicUi::isAcoustic (p);
    const bool hollow = acoustic && MicUi::get (p, ParamIDs::acMicMix) <= 0.0f;
    const auto type = MicUi::micTypeOf (p, mic);
    const double angle = acoustic ? MicUi::acousticPlacementOf (p, mic).angleDeg : MicUi::placementOf (p, mic).angleDeg;

    // Each generic silhouette at its true relative size (no trade dress).
    static constexpr float relativeSize[] = { 0.72f, 0.95f, 0.82f, 1.0f, 0.92f, 0.9f, 0.98f };
    const float s = relativeSize[(int) type] * (float) kSize * 0.82f;

    // Off-axis: foreshortened across, plus an aim arrow.
    const float squash = juce::jlimit (0.3f, 1.0f, (float) std::abs (std::cos (juce::degreesToRadians (angle))));
    auto body = juce::Rectangle<float> (s * squash, s).withCentre (getLocalBounds().toFloat().getCentre());

    juce::Path shape;

    switch (type)
    {
        case MicType::SM57:        shape.addEllipse (body); break;                                   // small ridged round grille
        case MicType::SM7B:        shape.addEllipse (body.expanded (1.0f)); break;                   // foam ball
        case MicType::MD421:       shape.addEllipse (body.withSizeKeepingCentre (body.getWidth() * 0.8f, body.getHeight())); break;
        case MicType::U87:         shape.addRoundedRectangle (body, body.getWidth() * 0.35f); break; // large rounded mesh
        case MicType::RibbonR121:  shape.addRectangle (body.withSizeKeepingCentre (body.getWidth() * 0.7f, body.getHeight())); break;
        case MicType::C414:        shape.addRoundedRectangle (body, 2.0f); break;                    // squared mesh
        case MicType::D112:        shape.addEllipse (body.withSizeKeepingCentre (body.getWidth(), body.getHeight() * 1.1f)); break; // egg
        case MicType::NumMics:
        default:                   shape.addEllipse (body); break;
    }

    if (hollow)
    {
        g.setColour (colour);
        g.strokePath (shape, juce::PathStrokeType (1.6f));
    }
    else
    {
        g.setColour (Palette::panelSunken);
        g.fillPath (shape);
        g.setColour (colour);
        g.strokePath (shape, juce::PathStrokeType (1.6f));

        // The grille's texture: ridges, slots or mesh.
        g.saveState();
        g.reduceClipRegion (shape);
        g.setColour (colour.withAlpha (0.35f));

        for (float y = body.getY() + 2.0f; y < body.getBottom(); y += 3.0f)
            g.drawHorizontalLine ((int) y, body.getX(), body.getRight());

        if (type == MicType::U87 || type == MicType::C414 || type == MicType::MD421 || type == MicType::RibbonR121)
            for (float x = body.getX() + 2.0f; x < body.getRight(); x += 3.0f)
                g.drawVerticalLine ((int) x, body.getY(), body.getBottom());

        g.restoreState();
    }

    if (angle > 1.0)
    {
        const auto c = body.getCentre();
        const float len = (float) (s * 0.6 * std::sin (juce::degreesToRadians (juce::jmin (90.0, angle))));
        g.setColour (colour);
        g.drawArrow ({ c, c.translated (len, -len * 0.4f) }, 1.2f, 4.0f, 4.0f);
    }

    // The digit: the handle reads without colour vision.
    g.setColour (hollow ? colour : Palette::textPrimary);
    g.setFont (Fonts::ui (11.0f, true));
    g.drawText (juce::String (mic + 1), getLocalBounds(), juce::Justification::centred);

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accentBright);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);
    }
}

void MicHandle::mouseDown (const juce::MouseEvent& e)
{
    face.setFocusedMic (mic);
    grabKeyboardFocus();

    if (e.mods.isPopupMenu())
    {
        // gui-integration 16's parameter menu for X, with Y, Distance and Angle
        // in a submenu.
        auto& p = face.getProcessor();
        const bool acoustic = MicUi::isAcoustic (p);

        juce::PopupMenu menu, more;
        menu.addSectionHeader (getTitle());
        menu.addItem (1, "Reset " + juce::String (acoustic ? "Along" : "X"));
        more.addItem (2, "Reset " + juce::String (acoustic ? "Across" : "Y"));
        more.addItem (3, "Reset Distance");
        more.addItem (4, "Reset Angle");
        menu.addSubMenu ("More", more);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int result)
        {
            if (result <= 0)
                return;

            auto& proc = face.getProcessor();
            auto& state = proc.getState();

            const auto& idsInner = MicPlacementMigration::idsFor (mic);
            const bool ac = MicUi::isAcoustic (proc);
            const char* acIds[2][4] = { { ParamIDs::acMicAlong, ParamIDs::acMicAcross, ParamIDs::acMicDist, ParamIDs::acMicAngle },
                                        { ParamIDs::acMicAlong2, ParamIDs::acMicAcross2, ParamIDs::acMicDist2, ParamIDs::acMicAngle2 } };
            const char* elIds[4] = { idsInner.x, idsInner.y, idsInner.dist, idsInner.angle };
            const char* id = ac ? acIds[mic][result - 1] : elIds[result - 1];

            if (auto* prm = state.getParameter (id))
            {
                MicEdit edit (proc, "Reset " + prm->getName (64), { id });
                prm->setValueNotifyingHost (prm->getDefaultValue());
            }
        });

        return;
    }

    auto& p = face.getProcessor();
    const bool acoustic = MicUi::isAcoustic (p);
    const auto& ids = MicPlacementMigration::idsFor (mic);

    // One undo entry for the whole drag, both coordinates (section 8).
    const auto from = acoustic ? MicUi::nearestAcousticLandmark (MicUi::acousticPlacementOf (p, mic).along)
                               : MicUi::nearestLandmark (MicUi::placementOf (p, mic).radius());
    const auto cm = acoustic ? MicUi::acousticPlacementOf (p, mic).distCm : MicUi::placementOf (p, mic).distCm;
    dragStartText = Localisation::get().translate ("mic.undo.move", { { "n", juce::String (mic + 1) }, { "from", from },
                                                                    { "cm", juce::String (cm, 1) } });

    juce::StringArray dragIds;
    if (acoustic) dragIds = { mic == 0 ? ParamIDs::acMicAlong : ParamIDs::acMicAlong2, mic == 0 ? ParamIDs::acMicAcross : ParamIDs::acMicAcross2 };
    else          dragIds = { ids.x, ids.y };

    drag = std::make_unique<MicEdit> (p, dragStartText, dragIds);
    ++MicUi::activeDrags();

    // Grab the handle where it was clicked, not by its centre.
    dragOffset = face.handleCentre (mic) - e.getEventRelativeTo (&face).position;
}

void MicHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == nullptr)
        return;

    auto& p = face.getProcessor();
    const auto at = e.getEventRelativeTo (&face).position + dragOffset;
    auto xy = face.placementAt (at, mic);

    if (MicUi::isAcoustic (p))
    {
        drag->set (mic == 0 ? ParamIDs::acMicAlong : ParamIDs::acMicAlong2, (float) xy.x);
        drag->set (mic == 0 ? ParamIDs::acMicAcross : ParamIDs::acMicAcross2, (float) xy.y);
    }
    else
    {
        // Magnetic snap within 6 px, unless Alt is held (6.2).
        bool snappedNow = false;

        if (! e.mods.isAltDown() && UiPreferences::get().getBool ("mic.snapToLandmarks", true))
            xy = face.snapped (xy, snappedNow);

        const auto& ids = MicPlacementMigration::idsFor (mic);
        drag->set (ids.x, (float) xy.x);
        drag->set (ids.y, (float) xy.y);
    }

    face.refresh();

    if (face.onPlacementChanged)
        face.onPlacementChanged();
}

void MicHandle::mouseUp (const juce::MouseEvent& e)
{
    if (drag == nullptr)
        return;

    auto& p = face.getProcessor();

    if (! MicUi::isAcoustic (p) && ! e.mods.isAltDown() && UiPreferences::get().getBool ("mic.snapToLandmarks", true))
    {
        bool didSnap = false;
        const auto& ids = MicPlacementMigration::idsFor (mic);
        const auto xy = face.snapped ({ MicUi::get (p, ids.x), MicUi::get (p, ids.y) }, didSnap);

        if (didSnap)
        {
            drag->set (ids.x, (float) xy.x);
            drag->set (ids.y, (float) xy.y);

            // A 60 ms ease on the drawing; none under reduced motion.
            if (! AccessibilitySettings::get().isReducedMotion())
            {
                easeLeft = 4;
                startTimerHz (60);
            }
        }
    }

    drag.reset();
    --MicUi::activeDrags();
    face.refresh();

    if (face.onPlacementChanged)
        face.onPlacementChanged();
}

void MicHandle::mouseDoubleClick (const juce::MouseEvent&)
{
    if (face.onOpenEditor)
        face.onOpenEditor();
}

void MicHandle::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // The wheel over a handle is distance; Shift for fine (6.2).
    const bool acoustic = MicUi::isAcoustic (face.getProcessor());
    const char* id = acoustic ? (mic == 0 ? ParamIDs::acMicDist : ParamIDs::acMicDist2)
                              : MicPlacementMigration::idsFor (mic).dist;
    const float step = e.mods.isShiftDown() ? 0.1f : 1.0f;
    nudge (id, wheel.deltaY > 0.0f ? step : -step, "Distance");
}

void MicHandle::nudge (const char* id, float delta, const juce::String& what)
{
    auto& p = face.getProcessor();
    const double now = juce::Time::getMillisecondCounterHiRes();

    // Nudges to the same parameter within 200 ms are one undo entry (8).
    if (nudgeGroup == nullptr || nudgeParam != id || now - lastNudgeMs > 200.0)
    {
        nudgeGroup.reset();
        nudgeGroup = std::make_unique<MicEdit> (p, "Nudge Mic " + juce::String (mic + 1) + " " + what, juce::StringArray { id });
        nudgeParam = id;
    }

    lastNudgeMs = now;

    if (auto* prm = p.getState().getParameter (id))
    {
        const auto range = prm->getNormalisableRange();
        const float value = range.snapToLegalValue (MicUi::get (p, id) + delta);
        nudgeGroup->set (id, value);
    }

    startTimer (50);
    face.refresh();

    if (face.onPlacementChanged)
        face.onPlacementChanged();
}

void MicHandle::endNudgeGroup()
{
    nudgeGroup.reset();
    nudgeParam.clear();
}

void MicHandle::focusLost (FocusChangeType)
{
    endNudgeGroup();
    repaint();
}

void MicHandle::timerCallback()
{
    if (easeLeft > 0)
    {
        --easeLeft;
        face.repaint();
    }

    if (nudgeGroup != nullptr && juce::Time::getMillisecondCounterHiRes() - lastNudgeMs > 200.0)
        endNudgeGroup();

    if (easeLeft <= 0 && nudgeGroup == nullptr)
        stopTimer();
}

bool MicHandle::keyPressed (const juce::KeyPress& key)
{
    auto& p = face.getProcessor();
    const bool acoustic = MicUi::isAcoustic (p);
    const auto& ids = MicPlacementMigration::idsFor (mic);
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();

    const char* xId = acoustic ? (mic == 0 ? ParamIDs::acMicAlong : ParamIDs::acMicAlong2) : ids.x;
    const char* yId = acoustic ? (mic == 0 ? ParamIDs::acMicAcross : ParamIDs::acMicAcross2) : ids.y;
    const char* dId = acoustic ? (mic == 0 ? ParamIDs::acMicDist : ParamIDs::acMicDist2) : ids.dist;
    const char* aId = acoustic ? (mic == 0 ? ParamIDs::acMicAngle : ParamIDs::acMicAngle2) : ids.angle;

    // Section 8's table. Alt+arrows first: they are angle, not position.
    if (mods.isAltDown() && (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey))
    {
        const float step = mods.isShiftDown() ? 0.2f : 1.0f;
        nudge (aId, code == juce::KeyPress::upKey ? step : -step, "Angle");
        return true;
    }

    const float posStep = mods.isShiftDown() ? 0.002f : mods.isCommandDown() || mods.isCtrlDown() ? 0.05f : 0.01f;
    const float alongScale = acoustic ? 2.0f : 1.0f;

    if (code == juce::KeyPress::rightKey) { nudge (xId,  posStep * alongScale, acoustic ? "Along" : "X"); return true; }
    if (code == juce::KeyPress::leftKey)  { nudge (xId, -posStep * alongScale, acoustic ? "Along" : "X"); return true; }
    if (code == juce::KeyPress::upKey)    { nudge (yId, acoustic ? -posStep : posStep, acoustic ? "Across" : "Y"); return true; }
    if (code == juce::KeyPress::downKey)  { nudge (yId, acoustic ? posStep : -posStep, acoustic ? "Across" : "Y"); return true; }

    if (code == juce::KeyPress::pageUpKey || code == juce::KeyPress::pageDownKey)
    {
        const float step = mods.isShiftDown() ? 0.1f : mods.isCommandDown() || mods.isCtrlDown() ? 5.0f : 1.0f;
        nudge (dId, code == juce::KeyPress::pageUpKey ? step : -step, "Distance");
        return true;
    }

    const auto character = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if (character == 'n')
    {
        // The next (or previous) snap point along the handle's direction.
        endNudgeGroup();

        if (acoustic)
        {
            const double along = MicUi::get (p, xId);
            double target = along;
            const auto& marks = MicUi::landmarks();

            if (mods.isShiftDown())
            {
                for (auto it = marks.rbegin(); it != marks.rend(); ++it)
                    if (it->along < along - 1.0e-4) { target = it->along; break; }
            }
            else
            {
                for (const auto& l : marks)
                    if (l.along > along + 1.0e-4) { target = l.along; break; }
            }

            MicEdit edit (p, "Snap Mic " + juce::String (mic + 1), { xId });
            edit.set (xId, (float) target);
        }
        else
        {
            const double x = MicUi::get (p, ids.x), y = MicUi::get (p, ids.y);
            const double u = std::hypot (x, y);
            double target = u;
            const auto& rings = MicUi::rings();

            if (mods.isShiftDown())
            {
                for (auto it = rings.rbegin(); it != rings.rend(); ++it)
                    if (it->u < u - 1.0e-4) { target = it->u; break; }
            }
            else
            {
                for (const auto& r : rings)
                    if (r.u > u + 1.0e-4) { target = r.u; break; }
            }

            const double dirX = u > 1.0e-6 ? x / u : 1.0, dirY = u > 1.0e-6 ? y / u : 0.0;
            MicEdit edit (p, "Snap Mic " + juce::String (mic + 1), { ids.x, ids.y });
            edit.set (ids.x, (float) (dirX * target));
            edit.set (ids.y, (float) (dirY * target));
        }

        face.refresh();
        return true;
    }

    if (character == 'r' && ! acoustic)
    {
        endNudgeGroup();
        MicEdit edit (p, "Toggle Mic " + juce::String (mic + 1) + " Rear", { ids.rear });
        edit.set (ids.rear, MicUi::get (p, ids.rear) > 0.5f ? 0.0f : 1.0f);
        face.refresh();
        return true;
    }

    if (code == juce::KeyPress::homeKey)
    {
        // Reset this mic to section 7's defaults: one multi-target entry.
        endNudgeGroup();
        juce::StringArray resetIds;

        if (acoustic) resetIds = { xId, yId, dId, aId };
        else          resetIds = { ids.x, ids.y, ids.dist, ids.angle, ids.speaker, ids.rear };

        MicEdit edit (p, "Reset Mic " + juce::String (mic + 1), resetIds);

        for (const auto& id : resetIds)
            if (auto* prm = p.getState().getParameter (id))
                prm->setValueNotifyingHost (prm->getDefaultValue());

        face.refresh();
        return true;
    }

    if (code == juce::KeyPress::returnKey)
    {
        if (face.onOpenEditor)
            face.onOpenEditor();

        return true;
    }

    return false;
}

//==============================================================================
MicFace::MicFace (LuthierAudioProcessor& p, bool full)
    : processor (p), fullCabinet (full)
{
    setOpaque (false);

    for (int m = 0; m < 2; ++m)
    {
        handles[(size_t) m] = std::make_unique<MicHandle> (*this, m);
        addAndMakeVisible (*handles[(size_t) m]);
    }

    grilleVisible = processor.getUiState().micGrilleVisible;
    focusedMic = juce::jlimit (0, 1, processor.getUiState().micFocusedHandle);

    refresh();
    startTimerHz (30);
}

MicFace::~MicFace()
{
    stopTimer();
}

void MicFace::setFocusedMic (int m)
{
    focusedMic = juce::jlimit (0, 1, m);
    processor.getUiState().micFocusedHandle = focusedMic;
    refresh();
}

void MicFace::resized()
{
    refresh();
}

juce::Rectangle<float> MicFace::speakerRect (int speaker) const
{
    const auto& geom = cabGeometry (cabinet);
    const int cols = juce::jmax (1, geom.columns);
    const int rows = juce::jmax (1, (geom.numSpeakers + cols - 1) / cols);
    auto area = getLocalBounds().toFloat().reduced (8.0f);
    const float w = area.getWidth() / (float) cols, h = area.getHeight() / (float) rows;
    const int s = juce::jlimit (1, geom.numSpeakers, speaker) - 1;
    return { area.getX() + w * (float) (s % cols), area.getY() + h * (float) (s / cols), w, h };
}

juce::Point<float> MicFace::coneCentre (int mic) const
{
    if (! fullCabinet)
        return getLocalBounds().toFloat().getCentre();

    const auto p = MicUi::placementOf (processor, mic);
    return speakerRect (resolveSpeaker (cabinet, p.speaker)).getCentre();
}

float MicFace::coneRadius (int mic) const
{
    // u = 1.4 (the baffle's edge of the drawing) fits the space.
    const float sizeScale = (float) (coneRadiusMm (cabGeometry (cabinet).sizeInches) / 110.0);

    if (! fullCabinet)
        return juce::jmin (getWidth(), getHeight()) * 0.5f / 1.42f * juce::jmin (1.0f, sizeScale);

    const auto p = MicUi::placementOf (processor, mic);
    const auto r = speakerRect (resolveSpeaker (cabinet, p.speaker));
    return juce::jmin (r.getWidth(), r.getHeight()) * 0.5f / 1.42f * juce::jmin (1.0f, sizeScale);
}

float MicFace::ringRadiusPx (double u) const
{
    return coneRadius (focusedMic) * (float) u;
}

juce::Rectangle<float> MicFace::bodyArea() const
{
    return getLocalBounds().toFloat().reduced (6.0f, 16.0f);
}

juce::Point<float> MicFace::bodyPoint (double alongMm, double acrossMm) const
{
    // The body from above, tail on the left, the neck to the right; the
    // treble side down (guitar-illustration.md 1's +Y).
    const auto area = bodyArea();
    const double x0 = landmarks.alongMm[0] - 10.0, x1 = landmarks.alongMm[4] + 30.0;
    const double halfW = landmarks.lowerBoutHalfWidthMm * 1.05;
    const double scale = juce::jmin (area.getWidth() / (x1 - x0), area.getHeight() / (2.0 * halfW));
    const double cx = area.getCentreX() - (x0 + x1) * 0.5 * scale;
    return { (float) (cx + alongMm * scale), (float) (area.getCentreY() + acrossMm * scale) };
}

juce::Point<float> MicFace::handleCentre (int mic) const
{
    if (acoustic)
    {
        const auto a = MicUi::acousticPlacementOf (processor, mic);
        return bodyPoint (landmarks.alongToMm (a.along), landmarks.acrossToMm (a.across));
    }

    const auto p = MicUi::placementOf (processor, mic);
    const auto c = coneCentre (mic);
    const float r = coneRadius (mic);
    return { c.x + (float) p.x * r, c.y - (float) p.y * r };
}

juce::Point<double> MicFace::placementAt (juce::Point<float> pixel, int mic) const
{
    if (acoustic)
    {
        // Invert bodyPoint, then mm back to landmark units.
        const auto origin = bodyPoint (0.0, 0.0);
        const auto unit = bodyPoint (1.0, 1.0);
        const double scale = juce::jmax (1.0e-6, (double) (unit.x - origin.x));
        const double alongMm = (pixel.x - origin.x) / scale;
        const double acrossMm = (pixel.y - origin.y) / scale;

        double along = 0.0;

        if (alongMm <= landmarks.alongMm[0]) along = 0.0;
        else if (alongMm >= landmarks.alongMm[4]) along = 4.0;
        else
            for (int i = 0; i < 4; ++i)
                if (alongMm <= landmarks.alongMm[i + 1])
                {
                    along = i + (alongMm - landmarks.alongMm[i]) / (landmarks.alongMm[i + 1] - landmarks.alongMm[i]);
                    break;
                }

        return { juce::jlimit (0.0, 4.0, along),
                 juce::jlimit (-1.0, 1.0, acrossMm / juce::jmax (1.0, landmarks.lowerBoutHalfWidthMm)) };
    }

    const auto c = coneCentre (mic);
    const double r = juce::jmax (1.0f, coneRadius (mic));
    return { juce::jlimit (-1.4, 1.4, (pixel.x - c.x) / r), juce::jlimit (-1.4, 1.4, (c.y - pixel.y) / r) };
}

juce::Point<double> MicFace::snapped (juce::Point<double> xy, bool& didSnap) const
{
    didSnap = false;
    const double u = std::hypot (xy.x, xy.y);
    const double r = juce::jmax (1.0f, coneRadius (focusedMic));

    for (const auto& ring : MicUi::rings())
    {
        if (std::abs ((u - ring.u) * r) <= 6.0)
        {
            didSnap = true;

            if (ring.u == 0.0)
                return { 0.0, 0.0 };

            const double k = u > 1.0e-9 ? ring.u / u : 0.0;
            return u > 1.0e-9 ? juce::Point<double> (xy.x * k, xy.y * k) : juce::Point<double> (ring.u, 0.0);
        }
    }

    return xy;
}

void MicFace::refresh()
{
    const bool nowAcoustic = MicUi::isAcoustic (processor);

    if (nowAcoustic != acoustic)
    {
        acoustic = nowAcoustic;

        for (auto& h : handles)
            h->rebind();
    }

    cabinet = MicUi::cabinetOf (processor);

    if (acoustic)
        landmarks = processor.getEngine().getAcousticMicModel().getLandmarks();

    const bool dual = MicUi::dualMicOn (processor);

    for (int m = 0; m < 2; ++m)
    {
        auto& h = *handles[(size_t) m];
        bool show = (m == 0 || dual);

        // Placement is baked into a user IR (mic-placement.md 9): no handle.
        if (! acoustic && processor.getCabIrSlot (m).isEngaged())
            show = false;

        // The compact face shows one speaker: the other mic only if it is on it.
        if (show && ! acoustic && ! fullCabinet && m != focusedMic)
            show = resolveSpeaker (cabinet, MicUi::placementOf (processor, m).speaker)
                   == resolveSpeaker (cabinet, MicUi::placementOf (processor, focusedMic).speaker);

        const auto c = handleCentre (m);
        h.setBounds (juce::Rectangle<int> (MicHandle::kSize, MicHandle::kSize).withCentre (c.roundToInt()));
        h.setVisible (show && getWidth() > 0);
        h.setTooltip (MicUi::describe (processor, m));
        h.repaint();
    }

    repaint();
}

void MicFace::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onOpenEditor)
        onOpenEditor();
}

void MicFace::drawCone (juce::Graphics& g, juce::Point<float> c, float r, bool miked) const
{
    // Baffle and surround roll, frame and bolts.
    g.setColour (Palette::panelSunken);
    g.fillEllipse (juce::Rectangle<float> (r * 2.8f, r * 2.8f).withCentre (c));
    g.setColour (Palette::edge);
    g.drawEllipse (juce::Rectangle<float> (r * 2.24f, r * 2.24f).withCentre (c), 2.0f);

    for (int b = 0; b < 8; ++b)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) b / 8.0f;
        g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (c.translated (std::cos (a) * r * 1.3f, std::sin (a) * r * 1.3f)));
    }

    g.setColour (Palette::panel.darker (0.2f));
    g.fillEllipse (juce::Rectangle<float> (r * 2.24f, r * 2.24f).withCentre (c));

    // Paper: a radial gradient with concentric ribs.
    juce::ColourGradient paper (Palette::panelRaised.brighter (0.25f), c.x, c.y,
                                Palette::panelSunken, c.x + r, c.y, true);
    g.setGradientFill (paper);
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));

    g.setColour (Palette::edge.withAlpha (0.5f));
    for (float f = 0.45f; f < 1.0f; f += 0.09f)
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f * f, r * 2.0f * f).withCentre (c), 0.6f);

    // The dust cap, with a highlight.
    const float cap = r * 0.35f;
    juce::ColourGradient dome (Palette::textMuted.withAlpha (0.55f), c.x - cap * 0.4f, c.y - cap * 0.4f,
                               Palette::panelSunken, c.x + cap, c.y + cap, true);
    g.setGradientFill (dome);
    g.fillEllipse (juce::Rectangle<float> (cap * 2.0f, cap * 2.0f).withCentre (c));

    if (! miked)
        return;

    // The snap rings, labelled.
    g.setFont (Fonts::ui (9.0f));

    for (const auto& ring : MicUi::rings())
    {
        if (ring.u <= 0.0)
            continue;

        g.setColour (Palette::accentDim.withAlpha (0.7f));
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f * (float) ring.u, r * 2.0f * (float) ring.u).withCentre (c), 0.8f);
        g.setColour (Palette::textMuted);
        g.drawText (MicUi::text (ring.key), juce::Rectangle<float> (60.0f, 10.0f).withCentre (c.translated (0.0f, -r * (float) ring.u - 5.0f)),
                    juce::Justification::centred);
    }
}

void MicFace::drawBody (juce::Graphics& g) const
{
    const auto* style = outlines::findBodyStyle (landmarks.styleId);

    if (style == nullptr)
        return;

    juce::Path body;

    for (int i = 0; i < style->numOutline; ++i)
    {
        const auto& pt = style->outline[i];
        const auto at = bodyPoint ((style->saddleU - pt.u) * style->lengthMm, pt.v * style->widthMm);
        if (i == 0) body.startNewSubPath (at); else body.lineTo (at);
    }

    body.closeSubPath();
    body = body.createPathWithRoundedCorners (6.0f);

    g.setColour (Palette::panelRaised.brighter (0.15f));
    g.fillPath (body);
    g.setColour (Palette::edgeBright);
    g.strokePath (body, juce::PathStrokeType (1.4f));

    // The neck to the 12th fret.
    const auto neckA = bodyPoint (style->lengthMm * (style->saddleU - style->neckU), -22.0);
    const auto neckB = bodyPoint (landmarks.alongMm[4] + 25.0, 22.0);
    g.setColour (Palette::panel.brighter (0.1f));
    g.fillRect (juce::Rectangle<float> (neckA, neckB));

    // The soundhole, where there is one.
    if (landmarks.hasSoundhole)
    {
        const auto hole = bodyPoint (landmarks.alongMm[3], 0.0);
        const auto rim = bodyPoint (landmarks.alongMm[3] + 45.0, 0.0);
        const float rr = std::abs (rim.x - hole.x);
        g.setColour (Palette::backgroundDeep);
        g.fillEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (hole));
    }

    // Landmark ticks.
    g.setFont (Fonts::ui (9.0f));

    // Ticks along the treble edge; labels alternate above and below the
    // body so the close ones (soundhole, upper bout, 12th fret) never collide.
    int index = 0;

    for (const auto& l : MicUi::landmarks())
    {
        const bool below = (index++ % 2) == 0;
        const double across = landmarks.lowerBoutHalfWidthMm * (below ? 1.0 : -1.0);
        const auto at = bodyPoint (landmarks.alongToMm (l.along), across);
        g.setColour (Palette::accentDim);
        g.drawVerticalLine ((int) at.x, at.y - 3.0f, at.y + 3.0f);
        g.setColour (Palette::textMuted);
        g.drawText (l.label, juce::Rectangle<float> (64.0f, 10.0f).withCentre (at.translated (0.0f, below ? 7.0f : -7.0f)),
                    juce::Justification::centred);
    }
}

void MicFace::paint (juce::Graphics& g)
{
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), Metrics::panelCorner);

    if (acoustic)
    {
        drawBody (g);
        return;
    }

    if (fullCabinet)
    {
        // Tolex and piping, every speaker, the grille over them.
        g.setColour (Palette::backgroundDeep);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
        g.setColour (Palette::edgeBright);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 6.0f, 1.5f);

        const auto& geom = cabGeometry (cabinet);
        const float sizeScale = (float) (coneRadiusMm (geom.sizeInches) / 110.0);
        const int miked1 = resolveSpeaker (cabinet, MicUi::placementOf (processor, 0).speaker);
        const int miked2 = MicUi::dualMicOn (processor) ? resolveSpeaker (cabinet, MicUi::placementOf (processor, 1).speaker) : -1;

        for (int s = 1; s <= geom.numSpeakers; ++s)
        {
            const auto rect = speakerRect (s);
            const float r = juce::jmin (rect.getWidth(), rect.getHeight()) * 0.5f / 1.42f * juce::jmin (1.0f, sizeScale);
            drawCone (g, rect.getCentre(), r, s == miked1 || s == miked2);

            if (s == miked1 || s == miked2)
            {
                g.setColour (MicUi::colourFor (s == miked1 ? 0 : 1).withAlpha (0.6f));
                g.drawRoundedRectangle (rect.reduced (2.0f), 4.0f, 1.2f);
            }
        }

        // A basket-weave grille, per cabinet colour; see-through when off.
        const auto grilleColour = cabinet == CabinetType::Cab4x12Vintage ? juce::Colour (0xffb8b0a0)
                                : cabinet == CabinetType::Cab2x12Open    ? juce::Colour (0xff5a1e1e)
                                                                         : juce::Colour (0xff101010);
        g.setColour (grilleColour.withAlpha (grilleVisible ? 0.55f : 0.25f));

        const auto area = getLocalBounds().toFloat().reduced (8.0f);

        for (float y = area.getY(); y < area.getBottom(); y += 4.0f)
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());

        for (float x = area.getX(); x < area.getRight(); x += 4.0f)
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());

        return;
    }

    drawCone (g, coneCentre (focusedMic), coneRadius (focusedMic), true);
}

//==============================================================================
SpeakerThumb::SpeakerThumb (LuthierAudioProcessor& p, MicFace& f)
    : processor (p), face (f)
{
    setTooltip (MicUi::text ("mic.thumb.tooltip"));
}

juce::Rectangle<float> SpeakerThumb::cell (int speaker) const
{
    const auto& geom = cabGeometry (MicUi::cabinetOf (processor));
    const int cols = juce::jmax (1, geom.columns);
    const int rows = juce::jmax (1, (geom.numSpeakers + cols - 1) / cols);
    auto area = getLocalBounds().toFloat().reduced (3.0f);
    const float w = area.getWidth() / (float) cols, h = area.getHeight() / (float) rows;
    const int s = juce::jlimit (1, geom.numSpeakers, speaker) - 1;
    return { area.getX() + w * (float) (s % cols), area.getY() + h * (float) (s / cols), w, h };
}

int SpeakerThumb::speakerAt (juce::Point<float> at) const
{
    const auto& geom = cabGeometry (MicUi::cabinetOf (processor));

    for (int s = 1; s <= geom.numSpeakers; ++s)
        if (cell (s).contains (at))
            return s;

    return 0;
}

void SpeakerThumb::moveFocusedMicTo (int speaker)
{
    if (speaker <= 0)
        return;

    const int mic = face.getFocusedMic();
    const auto& ids = MicPlacementMigration::idsFor (mic);
    MicEdit edit (processor, "Move Mic " + juce::String (mic + 1) + " to speaker " + juce::String (speaker), { ids.speaker });
    edit.set (ids.speaker, (float) speaker);
    face.refresh();
    repaint();
}

void SpeakerThumb::mouseDown (const juce::MouseEvent& e)
{
    moveFocusedMicTo (speakerAt (e.position));
}

void SpeakerThumb::paint (juce::Graphics& g)
{
    g.setColour (Palette::backgroundDeep);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

    const auto cab = MicUi::cabinetOf (processor);
    const auto& geom = cabGeometry (cab);
    const int s1 = resolveSpeaker (cab, MicUi::placementOf (processor, 0).speaker);
    const int s2 = MicUi::dualMicOn (processor) ? resolveSpeaker (cab, MicUi::placementOf (processor, 1).speaker) : -1;

    for (int s = 1; s <= geom.numSpeakers; ++s)
    {
        const auto c = cell (s);
        const float r = juce::jmin (c.getWidth(), c.getHeight()) * 0.4f;
        g.setColour (Palette::panel.brighter (0.2f));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c.getCentre()));

        for (int m = 0; m < 2; ++m)
        {
            if ((m == 0 ? s1 : s2) != s)
                continue;

            g.setColour (MicUi::colourFor (m));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c.getCentre()).reduced ((float) m * 2.0f), 1.6f);
            g.setFont (Fonts::ui (9.0f, true));
            g.drawText (juce::String (m + 1), c.withTrimmedLeft ((float) m * c.getWidth() * 0.5f), juce::Justification::centred);
        }
    }
}

//==============================================================================
MicResponsePlot::MicResponsePlot (LuthierAudioProcessor& p, bool d)
    : processor (p), detailed (d)
{
    setOpaque (false);
    update();
    startTimerHz (30);
}

MicResponsePlot::~MicResponsePlot()
{
    stopTimer();
}

double MicResponsePlot::frequencyAt (int index) noexcept
{
    return 20.0 * std::pow (1000.0, (double) index / (double) (kPoints - 1));
}

MicResponsePlot::Curves MicResponsePlot::compute (LuthierAudioProcessor& p, double sr)
{
    Curves c;
    const bool acoustic = MicUi::isAcoustic (p);
    c.hasMic2 = MicUi::dualMicOn (p);

    const bool physical = MicUi::get (p, ParamIDs::micTofMode) > 0.5f;
    const double blend = acoustic ? MicUi::get (p, ParamIDs::acMicBlend) : MicUi::get (p, ParamIDs::micBlend);
    const double w1 = std::cos (blend * constants::kPi * 0.5), w2 = std::sin (blend * constants::kPi * 0.5);

    std::complex<double> h1[kPoints], h2[kPoints];
    double path[2] = { 0.0, 0.0 };

    for (int m = 0; m < (c.hasMic2 ? 2 : 1); ++m)
    {
        auto* h = m == 0 ? h1 : h2;

        if (acoustic)
        {
            auto& model = p.getEngine().getAcousticMicModel();
            AcousticMicModel::Input in;
            in.landmarks = model.getLandmarks();
            in.airHz = p.getEngine().getBodyEngine().getAirResonanceHz();
            in.mic = MicUi::micTypeOf (p, m);
            in.placement = MicUi::acousticPlacementOf (p, m);
            in.levelMatch = MicUi::get (p, ParamIDs::micLevelMatch) > 0.5f;
            in.calibration = model.getCalibration();
            const auto t = AcousticMicModel::evaluate (in);
            path[m] = t.pathM;

            for (int i = 0; i < kPoints; ++i)
                h[i] = AcousticMicModel::response (t, sr, frequencyAt (i));
        }
        else
        {
            const auto t = MicPlacementModel::evaluate (MicUi::inputFor (p, m));
            path[m] = t.pathM;

            for (int i = 0; i < kPoints; ++i)
                h[i] = PlacementResponse::response (t, sr, frequencyAt (i));
        }
    }

    for (int i = 0; i < kPoints; ++i)
    {
        c.mic1[(size_t) i] = (float) gainToDb (std::abs (h1[i]));

        if (c.hasMic2)
            c.mic2[(size_t) i] = (float) gainToDb (std::abs (h2[i]));
    }

    // The sum as the blend hears it: with Physical ToF the farther mic lags,
    // and the comb shows (6.2).
    if (c.hasMic2)
    {
        c.showSum = true;
        const double lag = physical ? (path[1] - path[0]) / MicPlacementModel::kSpeedOfSound : 0.0;
        double previous = 1.0e9;
        bool falling = false;

        for (int i = 0; i < kPoints; ++i)
        {
            const double f = frequencyAt (i);
            const auto d1 = std::polar (1.0, -constants::kTwoPi * f * juce::jmax (0.0, -lag));
            const auto d2 = std::polar (1.0, -constants::kTwoPi * f * juce::jmax (0.0, lag));
            const double db = gainToDb (std::abs (w1 * h1[i] * d1 + w2 * h2[i] * d2));
            c.sum[(size_t) i] = (float) db;

            if (physical && c.firstNotchHz == 0.0 && f > 60.0)
            {
                if (db < previous) falling = true;
                else if (falling && previous < db - 0.01) c.firstNotchHz = frequencyAt (i - 1);
            }

            previous = db;
        }
    }

    return c;
}

void MicResponsePlot::update()
{
    const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    curves = compute (processor, sr);
    repaint();
}

void MicResponsePlot::timerCallback()
{
    // Under reduced motion the plot waits for the release (8).
    if (AccessibilitySettings::get().isReducedMotion() && MicUi::activeDrags() > 0)
        return;

    // Only when something it draws moved.
    juce::uint32 signature = 0;

    for (const char* id : { ParamIDs::micX, ParamIDs::micY, ParamIDs::micDist, ParamIDs::micAngle, ParamIDs::micSpeaker,
                            ParamIDs::micRear, ParamIDs::micX2, ParamIDs::micY2, ParamIDs::micDist2, ParamIDs::micAngle2,
                            ParamIDs::micSpeaker2, ParamIDs::micRear2, ParamIDs::micTofMode, ParamIDs::micLevelMatch,
                            ParamIDs::acMicAlong, ParamIDs::acMicAcross, ParamIDs::acMicDist, ParamIDs::acMicAngle,
                            ParamIDs::acMic2On, ParamIDs::acMicAlong2, ParamIDs::acMicAcross2, ParamIDs::acMicDist2,
                            ParamIDs::acMicAngle2, ParamIDs::acMicBlend, ParamIDs::cabType, ParamIDs::cabSpeaker,
                            ParamIDs::micType, ParamIDs::micType2, ParamIDs::dualMic, ParamIDs::micBlend, ParamIDs::guitarType })
    {
        const float v = MicUi::get (processor, id);
        juce::uint32 bits;
        std::memcpy (&bits, &v, sizeof (bits));
        signature = signature * 16777619u ^ bits;
    }

    if (signature != lastSignature)
    {
        lastSignature = signature;
        update();
    }
}

void MicResponsePlot::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (area, 3.0f);

    const float top = detailed ? 12.0f : 6.0f, bottom = detailed ? -24.0f : -12.0f;
    const auto yOf = [&] (float db) { return juce::jmap (juce::jlimit (bottom, top, db), top, bottom, area.getY() + 2.0f, area.getBottom() - 2.0f); };
    const auto xOf = [&] (int i) { return juce::jmap ((float) i, 0.0f, (float) (kPoints - 1), area.getX(), area.getRight()); };

    g.setColour (Palette::edge);
    g.drawHorizontalLine ((int) yOf (0.0f), area.getX(), area.getRight());

    const auto trace = [&] (const std::array<float, kPoints>& curve, juce::Colour colour, float thickness)
    {
        juce::Path path;

        for (int i = 0; i < kPoints; ++i)
        {
            const juce::Point<float> at (xOf (i), yOf (curve[(size_t) i]));
            if (i == 0) path.startNewSubPath (at); else path.lineTo (at);
        }

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness));
    };

    if (hasGhost)
        trace (ghost.mic1, Palette::textDisabled, 1.0f);

    if (curves.showSum && detailed)
        trace (curves.sum, Palette::textPrimary, 1.2f);

    if (curves.hasMic2)
        trace (curves.mic2, MicUi::colourFor (1), 1.2f);

    trace (curves.mic1, MicUi::colourFor (0), 1.4f);

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (9.0f));
    g.drawText (MicUi::text (MicUi::isAcoustic (processor) ? "mic.plot.acoustic" : "mic.plot.title"),
                area.reduced (4.0f, 1.0f), juce::Justification::topLeft);

    if (detailed && curves.firstNotchHz > 0.0)
        g.drawText (Localisation::get().translate ("mic.plot.notch", { { "hz", curves.firstNotchHz >= 1000.0 ? juce::String (curves.firstNotchHz / 1000.0, 1) + " kHz"
                                                                                                        : juce::String (juce::roundToInt (curves.firstNotchHz)) + " Hz" } }),
                    area.reduced (4.0f, 1.0f), juce::Justification::topRight);
}

//==============================================================================
struct MicPlacementView::QuickMapper : juce::AudioProcessorParameter::Listener
{
    QuickMapper (LuthierAudioProcessor& p, int m, bool isPosition, juce::RangedAudioParameter& prm)
        : processor (p), mic (m), position (isPosition), parameter (prm) {}

    void parameterGestureChanged (int, bool starting) override { inGesture = starting; }

    void parameterValueChanged (int, float newValue) override
    {
        if (! inGesture || busy || ! juce::MessageManager::existsAndIsCurrentThread())
            return;

        // Listeners run newest first, so the value tree's copy of this
        // parameter is not updated yet: the new choice is the one passed in.
        const juce::ScopedValueSetter<bool> guard (busy, true);
        const auto& ids = MicPlacementMigration::idsFor (mic);
        const int picked = juce::roundToInt (parameter.convertFrom0to1 (newValue));
        MicPlacementMigration::writeMapped (processor.getState(), mic,
                                            position ? picked : juce::roundToInt (MicUi::get (processor, ids.position)),
                                            position ? juce::roundToInt (MicUi::get (processor, ids.distance)) : picked);
    }

    LuthierAudioProcessor& processor;
    int mic;
    bool position;
    juce::RangedAudioParameter& parameter;
    bool inGesture = false, busy = false;
};

//==============================================================================
struct MicPlacementView::AutomationWatch : juce::AudioProcessorParameter::Listener
{
    explicit AutomationWatch (LuthierAudioProcessor& p) : processor (p)
    {
        for (int mic = 0; mic < 2; ++mic)
        {
            const auto& ids = MicPlacementMigration::idsFor (mic);

            for (const char* id : { ids.x, ids.y, ids.dist, ids.angle, ids.speaker, ids.rear })
                if (auto* prm = processor.getState().getParameter (id))
                {
                    prm->addListener (this);
                    watched.push_back ({ prm, mic });
                }
        }

        for (auto& t : lastWrite)
            t = -1.0e9;
    }

    ~AutomationWatch() override
    {
        for (auto& [prm, mic] : watched)
            prm->removeListener (this);
    }

    int micOf (int index) const
    {
        for (const auto& [prm, mic] : watched)
            if (prm->getParameterIndex() == index)
                return mic;

        return -1;
    }

    // Any thread: a host writes its lanes from the audio thread.
    void parameterValueChanged (int index, float) override
    {
        const int mic = micOf (index);

        if (mic >= 0 && gestures[(size_t) mic].load() == 0)
            lastWrite[(size_t) mic] = juce::Time::getMillisecondCounterHiRes();
    }

    void parameterGestureChanged (int index, bool starting) override
    {
        const int mic = micOf (index);

        if (mic >= 0)
            gestures[(size_t) mic] += starting ? 1 : -1;
    }

    LuthierAudioProcessor& processor;
    std::vector<std::pair<juce::AudioProcessorParameter*, int>> watched;
    std::array<std::atomic<double>, 2> lastWrite {};
    std::array<std::atomic<int>, 2> gestures {};
};

bool MicPlacementView::isDrivenByAutomation (int mic) const noexcept
{
    return automationWatch != nullptr
        && juce::Time::getMillisecondCounterHiRes() - automationWatch->lastWrite[(size_t) juce::jlimit (0, 1, mic)].load() < 500.0;
}

//==============================================================================
MicPlacementView::MicPlacementView (LuthierAudioProcessor& p)
    : processor (p), face (p, false)
{
    addAndMakeVisible (face);
    addAndMakeVisible (thumb);
    addAndMakeVisible (plot);

    expandButton.setTooltip (MicUi::text ("mic.expand.tooltip"));
    expandButton.setTitle ("Expand mic placement");
    expandButton.onClick = [this] { if (onOpenEditor) onOpenEditor(); };
    addAndMakeVisible (expandButton);

    face.onOpenEditor = [this] { if (onOpenEditor) onOpenEditor(); };
    face.onPlacementChanged = [this] { plot.update(); thumb.repaint(); };

    dist1.attachTo (p, ParamIDs::micDist, "Mic 1 distance from the grille, in centimetres");
    angle1.attachTo (p, ParamIDs::micAngle, "Mic 1 angle off-axis, in degrees");
    dist2.attachTo (p, ParamIDs::micDist2, "Mic 2 distance from the grille, in centimetres");
    angle2.attachTo (p, ParamIDs::micAngle2, "Mic 2 angle off-axis, in degrees");
    rear1.attachTo (p, ParamIDs::micRear, "Mic 1 behind the cabinet");
    rear2.attachTo (p, ParamIDs::micRear2, "Mic 2 behind the cabinet");
    x1.attachTo (p, ParamIDs::micX, "Mic 1 across the cone: 0 the cap centre, 0.35 the cap edge, 1 the cone edge");
    y1.attachTo (p, ParamIDs::micY, "Mic 1 up the cone");
    speaker1.attachTo (p, ParamIDs::micSpeaker, "Which speaker mic 1 is on, numbered left to right, top to bottom");
    x2.attachTo (p, ParamIDs::micX2, "Mic 2 across the cone");
    y2.attachTo (p, ParamIDs::micY2, "Mic 2 up the cone");
    speaker2.attachTo (p, ParamIDs::micSpeaker2, "Which speaker mic 2 is on");

    quickPos1.attachTo (p, ParamIDs::micPosition, "A classic placement for mic 1; it sets the continuous placement");
    quickDist1.attachTo (p, ParamIDs::micDistance, "A classic distance for mic 1");
    quickPos2.attachTo (p, ParamIDs::micPosition2, "A classic placement for mic 2");
    quickDist2.attachTo (p, ParamIDs::micDistance2, "A classic distance for mic 2");

    wireQuick (quickPos1, 0, true);
    wireQuick (quickDist1, 0, false);
    wireQuick (quickPos2, 1, true);
    wireQuick (quickDist2, 1, false);

    tofMode.attachTo (p, ParamIDs::micTofMode, "Physical: the farther mic arrives later, as on a real session. "
                                               "Aligned: both arrive together.");
    levelMatch.attachTo (p, ParamIDs::micLevelMatch, "Sets each mic's gain the way an engineer would, so moving "
                                                     "a mic changes its tone, not its level");

    acMix.attachTo (p, ParamIDs::acMicMix, "Pickup against external microphones. 0 is the pickup alone.");
    acBlend.attachTo (p, ParamIDs::acMicBlend, "Mic 1 against mic 2");
    acMic2.attachTo (p, ParamIDs::acMic2On, "A second external microphone");
    acDist1.attachTo (p, ParamIDs::acMicDist, "Mic 1 distance from the guitar, in centimetres");
    acAngle1.attachTo (p, ParamIDs::acMicAngle, "Mic 1 angle off-axis, in degrees");
    acDist2.attachTo (p, ParamIDs::acMicDist2, "Mic 2 distance from the guitar, in centimetres");
    acAngle2.attachTo (p, ParamIDs::acMicAngle2, "Mic 2 angle off-axis, in degrees");
    acAlong1.attachTo (p, ParamIDs::acMicAlong, "Mic 1 along the guitar: 0 the tail, 2 the bridge, 3 the soundhole, 4 the 12th fret");
    acAcross1.attachTo (p, ParamIDs::acMicAcross, "Mic 1 across the guitar: + the treble side");
    acAlong2.attachTo (p, ParamIDs::acMicAlong2, "Mic 2 along the guitar");
    acAcross2.attachTo (p, ParamIDs::acMicAcross2, "Mic 2 across the guitar");

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &dist1, &angle1, &dist2, &angle2, &rear1, &rear2, &x1, &y1, &speaker1, &x2, &y2, &speaker2,
             &quickPos1, &quickDist1, &quickPos2, &quickDist2, &tofMode, &levelMatch,
             &acMix, &acBlend, &acMic2, &acDist1, &acAngle1, &acDist2, &acAngle2, &acAlong1, &acAcross1, &acAlong2, &acAcross2 })
        addChildComponent (c);

    automationWatch = std::make_unique<AutomationWatch> (p);

    applyFamily();
    startTimerHz (10);
}

MicPlacementView::~MicPlacementView()
{
    stopTimer();

    for (auto& [prm, mapper] : quickMappers)
        prm->removeListener (mapper.get());
}

void MicPlacementView::wireQuick (LuthierChoice& box, int mic, bool position)
{
    /*  A Quick pick is the legacy parameter's canonical control (6.1): it
        writes the discrete choice and the mapped continuous values in one
        gesture, so the attachment's single undo entry covers both. The
        mapping is written from the parameter's own change inside the
        attachment's gesture - a preset load moves the box too, and must not
        remap anything. */
    juce::ignoreUnused (box);
    const auto& ids = MicPlacementMigration::idsFor (mic);

    if (auto* prm = processor.getState().getParameter (position ? ids.position : ids.distance))
    {
        auto mapper = std::make_unique<QuickMapper> (processor, mic, position, *prm);
        prm->addListener (mapper.get());
        quickMappers.push_back ({ prm, std::move (mapper) });
    }
}

juce::String MicPlacementView::getSectionTitle() const
{
    return shownAcoustic ? MicUi::text ("mic.section.acoustic") : MicUi::text ("mic.section.cabinet");
}

void MicPlacementView::applyFamily()
{
    updateFamilyVisibility();
    face.refresh();
    resized();

    if (onFamilyChanged)
        onFamilyChanged();
}

void MicPlacementView::updateFamilyVisibility()
{
    shownAcoustic = MicUi::isAcoustic (processor);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &dist1, &angle1, &dist2, &angle2, &rear1, &rear2, &x1, &y1, &speaker1, &x2, &y2, &speaker2,
             &quickPos1, &quickDist1, &quickPos2, &quickDist2 })
        c->setVisible (! shownAcoustic);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &acMix, &acBlend, &acMic2, &acDist1, &acAngle1, &acDist2, &acAngle2, &acAlong1, &acAcross1, &acAlong2, &acAcross2 })
        c->setVisible (shownAcoustic);

    thumb.setVisible (! shownAcoustic);
    tofMode.setVisible (true);
    levelMatch.setVisible (true);
    plot.setVisible (UiPreferences::get().getBool ("mic.showResponsePlot", true));
}

void MicPlacementView::timerCallback()
{
    if (MicUi::isAcoustic (processor) != shownAcoustic)
        applyFamily();

    auto messages = MicUi::statusMessages (processor);

    if (! shownAcoustic && (isDrivenByAutomation (0) || (MicUi::dualMicOn (processor) && isDrivenByAutomation (1))))
        messages.add (MicUi::text ("mic.chip.automation"));

    const auto text = messages.joinIntoString ("  ");

    if (text != status)
    {
        status = text;
        repaint();
    }

    const bool plotWanted = UiPreferences::get().getBool ("mic.showResponsePlot", true);

    if (plot.isVisible() != plotWanted)
    {
        plot.setVisible (plotWanted);
        resized();
    }
}

int MicPlacementView::getPreferredHeight() const
{
    const int knobRow = LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small);
    const int faceH = 150;

    const int electric = kHeaderRow + faceH + 2 * (knobRow + 3 * kSliderRow) + 2 * kChoiceRow + kPlotRow + kChoiceRow + 8 * Metrics::gridHalf;
    const int acoustic = kHeaderRow + faceH + 3 * knobRow + 4 * kSliderRow + kPlotRow + kChoiceRow + 8 * Metrics::gridHalf;
    return juce::jmax (electric, acoustic);
}

void MicPlacementView::resized()
{
    // A family switch caught by a layout pass before the timer's.
    if (MicUi::isAcoustic (processor) != shownAcoustic)
        updateFamilyVisibility();

    auto area = getLocalBounds();
    const int gap = Metrics::gridHalf;

    auto header = area.removeFromTop (kHeaderRow);
    expandButton.setBounds (header.removeFromRight (28).reduced (0, 2));

    auto faceRow = area.removeFromTop (150);

    if (! shownAcoustic)
    {
        auto thumbArea = faceRow.removeFromRight (juce::jmin (64, faceRow.getWidth() / 4));
        thumb.setBounds (thumbArea.removeFromTop (juce::jmin (64, thumbArea.getHeight())).reduced (2));
        faceRow.removeFromRight (gap);
    }

    face.setBounds (faceRow);
    area.removeFromTop (gap);

    const int knobRow = LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small);

    const auto threeAcross = [] (juce::Rectangle<int> row, juce::Component& a, juce::Component& b, juce::Component& c)
    {
        const int w = row.getWidth() / 3;
        a.setBounds (row.removeFromLeft (w));
        b.setBounds (row.removeFromLeft (w));
        c.setBounds (row.withSizeKeepingCentre (row.getWidth(), juce::jmin (row.getHeight(), Metrics::buttonHeight)));
    };

    if (! shownAcoustic)
    {
        threeAcross (area.removeFromTop (knobRow), dist1, angle1, rear1);
        x1.setBounds (area.removeFromTop (kSliderRow));
        y1.setBounds (area.removeFromTop (kSliderRow));
        speaker1.setBounds (area.removeFromTop (kSliderRow));
        area.removeFromTop (gap);

        threeAcross (area.removeFromTop (knobRow), dist2, angle2, rear2);
        x2.setBounds (area.removeFromTop (kSliderRow));
        y2.setBounds (area.removeFromTop (kSliderRow));
        speaker2.setBounds (area.removeFromTop (kSliderRow));
        area.removeFromTop (gap);

        auto q1 = area.removeFromTop (kChoiceRow);
        quickPos1.setBounds (q1.removeFromLeft (q1.getWidth() / 2));
        quickDist1.setBounds (q1);
        auto q2 = area.removeFromTop (kChoiceRow);
        quickPos2.setBounds (q2.removeFromLeft (q2.getWidth() / 2));
        quickDist2.setBounds (q2);
    }
    else
    {
        threeAcross (area.removeFromTop (knobRow), acMix, acBlend, acMic2);
        auto row1 = area.removeFromTop (knobRow);
        acDist1.setBounds (row1.removeFromLeft (row1.getWidth() / 2));
        acAngle1.setBounds (row1);
        acAlong1.setBounds (area.removeFromTop (kSliderRow));
        acAcross1.setBounds (area.removeFromTop (kSliderRow));
        area.removeFromTop (gap);

        auto row2 = area.removeFromTop (knobRow);
        acDist2.setBounds (row2.removeFromLeft (row2.getWidth() / 2));
        acAngle2.setBounds (row2);
        acAlong2.setBounds (area.removeFromTop (kSliderRow));
        acAcross2.setBounds (area.removeFromTop (kSliderRow));
    }

    area.removeFromTop (gap);

    if (plot.isVisible())
    {
        plot.setBounds (area.removeFromTop (kPlotRow));
        area.removeFromTop (gap);
    }

    auto bottom = area.removeFromTop (kChoiceRow);
    tofMode.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2));
    levelMatch.setBounds (bottom.withSizeKeepingCentre (bottom.getWidth(), Metrics::buttonHeight).reduced (2, 0));
}

void MicPlacementView::paint (juce::Graphics& g)
{
    auto header = getLocalBounds().removeFromTop (kHeaderRow).withTrimmedRight (32);

    if (status.isNotEmpty())
    {
        g.setColour (Palette::warning);
        g.setFont (Fonts::ui (10.0f));
        g.drawFittedText (status, header, juce::Justification::centredLeft, 2);
    }
    else
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (10.0f));
        g.drawFittedText (MicUi::describe (processor, face.getFocusedMic()), header, juce::Justification::centredLeft, 2);
    }
}

bool MicPlacementView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey && expandButton.hasKeyboardFocus (false))
    {
        if (onOpenEditor)
            onOpenEditor();

        return true;
    }

    return false;
}

} // namespace luthier
