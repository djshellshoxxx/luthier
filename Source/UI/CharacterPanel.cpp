#include "CharacterPanel.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    void styleHeading (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        label.setColour (juce::Label::textColourId, Palette::accent);
    }

    void styleSlider (juce::Slider& slider, double minimum, double maximum,
                      double interval, const juce::String& suffix)
    {
        slider.setRange (minimum, maximum, interval);
        slider.setTextValueSuffix (suffix);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }
}

//==============================================================================
DeadSpotMap::DeadSpotMap (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Dead spots. Each marker is a fret where that string sustains "
                "less. Drag sideways to move one, up and down to deepen it.");
}

DeadSpotMap::~DeadSpotMap() = default;

CharacterEngine& DeadSpotMap::character()
{
    return processor.getEngine().getCharacterEngine();
}

void DeadSpotMap::refresh()
{
    repaint();
}

float DeadSpotMap::fretX (double fret) const
{
    const float usable = (float) getWidth() - 24.0f;

    return 20.0f + usable * (float) (fret / (double) numFretsShown);
}

bool DeadSpotMap::hitTest (juce::Point<int> position, int& stringIndex, double& fret) const
{
    const int strings = juce::jlimit (1, kMaxStrings, processor.getEngine().getNumStrings());

    stringIndex = position.y / rowHeight;

    if (! juce::isPositiveAndBelow (stringIndex, strings))
        return false;

    const float usable = (float) getWidth() - 24.0f;

    if (usable <= 0.0f)
        return false;

    fret = (double) ((float) position.x - 20.0f) / usable * (double) numFretsShown;

    return fret >= 0.0 && fret <= (double) numFretsShown;
}

void DeadSpotMap::paint (juce::Graphics& g)
{
    auto& engine = character();

    const int strings = juce::jlimit (1, kMaxStrings, processor.getEngine().getNumStrings());

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

    // ---- fret lines ----------------------------------------------------------------
    for (int fret = 0; fret <= numFretsShown; ++fret)
    {
        const float x = fretX (fret);

        // The position markers a guitarist reads the neck by.
        const bool marker = fret == 3 || fret == 5 || fret == 7 || fret == 9
                              || fret == 12 || fret == 15 || fret == 17 || fret == 19;

        g.setColour (fret == 0 ? Palette::textMuted
                               : (marker ? Palette::edgeBright : Palette::edge));

        g.drawLine (x, 2.0f, x, (float) (strings * rowHeight), fret == 0 ? 2.0f : 1.0f);
    }

    // ---- strings --------------------------------------------------------------------
    for (int s = 0; s < strings; ++s)
    {
        const float y = (float) (s * rowHeight + rowHeight / 2);

        g.setColour (Palette::edge);
        g.drawLine (20.0f, y, (float) getWidth() - 4.0f, y, 1.0f);

        g.setColour (Palette::textDisabled);
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText (juce::String (s + 1), 2, s * rowHeight, 16, rowHeight,
                    juce::Justification::centred, false);

        // ---- the spots ----------------------------------------------------------------
        for (int i = 0; i < engine.getNumDeadSpots (s); ++i)
        {
            const auto spot = engine.getDeadSpot (s, i);

            const float x = fretX (spot.fret);
            const float halfWidth = (fretX (spot.width) - fretX (0.0)) * 0.5f;

            // The spot's width is drawn as a smear, because that is what it is:
            // a region of the neck, not a point on it.
            g.setColour (Palette::clip.withAlpha ((float) (0.18 + 0.4 * spot.depth)));
            g.fillRoundedRectangle (x - halfWidth, y - 5.0f, halfWidth * 2.0f, 10.0f, 4.0f);

            // Depth is shown by size as well as by opacity, so it survives a
            // colourblind palette (accessibility 2).
            const float radius = (float) (2.5 + 3.5 * spot.depth);

            g.setColour (Palette::clip);
            g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        }
    }

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);
}

