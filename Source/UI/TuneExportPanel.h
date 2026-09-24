#pragma once

/*  The TUNE tab's one-screen export dialog (tune-builder.md 9; 2.6 "the
    export dialog is one screen"; gui-integration 4.4 and 19; DECISIONS C-53).

    Four destinations on one screen, each with a tick: audio (format, depth,
    rate, tail, main stereo or every routing bus as a stem), MIDI (Luthier or
    Generic profile, track split, realism), notation (MusicXML, Guitar Pro or
    ASCII tab) and the project file. One folder, one base name, one EXPORT.

    Nothing here renders or writes on its own: the files come from
    TuneExport's writers, and the audio from the plugin's AudioExporter
    rendering an offline instance that carries the tune in its state
    (TuneSession's render intent). Stems are the same render once per bus,
    chained on the exporter's completion, so a cancel stops the chain.

    An OverlayPanel, so the editor's OverlayHost shows it like the header's
    Export; the TUNE panel opens it in a window of its own when no host is
    wired. Every control has a tooltip and an accessible name; the strings
    are the catalog's (accessibility 0.4).
*/

#include "Overlays.h"
#include "../Tune/TuneExport.h"
#include "../Tune/TuneSession.h"

namespace luthier
{

class LuthierAudioProcessor;

class TuneExportPanel : public OverlayPanel,
                        private juce::Timer
{
public:
    TuneExportPanel (LuthierAudioProcessor& processor, TuneSession& session);
    ~TuneExportPanel() override;

    juce::Point<int> getPreferredSize() const override { return { 640, 560 }; }

    void overlayShown() override;

    /** The screen as a request, and back. */
    TuneExportRequest getRequest() const;
    void setRequest (const TuneExportRequest& request);

    /** EXPORT: writes the ticked files now and starts the audio render.
        False when nothing is ticked, the folder cannot be made, or a render
        is already running. */
    bool startExport();

    /** True while the audio render (or a stem chain) runs. */
    bool isBusy() const;

    /** What the last export wrote and what failed; the audio files join it
        as each render finishes. */
    const TuneExportReport& getReport() const noexcept { return report; }

    /** Called when every destination has finished, with the report. */
    std::function<void (const TuneExportReport&)> onFinished;

    //==========================================================================
    // For tests: the controls.
    juce::TextEditor& getNameEditor() noexcept        { return nameEditor; }
    juce::ToggleButton& getAudioToggle() noexcept     { return audioToggle; }
    juce::ComboBox& getFormatBox() noexcept           { return formatBox; }
    juce::ComboBox& getBitDepthBox() noexcept         { return bitDepthBox; }
    juce::ComboBox& getSampleRateBox() noexcept       { return sampleRateBox; }
    juce::Slider& getTailSlider() noexcept            { return tailSlider; }
    juce::ComboBox& getStemsBox() noexcept            { return stemsBox; }
    juce::ToggleButton& getMidiToggle() noexcept      { return midiToggle; }
    juce::ComboBox& getProfileBox() noexcept          { return profileBox; }
    juce::ComboBox& getSplitBox() noexcept            { return splitBox; }
    juce::ToggleButton& getRealismToggle() noexcept   { return realismToggle; }
    juce::ToggleButton& getNotationToggle() noexcept  { return notationToggle; }
    juce::ComboBox& getNotationFormatBox() noexcept   { return notationFormatBox; }
    juce::ToggleButton& getChordSymbolsToggle() noexcept { return chordSymbolsToggle; }
    juce::ToggleButton& getProjectToggle() noexcept   { return projectToggle; }
    juce::TextButton& getExportButton() noexcept      { return exportButton; }
    juce::TextButton& getCancelButton() noexcept      { return cancelButton; }
    juce::String getStatusText() const                { return statusLabel.getText(); }
    juce::File getFolder() const                      { return folder; }
    void setFolder (const juce::File& newFolder);

protected:
    void layoutContent (juce::Rectangle<int> content) override;

private:
    void timerCallback() override;
    void updateEnabled();
    void updateEstimate();
    void startStem (int stemIndex);
    void finish();
    void chooseFolder();

    LuthierAudioProcessor& processor;
    TuneSession& session;

    juce::File folder;
    juce::TextEditor nameEditor;
    juce::TextButton folderButton;
    juce::Label folderLabel;

    juce::ToggleButton audioToggle, midiToggle, notationToggle, projectToggle;
    juce::ComboBox formatBox, bitDepthBox, sampleRateBox, stemsBox;
    juce::Slider tailSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ComboBox profileBox, splitBox;
    juce::ToggleButton realismToggle, chordSymbolsToggle;
    juce::ComboBox notationFormatBox;

    juce::TextButton exportButton, cancelButton;
    juce::Label statusLabel, estimateLabel;
    juce::ProgressBar progressBar { progress };
    double progress = 0.0;

    TuneExportRequest running;
    TuneExportReport report;
    juce::StringArray stemNames;
    bool busy = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneExportPanel)
};

} // namespace luthier
