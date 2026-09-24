#include "WorkshopPanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "RealismGroups.h"   // REALISM-C

namespace luthier
{

namespace
{
    int pickupIndexOfRegion (GuitarRegion r) noexcept
    {
        return r == GuitarRegion::pickupNeck ? 0 : r == GuitarRegion::pickupMiddle ? 1 : r == GuitarRegion::pickupBridge ? 2 : -1;
    }

    /** The slot a region on the illustration stands for (section 3). */
    GuitarSlot slotForRegion (GuitarRegion r) noexcept
    {
        switch (r)
        {
            case GuitarRegion::body:          return GuitarSlot::body;
            case GuitarRegion::pickguard:     return GuitarSlot::pickguard;
            case GuitarRegion::soundhole:     return GuitarSlot::body;
            case GuitarRegion::bridge:        return GuitarSlot::bridge;
            case GuitarRegion::tailpiece:     return GuitarSlot::tailpiece;
            case GuitarRegion::pickupNeck:    return GuitarSlot::pickupNeck;
            case GuitarRegion::pickupMiddle:  return GuitarSlot::pickupMiddle;
            case GuitarRegion::pickupBridge:  return GuitarSlot::pickupBridge;
            case GuitarRegion::strings:       return GuitarSlot::strings;
            case GuitarRegion::neck:          return GuitarSlot::neck;
            case GuitarRegion::fretboard:     return GuitarSlot::fretboard;
            case GuitarRegion::nut:           return GuitarSlot::nut;
            case GuitarRegion::headstock:     return GuitarSlot::tuners;
            case GuitarRegion::tuners:        return GuitarSlot::tuners;
            case GuitarRegion::controls:      return GuitarSlot::wiring;
            case GuitarRegion::selector:      return GuitarSlot::wiring;
            case GuitarRegion::jack:          return GuitarSlot::wiring;
            case GuitarRegion::none:
            case GuitarRegion::numRegions:    break;
        }

        return GuitarSlot::numSlots;
    }

    GuitarRenderer::Options renderOptions()
    {
        GuitarRenderer::Options o;
        o.materials = Palette::textured;
        return o;
    }
}

//==============================================================================
BenchIllustration::BenchIllustration (LuthierAudioProcessor& p)
    : processor (p), bench (p.getBench())
{
    setWantsKeyboardFocus (true);
    setTitle ("Workshop guitar");
    setDescription ("The guitar on the bench. Tab walks the parts; arrow keys nudge the selected one.");
    rebuild (true);
    startTimerHz (30);
}

BenchIllustration::~BenchIllustration()
{
    stopTimer();
}

const std::vector<GuitarRegion>& BenchIllustration::builderOrder()
{
    // Section 10: body, neck, fretboard, frets, nut, bridge, tailpiece, tuners,
    // pickups neck to bridge, wiring, strings, pickguard.
    static const std::vector<GuitarRegion> order {
        GuitarRegion::body, GuitarRegion::neck, GuitarRegion::fretboard, GuitarRegion::nut,
        GuitarRegion::bridge, GuitarRegion::tailpiece, GuitarRegion::tuners,
        GuitarRegion::pickupNeck, GuitarRegion::pickupMiddle, GuitarRegion::pickupBridge,
        GuitarRegion::controls, GuitarRegion::strings, GuitarRegion::pickguard
    };
    return order;
}

void BenchIllustration::rebuild (bool force)
{
    const auto* audition = bench.getAuditionGuitar();
    const auto& guitar = audition != nullptr ? *audition : bench.current();
    const auto options = renderOptions();
    const auto key = GuitarRenderer::keyFor (guitar, options);

    if (! force && key == shownKey && (audition != nullptr) == shownAudition)
        return;

    scene = GuitarRenderer::build (guitar, options);
    shownKey = key;
    shownAudition = audition != nullptr;
    resized();
    repaint();
}

void BenchIllustration::refresh()
{
    rebuild (false);
}

void BenchIllustration::resized()
{
    // The fitted view, then zoom about the centre and pan (guitar-illustration.md 1).
    auto area = getLocalBounds().toFloat().reduced (8.0f);
    area.removeFromBottom (22.0f);   // the ruler

    const auto fit = GuitarRenderer::fitTransform (scene, area);
    const auto c = area.getCentre();
    mmToPx = fit.followedBy (juce::AffineTransform::scale (zoom, zoom, c.x, c.y))
                .followedBy (juce::AffineTransform::translation (panPx.x, panPx.y));
}

juce::Point<float> BenchIllustration::toMm (juce::Point<float> px) const
{
    auto p = px;
    mmToPx.inverted().transformPoint (p.x, p.y);
    return p;
}

juce::Point<float> BenchIllustration::toPx (juce::Point<float> mm) const
{
    auto p = mm;
    mmToPx.transformPoint (p.x, p.y);
    return p;
}

void BenchIllustration::timerCallback()
{
    rebuild (false);
}

GuitarRegion BenchIllustration::regionAt (juce::Point<float> px, int* stringIndex) const
{
    const auto mm = toMm (px);
    const auto* hit = GuitarRenderer::hitTest (scene, mm);

    if (hit == nullptr)
        return GuitarRegion::none;

    if (stringIndex != nullptr && hit->region == GuitarRegion::strings)
    {
        // The nearest string where the pointer is along the neck.
        float best = 1.0e9f;

        for (int s = 0; s < (int) scene.saddlePoints.size(); ++s)
        {
            const auto a = scene.saddlePoints[(size_t) s], b = scene.nutPoints[(size_t) s];
            const float t = juce::jlimit (0.0f, 1.0f, (mm.x - a.x) / juce::jmax (1.0f, b.x - a.x));
            const float y = a.y + (b.y - a.y) * t;

            if (std::abs (mm.y - y) < best)
            {
                best = std::abs (mm.y - y);
                *stringIndex = s;
            }
        }
    }

    return hit->region;
}

int BenchIllustration::pickupIndexFor (GuitarRegion r) const
{
    return pickupIndexOfRegion (r);
}

void BenchIllustration::select (GuitarRegion region, int stringIndex)
{
    selected = region;
    selectedString = region == GuitarRegion::strings || region == GuitarRegion::bridge ? stringIndex : -1;

    // Screen readers hear the part (guitar-illustration.md 16).
    for (auto& h : scene.hits)
        if (h.region == region)
            setDescription (h.description);

    repaint();

    if (onSelectionChanged)
        onSelectionChanged();
}

//==============================================================================
void BenchIllustration::paint (juce::Graphics& g)
{
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), Metrics::panelCorner);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (getLocalBounds().reduced (1));
        GuitarRenderer::paint (g, scene, mmToPx);

        GuitarOverlay overlay;
        overlay.accent = Palette::accent;
        overlay.hovered = hovered;
        overlay.selected = selected;
        GuitarRenderer::paintOverlay (g, scene, mmToPx, overlay);

        // A selected string, outlined along its length.
        if (selected == GuitarRegion::strings && juce::isPositiveAndBelow (selectedString, (int) scene.saddlePoints.size()))
        {
            g.setColour (Palette::accent);
            g.drawLine ({ toPx (scene.saddlePoints[(size_t) selectedString]), toPx (scene.nutPoints[(size_t) selectedString]) }, 2.0f);
        }
    }

    // ---- the ruler: mm from the saddle, with the pickup rail (section 2) ------------------
    const float y = (float) getHeight() - 16.0f;
    const float x0 = toPx ({ 0.0f, 0.0f }).x;
    const float x250 = toPx ({ 250.0f, 0.0f }).x;

    g.setColour (Palette::edgeBright);
    g.drawLine (juce::jmin (x0, x250), y, juce::jmax (x0, x250), y, 1.0f);
    g.setFont (Fonts::mono (9.0f));

    for (int mm = 0; mm <= 250; mm += 10)
    {
        const float x = toPx ({ (float) mm, 0.0f }).x;
        const bool major = mm % 50 == 0;
        g.setColour (major ? Palette::textMuted : Palette::edgeBright);
        g.drawLine (x, y, x, y + (major ? 6.0f : 3.0f), 1.0f);

        if (major)
            g.drawText (juce::String (mm), juce::Rectangle<float> (x - 15.0f, y + 5.0f, 30.0f, 10.0f), juce::Justification::centred, false);
    }

    g.setColour (Palette::textMuted);
    g.drawText ("mm from saddle", juce::Rectangle<float> (juce::jmax (x0, x250) + 6.0f, y - 5.0f, 90.0f, 10.0f),
                juce::Justification::centredLeft, false);

    // The pickup rail: where each pickup's centre sits.
    const auto& guitar = bench.current();

    for (int i = 0; i < 3; ++i)
    {
        if (guitar.get (WorkshopGuitar::pickupSlot (i)) == nullptr)
            continue;

        const float x = toPx ({ (float) guitar.placements[(size_t) i].positionMm, 0.0f }).x;
        juce::Path marker;
        marker.addTriangle (x - 4.0f, y - 7.0f, x + 4.0f, y - 7.0f, x, y - 1.0f);
        g.setColour (pickupIndexOfRegion (selected) == i ? Palette::accent : Palette::textMuted);
        g.fillPath (marker);

        if (pickupIndexOfRegion (selected) == i || drag == Drag::pickup)
            g.drawText (juce::String (guitar.placements[(size_t) i].positionMm, 1),
                        juce::Rectangle<float> (x - 20.0f, y - 20.0f, 40.0f, 10.0f), juce::Justification::centred, false);
    }

    if (bench.isAuditioning())
    {
        g.setColour (Palette::accent);
        g.setFont (Fonts::ui (11.0f, true));
        g.drawText ("AUDITIONING - release Alt to go back", getLocalBounds().reduced (10, 6), juce::Justification::topRight, false);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), Metrics::panelCorner, 1.5f);
    }
}

