#include "TechniquePages.h"
#include "../../PluginProcessor.h"

namespace luthier
{

//==============================================================================
ControlFlow::ControlFlow (LuthierAudioProcessor& p)
    : processor (p)
{
}

void ControlFlow::addHeading (const juce::String& text)
{
    items.push_back ({ nullptr, 0, 22, true, false, text });
}

LuthierKnob& ControlFlow::addKnob (const char* id, const juce::String& label, const juce::String& tooltip)
{
    auto* knob = new LuthierKnob (label, LuthierKnob::Size::Small);
    owned.add (knob);
    knob->attachTo (processor, id, tooltip);
    addAndMakeVisible (knob);
    parameterIds.add (id);
    items.push_back ({ knob, 74, LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small), false, false, {} });
    return *knob;
}

LuthierChoice& ControlFlow::addChoice (const char* id, const juce::String& label, const juce::String& tooltip)
{
    auto* choice = new LuthierChoice (label);
    owned.add (choice);
    choice->attachTo (processor, id, tooltip);
    addAndMakeVisible (choice);
    parameterIds.add (id);
    items.push_back ({ choice, 150, 40, false, false, {} });
    return *choice;
}

LuthierToggle& ControlFlow::addToggle (const char* id, const juce::String& label, const juce::String& tooltip)
{
    auto* toggle = new LuthierToggle (label);
    owned.add (toggle);
    toggle->attachTo (processor, id, tooltip);
    addAndMakeVisible (toggle);
    parameterIds.add (id);
    items.push_back ({ toggle, 150, Metrics::buttonHeight, false, false, {} });
    return *toggle;
}

juce::TextButton& ControlFlow::addButton (const juce::String& text, const juce::String& tooltip, std::function<void()> onClick)
{
    auto* button = new juce::TextButton (text);
    owned.add (button);
    button->setTooltip (tooltip);
    button->onClick = std::move (onClick);
    AccessibleSetup::configureButton (*button, text, tooltip);
    addAndMakeVisible (button);
    items.push_back ({ button, 150, Metrics::buttonHeight, false, false, {} });
    return *button;
}

void ControlFlow::addOwnedWide (juce::Component* component, int height)
{
    owned.add (component);
    addWide (*component, height);
}

void ControlFlow::addWide (juce::Component& component, int height)
{
    addAndMakeVisible (component);
    items.push_back ({ &component, 0, height, false, true, {} });
}

juce::StringArray ControlFlow::getParameterIds() const
{
    return parameterIds;
}

int ControlFlow::layout (int width, bool apply)
{
    const int gap = Metrics::gridHalf;
    int x = 0, y = 0, rowHeight = 0;

    if (apply)
        headings.clear();

    auto newRow = [&]
    {
        if (x > 0)
        {
            y += rowHeight + gap;
            x = 0;
            rowHeight = 0;
        }
    };

    for (const auto& item : items)
    {
        if (item.heading || item.wide)
        {
            newRow();
            const juce::Rectangle<int> r (0, y, width, item.height);

            if (apply)
            {
                if (item.heading)
                    headings.add ({ r, item.text });
                else
                    item.component->setBounds (r);
            }

            y += item.height + gap;
            continue;
        }

        if (x > 0 && x + item.width > width)
            newRow();

        if (apply)
            item.component->setBounds (x, y, juce::jmin (item.width, width), item.height);

        x += item.width + gap;
        rowHeight = juce::jmax (rowHeight, item.height);
    }

    newRow();
    return y;
}

int ControlFlow::getHeightForWidth (int width) const
{
    return const_cast<ControlFlow*> (this)->layout (juce::jmax (80, width), false);
}

void ControlFlow::resized()
{
    layout (getWidth(), true);
}

void ControlFlow::paint (juce::Graphics& g)
{
    for (const auto& [area, text] : headings)
        LuthierLookAndFeel::drawSectionHeader (g, area, text);
}

