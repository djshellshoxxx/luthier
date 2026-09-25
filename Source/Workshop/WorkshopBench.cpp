#include "WorkshopBench.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    int pickupIndexOf (GuitarSlot slot) noexcept
    {
        return slot == GuitarSlot::pickupNeck ? 0 : slot == GuitarSlot::pickupMiddle ? 1 : slot == GuitarSlot::pickupBridge ? 2 : -1;
    }

    const char* pickupName (int index) noexcept
    {
        return index == 0 ? "neck pickup" : index == 1 ? "middle pickup" : "bridge pickup";
    }

    /** workshop-ui.md 8's arrow: "Moved neck pickup 150 -> 142 mm", with a real arrow. */
    const juce::String& arrow()
    {
        static const juce::String a = juce::String::fromUTF8 ("\xe2\x86\x92");
        return a;
    }

    /** juce::String (v, 0) means "all the digits", not none, hence the rounding. */
    juce::String mm (double v, int decimals = 1)          { return decimals == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, decimals); }
    juce::String signedMm (double v)                      { return (v >= 0.0 ? "+" : "") + juce::String (v, 1); }

    /** Guitar strings are numbered for people from 1 = high E, as the engine counts from 0. */
    juce::String stringLabel (int engineIndex)            { return "string " + juce::String (engineIndex + 1); }

    double valueAt (const juce::Array<double>& values, int i, double fallback)
    {
        return juce::isPositiveAndBelow (i, values.size()) ? values[i] : fallback;
    }
}

//==============================================================================
WorkshopBench::WorkshopBench (LuthierAudioProcessor& p) : processor (p) {}

const WorkshopGuitar& WorkshopBench::current() const
{
    return gesture ? gesture->live : processor.getCurrentGuitar();
}

bool WorkshopBench::isModified() const
{
    return processor.isGuitarEdited();
}

juce::String WorkshopBench::describeSlot (GuitarSlot slot)
{
    switch (slot)
    {
        case GuitarSlot::pickupNeck:    return "neck pickup";
        case GuitarSlot::pickupMiddle:  return "middle pickup";
        case GuitarSlot::pickupBridge:  return "bridge pickup";
        case GuitarSlot::body:          return "body";
        case GuitarSlot::top:           return "top";
        case GuitarSlot::neck:          return "neck";
        case GuitarSlot::fretboard:     return "fretboard";
        case GuitarSlot::frets:         return "frets";
        case GuitarSlot::nut:           return "nut";
        case GuitarSlot::bridge:        return "bridge";
        case GuitarSlot::tailpiece:     return "tailpiece";
        case GuitarSlot::tuners:        return "tuners";
        case GuitarSlot::wiring:        return "wiring";
        case GuitarSlot::strings:       return "strings";
        case GuitarSlot::pickguard:     return "pickguard";
        case GuitarSlot::numSlots:      break;
    }

    return "part";
}

double WorkshopBench::pickupDepthMm (const Part* pickup, bool bass)
{
    if (pickup == nullptr)
        return 0.0;

    const auto family = pickup->text ("family", "single_coil");

    if (family == "humbucker")       return bass ? 40.0 : 39.0;
    if (family == "p90")             return 32.0;
    if (family == "mini_humbucker")  return 30.0;
    if (family == "active")          return 38.0;
    if (family == "split_coil")      return 48.0;
    if (family == "soundhole")       return 20.0;
    if (family == "piezo")           return 0.0;
    return bass ? 20.0 : 18.0;
}

//==============================================================================
void WorkshopBench::commit (const WorkshopGuitar& edited, const juce::String& description)
{
    processor.pushUndoState (description);
    processor.applyEditedGuitar (edited);
}

bool WorkshopBench::editField (GuitarSlot slot, const juce::String& field, const juce::var& value)
{
    const auto old = processor.getCurrentGuitar().get (slot);

    if (old == nullptr)
        return false;

    const auto before = old->fields.getProperty (field, {});

    if (before.toString() == value.toString())
        return false;

    // A factory part is never written to: the edit is a new, unsaved user part
    // (guitar-workshop.md 7), which "Save as user part" can then keep.
    auto copy = std::make_shared<Part> (*old);
    copy->fields = juce::JSON::parse (juce::JSON::toString (old->fields));
    copy->isFactory = false;
    copy->file = juce::File();

    if (auto* object = copy->fields.getDynamicObject())
        object->setProperty (field, value);
    else
        return false;

    return fit (slot, copy, "Set " + field.replaceCharacter ('_', ' ') + " of " + old->name + " "
                              + before.toString() + " " + arrow() + " " + value.toString());
}