//==============================================================================
void BenchIllustration::mouseMove (const juce::MouseEvent& e)
{
    // Section 3.1: hover outlines and names a part; it never selects.
    const auto r = regionAt (e.position);

    if (r != hovered)
    {
        hovered = r;
        repaint();
    }

    juce::String tip;

    for (auto& h : scene.hits)
        if (h.region == r)
            tip = h.description;

    if (pickupIndexOfRegion (r) >= 0)
        tip << "  Drag along the strings to move it; scroll to raise or lower it (Shift: treble side, Alt: bass side).";
    else if (r == GuitarRegion::bridge)
        tip << "  Drag a saddle along the string to set its intonation.";

    setMouseCursor (pickupIndexOfRegion (r) >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    setHelpText (tip);
}

void BenchIllustration::mouseExit (const juce::MouseEvent&)
{
    hovered = GuitarRegion::none;
    repaint();
}

void BenchIllustration::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    if (e.mods.isMiddleButtonDown() || (e.mods.isRightButtonDown() && zoom > 1.0f))
    {
        drag = Drag::pan;
        dragStartMm = e.position;
        dragStartValue = 0.0;
        return;
    }

    int stringIndex = -1;
    auto r = regionAt (e.position, &stringIndex);

    // Section 13.1: Shift-click reaches the part under the topmost one.
    if (e.mods.isShiftDown())
    {
        const auto mm = toMm (e.position);
        const GuitarScene::Hit* top = nullptr;
        const GuitarScene::Hit* below = nullptr;

        for (auto& h : scene.hits)
            if (h.area.contains (mm))
            {
                below = top;
                top = &h;
            }

        if (below != nullptr)
            r = below->region;
    }

    // A click on the bridge picks the nearest string's saddle.
    if (r == GuitarRegion::bridge)
    {
        const auto mm = toMm (e.position);
        float best = 1.0e9f;

        for (int s = 0; s < (int) scene.saddlePoints.size(); ++s)
            if (std::abs (scene.saddlePoints[(size_t) s].y - mm.y) < best)
            {
                best = std::abs (scene.saddlePoints[(size_t) s].y - mm.y);
                stringIndex = s;
            }
    }

    select (r, stringIndex);

    const int pickup = pickupIndexOfRegion (r);

    if (pickup >= 0 && ! e.mods.isShiftDown())
    {
        drag = Drag::pickup;
        dragIndex = pickup;
        dragStartMm = toMm (e.position);
        dragStartValue = bench.current().placements[(size_t) pickup].positionMm;
        bench.beginGesture();
    }
    else if (r == GuitarRegion::bridge && stringIndex >= 0)
    {
        drag = Drag::saddle;
        dragIndex = stringIndex;
        dragStartMm = toMm (e.position);
        const auto& values = bench.current().setup.intonationMm;
        dragStartValue = juce::isPositiveAndBelow (stringIndex, values.size()) ? values[stringIndex] : 0.0;
        bench.beginGesture();
    }
}

void BenchIllustration::mouseDrag (const juce::MouseEvent& e)
{
    const bool fine = e.mods.isShiftDown(), free = e.mods.isAltDown();

    if (drag == Drag::pan)
    {
        panPx += e.position - dragStartMm;
        dragStartMm = e.position;
        resized();
        repaint();
        return;
    }

    const auto mm = toMm (e.position);

    if (drag == Drag::pickup)
    {
        // Constrained to the string axis (section 4): only X counts.
        const double target = WorkshopBench::snap (dragStartValue + (double) (mm.x - dragStartMm.x), fine, free);
        juce::String why;
        const double landed = bench.movePickup (dragIndex, target, &why);

        if (why.isNotEmpty() && onLimit)
            onLimit ("The " + juce::String (dragIndex == 0 ? "neck" : dragIndex == 1 ? "middle" : "bridge")
                     + " pickup stops here: " + why + ".");

        if (onPickupDragged)
            onPickupDragged (dragIndex, landed);

        rebuild (false);
    }
    else if (drag == Drag::saddle)
    {
        // Pulling the saddle back (away from the nut) adds compensation.
        const double target = WorkshopBench::snap (dragStartValue - (double) (mm.x - dragStartMm.x), fine, free, 0.1);
        bench.setIntonation (dragIndex, target);
        rebuild (false);
    }
}

