#include "TuneExportPanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"

namespace luthier
{

namespace
{
    constexpr int kRow = 26;
    constexpr int kLabelWidth = 84;

    void labelBox (juce::ComboBox& box, const juce::String& name, const juce::String& tooltip)
    {
        box.setTooltip (tooltip);
        AccessibleSetup::configureComboBox (box, name);
    }

    void labelToggle (juce::ToggleButton& toggle, const juce::String& text, const juce::String& tooltip)
    {
        toggle.setButtonText (text);
        toggle.setTooltip (tooltip);
        AccessibleSetup::configureButton (toggle, text, tooltip);
    }
}

//==============================================================================
TuneExportPanel::TuneExportPanel (LuthierAudioProcessor& p, TuneSession& s)
    : OverlayPanel (tr ("tune.export.title")), processor (p), session (s)
{
    folder = PresetManager::getRenderFolder();

    // ---- where -----------------------------------------------------------------------
    addAndMakeVisible (nameEditor);
    nameEditor.setTooltip (tr ("tune.export.name.tooltip"));
    AccessibleSetup::configureDescriptive (nameEditor, tr ("tune.export.name"), tr ("tune.export.name.tooltip"));
    nameEditor.onTextChange = [this] { updateEstimate(); };

    addAndMakeVisible (folderButton);
    folderButton.setButtonText (tr ("common.browse"));
    folderButton.setTooltip (tr ("tune.export.folder.tooltip"));
    AccessibleSetup::configureButton (folderButton, tr ("tune.export.folder"), tr ("tune.export.folder.tooltip"));
    folderButton.onClick = [this] { chooseFolder(); };

    addAndMakeVisible (folderLabel);
    folderLabel.setFont (Fonts::mono (10.5f));
    folderLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    folderLabel.setMinimumHorizontalScale (0.7f);

    // ---- 9.1 audio -------------------------------------------------------------------
    addAndMakeVisible (audioToggle);
    labelToggle (audioToggle, tr ("tune.export.audio"), tr ("tune.export.audio.tooltip"));
    audioToggle.setToggleState (true, juce::dontSendNotification);
    audioToggle.onClick = [this] { updateEnabled(); };

    addAndMakeVisible (formatBox);
    labelBox (formatBox, tr ("tune.export.format"), tr ("tune.export.format.tooltip"));

    for (auto format : { AudioExporter::Format::Wav, AudioExporter::Format::Aiff, AudioExporter::Format::Flac })
        formatBox.addItem (AudioExporter::getFormatName (format), (int) format + 1);

    formatBox.setSelectedId (1, juce::dontSendNotification);
    formatBox.onChange = [this]
    {
        const auto format = (AudioExporter::Format) juce::jmax (0, formatBox.getSelectedId() - 1);
        const auto depths = AudioExporter::getSupportedBitDepths (format);
        const int previous = bitDepthBox.getSelectedId();

        bitDepthBox.clear (juce::dontSendNotification);

        for (int d : depths)
            bitDepthBox.addItem (tr ("tune.export.bits", { { "n", juce::String (d) } }), d);

        bitDepthBox.setSelectedId (depths.contains (previous) ? previous : depths.getLast(), juce::dontSendNotification);
        updateEstimate();
    };

    addAndMakeVisible (bitDepthBox);
    labelBox (bitDepthBox, tr ("tune.export.bitDepth"), tr ("tune.export.bitDepth.tooltip"));

    for (int d : { 16, 24, 32 })
        bitDepthBox.addItem (tr ("tune.export.bits", { { "n", juce::String (d) } }), d);

    bitDepthBox.setSelectedId (24, juce::dontSendNotification);
    bitDepthBox.onChange = [this] { updateEstimate(); };

    addAndMakeVisible (sampleRateBox);
    labelBox (sampleRateBox, tr ("tune.export.sampleRate"), tr ("tune.export.sampleRate.tooltip"));

    for (int rate : { 44100, 48000, 88200, 96000, 176400, 192000 })
        sampleRateBox.addItem (juce::String (rate / 1000.0, 1) + " kHz", rate);

    const int hostRate = (int) std::lround (processor.getSampleRate());
    sampleRateBox.setSelectedId (sampleRateBox.indexOfItemId (hostRate) >= 0 ? hostRate : 48000, juce::dontSendNotification);
    sampleRateBox.onChange = [this] { updateEstimate(); };

    addAndMakeVisible (tailSlider);
    tailSlider.setRange (0.0, 5.0, 0.1);
    tailSlider.setValue (2.0, juce::dontSendNotification);
    tailSlider.setTextValueSuffix (" s");
    tailSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
    tailSlider.setTooltip (tr ("tune.export.tail.tooltip"));
    AccessibleSetup::configureSlider (tailSlider, tr ("tune.export.tail"), " s");
    tailSlider.onValueChange = [this] { updateEstimate(); };

    addAndMakeVisible (stemsBox);
    labelBox (stemsBox, tr ("tune.export.stems"), tr ("tune.export.stems.tooltip"));
    stemsBox.addItem (tr ("tune.export.stems.main"), (int) TuneStemChoice::mainStereo + 1);
    stemsBox.addItem (tr ("tune.export.stems.every"), (int) TuneStemChoice::everyBus + 1);
    stemsBox.setSelectedId (1, juce::dontSendNotification);
    stemsBox.onChange = [this] { updateEstimate(); };

    // ---- 9.2 MIDI --------------------------------------------------------------------
    addAndMakeVisible (midiToggle);
    labelToggle (midiToggle, tr ("tune.export.midi"), tr ("tune.export.midi.tooltip"));
    midiToggle.setToggleState (true, juce::dontSendNotification);
    midiToggle.onClick = [this] { updateEnabled(); };

    addAndMakeVisible (profileBox);
    labelBox (profileBox, tr ("tune.export.profile"), tr ("tune.export.profile.tooltip"));
    profileBox.addItem (tr ("tune.export.profile.luthier"), (int) MidiProfile::luthier + 1);
    profileBox.addItem (tr ("tune.export.profile.generic"), (int) MidiProfile::generic + 1);
    profileBox.setSelectedId (1, juce::dontSendNotification);

    addAndMakeVisible (splitBox);
    labelBox (splitBox, tr ("tune.export.split"), tr ("tune.export.split.tooltip"));
    splitBox.addItem (tr ("tune.export.split.single"), (int) MidiTrackSplit::single + 1);
    splitBox.addItem (tr ("tune.export.split.section"), (int) MidiTrackSplit::perSection + 1);
    splitBox.addItem (tr ("tune.export.split.instrument"), (int) MidiTrackSplit::perInstrument + 1);
    splitBox.addItem (tr ("tune.export.split.string"), (int) MidiTrackSplit::perString + 1);
    splitBox.setSelectedId ((int) MidiTrackSplit::perInstrument + 1, juce::dontSendNotification);

    addAndMakeVisible (realismToggle);
    labelToggle (realismToggle, tr ("tune.export.realism"), tr ("tune.export.realism.tooltip"));
    realismToggle.setToggleState (true, juce::dontSendNotification);

    // ---- 9.3 notation ----------------------------------------------------------------
    addAndMakeVisible (notationToggle);
    labelToggle (notationToggle, tr ("tune.export.notation"), tr ("tune.export.notation.tooltip"));
    notationToggle.onClick = [this] { updateEnabled(); };

    addAndMakeVisible (notationFormatBox);
    labelBox (notationFormatBox, tr ("tune.export.notationFormat"), tr ("tune.export.notationFormat.tooltip"));

    for (auto format : { NotationFormat::musicXml, NotationFormat::guitarPro, NotationFormat::asciiTab })
        notationFormatBox.addItem (getNotationFormatName (format), (int) format + 1);

    notationFormatBox.setSelectedId (1, juce::dontSendNotification);

    addAndMakeVisible (chordSymbolsToggle);
    labelToggle (chordSymbolsToggle, tr ("tune.export.chordSymbols"), tr ("tune.export.chordSymbols.tooltip"));
    chordSymbolsToggle.setToggleState (true, juce::dontSendNotification);

    // ---- 9.4 project -----------------------------------------------------------------
    addAndMakeVisible (projectToggle);
    labelToggle (projectToggle, tr ("tune.export.project"), tr ("tune.export.project.tooltip"));
    projectToggle.setToggleState (true, juce::dontSendNotification);
    projectToggle.onClick = [this] { updateEnabled(); };

    // ---- go --------------------------------------------------------------------------
    addAndMakeVisible (exportButton);
    exportButton.setButtonText (tr ("common.export"));
    exportButton.setColour (juce::TextButton::textColourOffId, Palette::accent);
    exportButton.setTooltip (tr ("tune.export.go.tooltip"));
    AccessibleSetup::configureButton (exportButton, tr ("common.export"), tr ("tune.export.go.tooltip"));
    exportButton.onClick = [this] { startExport(); };

    addAndMakeVisible (cancelButton);
    cancelButton.setButtonText (tr ("common.cancel"));
    cancelButton.setTooltip (tr ("tune.export.cancel.tooltip"));
    AccessibleSetup::configureButton (cancelButton, tr ("common.cancel"), tr ("tune.export.cancel.tooltip"));
    cancelButton.setEnabled (false);
    cancelButton.onClick = [this]
    {
        processor.getExporter().cancelExport();
        stemNames.clear();
        report.errors.add (tr ("tune.export.cancelled"));
        finish();
    };

    addAndMakeVisible (statusLabel);
    statusLabel.setFont (Fonts::ui (11.5f));
    statusLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setMinimumHorizontalScale (0.8f);

    addAndMakeVisible (estimateLabel);
    estimateLabel.setFont (Fonts::mono (11.0f));
    estimateLabel.setColour (juce::Label::textColourId, Palette::textMuted);

    addAndMakeVisible (progressBar);
    progressBar.setVisible (false);

    updateEnabled();
    startTimerHz (10);
}

TuneExportPanel::~TuneExportPanel()
{
    stopTimer();
}

void TuneExportPanel::overlayShown()
{
    const auto& tune = session.getTune();
    nameEditor.setText (tune.meta.title.isNotEmpty() ? tune.meta.title : tr ("tune.untitled"), false);
    updateEnabled();
    updateEstimate();
    AccessibleSetup::announceOverlayOpened (*this, tr ("tune.export.title"));
}

void TuneExportPanel::setFolder (const juce::File& newFolder)
{
    folder = newFolder;
    updateEstimate();
}

//==============================================================================
TuneExportRequest TuneExportPanel::getRequest() const
{
    TuneExportRequest r;
    r.folder = folder;
    r.baseName = nameEditor.getText().trim();

    r.audio = audioToggle.getToggleState();
    r.audioFormat = (AudioExporter::Format) juce::jmax (0, formatBox.getSelectedId() - 1);
    r.bitDepth = juce::jmax (16, bitDepthBox.getSelectedId());
    r.sampleRate = (double) juce::jmax (8000, sampleRateBox.getSelectedId());
    r.tailSeconds = tailSlider.getValue();
    r.stems = (TuneStemChoice) juce::jmax (0, stemsBox.getSelectedId() - 1);

    r.midi = midiToggle.getToggleState();
    r.profile = (MidiProfile) juce::jmax (0, profileBox.getSelectedId() - 1);
    r.split = (MidiTrackSplit) juce::jmax (0, splitBox.getSelectedId() - 1);
    r.realism = realismToggle.getToggleState();

    r.notation = notationToggle.getToggleState();
    r.notationFormat = (NotationFormat) juce::jmax (0, notationFormatBox.getSelectedId() - 1);
    r.chordSymbols = chordSymbolsToggle.getToggleState();

    r.project = projectToggle.getToggleState();
    return r;
}

void TuneExportPanel::setRequest (const TuneExportRequest& r)
{
    folder = r.folder;
    nameEditor.setText (r.baseName, false);

    audioToggle.setToggleState (r.audio, juce::dontSendNotification);
    formatBox.setSelectedId ((int) r.audioFormat + 1, juce::sendNotificationSync);
    bitDepthBox.setSelectedId (r.bitDepth, juce::dontSendNotification);
    sampleRateBox.setSelectedId ((int) std::lround (r.sampleRate), juce::dontSendNotification);
    tailSlider.setValue (r.tailSeconds, juce::dontSendNotification);
    stemsBox.setSelectedId ((int) r.stems + 1, juce::dontSendNotification);

    midiToggle.setToggleState (r.midi, juce::dontSendNotification);
    profileBox.setSelectedId ((int) r.profile + 1, juce::dontSendNotification);
    splitBox.setSelectedId ((int) r.split + 1, juce::dontSendNotification);
    realismToggle.setToggleState (r.realism, juce::dontSendNotification);

    notationToggle.setToggleState (r.notation, juce::dontSendNotification);
    notationFormatBox.setSelectedId ((int) r.notationFormat + 1, juce::dontSendNotification);
    chordSymbolsToggle.setToggleState (r.chordSymbols, juce::dontSendNotification);

    projectToggle.setToggleState (r.project, juce::dontSendNotification);

    updateEnabled();
    updateEstimate();
}

bool TuneExportPanel::isBusy() const
{
    return busy || processor.getExporter().isExporting();
}

//==============================================================================
bool TuneExportPanel::startExport()
{
    if (isBusy())
        return false;

    running = getRequest();
    report = TuneExportReport();

    if (! running.exportsAnything())
    {
        statusLabel.setText (tr ("tune.export.nothingTicked"), juce::dontSendNotification);
        return false;
    }

    if (session.getTune().getPlayOrder().empty())
    {
        statusLabel.setText (tr ("tune.export.noSections"), juce::dontSendNotification);
        return false;
    }

    if (! running.folder.createDirectory().wasOk())
    {
        statusLabel.setText (tr ("tune.export.noFolder", { { "path", running.folder.getFullPathName() } }),
                             juce::dontSendNotification);
        return false;
    }

    // The files that need no render are written now, on this thread.
    report = TuneExport::writeFiles (session.getTune(), session.getMidiOptions(), running);

    if (! running.audio)
    {
        finish();
        return true;
    }

    stemNames = TuneExport::getStemNames (running.stems);
    busy = true;
    exportButton.setEnabled (false);
    cancelButton.setEnabled (true);
    progressBar.setVisible (true);
    startStem (0);
    return true;
}

void TuneExportPanel::startStem (int stemIndex)
{
    if (stemIndex >= stemNames.size())
    {
        finish();
        return;
    }

    const auto& tune = session.getTune();
    const auto file = running.fileFor (stemNames.size() > 1 ? stemNames[stemIndex] : juce::String(),
                                       AudioExporter::getExtension (running.audioFormat));
    const auto options = TuneExport::makeAudioOptions (tune, running, file);

    // 9.1 and TuneSession: the offline instance gets this instance's state
    // with the tune told to play itself, once, from the top.
    session.setRenderIntent (true);
    const auto state = processor.captureStateBlock();
    session.setRenderIntent (false);

    statusLabel.setText (tr ("tune.export.rendering", { { "name", file.getFileName() } }), juce::dontSendNotification);

    const int bus = TuneExport::getStemBusIndex (stemIndex);

    const bool started = processor.getExporter().startExport (
        options, TuneExport::makeRenderSequence (tune), state,
        [bus] { return TuneExport::wrapForBus (LuthierAudioProcessor::createOfflineInstance(), bus); },
        [safe = juce::Component::SafePointer<TuneExportPanel> (this), stemIndex] (const AudioExporter::Result& result)
        {
            if (safe == nullptr)
                return;

            if (result.success)
                safe->report.files.add (result.file);
            else
                safe->report.errors.add (result.message);

            if (result.success && ! safe->stemNames.isEmpty())
                safe->startStem (stemIndex + 1);
            else
                safe->finish();
        });

    if (! started)
    {
        report.errors.add (tr ("tune.export.busy"));
        finish();
    }
}

void TuneExportPanel::finish()
{
    busy = false;
    stemNames.clear();
    exportButton.setEnabled (true);
    cancelButton.setEnabled (false);
    progressBar.setVisible (false);

    statusLabel.setText (report.ok() ? tr ("tune.export.done", { { "n", juce::String (report.files.size()) },
                                                                 { "path", running.folder.getFullPathName() } })
                                     : tr ("tune.export.failed", { { "errors", report.errors.joinIntoString ("; ") } }),
                         juce::dontSendNotification);

    if (onFinished != nullptr)
        onFinished (report);
}

void TuneExportPanel::timerCallback()
{
    if (! busy)
        return;

    progress = processor.getExporter().getProgress();
}

//==============================================================================
void TuneExportPanel::updateEnabled()
{
    const bool audio = audioToggle.getToggleState();

    for (auto* c : { static_cast<juce::Component*> (&formatBox), static_cast<juce::Component*> (&bitDepthBox),
                     static_cast<juce::Component*> (&sampleRateBox), static_cast<juce::Component*> (&tailSlider),
                     static_cast<juce::Component*> (&stemsBox) })
        c->setEnabled (audio);

    const bool midi = midiToggle.getToggleState();

    for (auto* c : { static_cast<juce::Component*> (&profileBox), static_cast<juce::Component*> (&splitBox),
                     static_cast<juce::Component*> (&realismToggle) })
        c->setEnabled (midi);

    const bool notation = notationToggle.getToggleState();
    notationFormatBox.setEnabled (notation);
    chordSymbolsToggle.setEnabled (notation);

    exportButton.setEnabled (! busy && getRequest().exportsAnything());
    updateEstimate();
}

void TuneExportPanel::updateEstimate()
{
    const auto request = getRequest();
    const auto& tune = session.getTune();

    juce::StringArray parts;

    if (request.audio)
    {
        const double seconds = TuneExport::getTuneLengthSeconds (tune) + request.tailSeconds;
        const double bytes = seconds * request.sampleRate * (request.bitDepth / 8.0) * 2.0
                               * (double) TuneExport::getStemNames (request.stems).size();

        parts.add (tr ("tune.export.estimate", { { "seconds", juce::String (seconds, 1) },
                                                 { "size", juce::File::descriptionOfSizeInBytes ((juce::int64) bytes) } }));
    }

    if (request.midi)     parts.add (".mid");
    if (request.notation) parts.add (getNotationFormatExtension (request.notationFormat));
    if (request.project)  parts.add (TuneFile::kFileExtension);

    estimateLabel.setText (parts.joinIntoString ("   "), juce::dontSendNotification);
    folderLabel.setText (request.folder.getFullPathName(), juce::dontSendNotification);
    folderLabel.setTooltip (request.folder.getFullPathName());
}

void TuneExportPanel::chooseFolder()
{
    auto chooser = std::make_shared<juce::FileChooser> (tr ("tune.export.folder.tooltip"), folder);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe = juce::Component::SafePointer<TuneExportPanel> (this), chooser] (const juce::FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult().isDirectory())
            safe->setFolder (fc.getResult());
    });
}