bool WorkshopBench::fit (GuitarSlot slot, const PartPtr& part, const juce::String& customSentence)
{
    endAudition();

    const auto& committed = processor.getCurrentGuitar();
    const auto old = committed.get (slot);

    if (old == part || (old != nullptr && part != nullptr && old->name == part->name
                        && juce::JSON::toString (old->fields) == juce::JSON::toString (part->fields)))
        return false;

    if (part == nullptr && isSlotRequired (slot))
        return false;

    const auto edited = withPart (slot, part);
    const auto where = pickupIndexOf (slot) >= 0 ? " in the " + describeSlot (slot) + " slot" : juce::String();

    const auto sentence = customSentence.isNotEmpty() ? customSentence
        : part == nullptr
        ? "Removed " + describeSlot (slot) + (old != nullptr ? " (was " + old->name + ")" : juce::String())
        : "Fitted " + part->name + where + (old != nullptr ? " (was " + old->name + ")" : juce::String());

    commit (edited, sentence);

    // midi-export.md 6: WORKSHOP events, for a host recording the performance.
    processor.postWorkshopChange (getSlotId (slot), part != nullptr ? part->name : juce::String(),
                                  old != nullptr ? old->name : juce::String());
    return true;
}

bool WorkshopBench::fitAccessory (const PartPtr& part)
{
    if (part == nullptr)
        return false;

    if (part->type == PartType::capo)
    {
        const auto old = processor.getCapoPart();
        processor.pushUndoState ("Fitted " + part->name + (old != nullptr ? " (was " + old->name + ")" : juce::String()));
        processor.setCapoPart (part);
        return true;
    }

    if (part->type == PartType::pick)
    {
        processor.pushUndoState ("Fitted " + part->name + (pickPart != nullptr ? " (was " + pickPart->name + ")" : juce::String()));
        pickPart = part;

        // pick-noise.md 2 through the parameters it already has (DECISIONS: the
        // 12-choice material list stays; ultex plays as delrin, brass as metal).
        const auto m = part->text ("material", "celluloid");
        const int material = m == "nylon" ? 0 : m == "delrin" || m == "ultex" ? 2
                           : m == "brass" || m == "steel" || m == "metal" ? 3 : m == "wood" ? 4 : m == "felt" ? 5 : 1;
        setPlain (ParamIDs::pickMaterial, material);

        const double thickness = juce::jlimit (0.38, 3.0, part->number ("thickness_mm", 0.73));
        setPlain (ParamIDs::pickThickness, std::log (thickness / 0.38) / std::log (3.0 / 0.38));
        setPlain (ParamIDs::pickTipRadius, part->number ("tip_radius_mm", 1.0));
        setPlain (ParamIDs::pickBevel, part->number ("bevel", 0.2));
        setPlain (ParamIDs::pickWear, part->number ("wear", 0.1));
        return true;
    }

    if (part->type == PartType::slide)
    {
        processor.pushUndoState ("Fitted " + part->name + (slidePart != nullptr ? " (was " + slidePart->name + ")" : juce::String()));
        slidePart = part;
        // TODO(engine hook): processor.getEngine().setSlideBar (getSlideBar()) once the engine takes it.
        return true;
    }

    return false;
}

PartPtr WorkshopBench::getAccessory (PartType type) const
{
    if (type == PartType::capo)   return processor.getCapoPart();
    if (type == PartType::pick)   return pickPart;
    if (type == PartType::slide)  return slidePart;
    return nullptr;
}