void BenchIllustration::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::pickup || drag == Drag::saddle)
        bench.endGesture();

    if (drag == Drag::pickup && onPickupDragged)
        onPickupDragged (-1, 0.0);

    drag = Drag::none;
    dragIndex = -1;
    rebuild (false);
}

void BenchIllustration::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Ctrl-scroll zooms up to 4x about the pointer (guitar-illustration.md 1).
    if (e.mods.isCommandDown())
    {
        const float before = zoom;
        zoom = juce::jlimit (1.0f, 4.0f, zoom * (wheel.deltaY > 0 ? 1.15f : 1.0f / 1.15f));

        if (zoom <= 1.0f)
            panPx = {};
        else if (before != zoom)
        {
            // Keep the millimetre under the pointer where it was.
            const auto under = toMm (e.position);
            resized();
            panPx += e.position - toPx (under);
        }

        resized();
        repaint();
        return;
    }

    // Scroll over a pickup raises or lowers it: 0.1 mm a notch (section 4).
    const int pickup = pickupIndexOfRegion (regionAt (e.position));

    if (pickup < 0 || bench.current().get (WorkshopGuitar::pickupSlot (pickup)) == nullptr)
        return;

    const auto& p = bench.current().placements[(size_t) pickup];
    const double step = wheel.deltaY > 0 ? -0.1 : 0.1;   // up raises: less distance to the strings
    const bool trebleOnly = e.mods.isShiftDown(), bassOnly = e.mods.isAltDown();

    bench.setPickupHeights (pickup,
                            bassOnly ? p.heightTrebleMm : p.heightTrebleMm + step,
                            trebleOnly ? p.heightBassMm : p.heightBassMm + step);
    select (pickup == 0 ? GuitarRegion::pickupNeck : pickup == 1 ? GuitarRegion::pickupMiddle : GuitarRegion::pickupBridge);
    rebuild (false);
}

bool BenchIllustration::keyPressed (const juce::KeyPress& key)
{
    // Section 10: Tab walks the parts in the builder's order; arrows nudge.
    if (key.getKeyCode() == juce::KeyPress::tabKey)
    {
        std::vector<GuitarRegion> present;

        for (auto r : builderOrder())
            for (auto& h : scene.hits)
                if (h.region == r)
                {
                    present.push_back (r);
                    break;
                }

        if (present.empty())
            return false;

        auto it = std::find (present.begin(), present.end(), selected);
        int index = it == present.end() ? -1 : (int) std::distance (present.begin(), it);
        index = key.getModifiers().isShiftDown() ? (index <= 0 ? (int) present.size() - 1 : index - 1)
                                                 : (index + 1) % (int) present.size();
        select (present[(size_t) index], selected == GuitarRegion::strings ? juce::jmax (0, selectedString) : 0);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::escapeKey && selected != GuitarRegion::none)
    {
        select (GuitarRegion::none);
        return true;
    }

    const bool fine = key.getModifiers().isShiftDown();
    const int pickup = pickupIndexOfRegion (selected);
    const bool left = key.getKeyCode() == juce::KeyPress::leftKey, right = key.getKeyCode() == juce::KeyPress::rightKey;
    const bool up = key.getKeyCode() == juce::KeyPress::upKey, down = key.getKeyCode() == juce::KeyPress::downKey;

    if (pickup >= 0 && (left || right))
    {
        // Left on screen is toward the headstock: further from the saddle.
        const double step = fine ? 0.1 : 1.0;
        const double from = bench.current().placements[(size_t) pickup].positionMm;
        juce::String why;
        bench.movePickup (pickup, from + (left ? step : -step), &why);

        if (why.isNotEmpty() && onLimit)
            onLimit ("It stops here: " + why + ".");

        rebuild (false);
        return true;
    }

    if (pickup >= 0 && (up || down))
    {
        const auto& p = bench.current().placements[(size_t) pickup];
        const double step = up ? -0.1 : 0.1;
        bench.setPickupHeights (pickup, p.heightTrebleMm + step, p.heightBassMm + step);
        rebuild (false);
        return true;
    }

    if (selected == GuitarRegion::bridge && selectedString >= 0 && (left || right))
    {
        const auto& values = bench.current().setup.intonationMm;
        const double from = juce::isPositiveAndBelow (selectedString, values.size()) ? values[selectedString] : 0.0;
        bench.setIntonation (selectedString, from + (right ? 0.1 : -0.1) * (fine ? 0.1 : 1.0));
        rebuild (false);
        return true;
    }

    if (selected == GuitarRegion::strings && (up || down))
    {
        const int n = (int) scene.saddlePoints.size();
        select (GuitarRegion::strings, juce::jlimit (0, n - 1, selectedString + (down ? -1 : 1)));
        return true;
    }

    return false;
}

//==============================================================================
//  WorkshopPanel
//==============================================================================
namespace
{
    struct Category
    {
        const char* name;
        PartType type;
    };

    // Section 1's drawer. Preamp shows the wiring parts that are active.
    // "Guitar" is the family selector (guitar-illustration.md 12.1), not a part type.
    const Category kCategories[] = {
        { "Guitar", PartType::numTypes },
        { "Body", PartType::body },         { "Neck", PartType::neck },       { "Frets", PartType::frets },
        { "Nut", PartType::nut },           { "Bridge", PartType::bridge },   { "Tuners", PartType::tuners },
        { "Strings", PartType::strings },   { "Pickups", PartType::pickup },  { "Wiring", PartType::wiring },
        { "Preamp", PartType::wiring },     { "Pick", PartType::pick },       { "Slide", PartType::slide },
        { "Capo", PartType::capo }
    };

    const Category* findCategory (const juce::String& name)
    {
        for (auto& c : kCategories)
            if (name == c.name)
                return &c;
        return nullptr;
    }

