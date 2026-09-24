#include "NotationPanel.h"
#include "MidiExportDefaults.h"
#include "UiPreferences.h"
#include "../PluginProcessor.h"
#include "../Presets/PresetManager.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    constexpr int kHeader = 26;
    constexpr int kRowGap = Metrics::gridHalf;

    /** Quantise grids in quarter notes (6.5): none, 1/4, 1/8, 1/16, 1/8 triplet. */
    constexpr double kGrids[] = { 0.0, 1.0, 0.5, 0.25, 1.0 / 3.0 };
    const char* const kGridNames[] = { "No quantise", "1/4", "1/8", "1/16", "1/8 triplet" };

    /** Scroll speed as refresh period in timer ticks (10 Hz): slow, medium, fast; freeze stops. */
    constexpr int kSpeedTicks[] = { 10, 3, 1, 0 };

    /** The string roll's height; collapsing it hands the room to the live tab. */
    constexpr int kRollHeight = 120;
    constexpr int kTabHeight = 180;
    const char* const kShowRollKey = "notation.showStringRoll";
}

//==============================================================================
NotationFormat NotationTakeExport::formatForFile (const juce::File& file) noexcept
{
    const auto ext = file.getFileExtension().toLowerCase();

    if (ext == ".gp" || ext == ".gpx")                     return NotationFormat::guitarPro;
    if (ext == ".txt")                                      return NotationFormat::asciiTab;
    if (ext == ".mid" || ext == ".midi")                    return NotationFormat::midi;
    return NotationFormat::musicXml;
}

bool NotationTakeExport::write (LuthierAudioProcessor& processor, NotationFormat format, const juce::File& destination,
                                const CaptureScoreOptions& capture, const NotationExportOptions& options,
                                juce::String* error)
{
    auto& take = processor.getPerformanceCapture();
    processor.drainPerformanceCapture();

    if (take.getNotes().empty())
    {
        if (error != nullptr)
            *error = "Nothing has been captured yet. Play something first.";

        return false;
    }

    // MIDI is midi-export's: the capture as a performance, in the MIDI OUT profile.
    if (format == NotationFormat::midi)
    {
        const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
        return MidiProfiles::exportToFile (take.toPerformance (rate, capture), MidiExportDefaults::load(),
                                           destination, error);
    }

    PerformanceScore score;
    take.toScore (score, capture);

    NotationExporter exporter;
    const bool ok = exporter.write (score, format, destination, options);

    if (! ok && error != nullptr)
        *error = exporter.getLastError();

    return ok;
}

