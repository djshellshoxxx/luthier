#include "AdvancedPanel.h"
#include "../PluginProcessor.h"

namespace luthier
{

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
        muted = ! muted;

        processor.getEngine().getString (stringIndex).setDamping (
            muted ? StringEngine::Damping::Choked : StringEngine::Damping::Open, 1.0);

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
    items.add (item);
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
    constexpr int kKnobRow = 0;   // placeholder to keep the helpers readable
}

AdvancedPanel::AdvancedPanel (LuthierAudioProcessor& p)
    : processor (p),
      guitarBody (p),
      fretboard (p)
{
    juce::ignoreUnused (kKnobRow);

    addAndMakeVisible (guitarBody);
    addAndMakeVisible (fretboard);
    fretboard.setCompact (true);

    fretboard.onStringSelected = [this] (int s) { setSelectedString (s); };
    guitarBody.onPickupSelected = [this] (int) {};

    const char* titles[4] = { "Strings", "String Detail", "Body / Pickups / Hand", "Rig" };

    for (int i = 0; i < 4; ++i)
    {
        columns[i] = std::make_unique<Column> (titles[i]);

        viewports[i].setViewedComponent (columns[i].get(), false);
        viewports[i].setScrollBarsShown (true, false);
        viewports[i].setScrollBarThickness (8);
        addAndMakeVisible (viewports[i]);
    }

    buildStringColumn();
    buildDetailColumn();
    buildBodyColumn();
    buildRigColumn();

    setSelectedString (processor.getUiState().selectedString);
}

AdvancedPanel::~AdvancedPanel() = default;