    /** One line a card shows under the part's name. */
    juce::String summaryOf (const Part& p)
    {
        // juce::String (v, 0) would print every digit; whole numbers are rounded instead.
        auto num = [&p] (const char* f, int dp) { return dp == 0 ? juce::String (juce::roundToInt (p.number (f, 0.0)))
                                                                   : juce::String (p.number (f, 0.0), dp); };

        switch (p.type)
        {
            case PartType::pickup:   return p.text ("family").replaceCharacter ('_', ' ') + ", " + num ("dc_resistance_k", 1) + "k, " + p.text ("magnet");
            case PartType::bridge:   return p.text ("type").replaceCharacter ('_', ' ') + ", " + num ("mass_g", 0) + " g";
            case PartType::strings:  return p.text ("winding_material").replaceCharacter ('_', ' ') + ", " + p.text ("winding") + " wound";
            case PartType::body:     return p.text ("wood").replaceCharacter ('_', ' ') + ", " + p.text ("chambering");
            case PartType::neck:     return p.text ("wood").replaceCharacter ('_', ' ') + ", " + num ("scale_length_mm", 0) + " mm, " + p.text ("joint");
            case PartType::wiring:   return p.text ("switching").replaceCharacter ('_', ' ') + (p.flag ("active") ? ", active" : ", passive");
            case PartType::nut:      return p.text ("material") + ", " + num ("width_mm", 1) + " mm";
            case PartType::frets:    return p.text ("material").replaceCharacter ('_', ' ') + ", " + num ("height_mm", 2) + " mm";
            case PartType::tuners:   return juce::String (juce::roundToInt (p.number ("ratio", 15.0))) + ":1" + (p.flag ("locking") ? ", locking" : "");
            case PartType::pick:     return p.text ("material") + ", " + num ("thickness_mm", 2) + " mm";
            case PartType::slide:    return p.text ("material") + ", " + num ("mass_g", 0) + " g";
            case PartType::capo:     return p.text ("type") + ", pressure " + num ("pressure", 2);
            case PartType::numTypes: return "Rebuild as a " + p.text ("family") + " guitar (12.1)";
            case PartType::top:
            case PartType::fretboard:
            case PartType::tailpiece:
            case PartType::pickguard: break;
        }

        return p.text ("wood", p.text ("material", p.text ("type")));
    }

    /** A field value as the inspector shows it. */
    juce::String fieldText (const juce::var& v)
    {
        if (v.isArray())
        {
            juce::StringArray items;
            for (auto& x : *v.getArray())
                items.add (x.toString());
            return "[" + items.joinIntoString (", ") + "]";
        }

        return v.toString();
    }
}

WorkshopPanel::WorkshopPanel (LuthierAudioProcessor& p)
    : processor (p), bench (p.getBench()), illustration (p)
{
    setTitle ("Workshop");
    setDescription ("The bench: take the guitar apart, swap parts, move pickups, hear what changed.");

    addAndMakeVisible (title);
    title.setText ("WORKSHOP", juce::dontSendNotification);
    title.setFont (Fonts::display (20.0f));
    title.setColour (juce::Label::textColourId, Palette::textPrimary);

    addAndMakeVisible (guitarName);
    guitarName.setFont (Fonts::ui (13.0f, true));
    guitarName.setColour (juce::Label::textColourId, Palette::textMuted);

    addAndMakeVisible (saveAsButton);
    saveAsButton.setTooltip ("Save this guitar as a .luthierguitar file (Ctrl+G)");
    saveAsButton.onClick = [this]
    {
        if (onSaveAsGuitar)
            onSaveAsGuitar();
    };

    // Section 7: eight A/B slots. Click stores into an empty slot and recalls a
    // full one; Shift-click clears.
    for (int i = 0; i < WorkshopBench::kNumSlots; ++i)
    {
        auto* b = slotButtons.add (new juce::TextButton (WorkshopBench::slotName (i)));
        addAndMakeVisible (b);
        b->setClickingTogglesState (false);
        b->onClick = [this, i]
        {
            if (juce::ModifierKeys::currentModifiers.isShiftDown())
                bench.clearSlot (i);
            else if (bench.hasSlot (i))
                bench.recallSlot (i);
            else
                bench.storeSlot (i);

            refreshAll();
        };
    }

    for (auto& c : kCategories)
    {
        auto* b = categoryButtons.add (new juce::TextButton (c.name));
        addAndMakeVisible (b);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (0x57);
        b->onClick = [this, name = juce::String (c.name)] { showCategory (name); };
    }

    addAndMakeVisible (illustration);
    illustration.onSelectionChanged = [this]
    {
        // Selecting a part shows its category in the drawer (section 5's Swap, done for you).
        const auto slot = slotForRegion (illustration.getSelected());

        if (slot != GuitarSlot::numSlots)
            for (auto& c : kCategories)
                if (c.type == getSlotPartType (slot) && juce::String (c.name) != "Preamp")
                {
                    showCategory (c.name);
                    break;
                }

        refreshInspector();
        repaint();
    };
    illustration.onPickupDragged = [this] (int index, double mm)
    {
        if (index >= 0)
            requestSpectrum (index, mm);
        refreshInspector();
    };
    illustration.onLimit = [this] (const juce::String& message)
    {
        limitMessage = message;
        limitShownAt = juce::Time::getMillisecondCounter();
        repaint();
    };

    addAndMakeVisible (swapButton);
    swapButton.setTooltip ("Show the parts that can go in this slot");
    swapButton.onClick = [this]
    {
        const auto slot = slotForRegion (illustration.getSelected());
        if (slot != GuitarSlot::numSlots)
            for (auto& c : kCategories)
                if (c.type == getSlotPartType (slot))
                {
                    showCategory (c.name);
                    break;
                }
    };

    addAndMakeVisible (revertButton);
    revertButton.setTooltip ("Put this slot back to what the guitar file has");
    revertButton.onClick = [this]
    {
        const auto slot = slotForRegion (illustration.getSelected());
        if (slot != GuitarSlot::numSlots)
            bench.revert (slot);
        refreshAll();
    };

    addAndMakeVisible (savePartButton);
    savePartButton.setTooltip ("Keep this edited part in your parts folder");
    savePartButton.onClick = [this]
    {
        const auto slot = slotForRegion (illustration.getSelected());
        const auto part = slot != GuitarSlot::numSlots ? bench.current().get (slot) : nullptr;

        if (part == nullptr)
            return;

        auto* window = new juce::AlertWindow ("Save as user part", "Name the part:", juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", part->name + " (mine)");
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window, slot] (int result)
        {
            if (result == 1)
                processor.savePartAs (slot, window->getTextEditorContents ("name"));
            refreshAll();
        }), true);
    };

    addAndMakeVisible (autoZoomToggle);
    autoZoomToggle.setTooltip ("Fit the spectrum's scale to the change instead of +-12 dB");
    autoZoomToggle.onClick = [this] { autoZoom = autoZoomToggle.getToggleState(); repaint(); };

    // ---- setup strip: the parameters fret-buzz.md 1 already has ------------------------
    actionTreble = std::make_unique<LuthierKnob> ("Action T", LuthierKnob::Size::Small);
    actionBass   = std::make_unique<LuthierKnob> ("Action B", LuthierKnob::Size::Small);
    relief       = std::make_unique<LuthierKnob> ("Relief", LuthierKnob::Size::Small);

    actionTreble->attachTo (processor, ParamIDs::setupActionTreble, "String height at the 12th fret, treble side (mm)");
    actionBass->attachTo (processor, ParamIDs::setupActionBass, "String height at the 12th fret, bass side (mm)");
    relief->attachTo (processor, ParamIDs::setupRelief, "Neck relief at the 7th fret (mm)");

    for (auto* k : { actionTreble.get(), actionBass.get(), relief.get() })
        addAndMakeVisible (k);

    for (int n = 1; n <= ParamIDs::kNumNutDepths; ++n)
    {
        auto* k = nutDepths.add (new LuthierKnob ("Nut " + juce::String (n), LuthierKnob::Size::Small));
        k->attachTo (processor, ParamIDs::setupNutDepth (n), "Nut slot depth, string " + juce::String (n) + " (mm)");
        addAndMakeVisible (k);
    }

    showCategory (category);
    refreshAll();
    startTimerHz (20);
}