//==============================================================================
//  SCRAPE (string-scraping.md 2)
//==============================================================================
ScrapePage::ScrapePage (LuthierAudioProcessor& p)
    : ControlFlow (p)
{
    addHeading ("TRIGGER");
    addChoice (ParamIDs::scrapeTrigger, "Trigger", "What starts a scrape: keyswitch 12, a CC held, notes on the MPE zone, or the button only");
    addKnob (ParamIDs::scrapeTriggerCc, "Trigger CC", "The CC that starts a scrape (Trigger: CC)");
    addChoice (ParamIDs::scrapeDirection, "Direction", "Bridge to nut, nut to bridge, or hold and sweep with a controller");
    addChoice (ParamIDs::scrapeSweepSource, "Sweep source", "What moves the pick in Hold + Sweep");
    addKnob (ParamIDs::scrapeSweepCc, "Sweep CC", "The CC that sweeps (Sweep source: Custom CC)");
    addKnob (ParamIDs::scrapeRetrigger, "Retrigger", "The shortest gap before another scrape can start");
    addButton ("Scrape now", "Plays one scrape with these settings (while armed)",
               [&p] { p.getEngine().getScrapeEngine().requestTrigger(); });

    addHeading ("GESTURE");
    addKnob (ParamIDs::scrapeStartMm, "Start", "Where the pick starts, mm from the bridge");
    addKnob (ParamIDs::scrapeEndMm, "End", "Where it stops, mm from the bridge");
    addKnob (ParamIDs::scrapeDuration, "Duration", "How long the scrape takes");
    addKnob (ParamIDs::scrapePressure, "Pressure", "How hard the edge is pressed into the windings");
    addKnob (ParamIDs::scrapeAngle, "Angle", "The pick's angle to the string; flatter catches more windings");
    addKnob (ParamIDs::pickScrapeAmount, "Level", "The scrape's level");
    addChoice (ParamIDs::scrapeTool, "Tool", "Pick edge, fingernail or thumb");

    addHeading ("STRINGS");
    auto* mask = new StringMaskSelector (p, ParamIDs::scrapeStringMask);
    mask->setTooltip ("Which strings the scrape crosses; none selected follows the held strings");
    addOwnedWide (mask, 26);
}

//==============================================================================
//  SLIDE (slide-technique-controls.md 1)
//==============================================================================
SlidePage::SlidePage (LuthierAudioProcessor& p)
    : ControlFlow (p)
{
    addHeading ("BAR POSITION");
    addChoice (ParamIDs::slidePosSource, "Position source",
               "What moves the bar: the mod wheel, pitch bend, MPE Y, expression, a CC or a drag on the fretboard. "
               "None: the notes you play move it, as before.");
    addKnob (ParamIDs::slidePosCc, "Position CC", "The CC that moves the bar (Position source: Custom CC)");
    addChoice (ParamIDs::slidePosMode, "Position mode", "Absolute: 0-1 is fret 0-24. Relative: the control adds to the fretted note.");
    addKnob (ParamIDs::slidePosRange, "Relative range", "How far a full relative throw moves the bar, in frets");
    addKnob (ParamIDs::slideSpeedLimit, "Speed limit", "The fastest the bar can travel, cents per second (4800 is an octave a second)");
    addChoice (ParamIDs::slideContact, "Contact strings", "Which strings the bar touches; the rest stay free to fret and pluck");

    addHeading ("SLANT AND PRESSURE");
    addChoice (ParamIDs::slideSlantSource, "Slant source", "A controller that tilts the bar, +/-30 degrees");
    addKnob (ParamIDs::slideSlantCc, "Slant CC", "The CC for slant (Custom CC)");
    addChoice (ParamIDs::slidePressureSource, "Pressure source", "A controller that presses the bar");
    addKnob (ParamIDs::slidePressureCc, "Pressure CC", "The CC for pressure (Custom CC)");

    addHeading ("AUTO-VIBRATO ON HOLD");
    addToggle (ParamIDs::slideAutoVibrato, "Auto-vibrato", "Held still for 300 ms, the bar starts a subtle vibrato");
    addKnob (ParamIDs::slideAutoVibDepth, "Depth", "Auto-vibrato depth, cents");
    addKnob (ParamIDs::slideAutoVibRate, "Rate", "Auto-vibrato rate");

    addHeading ("SCRIPTED GESTURE");
    addChoice (ParamIDs::slideGestureTrigger, "Trigger", "Keyswitch 21, or a CC, runs the slide below");
    addKnob (ParamIDs::slideGestureCc, "Trigger CC", "The CC that runs it (Trigger: CC)");
    addKnob (ParamIDs::slideGestureFrom, "From", "Start fret (continuous)");
    addKnob (ParamIDs::slideGestureTo, "To", "End fret (continuous)");
    addKnob (ParamIDs::slideGestureTime, "Time", "How long the slide takes");
    addChoice (ParamIDs::slideGestureCurve, "Curve", "Linear, ease in, ease out, ease in-out");
    addKnob (ParamIDs::slideGestureSlantStart, "Slant from", "Bar slant at the start");
    addKnob (ParamIDs::slideGestureSlantEnd, "Slant to", "Bar slant at the end");
    addKnob (ParamIDs::slideGesturePressure, "Pressure", "Bar pressure during the gesture");
    addButton ("Run gesture", "Plays the scripted slide now (Slide Mode on)",
               [&p] { p.getEngine().getSlideEngine().requestGesture(); });
}