//==============================================================================
juce::String WorkshopBench::describeString (const WorkshopGuitar& guitar, int stringIndex)
{
    // "0.018 plain", "0.026 wound", and the material when it is not the set's.
    auto text = juce::String (guitar.getStringGaugeIn (stringIndex), 3) + (guitar.isStringWound (stringIndex) ? " wound" : " plain");

    const auto set = guitar.get (GuitarSlot::strings);
    const auto setMaterial = set != nullptr ? set->text ("winding_material", "nickel_plated_steel") : juce::String ("nickel_plated_steel");

    if (guitar.getStringMaterial (stringIndex) != setMaterial)
        text << " " << guitar.getStringMaterial (stringIndex).replaceCharacter ('_', ' ');

    return text;
}

bool WorkshopBench::setStringOverride (const StringOverride& o)
{
    endAudition();

    const auto& committed = processor.getCurrentGuitar();

    if (! juce::isPositiveAndBelow (o.stringIndex, committed.getStringCount()))
        return false;

    auto edited = committed;
    edited.setStringOverride (o);

    if (edited == committed)
        return false;

    const auto was = describeString (committed, o.stringIndex);
    const auto now = describeString (edited, o.stringIndex);
    const bool cleared = edited.getStringOverride (o.stringIndex) == nullptr;

    commit (edited, (cleared ? "Cleared " + stringLabel (o.stringIndex) + " override, back to " + now
                             : "Set " + stringLabel (o.stringIndex) + " to " + now) + " (was " + was + ")");
    return true;
}

bool WorkshopBench::clearStringOverride (int stringIndex)
{
    StringOverride empty;
    empty.stringIndex = stringIndex;
    return setStringOverride (empty);
}

bool WorkshopBench::revert (GuitarSlot slot)
{
    // The slot as the guitar file has it, ignoring the preset's edits.
    WorkshopGuitar original;
    PartLibrary::LoadReport report;
    const auto file = processor.getGuitarFile();

    if (! file.existsAsFile() || ! processor.getPartLibrary().loadGuitar (file, original, report))
        return false;

    // A body goes back with the outline the file drew it with (a factory guitar
    // may draw a shared body part in its own style).
    if (slot == GuitarSlot::body)
        revertBodyStyle = original.bodyStyle;

    const bool changed = fit (slot, original.get (slot));
    revertBodyStyle.reset();
    return changed;
}

WorkshopGuitar WorkshopBench::withPart (GuitarSlot slot, const PartPtr& candidate) const
{
    auto g = processor.getCurrentGuitar();
    g.parts[(size_t) slot] = candidate;

    /*  guitar-illustration.md 4 / 18: a body brings its own outline. The guitar's
        body_style follows the fitted body, so the illustration draws it and the
        engine's body shape (PartAcoustics' shapeFor) hears it. A body that names
        no style (an older user part) leaves the guitar's as it was. */
    if (slot == GuitarSlot::body)
    {
        if (revertBodyStyle.has_value())
            g.bodyStyle = *revertBodyStyle;
        else if (const auto style = WorkshopGuitar::bodyStyleOf (candidate.get()); style.isNotEmpty())
            g.bodyStyle = style;
    }

    // A pickup going into an empty slot needs somewhere to sit: the usual
    // place for that slot, scaled to this guitar's scale length.
    if (const int i = pickupIndexOf (slot); i >= 0 && processor.getCurrentGuitar().get (slot) == nullptr && candidate != nullptr)
    {
        const double scale = g.get (GuitarSlot::neck) != nullptr ? g.get (GuitarSlot::neck)->number ("scale_length_mm", 648.0) : 648.0;
        const double usual[] = { 150.0, 95.0, 40.0 };
        g.placements[(size_t) i].positionMm = usual[i] * scale / 648.0;
        g.placements[(size_t) i].heightTrebleMm = 2.4;
        g.placements[(size_t) i].heightBassMm = 2.7;
    }

    return g;
}

//==============================================================================
void WorkshopBench::beginGesture()
{
    if (gesture)
        return;

    endAudition();

    Gesture g;
    g.before = processor.getCurrentGuitar();
    g.live = g.before;
    g.placementsBefore = readPlacements();
    gesture = std::move (g);
}