WorkshopPanel::~WorkshopPanel()
{
    stopTimer();
    bench.endAudition();
}

juce::StringArray WorkshopPanel::drawerCategories()
{
    juce::StringArray names;
    for (auto& c : kCategories)
        names.add (c.name);
    return names;
}

void WorkshopPanel::showCategory (const juce::String& name)
{
    if (findCategory (name) == nullptr)
        return;

    category = name;

    for (auto* b : categoryButtons)
        b->setToggleState (b->getButtonText() == name, juce::dontSendNotification);

    refreshDrawer();
    repaint();
}

GuitarSlot WorkshopPanel::targetSlot() const
{
    const auto* c = findCategory (category);

    if (c == nullptr || c->type == PartType::pick || c->type == PartType::slide || c->type == PartType::capo)
        return GuitarSlot::numSlots;

    if (c->type == PartType::pickup)
    {
        // The selected pickup, else the first fitted one (neck first), else the neck slot.
        const int selected = pickupIndexOfRegion (illustration.getSelected());
        if (selected >= 0)
            return WorkshopGuitar::pickupSlot (selected);

        for (int i = 0; i < 3; ++i)
            if (bench.current().get (WorkshopGuitar::pickupSlot (i)) != nullptr)
                return WorkshopGuitar::pickupSlot (i);

        return GuitarSlot::pickupNeck;
    }

    for (int i = 0; i < kNumGuitarSlots; ++i)
        if (getSlotPartType ((GuitarSlot) i) == c->type)
            return (GuitarSlot) i;

    return GuitarSlot::numSlots;
}

void WorkshopPanel::refreshDrawer()
{
    drawerParts.clear();
    const auto* c = findCategory (category);

    if (c == nullptr)
        return;

    if (c->type == PartType::numTypes)
    {
        // One card per family; the card is a stand-in, never fitted as a part.
        for (auto family : { "electric", "acoustic", "classical", "bass", "resonator" })
        {
            auto card = std::make_shared<Part>();
            card->name = juce::String (family).substring (0, 1).toUpperCase() + juce::String (family).substring (1);
            card->type = PartType::numTypes;
            card->fields = juce::var (new juce::DynamicObject());
            card->fields.getDynamicObject()->setProperty ("family", juce::String (family));
            card->isFactory = true;
            drawerParts.add (card);
        }

        return;
    }

    for (const auto& p : processor.getPartLibrary().getParts (c->type))
    {
        if (juce::String (c->name) == "Preamp" && ! p->flag ("active"))
            continue;

        drawerParts.add (p);
    }

    // Factory first, then the user's (section 9's user section), each by name.
    std::stable_sort (drawerParts.begin(), drawerParts.end(), [] (const PartPtr& a, const PartPtr& b)
    {
        if (a->isFactory != b->isFactory)
            return a->isFactory;
        return a->name.compareNatural (b->name) < 0;
    });
}

bool WorkshopPanel::switchFamily (const juce::String& family, bool confirmed)
{
    if (family == processor.getCurrentGuitar().family)
        return false;

    if (! confirmed && ! familyConfirmedThisSession)
    {
        // 12.1's one-time question, the first time in a session.
        auto options = juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon,
                           "Change guitar family?",
                           "This will replace incompatible parts with defaults for the new family.",
                           "Change", "Cancel", this);

        juce::AlertWindow::showAsync (options, [safe = juce::Component::SafePointer<WorkshopPanel> (this), family] (int result)
        {
            if (safe != nullptr && result == 1)
            {
                safe->familyConfirmedThisSession = true;
                safe->switchFamily (family, true);
            }
        });

        return false;
    }

    familyConfirmedThisSession = true;
    const bool ok = processor.switchGuitarFamily (family);

    // The banner (12.1) lists what changed.
    for (const auto& notice : processor.takeGuitarNotices())
    {
        limitMessage = notice;
        limitShownAt = juce::Time::getMillisecondCounter();
    }

    refreshAll();
    return ok;
}

bool WorkshopPanel::editInspectorField (const juce::String& field, const juce::String& text)
{
    const auto slot = slotForRegion (illustration.getSelected());

    if (slot == GuitarSlot::numSlots || field.isEmpty())
        return false;

    // Numbers stay numbers, true/false stay flags, lists stay lists; anything else is text.
    const auto trimmed = text.trim();
    juce::var value;

    if (trimmed.startsWithChar ('['))
        value = juce::JSON::parse (trimmed);
    else if (trimmed.equalsIgnoreCase ("true") || trimmed.equalsIgnoreCase ("false"))
        value = trimmed.equalsIgnoreCase ("true");
    else if (trimmed.containsOnly ("0123456789.-+eE") && trimmed.isNotEmpty())
        value = trimmed.getDoubleValue();
    else
        value = trimmed;

    const bool ok = bench.editField (slot, field, value);
    refreshAll();
    return ok;
}

void WorkshopPanel::clickCard (int index)
{
    if (! juce::isPositiveAndBelow (index, drawerParts.size()))
        return;

    const auto part = drawerParts[index];
    bench.endAudition();
    auditioning = false;

    if (part->type == PartType::numTypes)
    {
        switchFamily (part->text ("family"), false);
        return;
    }

    if (part->type == PartType::pick || part->type == PartType::slide || part->type == PartType::capo)
    {
        // Section 9: a slide only fits in Slide Mode.
        if (part->type == PartType::slide
            && processor.getState().getRawParameterValue (ParamIDs::slideGuitar)->load() < 0.5f)
        {
            limitMessage = "Turn on Slide Mode (S) to fit a slide.";
            limitShownAt = juce::Time::getMillisecondCounter();
            repaint();
            return;
        }

        bench.fitAccessory (part);
    }
    else if (const auto slot = targetSlot(); slot != GuitarSlot::numSlots)
    {
        bench.fit (slot, part);
    }

    refreshAll();
}