//==============================================================================
//  SLAP (string-slap-technique.md 1; bass-techniques.md 2-5)
//==============================================================================
SlapPage::SlapPage (LuthierAudioProcessor& p)
    : ControlFlow (p)
{
    addHeading ("TYPE AND TRIGGER");
    addChoice (ParamIDs::slapType, "Slap type", "Thumb slap, finger pop, palm slap or body tap");
    addChoice (ParamIDs::slapTrigger, "Trigger", "Keyswitch 15 (16 ghost, 17 body tap, 18 palm slap), a CC, the MPE zone, the button, or a velocity zone");
    addKnob (ParamIDs::slapVelocityZone, "Velocity zone", "Notes at or above this velocity are slapped (Trigger: Velocity Zone)");
    addKnob (ParamIDs::slapTriggerCc, "Trigger CC", "The CC that holds the slap on (Trigger: CC)");
    addKnob (ParamIDs::slapGhostCc, "Ghost CC", "The CC that holds ghost mode");
    addButton ("Slap now", "Strikes the selected strings with this slap (while armed)", [&p]
    {
        p.getEngine().getTechniqueTriggers().request (TechniqueId::slap, 0, true);
        p.getEngine().getTechniqueTriggers().request (TechniqueId::slap, 0, false);
    });

    addHeading ("THE HAND");
    addKnob (ParamIDs::slapForce, "Force", "How hard the hand lands");
    addKnob (ParamIDs::slapPalmPositionMm, "Palm position", "Where a palm slap lands, mm from the bridge");
    addToggle (ParamIDs::slapGhostMode, "Ghost mode", "Every slapped note is a ghost: the fretting hand only rests on the string");
    addKnob (ParamIDs::slapReboundGap, "Rebound gap", "The double thump's gap between down and up strokes");
    addKnob (ParamIDs::slapSnapBack, "Snap-back", "How hard a pop snaps back onto the frets");
    addChoice (ParamIDs::slapBodyPart, "Body tap resonance", "Where a body tap lands: top, side or back");

    addHeading ("THUMB AND POP");
    addKnob (ParamIDs::slapStrength, "Slap strength", "The thumb's strike");
    addKnob (ParamIDs::slapPositionMm, "Slap position", "Where the thumb lands, mm from the bridge end of the neck");
    addKnob (ParamIDs::slapThumbHardness, "Thumb hardness", "Soft pad to hard bone");
    addKnob (ParamIDs::slapFretContact, "Fret contact", "How much of the clack is the string hitting the frets");
    addKnob (ParamIDs::popStrength, "Pop strength", "The finger's pull");
    addKnob (ParamIDs::popPositionMm, "Pop position", "Where the finger pulls, mm");
    addToggle (ParamIDs::doubleThumpEnabled, "Double thump", "A thumb down-stroke and an up-stroke on the way back");
    addKnob (ParamIDs::doubleThumpUpRatio, "Up ratio", "The up-stroke's strength against the down");

    addHeading ("GHOSTS");
    addKnob (ParamIDs::ghostLevel, "Ghost level", "How loud a ghost note is");
    addKnob (ParamIDs::ghostDamping, "Ghost damping", "How dead a ghost is");
    addToggle (ParamIDs::ghostAuto, "Auto ghost", "Soft notes become ghosts");
    addKnob (ParamIDs::ghostVelocityThreshold, "Ghost below", "The velocity under which a note is a ghost (Auto ghost)");

    addHeading ("STRINGS");
    auto* mask = new StringMaskSelector (p, ParamIDs::slapStringMask);
    mask->setTooltip ("Which strings the button and a palm slap strike; none selected follows the held strings");
    addOwnedWide (mask, 26);
}