void DeadSpotMap::mouseDown (const juce::MouseEvent& event)
{
    int stringIndex = 0;
    double fret = 0.0;

    if (! hitTest (event.getPosition(), stringIndex, fret))
        return;

    auto& engine = character();

    // Grab the nearest spot on this string, or add one where the click landed.
    int nearest = -1;
    double bestDistance = 2.5;

    for (int i = 0; i < engine.getNumDeadSpots (stringIndex); ++i)
    {
        const double distance = std::abs ((double) engine.getDeadSpot (stringIndex, i).fret - fret);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            nearest = i;
        }
    }

    if (nearest < 0 && engine.getNumDeadSpots (stringIndex) < CharacterEngine::kMaxDeadSpotsPerString)
    {
        DeadSpot spot;
        spot.fret = juce::jlimit (0, CharacterEngine::kMaxFrets, (int) std::round (fret));
        spot.depth = 0.35;
        spot.width = 3.0;

        nearest = engine.getNumDeadSpots (stringIndex);
        engine.setDeadSpot (stringIndex, nearest, spot);
    }

    draggingString = stringIndex;
    draggingSpot = nearest;

    refresh();

    if (onEdited != nullptr)
        onEdited();
}

void DeadSpotMap::mouseDrag (const juce::MouseEvent& event)
{
    if (draggingString < 0 || draggingSpot < 0)
        return;

    int stringIndex = 0;
    double fret = 0.0;

    if (! hitTest ({ event.getPosition().x, draggingString * rowHeight + rowHeight / 2 },
                   stringIndex, fret))
        return;

    auto& engine = character();

    auto spot = engine.getDeadSpot (draggingString, draggingSpot);

    spot.fret = juce::jlimit (0, CharacterEngine::kMaxFrets, (int) std::round (fret));

    // Vertical travel from where the drag started deepens or shallows it.
    spot.depth = juce::jlimit (0.1, 0.7, spot.depth - (double) event.getDistanceFromDragStartY() * 0.004);

    engine.setDeadSpot (draggingString, draggingSpot, spot);

    refresh();

    if (onEdited != nullptr)
        onEdited();
}

//==============================================================================
FretWearMap::FretWearMap (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Fret wear. Taller means more worn: less sustain, more buzz, "
                "and a note a few cents flat. Drag to change it.");
}

FretWearMap::~FretWearMap() = default;

CharacterEngine& FretWearMap::character()
{
    return processor.getEngine().getCharacterEngine();
}

void FretWearMap::refresh()
{
    repaint();
}

void FretWearMap::paint (juce::Graphics& g)
{
    auto& engine = character();

    auto bounds = getLocalBounds().reduced (2);

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds.toFloat(), 3.0f);

    const int numFrets = CharacterEngine::kMaxFrets + 1;
    const float barWidth = (float) bounds.getWidth() / (float) numFrets;

    for (int fret = 0; fret < numFrets; ++fret)
    {
        const double wear = engine.getFretWear (fret);

        const float x = (float) bounds.getX() + barWidth * (float) fret;
        const float height = (float) (bounds.getHeight() - 12) * (float) wear;

        g.setColour (Palette::warning.withAlpha (0.25f + 0.6f * (float) wear));
        g.fillRect (x + 1.0f, (float) bounds.getBottom() - 12.0f - height,
                    barWidth - 2.0f, height);

        // Every fifth fret is labelled, so the user can find the one they mean.
        if (fret % 5 == 0)
        {
            g.setColour (Palette::textDisabled);
            g.setFont (juce::Font (juce::FontOptions (8.0f)));
            g.drawText (juce::String (fret),
                        juce::Rectangle<float> (x, (float) bounds.getBottom() - 11.0f,
                                                barWidth, 10.0f).toNearestInt(),
                        juce::Justification::centred, false);
        }
    }

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (bounds.toFloat(), 3.0f, 1.0f);
}

