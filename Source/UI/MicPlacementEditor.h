#pragma once

/*  mic-placement.md 6.2 and 6.3.

      MicPlacementEditor - the expanded editor. In Advanced it takes over
                           Columns 3 and 4 the way the Workshop does, the tab
                           strip staying visible; in Easy it is an overlay.
      MicSideView        - the mic seen from the side: drag it for distance,
                           drag its tail for angle, over a log ruler.
      MicPad             - Easy's "bright <-> warm" pad in the Cabinet card.
*/

#include "MicPlacementView.h"
#include "AnimationPolicy.h"   // cpu-quality-modes 6 (INTEGRATE-2)
#include "Overlays.h"

namespace luthier
{

//==============================================================================
class MicSideView : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    MicSideView (LuthierAudioProcessor& processor, MicFace& face);
    ~MicSideView() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** The ruler: centimetres to x and back, log-spaced (0 at the grille). */
    float xForCm (double cm) const;
    double cmForX (float x) const;

private:
    void timerCallback() override { repaint(); }
    const char* distId() const;
    const char* angleId() const;
    juce::Point<float> micPoint() const;

    LuthierAudioProcessor& processor;
    MicFace& face;
    std::unique_ptr<MicEdit> drag;
    bool draggingTail = false;

    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "MicSideView" };   // cpu-quality-modes 6
};

//==============================================================================
class MicPlacementEditor : public juce::Component,
                           private juce::Timer
{
public:
    explicit MicPlacementEditor (LuthierAudioProcessor& processor);
    ~MicPlacementEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    /** Escape, x, or the owner's expand button. */
    std::function<void()> onClose;

    MicFace& getFront() noexcept { return front; }
    MicSideView& getSideView() noexcept { return side; }
    MicResponsePlot& getPlot() noexcept { return plot; }
    juce::Button& getResetButton() noexcept { return resetButton; }
    juce::Button& getGrilleButton() noexcept { return grilleButton; }

    /** Returns the focused mic (or, with `both`, both) to section 7's defaults:
        one multi-target undo entry. */
    void resetMics (bool both);

    juce::String getTitleText() const;

private:
    void timerCallback() override;
    void applyFamily();

    LuthierAudioProcessor& processor;

    MicFace front;
    MicSideView side { processor, front };
    MicResponsePlot plot { processor, true };

    juce::TextButton grilleButton { "Grille" }, resetButton { "Reset" }, closeButton { "x" };

    struct Card
    {
        std::unique_ptr<LuthierChoice> micType;
        std::unique_ptr<LuthierToggle> rear;
        std::unique_ptr<LuthierSlider> speaker, x, y, dist, angle;
        std::unique_ptr<LuthierSlider> along, across, acDist, acAngle;
    };

    std::array<Card, 2> cards;
    bool shownAcoustic = false;

    static constexpr int kHeader = 32;
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "MicPlacementEditor" };   // cpu-quality-modes 6

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicPlacementEditor)
};

//==============================================================================
/** Easy mode's editor: the same editor as an overlay (6.3). */
class MicPlacementOverlay : public OverlayPanel
{
public:
    explicit MicPlacementOverlay (LuthierAudioProcessor& processor)
        : OverlayPanel ("Mic Placement"), editor (processor)
    {
        addAndMakeVisible (editor);
    }

    juce::Point<int> getPreferredSize() const override { return { 1100, 680 }; }
    MicPlacementEditor& getEditor() noexcept { return editor; }

protected:
    void layoutContent (juce::Rectangle<int> content) override { editor.setBounds (content); }

private:
    MicPlacementEditor editor;
};

//==============================================================================
/*  Easy's pad (6.3): across is bright to warm (u 0 to 0.9 keeping the
    handle's direction; along 4 to 1 on an acoustic), down is close to far
    (1 to 100 cm, log). It moves mic 1; mic 2, when on, is a hollow ghost.
    One focusable control, arrows for both axes. */
class MicPad : public juce::Component,
               public juce::SettableTooltipClient,
               private juce::Timer
{
public:
    explicit MicPad (LuthierAudioProcessor& processor);
    ~MicPad() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    /** Pad position (0-1 each) of a mic, and back. */
    juce::Point<float> padPositionOf (int mic) const;
    void setFromPad (juce::Point<float> normalised);

    /** True when the mic 2 ghost is drawn. */
    bool isShowingGhost() const;

    std::function<void()> onOpenEditor;

    static constexpr int kWidth = 120, kHeight = 72;

private:
    void timerCallback() override { repaint(); }

    LuthierAudioProcessor& processor;
    std::unique_ptr<MicEdit> drag;
    AnimationPolicy::Registration motion { *this, AnimationPolicy::LiveReadout, "MicPad" };   // cpu-quality-modes 6

};

} // namespace luthier
