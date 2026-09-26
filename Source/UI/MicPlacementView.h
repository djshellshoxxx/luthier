#pragma once

/*  mic-placement.md 6: dragging the microphone across the speaker.

      MicPlacementView  - Advanced, Column 3, the CAB section (6.1): the face
                          of the miked speaker (or, on an acoustic guitar, the
                          body from above) with a handle per mic, the cabinet
                          thumbnail, each mic's distance / angle / rear, the
                          Quick Position and Distance choices, the mini-plot,
                          ToF and level match.
      MicFace           - the face itself; also the expanded editor's front.
      MicHandle         - one mic: drawn as its generic silhouette, dragged,
                          nudged from the keyboard, read by a screen reader as
                          a group of four sliders.
      SpeakerThumb      - the cabinet seen small; a click moves the focused mic.
      MicResponsePlot   - the placement delta, from the same MicPlacementModel
                          the engine runs (ground rule 0.5), so the plot is
                          what you hear.

    Every control writes the same parameters the host automates. Drags and
    Quick picks are one undo entry each (section 8); keyboard nudges group
    within 200 ms.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "../DSP/Amp/MicPlacement.h"
#include "../DSP/Body/AcousticMicModel.h"
#include "../PluginProcessor.h"

namespace luthier
{

//==============================================================================
namespace MicUi
{
    /** The rings a handle snaps to, and their labels (mic-placement.md 2.1). */
    struct Ring { double u; const char* key; const char* label; };
    const std::vector<Ring>& rings();

    /** The acoustic landmarks' labels, by along value. */
    struct Landmark { double along; const char* label; };
    const std::vector<Landmark>& landmarks();

    float get (LuthierAudioProcessor& p, const char* id);
    void set (LuthierAudioProcessor& p, const char* id, float plain);

    bool isAcoustic (LuthierAudioProcessor& p);
    CabinetType cabinetOf (LuthierAudioProcessor& p);
    MicType micTypeOf (LuthierAudioProcessor& p, int mic);
    bool dualMicOn (LuthierAudioProcessor& p);

    MicPlacement placementOf (LuthierAudioProcessor& p, int mic);
    AcousticMicPlacement acousticPlacementOf (LuthierAudioProcessor& p, int mic);

    /** Everything the model needs to draw what this mic sounds like. */
    MicPlacementModel::Input inputFor (LuthierAudioProcessor& p, int mic);

    /** "Cap Edge", "near Cone", "Soundhole"... */
    juce::String nearestLandmark (double u);
    juce::String nearestAcousticLandmark (double along);

    /** The accessible value text of section 8: "Mic 1, Classic Dynamic, near
        Cap Edge, 2.5 centimetres, 0 degrees, speaker 2 of 4". */
    juce::String describe (LuthierAudioProcessor& p, int mic);

    /** The mic's identity colour: accent for 1, secondary for 2. */
    juce::Colour colourFor (int mic);

    /** The empty-state and chip messages of 6.2 / 6.4 for the current rig,
        in priority order; empty when there is nothing to say. */
    juce::StringArray statusMessages (LuthierAudioProcessor& p);

    /** "Mic in its null": a figure-8 near 90 degrees, a cardioid past 150. */
    bool isInNull (LuthierAudioProcessor& p, int mic);

    /** A translated catalog string (accessibility.md 6). */
    juce::String text (const char* key);

    /** Handle drags in flight (the plot waits for the release under reduced motion). */
    int& activeDrags();
}

//==============================================================================
/*  One multi-target edit: one undo entry, change gestures held on every
    parameter it writes, released when it ends (action-and-undo.md 3.5). */
class MicEdit
{
public:
    MicEdit (LuthierAudioProcessor& p, const juce::String& description, juce::StringArray ids);
    ~MicEdit();

    void set (const char* id, float plain);

private:
    LuthierAudioProcessor& processor;
    juce::StringArray ids;
    std::unique_ptr<LuthierAudioProcessor::ScopedUndoAction> undo;
};

class MicFace;

//==============================================================================
class MicHandle : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::Timer
{
public:
    MicHandle (MicFace& owner, int mic);
    ~MicHandle() override;