//==============================================================================
NotationPanel::NotationPanel (LuthierAudioProcessor& p)
    : processor (p), stringRoll (p)
{
    auto makeToggle = [this] (const juce::String& text, const juce::String& tip, std::function<void()> onClick)
    {
        auto toggle = std::make_unique<LuthierToggle> (text);
        toggle->setTooltip (tip);
        toggle->getButton().setClickingTogglesState (true);
        toggle->getButton().onClick = std::move (onClick);
        AccessibleSetup::configureButton (toggle->getButton(), text, tip);
        addAndMakeVisible (*toggle);
        return toggle;
    };

    auto setUpBox = [this] (juce::ComboBox& box, const juce::String& label, const juce::String& tip)
    {
        box.setTooltip (tip);
        AccessibleSetup::configureComboBox (box, label);
        addAndMakeVisible (box);
    };

    // --- capture ----------------------------------------------------------------------
    offButton = makeToggle ("OFF", "Capture nothing. Costs nothing.", [this] { setCaptureState (CaptureState::off); });
    rollingButton = makeToggle ("ROLLING", "Keep the last minutes of what you play, always - so the good "
                                           "take you did not know you wanted is there.",
                                [this] { setCaptureState (CaptureState::rolling); });
    armedButton = makeToggle ("ARMED", "Clear the take and record from the next note, for a deliberate take.",
                              [this] { setCaptureState (CaptureState::armed); });

    rollingMinutes.setRange (1.0, 60.0, 1.0);
    rollingMinutes.setValue (processor.getPerformanceCapture().getRollingMinutes(), juce::dontSendNotification);
    rollingMinutes.setTextValueSuffix (" min");
    rollingMinutes.setTooltip ("How much a rolling capture keeps.");
    rollingMinutes.onValueChange = [this] { processor.getPerformanceCapture().setRollingMinutes (rollingMinutes.getValue()); };
    AccessibleSetup::configureSlider (rollingMinutes, "Rolling minutes", " min");
    addAndMakeVisible (rollingMinutes);

    clearButton.setTooltip ("Forget everything captured so far.");
    clearButton.onClick = [this] { processor.getPerformanceCapture().clearTake(); refresh(); };
    AccessibleSetup::configureButton (clearButton, "Clear take");
    addAndMakeVisible (clearButton);

    // --- string roll ------------------------------------------------------------------
    showRoll = makeToggle ("SHOW ROLL", "Show the string roll: one lane per string, what you play scrolling by. "
                                        "Click a lane to pluck that string.",
                           [this]
                           {
                               UiPreferences::get().setBool (kShowRollKey, showRoll->getButton().getToggleState());
                               resized();
                           });
    showRoll->getButton().setToggleState (UiPreferences::get().getBool (kShowRollKey, true), juce::dontSendNotification);
    addAndMakeVisible (stringRoll);

    // --- live tab ---------------------------------------------------------------------
    showTab = makeToggle ("SHOW TAB", "Show the last bars of what you played as tab.", [this] { resized(); refresh(); });
    showTab->getButton().setToggleState (true, juce::dontSendNotification);

    for (int bars = 1; bars <= 8; ++bars)
        barsBox.addItem (juce::String (bars) + (bars == 1 ? " bar" : " bars"), bars);

    barsBox.setSelectedId (4, juce::dontSendNotification);
    barsBox.onChange = [this] { refresh(); };
    setUpBox (barsBox, "Bars on screen", "How many bars the live tab shows.");

    densityBox.addItem ("Full", 1);
    densityBox.addItem ("Minimal", 2);
    densityBox.addItem ("Notes only", 3);
    densityBox.setSelectedId (1, juce::dontSendNotification);
    densityBox.onChange = [this] { refresh(); updatePreview(); };
    setUpBox (densityBox, "Symbol density", "How much technique notation the tab shows.");

    speedBox.addItem ("Slow", 1);
    speedBox.addItem ("Medium", 2);
    speedBox.addItem ("Fast", 3);
    speedBox.addItem ("Freeze", 4);
    speedBox.setSelectedId (2, juce::dontSendNotification);
    setUpBox (speedBox, "Scroll speed", "How often the live tab follows your playing; Freeze holds it still.");

    tabView.setMultiLine (true);
    tabView.setReadOnly (true);
    tabView.setScrollbarsShown (true);
    tabView.setFont (Fonts::mono (11.0f));
    tabView.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    tabView.setColour (juce::TextEditor::textColourId, Palette::textPrimary);
    AccessibleSetup::configureDescriptive (tabView, "Live tab", "The last bars of what you played, as tablature");
    addAndMakeVisible (tabView);

    // --- export -----------------------------------------------------------------------
    for (int f = 0; f < (int) NotationFormat::numFormats; ++f)
        formatBox.addItem (getNotationFormatName ((NotationFormat) f), f + 1);

    formatBox.setSelectedId (1, juce::dontSendNotification);
    formatBox.onChange = [this] { resized(); updatePreview(); };
    setUpBox (formatBox, "Notation format", "What to export the take as.");

    rangeBox.addItem ("Entire capture", 1);
    rangeBox.addItem ("Last N seconds", 2);
    rangeBox.setSelectedId (1, juce::dontSendNotification);
    rangeBox.onChange = [this] { resized(); updatePreview(); };
    setUpBox (rangeBox, "Export range", "How much of the take to export.");

    lastSeconds.setRange (1.0, 600.0, 1.0);
    lastSeconds.setValue (30.0, juce::dontSendNotification);
    lastSeconds.setTextValueSuffix (" s");
    lastSeconds.onValueChange = [this] { updatePreview(); };
    AccessibleSetup::configureSlider (lastSeconds, "Last seconds", " s");
    addAndMakeVisible (lastSeconds);

    for (int g = 0; g < (int) (sizeof (kGrids) / sizeof (kGrids[0])); ++g)
        quantiseBox.addItem (kGridNames[g], g + 1);

    quantiseBox.setSelectedId (1, juce::dontSendNotification);
    quantiseBox.onChange = [this] { updatePreview(); };
    setUpBox (quantiseBox, "Quantise", "Snap the export to a grid. The take itself is never quantised.");

    lineWidth.setRange (40.0, 200.0, 1.0);
    lineWidth.setValue (80.0, juce::dontSendNotification);
    lineWidth.setTextValueSuffix (" chars");
    lineWidth.setTooltip ("ASCII tab: the width of a line.");
    lineWidth.onValueChange = [this] { updatePreview(); };
    AccessibleSetup::configureSlider (lineWidth, "Line width", " characters");
    addAndMakeVisible (lineWidth);

    chordDiagrams = makeToggle ("CHORD DIAGRAMS", "Guitar Pro: a chord diagram the first time each chord appears.",
                                [] {});
    chordDiagrams->getButton().setToggleState (true, juce::dontSendNotification);

    previewView.setMultiLine (true);
    previewView.setReadOnly (true);
    previewView.setFont (Fonts::mono (10.0f));
    previewView.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    previewView.setColour (juce::TextEditor::textColourId, Palette::textMuted);
    AccessibleSetup::configureDescriptive (previewView, "Export preview", "The first bar of the export");
    addAndMakeVisible (previewView);

    exportButton.setTooltip ("Write the take in the chosen format.");
    exportButton.onClick = [this] { exportWithChooser(); };
    AccessibleSetup::configureButton (exportButton, "Export notation");
    addAndMakeVisible (exportButton);

    setSize (320, getPreferredHeight());
    refresh();
    startTimerHz (10);
}