void WorkshopPanel::hoverCard (int index, bool altDown)
{
    // Section 3.2: Alt-hover auditions on a shadow guitar; moving off or
    // releasing Alt goes back. Accessories have nothing to audition on the guitar.
    const bool wantAudition = altDown && juce::isPositiveAndBelow (index, drawerParts.size())
                           && targetSlot() != GuitarSlot::numSlots;

    if (index != hoveredCard || wantAudition != auditioning)
    {
        hoveredCard = index;

        if (wantAudition)
        {
            bench.beginAudition (targetSlot(), drawerParts[index]);
            auditioning = true;
            requestSpectrum();
        }
        else if (auditioning)
        {
            bench.endAudition();
            auditioning = false;
        }

        illustration.refresh();
        refreshInspector();
        repaint();
    }
}

void WorkshopPanel::requestSpectrum (int pickupIndex, double positionMm)
{
    const auto& committed = processor.getCurrentGuitar();
    const auto* audition = bench.getAuditionGuitar();
    const auto& candidate = audition != nullptr ? *audition : bench.current();

    std::vector<float> notches;

    if (pickupIndex >= 0)
    {
        // Section 4: while dragging a pickup, the comb notches for the low string.
        const auto neck = candidate.get (GuitarSlot::neck);
        const double scale = neck != nullptr ? neck->number ("scale_length_mm", 648.0) : 648.0;
        const int lowest = processor.getEngine().getNumStrings() - 1;
        notches = SpectrumDelta::combNotches (positionMm, scale, processor.getEngine().getTuningEngine().getEffectiveOpenFrequency (lowest));
    }

    lastRequest = worker.request (committed, candidate, processor.getEngine().getGuitarType(), std::move (notches));
}

bool WorkshopPanel::waitForSpectrum (int timeoutMs)
{
    const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;

    while (juce::Time::getMillisecondCounter() < until)
    {
        SpectrumDelta::Result r;

        if (worker.takeResult (r))
        {
            spectrum = std::move (r);
            if (spectrum.requestId == lastRequest)
                return true;
        }

        juce::Thread::sleep (5);
    }

    return false;
}

void WorkshopPanel::refreshAll()
{
    refreshHeader();
    refreshDrawer();
    refreshInspector();
    illustration.refresh();
    repaint();
}

void WorkshopPanel::refreshHeader()
{
    const auto& g = processor.getCurrentGuitar();
    guitarName.setText (g.name + (bench.isModified() ? "  (modified)" : ""), juce::dontSendNotification);

    for (int i = 0; i < slotButtons.size(); ++i)
    {
        auto* b = slotButtons[i];
        const bool filled = bench.hasSlot (i);
        b->setToggleState (filled, juce::dontSendNotification);
        b->setTooltip (filled ? "Bench slot " + WorkshopBench::slotName (i) + ": click to recall, Shift-click to clear"
                              : "Bench slot " + WorkshopBench::slotName (i) + ": click to store the guitar as it is now");
    }
}

void WorkshopPanel::refreshInspector()
{
    inspectorLines.clear();
    inspectorFields.clear();

    const auto region = illustration.getSelected();
    const auto slot = slotForRegion (region);
    const auto* audition = bench.getAuditionGuitar();
    const auto& guitar = audition != nullptr ? *audition : bench.current();

    if (slot == GuitarSlot::numSlots)
    {
        inspectorTitle = "Nothing selected";
        inspectorLines.add ("Click a part of the guitar, or press Tab.");
        return;
    }

    const auto part = guitar.get (slot);
    const auto label = WorkshopBench::describeSlot (slot);
    inspectorTitle = label.substring (0, 1).toUpperCase() + label.substring (1) + ": "
                   + (part != nullptr ? part->name : juce::String ("(empty)"));

    if (audition != nullptr)
        inspectorLines.add ("AUDITIONING - not fitted");

    if (part != nullptr)
    {
        inspectorLines.add (part->isFactory ? "Factory part" : "User part (unsaved edits live in the preset)");

        if (! part->suits (guitar.family))
            inspectorLines.add ("Unusual on a " + guitar.family + " guitar - it will work.");

        if (auto* object = part->fields.getDynamicObject())
            for (auto& prop : object->getProperties())
            {
                inspectorLines.add (prop.name.toString().replaceCharacter ('_', ' ') + ": " + fieldText (prop.value));

                while (inspectorFields.size() < inspectorLines.size() - 1)
                    inspectorFields.add ({});

                inspectorFields.add (prop.name.toString());
            }
    }

    // tuning-stability.md 6: the tuners' and the nut's derived figures (REALISM-C).
    inspectorLines.addArray (describeTuningFigures (processor, slot, part.get()));

    if (const int i = pickupIndexOfRegion (region); i >= 0 && part != nullptr)
    {
        const auto& pl = guitar.placements[(size_t) i];
        const auto travel = bench.getPickupTravel (i);
        inspectorLines.add ("position: " + juce::String (pl.positionMm, 1) + " mm from the saddle ("
                            + juce::String (juce::roundToInt (travel.min)) + " - " + juce::String (juce::roundToInt (travel.max)) + ")");
        inspectorLines.add ("height: " + juce::String (pl.heightTrebleMm, 1) + " mm treble / "
                            + juce::String (pl.heightBassMm, 1) + " mm bass");
    }

    if (region == GuitarRegion::bridge || region == GuitarRegion::strings)
    {
        const int s = illustration.getSelectedString();

        if (s >= 0)
        {
            const auto& inton = guitar.setup.intonationMm;
            inspectorLines.add ("string " + juce::String (s + 1) + " saddle: "
                                + juce::String (juce::isPositiveAndBelow (s, inton.size()) ? inton[s] : 0.0, 1) + " mm");
        }
    }

    if (region == GuitarRegion::nut)
    {
        juce::StringArray depths;
        for (auto d : guitar.setup.nutSlotDepthsMm)
            depths.add (juce::String (d, 2));
        inspectorLines.add ("slot depths: " + depths.joinIntoString (", ") + " mm");
    }
}

