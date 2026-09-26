#include "TuneExportDialog.h"
#include "Theme.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Tune/TuneFile.h"

namespace luthier
{

namespace
{
    constexpr int kRow = 26;
    constexpr int kGap = 6;
    constexpr int kLabel = 110;

    const double kRates[] = { 0.0, 44100.0, 48000.0, 88200.0, 96000.0 };   // 0 = the host's

    class WorkerThread : public juce::Thread
    {
    public:
        WorkerThread (std::function<void()> job) : juce::Thread ("Tune export"), work (std::move (job)) {}
        void run() override { work(); }

    private:
        std::function<void()> work;
    };
}

TuneExportDialog::TuneExportDialog (LuthierAudioProcessor& p)
    : processor (p), folder (PresetManager::getRenderFolder())
{
    const char* names[] = { "AUDIO", "MIDI", "NOTATION", "PROJECT" };

    for (int i = 0; i < 4; ++i)
    {
        auto* b = destinationButtons.add (new juce::TextButton (names[i]));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (0x7e);
        b->onClick = [this, i] { setDestination ((Destination) i); };
        AccessibleSetup::configureButton (*b, juce::String ("Export ") + names[i]);
        addAndMakeVisible (b);
    }

    // --- audio (9.1) ----------------------------------------------------------------
    formatBox.addItem ("WAV", 1);
    formatBox.addItem ("AIFF", 2);
    formatBox.addItem ("FLAC", 3);
    formatBox.setSelectedId (1, juce::dontSendNotification);
    formatBox.setTooltip ("MP3 is not offered: this build has no MP3 encoder. Export WAV or FLAC and convert.");

    bitDepthBox.addItem ("16-bit", 16);
    bitDepthBox.addItem ("24-bit", 24);
    bitDepthBox.addItem ("32-bit float", 32);
    bitDepthBox.setSelectedId (24, juce::dontSendNotification);

    const double host = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    sampleRateBox.addItem ("Host (" + juce::String (host / 1000.0, 1) + " kHz)", 1);

    for (int i = 1; i < (int) std::size (kRates); ++i)
        sampleRateBox.addItem (juce::String (kRates[i] / 1000.0, 1) + " kHz", i + 1);

    sampleRateBox.setSelectedId (1, juce::dontSendNotification);

    tailSlider.setRange (0.0, TuneExport::kMaxTailSeconds, 0.1);
    tailSlider.setValue (2.0, juce::dontSendNotification);
    tailSlider.setTextValueSuffix (" s");
    tailSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, kRow - 4);

    // --- MIDI (9.2) ------------------------------------------------------------------
    profileBox.addItem ("Luthier profile (full realism, round-trips)", 1);
    profileBox.addItem ("Generic profile (any DAW or instrument)", 2);
    profileBox.setSelectedId (1, juce::dontSendNotification);

    splitBox.addItem ("Single track", 1);
    splitBox.addItem ("A track per section", 2);
    splitBox.addItem ("A track per instrument (main, bass)", 3);
    splitBox.addItem ("A track per string", 4);
    splitBox.setSelectedId (3, juce::dontSendNotification);
    realismToggle.setToggleState (true, juce::dontSendNotification);