//==============================================================================
//  MUTE (muting-rhythm.md 3)
//==============================================================================
MutePage::MutePage (LuthierAudioProcessor& p)
    : ControlFlow (p), group (p)
{
    addWide (group, MuteGroup::preferredHeight);
}

//==============================================================================
//  TAP (two-hand-tapping.md 3)
//==============================================================================
TapPage::TapPage (LuthierAudioProcessor& p)
    : ControlFlow (p)
{
    addHeading ("TRIGGER");
    addChoice (ParamIDs::tapSource, "Tap trigger",
               "Right-hand notes on their own MIDI channel, the notes under keyswitch 19, or clicks on the fretboard");
    addKnob (ParamIDs::tapChannel, "Tap channel", "The right hand's MIDI channel (Tap trigger: MIDI Channel)");
    addKnob (ParamIDs::tapStrengthCurve, "Strength curve", "Velocity to tap strength: negative is a soft touch, positive a hard one, 0 linear");

    addHeading ("THE TAP");
    addToggle (ParamIDs::tapAutoPullOff, "Auto pull-off", "Lifting a tap with a fretted note held plucks the string on the way off");
    addKnob (ParamIDs::tapFlick, "Release flick", "How strongly the lifting finger flicks the string sideways");
    addKnob (ParamIDs::tapDuration, "Duration", "How long a fretboard or scripted tap holds before it lifts");
    addKnob (ParamIDs::tapMaxConcurrent, "Max taps", "Taps held at once on one string; the oldest lifts past this");
    addToggle (ParamIDs::tapFretSnap, "Fret snap", "Taps land on fret centres; off, the fretboard taps where you click");

    addHeading ("LEFT HAND");
    addKnob (ParamIDs::tapHammerThreshold, "Hammer-on below",
             "A note on a ringing string within 150 ms and under this velocity is a hammer-on, not a pick");
}

//==============================================================================
//  BEND (microtonal-bends.md 2)
//==============================================================================
BendCurveEditor::BendCurveEditor (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("The drawn bend curve (Bend curve or Release curve: Drawn). Drag a point.");
    AccessibleSetup::configureDescriptive (*this, "Drawn bend curve", "Five points from rest to full throw");
    startTimerHz (10);
}

BendCurveEditor::~BendCurveEditor()
{
    stopTimer();
}

void BendCurveEditor::setPoint (int index, double value)
{
    auto& bend = processor.getEngine().getTechniqueLayer().bend;

    if (std::abs (bend.getDrawnCurvePoint (index) - value) < 1.0e-6)
        return;

    processor.pushUndoState ("Draw bend curve");
    bend.setDrawnCurvePoint (index, value);
    repaint();
}

int BendCurveEditor::pointAt (juce::Point<float> pos) const
{
    const auto area = getLocalBounds().toFloat().reduced (6.0f);
    const int n = BendEngine::kCurvePoints;
    const int i = juce::roundToInt ((pos.x - area.getX()) / area.getWidth() * (float) (n - 1));
    return juce::jlimit (0, n - 1, i);
}

void BendCurveEditor::mouseDown (const juce::MouseEvent& e)
{
    dragging = pointAt (e.position);
    mouseDrag (e);
}

void BendCurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const auto area = getLocalBounds().toFloat().reduced (6.0f);
    const double v = juce::jlimit (0.0, 1.0, (double) ((area.getBottom() - e.position.y) / area.getHeight()));
    auto& bend = processor.getEngine().getTechniqueLayer().bend;

    // One undo entry for a whole drag.
    if (e.mouseWasDraggedSinceMouseDown())
        bend.setDrawnCurvePoint (dragging, v);
    else
        setPoint (dragging, v);

    repaint();
}

void BendCurveEditor::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (6.0f);
    LuthierLookAndFeel::drawPanel (g, getLocalBounds().toFloat());

    const auto& bend = processor.getEngine().getTechniqueLayer().bend;
    juce::Path path;

    for (int i = 0; i < BendEngine::kCurvePoints; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) (BendEngine::kCurvePoints - 1);
        const float y = area.getBottom() - area.getHeight() * (float) bend.getDrawnCurvePoint (i);

        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);

        g.setColour (Palette::accentBright);
        g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
    }

    g.setColour (Palette::accent);
    g.strokePath (path, juce::PathStrokeType (1.5f));
}

