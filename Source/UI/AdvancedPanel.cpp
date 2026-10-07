#include "AdvancedPanel.h"
#include "Onboarding.h"
#include "UiPreferences.h"
#include "RangesUi.h"
#include "OptionsPages.h"
#include "MidiOutPanel.h"
#include "NotationPanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "Search/LiveControls.h"   // global-search.md 3.2 (FEAT-SEARCH)

namespace luthier
{

//==============================================================================
//  ScrollHintViewport
//==============================================================================
class ScrollHintViewport::OverflowChevron : public juce::Component,
                                            public juce::SettableTooltipClient
{
public:
    OverflowChevron (ScrollHintViewport& v, bool isTop)
        : viewport (v), top (isTop)
    {
        setTooltip ((top ? "More controls above: scroll up, or click here" : "More controls below: scroll down, or click here"));
        setMouseCursor (juce::MouseCursor::PointingHandCursor);

        AccessibleSetup::configureDescriptive (*this,
                                               (top ? "More above" : "More below"),
                                               getTooltip());
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // The fade runs from the page background at the edge to nothing at the
        // inner side, so the strip reads as the column running under the frame.
        const auto solid = top ? bounds.getY() : bounds.getBottom();
        const auto clear = top ? bounds.getBottom() : bounds.getY();

        g.setGradientFill (juce::ColourGradient (Palette::background.withAlpha (0.92f),
                                                 bounds.getX(), solid,
                                                 Palette::background.withAlpha (0.0f),
                                                 bounds.getX(), clear, false));
        g.fillRect (bounds);

        LuthierLookAndFeel::drawChevron (g, bounds.getCentre(), 5.0f, top ? 0 : 2,
                                         hovering ? Palette::accentBright : Palette::accent, 1.6f);
    }

    void mouseEnter (const juce::MouseEvent&) override { hovering = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovering = false; repaint(); }

    /** Only the glyph takes the mouse; a control scrolled under the fade
        either side of it still gets its click. */
    bool hitTest (int x, int y) override
    {
        return juce::isPositiveAndBelow (y, getHeight())
            && std::abs (x - getWidth() / 2) <= ScrollHintViewport::hintGlyphHalfWidth;
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        viewport.pageBy (top ? -1 : 1);
    }

    // mouseWheelMove is deliberately not overridden: Component's default hands
    // the wheel to the parent, which is the Viewport, which scrolls.

private:
    ScrollHintViewport& viewport;
    const bool top;
    bool hovering = false;
};

ScrollHintViewport::ScrollHintViewport (const juce::String& componentName)
    : juce::Viewport (componentName)
{
    topHint = std::make_unique<OverflowChevron> (*this, true);
    bottomHint = std::make_unique<OverflowChevron> (*this, false);

    // Added after the Viewport's own content holder and scrollbars, so they sit
    // on top of both.
    addChildComponent (*topHint);
    addChildComponent (*bottomHint);
}

ScrollHintViewport::~ScrollHintViewport()
{
    if (watchedContent != nullptr)
        watchedContent->removeComponentListener (&contentWatcher);
}

void ScrollHintViewport::resized()
{
    juce::Viewport::resized();
    updateHints();
}

void ScrollHintViewport::visibleAreaChanged (const juce::Rectangle<int>&)
{
    updateHints();
}

void ScrollHintViewport::viewedComponentChanged (juce::Component* newComponent)
{
    if (watchedContent != nullptr)
        watchedContent->removeComponentListener (&contentWatcher);

    watchedContent = newComponent;

    if (watchedContent != nullptr)
        watchedContent->addComponentListener (&contentWatcher);

    updateHints();
}

void ScrollHintViewport::ContentWatcher::componentMovedOrResized (juce::Component&, bool, bool wasResized)
{
    // A taller or shorter content leaves the bottom hint stale otherwise:
    // resized() and visibleAreaChanged() only run on the viewport's own moves.
    if (wasResized)
        owner.updateHints();
}

void ScrollHintViewport::updateHints()
{
    const int width = getMaximumVisibleWidth();
    const int height = getMaximumVisibleHeight();

    topHint->setBounds (0, 0, width, hintHeight);
    bottomHint->setBounds (0, height - hintHeight, width, hintHeight);

    const auto* content = getViewedComponent();
    const int contentHeight = content != nullptr ? content->getHeight() : 0;
    const int viewY = getViewPositionY();

    topHint->setVisible (viewY > 0);
    bottomHint->setVisible (contentHeight > viewY + height);

    topHint->toFront (false);
    bottomHint->toFront (false);
}

bool ScrollHintViewport::isTopHintShowing() const noexcept     { return topHint->isVisible(); }
bool ScrollHintViewport::isBottomHintShowing() const noexcept  { return bottomHint->isVisible(); }

void ScrollHintViewport::pageBy (int direction)
{
    /*  Reduced motion (accessibility 5) is respected by construction: the view
        moves in one step, with no animation to disable. */
    const int step = juce::roundToInt ((float) getMaximumVisibleHeight() * 0.8f);
    setViewPosition (getViewPositionX(), getViewPositionY() + direction * step);
}


//==============================================================================
//  StringRow
//==============================================================================
StringRow::StringRow (LuthierAudioProcessor& p, int index)
    : processor (p), stringIndex (index)
{
    setTooltip ("Click to select this string. The square on the right mutes it.");
    startTimerHz (8);
}

StringRow::~StringRow()
{
    stopTimer();
}

void StringRow::setSelected (bool s)
{
    if (s != selected)
    {
        selected = s;
        repaint();
    }
}

void StringRow::timerCallback()
{
    auto& engine = processor.getEngine();

    if (stringIndex >= engine.getNumStrings())
    {
        setVisible (false);
        return;
    }

    setVisible (true);

    const auto& tuning = engine.getTuningEngine();
    const double openHz = tuning.getEffectiveOpenFrequency (stringIndex);

    const auto newNoteText = TuningEngine::describeFrequency (openHz, tuning.getConcertA());
    const double newTension = engine.getStringTensionNewtons (stringIndex);

    const auto& spec = engine.getStringSpec (stringIndex);

    juce::String newTensionText = juce::String (newTension, 1) + " N  ."
                                  + juce::String (spec.diameterInches, 3).fromFirstOccurrenceOf (".", false, false);

    if (newNoteText != noteText || newTensionText != tensionText)
    {
        noteText = newNoteText;
        tensionText = newTensionText;
        tensionNewtons = newTension;
        tensionPlayable = StringMaterials::isTensionPlayable (newTension);
        repaint();
    }
}

void StringRow::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    if (selected)
    {
        g.setColour (Palette::accent.withAlpha (0.12f));
        g.fillRoundedRectangle (bounds.toFloat(), Metrics::controlCorner);

        g.setColour (Palette::accent);
        g.fillRect (bounds.removeFromLeft (2));
    }

    bounds.reduce (Metrics::gridHalf, 0);

    // ---- string number ---------------------------------------------------------
    auto numberArea = bounds.removeFromLeft (18);

    g.setColour (selected ? Palette::accent : Palette::textDisabled);
    g.setFont (Fonts::mono (11.0f));
    g.drawText (juce::String (stringIndex + 1), numberArea, juce::Justification::centred, false);

    // ---- mute -------------------------------------------------------------------
    muteBounds = bounds.removeFromRight (18).withSizeKeepingCentre (12, 12);
    muted = processor.isStringMuted (stringIndex);   // the fretboard's menu mutes too

    g.setColour (muted ? Palette::warning : Palette::edge);
    g.drawRoundedRectangle (muteBounds.toFloat(), 2.0f, 1.0f);

    if (muted)
    {
        g.setColour (Palette::warning);
        g.fillRoundedRectangle (muteBounds.toFloat().reduced (3.0f), 1.0f);
    }

    // ---- note and tension ---------------------------------------------------------
    auto top = bounds.removeFromTop (bounds.getHeight() / 2);

    g.setColour (selected ? Palette::textPrimary : Palette::textMuted);
    g.setFont (Fonts::mono (12.0f));
    g.drawText (noteText, top, juce::Justification::centredLeft, false);

    // Tension is coloured by whether it is actually playable, which turns identity
    // rule 1 from a hidden check into something the user can see and act on.
    g.setColour (tensionPlayable ? Palette::textDisabled : Palette::warning);
    g.setFont (Fonts::mono (9.5f));
    g.drawText (tensionText, bounds, juce::Justification::centredLeft, false);
}

void StringRow::resized() {}

void StringRow::mouseDown (const juce::MouseEvent& e)
{
    if (muteBounds.contains (e.getPosition()))
    {
        muted = ! processor.isStringMuted (stringIndex);

        // CB-17: the string is the audio thread's; the processor queues the
        // mute and applies it at the top of the next block.
        processor.setStringMuted (stringIndex, muted);

        repaint();
        return;
    }

    if (onSelected)
        onSelected (stringIndex);
}

//==============================================================================
//  Column
//==============================================================================
AdvancedPanel::Column::Column (const juce::String& t)
    : title (t)
{
}

void AdvancedPanel::Column::addSection (const juce::String& heading)
{
    Item item;
    item.heading = heading;
    item.height = 24;

    // gui-integration 20 (TUNE-HELP-ONBOARDING): a ? on every section heading.
    item.help = helpButtons.add (new PanelHelpButton (heading));
    item.help->onHelp = [this] (const juce::String& topic)
    {
        if (onHelp != nullptr)
            onHelp (topic);
    };
    addAndMakeVisible (item.help);

    items.add (item);
}