    // --- notation (9.3) ------------------------------------------------------------
    notationBox.addItem ("MusicXML (standard and tab)", 1);
    notationBox.addItem ("Guitar Pro (standard and tab)", 2);
    notationBox.addItem ("ASCII tab", 3);
    notationBox.setSelectedId (1, juce::dontSendNotification);
    chordsToggle.setToggleState (true, juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &formatBox, &bitDepthBox, &sampleRateBox, &stemsToggle, &tailSlider,
                                                              &profileBox, &splitBox, &realismToggle, &notationBox, &chordsToggle,
                                                              &bundleToggle, &nameEditor, &folderButton, &exportButton })
        addAndMakeVisible (c);

    for (auto* box : { &formatBox, &bitDepthBox, &sampleRateBox, &profileBox, &splitBox, &notationBox })
        box->onChange = [this] { repaint(); };

    AccessibleSetup::configureComboBox (formatBox, "Audio format");
    AccessibleSetup::configureComboBox (bitDepthBox, "Bit depth");
    AccessibleSetup::configureComboBox (sampleRateBox, "Sample rate");
    AccessibleSetup::configureSlider (tailSlider, "Loop tail", " seconds");
    AccessibleSetup::configureComboBox (profileBox, "MIDI profile");
    AccessibleSetup::configureComboBox (splitBox, "Track split");
    AccessibleSetup::configureComboBox (notationBox, "Notation format");
    AccessibleSetup::configureDescriptive (nameEditor, "File name", "The exported file's name.");
    AccessibleSetup::configureButton (exportButton, "Export");

    const auto& title = processor.getTuneSession().getTune().meta.title;
    nameEditor.setText (juce::File::createLegalFileName (title.isNotEmpty() ? title : juce::String ("Untitled Tune")),
                        juce::dontSendNotification);
    nameEditor.onTextChange = [this] { repaint(); };

    folderButton.onClick = [this] { chooseFolder(); };
    exportButton.onClick = [this]
    {
        if (destination == Destination::audio)
        {
            startAudioExport();
            return;
        }

        juce::String message;
        exportNow (message);
        status = message;
        repaint();
    };

    setDestination (Destination::audio);
    setSize (560, 360);
}

TuneExportDialog::~TuneExportDialog()
{
    stopTimer();
    cancel = true;

    if (worker != nullptr)
        worker->stopThread (10000);
}

void TuneExportDialog::setDestination (Destination d)
{
    destination = d;

    for (int i = 0; i < destinationButtons.size(); ++i)
        destinationButtons[i]->setToggleState (i == (int) d, juce::dontSendNotification);

    refreshVisibility();
    resized();
    repaint();
}

void TuneExportDialog::setFolder (const juce::File& f)
{
    folder = f;
    repaint();
}

void TuneExportDialog::refreshVisibility()
{
    const bool audio = destination == Destination::audio, midi = destination == Destination::midi,
               notation = destination == Destination::notation, project = destination == Destination::project;

    for (auto* c : std::initializer_list<juce::Component*> { &formatBox, &bitDepthBox, &sampleRateBox, &stemsToggle, &tailSlider })
        c->setVisible (audio);

    for (auto* c : std::initializer_list<juce::Component*> { &profileBox, &splitBox, &realismToggle })
        c->setVisible (midi);

    notationBox.setVisible (notation);
    chordsToggle.setVisible (notation);
    bundleToggle.setVisible (project);
}

TuneExport::AudioOptions TuneExportDialog::getAudioOptions() const
{
    TuneExport::AudioOptions o;
    o.folder = folder;
    o.baseName = nameEditor.getText().trim().isNotEmpty() ? nameEditor.getText().trim() : juce::String ("Tune");
    o.format = (AudioExporter::Format) juce::jmax (0, formatBox.getSelectedId() - 1);
    o.bitDepth = juce::jmax (16, bitDepthBox.getSelectedId());
    const int rate = sampleRateBox.getSelectedId() - 1;
    o.sampleRate = rate > 0 && rate < (int) std::size (kRates) ? kRates[rate]
                 : (processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0);
    o.stems = stemsToggle.getToggleState();
    o.tailSeconds = tailSlider.getValue();
    return o;
}

TuneExport::MidiOptions TuneExportDialog::getMidiOptions() const
{
    TuneExport::MidiOptions o;
    o.profile.profile = profileBox.getSelectedId() == 2 ? MidiProfile::generic : MidiProfile::luthier;
    o.profile.includeRealism = realismToggle.getToggleState();

    switch (splitBox.getSelectedId())
    {
        case 2:  o.profile.split = MidiTrackSplit::perSection; break;
        case 3:  o.profile.split = MidiTrackSplit::perInstrument; break;
        case 4:  o.profile.split = MidiTrackSplit::perString; break;
        default: o.profile.split = MidiTrackSplit::single; break;
    }

    o.includeRealism = realismToggle.getToggleState();
    return o;
}