NotationPanel::~NotationPanel()
{
    stopTimer();
}

//==============================================================================
juce::Button& NotationPanel::getStateButton (CaptureState state) noexcept
{
    return state == CaptureState::off ? offButton->getButton()
         : state == CaptureState::armed ? armedButton->getButton()
                                        : rollingButton->getButton();
}

void NotationPanel::setCaptureState (CaptureState state)
{
    processor.getPerformanceCapture().setState (state);
    refresh();
}

NotationFormat NotationPanel::currentFormat() const noexcept
{
    return (NotationFormat) juce::jlimit (0, (int) NotationFormat::numFormats - 1, formatBox.getSelectedId() - 1);
}

CaptureScoreOptions NotationPanel::currentCaptureOptions() const
{
    CaptureScoreOptions options;
    options.quantiseBeats = kGrids[(size_t) juce::jlimit (0, 4, quantiseBox.getSelectedId() - 1)];
    options.lastSeconds = rangeBox.getSelectedId() == 2 ? lastSeconds.getValue() : 0.0;
    return options;
}

NotationExportOptions NotationPanel::currentOptions() const
{
    NotationExportOptions options;
    options.lineWidth = (int) lineWidth.getValue();
    options.chordDiagrams = chordDiagrams->getButton().getToggleState();
    options.density = (NotationExportOptions::SymbolDensity) juce::jlimit (0, 2, densityBox.getSelectedId() - 1);
    return options;
}