void WorkshopBench::endGesture()
{
    if (! gesture)
        return;

    const auto& before = gesture->before;
    const auto& after = gesture->live;
    juce::StringArray sentences;

    for (int i = 0; i < 3; ++i)
    {
        const auto& a = before.placements[(size_t) i];
        const auto& b = after.placements[(size_t) i];

        if (std::abs (a.positionMm - b.positionMm) > 0.01)
            sentences.add ("Moved " + juce::String (pickupName (i)) + " " + mm (a.positionMm, 0) + " " + arrow() + " " + mm (b.positionMm, 0) + " mm");

        auto height = [&] (const char* side, double from, double to)
        {
            if (std::abs (from - to) > 0.001)
                sentences.add (juce::String (to < from ? "Raised " : "Lowered ") + pickupName (i) + " " + side + " side "
                               + mm (from) + " " + arrow() + " " + mm (to) + " mm");
        };

        height ("treble", a.heightTrebleMm, b.heightTrebleMm);
        height ("bass", a.heightBassMm, b.heightBassMm);
    }

    const int n = juce::jmax (before.getStringCount(), after.getStringCount());

    for (int s = 0; s < n; ++s)
    {
        const double ia = valueAt (before.setup.intonationMm, s, 0.0), ib = valueAt (after.setup.intonationMm, s, 0.0);
        if (std::abs (ia - ib) > 0.001)
            sentences.add ("Moved " + stringLabel (s) + " saddle " + signedMm (ia) + " " + arrow() + " " + signedMm (ib) + " mm");

        const double na = valueAt (before.setup.nutSlotDepthsMm, s, 0.5), nb = valueAt (after.setup.nutSlotDepthsMm, s, 0.5);
        if (std::abs (na - nb) > 0.0001)
            sentences.add ("Set " + stringLabel (s) + " nut slot " + mm (na, 2) + " " + arrow() + " " + mm (nb, 2) + " mm");
    }

    // Paint previewed from the colour picker (one entry for the whole drag).
    sentences.addArray (describePaintChanges (before.finish, after.finish));

    // The accessories (pick, slide, capo) live in parameters, not the guitar.
    const auto placementsBefore = gesture->placementsBefore;
    const auto placementsAfter = readPlacements();
    const auto placementSentences = describePlacementChanges (placementsBefore, placementsAfter);
    sentences.addArray (placementSentences);

    const auto live = gesture->live;
    const bool guitarChanged = ! (live == before);
    gesture.reset();

    if (sentences.isEmpty())
        return;   // a click without a move changes nothing and pushes nothing

    if (! placementSentences.isEmpty())
    {
        /*  One entry carrying the state from before the drag: the parameters
            are put back for the push and set again after it. The audio thread
            reads them at its next block, so a block that happens to start
            between the two writes plays the old placement once (DECISIONS). */
        writePlacements (placementsBefore);
        processor.pushUndoState (sentences.joinIntoString ("; "));
        writePlacements (placementsAfter);

        if (guitarChanged)
            processor.applyEditedGuitar (live);

        return;
    }

    commit (live, sentences.joinIntoString ("; "));
}

//==============================================================================
namespace
{
    juce::String hexOf (juce::Colour c)   { return "#" + c.toDisplayString (false).toUpperCase(); }

    bool isBurst (const GuitarFinish& f)  { return f.type.equalsIgnoreCase ("burst"); }

    void paintInto (GuitarFinish& f, WorkshopBench::Paint which, juce::Colour colour)
    {
        switch (which)
        {
            case WorkshopBench::Paint::body:      (isBurst (f) ? f.colourB : f.colourA) = hexOf (colour); break;
            case WorkshopBench::Paint::burstEdge: f.colourA = hexOf (colour); break;
            case WorkshopBench::Paint::plastics:  f.plasticColour = hexOf (colour); break;
        }
    }
}

juce::String WorkshopBench::getPaint (Paint which) const
{
    const auto& f = current().finish;

    switch (which)
    {
        case Paint::body:      return isBurst (f) ? f.colourB : f.colourA;
        case Paint::burstEdge: return f.colourA;
        case Paint::plastics:  return f.plasticColour;
    }

    return {};
}

void WorkshopBench::previewPaint (Paint which, juce::Colour colour)
{
    endAudition();

    if (! gesture)
        beginGesture();

    paintInto (gesture->live.finish, which, colour);
}

