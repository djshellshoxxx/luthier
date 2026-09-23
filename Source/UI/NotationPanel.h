#pragma once

/*  The NOTATION workspace tab (gui-integration.md 4.4; notation-export.md 3-6).

      CAPTURE - off / rolling / armed (6.3), how many minutes rolling keeps,
          clear, and what the take holds.

      LIVE TAB - the last 1-8 bars of what was played as ASCII tab (3), with
          symbol density and a scroll speed (slow / medium / fast / freeze),
          and the chord symbols as they changed (4).

      EXPORT - MusicXML, Guitar Pro 8, ASCII tab or MIDI (5): the entire take
          or its last N seconds, an optional quantise (6.5: never on the way
          in), the format's own options, and a preview of the first bar.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "../Capture/PerformanceCapture.h"
#include "../Notation/NotationExport.h"

namespace luthier
{

class LuthierAudioProcessor;

/** Export the capture as notation, without a dialog (the NOTATION tab and the
    header's "Export notation..."). MIDI goes through the MIDI OUT profile. */
namespace NotationTakeExport
{
    bool write (LuthierAudioProcessor& processor, NotationFormat format, const juce::File& destination,
                const CaptureScoreOptions& capture, const NotationExportOptions& options, juce::String* error);

    /** The format a file name asks for, by its extension. */
    NotationFormat formatForFile (const juce::File& file) noexcept;
}

//==============================================================================
class NotationPanel : public juce::Component,
                      private juce::Timer
{
public:
    explicit NotationPanel (LuthierAudioProcessor& processor);
    ~NotationPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    int getPreferredHeight() const;

    /** Re-reads the capture into the view (the timer's work). */
    void refresh();

    //==========================================================================
    // For tests.
    juce::Button& getStateButton (CaptureState state) noexcept;
    juce::ComboBox& getFormatBox() noexcept      { return formatBox; }
    juce::ComboBox& getBarsBox() noexcept        { return barsBox; }
    juce::ComboBox& getSpeedBox() noexcept       { return speedBox; }
    juce::ComboBox& getQuantiseBox() noexcept    { return quantiseBox; }
    juce::String getLiveTabText() const          { return tabView.getText(); }
    juce::String getPreviewText() const          { return previewView.getText(); }
    juce::String getChordHistoryText() const     { return chordHistory; }
    juce::String getStatusText() const           { return statusText; }

    bool exportTo (const juce::File& destination, juce::String* error = nullptr);

private:
    void timerCallback() override;
    void setCaptureState (CaptureState state);
    void updatePreview();
    void exportWithChooser();

    NotationFormat currentFormat() const noexcept;
    CaptureScoreOptions currentCaptureOptions() const;
    NotationExportOptions currentOptions() const;

    LuthierAudioProcessor& processor;

    // --- capture --------------------------------------------------------------------
    std::unique_ptr<LuthierToggle> offButton, rollingButton, armedButton;
    juce::Slider rollingMinutes { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton clearButton { "CLEAR TAKE" };
    juce::String statusText;

    // --- live tab -------------------------------------------------------------------
    std::unique_ptr<LuthierToggle> showTab;
    juce::ComboBox barsBox, densityBox, speedBox;
    juce::TextEditor tabView;
    juce::String chordHistory;
    int tabTicks = 0;

    // --- export ---------------------------------------------------------------------
    juce::ComboBox formatBox, rangeBox, quantiseBox;
    juce::Slider lastSeconds { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider lineWidth { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    std::unique_ptr<LuthierToggle> chordDiagrams;
    juce::TextEditor previewView;
    juce::TextButton exportButton { "EXPORT NOTATION..." };

    juce::Rectangle<int> captureHeader, tabHeader, exportHeader, statusBounds, chordBounds;
    std::unique_ptr<juce::FileChooser> chooser;
    size_t shownNotes = (size_t) -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NotationPanel)
};

} // namespace luthier