//==============================================================================
void NotationPanel::refresh()
{
    auto& take = processor.getPerformanceCapture();
    const auto state = take.getState();

    offButton->getButton().setToggleState (state == CaptureState::off, juce::dontSendNotification);
    rollingButton->getButton().setToggleState (state == CaptureState::rolling, juce::dontSendNotification);
    armedButton->getButton().setToggleState (state == CaptureState::armed, juce::dontSendNotification);
    rollingMinutes.setEnabled (state == CaptureState::rolling);

    statusText = juce::String ((int) take.getNotes().size()) + " notes, "
               + juce::String ((int) take.getChords().size()) + " chord changes"
               + (take.getDroppedCount() > 0 ? ", " + juce::String (take.getDroppedCount()) + " dropped" : juce::String());

    // 4: the chord symbols as they changed, the latest last.
    juce::StringArray recent;

    for (const auto& chord : take.getChords())
        if (recent.isEmpty() || recent[recent.size() - 1] != chord.name)
            recent.add (chord.name);

    recent.removeRange (0, juce::jmax (0, recent.size() - 8));
    chordHistory = recent.isEmpty() ? juce::String ("Chords: none yet") : "Chords: " + recent.joinIntoString ("  ");

    const bool shown = showTab->getButton().getToggleState();
    tabView.setVisible (shown);

    if (shown)
    {
        const auto density = (NotationExportOptions::SymbolDensity) juce::jlimit (0, 2, densityBox.getSelectedId() - 1);
        tabView.setText (take.renderLiveTab (juce::jlimit (1, 8, barsBox.getSelectedId()), density), false);
    }

    exportButton.setEnabled (! take.getNotes().empty());

    if (take.getNotes().size() != shownNotes)
    {
        shownNotes = take.getNotes().size();
        updatePreview();
    }

    repaint (statusBounds.getUnion (chordBounds));
}

void NotationPanel::updatePreview()
{
    const auto format = currentFormat();
    auto& take = processor.getPerformanceCapture();

    lineWidth.setVisible (format == NotationFormat::asciiTab);
    chordDiagrams->setVisible (format == NotationFormat::guitarPro);
    lastSeconds.setVisible (rangeBox.getSelectedId() == 2);

    // 5: a preview of the first bar, for MusicXML and ASCII.
    if (take.getNotes().empty() || (format != NotationFormat::musicXml && format != NotationFormat::asciiTab))
    {
        previewView.setText (take.getNotes().empty() ? "Play something and the first bar of the export shows here."
                                                     : "No preview for this format.", false);
        return;
    }

    PerformanceScore score;
    take.toScore (score, currentCaptureOptions());

    NotationExporter exporter;
    auto options = currentOptions();
    juce::String text;

    if (format == NotationFormat::asciiTab)
    {
        text = exporter.renderAsciiTabWindow (score, 0, 1, options);
    }
    else
    {
        // The first measure's XML: up to the end of it.
        const auto xml = exporter.renderMusicXml (score, options);
        const int end = xml.indexOf ("</measure>");
        text = end > 0 ? xml.substring (juce::jmax (0, xml.indexOf ("<measure")), end + 10) : xml.substring (0, 1200);
    }

    previewView.setText (text, false);
}

void NotationPanel::timerCallback()
{
    // 3's scroll speed is how often the view follows; Freeze holds it.
    const int every = kSpeedTicks[(size_t) juce::jlimit (0, 3, speedBox.getSelectedId() - 1)];

    if (every > 0 && ++tabTicks >= every)
    {
        tabTicks = 0;
        refresh();
    }
}

//==============================================================================
bool NotationPanel::exportTo (const juce::File& destination, juce::String* error)
{
    return NotationTakeExport::write (processor, currentFormat(), destination, currentCaptureOptions(),
                                      currentOptions(), error);
}

void NotationPanel::exportWithChooser()
{
    const auto format = currentFormat();
    const auto file = PresetManager::getRenderFolder().getChildFile (
        "Luthier Take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S")
        + getNotationFormatExtension (format));

    chooser = std::make_unique<juce::FileChooser> ("Export the take as notation", file,
                                                   juce::String ("*") + getNotationFormatExtension (format));

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<NotationPanel> (this)] (const juce::FileChooser& fc)
    {
        const auto chosen = fc.getResult();

        if (safe == nullptr || chosen == juce::File())
            return;

        juce::String error;
        const bool ok = safe->exportTo (chosen, &error);

        juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                               .withIconType (ok ? juce::MessageBoxIconType::InfoIcon
                                                                 : juce::MessageBoxIconType::WarningIcon)
                                               .withTitle (ok ? "Notation exported" : "Could not export")
                                               .withMessage (ok ? "Saved to\n" + chosen.getFullPathName() : error)
                                               .withButton ("OK"),
                                           nullptr);
    });
}