bool WorkshopBench::setPaint (Paint which, juce::Colour colour)
{
    if (gesture)
    {
        // The drag's last value, then its one entry.
        paintInto (gesture->live.finish, which, colour);
        const bool changed = ! (gesture->live.finish.colourA == gesture->before.finish.colourA
                                && gesture->live.finish.colourB == gesture->before.finish.colourB
                                && gesture->live.finish.plasticColour == gesture->before.finish.plasticColour);
        endGesture();
        return changed;
    }

    endAudition();
    auto edited = processor.getCurrentGuitar();
    paintInto (edited.finish, which, colour);

    const auto sentences = describePaintChanges (processor.getCurrentGuitar().finish, edited.finish);

    if (sentences.isEmpty())
        return false;

    commit (edited, sentences.joinIntoString ("; "));
    return true;
}

bool WorkshopBench::clearPlastics()
{
    endGesture();

    auto edited = processor.getCurrentGuitar();

    if (edited.finish.plasticColour.isEmpty())
        return false;

    edited.finish.plasticColour.clear();
    commit (edited, describePaintChanges (processor.getCurrentGuitar().finish, edited.finish).joinIntoString ("; "));
    return true;
}

juce::StringArray WorkshopBench::finishPresetIds()
{
    return { "sunburst", "cherry", "black", "white", "seafoam", "natural" };
}

bool WorkshopBench::finishPreset (const juce::String& id, GuitarFinish& f)
{
    // guitar-illustration.md 11.1 and 11.2's own hexes.
    if (id == "sunburst")      { f.type = "burst"; f.colourA = "#3E2A1A"; f.colourB = "#E4D2A0"; f.burstShape = "radial"; }
    else if (id == "cherry")   { f.type = "burst"; f.colourA = "#9E2A1E"; f.colourB = "#F0DA8C"; f.burstShape = "radial"; }
    else if (id == "black")    { f.type = "solid"; f.colourA = "#0F0F0F"; }
    else if (id == "white")    { f.type = "solid"; f.colourA = "#F5F1E8"; }
    else if (id == "seafoam")  { f.type = "solid"; f.colourA = "#98C9B0"; }
    else if (id == "natural")  { f.type = "natural"; f.colourA = {}; }
    else                       return false;

    return true;
}

bool WorkshopBench::applyFinishPreset (const juce::String& id)
{
    endGesture();
    endAudition();

    auto edited = processor.getCurrentGuitar();

    if (! finishPreset (id, edited.finish))
        return false;

    const auto& before = processor.getCurrentGuitar().finish;

    if (describePaintChanges (before, edited.finish).isEmpty())
        return false;

    const auto was = before.type + (before.colourA.isNotEmpty() ? " " + before.colourA : juce::String())
                   + (isBurst (before) ? " / " + before.colourB : juce::String());
    commit (edited, "Finish " + id + " (was " + was + ")");
    return true;
}

juce::StringArray WorkshopBench::describePaintChanges (const GuitarFinish& a, const GuitarFinish& b)
{
    juce::StringArray out;
    auto shown = [] (const juce::String& hex, const char* empty) { return hex.isNotEmpty() ? hex : juce::String (empty); };

    if (! a.type.equalsIgnoreCase (b.type) || a.burstShape != b.burstShape)
        out.add ("Set finish " + a.type + " " + arrow() + " " + b.type);

    // A burst's body colour is its centre; its edge is colour_a.
    const bool burst = isBurst (b);

    if (burst ? a.colourB != b.colourB : a.colourA != b.colourA)
        out.add ("Set body colour " + shown (burst ? a.colourB : a.colourA, "wood") + " " + arrow() + " "
                 + shown (burst ? b.colourB : b.colourA, "wood"));

    if (burst && a.colourA != b.colourA)
        out.add ("Set burst edge colour " + shown (a.colourA, "wood") + " " + arrow() + " " + shown (b.colourA, "wood"));

    if (a.plasticColour != b.plasticColour)
        out.add ("Set plastics colour " + shown (a.plasticColour, "pickguard's own") + " " + arrow() + " "
                 + shown (b.plasticColour, "pickguard's own"));

    return out;
}

//==============================================================================
double WorkshopBench::readPlain (const char* id) const
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
        return p->convertFrom0to1 (p->getValue());

    return 0.0;
}