juce::String AdvancedPanel::Column::getSectionAt (int y) const
{
    int top = Metrics::grid;
    juce::String heading;

    for (const auto& item : items)
    {
        if (item.heading.isNotEmpty())
            heading = item.heading;

        top += item.height + Metrics::gridHalf;

        if (y < top)
            break;
    }

    return heading;
}

void AdvancedPanel::Column::mouseDown (const juce::MouseEvent& e)
{
    // gui-integration 16: a panel's empty area offers its Docs.
    if (! e.mods.isPopupMenu())
        return;

    const auto section = getSectionAt (e.y);

    if (section.isEmpty())
        return;

    juce::PopupMenu menu;
    menu.addItem (1, "Docs: " + section);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<Column> (this), section] (int result)
    {
        if (safe != nullptr && result == 1 && safe->onHelp != nullptr)
            safe->onHelp (section);
    });
}

void AdvancedPanel::Column::addControl (juce::Component* component, int height)
{
    if (component == nullptr)
        return;

    addAndMakeVisible (component);

    Item item;
    item.component = component;
    item.height = height;
    items.add (item);
}

void AdvancedPanel::Column::renameSection (const juce::String& from, const juce::String& to)
{
    for (auto& item : items)
        if (item.heading == from)
            item.heading = to;

    repaint();
}

void AdvancedPanel::Column::addGap (int height)
{
    Item item;
    item.isGap = true;
    item.height = height;
    items.add (item);
}

int AdvancedPanel::Column::layout (int width)
{
    int y = Metrics::grid;

    for (auto& item : items)
    {
        if (item.component != nullptr)
            item.component->setBounds (Metrics::grid, y,
                                       juce::jmax (40, width - Metrics::grid * 2), item.height);

        if (item.help != nullptr)
            item.help->setBounds (width - Metrics::grid - PanelHelpButton::kSize,
                                  y + (item.height - PanelHelpButton::kSize) / 2 - 1,
                                  PanelHelpButton::kSize, PanelHelpButton::kSize);

        y += item.height + Metrics::gridHalf;
    }

    contentHeight = y + Metrics::grid;
    setSize (width, contentHeight);

    return contentHeight;
}

void AdvancedPanel::Column::resized()
{
    layout (getWidth());
}

void AdvancedPanel::Column::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    int y = Metrics::grid;

    for (const auto& item : items)
    {
        if (item.heading.isNotEmpty())
        {
            LuthierLookAndFeel::drawSectionHeader (
                g, { Metrics::grid, y, getWidth() - Metrics::grid * 2, item.height },
                item.heading);

            LuthierLookAndFeel::drawSeparator (
                g, { Metrics::grid, y + item.height - 2, getWidth() - Metrics::grid * 2, 1 });
        }

        y += item.height + Metrics::gridHalf;
    }
}

//==============================================================================
//  AdvancedPanel
//==============================================================================
namespace
{
    /*  Section 4.5, the column widths: 260 each at 1280, a 220 floor for a
        column and a 480 floor for the workspace, and columns 2 and 3 stacked
        into one slot below 1280. */
    constexpr int kColumnWidth = 260;
    constexpr int kMinColumnWidth = 220;
    constexpr int kMinWorkspaceWidth = 480;
    constexpr int kStackBelowWidth = 1280;

    constexpr int kKnobRow = 0;   // placeholder to keep the helpers readable
}

AdvancedPanel::AdvancedPanel (LuthierAudioProcessor& p)
    : processor (p),
      guitarBody (p),
      fretboard (p)
{
    setOpaque (true);   // paint() fills every pixel; spares the editor's paint under it
    juce::ignoreUnused (kKnobRow);

    addAndMakeVisible (guitarBody);
    addAndMakeVisible (fretboard);
    fretboard.setCompact (true);

    // piano-roll-chord-display.md 1: the piano roll, directly under the fretboard.
    pianoRoll = std::make_unique<PianoRollStrip> (processor, true);
    pianoRoll->setFretboard (&fretboard);
    pianoRoll->onLayoutChanged = [this] { resized(); repaint(); };
    addChildComponent (*pianoRoll);

    fretboard.onStringSelected = [this] (int s) { setSelectedString (s); };
    guitarBody.onPickupSelected = [this] (int) {};

    const char* titles[3] = { "Instrument", "Signal capture", "Amplification" };

    for (int i = 0; i < 3; ++i)
    {
        columns[i] = std::make_unique<Column> (titles[i]);
        columns[i]->onHelp = [this] (const juce::String& topic) { showHelp (topic); };   // gui-integration 20

        viewports[i].setViewedComponent (columns[i].get(), false);
        viewports[i].setScrollBarsShown (true, false);
        viewports[i].setScrollBarThickness (8);
        addAndMakeVisible (viewports[i]);

        // global-search.md 3.2 (FEAT-SEARCH): the column is a place; opening
        // it leaves WORKSHOP, which takes columns 2 and 3 over.
        search::SearchAnchors::tag (viewports[i], "column:" + juce::String (i + 1), [this]
        {
            if (isWorkshopShowing())
                for (int t = 0; t < workspacePanels.size(); ++t)
                    if (workspacePanels[t] != workshopPanel.get())
                    {
                        showWorkspaceTab (t);
                        break;
                    }
        });
    }

    workspaceViewport.setScrollBarsShown (true, false);
    workspaceViewport.setScrollBarThickness (8);
    addAndMakeVisible (workspaceViewport);

    // gui-integration 20 (TUNE-HELP-ONBOARDING): the workspace panel's ?.
    addAndMakeVisible (workspaceHelp);
    workspaceHelp.onHelp = [this] (const juce::String& topic) { showHelp (topic); };

    buildColumn1();
    buildColumn2();
    buildColumn3();
    buildWorkspace();

    setSelectedString (processor.getUiState().selectedString);
}

AdvancedPanel::~AdvancedPanel() = default;

//==============================================================================
void AdvancedPanel::setOversamplingNote (const juce::String& note)   // cpu-quality-modes 5
{
    if (oversampling == nullptr)
        return;

    juce::String text ("Oversampling for the nonlinear stages. Higher is cleaner and costs more CPU.");

    if (note.isNotEmpty())
        text << " " << note;

    oversampling->setTooltip (text);

    for (auto* child : oversampling->getChildren())
        if (auto* client = dynamic_cast<juce::SettableTooltipClient*> (child))
            client->setTooltip (text);
}

void AdvancedPanel::setSelectedString (int index)
{
    selectedString = juce::jlimit (0, kMaxStrings - 1, index);
    processor.getUiState().selectedString = selectedString;

    for (auto* row : stringRows)
        row->setSelected (row->isVisible() && stringRows.indexOf (row) == selectedString);

    fretboard.setSelectedString (selectedString);

    if (stringInfoLabel != nullptr)
    {
        auto& engine = processor.getEngine();
        const auto& spec = engine.getStringSpec (selectedString);
        const auto& physical = engine.getString (selectedString).getPhysical();

        juce::String text;
        text << "String " << (selectedString + 1) << "\n\n"
             << "Gauge        ." << juce::String (spec.diameterInches, 3)
                                       .fromFirstOccurrenceOf (".", false, false)
             << "  (" << juce::String (spec.diameterMm, 2) << " mm)\n"
             << "Construction " << (spec.wound ? "wound" : "plain") << "\n"
             << "Core         " << juce::String (spec.coreDiameterMm, 2) << " mm\n"
             << "Mass / metre " << juce::String (spec.linearDensity * 1000.0, 3) << " g\n"
             << "Tension      " << juce::String (spec.tensionNewtons, 1) << " N"
             << (StringMaterials::isTensionPlayable (spec.tensionNewtons) ? "" : "  (out of range)") << "\n"
             << "Scale        " << juce::String (physical.scaleLengthMm, 1) << " mm\n"
             << "Inharmonicity B = " << juce::String (spec.inharmonicityB, 6) << "\n"
             << "Sustain      " << juce::String (spec.sustainSeconds, 2) << " s (T60)\n"
             << "Brightness   " << juce::String (spec.brightnessHz, 0) << " Hz\n"
             << "Squeak       " << juce::String (spec.squeak, 2);

        stringInfoLabel->setText (text, juce::dontSendNotification);
    }

    repaint();
}