    int getMic() const noexcept { return mic; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override;
    void resized() override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    /** The four sliders a screen reader reads (section 8): X, Y, Distance,
        Angle - or, on an acoustic guitar, Along, Across, Distance, Angle. */
    juce::Slider& getAccessibleSlider (int index) noexcept { return *proxies[(size_t) juce::jlimit (0, 3, index)]; }

    /** Rebinds the proxies when the guitar family changes. */
    void rebind();

    /** True while a snap ease is animating (never under reduced motion). */
    bool isAnimatingSnap() const noexcept { return easeLeft > 0; }

    static constexpr int kSize = 26;

private:
    void timerCallback() override;
    void nudge (const char* id, float delta, const juce::String& what);
    void endNudgeGroup();

    MicFace& face;
    int mic;

    std::unique_ptr<MicEdit> drag;
    juce::Point<float> dragOffset;
    juce::String dragStartText;

    std::unique_ptr<MicEdit> nudgeGroup;
    juce::String nudgeParam;
    double lastNudgeMs = 0.0;

    int easeLeft = 0;

    class ProxySlider : public juce::Slider
    {
    public:
        ProxySlider() { setInterceptsMouseClicks (false, false); setWantsKeyboardFocus (false); }
        void paint (juce::Graphics&) override {}
    };

    std::array<std::unique_ptr<ProxySlider>, 4> proxies;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> proxyAttachments;
    bool boundAcoustic = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicHandle)
};

//==============================================================================
/*  The miked speaker's face (or the body from above). In `fullCabinet`, the
    expanded editor's front: every speaker in the box, the grille over them. */
class MicFace : public juce::Component,
                private juce::Timer
{
public:
    MicFace (LuthierAudioProcessor& processor, bool fullCabinet);
    ~MicFace() override;

    LuthierAudioProcessor& getProcessor() noexcept { return processor; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    MicHandle& getHandle (int mic) noexcept { return *handles[(size_t) juce::jlimit (0, 1, mic)]; }

    /** Where a mic's handle sits, in this component's pixels. */
    juce::Point<float> handleCentre (int mic) const;

    /** A pixel back to placement: (x, y) in u, or (along, across) acoustic. */
    juce::Point<double> placementAt (juce::Point<float> pixel, int mic) const;

    /** The pixel radius of a ring in u, on the miked speaker. */
    float ringRadiusPx (double u) const;

    /** Snaps a placement to the nearest ring within 6 px (section 6.2). */
    juce::Point<double> snapped (juce::Point<double> xy, bool& didSnap) const;

    int getFocusedMic() const noexcept { return focusedMic; }
    void setFocusedMic (int m);

    void setGrilleVisible (bool v) { grilleVisible = v; repaint(); }
    bool isGrilleVisible() const noexcept { return grilleVisible; }

    /** Refreshes handle positions, visibility and the family's drawing. */
    void refresh();

    std::function<void()> onOpenEditor;
    std::function<void()> onPlacementChanged;

private:
    void timerCallback() override { refresh(); }

    juce::Rectangle<float> speakerRect (int speaker) const;   ///< 1-based, full-cabinet mode
    juce::Point<float> coneCentre (int mic) const;
    float coneRadius (int mic) const;
    juce::Rectangle<float> bodyArea() const;
    juce::Point<float> bodyPoint (double alongMm, double acrossMm) const;

    void drawCone (juce::Graphics&, juce::Point<float> centre, float radiusPx, bool miked) const;
    void drawBody (juce::Graphics&) const;

    LuthierAudioProcessor& processor;
    bool fullCabinet;
    bool grilleVisible = true;
    int focusedMic = 0;
    bool acoustic = false;
    CabinetType cabinet = CabinetType::Cab4x12;
    AcousticLandmarks landmarks;

    std::array<std::unique_ptr<MicHandle>, 2> handles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicFace)
};

//==============================================================================
/** The cabinet seen small: which speaker each mic is on. */
class SpeakerThumb : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    SpeakerThumb (LuthierAudioProcessor& processor, MicFace& face);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    /** The speaker under a point, 1-based, or 0. */
    int speakerAt (juce::Point<float>) const;

    /** Moves the focused mic to a speaker: one toggle-class undo entry. */
    void moveFocusedMicTo (int speaker);

private:
    juce::Rectangle<float> cell (int speaker) const;

    LuthierAudioProcessor& processor;
    MicFace& face;
};

//==============================================================================
/** The placement's response: each mic's delta, and their sum (6.1, 6.2). */
class MicResponsePlot : public juce::Component,
                        private juce::Timer
{
public:
    MicResponsePlot (LuthierAudioProcessor& processor, bool detailed);
    ~MicResponsePlot() override;

    void paint (juce::Graphics&) override;

    static constexpr int kPoints = 256;

    /** The curves as they would be drawn (dB at kPoints log-spaced Hz). */
    struct Curves
    {
        std::array<float, kPoints> mic1 {}, mic2 {}, sum {};
        bool hasMic2 = false, showSum = false;
        double firstNotchHz = 0.0;
    };

    static Curves compute (LuthierAudioProcessor& p, double sampleRate);
    static double frequencyAt (int index) noexcept;

    const Curves& getCurves() const noexcept { return curves; }

    /** The drag-start ghost (6.2): the curves when a drag began. */
    void captureGhost() { ghost = curves; hasGhost = true; }
    void clearGhost() { hasGhost = false; repaint(); }

    /** Recomputes now (tests; the timer does it at 30 Hz while moving). */
    void update();

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    bool detailed;
    Curves curves, ghost;
    bool hasGhost = false;
    juce::uint32 lastSignature = 0;
};