void WorkshopBench::setPlain (const char* id, double plain)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
    {
        const float normalised = p->convertTo0to1 ((float) plain);

        if (std::abs (normalised - p->getValue()) > 1.0e-7f)
            p->setValueNotifyingHost (normalised);
    }
}

double WorkshopBench::getScaleLengthMm() const
{
    const auto neck = current().get (GuitarSlot::neck);
    return neck != nullptr ? neck->number ("scale_length_mm", 648.0) : 648.0;
}

WorkshopBench::Placements WorkshopBench::readPlacements() const
{
    Placements p;
    p.pickPositionMm = readPlain (ParamIDs::pluckPosition) * getScaleLengthMm();
    p.pickAngle = Parameters::pickAngleDegrees (readPlain (ParamIDs::pickAngle));
    p.slideSlant = readPlain (ParamIDs::slideSlant);
    p.slideFret = slideFret;
    p.capoFret = juce::roundToInt (readPlain (ParamIDs::capoFret));
    return p;
}

void WorkshopBench::writePlacements (const Placements& p)
{
    setPlain (ParamIDs::pluckPosition, p.pickPositionMm / getScaleLengthMm());
    setPlain (ParamIDs::pickAngle, p.pickAngle / kMaxPickAngle);
    setPlain (ParamIDs::slideSlant, p.slideSlant);
    setPlain (ParamIDs::capoFret, (double) p.capoFret);
    slideFret = p.slideFret;
}

juce::StringArray WorkshopBench::describePlacementChanges (const Placements& a, const Placements& b) const
{
    juce::StringArray sentences;

    if (std::abs (a.pickPositionMm - b.pickPositionMm) > 0.05)
        sentences.add ("Moved pick " + mm (a.pickPositionMm, 0) + " " + arrow() + " " + mm (b.pickPositionMm, 0) + " mm from the saddle");

    if (std::abs (a.pickAngle - b.pickAngle) > 0.05)
        sentences.add ("Angled pick " + mm (a.pickAngle, 0) + " " + arrow() + " " + mm (b.pickAngle, 0) + " degrees");

    if (std::abs (a.slideFret - b.slideFret) > 0.005)
        sentences.add ("Moved slide to fret " + mm (b.slideFret, 1) + " (was " + mm (a.slideFret, 1) + ")");

    if (std::abs (a.slideSlant - b.slideSlant) > 0.05)
        sentences.add ("Slanted slide " + mm (a.slideSlant, 0) + " " + arrow() + " " + mm (b.slideSlant, 0) + " degrees");

    if (a.capoFret != b.capoFret)
        sentences.add (b.capoFret == 0 ? "Took the capo off (was fret " + juce::String (a.capoFret) + ")"
                     : a.capoFret == 0 ? "Put the capo on fret " + juce::String (b.capoFret)
                     : "Moved capo fret " + juce::String (a.capoFret) + " " + arrow() + " " + juce::String (b.capoFret));

    return sentences;
}

double WorkshopBench::getPickPositionMm() const     { return readPlacements().pickPositionMm; }
double WorkshopBench::getPickAngleDegrees() const   { return readPlacements().pickAngle; }
double WorkshopBench::getSlideSlantDegrees() const  { return readPlain (ParamIDs::slideSlant); }
int WorkshopBench::getCapoFret() const              { return juce::roundToInt (readPlain (ParamIDs::capoFret)); }

void WorkshopBench::setPickPlacement (double positionMm, double angleDegrees)
{
    const bool own = ! gesture;
    beginGesture();

    // The pick_position parameter runs 0.02 - 0.5 of the scale (Parameters.cpp).
    const double scale = getScaleLengthMm();
    setPlain (ParamIDs::pluckPosition, juce::jlimit (0.02, 0.5, positionMm / scale));
    setPlain (ParamIDs::pickAngle, juce::jlimit (0.0, 1.0, angleDegrees / kMaxPickAngle));

    if (own)
        endGesture();
}