void FretWearMap::setFromPosition (juce::Point<int> position)
{
    auto bounds = getLocalBounds().reduced (2);

    const int numFrets = CharacterEngine::kMaxFrets + 1;
    const float barWidth = (float) bounds.getWidth() / (float) numFrets;

    if (barWidth <= 0.0f)
        return;

    const int fret = juce::jlimit (0, numFrets - 1,
                                   (int) ((float) (position.x - bounds.getX()) / barWidth));

    const double wear = juce::jlimit (
        0.0, 1.0,
        (double) (bounds.getBottom() - 12 - position.y) / (double) juce::jmax (1, bounds.getHeight() - 12));

    character().setFretWear (fret, wear);

    refresh();

    if (onEdited != nullptr)
        onEdited();
}

void FretWearMap::mouseDown (const juce::MouseEvent& event)
{
    setFromPosition (event.getPosition());
}

void FretWearMap::mouseDrag (const juce::MouseEvent& event)
{
    setFromPosition (event.getPosition());
}

//==============================================================================
CharacterPanel::CharacterPanel (LuthierAudioProcessor& p)
    : processor (p)
{
    buildControls();

    deadSpotMap = std::make_unique<DeadSpotMap> (processor);
    fretWearMap = std::make_unique<FretWearMap> (processor);

    deadSpotMap->onEdited = [this] { refreshFromEngine(); };
    fretWearMap->onEdited = [this] { refreshFromEngine(); };

    addAndMakeVisible (*deadSpotMap);
    addAndMakeVisible (*fretWearMap);

    noiseGroups = std::make_unique<NoiseGroups> (processor);
    addAndMakeVisible (*noiseGroups);

    setupGroup = std::make_unique<SetupGroup> (processor);
    addAndMakeVisible (*setupGroup);

    // SLIDE appears only in Slide Mode, so the panel re-fits when it does.
    slideGroup = std::make_unique<SlideGroup> (processor);
    addChildComponent (*slideGroup);
    slideGroup->onShownChanged = [this] { fitToContent(); };

    fitToContent();

    styleHeading (seedHeading,        "CHARACTER");
    styleHeading (mapsHeading,        "DEAD SPOTS AND FRET WEAR");
    styleHeading (tunerHeading,       "TUNERS");
    styleHeading (electronicsHeading, "AGED ELECTRONICS");
    styleHeading (bodyHeading,        "BODY");
    styleHeading (environmentHeading, "ENVIRONMENT");

    for (auto* label : { &seedHeading, &mapsHeading, &tunerHeading,
                         &electronicsHeading, &bodyHeading, &environmentHeading })
        addAndMakeVisible (*label);

    refreshFromEngine();
    startTimerHz (4);
}

CharacterPanel::~CharacterPanel()
{
    stopTimer();
}

CharacterEngine& CharacterPanel::character()
{
    return processor.getEngine().getCharacterEngine();
}