BendPage::BendPage (LuthierAudioProcessor& p)
    : ControlFlow (p), curve (p)
{
    addHeading ("GLOBAL BEND");
    addChoice (ParamIDs::bendGlobalSource, "Global source", "Pitch bend, expression or a CC bends every string alike");
    addKnob (ParamIDs::bendGlobalCc, "Global CC", "The CC (Global source: Custom CC)");
    addKnob (ParamIDs::bendGlobalRange, "Global range", "A full throw in cents; 200 is a whole tone");

    addHeading ("PER STRING");
    addChoice (ParamIDs::bendStringSource, "String source", "Each string's own bend: MPE per-note pitch bend, MPE Y, a CC per string, or none");
    addKnob (ParamIDs::bendStringCc, "First CC", "Custom CC: string 1 uses this CC, string 2 the next, and so on");

    for (int n = 1; n <= 6; ++n)
    {
        static const juce::String ids[6] = { ParamIDs::bendStringRange (1), ParamIDs::bendStringRange (2), ParamIDs::bendStringRange (3),
                                             ParamIDs::bendStringRange (4), ParamIDs::bendStringRange (5), ParamIDs::bendStringRange (6) };
        addKnob (ids[n - 1].toRawUTF8(), "String " + juce::String (n),
                 "String " + juce::String (n) + "'s bend range, cents. Takes effect at its next note.");
    }

    addHeading ("VIBRATO");
    addChoice (ParamIDs::bendVibratoSource, "Vibrato source", "An automatic LFO, aftertouch or MPE Z");
    addKnob (ParamIDs::bendVibratoRate, "Rate", "Vibrato rate");
    addKnob (ParamIDs::bendVibratoDepth, "Depth", "Vibrato depth, cents");
    addKnob (ParamIDs::bendVibratoOnset, "Onset delay", "How long after a note before the vibrato starts");

    addHeading ("QUANTISE");
    addChoice (ParamIDs::bendQuantise, "Quantise", "Pull held bends to a grid: quarter-tones, semitones, 22/24/31/53-EDO, or a loaded scale");
    addKnob (ParamIDs::bendSnap, "Snap", "1 lands exactly on the grid; lower is only an attraction");
    addButton ("Load scale...", "Loads a Scala .scl or AnaMark .tun file as the custom scale", [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a scale", juce::File(), "*.scl;*.tun");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult() != juce::File())
                loadScale (fc.getResult());
        });
    });

    scaleStatus.setFont (Fonts::ui (11.0f));
    scaleStatus.setColour (juce::Label::textColourId, Palette::textMuted);
    scaleStatus.setText ("Custom scale: " + p.getEngine().getTechniqueLayer().bend.getCustomScale().getName(),
                         juce::dontSendNotification);
    addWide (scaleStatus, 18);

    addHeading ("PRE-BEND");
    addKnob (ParamIDs::bendPreBendAmount, "Amount", "Where a pre-bent note starts, in cents from its pitch");
    addChoice (ParamIDs::bendPreBendTrigger, "Trigger", "Keyswitch 20, or a CC, pre-bends the next note");
    addKnob (ParamIDs::bendPreBendCc, "Trigger CC", "The CC (Trigger: CC)");
    addKnob (ParamIDs::bendPreBendRelease, "Release", "How long it takes to release to pitch");
    addButton ("Pre-bend next note", "The next note starts pre-bent",
               [&p] { p.getEngine().getTechniqueLayer().bend.requestPreBend(); });

    addHeading ("CURVES");
    addChoice (ParamIDs::bendCurve, "Bend curve", "How the pitch follows the control: linear, exponential (finger mechanics) or drawn");
    addChoice (ParamIDs::bendReleaseCurve, "Release curve", "The shape of a release and of vibrato onset");
    addWide (curve, 70);
}

bool BendPage::loadScale (const juce::File& file)
{
    MicrotonalScale scale;
    juce::String error;

    if (! scale.loadFile (file, error))
    {
        scaleStatus.setText ("Could not load " + file.getFileName() + ": " + error, juce::dontSendNotification);
        scaleStatus.setColour (juce::Label::textColourId, Palette::warning);
        return false;
    }

    processor.pushUndoState ("Load bend scale");
    processor.getEngine().getTechniqueLayer().bend.setCustomScale (scale);

    scaleStatus.setText ("Custom scale: " + scale.getName() + " (" + juce::String (scale.getNumPoints()) + " pitches)",
                         juce::dontSendNotification);
    scaleStatus.setColour (juce::Label::textColourId, Palette::textMuted);
    return true;
}