NotationFormat TuneExportDialog::getNotationFormat() const
{
    switch (notationBox.getSelectedId())
    {
        case 2:  return NotationFormat::guitarPro;
        case 3:  return NotationFormat::asciiTab;
        default: return NotationFormat::musicXml;
    }
}

juce::File TuneExportDialog::getTargetFile() const
{
    const auto base = juce::File::createLegalFileName (getAudioOptions().baseName);

    switch (destination)
    {
        case Destination::audio:    return folder.getChildFile (base + AudioExporter::getExtension (getAudioOptions().format));
        case Destination::midi:     return folder.getChildFile (base + ".mid");
        case Destination::notation: return folder.getChildFile (base + getNotationFormatExtension (getNotationFormat()));
        case Destination::project:  return folder.getChildFile (base + TuneFile::kFileExtension);
    }

    return {};
}

juce::Array<juce::File> TuneExportDialog::exportNow (juce::String& message)
{
    juce::Array<juce::File> files;
    juce::String error;
    const auto& tune = processor.getTuneSession().getTune();
    const auto target = getTargetFile();
    folder.createDirectory();

    switch (destination)
    {
        case Destination::audio:
            files = TuneExport::exportAudio (processor.captureStateBlock(), getAudioOptions(), error);
            break;

        case Destination::midi:
            if (TuneExport::exportMidi (tune, target, getMidiOptions(), error))
                files.add (target);
            break;

        case Destination::notation:
        {
            NotationExportOptions options;
            options.chordSymbols = chordsToggle.getToggleState();

            if (TuneExport::exportNotation (tune, getNotationFormat(), target, options, error))
                files.add (target);
            break;
        }

        case Destination::project:
            if (TuneExport::exportProject (tune, target, bundleToggle.getToggleState(),
                                           processor.getPresetManager().toVar(),
                                           processor.getCurrentGuitar().toVar(), error))
                files.add (target);
            break;
    }

    message = files.isEmpty() ? "Export failed: " + error
                              : "Exported " + juce::String (files.size()) + (files.size() == 1 ? " file" : " files")
                                  + " to " + folder.getFullPathName();
    return files;
}

void TuneExportDialog::startAudioExport()
{
    if (worker != nullptr && worker->isThreadRunning())
    {
        cancel = true;   // the button doubles as Cancel while rendering
        return;
    }

    const auto state = processor.captureStateBlock();
    const auto options = getAudioOptions();
    cancel = false;
    progress = 0.0;
    workerDone = false;

    worker = std::make_unique<WorkerThread> ([this, state, options]
    {
        juce::String error;
        const auto files = TuneExport::exportAudio (state, options, error, [this] (double p)
        {
            progress = p;
            return ! cancel.load();
        });

        const juce::ScopedLock lock (resultLock);
        workerMessage = files.isEmpty() ? (cancel.load() ? juce::String ("Export cancelled.") : "Export failed: " + error)
                                        : "Exported " + juce::String (files.size()) + " file"
                                            + (files.size() == 1 ? "" : "s") + " to " + options.folder.getFullPathName();
        workerDone = true;
    });

    worker->startThread();
    exportButton.setButtonText ("CANCEL");
    startTimerHz (15);
}

void TuneExportDialog::timerCallback()
{
    bool done = false;

    {
        const juce::ScopedLock lock (resultLock);
        done = workerDone;

        if (done)
            status = workerMessage;
    }

    if (done)
    {
        stopTimer();
        progress = -1.0;
        exportButton.setButtonText ("EXPORT");
    }
    else
    {
        status = "Rendering... " + juce::String (juce::roundToInt (progress.load() * 100.0)) + "%";
    }

    repaint();
}