//==============================================================================
void CharacterPanel::buildControls()
{
    // ---- seed ---------------------------------------------------------------------
    seedLabel.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                                      10.0f, juce::Font::plain)));
    seedLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible (seedLabel);

    newCharacterButton.setTooltip ("Roll a new instrument. The dead spots, the wear "
                                   "and the component tolerances all change, and the "
                                   "new seed is saved into the preset.");

    newCharacterButton.onClick = [this]
    {
        processor.pushUndoAction ("New character", "character-edit", "reroll");   // action-and-undo.md 3.15
        character().reroll();
        refreshFromEngine();
        deadSpotMap->refresh();
        fretWearMap->refresh();
    };

    addAndMakeVisible (newCharacterButton);

    // ---- master ---------------------------------------------------------------------
    enableToggle = std::make_unique<LuthierToggle> ("CHARACTER");
    enableToggle->getButton().setClickingTogglesState (true);
    enableToggle->getButton().onClick = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Turn character on/off", "character-edit", "enabled");   // action-and-undo.md 3.15
        character().setEnabled (enableToggle->getButton().getToggleState());
    };

    enableToggle->setTooltip ("All of the instrument's physical imperfections at once.");
    addAndMakeVisible (*enableToggle);

    styleSlider (amountSlider, 0.0, 100.0, 1.0, " %");
    amountSlider.setTooltip ("How pronounced every character effect is.");
    // The amount is the character macro parameter (gui-integration.md 3.3), so
    // Easy's macro and this slider are one control, automatable and saved.
    amountSlider.onValueChange = [this]
    {
        if (updatingControls)
            return;

        if (auto* p = processor.getState().getParameter (ParamIDs::macroCharacter))
        {
            processor.pushUndoAction ("Change character amount", "character-edit", "amount");   // action-and-undo.md 3.15
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (float) (amountSlider.getValue() * 0.01)));
        }
    };
    addAndMakeVisible (amountSlider);

    // ---- maps -----------------------------------------------------------------------
    refretButton.setTooltip ("New frets: clears the whole wear map.");
    refretButton.onClick = [this] { processor.pushUndoAction ("Refret", "character-edit", "refret"); character().refret(); fretWearMap->refresh(); };
    addAndMakeVisible (refretButton);

    // ---- tuners ---------------------------------------------------------------------
    styleSlider (loosenessSlider, 0.0, 100.0, 1.0, " %");
    loosenessSlider.setTooltip ("How badly the machine heads hold their tuning.");
    loosenessSlider.onValueChange = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Change tuner looseness", "character-edit", "looseness");   // action-and-undo.md 3.15
        character().setTunerLooseness (loosenessSlider.getValue());
    };
    addAndMakeVisible (loosenessSlider);

    retuneButton.setTooltip ("Puts every string back in tune and starts drifting again.");
    retuneButton.onClick = [this] { processor.pushUndoAction ("Retune", "character-edit", "retune"); character().retune(); refreshFromEngine(); };
    addAndMakeVisible (retuneButton);

    driftLabel.setFont (juce::Font (juce::FontOptions (9.0f)));
    driftLabel.setColour (juce::Label::textColourId, Palette::textDisabled);
    addAndMakeVisible (driftLabel);

    // ---- electronics ------------------------------------------------------------------
    styleSlider (potLinearitySlider, 0.0, 100.0, 1.0, " %");
    potLinearitySlider.setTooltip ("How far the volume pot's taper has worn from nominal.");
    potLinearitySlider.onValueChange = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Change pot wear", "character-edit", "potLinearity");   // action-and-undo.md 3.15
        character().setPotLinearityAmount (potLinearitySlider.getValue() * 0.01);
    };
    addAndMakeVisible (potLinearitySlider);

    styleSlider (capDriftSlider, 0.0, 25.0, 0.5, " %");
    capDriftSlider.setTooltip ("How far the tone capacitor may have drifted from its "
                               "marked value.");
    capDriftSlider.onValueChange = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Change capacitor drift", "character-edit", "capDrift");   // action-and-undo.md 3.15
        character().setCapacitorDriftRange (capDriftSlider.getValue() * 0.01);
    };
    addAndMakeVisible (capDriftSlider);

    jackToggle.setClickingTogglesState (true);
    jackToggle.setTooltip ("A dodgy output jack that cuts out briefly now and then. "
                           "Off by default, because it will surprise you.");
    jackToggle.onClick = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Toggle intermittent jack", "character-edit", "jack");   // action-and-undo.md 3.15
        character().setJackIntermittentEnabled (jackToggle.getToggleState());
    };
    addAndMakeVisible (jackToggle);

    boneNutToggle.setClickingTogglesState (true);
    boneNutToggle.setTooltip ("Bone damps the string less than a synthetic nut.");
    boneNutToggle.onClick = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Toggle bone nut", "character-edit", "boneNut");   // action-and-undo.md 3.15
        character().setBoneNut (boneNutToggle.getToggleState());
    };
    addAndMakeVisible (boneNutToggle);

    // ---- body -------------------------------------------------------------------------
    styleSlider (bodyAgeSlider, 0.0, 100.0, 1.0, "");
    bodyAgeSlider.setTooltip ("How long the body has been played in. Older bodies have "
                              "a lower air resonance and less high-frequency damping.");
    bodyAgeSlider.onValueChange = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Change body age", "character-edit", "bodyAge");   // action-and-undo.md 3.15
        character().setBodyAge (bodyAgeSlider.getValue());
    };
    addAndMakeVisible (bodyAgeSlider);

    // ---- environment --------------------------------------------------------------------
    for (int i = 0; i < (int) Temperature::numTemperatures; ++i)
        temperatureBox.addItem (getTemperatureName ((Temperature) i), i + 1);

    temperatureBox.onChange = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Change temperature", "character-edit", "temperature");   // action-and-undo.md 3.15
        character().setTemperature ((Temperature) (temperatureBox.getSelectedId() - 1));
    };

    temperatureBox.setTooltip ("A cold instrument plays sharp, a warm one flat.");
    addAndMakeVisible (temperatureBox);

    for (int i = 0; i < (int) Humidity::numHumidities; ++i)
        humidityBox.addItem (getHumidityName ((Humidity) i), i + 1);

    humidityBox.onChange = [this]
    {
        if (updatingControls)
            return;

        processor.pushUndoAction ("Change humidity", "character-edit", "humidity");   // action-and-undo.md 3.15
        character().setHumidity ((Humidity) (humidityBox.getSelectedId() - 1));
    };

    humidityBox.setTooltip ("Damp wood is lossier and softer; dry wood is stiffer.");
    addAndMakeVisible (humidityBox);

    sessionLabel.setFont (juce::Font (juce::FontOptions (9.0f)));
    sessionLabel.setColour (juce::Label::textColourId, Palette::textDisabled);
    addAndMakeVisible (sessionLabel);

    // ---- presets ---------------------------------------------------------------------------
    allFreshButton.setTooltip ("A machine-perfect instrument: no wear of any kind.");
    allFreshButton.onClick = [this]
    {
        processor.pushUndoAction ("Character: all fresh", "character-edit", "allFresh");   // action-and-undo.md 3.15
        character().setAllFresh();
        refreshFromEngine();
        deadSpotMap->refresh();
        fretWearMap->refresh();
    };

    allOldButton.setTooltip ("A well-used one, for comparison.");
    allOldButton.onClick = [this]
    {
        processor.pushUndoAction ("Character: all old", "character-edit", "allOld");   // action-and-undo.md 3.15
        character().setAllOld();
        refreshFromEngine();
        deadSpotMap->refresh();
        fretWearMap->refresh();
    };

    addAndMakeVisible (allFreshButton);
    addAndMakeVisible (allOldButton);
}