//==============================================================================
//  CASCADE (technique-cascade.md 6)
//==============================================================================
CascadeView::CascadeView (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Which techniques are live on which string. A lit cell is sounding now; "
                "a red row conflicts with another armed technique.");
    AccessibleSetup::configureDescriptive (*this, "Cascade overview", "Techniques down, strings across");
    refresh();
    startTimerHz (30);
}

CascadeView::~CascadeView()
{
    stopTimer();
}

bool CascadeView::isActiveCell (TechniqueSlot slot, int string) const noexcept
{
    if (! juce::isPositiveAndBelow (string, kMaxStrings))
        return false;

    return (activity[(size_t) string] & (1 << (int) TechniqueTable::get (slot).cascade)) != 0;
}

void CascadeView::refresh()
{
    auto& engine = processor.getEngine();
    numStrings = engine.getNumStrings();

    conflicts.clear();

    for (int t = 0; t < TechniqueTable::count; ++t)
    {
        armed[(size_t) t] = TechniqueTable::isArmed (processor, (TechniqueSlot) t);

        if (const auto c = TechniqueTable::conflictFor (processor, (TechniqueSlot) t); c.isNotEmpty())
            conflicts.add (juce::String (TechniqueTable::get ((TechniqueSlot) t).spokenName) + ": " + c);
    }

    for (int s = 0; s < kMaxStrings; ++s)
        activity[(size_t) s] = engine.getTechniqueLayer().cascade.getPublishedMask (s);

    repaint();
}

void CascadeView::paint (juce::Graphics& g)
{
    LuthierLookAndFeel::drawPanel (g, getLocalBounds().toFloat());

    auto area = getLocalBounds().reduced (6);

    // technique-cascade.md 6: the conflicts, in words, under the grid.
    auto notes = area.removeFromBottom (juce::jmin (area.getHeight() / 3, 14 * juce::jmax (1, conflicts.size())));
    g.setFont (Fonts::ui (11.0f));

    if (conflicts.isEmpty())
    {
        g.setColour (Palette::textMuted);
        g.drawText ("No conflicts: everything armed cascades.", notes, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (Palette::clip);

        for (const auto& c : conflicts)
            g.drawFittedText (c, notes.removeFromTop (14), juce::Justification::centredLeft, 1);
    }

    const int labelW = 64;
    const int rowH = juce::jmax (12, (area.getHeight() - 16) / TechniqueTable::count);

    // Strings across: 1 (high E) on the left, as the player numbers them.
    auto header = area.removeFromTop (16);
    header.removeFromLeft (labelW);
    const int cellW = juce::jmax (8, header.getWidth() / juce::jmax (1, numStrings));

    g.setFont (Fonts::ui (10.0f));
    g.setColour (Palette::textMuted);

    for (int s = 0; s < numStrings; ++s)
        g.drawText (juce::String (s + 1), header.getX() + s * cellW, header.getY(), cellW, 16, juce::Justification::centred);

    for (int t = 0; t < TechniqueTable::count; ++t)
    {
        auto row = area.removeFromTop (rowH);
        const auto slot = (TechniqueSlot) t;
        const bool isConflict = TechniqueTable::conflictFor (processor, slot).isNotEmpty();

        g.setColour (isConflict ? Palette::clip : (armed[(size_t) t] ? Palette::textPrimary : Palette::textDisabled));
        g.drawText (TechniqueTable::get (slot).name, row.removeFromLeft (labelW), juce::Justification::centredLeft);

        for (int s = 0; s < numStrings; ++s)
        {
            const auto cell = juce::Rectangle<int> (row.getX() + s * cellW, row.getY(), cellW, row.getHeight()).reduced (2).toFloat();
            const bool active = isActiveCell (slot, s);

            g.setColour (active ? Palette::accentBright : (armed[(size_t) t] ? Palette::panelRaised : Palette::panelSunken));
            g.fillRoundedRectangle (cell, 2.0f);

            if (armed[(size_t) t])
            {
                g.setColour (Palette::edge);
                g.drawRoundedRectangle (cell, 2.0f, 1.0f);
            }
        }
    }
}

CascadePage::CascadePage (LuthierAudioProcessor& p)
    : ControlFlow (p), view (p)
{
    addHeading ("LIVE: TECHNIQUES x STRINGS");
    addWide (view, 240);
}

} // namespace luthier