//==============================================================================
/*  Column 1, section 4.1: GUITAR, BODY, STRINGS, WHAMMY.

    Section 4.1 gives GUITAR an instrument library and an "Open in Workshop"
    button as well; the library is an Easy-mode surface, so what is here is the
    tuning and temperament half of it and the button into the Workshop tab.

    Three sections in this column are not in section 4.1, and are here because
    they are the only home their parameters have: SELECTED STRING, NECK - whose
    canonical home is the CHARACTER tab SETUP group, blocked on fret-buzz.md -
    and SYMPATHETIC. GAPS.md A1 lists them.
*/
void AdvancedPanel::buildColumn1()
{
    auto& column = *columns[0];

    auto addKnob = [&column, this] (std::unique_ptr<LuthierKnob>& knob, const char* name,
                                    const char* paramId, const char* tooltip)
    {
        knob = std::make_unique<LuthierKnob> (name);
        knob->attachTo (processor, paramId, tooltip);
        column.addControl (knob.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));
    };

    auto addChoice = [&column, this] (std::unique_ptr<LuthierChoice>& choice, const char* name,
                                      const char* paramId, const char* tooltip)
    {
        choice = std::make_unique<LuthierChoice> (name);
        choice->attachTo (processor, paramId, tooltip);
        column.addControl (choice.get(), 36);
    };

    auto addToggle = [&column, this] (std::unique_ptr<LuthierToggle>& toggle, const char* name,
                                      const char* paramId, const char* tooltip)
    {
        toggle = std::make_unique<LuthierToggle> (name);
        toggle->attachTo (processor, paramId, tooltip);
        column.addControl (toggle.get(), Metrics::buttonHeight);
    };

    juce::ignoreUnused (addKnob, addChoice, addToggle);

    // gui-integration 4.1: GUITAR's "Open in Workshop", the way into the bench
    // (the WORKSHOP tab) from the column that names the guitar.
    openWorkshopButton.setTooltip ("Opens the WORKSHOP tab: the guitar's parts, setup and strings on the bench");
    openWorkshopButton.onClick = [this] { setWorkspaceTabNamed ("WORKSHOP"); };
    column.addControl (&openWorkshopButton, Metrics::buttonHeight);

    column.addGap (Metrics::grid);
    column.addSection ("Temperament");

    temperament = std::make_unique<LuthierChoice> ("Temperament");
    temperament->attachTo (processor, ParamIDs::temperament,
                           "How the frets are spaced. Bends and fretless play stay continuous "
                           "in every temperament.");
    column.addControl (temperament.get(), 36);

    concertA = std::make_unique<LuthierKnob> ("Concert A");
    concertA->attachTo (processor, ParamIDs::concertA, "Reference pitch for A4");
    column.addControl (concertA.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

    /*  gui-integration 19 names this column as the capo's home, alongside the
        Workshop capo drag that does not exist. The headstock popover and the
        fretboard's right-click reach the same parameter. */
    capo = std::make_unique<LuthierChoice> ("Capo");
    capo->attachTo (processor, ParamIDs::capoFret,
                    "Where the capo sits. The open strings become the capo'd notes, and "
                    "the neck gets that much shorter. Partial capos need the Workshop.");
    column.addControl (capo.get(), 36);

    // ---- body ------------------------------------------------------------------
    column.addSection ("Body");

    bodyMode = std::make_unique<LuthierChoice> ("Mode");
    bodyMode->attachTo (processor, ParamIDs::bodyMode,
                        "Convolution uses a measured impulse response. Modal builds the body "
                        "from its dimensions, so changing the size really does change the "
                        "resonances.");
    column.addControl (bodyMode.get(), 36);
    addKnob (bodyAmount, "Amount", ParamIDs::bodyAmount,
             "How much body colour reaches the output");
    addKnob (bodyWidth, "Size", ParamIDs::bodyWidth,
             "Body width. In Modal mode a bigger body really does ring lower.");
    addKnob (bodyDepth, "Depth", ParamIDs::bodyDepth,
             "Body depth. Changes the enclosed volume and so the air resonance.");
    addKnob (topThickness, "Top", ParamIDs::bodyTopThick,
             "Top plate thickness. A thinner top is more responsive and pitched lower.");
    addKnob (soundhole, "Sound Hole", ParamIDs::bodySoundhole,
             "Sound hole size. Drives the Helmholtz air resonance directly.");
    addKnob (bodyAge, "Age", ParamIDs::bodyAge,
             "Seasoned wood has less internal damping, so the body rings longer.");
    addKnob (airGain, "Air", ParamIDs::bodyAirGain,
             "Emphasis on the air resonance: the boom of the box");

    // body-coupling.md 5: the body's return path onto the strings.
    addKnob (bodyCoupling, "Coupling", ParamIDs::bodyCouplingAmount,
             "How much the body pushes back on the strings: wolf notes, tap tones and ring "
             "through the body. The modes and the wolf map are on the CHARACTER tab.");

    bracing = std::make_unique<LuthierChoice> ("Bracing");
    bracing->attachTo (processor, ParamIDs::bodyBracing,
                       "Bracing stiffens the top, which raises every plate mode");
    column.addControl (bracing.get(), 36);

    topWood = std::make_unique<LuthierChoice> ("Top Wood");
    topWood->attachTo (processor, ParamIDs::bodyTopWood, "Top plate material");
    column.addControl (topWood.get(), 36);

    backWood = std::make_unique<LuthierChoice> ("Back / Sides");
    backWood->attachTo (processor, ParamIDs::bodyBackWood, "Back and side material");
    column.addControl (backWood.get(), 36);

    column.addSection ("Strings");

    for (int i = 0; i < kMaxStrings; ++i)
    {
        auto* row = new StringRow (processor, i);
        row->onSelected = [this] (int s) { setSelectedString (s); };

        stringRows.add (row);
        column.addControl (row, StringRow::preferredHeight);
    }

    column.addGap (Metrics::grid);
    column.addSection ("String Set");

    stringMaterial = std::make_unique<LuthierChoice> ("Material");
    stringMaterial->attachTo (processor, ParamIDs::stringMaterial,
                              "Wire material. Sets density, stiffness, sustain and how much "
                              "the winding squeaks under a moving finger.");
    column.addControl (stringMaterial.get(), 36);

    stringGauge = std::make_unique<LuthierChoice> ("Gauge");
    stringGauge->attachTo (processor, ParamIDs::stringGauge,
                           "Gauge set. Tension is recomputed for every string from its pitch, "
                           "the wire diameter and the scale length.");
    column.addControl (stringGauge.get(), 36);

    // string-aging.md 7: the age slider is the set's hours now (string_age
    // stays in the layout, inert, for old presets).
    stringAgeHours = std::make_unique<LuthierKnob> ("Age (h)");
    stringAgeHours->attachTo (processor, ParamIDs::stringAgeHours,
                              "Hours played: Fresh 0, Broken in 12, Old 120. Fresh strings are bright and "
                              "zingy; old ones are dull, die sooner and play sharp up the neck. "
                              "Per-string aging is on the CHARACTER tab.");
    column.addControl (stringAgeHours.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

    column.addGap (Metrics::gridHalf);

    sustain = std::make_unique<LuthierKnob> ("Sustain");
    sustain->attachTo (processor, ParamIDs::sustainScale,
                       "Scales every string's decay time. Higher notes still die sooner than "
                       "low ones, as they do on a real instrument.");
    column.addControl (sustain.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

    // sustain-and-decay.md 8: the DECAY row - the style and the decay sketch.
    decayRow = std::make_unique<DecayRow> (processor);
    column.addControl (decayRow.get(), DecayRow::preferredHeight);

    column.addGap (Metrics::grid);
    column.addSection ("Tuning Realism");

    realismDetune = std::make_unique<LuthierKnob> ("Detune");
    realismDetune->attachTo (processor, ParamIDs::realismDetune,
                             "How imperfectly the guitar is tuned. A real instrument is never "
                             "exact; zero here is a deliberate choice, not the default.");
    column.addControl (realismDetune.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

    intonation = std::make_unique<LuthierKnob> ("Intonation");
    intonation->attachTo (processor, ParamIDs::intonationErr,
                          "Cents of sharpening per fret. Real guitars go progressively sharp "
                          "up the neck because fretting stretches the string.");
    column.addControl (intonation.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

    driftToggle = std::make_unique<LuthierToggle> ("Tuning drift");
    driftToggle->attachTo (processor, ParamIDs::tuningDrift,
                           "Lets the guitar slowly go out of tune while you play");
    column.addControl (driftToggle.get(), Metrics::buttonHeight);

    column.addSection ("Selected String");

    stringInfoLabel = std::make_unique<juce::Label>();
    stringInfoLabel->setFont (Fonts::mono (10.5f));
    stringInfoLabel->setColour (juce::Label::textColourId, Palette::textMuted);
    stringInfoLabel->setJustificationType (juce::Justification::topLeft);
    column.addControl (stringInfoLabel.get(), 170);

    column.addGap (Metrics::grid);
    column.addSection ("Neck");

    fretlessToggle = std::make_unique<LuthierToggle> ("Fretless");
    fretlessToggle->attachTo (processor, ParamIDs::fretless,
                              "No frets: pitch becomes continuous, slides turn vocal, fret buzz "
                              "disappears and the attack softens.");
    column.addControl (fretlessToggle.get(), Metrics::buttonHeight);

    slideGuitarToggle = std::make_unique<LuthierToggle> ("Slide guitar");
    slideGuitarToggle->attachTo (processor, ParamIDs::slideGuitar,
                                 "Bottleneck mode: every note is played with a slide, whatever "
                                 "the instrument.");
    column.addControl (slideGuitarToggle.get(), Metrics::buttonHeight);

    column.addGap (Metrics::gridHalf);

    // The action is the setup's now (CHARACTER -> SETUP, fret-buzz.md); the
    // old single Action knob would be a second control that did nothing.
    fretBuzz = std::make_unique<LuthierKnob> ("Contact");
    fretBuzz->attachTo (processor, ParamIDs::fretBuzz,
                        "How much a string loses when it slaps a fret. Where and when it buzzes "
                        "is set by the setup in CHARACTER -> SETUP.");
    column.addControl (fretBuzz.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

    column.addGap (Metrics::grid);
    column.addSection ("Sympathetic");

    couplingAmount = std::make_unique<LuthierKnob> ("Coupling");
    couplingAmount->attachTo (processor, ParamIDs::couplingAmount,
                              "How strongly the strings ring each other through the bridge. "
                              "Never fully off: this is a large part of why a guitar sounds "
                              "like a guitar.");
    column.addControl (couplingAmount.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

    // ---- hardware -----------------------------------------------------------------
    column.addSection ("Bridge");

    addChoice (bridgeType, "Bridge", ParamIDs::bridgeType,
               "A vintage trem detunes chords as you bend, because the bridge moves the "
               "slack strings further than the tight ones. A transposing tremolo applies the same "
               "ratio to every string, so chords stay in tune.");

    addKnob (whammyPos, "Whammy", ParamIDs::whammyPos, "Bar position");
    addKnob (whammyDown, "Down Range", ParamIDs::whammyDown, "Semitones at full dive");
    addKnob (whammyUp, "Up Range", ParamIDs::whammyUp, "Semitones at full pull-up");
    addKnob (whammySprings, "Springs", ParamIDs::whammySprings,
             "Locking tremolo only: the spring cavity ringing as the bar snaps back");
    addKnob (transposeLock, "Transpose", ParamIDs::transposeLock,
             "Transposing-tremolo detente: locks the bar at a whole number of semitones");
}

//==============================================================================
/*  Column 2, section 4.2: PICKUPS, CIRCUIT, PRE-EFFECTS RACK.

    CIRCUIT is volume-knob-interaction.md 5 and replaced CABLE outright, the
    way GuitarCircuit replaced CableSim.

    PLAYING HAND and STRING NOISE are not in section 4.2. Their canonical homes
    are the CHARACTER tab PICK and STRING NOISE groups, both blocked on specs
    that are not written; they describe the hand that drives the pickup, so
    they wait here rather than in the rig. GAPS.md A1.
*/
void AdvancedPanel::buildColumn2()
{
    auto& column = *columns[1];

    auto addKnob = [&column, this] (std::unique_ptr<LuthierKnob>& knob, const char* name,
                                    const char* paramId, const char* tooltip)
    {
        knob = std::make_unique<LuthierKnob> (name);
        knob->attachTo (processor, paramId, tooltip);
        column.addControl (knob.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));
    };

    auto addChoice = [&column, this] (std::unique_ptr<LuthierChoice>& choice, const char* name,
                                      const char* paramId, const char* tooltip)
    {
        choice = std::make_unique<LuthierChoice> (name);
        choice->attachTo (processor, paramId, tooltip);
        column.addControl (choice.get(), 36);
    };

    auto addToggle = [&column, this] (std::unique_ptr<LuthierToggle>& toggle, const char* name,
                                      const char* paramId, const char* tooltip)
    {
        toggle = std::make_unique<LuthierToggle> (name);
        toggle->attachTo (processor, paramId, tooltip);
        column.addControl (toggle.get(), Metrics::buttonHeight);
    };

    juce::ignoreUnused (addKnob, addChoice, addToggle);

    // ---- pickups ------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Pickups");

    pickupSelector = std::make_unique<LuthierChoice> ("Selector");
    pickupSelector->attachTo (processor, ParamIDs::pickupSelector,
                              "Switch position. Selecting nothing would silence the instrument, "
                              "so at least one pickup is always live.");
    column.addControl (pickupSelector.get(), 36);

    // SPEC-SWEEP: SP-17 / ISS-2 - the continuous blend beside the switch.
    addKnob (pickupBlend, "Blend", ParamIDs::pickupBlend,
             "Balance between the two outermost pickups the switch has on: left favours "
             "the bridge side, right the neck side, centre is both at full level");

    for (int slot = 0; slot < PickupEngine::kMaxPickups; ++slot)
    {
        const juce::String n (slot + 1);

        pickupType[slot] = std::make_unique<LuthierChoice> ("Pickup " + n);
        pickupType[slot]->attachTo (processor, ParamIDs::pickupType (slot),
                                    "Pickup type. A humbucker is two coils at different points "
                                    "along the string, summed - that comb filter is what makes "
                                    "it sound like a humbucker.");
        column.addControl (pickupType[slot].get(), 36);

        // Position and height are placement on the guitar, set on the Workshop
        // bench (guitar-workshop.md 9, gui-integration 19).

        pickupMagnet[slot] = std::make_unique<LuthierChoice> ("Magnet " + n);
        pickupMagnet[slot]->attachTo (processor, ParamIDs::pickupMagnet (slot),
                                      "Magnet type, each with its own EQ character");
        column.addControl (pickupMagnet[slot].get(), 36);

        pickupVolume[slot] = std::make_unique<LuthierKnob> ("Volume " + n);
        pickupVolume[slot]->attachTo (processor, ParamIDs::pickupVolume (slot),
                                      "Per-pickup volume, as on a vintage single-cut");
        column.addControl (pickupVolume[slot].get(),
                           LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));
    }

    coilTap = std::make_unique<LuthierToggle> ("Coil tap");
    coilTap->attachTo (processor, ParamIDs::coilTap,
                       "Splits every humbucker to a single coil");
    column.addControl (coilTap.get(), Metrics::buttonHeight);

    addKnob (piezoMicBlend, "Piezo / Mic", ParamIDs::piezoMicBlend,
             "Acoustic instruments: balance between the under-saddle piezo and the "
             "internal condenser mic");
    // ---- circuit (volume-knob-interaction.md 5) --------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Circuit");

    circuitView = std::make_unique<CircuitResponseView> (processor);
    circuitView->setTooltip ("What the pickup, pots, cable and amp input do together. "
                             "Turn the volume down and watch the peak slide and flatten.");
    column.addControl (circuitView.get(), 96);

    // Large: these are played, not set.
    auto addLargeKnob = [&column, this] (std::unique_ptr<LuthierKnob>& knob, const char* name,
                                         const char* paramId, const char* tooltip)
    {
        knob = std::make_unique<LuthierKnob> (name, LuthierKnob::Size::Large);
        knob->attachTo (processor, paramId, tooltip);
        column.addControl (knob.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Large));
    };

    addLargeKnob (guitarVolume, "Volume", ParamIDs::guitarVolume,
                  "The volume pot's wiper. On a passive guitar turning down also darkens, "
                  "because the pot loads the pickup - the circuit view shows it.");
    addLargeKnob (guitarTone, "Tone", ParamIDs::guitarTone,
                  "The tone pot and cap: a passive treble cut, never a boost");

    auto addStandard = [&column, this] (std::unique_ptr<StandardValueChoice>& choice, const char* name,
                                        const char* paramId, juce::Array<double> values,
                                        juce::StringArray names, const char* tooltip)
    {
        choice = std::make_unique<StandardValueChoice> (name, std::move (values), std::move (names));
        choice->attachTo (processor, paramId, tooltip);
        column.addControl (choice.get(), 36);
    };

    addStandard (volumePotChoice, "Volume pot", ParamIDs::circuitVolumePot,
                 { 250.0e3, 500.0e3, 1.0e6 }, { "250k", "500k", "1M" },
                 "250k for single coils, 500k for humbuckers is the convention. "
                 "Bigger pots load the pickup less and sound brighter.");
    addKnob (volumePot, "Custom", ParamIDs::circuitVolumePot, "Any volume pot value");

    addStandard (tonePotChoice, "Tone pot", ParamIDs::circuitTonePot,
                 { 250.0e3, 500.0e3, 1.0e6 }, { "250k", "500k", "1M" },
                 "The tone pot also loads the pickup, even fully open");
    addKnob (tonePot, "Custom", ParamIDs::circuitTonePot, "Any tone pot value");

    addStandard (toneCapChoice, "Tone cap", ParamIDs::circuitToneCap,
                 { 10.0, 22.0, 47.0 }, { "10n", "22n", "47n" },
                 "A bigger cap reaches further down as the tone comes off");
    addKnob (toneCap, "Custom", ParamIDs::circuitToneCap, "Any tone cap value, in nF");

    addChoice (potTaper, "Taper", ParamIDs::circuitPotTaper,
               "Audio or linear pots, or 50s wiring - the tone control taken off the "
               "wiper, which keeps the top as the volume comes down");

    addChoice (trebleBleed, "Treble bleed", ParamIDs::circuitTrebleBleed,
               "A cap (and resistor) across the volume pot so turning down keeps the top. "
               "Modern RC is the gentle one, Vintage Cap the bright one.");
    addKnob (bleedR, "Bleed R", ParamIDs::circuitBleedR, "Custom bleed resistor");
    addKnob (bleedC, "Bleed C", ParamIDs::circuitBleedC, "Custom bleed cap, in nF");
    addChoice (bleedMode, "Bleed wiring", ParamIDs::circuitBleedMode,
               "Custom bleed: resistor and cap side by side, or one after the other");

    addToggle (circuitActive, "Active", ParamIDs::circuitActive,
               "A buffer straight after the pickup: the pots and cable stop loading it, "
               "so turning down loses level and not top");

    addToggle (cableOn, "Cable", ParamIDs::cableOn,
               "Off is a zero-length cable, not silence");
    addKnob (cableLength, "Length", ParamIDs::cableLength,
             "Cable capacitance grows with length. Ten metres of cheap cable moves "
             "the resonance a long way; one metre of studio cable barely does.");
    addChoice (cableQuality, "Cable", ParamIDs::cableQuality,
               "Capacitance per metre: studio 52 pF, standard 98, cheap 160, coiled 220");
    addKnob (ampInput, "Amp input", ParamIDs::ampInputImpedance,
             "The amp's input impedance, the last load on the pickup");

    // ---- pre-amp pedals ----------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Pedalboard (before the amp)");

    preRack = std::make_unique<PedalRack> (processor, false);
    column.addControl (preRack.get(), preRack->getPreferredHeight());
    addKnob (fxSaturation, "Saturation", ParamIDs::fxSaturation,
             "Soft clipping ahead of the amp, after the pedals. Turn it up for a thicker, "
             "more compressed sound; at zero it is off.");   // FEAT-SAT

    // ---- right hand ------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Playing Hand");

    useFingers = std::make_unique<LuthierToggle> ("Fingers");
    useFingers->attachTo (processor, ParamIDs::useFingers,
                          "Play with fingers instead of a pick");
    column.addControl (useFingers.get(), Metrics::buttonHeight);

    pickMaterial = std::make_unique<LuthierChoice> ("Pick / Finger");
    pickMaterial->attachTo (processor, ParamIDs::pickMaterial,
                            "What is touching the string. Each has its own contact spectrum.");
    column.addControl (pickMaterial.get(), 36);

    addKnob (pickThickness, "Thickness", ParamIDs::pickThickness,
             "Thin picks are bright and snappy, heavy ones warm and rounded");
    addKnob (pickAngle, "Angle", ParamIDs::pickAngle,
             "Parallel to the strings is aggressive; angled is softer");
    addKnob (pluckPosition, "Position", ParamIDs::pluckPosition,
             "Where the hand strikes: near the bridge is thin and bright, "
             "over the neck is round and full");
    addKnob (nailVsFlesh, "Nail / Flesh", ParamIDs::nailVsFlesh,
             "Fingerstyle only: nail is bright and sharp, flesh is warm");

    column.addGap (Metrics::grid);
    column.addSection ("String Noise");

    addKnob (slideNoise, "Slide", ParamIDs::slideNoise,
             "Finger squeak on wound strings. Wound strings squeak; plain ones barely do.");
    addKnob (fretNoise, "Fret", ParamIDs::fretNoise, "Click as a finger lands on a fret");
    addKnob (releaseNoise, "Release", ParamIDs::releaseNoise, "Thump as a note is stopped");
    addKnob (bodyKnock, "Body Knock", ParamIDs::bodyKnock, "Percussive tap on the body");
    addKnob (pickNoise, "Pick Attack", ParamIDs::pickNoise, "Contact noise under the pick");
    addKnob (ampBuzz, "Single-coil Hum", ParamIDs::ampBuzz,
             "Mains hum. Single coils hum; humbuckers cancel it.");
}

//==============================================================================
/*  Column 3, section 4.3: AMP, POST-EFFECTS RACK, CAB, ROOM, SUSTAIN.

    In section 4.3 order, which puts the post rack directly after the amp
    rather than after the room.

    PERFORMANCE, HUMANISE, FEEDBACK and MASTER follow them. Section 4 has no
    slot for any of the four - it describes the instrument and the rig, not the
    playing or the output stage - and they are the only home those parameters
    have, so they sit at the end of the rig column. GAPS.md A1.
*/
void AdvancedPanel::buildColumn3()
{
    auto& column = *columns[2];

    auto addKnob = [&column, this] (std::unique_ptr<LuthierKnob>& knob, const char* name,
                                    const char* paramId, const char* tooltip)
    {
        knob = std::make_unique<LuthierKnob> (name);
        knob->attachTo (processor, paramId, tooltip);
        column.addControl (knob.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));
    };

    auto addChoice = [&column, this] (std::unique_ptr<LuthierChoice>& choice, const char* name,
                                      const char* paramId, const char* tooltip)
    {
        choice = std::make_unique<LuthierChoice> (name);
        choice->attachTo (processor, paramId, tooltip);
        column.addControl (choice.get(), 36);
    };

    auto addToggle = [&column, this] (std::unique_ptr<LuthierToggle>& toggle, const char* name,
                                      const char* paramId, const char* tooltip)
    {
        toggle = std::make_unique<LuthierToggle> (name);
        toggle->attachTo (processor, paramId, tooltip);
        column.addControl (toggle.get(), Metrics::buttonHeight);
    };

    juce::ignoreUnused (addKnob, addChoice, addToggle);

    // ---- amp ----------------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Amplifier");

    addChoice (ampModel, "Amp", ParamIDs::ampModel,
               "Preamp stage count, tone stack topology, power tube type and negative "
               "feedback all change with the model.");

    /*  visual-polish.md 2: the six knobs and three switches that were a column of
        rows here sit on the amp's own face, attached to the same parameters
        with the same tooltips (AmpFacePanel), in the same place in the column. */
    ampFace = std::make_unique<AmpFacePanel> (processor, AmpFacePanel::Style::section);
    column.addControl (ampFace.get(), AmpFacePanel::sectionHeight);

    // ---- post pedals -----------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Effects Loop (after the amp)");

    postRack = std::make_unique<PedalRack> (processor, true);
    column.addControl (postRack.get(), postRack->getPreferredHeight());

    // ---- cabinet --------------------------------------------------------------------------
    column.addGap (Metrics::grid);
    micSectionTitle = MicUi::text ("mic.section.cabinet");
    column.addSection (micSectionTitle);

    addToggle (cabOn, "Cabinet", ParamIDs::cabOn, "Speaker and microphone simulation");
    addChoice (cabType, "Cabinet", ParamIDs::cabType, "Cabinet size and back construction");
    addChoice (cabSpeaker, "Speaker", ParamIDs::cabSpeaker, "Speaker model");
    addKnob (speakerAge, "Speaker Age", ParamIDs::cabSpeakerAge,
             "A broken-in speaker has a looser surround: lower and less peaky");

    addChoice (micType, "Mic 1", ParamIDs::micType, "First microphone");
    addToggle (dualMic, "Second mic", ParamIDs::dualMic, "Blend a second microphone");
    addChoice (micType2, "Mic 2", ParamIDs::micType2, "Second microphone");

    /*  mic-placement.md 6.1 (FEAT-MIC): the placement view replaces the
        Position and Distance combos; they live on inside it as the Quick
        choices, their canonical controls. */
    micView = std::make_unique<MicPlacementView> (processor);
    micView->onOpenEditor = [this] { if (isMicEditorShowing()) closeMicEditor(); else openMicEditor(); };
    micView->onFamilyChanged = [this] { if (getWidth() > 0) resized(); };
    column.addControl (micView.get(), micView->getPreferredHeight());

    addKnob (micBlend, "Mic Blend", ParamIDs::micBlend, "Balance between the two mics");
    addKnob (micWidth, "Width", ParamIDs::micWidth,
             "How far apart the two mics sit in the stereo image. Zero is fully mono-safe.");
    addKnob (micPhase, "Phase Align", ParamIDs::micPhaseAlign,
             "Compensates the time-of-flight difference between the mics. Getting this "
             "wrong is what makes a two-mic blend sound thin.");

    // ---- room ---------------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Room");

    addToggle (roomOn, "Room", ParamIDs::roomOn, "The room around the amp");
    addChoice (roomSize, "Size", ParamIDs::roomSize,
               "Reflection times are computed from the room's dimensions");
    addChoice (roomMaterial, "Material", ParamIDs::roomMaterial,
               "How absorbent the surfaces are");
    addKnob (roomBlend, "Blend", ParamIDs::roomBlend, "Close mic against room mic");
    addKnob (roomDecay, "Decay", ParamIDs::roomDecay, "Scales the room's natural decay");
    addKnob (roomWidth, "Width", ParamIDs::roomWidth, "Stereo width of the room mics");


    /*  ---- sustain (ambiguity-resolutions 2.3) --------------------------------------------
        Two rows, because they are two mechanisms. Freeze captures a window of what
        you just played and loops it; E-Bow drives the strings that are still
        ringing at their own resonance. Freeze holds a chord you have stopped
        playing, E-Bow sustains one you are still holding.
    */
    column.addGap (Metrics::grid);
    column.addSection ("Sustain");

    addToggle (freezeEnable, "Freeze", ParamIDs::freezeEnable,
               "Captures a window of what is sounding and loops it under what you play "
               "next, holding indefinitely. Switching it on again captures a new one.");
    addKnob (freezeCapture, "Capture", ParamIDs::freezeCaptureMs,
             "How much audio the freeze grabs. Longer catches a whole chord; shorter "
             "is tighter and more of a texture.");
    addKnob (freezeLevel, "Level", ParamIDs::freezeLevel,
             "How loud the held layer sits under the live signal");
    addKnob (freezeAttack, "Attack", ParamIDs::freezeAttackMs,
             "How quickly the held layer fades in once captured");
    addKnob (freezeRelease, "Release", ParamIDs::freezeReleaseMs,
             "How quickly it fades out when freeze is switched off");
    addKnob (freezeLowPass, "Low Pass", ParamIDs::freezeLpCutoff,
             "Darkens the held layer so it sits behind what you are playing");
    addKnob (freezeHighPass, "High Pass", ParamIDs::freezeHpCutoff,
             "Thins the held layer so it does not muddy the low end");

    addToggle (ebowToggle, "E-Bow", ParamIDs::ebowEnable,
               "Drives the ringing strings at their own resonance so they sustain "
               "indefinitely, the way an E-Bow does. Unlike Freeze, it only sustains "
               "notes you are still holding.");

    ebowStrings = std::make_unique<StringMaskSelector> (processor, ParamIDs::ebowStringMask);
    column.addControl (ebowStrings.get(), 26);

    addKnob (ebowIntensity, "Intensity", ParamIDs::ebowIntensity,
             "How hard the E-Bow drives: how loud the sustained note settles");
    addChoice (ebowHarmonic, "Harmonic", ParamIDs::ebowHarmonic,
               "Which partial of the note the E-Bow sustains - the fundamental, or a harmonic above it");

    // ambiguity-resolutions 1.3: the feedback row. The amp's sound reaching the
    // strings through the air, so pickups, volume knob and amp gain all matter.
    feedbackLed = std::make_unique<FeedbackLed> (processor);
    column.addControl (feedbackLed.get(), 22);

    addKnob (feedbackAmount, "Feedback", ParamIDs::feedbackAmount,
             "How much of the amp's sound reaches the strings. Loud amp, ringing note, "
             "and it takes over; roll the guitar's volume back to tame it.");
    addKnob (feedbackDistance, "Distance", ParamIDs::feedbackDistance,
             "How far the guitar is from the speaker. Closer feeds back sooner.");
    addKnob (feedbackAngle, "Angle", ParamIDs::feedbackAngle,
             "Which way the guitar faces the speaker. Turning away loses most of it.");
    addKnob (feedbackFocus, "Focus", ParamIDs::feedbackFocus,
             "How narrowly each string hears its own note. High focus locks to one pitch.");
    addKnob (feedbackOctave, "Octave", ParamIDs::feedbackOctaveBias,
             "Which octave of the note the feedback settles on.");

    // ---- performance ------------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Performance");

    addToggle (mpeToggle, "MPE", ParamIDs::mpeEnabled,
               "Per-note pitch bend, pressure and timbre from an MPE controller");
    addKnob (bendRange, "Bend Range", ParamIDs::bendRange,
             "Pitch-bend range. MPE controllers usually expect 48 semitones.");
    addKnob (legatoWindow, "Legato Window", ParamIDs::legatoWindow,
             "Notes closer together than this on one string become a slide rather than "
             "two separate articulations");
    addKnob (chordWindow, "Chord Window", ParamIDs::chordWindow,
             "How long Poly mode waits to collect a chord. Longer catches chords split "
             "across buffers; shorter has less latency.");
    // strum-dynamics 7: a mirror of the STRUM group's crossing control, which
    // replaced strum_speed (gui-integration 0.1 allows mirrors).
    addKnob (strumSpeed, "Strum Crossing", ParamIDs::strumCrossingSps,
             "How fast a strummed chord crosses the strings, in strings per second. 200 is a medium strum.");
    addChoice (strumDirection, "Strum", ParamIDs::strumDir, "Strum direction");
    addKnob (vibratoRate, "Vibrato Rate", ParamIDs::vibratoRate, "Vibrato speed");
    addKnob (vibratoDepth, "Vibrato Depth", ParamIDs::vibratoDepth, "Vibrato width in cents");
    addChoice (vibratoShape, "Vibrato Shape", ParamIDs::vibratoShape,
               "Finger vibrato is asymmetric, because a real hand pulls the string sharp "
               "faster than it lets it return");

    // ---- humanisation ----------------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Humanise");

    addKnob (humTiming, "Timing", ParamIDs::humTiming, "Timing jitter");
    addKnob (humVelocity, "Velocity", ParamIDs::humVelocity, "Velocity variation");
    addKnob (humDetune, "Detune", ParamIDs::humDetune, "Micro-detune, refreshed every note");
    addKnob (humAttack, "Attack", ParamIDs::humAttack, "Attack-time variation");
    addKnob (humNoise, "Noise", ParamIDs::humNoise, "How often incidental string noise occurs");
    addKnob (humStrum, "Strum", ParamIDs::humStrum, "Strum-speed variation");

    // ---- feedback, doubler, master ------------------------------------------------------------------
    // The doubler is a pedal in the post-amp rack (ambiguity-resolutions 3.1).

    column.addGap (Metrics::grid);
    column.addSection ("Master");

    addKnob (masterGain, "Master", ParamIDs::masterGain, "Output level");
    addToggle (limiterOn, "Limiter", ParamIDs::limiterOn,
               "Safety limiter at -0.3 dBFS. Transparent until the signal would clip.");
    addChoice (oversampling, "Oversampling", ParamIDs::oversample,
               "Oversampling for the nonlinear stages. Higher is cleaner and costs more CPU.");
}

/*  Column 4, section 4.4: the workspace.

    The tab order is section 4.4's own, with TECHNIQUES before HELP
    (gui-techniques-updates.md 0.2), and every tab has a panel behind it: a
    tab that opens on nothing is worse than no tab. GAPS.md A2 has the history.

    Five of the seven used to be sections stacked at the bottom of the rig column,
    each with a comment saying it should have been a tab. They are tabs now.

    LIVE is new rather than moved. `LiveStrip` was the only live surface and it is
    the runtime one - one press per thing, for a player mid-set - so the bank of
    128 snapshots and the order they come in had nowhere to be edited at all.

    CONTROLLERS is the sixth, and it came from the other direction: section 19
    always put controller setup here, and it sat on the Options overlay only
    because this strip did not exist. It is moved rather than copied, because two
    pages would mean two ControllerProfileLibrary instances scanning the
    Controllers folder separately and going stale against each other the moment
    either one saved a profile. Options is ten tabs now, which is section 5's
    list exactly, minus RANGES.
*/
void AdvancedPanel::showHelp (const juce::String& topic)
{
    if (helpTab == nullptr)
        return;

    helpTab->showTopicFor (topic);
    setWorkspaceTabNamed ("HELP");
}

juce::String AdvancedPanel::getHelpContextFor (const juce::Component* focused) const
{
    if (focused != nullptr)
    {
        for (int i = 0; i < workspacePanels.size(); ++i)
            if (auto* panel = workspacePanels[i]; panel != nullptr && (panel == focused || panel->isParentOf (focused)))
                return workspacePanels[i] == helpTab.get() ? juce::String() : getWorkspaceTabName (i);

        for (const auto& column : columns)
            if (column != nullptr)
                if (auto section = column->getSectionContaining (focused); section.isNotEmpty())
                    return section;
    }

    const auto shown = getWorkspaceTabName (workspaceTab);
    return shown == "HELP" ? juce::String() : shown;
}

juce::String AdvancedPanel::Column::getSectionContaining (const juce::Component* c) const
{
    if (c == nullptr)
        return {};

    juce::String heading;

    for (const auto& item : items)
    {
        if (item.component == nullptr && ! item.isGap)
            heading = item.heading;
        else if (item.component != nullptr && (item.component == c || item.component->isParentOf (c)))
            return heading;
    }

    return {};
}

//==============================================================================
// global-search.md 3.2 (FEAT-SEARCH)
juce::StringArray AdvancedPanel::Column::getSections() const
{
    juce::StringArray headings;

    for (const auto& item : items)
        if (item.heading.isNotEmpty())
            headings.add (item.heading);

    return headings;
}

int AdvancedPanel::Column::getSectionY (const juce::String& heading) const
{
    int y = Metrics::grid;

    for (const auto& item : items)
    {
        if (item.heading.equalsIgnoreCase (heading))
            return y;

        y += item.height + Metrics::gridHalf;
    }

    return -1;
}

juce::StringArray AdvancedPanel::getColumnSections (int column) const
{
    return juce::isPositiveAndBelow (column - 1, 3) && columns[column - 1] != nullptr
             ? columns[column - 1]->getSections() : juce::StringArray();
}

juce::String AdvancedPanel::getColumnSectionFor (const juce::Component* c, int& column) const
{
    for (int i = 0; i < 3; ++i)
        if (columns[i] != nullptr && c != nullptr && columns[i]->isParentOf (c))
        {
            column = i + 1;
            return columns[i]->getSectionContaining (c);
        }

    column = 0;
    return {};
}

bool AdvancedPanel::revealColumnSection (const juce::String& heading)
{
    for (int i = 0; i < 3; ++i)
    {
        const int y = columns[i] != nullptr ? columns[i]->getSectionY (heading) : -1;

        if (y < 0)
            continue;

        search::SearchAnchors::open (viewports[i]);
        resized();
        viewports[i].setViewPosition (0, juce::jmax (0, y - Metrics::grid));
        return true;
    }

    return false;
}

std::vector<PanelHelpButton*> AdvancedPanel::getHelpButtons() const
{
    std::vector<PanelHelpButton*> result;

    for (const auto& column : columns)
        if (column != nullptr)
            for (auto* b : column->helpButtons)
                result.push_back (b);

    result.push_back (const_cast<PanelHelpButton*> (&workspaceHelp));
    return result;
}

juce::Component* AdvancedPanel::getColumnViewport (int column) noexcept
{
    return juce::isPositiveAndBelow (column, 3) ? &viewports[column] : nullptr;
}

juce::Button* AdvancedPanel::getWorkspaceTabButton (const juce::String& tabName) const
{
    for (auto* tab : workspaceTabs)
        if (tab->getButtonText().equalsIgnoreCase (tabName))
            return tab;

    return nullptr;
}

//==============================================================================
void AdvancedPanel::openMicEditor()   // mic-placement.md 6.2 (FEAT-MIC)
{
    if (micEditor == nullptr)
    {
        micEditor = std::make_unique<MicPlacementEditor> (processor);
        micEditor->onClose = [this] { closeMicEditor(); };
        addChildComponent (*micEditor);
    }

    micEditor->setVisible (true);
    resized();
    micEditor->resized();   // the family may have changed while it was closed
    micEditor->grabKeyboardFocus();
}

void AdvancedPanel::closeMicEditor()
{
    if (micEditor == nullptr || ! micEditor->isVisible())
        return;

    micEditor->setVisible (false);
    resized();

    // Focus back to the button that opened it (6.2).
    if (micView != nullptr)
        micView->restoreFocusToExpand();
}

bool AdvancedPanel::isWorkshopShowing() const noexcept
{
    return workshopPanel != nullptr && juce::isPositiveAndBelow (workspaceTab, workspacePanels.size())
        && workspacePanels[workspaceTab] == workshopPanel.get();
}

void AdvancedPanel::buildWorkspace()
{
    workshopPanel   = std::make_unique<WorkshopPanel> (processor);
    modMatrixPanel  = std::make_unique<ModMatrixPanel> (processor);
    rhythmPanel     = std::make_unique<RhythmPanel> (processor);
    tunePanel       = std::make_unique<TunePanel> (processor, processor.getTunePlayer(), processor.getTuneSession());
    jamPanel        = std::make_unique<JamPanel> (processor);   // FEAT-JAM
    riffsPanel      = std::make_unique<RiffBrowser> (processor, false);   // riff-library 7.1
    livePanel       = std::make_unique<LivePanel> (processor);
    routingPanel    = std::make_unique<RoutingPanel> (processor);
    toneMatchPanel  = std::make_unique<ToneMatchPanel> (processor);
    characterPanel  = std::make_unique<CharacterPanel> (processor);
    controllersPage = std::make_unique<ControllersPage> (processor);
    midiOutPanel    = std::make_unique<MidiOutPanel> (processor);
    notationPanel   = std::make_unique<NotationPanel> (processor);
    practiceSetupPanel = std::make_unique<PracticeSetupPanel> (processor);
    helpTab         = std::make_unique<HelpTab> (processor);
    techniquesPanel = std::make_unique<TechniquesPanel> (processor);   // gui-techniques-updates.md 1

    const struct { const char* name; juce::Component* panel; } tabs[] =
    {
        { "WORKSHOP",    workshopPanel.get() },
        { "MOD",         modMatrixPanel.get() },
        { "RHYTHM",      rhythmPanel.get() },
        { "TUNE",        tunePanel.get() },
        { "JAM",         jamPanel.get() },   // FEAT-JAM: jam-mode 8.1, between TUNE and LIVE
        { "RIFFS",       riffsPanel.get() },   // riff-library 7.1: between TUNE and LIVE
        { "LIVE",        livePanel.get() },
        { "ROUTING",     routingPanel.get() },
        { "TONE MATCH",  toneMatchPanel.get() },
        { "CHARACTER",   characterPanel.get() },
        { "PRACTICE",    practiceSetupPanel.get() },
        { "NOTATION",    notationPanel.get() },
        { "MIDI OUT",    midiOutPanel.get() },
        { "CONTROLLERS", controllersPage.get() },
        { "TECHNIQUES",  techniquesPanel.get() },   // gui-techniques-updates.md 0.2
        { "HELP",        helpTab.get() }
    };

    for (const auto& tab : tabs)
    {
        // gui-integration 21: the tabs holding physical parameters carry a
        // range padlock. WORKSHOP joins when it exists.
        juce::TextButton* made = nullptr;

        if (juce::String (tab.name) == "CHARACTER")
            made = new RangesUi::RangeTabButton (tab.name, processor,
                                                 { RangeFamily::pick, RangeFamily::squeak,
                                                   RangeFamily::buzz, RangeFamily::slide,
                                                   // REALISM-A: string-aging 7, environment 7, body-coupling 5
                                                   RangeFamily::strings, RangeFamily::environment, RangeFamily::body });
        else if (juce::String (tab.name) == "MOD")   // SPEC-SWEEP: AR-15, the modulation family
            made = new RangesUi::RangeTabButton (tab.name, processor, { RangeFamily::modulation });
        else if (juce::String (tab.name) == "JAM")   // FEAT-JAM: kit tuning and damping (jam-mode 10)
            made = new RangesUi::RangeTabButton (tab.name, processor, { RangeFamily::jam });
        else if (juce::String (tab.name) == "WORKSHOP")   // gui-integration 21: the bench's setup strip, pick and slide
            made = new RangesUi::RangeTabButton (tab.name, processor,
                                                 { RangeFamily::buzz, RangeFamily::pick, RangeFamily::slide });
        else
            made = new juce::TextButton (tab.name);

        auto* button = workspaceTabs.add (made);

        button->setClickingTogglesState (true);
        button->setRadioGroupId (0x21);

        const int index = workspaceTabs.size() - 1;
        button->onClick = [this, index] { showWorkspaceTab (index); };

        button->setTooltip (juce::String (tab.name) + " workspace"
                              + "  (Ctrl+[ / Ctrl+] step tabs)");

        AccessibleSetup::configureButton (*button, tab.name,
                                          "Workspace tab. Shows the " + juce::String (tab.name)
                                            + " panel in column four.");

        workspacePanels.add (tab.panel);
    }

    // FEAT-RIFFS: the strip lays the buttons out, scrolling when they overflow.
    {
        juce::Array<juce::Button*> buttons;

        for (auto* b : workspaceTabs)
            buttons.add (b);

        workspaceStrip.setTabs (buttons);
        addAndMakeVisible (workspaceStrip);
    }

    /*  Section 4.4: the last-used tab persists across sessions in the plugin's
        user-global settings. Restored without writing back, so opening a window
        and closing it again does not rewrite the file. An out-of-range value -
        from a build with more tabs than this one - is clamped by showWorkspaceTab
        rather than refused. */
    const auto savedName = UiPreferences::get().getString (workspaceTabNamePreferenceKey, {});
    int saved = UiPreferences::get().getInt (workspaceTabPreferenceKey, 0);

    for (int i = 0; i < workspaceTabs.size(); ++i)
        if (savedName.isNotEmpty() && workspaceTabs[i]->getButtonText().equalsIgnoreCase (savedName))
            saved = i;

    showWorkspaceTab (saved, false);
}

juce::String AdvancedPanel::getWorkspaceTabName (int index) const
{
    if (auto* button = workspaceTabs[index])
        return button->getButtonText();

    return {};
}

juce::Component* AdvancedPanel::getWorkspacePanel (int index) const
{
    return juce::isPositiveAndBelow (index, workspacePanels.size())
             ? workspacePanels[index] : nullptr;
}

void AdvancedPanel::setWorkspaceTab (int index)
{
    showWorkspaceTab (index);
}

bool AdvancedPanel::setWorkspaceTabNamed (const juce::String& tabName)
{
    for (int i = 0; i < workspaceTabs.size(); ++i)
    {
        if (workspaceTabs[i]->getButtonText().equalsIgnoreCase (tabName))
        {
            showWorkspaceTab (i);
            return true;
        }
    }

    return false;
}

void AdvancedPanel::stepWorkspaceTab (int delta)
{
    const int count = workspacePanels.size();

    if (count <= 0)
        return;

    showWorkspaceTab (((workspaceTab + delta) % count + count) % count);
}

void AdvancedPanel::showWorkspaceTab (int index, bool remember)
{
    if (workspacePanels.isEmpty())
        return;

    workspaceTab = juce::jlimit (0, workspacePanels.size() - 1, index);

    if (remember)
    {
        UiPreferences::get().setInt (workspaceTabPreferenceKey, workspaceTab);
        UiPreferences::get().setString (workspaceTabNamePreferenceKey, getWorkspaceTabName (workspaceTab));
        Onboarding::markTabOpened (getWorkspaceTabName (workspaceTab));   // onboarding 4
    }

    // gui-integration 20: the ? follows the tab; HELP needs none.
    workspaceHelp.setTopic (getWorkspaceTabName (workspaceTab));
    workspaceHelp.setVisible (getWorkspaceTabName (workspaceTab) != "HELP");

    for (int i = 0; i < workspaceTabs.size(); ++i)
        workspaceTabs[i]->setToggleState (i == workspaceTab, juce::dontSendNotification);

    workspaceStrip.setSelectedIndex (workspaceTab);

    /*  Only the selected panel is on screen. Without this the panels that are not
        in the viewport keep whatever visibility they were built with, and a test
        - or a screen reader walking the tree - finds all of them showing at
        once. */
    for (int i = 0; i < workspacePanels.size(); ++i)
        workspacePanels[i]->setVisible (i == workspaceTab);

    /*  The viewport owns nothing: the panels are unique_ptr members, and handing
        one over with deleteWhenRemoved would delete it the next time the tab
        changed. */
    workspaceViewport.setViewedComponent (workspacePanels[workspaceTab], false);

    /*  The panels not in the viewport stay in the tree as hidden children: in
        the window but not showing, so AnimationPolicy stops their timers (the
        notation preview, the workshop bench) instead of them running detached. */
    for (int i = 0; i < workspacePanels.size(); ++i)
        if (auto* panel = workspacePanels[i]; i != workspaceTab && panel != nullptr && panel->getParentComponent() == nullptr)
            addChildComponent (panel);

    // The WORKSHOP tab changes the column layout, not just what column 4 shows.
    resized();

    /*  CONTROLLERS can be stale by the time it is opened - a controller may have
        been unplugged, or a profile saved from somewhere else - so it reads the
        world again on the way in. This is what OptionsPanel did for it when it
        lived there, and the reason it is a named case rather than a virtual on
        every panel is that the other five already track the processor on a timer. */
    if (controllersPage != nullptr && workspacePanels[workspaceTab] == controllersPage.get())
        controllersPage->refresh();

    resized();
}

//==============================================================================
void AdvancedPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto bounds = getLocalBounds().reduced (Metrics::windowPadding, Metrics::grid);
    const int stripHeight = getGuitarStripHeight (bounds.getHeight());
    auto columnsArea = bounds.withTrimmedTop (stripHeight + Metrics::grid);

    g.setColour (Palette::edge);

    for (const auto x : dividerX)
        g.fillRect (x, columnsArea.getY(), 1, columnsArea.getHeight());

    LuthierLookAndFeel::drawSeparator (
        g, { bounds.getX(), bounds.getY() + stripHeight + 2, bounds.getWidth(), 1 });
}

int AdvancedPanel::getGuitarStripHeight (int boundsHeight) const
{
    const int rollHeight = pianoRoll != nullptr && pianoRoll->isWanted() ? pianoRoll->getPreferredHeight() + Metrics::gridHalf : 0;
    return juce::roundToInt ((float) boundsHeight * 0.25f) + rollHeight;
}

void AdvancedPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::windowPadding, Metrics::grid);

    // ---- the compressed guitar strip ---------------------------------------------
    const int stripHeight = getGuitarStripHeight (bounds.getHeight());
    auto strip = bounds.removeFromTop (stripHeight);

    auto guitarArea = strip.removeFromLeft (juce::jmin (260, strip.getWidth() / 3));
    guitarBody.setBounds (guitarArea);

    // piano-roll-chord-display.md 1: the roll under the fretboard, its full width.
    if (pianoRoll != nullptr)
    {
        pianoRoll->setVisible (pianoRoll->isWanted());

        if (pianoRoll->isVisible())
            pianoRoll->setBounds (strip.removeFromBottom (pianoRoll->getPreferredHeight()).reduced (Metrics::grid, 0));
    }

    fretboard.setBounds (strip.reduced (Metrics::grid, Metrics::gridHalf));

    bounds.removeFromTop (Metrics::grid);

    /*  ---- the columns, section 4.5 -------------------------------------------------

        260 points each for columns 1 to 3 and the rest for the workspace, with
        220 as the columns' floor and 480 as the workspace's. Below 1280 there is
        not room for three of them beside a usable workspace, so columns 2 and 3
        share one slot, stacked - which is what section 4.5 asks for, and keeps
        every panel reachable rather than hiding one.
    */
    const bool stacked = getWidth() < kStackBelowWidth;

    // gui-integration.md 6: the Workshop takes over columns 3 and 4 (and, when
    // 2 and 3 share a slot, that slot).
    const bool workshop = isWorkshopShowing();
    const int slots = workshop ? (stacked ? 1 : 2) : (stacked ? 2 : 3);

    const int columnWidth = juce::jlimit (kMinColumnWidth, kColumnWidth,
                                          (bounds.getWidth() - kMinWorkspaceWidth) / slots);

    dividerX.clearQuick();

    auto takeColumn = [&bounds, columnWidth, this]
    {
        auto area = bounds.removeFromLeft (columnWidth);
        dividerX.add (bounds.getX());
        return area;
    };

    viewports[0].setBounds (takeColumn().reduced (1, 0));

    viewports[1].setVisible (! (workshop && stacked));
    viewports[2].setVisible (! workshop);

    if (workshop)
    {
        if (! stacked)
            viewports[1].setBounds (takeColumn().reduced (1, 0));
    }
    else if (stacked)
    {
        auto shared = takeColumn();
        auto top = shared.removeFromTop (shared.getHeight() / 2);

        viewports[1].setBounds (top.reduced (1, 0));
        viewports[2].setBounds (shared.reduced (1, Metrics::gridHalf));
    }
    else
    {
        viewports[1].setBounds (takeColumn().reduced (1, 0));
        viewports[2].setBounds (takeColumn().reduced (1, 0));
    }

    for (int i = 0; i < 3; ++i)
        if (columns[i] != nullptr)
            columns[i]->layout (juce::jmax (80, viewports[i].getMaximumVisibleWidth()));

    // ---- column 4: the tab strip, then whichever panel it selected ------------------
    workspaceLeft = bounds.getX();

    // gui-integration 20: the workspace's ? at the end of the (first) tab row.
    workspaceHelp.setBounds (bounds.withHeight (Metrics::buttonHeight).removeFromRight (Metrics::buttonHeight)
                                     .withSizeKeepingCentre (PanelHelpButton::kSize + 2, PanelHelpButton::kSize + 2));
    const int tabsRight = bounds.getRight() - Metrics::buttonHeight - Metrics::gridHalf;
    workspaceTabStrip = bounds.withHeight (Metrics::buttonHeight).withRight (tabsRight);

    // FEAT-RIFFS: one row. WorkspaceTabStrip gives each tab its label's width
    // (sharing out any spare room) and, when they do not all fit, scrolls with
    // arrows and an overflow menu, keeping the selected tab whole on screen.
    workspaceStrip.setBounds (workspaceTabStrip);
    bounds.removeFromTop (Metrics::buttonHeight);

    bounds.removeFromTop (Metrics::gridHalf);

    workspaceViewport.setBounds (bounds.reduced (1, 0));

    // mic-placement.md 6.2 (FEAT-MIC): the expanded editor takes Column 3 and
    // the workspace under the tab strip; the strip stays.
    if (isMicEditorShowing())
    {
        auto takeover = bounds;

        if (viewports[2].isVisible())
            takeover = takeover.getUnion (viewports[2].getBounds().withTop (bounds.getY()));

        micEditor->setBounds (takeover);
        micEditor->toFront (false);
    }

    viewports[2].setVisible (viewports[2].isVisible() && ! isMicEditorShowing());
    workspaceViewport.setVisible (! isMicEditorShowing());

    if (micView != nullptr)
    {
        const auto title = micView->getSectionTitle();

        if (title != micSectionTitle)
        {
            columns[2]->renameSection (micSectionTitle, title);
            micSectionTitle = title;
        }
    }

    // The panel keeps whatever height it asked for and takes the viewport's
    // width, so the workspace scrolls vertically exactly as a column does. The
    // bench fills the space instead: it is one surface, not a list.
    /*  Panels that size themselves (a preferred height from their content) keep
        it; the rest fill the viewport. They used to keep "whatever height they
        had", which for panels that never set one was the 80-point floor, so they
        rendered as a sliver (TODO V screenshots). */
    if (auto* panel = workspaceViewport.getViewedComponent())
    {
        const int visible = workspaceViewport.getMaximumVisibleHeight();
        int height = juce::jmax (80, visible);

        if (workshop)
            height = juce::jmax (440, visible);   // the bench fits a 1280x800 window without scrolling (TODO V)
        else if (panel == helpTab.get() || panel == techniquesPanel.get())   // TECHNIQUES: scrolls inside
            height = juce::jmax (360, visible);
        else if (panel == riffsPanel.get())
            height = juce::jmax (480, visible);   // riff-library 7.2
        else if (auto* p = dynamic_cast<TunePanel*> (panel))                 height = juce::jmax (visible, p->getPreferredHeight());
        else if (auto* p = dynamic_cast<JamPanel*> (panel))                  height = juce::jmax (visible, p->getPreferredHeightFor (workspaceViewport.getMaximumVisibleWidth()));   // FEAT-JAM
        else if (auto* p = dynamic_cast<MidiOutPanel*> (panel))              height = juce::jmax (visible, p->getPreferredHeight());
        else if (auto* p = dynamic_cast<NotationPanel*> (panel))             height = juce::jmax (visible, p->getPreferredHeight());
        else if (auto* p = dynamic_cast<PracticeSetupPanel*> (panel))        height = juce::jmax (visible, p->getPreferredHeight());
        else if (auto* p = dynamic_cast<RhythmPanel*> (panel))               height = juce::jmax (visible, p->preferredHeight());   // issues.md 8: the STRUM group's lower rows
        else if (auto* p = dynamic_cast<ModMatrixPanel*> (panel))            height = juce::jmax (visible, p->preferredHeight());   // the user macros above the route table
        else if (panel == characterPanel.get() || panel == controllersPage.get())
            height = juce::jmax (80, panel->getHeight());

        panel->setSize (juce::jmax (80, workspaceViewport.getMaximumVisibleWidth()), height);
    }
}

} // namespace luthier