//==============================================================================
void CharacterPanel::refreshFromEngine()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    auto& engine = character();

    seedLabel.setText ("seed " + juce::String::toHexString ((juce::int64) engine.getSeed()),
                       juce::dontSendNotification);

    enableToggle->getButton().setToggleState (engine.isEnabled(), juce::dontSendNotification);
    if (auto* p = processor.getState().getParameter (ParamIDs::macroCharacter))
        amountSlider.setValue (p->getValue() * 100.0, juce::dontSendNotification);

    loosenessSlider.setValue (engine.getTunerLooseness(), juce::dontSendNotification);
    potLinearitySlider.setValue (engine.getPotLinearityAmount() * 100.0, juce::dontSendNotification);

    jackToggle.setToggleState (engine.isJackIntermittentEnabled(), juce::dontSendNotification);
    boneNutToggle.setToggleState (engine.isBoneNut(), juce::dontSendNotification);
    boneNutToggle.setButtonText (engine.isBoneNut() ? "Bone nut" : "Synthetic nut");

    bodyAgeSlider.setValue (engine.getBodyAge(), juce::dontSendNotification);

    temperatureBox.setSelectedId ((int) engine.getTemperature() + 1, juce::dontSendNotification);
    humidityBox.setSelectedId ((int) engine.getHumidity() + 1, juce::dontSendNotification);
}