void WorkshopBench::setSlidePlacement (double fret, double slantDegrees)
{
    const bool own = ! gesture;
    beginGesture();

    const auto neck = current().get (GuitarSlot::neck);
    const double frets = neck != nullptr ? neck->number ("frets", 22.0) : 22.0;
    slideFret = juce::jlimit (0.0, frets, fret);
    setPlain (ParamIDs::slideSlant, juce::jlimit (-kMaxSlideSlant, kMaxSlideSlant, slantDegrees));

    // TODO(engine hook): a "bench" position source for SlideEngine (slide-technique-controls.md, 5b).

    if (own)
        endGesture();
}

void WorkshopBench::setCapoFret (int fret)
{
    const bool own = ! gesture;
    beginGesture();

    setPlain (ParamIDs::capoFret, (double) juce::jlimit (0, kMaxCapoFret, fret));

    if (own)
        endGesture();
}

SlideBar WorkshopBench::getSlideBar() const
{
    SlideBar bar;

    if (slidePart == nullptr)
        return bar;

    const auto m = slidePart->text ("material", "glass");
    bar.material = m == "glass_thick" ? SlideMaterial::glassThick : m == "brass" ? SlideMaterial::brass
                 : m == "steel" ? SlideMaterial::steel : m == "ceramic" ? SlideMaterial::ceramic
                 : m == "bone" ? SlideMaterial::bone : m == "dobro_bar" ? SlideMaterial::dobroBar : SlideMaterial::glass;
    bar.massGrams = slidePart->number ("mass_g", 65.0);
    bar.lengthMm = slidePart->number ("length_mm", 70.0);
    bar.diameterMm = slidePart->number ("diameter_mm", 22.0);
    return bar;
}

WorkshopBench::Travel WorkshopBench::getPickupTravel (int index) const
{
    Travel t;
    const auto& g = current();
    const bool bass = g.family == "bass";

    const auto self = g.get (WorkshopGuitar::pickupSlot (index));
    const double half = pickupDepthMm (self.get(), bass) * 0.5;

    // Toward the bridge: the bridge, or the next pickup that way.
    t.min = half + 12.0;
    t.belowMin = "it would run into the bridge";

    for (int j = index + 1; j < 3; ++j)
        if (auto other = g.get (WorkshopGuitar::pickupSlot (j)); other != nullptr && other->text ("family") != "piezo")
        {
            t.min = g.placements[(size_t) j].positionMm + pickupDepthMm (other.get(), bass) * 0.5 + half + 2.0;
            t.belowMin = "it would hit the " + juce::String (pickupName (j));
            break;
        }

    // Toward the neck: the next pickup that way, or where the fretboard ends.
    const auto neck = g.get (GuitarSlot::neck);
    const double scale = neck != nullptr ? neck->number ("scale_length_mm", 648.0) : 648.0;
    const int frets = neck != nullptr ? (int) neck->number ("frets", 22.0) : 22;
    // The board runs about 6 mm past its last fret; a neck pickup's ring may tuck
    // a few millimetres under it, as a Les Paul's does.
    const double fretboardEnd = scale * std::pow (2.0, -(double) frets / 12.0) - 6.0;

    t.max = fretboardEnd - half + 4.0;
    t.aboveMax = "the fretboard ends there";

    for (int j = index - 1; j >= 0; --j)
        if (auto other = g.get (WorkshopGuitar::pickupSlot (j)); other != nullptr && other->text ("family") != "piezo")
        {
            t.max = g.placements[(size_t) j].positionMm - pickupDepthMm (other.get(), bass) * 0.5 - half - 2.0;
            t.aboveMax = "it would hit the " + juce::String (pickupName (j));
            break;
        }

    if (t.max < t.min)
        t.max = t.min;

    return t;
}

double WorkshopBench::movePickup (int index, double positionMm, juce::String* stoppedBecause)
{
    if (! juce::isPositiveAndBelow (index, 3) || current().get (WorkshopGuitar::pickupSlot (index)) == nullptr)
        return 0.0;

    const bool own = ! gesture;
    beginGesture();

    const auto travel = getPickupTravel (index);
    const double clamped = juce::jlimit (travel.min, travel.max, positionMm);

    if (stoppedBecause != nullptr)
        *stoppedBecause = positionMm < travel.min - 1.0e-6 ? travel.belowMin
                        : positionMm > travel.max + 1.0e-6 ? travel.aboveMax : juce::String();

    gesture->live.placements[(size_t) index].positionMm = clamped;
    applyLive();

    if (own)
        endGesture();

    return clamped;
}