//==============================================================================
void WorkshopPanel::timerCallback()
{
    const auto key = GuitarRenderer::keyFor (processor.getCurrentGuitar(), {});

    if (key != shownGuitarKey)
    {
        // A commit, an undo, a recall, a preset: show what the last change did.
        shownGuitarKey = key;
        refreshAll();
    }

    SpectrumDelta::Result r;

    if (worker.takeResult (r))
    {
        spectrum = std::move (r);
        repaint (spectrumArea);
    }

    // Alt let go without the mouse moving: the audition ends (section 3.2).
    if (auditioning && ! juce::ModifierKeys::currentModifiers.isAltDown())
        hoverCard (hoveredCard, false);

    if (limitMessage.isNotEmpty() && juce::Time::getMillisecondCounter() - limitShownAt > 3000)
    {
        limitMessage.clear();
        repaint();
    }
}

int WorkshopPanel::cardAt (juce::Point<int> p) const
{
    for (int i = 0; i < cardBounds.size(); ++i)
        if (cardBounds[i].contains (p))
            return i;
    return -1;
}

void WorkshopPanel::mouseMove (const juce::MouseEvent& e)
{
    const int card = cardAt (e.getPosition());
    hoverCard (card, e.mods.isAltDown());

    if (card >= 0)
    {
        const auto& p = *drawerParts[card];
        setHelpText (p.name + " - " + summaryOf (p) + ". Click to fit; hold Alt to hear it without fitting.");
    }
}

void WorkshopPanel::mouseExit (const juce::MouseEvent&)
{
    hoverCard (-1, false);
}

void WorkshopPanel::mouseDown (const juce::MouseEvent& e)
{
    if (e.getNumberOfClicks() >= 2)
    {
        for (int i = 0; i < inspectorRows.size(); ++i)
        {
            if (! inspectorRows[i].contains (e.getPosition()) || inspectorFields[i].isEmpty())
                continue;

            editingField = inspectorFields[i];
            fieldEditor = std::make_unique<juce::TextEditor>();
            addAndMakeVisible (*fieldEditor);
            fieldEditor->setBounds (inspectorRows[i]);
            fieldEditor->setText (inspectorLines[i].fromFirstOccurrenceOf (": ", false, false), false);
            fieldEditor->selectAll();
            fieldEditor->grabKeyboardFocus();
            fieldEditor->onReturnKey = [this]
            {
                const auto text = fieldEditor->getText();
                const auto field = editingField;
                fieldEditor.reset();
                editInspectorField (field, text);
            };
            fieldEditor->onEscapeKey = [this] { fieldEditor.reset(); repaint(); };
            fieldEditor->onFocusLost = [this] { fieldEditor.reset(); repaint(); };
            return;
        }
    }

    if (const int card = cardAt (e.getPosition()); card >= 0)
        clickCard (card);
}

void WorkshopPanel::modifierKeysChanged (const juce::ModifierKeys& mods)
{
    hoverCard (hoveredCard, mods.isAltDown());
}

//==============================================================================
void WorkshopPanel::resized()
{
    auto area = getLocalBounds().reduced (Metrics::grid);

    headerArea = area.removeFromTop (32);
    {
        auto h = headerArea;
        title.setBounds (h.removeFromLeft (130));
        for (int i = slotButtons.size(); --i >= 0;)
        {
            slotButtons[i]->setBounds (h.removeFromRight (26).reduced (1, 3));
        }
        h.removeFromRight (Metrics::grid);
        saveAsButton.setBounds (h.removeFromRight (130).reduced (0, 3));
        guitarName.setBounds (h);
    }

    area.removeFromTop (Metrics::gridHalf);

    // Section 1: the inspector down the right, the spectrum under it.
    const bool wide = area.getWidth() >= 900;
    auto right = area.removeFromRight (wide ? 240 : 190);
    area.removeFromRight (Metrics::grid);

    spectrumArea = right.removeFromBottom (juce::jmin (180, right.getHeight() / 3));
    autoZoomToggle.setBounds (spectrumArea.getRight() - 96, spectrumArea.getY() + 2, 94, 20);
    right.removeFromBottom (Metrics::grid);

    inspectorArea = right;
    {
        auto buttons = inspectorArea.withTrimmedTop (inspectorArea.getHeight() - 28);
        const int w = buttons.getWidth() / 3;
        swapButton.setBounds (buttons.removeFromLeft (w).reduced (1, 2));
        revertButton.setBounds (buttons.removeFromLeft (w).reduced (1, 2));
        savePartButton.setBounds (buttons.reduced (1, 2));
    }

    setupArea = area.removeFromBottom (86);
    {
        auto s = setupArea.reduced (0, 2);
        const int knobW = juce::jmax (48, s.getWidth() / 9);
        for (auto* k : { actionTreble.get(), actionBass.get(), relief.get() })
            k->setBounds (s.removeFromLeft (knobW));
        for (auto* k : nutDepths)
            k->setBounds (s.removeFromLeft (knobW));
    }
    area.removeFromBottom (Metrics::gridHalf);

    drawerArea = area.removeFromBottom (juce::jmax (110, area.getHeight() / 3));
    {
        auto tabs = drawerArea.removeFromTop (24);
        const int w = tabs.getWidth() / juce::jmax (1, categoryButtons.size());
        for (auto* b : categoryButtons)
            b->setBounds (tabs.removeFromLeft (w).reduced (1, 0));
    }
    area.removeFromBottom (Metrics::gridHalf);

    illustrationArea = area;
    illustration.setBounds (illustrationArea);
}

void WorkshopPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    paintDrawer (g, drawerArea);
    paintInspector (g, inspectorArea);
    paintSpectrum (g, spectrumArea);

    LuthierLookAndFeel::drawSectionHeader (g, setupArea.withHeight (16).translated (0, -2), "Setup");

    if (limitMessage.isNotEmpty())
    {
        auto r = illustrationArea.withHeight (22).reduced (8, 0).translated (0, 6);
        g.setColour (Palette::panelRaised.withAlpha (0.9f));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
        g.setColour (Palette::warning);
        g.setFont (Fonts::ui (12.0f));
        g.drawText (limitMessage, r.reduced (8, 0), juce::Justification::centredLeft, true);
    }
}