//==============================================================================
void TuneExportPanel::layoutContent (juce::Rectangle<int> content)
{
    auto row = [&content] (int height = kRow)
    {
        auto r = content.removeFromTop (height);
        content.removeFromTop (Metrics::gridHalf);
        return r;
    };

    // Name and folder.
    {
        auto r = row();
        nameEditor.setBounds (r.removeFromLeft (r.getWidth() / 2));
        r.removeFromLeft (Metrics::gridHalf);
        folderButton.setBounds (r.removeFromRight (96));
        r.removeFromRight (Metrics::gridHalf);
        folderLabel.setBounds (r);
    }

    content.removeFromTop (Metrics::gridHalf);

    // Audio.
    {
        auto r = row();
        audioToggle.setBounds (r.removeFromLeft (kLabelWidth));
        formatBox.setBounds (r.removeFromLeft (84));
        r.removeFromLeft (Metrics::gridHalf);
        bitDepthBox.setBounds (r.removeFromLeft (84));
        r.removeFromLeft (Metrics::gridHalf);
        sampleRateBox.setBounds (r.removeFromLeft (96));
        r.removeFromLeft (Metrics::gridHalf);
        stemsBox.setBounds (r);
    }

    {
        auto r = row();
        r.removeFromLeft (kLabelWidth);
        tailSlider.setBounds (r);
    }

    content.removeFromTop (Metrics::gridHalf);

    // MIDI.
    {
        auto r = row();
        midiToggle.setBounds (r.removeFromLeft (kLabelWidth));
        profileBox.setBounds (r.removeFromLeft (120));
        r.removeFromLeft (Metrics::gridHalf);
        splitBox.setBounds (r.removeFromLeft (150));
        r.removeFromLeft (Metrics::gridHalf);
        realismToggle.setBounds (r);
    }

    content.removeFromTop (Metrics::gridHalf);

    // Notation.
    {
        auto r = row();
        notationToggle.setBounds (r.removeFromLeft (kLabelWidth));
        notationFormatBox.setBounds (r.removeFromLeft (150));
        r.removeFromLeft (Metrics::gridHalf);
        chordSymbolsToggle.setBounds (r);
    }

    content.removeFromTop (Metrics::gridHalf);

    // Project.
    projectToggle.setBounds (row());

    estimateLabel.setBounds (row (20));

    auto buttons = content.removeFromBottom (Metrics::buttonHeight);
    exportButton.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (Metrics::gridHalf);
    cancelButton.setBounds (buttons.removeFromRight (100));

    content.removeFromBottom (Metrics::gridHalf);
    progressBar.setBounds (content.removeFromBottom (18));
    content.removeFromBottom (Metrics::gridHalf);

    statusLabel.setBounds (content);
}

} // namespace luthier