//==============================================================================
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
void AdvancedPanel::buildStringColumn()
{
    auto& column = *columns[0];

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

    stringAge = std::make_unique<LuthierChoice> ("Age");
    stringAge->attachTo (processor, ParamIDs::stringAge,
                         "Fresh strings are bright and squeaky; old ones are dull, die sooner "
                         "and drift out of tune.");
    column.addControl (stringAge.get(), 36);

    column.addGap (Metrics::gridHalf);

    sustain = std::make_unique<LuthierKnob> ("Sustain");
    sustain->attachTo (processor, ParamIDs::sustainScale,
                       "Scales every string's decay time. Higher notes still die sooner than "
                       "low ones, as they do on a real instrument.");
    column.addControl (sustain.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

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
}

//==============================================================================
void AdvancedPanel::buildDetailColumn()
{
    auto& column = *columns[1];

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

    freezeToggle = std::make_unique<LuthierToggle> ("Freeze / E-Bow");
    freezeToggle->attachTo (processor, ParamIDs::freeze,
                            "Drives the ringing strings at their own resonance so they sustain "
                            "indefinitely, the way an E-Bow does.");
    column.addControl (freezeToggle.get(), Metrics::buttonHeight);

    column.addGap (Metrics::gridHalf);

    fretAction = std::make_unique<LuthierKnob> ("Action");
    fretAction->attachTo (processor, ParamIDs::fretAction,
                          "String height above the frets. Low action buzzes more and makes "
                          "bends catch more easily.");
    column.addControl (fretAction.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

    fretBuzz = std::make_unique<LuthierKnob> ("Buzz");
    fretBuzz->attachTo (processor, ParamIDs::fretBuzz,
                        "How readily the string slaps the frets. Clips the loud peaks and adds "
                        "a bright rattle, exactly as the real thing does.");
    column.addControl (fretBuzz.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

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

    column.addGap (Metrics::grid);
    column.addSection ("Sympathetic");

    couplingAmount = std::make_unique<LuthierKnob> ("Coupling");
    couplingAmount->attachTo (processor, ParamIDs::couplingAmount,
                              "How strongly the strings ring each other through the bridge. "
                              "Never fully off: this is a large part of why a guitar sounds "
                              "like a guitar.");
    column.addControl (couplingAmount.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));
}

//==============================================================================
void AdvancedPanel::buildBodyColumn()
{
    auto& column = *columns[2];

    // ---- body ------------------------------------------------------------------
    column.addSection ("Body");

    bodyMode = std::make_unique<LuthierChoice> ("Mode");
    bodyMode->attachTo (processor, ParamIDs::bodyMode,
                        "Convolution uses a measured impulse response. Modal builds the body "
                        "from its dimensions, so changing the size really does change the "
                        "resonances.");
    column.addControl (bodyMode.get(), 36);

    auto addKnob = [&column, this] (std::unique_ptr<LuthierKnob>& knob, const char* name,
                                    const char* paramId, const char* tooltip)
    {
        knob = std::make_unique<LuthierKnob> (name);
        knob->attachTo (processor, paramId, tooltip);
        column.addControl (knob.get(), LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));
    };

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

    // ---- pickups ------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Pickups");

    pickupSelector = std::make_unique<LuthierChoice> ("Selector");
    pickupSelector->attachTo (processor, ParamIDs::pickupSelector,
                              "Switch position. Selecting nothing would silence the instrument, "
                              "so at least one pickup is always live.");
    column.addControl (pickupSelector.get(), 36);

    for (int slot = 0; slot < PickupEngine::kMaxPickups; ++slot)
    {
        const juce::String n (slot + 1);

        pickupType[slot] = std::make_unique<LuthierChoice> ("Pickup " + n);
        pickupType[slot]->attachTo (processor, ParamIDs::pickupType (slot),
                                    "Pickup type. A humbucker is two coils at different points "
                                    "along the string, summed - that comb filter is what makes "
                                    "it sound like a humbucker.");
        column.addControl (pickupType[slot].get(), 36);

        pickupPosition[slot] = std::make_unique<LuthierKnob> ("Position " + n);
        pickupPosition[slot]->attachTo (processor, ParamIDs::pickupPosition (slot),
                                        "Distance from the bridge as a fraction of the string. "
                                        "A pickup at 1/N of the string nulls the Nth harmonic.");
        column.addControl (pickupPosition[slot].get(),
                           LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

        pickupHeight[slot] = std::make_unique<LuthierKnob> ("Height " + n);
        pickupHeight[slot]->attachTo (processor, ParamIDs::pickupHeight (slot),
                                      "Closer is louder and brighter");
        column.addControl (pickupHeight[slot].get(),
                           LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));

        pickupMagnet[slot] = std::make_unique<LuthierChoice> ("Magnet " + n);
        pickupMagnet[slot]->attachTo (processor, ParamIDs::pickupMagnet (slot),
                                      "Magnet type, each with its own EQ character");
        column.addControl (pickupMagnet[slot].get(), 36);

        pickupVolume[slot] = std::make_unique<LuthierKnob> ("Volume " + n);
        pickupVolume[slot]->attachTo (processor, ParamIDs::pickupVolume (slot),
                                      "Per-pickup volume, as on a Les Paul");
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
    addKnob (guitarTone, "Tone", ParamIDs::guitarTone,
             "The guitar's own tone control: a passive treble roll-off, not an EQ");
    addKnob (guitarVolume, "Volume", ParamIDs::guitarVolume,
             "The guitar's volume control");

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
    addKnob (ampBuzz, "Amp Buzz", ParamIDs::ampBuzz,
             "Mains hum. Single coils hum; humbuckers cancel it.");
}

//==============================================================================
void AdvancedPanel::buildRigColumn()
{
    auto& column = *columns[3];

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

    // ---- hardware -----------------------------------------------------------------
    column.addSection ("Bridge");

    addChoice (bridgeType, "Bridge", ParamIDs::bridgeType,
               "A vintage trem detunes chords as you bend, because the bridge moves the "
               "slack strings further than the tight ones. A TransTrem applies the same "
               "ratio to every string, so chords stay in tune.");

    addKnob (whammyPos, "Whammy", ParamIDs::whammyPos, "Bar position");
    addKnob (whammyDown, "Down Range", ParamIDs::whammyDown, "Semitones at full dive");
    addKnob (whammyUp, "Up Range", ParamIDs::whammyUp, "Semitones at full pull-up");
    addKnob (whammySprings, "Springs", ParamIDs::whammySprings,
             "Floyd Rose only: the spring cavity ringing as the bar snaps back");
    addKnob (transposeLock, "Transpose", ParamIDs::transposeLock,
             "TransTrem detente: locks the bar at a whole number of semitones");

    // ---- cable ---------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Cable");

    addToggle (cableOn, "Cable", ParamIDs::cableOn, "Cable capacitance roll-off");
    addKnob (cableLength, "Length", ParamIDs::cableLength,
             "A long cable into a high-impedance pickup rolls the treble off. "
             "Ten metres is obvious; one metre is not.");

    // ---- pre-amp pedals ----------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Pedalboard (before the amp)");

    preRack = std::make_unique<PedalRack> (processor, false);
    column.addControl (preRack.get(), preRack->getPreferredHeight());

    // ---- amp ----------------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Amplifier");

    addChoice (ampModel, "Amp", ParamIDs::ampModel,
               "Preamp stage count, tone stack topology, power tube type and negative "
               "feedback all change with the model.");

    addKnob (ampGain, "Gain", ParamIDs::ampGain, "Preamp drive");
    addKnob (ampBass, "Bass", ParamIDs::ampBass,
             "Passive tone stack: the three controls interact, exactly as in the circuit");
    addKnob (ampMid, "Mid", ParamIDs::ampMid, "Passive tone stack midrange");
    addKnob (ampTreble, "Treble", ParamIDs::ampTreble, "Passive tone stack treble");
    addKnob (ampPresence, "Presence", ParamIDs::ampPresence,
             "Works inside the power amp's feedback loop, so it does more on amps that "
             "have plenty of feedback and almost nothing on a Vox");
    addKnob (ampMaster, "Master", ParamIDs::ampMaster,
             "Power amp drive. Turn it up for power-tube saturation and sag.");

    addToggle (ampBright, "Bright", ParamIDs::ampBright,
               "Treble bypass cap: strongest at low gain, gone as the gain comes up");
    addToggle (ampMidBoost, "Mid boost", ParamIDs::ampMidBoost, "Midrange lift ahead of the gain");
    addToggle (ampStandby, "Standby", ParamIDs::ampStandby,
               "Mutes the amp, and takes time to warm back up, like the real switch");

    // ---- cabinet --------------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Cabinet and Mic");

    addToggle (cabOn, "Cabinet", ParamIDs::cabOn, "Speaker and microphone simulation");
    addChoice (cabType, "Cabinet", ParamIDs::cabType, "Cabinet size and back construction");
    addChoice (cabSpeaker, "Speaker", ParamIDs::cabSpeaker, "Speaker model");
    addKnob (speakerAge, "Speaker Age", ParamIDs::cabSpeakerAge,
             "A broken-in speaker has a looser surround: lower and less peaky");

    addChoice (micType, "Mic 1", ParamIDs::micType, "First microphone");
    addChoice (micPosition, "Position 1", ParamIDs::micPosition,
               "On axis at the dust cap is brightest; off axis at the cone edge is darkest");
    addChoice (micDistance, "Distance 1", ParamIDs::micDistance,
               "Close gives proximity bass; far trades it for room");

    addToggle (dualMic, "Second mic", ParamIDs::dualMic, "Blend a second microphone");
    addChoice (micType2, "Mic 2", ParamIDs::micType2, "Second microphone");
    addChoice (micPosition2, "Position 2", ParamIDs::micPosition2, "Second mic position");
    addChoice (micDistance2, "Distance 2", ParamIDs::micDistance2, "Second mic distance");

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

    // ---- post pedals -----------------------------------------------------------------------
    column.addGap (Metrics::grid);
    column.addSection ("Effects Loop (after the amp)");

    postRack = std::make_unique<PedalRack> (processor, true);
    column.addControl (postRack.get(), postRack->getPreferredHeight());

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
    addKnob (strumSpeed, "Strum Speed", ParamIDs::strumSpeed,
             "Time between strings as the pick crosses them");
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
    column.addGap (Metrics::grid);
    column.addSection ("Amp Feedback and Doubler");

    addToggle (feedbackOn, "Feedback", ParamIDs::feedbackOn,
               "With the amp loud and a note sustaining, feedback builds at a harmonic "
               "and gradually takes over");
    addKnob (feedbackThreshold, "Threshold", ParamIDs::feedbackThres, "How loud it has to be");
    addKnob (feedbackSpeed, "Speed", ParamIDs::feedbackSpeed, "How fast it builds");

    addToggle (doublerOn, "Doubler", ParamIDs::doublerOn,
               "A second, slightly different take panned opposite");
    addKnob (doublerAmount, "Amount", ParamIDs::doublerAmount, "Doubler level");

    column.addGap (Metrics::grid);
    column.addSection ("Master");

    addKnob (masterGain, "Master", ParamIDs::masterGain, "Output level");
    addToggle (limiterOn, "Limiter", ParamIDs::limiterOn,
               "Safety limiter at -0.3 dBFS. Transparent until the signal would clip.");
    addChoice (oversampling, "Oversampling", ParamIDs::oversample,
               "Oversampling for the nonlinear stages. Higher is cleaner and costs more CPU.");

    // ---- routing -------------------------------------------------------------------
    // routing-io section 8. Last in the column because it describes where the
    // finished signal goes, which is the end of the chain the column walks.
    column.addSection ("Routing");

    routingPanel = std::make_unique<RoutingPanel> (processor);
    column.addControl (routingPanel.get(), routingPanel->preferredHeight());

    // ---- modulation ------------------------------------------------------------------
    // modulation-matrix section 5. The spec wants this as its own MOD tab; the
    // column has no tab strip, so it is the last section, after routing.
    column.addSection ("Mod Matrix");

    modMatrixPanel = std::make_unique<ModMatrixPanel> (processor);
    column.addControl (modMatrixPanel.get(), modMatrixPanel->preferredHeight());

    // ---- rhythm ----------------------------------------------------------------------
    // rhythm-engine section 8, likewise asked for as its own RHYTHM tab and
    // likewise built as a section, after the matrix that can modulate it.
    column.addSection ("Rhythm");

    rhythmPanel = std::make_unique<RhythmPanel> (processor);
    column.addControl (rhythmPanel.get(), rhythmPanel->preferredHeight());

    // ---- tone match --------------------------------------------------------------------
    // tone-match section 6, again a section rather than a tab.
    column.addSection ("Tone Match");

    toneMatchPanel = std::make_unique<ToneMatchPanel> (processor);
    column.addControl (toneMatchPanel.get(), toneMatchPanel->preferredHeight());

    // ---- character ----------------------------------------------------------------------
    // character-wear section 10.
    column.addSection ("Character");

    characterPanel = std::make_unique<CharacterPanel> (processor);
    column.addControl (characterPanel.get(), characterPanel->preferredHeight());
}

//==============================================================================
void AdvancedPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // Column dividers.
    auto bounds = getLocalBounds().reduced (Metrics::windowPadding, Metrics::grid);
    const int stripHeight = juce::roundToInt (bounds.getHeight() * 0.25f);
    auto columnsArea = bounds.withTrimmedTop (stripHeight + Metrics::grid);

    const int columnWidth = columnsArea.getWidth() / 4;

    g.setColour (Palette::edge);

    for (int i = 1; i < 4; ++i)
        g.fillRect (columnsArea.getX() + columnWidth * i, columnsArea.getY(), 1, columnsArea.getHeight());

    LuthierLookAndFeel::drawSeparator (
        g, { bounds.getX(), bounds.getY() + stripHeight + 2, bounds.getWidth(), 1 });
}

void AdvancedPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::windowPadding, Metrics::grid);

    // ---- the compressed guitar strip ---------------------------------------------
    const int stripHeight = juce::roundToInt (bounds.getHeight() * 0.25f);
    auto strip = bounds.removeFromTop (stripHeight);

    auto guitarArea = strip.removeFromLeft (juce::jmin (260, strip.getWidth() / 3));
    guitarBody.setBounds (guitarArea);

    fretboard.setBounds (strip.reduced (Metrics::grid, Metrics::gridHalf));

    bounds.removeFromTop (Metrics::grid);

    // ---- four columns ----------------------------------------------------------------
    const int columnWidth = bounds.getWidth() / 4;

    for (int i = 0; i < 4; ++i)
    {
        auto area = bounds.removeFromLeft (i == 3 ? bounds.getWidth() : columnWidth);
        viewports[i].setBounds (area.reduced (1, 0));

        if (columns[i] != nullptr)
            columns[i]->layout (juce::jmax (80, viewports[i].getMaximumVisibleWidth()));
    }
}

} // namespace luthier