void WorkshopBench::setPickupHeights (int index, double trebleMm, double bassMm)
{
    if (! juce::isPositiveAndBelow (index, 3) || current().get (WorkshopGuitar::pickupSlot (index)) == nullptr)
        return;

    const bool own = ! gesture;
    beginGesture();

    auto& p = gesture->live.placements[(size_t) index];
    p.heightTrebleMm = juce::jlimit (kMinPickupHeight, kMaxPickupHeight, trebleMm);
    p.heightBassMm = juce::jlimit (kMinPickupHeight, kMaxPickupHeight, bassMm);
    applyLive();

    if (own)
        endGesture();
}

void WorkshopBench::setIntonation (int stringIndex, double mmValue)
{
    if (! juce::isPositiveAndBelow (stringIndex, current().getStringCount()))
        return;

    const bool own = ! gesture;
    beginGesture();

    auto& values = gesture->live.setup.intonationMm;
    while (values.size() <= stringIndex)
        values.add (0.0);

    values.set (stringIndex, juce::jlimit (-kMaxIntonation, kMaxIntonation, mmValue));

    if (own)
        endGesture();
}

void WorkshopBench::setNutSlotDepth (int stringIndex, double mmValue)
{
    if (! juce::isPositiveAndBelow (stringIndex, current().getStringCount()))
        return;

    const bool own = ! gesture;
    beginGesture();

    auto& values = gesture->live.setup.nutSlotDepthsMm;
    while (values.size() <= stringIndex)
        values.add (0.5);

    values.set (stringIndex, juce::jlimit (0.0, kMaxNutSlot, mmValue));

    if (own)
        endGesture();
}

void WorkshopBench::applyLive()
{
    // A pickup that moves is heard moving (ground rule 3), without a swap per step.
    // The engine numbers its pickups from the bridge (PartAcoustics.cpp).
    const auto& g = gesture->live;
    const auto neck = g.get (GuitarSlot::neck);
    const double scale = neck != nullptr ? neck->number ("scale_length_mm", 648.0) : 648.0;
    int engineSlot = 0;

    for (int i = 2; i >= 0; --i)
    {
        auto p = g.get (WorkshopGuitar::pickupSlot (i));

        if (p == nullptr || p->text ("family") == "piezo")
            continue;

        const auto& pl = g.placements[(size_t) i];
        processor.getEngine().setPickupPlacementLive (engineSlot++, pl.positionMm / scale,
                                                      0.5 * (pl.heightTrebleMm + pl.heightBassMm));
    }
}

//==============================================================================
bool WorkshopBench::hasSlot (int index) const
{
    return juce::isPositiveAndBelow (index, kNumSlots) && ! processor.getUiState().benchSlots[(size_t) index].isVoid();
}

void WorkshopBench::storeSlot (int index)
{
    if (juce::isPositiveAndBelow (index, kNumSlots))
        processor.getUiState().benchSlots[(size_t) index] = processor.getCurrentGuitar().toEmbeddedVar();
}

bool WorkshopBench::recallSlot (int index)
{
    if (! hasSlot (index))
        return false;

    endAudition();

    WorkshopGuitar stored;
    PartLibrary::LoadReport report;

    if (! processor.getPartLibrary().buildGuitar (processor.getUiState().benchSlots[(size_t) index], stored, report))
        return false;

    commit (stored, "Recalled bench slot " + slotName (index));
    return true;
}

void WorkshopBench::clearSlot (int index)
{
    if (juce::isPositiveAndBelow (index, kNumSlots))
        processor.getUiState().benchSlots[(size_t) index] = juce::var();
}

//==============================================================================
void WorkshopBench::beginAudition (GuitarSlot slot, const PartPtr& candidate)
{
    if (gesture)
        return;

    audition = withPart (slot, candidate);
    processor.auditionGuitar (&*audition);
}

void WorkshopBench::endAudition()
{
    if (! audition)
        return;

    audition.reset();
    processor.auditionGuitar (nullptr);
}

} // namespace luthier