//==============================================================================
class MicPlacementView : public juce::Component,
                         private juce::Timer
{
public:
    explicit MicPlacementView (LuthierAudioProcessor& processor);
    ~MicPlacementView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    /** The height the column should give it: the taller of the two families. */
    int getPreferredHeight() const;

    MicFace& getFace() noexcept { return face; }
    SpeakerThumb& getThumb() noexcept { return thumb; }
    MicResponsePlot& getPlot() noexcept { return plot; }
    juce::Button& getExpandButton() noexcept { return expandButton; }

    /** Puts keyboard focus back on the expand button when the editor closes
        (6.2); records that it was asked, for a window with no desktop peer. */
    void restoreFocusToExpand() { focusRestored = true; expandButton.grabKeyboardFocus(); }
    bool didRestoreFocusToExpand() const noexcept { return focusRestored; }
    LuthierChoice& getQuickPosition (int mic) noexcept { return mic == 0 ? quickPos1 : quickPos2; }
    LuthierChoice& getQuickDistance (int mic) noexcept { return mic == 0 ? quickDist1 : quickDist2; }
    juce::String getStatusText() const { return status; }
    bool isShowingAcoustic() const noexcept { return shownAcoustic; }

    /** The section title: "Cabinet and Mic", or "Microphones" on an acoustic. */
    juce::String getSectionTitle() const;

    /** Opens the expanded editor (6.2); the owner decides where. */
    std::function<void()> onOpenEditor;

    /** The guitar family changed (electric <-> acoustic): the title with it. */
    std::function<void()> onFamilyChanged;

private:
    void timerCallback() override;
    void applyFamily();
    void updateFamilyVisibility();
    void wireQuick (LuthierChoice& box, int mic, bool position);

    LuthierAudioProcessor& processor;

    MicFace face;
    SpeakerThumb thumb { processor, face };
    MicResponsePlot plot { processor, false };
    juce::TextButton expandButton { juce::String (juce::CharPointer_UTF8 ("\xe2\xa4\xa2")) };

    // Electric.
    LuthierKnob dist1 { "Dist 1", LuthierKnob::Size::Small }, angle1 { "Angle 1", LuthierKnob::Size::Small };
    LuthierKnob dist2 { "Dist 2", LuthierKnob::Size::Small }, angle2 { "Angle 2", LuthierKnob::Size::Small };
    LuthierToggle rear1 { "Rear 1" }, rear2 { "Rear 2" };
    LuthierSlider x1 { "Mic 1 X" }, y1 { "Mic 1 Y" }, speaker1 { "Mic 1 Speaker" };
    LuthierSlider x2 { "Mic 2 X" }, y2 { "Mic 2 Y" }, speaker2 { "Mic 2 Speaker" };
    LuthierChoice quickPos1 { "Quick 1" }, quickDist1 { "Distance 1" };
    LuthierChoice quickPos2 { "Quick 2" }, quickDist2 { "Distance 2" };

    // Both families.
    LuthierChoice tofMode { "ToF" };
    LuthierToggle levelMatch { "Level Match" };

    // Acoustic.
    LuthierKnob acMix { "Pickup / Mic", LuthierKnob::Size::Small }, acBlend { "Blend", LuthierKnob::Size::Small };
    LuthierToggle acMic2 { "Mic 2" };
    LuthierKnob acDist1 { "Dist 1", LuthierKnob::Size::Small }, acAngle1 { "Angle 1", LuthierKnob::Size::Small };
    LuthierKnob acDist2 { "Dist 2", LuthierKnob::Size::Small }, acAngle2 { "Angle 2", LuthierKnob::Size::Small };
    LuthierSlider acAlong1 { "Mic 1 Along" }, acAcross1 { "Mic 1 Across" };
    LuthierSlider acAlong2 { "Mic 2 Along" }, acAcross2 { "Mic 2 Across" };

    juce::String status;
    bool shownAcoustic = false;
    bool focusRestored = false;

    struct QuickMapper;
    std::vector<std::pair<juce::AudioProcessorParameter*, std::unique_ptr<QuickMapper>>> quickMappers;

public:
    /** "Driven by automation" (6.2): a write to one of the mic's parameters
        from outside any gesture - a lane, modulation, a legacy mapping -
        within the last 500 ms. */
    bool isDrivenByAutomation (int mic) const noexcept;

private:
    struct AutomationWatch;
    std::unique_ptr<AutomationWatch> automationWatch;

    static constexpr int kHeaderRow = 24;
    static constexpr int kSliderRow = 26;
    static constexpr int kChoiceRow = 36;
    static constexpr int kPlotRow = 36;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicPlacementView)
};

} // namespace luthier
