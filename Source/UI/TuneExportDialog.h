#pragma once

/*  The TUNE tab's export dialog (tune-builder.md 2.6 "the export dialog is one
    screen", 9 "One dialog, four destinations"). TUNE-HELP-ONBOARDING workstream.

      AUDIO     format, bit depth, sample rate (the host's or chosen), stems
                (every aux bus as its own file), loop tail 0-5 s
      MIDI      Luthier or Generic profile, track split, realism or plain
      NOTATION  MusicXML, Guitar Pro or ASCII tab, chord symbols
      PROJECT   the .luthiertune, optionally with its preset and guitar

    Everything on one screen: the destination row picks which settings show,
    the file name and folder sit under them, and EXPORT writes it. Audio renders
    on a worker thread with progress; the rest are quick and run at once. The
    files go to ~/Documents/Luthier/Renders unless another folder is chosen.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Support/TuneExport.h"

#include <atomic>

namespace luthier
{

class LuthierAudioProcessor;

class TuneExportDialog : public juce::Component,
                         private juce::Timer
{
public:
    enum class Destination { audio = 0, midi, notation, project };

    explicit TuneExportDialog (LuthierAudioProcessor& processor);
    ~TuneExportDialog() override;

    void setDestination (Destination d);
    Destination getDestination() const noexcept { return destination; }

    /** The file the current settings would write (the main mix, for audio). */
    juce::File getTargetFile() const;

    void setFolder (const juce::File& folder);

    /** Exports now, on this thread (tests; the button runs audio on a worker).
        Returns the files written and sets `message` to what the user is told. */
    juce::Array<juce::File> exportNow (juce::String& message);

    /** The settings, as the controls hold them. */
    TuneExport::AudioOptions getAudioOptions() const;
    TuneExport::MidiOptions getMidiOptions() const;
    NotationFormat getNotationFormat() const;

    // For tests: the controls.
    juce::ComboBox& getFormatBox() noexcept        { return formatBox; }
    juce::ComboBox& getBitDepthBox() noexcept      { return bitDepthBox; }
    juce::ComboBox& getSampleRateBox() noexcept    { return sampleRateBox; }
    juce::ToggleButton& getStemsToggle() noexcept  { return stemsToggle; }
    juce::Slider& getTailSlider() noexcept         { return tailSlider; }
    juce::ComboBox& getProfileBox() noexcept       { return profileBox; }
    juce::ComboBox& getSplitBox() noexcept         { return splitBox; }
    juce::ToggleButton& getRealismToggle() noexcept { return realismToggle; }
    juce::ComboBox& getNotationBox() noexcept      { return notationBox; }
    juce::ToggleButton& getChordsToggle() noexcept { return chordsToggle; }
    juce::ToggleButton& getBundleToggle() noexcept { return bundleToggle; }
    juce::TextEditor& getNameEditor() noexcept     { return nameEditor; }
    juce::TextButton& getExportButton() noexcept   { return exportButton; }
    juce::TextButton& getDestinationButton (Destination d) noexcept { return *destinationButtons[(int) d]; }
    juce::String getStatus() const                 { return status; }

    /** Opens the dialog in a window of its own over `parent`. */
    static void launch (LuthierAudioProcessor& processor, juce::Component* parent);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void startAudioExport();
    void chooseFolder();
    void refreshVisibility();

    LuthierAudioProcessor& processor;
    Destination destination = Destination::audio;
    juce::File folder;

    juce::OwnedArray<juce::TextButton> destinationButtons;

    juce::ComboBox formatBox, bitDepthBox, sampleRateBox;
    juce::ToggleButton stemsToggle { "Stems: every aux bus as its own file" };
    juce::Slider tailSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    juce::ComboBox profileBox, splitBox;
    juce::ToggleButton realismToggle { "Realism events (bends, slides, vibrato)" };

    juce::ComboBox notationBox;
    juce::ToggleButton chordsToggle { "Chord symbols above the staff" };

    juce::ToggleButton bundleToggle { "Bundle the preset and guitar (self-contained)" };

    juce::TextEditor nameEditor;
    juce::TextButton folderButton { "Folder..." }, exportButton { "EXPORT" };
    juce::String status;
    std::unique_ptr<juce::FileChooser> chooser;

    // Audio on a worker.
    std::unique_ptr<juce::Thread> worker;
    std::atomic<double> progress { -1.0 };
    std::atomic<bool> cancel { false };
    juce::CriticalSection resultLock;
    juce::String workerMessage;
    bool workerDone = false;

    juce::Array<juce::Rectangle<int>> labelAreas;
    juce::StringArray labels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneExportDialog)
};

} // namespace luthier
