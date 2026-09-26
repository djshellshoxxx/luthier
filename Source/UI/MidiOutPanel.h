#pragma once

/*  The MIDI OUT workspace tab (gui-integration.md 4.4, midi-export.md 4, 6-8).

    Three sections:

      EXPORT PROFILE - Luthier or Generic, PPQ, track split, realism text in
          Generic, the SysEx copy in Luthier, identifier stripping, and which
          event classes a Luthier file carries. These are Options -> MIDI's
          defaults (8), so every export path starts from them; a .midprofile
          saves or loads the lot (7).

      EXPORT - the retrospective capture (entire, or the last N seconds) as a
          file (4.1), with the opening-bar preview, or dragged straight out of
          the plugin (4.2; Alt forces Generic).

      LIVE MIDI OUT - the routing panel's MIDI-out switches, the same
          MidiOutConfig: a change on either tab shows on the other (6).
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "../Export/MidiProfiles.h"
#include "../Parameters.h"
#include "../Routing/RoutingMatrix.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** The capture as a performance, and exporting it (shared with the header's
    "Save last MIDI take"). Message thread. */
namespace MidiTakeExport
{
    MidiPerformance capturedPerformance (LuthierAudioProcessor& processor);

    /** Writes the capture (or `lastSeconds` of it, when above zero) with
        `options`. False, with a reason, when there is nothing to write. */
    bool exportCapture (LuthierAudioProcessor& processor, const juce::File& destination,
                        const MidiExportOptions& options, double lastSeconds, juce::String* error);
}

//==============================================================================
class MidiOutPanel : public juce::Component,
                     private juce::Timer
{
public:
    explicit MidiOutPanel (LuthierAudioProcessor& processor);
    ~MidiOutPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    int getPreferredHeight() const;

    /** Re-reads the defaults and the live config into the controls. */
    void refresh();

    //==========================================================================
    // For tests: the controls, by what they do.
    enum class Source { passThrough, rhythm, strings, macroCc, tune, events, workshop,
                        jam,   // FEAT-JAM: jam-mode 9, the band's parts
                        numSources };

    juce::Button& getProfileButton (MidiProfile profile) noexcept;
    juce::Button& getClassToggle (LuthierEventClass eventClass) noexcept;
    juce::Button& getSourceToggle (Source source) noexcept;
    juce::Button& getLiveEnable() noexcept              { return liveEnable->getButton(); }
    juce::Button& getRealismToggle() noexcept           { return realismToggle->getButton(); }
    juce::Button& getSysExToggle() noexcept             { return sysExToggle->getButton(); }
    juce::ComboBox& getPpqBox() noexcept                { return ppqBox; }
    juce::ComboBox& getSplitBox() noexcept              { return splitBox; }
    juce::ComboBox& getRangeBox() noexcept              { return rangeBox; }
    juce::ComboBox& getJamChannelBox (bool bass) noexcept { return bass ? jamBassChannel : jamDrumChannel; }   // FEAT-JAM
    juce::ComboBox& getMacroCcBox (int macro) noexcept  { return macroCc[juce::jlimit (0, ParamIDs::kNumMacros - 1, macro)]; }
    juce::String getPreviewText() const                 { return previewText; }
    juce::String getCaptureText() const                 { return captureText; }

    /** What the Export button does, without the file chooser. */
    bool exportTo (const juce::File& destination, juce::String* error = nullptr);

    MidiExportOptions getOptions() const noexcept { return options; }

private:
    class DragSource;

    void timerCallback() override;

    void writeOptions();
    void writeLiveConfig();
    void updateEnablement();
    void updateCaptureReadout (bool force);
    double rangeSeconds() const;

public:
    /** 4.1's range for `performance`, the MIDI capture's (MODEL-GAPS: marked region, current section). */
    juce::Range<juce::int64> chosenRange (const MidiPerformance& performance) const;
    juce::TextButton& getMarkInButton() noexcept  { return markInButton; }
    juce::TextButton& getMarkOutButton() noexcept { return markOutButton; }

private:

    void saveProfileAs();
    void loadProfile();
    void exportWithChooser();

    LuthierAudioProcessor& processor;
    MidiExportOptions options;
    MidiOutConfig shownLive;
    bool updating = false;

    // --- export profile -----------------------------------------------------------
    std::unique_ptr<LuthierToggle> luthierProfile, genericProfile;
    juce::ComboBox ppqBox, splitBox;
    std::unique_ptr<LuthierToggle> realismToggle, sysExToggle, stripIdsToggle;
    juce::OwnedArray<LuthierToggle> classToggles;
    juce::TextButton saveProfileButton { "SAVE PROFILE..." }, loadProfileButton { "LOAD PROFILE..." };

    // --- export -------------------------------------------------------------------
    juce::ComboBox rangeBox;
    juce::Slider secondsSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton exportButton { "EXPORT MIDI..." };
    juce::TextButton markInButton { "MARK IN" }, markOutButton { "MARK OUT" };   // MODEL-GAPS
    std::unique_ptr<DragSource> dragSource;
    juce::String captureText, previewText;
    int shownEventCount = -1;

    // --- live ---------------------------------------------------------------------
    std::unique_ptr<LuthierToggle> liveEnable;
    juce::OwnedArray<LuthierToggle> sourceToggles;
    juce::ComboBox liveChannel;
    juce::ComboBox jamDrumChannel, jamBassChannel;   // FEAT-JAM: jam-mode 9
    juce::Label jamChannelLabel;                     // FEAT-JAM
    juce::ComboBox macroCc[ParamIDs::kNumMacros];
    juce::Label macroCcLabels[ParamIDs::kNumMacros];

    // Section headers, laid out in resized().
    juce::Rectangle<int> profileHeader, exportHeader, liveHeader, captureBounds, previewBounds;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiOutPanel)
};

} // namespace luthier