//==============================================================================
int NotationPanel::getPreferredHeight() const
{
    const int button = Metrics::buttonHeight;

    return kHeader + 2 * (button + kRowGap) + 32 + Metrics::grid
         + kHeader + (button + kRowGap) + kRollHeight + Metrics::grid
         + kHeader + 2 * (button + kRowGap) + kTabHeight + 22 + Metrics::grid
         + kHeader + 3 * (button + kRowGap) + 110 + button + Metrics::grid;
}

void NotationPanel::paint (juce::Graphics& g)
{
    LuthierLookAndFeel::drawSectionHeader (g, captureHeader, "CAPTURE");
    LuthierLookAndFeel::drawSectionHeader (g, rollHeader, "STRING ROLL");
    LuthierLookAndFeel::drawSectionHeader (g, tabHeader, "LIVE TAB");
    LuthierLookAndFeel::drawSectionHeader (g, exportHeader, "EXPORT");

    g.setFont (Fonts::ui (11.0f));
    g.setColour (Palette::textMuted);
    g.drawFittedText (statusText, statusBounds, juce::Justification::centredLeft, 2);
    g.drawFittedText (chordHistory, chordBounds, juce::Justification::centredLeft, 1);
}

void NotationPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::grid, 0);
    const int button = Metrics::buttonHeight;

    auto row = [&bounds, button]
    {
        auto r = bounds.removeFromTop (button);
        bounds.removeFromTop (kRowGap);
        return r;
    };

    auto split = [] (juce::Rectangle<int> r, std::initializer_list<juce::Component*> parts)
    {
        const int w = juce::jmax (1, r.getWidth() / (int) parts.size());

        for (auto* c : parts)
            c->setBounds (r.removeFromLeft (w).reduced (1));
    };

    captureHeader = bounds.removeFromTop (kHeader);
    split (row(), { offButton.get(), rollingButton.get(), armedButton.get() });
    split (row(), { &rollingMinutes, &clearButton });
    statusBounds = bounds.removeFromTop (32);
    bounds.removeFromTop (Metrics::grid);

    // The roll's room goes to the live tab when the roll is collapsed, so the
    // panel's height (what the column was told) does not change.
    const bool rollShown = showRoll->getButton().getToggleState();
    rollHeader = bounds.removeFromTop (kHeader);
    showRoll->setBounds (row().reduced (1));
    stringRoll.setVisible (rollShown);

    if (rollShown)
        stringRoll.setBounds (bounds.removeFromTop (kRollHeight));

    bounds.removeFromTop (Metrics::grid);

    tabHeader = bounds.removeFromTop (kHeader);
    split (row(), { showTab.get(), &barsBox });
    split (row(), { &densityBox, &speedBox });
    tabView.setBounds (bounds.removeFromTop (kTabHeight + (rollShown ? 0 : kRollHeight)));
    chordBounds = bounds.removeFromTop (22);
    bounds.removeFromTop (Metrics::grid);

    exportHeader = bounds.removeFromTop (kHeader);
    split (row(), { &formatBox, &quantiseBox });
    split (row(), { &rangeBox, &lastSeconds });

    {
        auto r = row();

        // The format's own option: ASCII's line width or Guitar Pro's diagrams.
        lineWidth.setBounds (r.reduced (1));
        chordDiagrams->setBounds (r.reduced (1));
    }

    previewView.setBounds (bounds.removeFromTop (110));
    exportButton.setBounds (bounds.removeFromTop (button).reduced (1));

    updatePreview();
}

} // namespace luthier