void WorkshopPanel::paintDrawer (juce::Graphics& g, juce::Rectangle<int> area)
{
    cardBounds.clear();
    LuthierLookAndFeel::drawPanel (g, area.toFloat());

    auto inner = area.reduced (6);
    const auto slot = targetSlot();
    const auto fitted = slot != GuitarSlot::numSlots ? bench.current().get (slot) : nullptr;
    const auto* c = findCategory (category);
    const auto accessory = c != nullptr ? bench.getAccessory (c->type) : nullptr;

    const bool slideOff = category == "Slide"
                       && processor.getState().getRawParameterValue (ParamIDs::slideGuitar)->load() < 0.5f;

    if (slideOff)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (12.0f));
        g.drawText ("Turn on Slide Mode (S) to fit a slide.", inner.removeFromTop (18), juce::Justification::centredLeft, false);
    }

    const int cardW = 168, cardH = 42;
    int x = inner.getX(), y = inner.getY();
    bool anyUser = false;

    for (int i = 0; i < drawerParts.size(); ++i)
    {
        const auto& part = *drawerParts[i];
        anyUser = anyUser || ! part.isFactory;

        if (x + cardW > inner.getRight())
        {
            x = inner.getX();
            y += cardH + 4;
        }

        const juce::Rectangle<int> card (x, y, cardW, cardH);
        x += cardW + 4;

        if (card.getBottom() > inner.getBottom())
            break;

        cardBounds.add (card);

        const bool isFitted = (fitted != nullptr && fitted->name == part.name) || (accessory != nullptr && accessory->name == part.name)
                           || (part.type == PartType::numTypes && part.text ("family") == bench.current().family);
        const bool hover = i == hoveredCard;

        g.setColour (isFitted ? Palette::accent.withAlpha (0.18f) : hover ? Palette::panelRaised.brighter (0.1f) : Palette::panelRaised);
        g.fillRoundedRectangle (card.toFloat(), 4.0f);
        g.setColour (isFitted ? Palette::accent : hover && auditioning ? Palette::secondary : Palette::edge);
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 4.0f, isFitted || (hover && auditioning) ? 1.5f : 1.0f);

        auto text = card.reduced (6, 3);
        g.setColour (slideOff ? Palette::textDisabled : Palette::textPrimary);
        g.setFont (Fonts::ui (12.0f, true));
        g.drawText (part.name, text.removeFromTop (18), juce::Justification::centredLeft, true);

        g.setFont (Fonts::ui (10.5f));
        g.setColour (Palette::textMuted);
        auto line = summaryOf (part);
        if (! part.suits (bench.current().family) && part.type != PartType::pick && part.type != PartType::slide
            && part.type != PartType::capo && part.type != PartType::numTypes)
            line = "Unusual here - " + line;
        if (! part.isFactory)
            line = "yours - " + line;
        g.drawText (line, text, juce::Justification::centredLeft, true);
    }

    // Section 9's empty user section.
    if (! anyUser && drawerParts.size() > 0 && y + cardH + 22 < inner.getBottom())
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (11.0f));
        g.drawText ("Your saved parts appear here. Edit any factory part and Save As to start.",
                    juce::Rectangle<int> (inner.getX(), y + cardH + 6, inner.getWidth(), 16), juce::Justification::centredLeft, true);
    }

    if (drawerParts.isEmpty())
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (12.0f));
        g.drawText ("No " + category.toLowerCase() + " parts are installed.", inner, juce::Justification::centred, true);
    }
}

void WorkshopPanel::paintInspector (juce::Graphics& g, juce::Rectangle<int> area)
{
    LuthierLookAndFeel::drawPanel (g, area.toFloat());
    auto inner = area.reduced (8).withTrimmedBottom (30);

    LuthierLookAndFeel::drawSectionHeader (g, inner.removeFromTop (22), "Inspector");
    inner.removeFromTop (4);

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (13.0f, true));
    g.drawFittedText (inspectorTitle, inner.removeFromTop (34), juce::Justification::topLeft, 2);

    g.setFont (Fonts::ui (11.5f));
    inspectorRows.clearQuick();

    for (int i = 0; i < inspectorLines.size(); ++i)
    {
        if (inner.getHeight() < 14)
            break;

        const auto& line = inspectorLines[i];
        const auto row = inner.removeFromTop (16);
        inspectorRows.add (row);

        const bool note = line.startsWith ("AUDITIONING") || line.startsWith ("Unusual");
        const bool editable = inspectorFields[i].isNotEmpty();
        g.setColour (note ? Palette::accent : editable ? Palette::textPrimary : Palette::textMuted);
        g.drawFittedText (line, row, juce::Justification::centredLeft, 1);
    }

    if (inspectorFields.joinIntoString ("").isNotEmpty() && inner.getHeight() > 16)
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (10.5f));
        g.drawFittedText ("Double-click a value to edit it. A factory part becomes your own copy.",
                          inner.removeFromTop (28), juce::Justification::topLeft, 2);
    }
}

void WorkshopPanel::paintSpectrum (juce::Graphics& g, juce::Rectangle<int> area)
{
    LuthierLookAndFeel::drawPanel (g, area.toFloat());
    auto inner = area.reduced (8);

    LuthierLookAndFeel::drawSectionHeader (g, inner.removeFromTop (20).withTrimmedRight (100), "Spectrum delta");
    auto summaryArea = inner.removeFromBottom (28);
    auto plot = inner.reduced (0, 4).toFloat();

    // Section 6: +-12 dB by default so a small change looks small.
    const float range = autoZoom ? juce::jlimit (1.0f, 24.0f, spectrum.largestDb * 1.25f + 0.25f) : 12.0f;

    auto xOf = [&plot] (float hz) { return plot.getX() + plot.getWidth() * std::log (hz / 60.0f) / std::log (12000.0f / 60.0f); };
    auto yOf = [&plot, range] (float db) { return plot.getCentreY() - (juce::jlimit (-range, range, db) / range) * plot.getHeight() * 0.5f; };

    g.setColour (Palette::edge);
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine ((int) xOf (hz), plot.getY(), plot.getBottom());
    g.setColour (Palette::edgeBright);
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

    g.setFont (Fonts::mono (9.0f));
    g.setColour (Palette::textMuted);
    g.drawText ("+" + juce::String (juce::roundToInt (range)) + " dB", plot.withHeight (10.0f), juce::Justification::topLeft, false);
    g.drawText ("-" + juce::String (juce::roundToInt (range)) + " dB", plot.withTrimmedTop (plot.getHeight() - 10.0f), juce::Justification::bottomLeft, false);

    for (float f : spectrum.combNotchesHz)
    {
        g.setColour (Palette::secondary.withAlpha (0.5f));
        g.drawVerticalLine ((int) xOf (f), plot.getY(), plot.getBottom());
    }

    if (! spectrum.deltaDb.empty())
    {
        juce::Path curve;
        for (size_t i = 0; i < spectrum.deltaDb.size(); ++i)
        {
            const juce::Point<float> p { xOf (spectrum.frequencies[i]), yOf (spectrum.deltaDb[i]) };
            if (i == 0) curve.startNewSubPath (p); else curve.lineTo (p);
        }

        g.setColour (Palette::accent);
        g.strokePath (curve, juce::PathStrokeType (1.6f));
    }

    g.setFont (Fonts::ui (11.0f));
    g.setColour (spectrum.noChange ? Palette::textMuted : Palette::textPrimary);
    g.drawFittedText (spectrum.summary.isNotEmpty() ? spectrum.summary
                                                    : juce::String ("Swap or audition a part to see what it changes."),
                      summaryArea, juce::Justification::centredLeft, 2);
}

} // namespace luthier
