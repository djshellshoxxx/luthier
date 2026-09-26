#pragma once

/*  The JAM workspace tab (jam-mode.md 8.1), between TUNE and LIVE in Advanced
    column 4.

    Header: ARMED, START/STOP and FILL, the status line and the chord line,
    and the 8.4 messages. Then the groups in the sketch's order - STYLE and
    FEEL, FOLLOW and START/STOP side by side (stacked below 720 px), then KIT,
    BASS and MIXER across, then the read-only LANES with the drag-out and
    Export MIDI. Every control is a parameter attachment except the three
    that are not parameters: link rhythm kit (the preset's jam block), the
    metronome preference (UiPreferences) and the drag length.

    A 30 Hz timer drains the band's JamStatus (8.3), marks it stale after
    250 ms, and posts the 12 announcements. */

#include <juce_gui_basics/juce_gui_basics.h>

#include "Widgets.h"
#include "JamWidgets.h"
#include "JamLaneView.h"
#include "JamUiText.h"
#include "AnimationPolicy.h"

namespace luthier
{

class LuthierAudioProcessor;

class JamPanel : public juce::Component,
                 private juce::Timer
{
public:
    explicit JamPanel (LuthierAudioProcessor& processor);
    ~JamPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    int getPreferredHeight() const;
    int getPreferredHeightFor (int width) const;

    /** Re-reads the status and everything shown from it (the timer's work). */
    void refresh();

    //==========================================================================
    // For tests.
    juce::String getStatusText() const        { return statusText; }
    juce::String getChordText() const         { return chordText; }
    juce::StringArray getMessages() const     { return messages; }
    juce::String getLastAnnouncement() const  { return lastAnnouncement; }
    JamLaneView& getLaneView() noexcept       { return lanes; }
    juce::Button& getLinkKitToggle() noexcept { return linkKit; }
    juce::Button& getMetronomeToggle() noexcept { return metronomeQuiet; }
    juce::ComboBox& getDragBarsBox() noexcept { return dragBars; }
    juce::Button& getExportButton() noexcept  { return exportButton; }

    /** Every focusable control in Tab order (12: the sketch's order). */
    std::vector<juce::Component*> getFocusOrder() const;

    /** The status the panel would show, injected (tests force the 8.4 states). */
    void showFacts (const JamUiFacts& facts);

    /** What Export MIDI writes, without the chooser. */
    juce::File exportTo (const juce::File& destination, bool luthierProfile) const;

    /** The drag length's choice in bars (0 = everything). */
    int getDragBars() const;

    /** The UiPreferences key of "Metronome goes quiet while the band plays". */
    static constexpr const char* kMetronomePreference = "jamMetronomeQuiet";

private:
    class DragOut;
    struct Group;

    void timerCallback() override { refresh(); }
    void buildGroups();
    int layoutGroups (int width, bool apply);
    void announce (const JamStatus& status);

    LuthierAudioProcessor& processor;

    // --- header ---------------------------------------------------------------------
    LuthierToggle armed { "ARMED" }, play { "START / STOP" }, fill { "FILL" };
    juce::Label statusLabel, chordLabel, messageLabel;

    // --- STYLE ----------------------------------------------------------------------
    LuthierChoice style { "Style" }, variation { "Variation" };
    juce::TextButton loadStyle { "User style..." };
    juce::ToggleButton linkKit { "Link guitar rhythm kit" };

    // --- FEEL -----------------------------------------------------------------------
    JamDots intensity;
    juce::Label intensityLabel { {}, "Intensity" };
    LuthierChoice fillEvery { "Fills every" };
    LuthierKnob swing { "Swing", LuthierKnob::Size::Small }, humanise { "Human", LuthierKnob::Size::Small };
    LuthierToggle dynamicsFollow { "Dynamics follow" };

    // --- FOLLOW ---------------------------------------------------------------------
    LuthierChoice source { "Source" }, follow { "Follow" };
    LuthierToggle predict { "Predict repeats" };

    // --- START/STOP -----------------------------------------------------------------
    LuthierChoice startMode { "Start" }, countIn { "Count" }, silenceBars { "Bars" };
    LuthierToggle stopOnSilence { "Stop when I stop" }, ending { "Play an ending" };
    juce::ToggleButton metronomeQuiet { "Metronome goes quiet while the band plays" };

    // --- KIT / BASS / MIXER ---------------------------------------------------------
    LuthierChoice kit { "Kit" }, perspective { "Perspective" };
    LuthierToggle kitAuto { "Auto kit" };
    LuthierKnob tuning { "Tuning", LuthierKnob::Size::Small }, damping { "Damping", LuthierKnob::Size::Small },
                room { "Room", LuthierKnob::Size::Small }, width { "Width", LuthierKnob::Size::Small };

    LuthierChoice bassVoice { "Bass" };
    LuthierKnob bassTone { "Tone", LuthierKnob::Size::Small };
    juce::Label bassNote;

    LuthierKnob volume { "Vol", LuthierKnob::Size::Small }, balance { "Bal", LuthierKnob::Size::Small },
                drumsPan { "Drums pan", LuthierKnob::Size::Small }, bassPan { "Bass pan", LuthierKnob::Size::Small };
    LuthierToggle drumsMute { "M drums" }, bassMute { "M bass" };
    LuthierChoice output { "Out" };

    // --- LANES ----------------------------------------------------------------------
    JamLaneView lanes;
    juce::ComboBox dragBars;
    std::unique_ptr<DragOut> dragOut;
    juce::TextButton exportButton { "Export MIDI" };

    std::vector<std::unique_ptr<Group>> groups;
    juce::Rectangle<int> meterBounds;

    // --- live data --------------------------------------------------------------------
    JamUiFacts facts;
    bool factsForced = false;
    uint32_t lastSequence = 0;
    double lastSequenceAt = 0.0;
    juce::String statusText, chordText, lastAnnouncement;
    juce::StringArray messages;
    JamState announcedState = JamState::off;
    int announcedStyle = -1, announcedIntensity = -1;
    juce::String announcedChord;
    double lastChordAnnouncementAt = 0.0;

    std::unique_ptr<juce::FileChooser> chooser;

    // cpu-quality-modes 6: the 30 Hz status drain is a live readout.
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "JamPanel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JamPanel)
};

} // namespace luthier