void CharacterPanel::timerCallback()
{
    auto& engine = character();

    // The live readouts: how far out of tune it has drifted, and how long it has
    // been since the last retune.
    double worstDrift = 0.0;

    for (int s = 0; s < processor.getEngine().getNumStrings(); ++s)
        worstDrift = juce::jmax (worstDrift, std::abs (engine.getTunerDriftCents (s)));

    driftLabel.setText ("worst drift " + juce::String (worstDrift, 2) + " cents",
                        juce::dontSendNotification);

    const int minutes = (int) (engine.getSessionSeconds() / 60.0);
    const int seconds = (int) engine.getSessionSeconds() % 60;

    sessionLabel.setText ("played for " + juce::String (minutes) + "m "
                            + juce::String (seconds) + "s since the last retune",
                          juce::dontSendNotification);
}

//==============================================================================
void CharacterPanel::fitToContent()
{
    setSize (juce::jmax (getWidth(), 200), preferredHeight());
    resized();
}

int CharacterPanel::preferredHeight() const
{
    return 16 + Metrics::buttonHeight + 22            // enable and amount
         + 16 + 22 + 26                               // seed row
         + 16 + DeadSpotMap::preferredHeight
         + FretWearMap::preferredHeight + 26          // maps and refret
         + 16 + 22 + 26 + 12                          // tuners
         + 16 + 22 + 22 + 26                          // electronics
         + 16 + 22                                    // body
         + 16 + 26 + 12                               // environment
         + 26 + 24                                    // presets
         + 8 + noiseGroups->preferredHeight()         // STRING NOISE and PICK
         + 8 + setupGroup->preferredHeight()          // SETUP
         + 8 + slideGroup->preferredHeight();         // SLIDE, only in Slide Mode
}

void CharacterPanel::paint (juce::Graphics& g)
{
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);
}

void CharacterPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    auto row = [&bounds] (int height, int gap = 2)
    {
        auto r = bounds.removeFromTop (height);
        bounds.removeFromTop (gap);
        return r;
    };

    seedHeading.setBounds (row (16));

    enableToggle->setBounds (row (Metrics::buttonHeight));
    amountSlider.setBounds (row (22));

    {
        auto r = row (26);
        newCharacterButton.setBounds (r.removeFromRight (110));
        r.removeFromRight (4);
        seedLabel.setBounds (r);
    }

    // ---- maps ---------------------------------------------------------------------
    mapsHeading.setBounds (row (16));
    deadSpotMap->setBounds (row (DeadSpotMap::preferredHeight));
    fretWearMap->setBounds (row (FretWearMap::preferredHeight));
    refretButton.setBounds (row (26).removeFromLeft (90));

    // ---- tuners -------------------------------------------------------------------
    tunerHeading.setBounds (row (16));
    loosenessSlider.setBounds (row (22));

    {
        auto r = row (26);
        retuneButton.setBounds (r.removeFromLeft (90));
    }

    driftLabel.setBounds (row (12));

    // ---- electronics ---------------------------------------------------------------
    electronicsHeading.setBounds (row (16));
    potLinearitySlider.setBounds (row (22));
    capDriftSlider.setBounds (row (22));

    {
        auto r = row (26);
        jackToggle.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        boneNutToggle.setBounds (r);
    }

    // ---- body ----------------------------------------------------------------------
    bodyHeading.setBounds (row (16));
    bodyAgeSlider.setBounds (row (22));

    // ---- environment -----------------------------------------------------------------
    environmentHeading.setBounds (row (16));

    {
        auto r = row (26);
        temperatureBox.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        humidityBox.setBounds (r);
    }

    sessionLabel.setBounds (row (12));

    // ---- presets --------------------------------------------------------------------
    {
        auto r = row (26);
        allFreshButton.setBounds (r.removeFromLeft (r.getWidth() / 2 - 2));
        r.removeFromLeft (4);
        allOldButton.setBounds (r);
    }

    bounds.removeFromTop (8);
    noiseGroups->setBounds (bounds.removeFromTop (noiseGroups->preferredHeight()));

    bounds.removeFromTop (8);
    setupGroup->setBounds (bounds.removeFromTop (setupGroup->preferredHeight()));

    bounds.removeFromTop (8);
    slideGroup->setBounds (bounds.removeFromTop (slideGroup->preferredHeight()));
}

} // namespace luthier