void TuneExportDialog::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Export to", folder);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe = juce::Component::SafePointer<TuneExportDialog> (this)] (const juce::FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult() != juce::File())
            safe->setFolder (fc.getResult());
    });
}

void TuneExportDialog::launch (LuthierAudioProcessor& processor, juce::Component* parent)
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new TuneExportDialog (processor));
    options.dialogTitle = "Export tune";
    options.dialogBackgroundColour = Palette::panel;
    options.componentToCentreAround = parent;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

void TuneExportDialog::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);
    g.setFont (Fonts::label());
    g.setColour (Palette::textMuted);

    for (int i = 0; i < labelAreas.size(); ++i)
        g.drawText (labels[i], labelAreas[i], juce::Justification::centredLeft);

    g.setFont (Fonts::ui (11.0f));
    g.setColour (Palette::textMuted);
    g.drawFittedText ("Writes " + getTargetFile().getFullPathName(),
                      getLocalBounds().reduced (12).removeFromBottom (48).removeFromTop (20), juce::Justification::centredLeft, 1);

    if (progress.load() >= 0.0)
    {
        auto bar = getLocalBounds().reduced (12).removeFromBottom (24).removeFromLeft (getWidth() - 140).toFloat();
        g.setColour (Palette::panelSunken);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (Palette::accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) juce::jlimit (0.0, 1.0, progress.load())), 3.0f);
    }
    else
    {
        g.setColour (status.startsWith ("Export failed") ? Palette::warning : Palette::textPrimary);
        g.drawFittedText (status, getLocalBounds().reduced (12).removeFromBottom (24).withTrimmedRight (120),
                          juce::Justification::centredLeft, 2);
    }
}

void TuneExportDialog::resized()
{
    auto area = getLocalBounds().reduced (12);
    labelAreas.clear();
    labels.clear();

    {
        auto row = area.removeFromTop (kRow);
        const int w = row.getWidth() / 4;

        for (auto* b : destinationButtons)
            b->setBounds (row.removeFromLeft (w).reduced (2, 0));

        area.removeFromTop (kGap * 2);
    }

    auto labelled = [this, &area] (const char* label, juce::Component& c)
    {
        auto row = area.removeFromTop (kRow);
        labelAreas.add (row.removeFromLeft (kLabel));
        labels.add (label);
        c.setBounds (row);
        area.removeFromTop (kGap);
    };

    auto plain = [&area] (juce::Component& c)
    {
        c.setBounds (area.removeFromTop (kRow));
        area.removeFromTop (kGap);
    };

    switch (destination)
    {
        case Destination::audio:
            labelled ("FORMAT", formatBox);
            labelled ("BIT DEPTH", bitDepthBox);
            labelled ("SAMPLE RATE", sampleRateBox);
            labelled ("LOOP TAIL", tailSlider);
            plain (stemsToggle);
            break;

        case Destination::midi:
            labelled ("PROFILE", profileBox);
            labelled ("TRACKS", splitBox);
            plain (realismToggle);
            break;

        case Destination::notation:
            labelled ("FORMAT", notationBox);
            plain (chordsToggle);
            break;

        case Destination::project:
            plain (bundleToggle);
            break;
    }

    auto bottom = getLocalBounds().reduced (12);
    auto buttons = bottom.removeFromBottom (24);
    exportButton.setBounds (buttons.removeFromRight (110));
    bottom.removeFromBottom (24);   // the "Writes ..." line

    auto nameRow = bottom.removeFromBottom (kRow);
    labelAreas.add (nameRow.removeFromLeft (kLabel));
    labels.add ("FILE NAME");
    folderButton.setBounds (nameRow.removeFromRight (90));
    nameRow.removeFromRight (kGap);
    nameEditor.setBounds (nameRow);
}

} // namespace luthier
