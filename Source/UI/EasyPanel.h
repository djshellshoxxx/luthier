#pragma once

/*  Easy mode (build spec, "EASY MODE").

    Three horizontal bands:
      1 (~50%) the guitar illustration and the interactive fretboard
      2 (~30%) six macro knobs, each with a dice and a lock
      3 (~20%) style preset, playing mode, MIDI indicator, AUDITION, export
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "FretboardComponent.h"
#include "GuitarBodyComponent.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class EasyPanel : public juce::Component,
                  private juce::Timer
{
public:
    explicit EasyPanel (LuthierAudioProcessor& processor);
    ~EasyPanel() override;

    std::function<void()> onOpenExport;

    FretboardComponent& getFretboard() noexcept { return fretboard; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshStyleList();
    void applyStylePreset (int presetIndex);

    LuthierAudioProcessor& processor;

    GuitarBodyComponent guitarBody;
    FretboardComponent fretboard;

    LuthierKnob attackKnob   { "Attack",   LuthierKnob::Size::Macro };
    LuthierKnob bodyKnob     { "Body",     LuthierKnob::Size::Macro };
    LuthierKnob driveKnob    { "Drive",    LuthierKnob::Size::Macro };
    LuthierKnob toneKnob     { "Tone",     LuthierKnob::Size::Macro };
    LuthierKnob spaceKnob    { "Space",    LuthierKnob::Size::Macro };
    LuthierKnob humanizeKnob { "Humanize", LuthierKnob::Size::Macro };

    juce::ComboBox styleBox;
    juce::Label styleLabel { {}, "Style" };

    LuthierChoice playingModeSelector { "Mode" };

    juce::TextButton auditionButton { "Audition" };
    juce::ComboBox auditionPhraseBox;
    juce::TextButton exportButton { "Export" };
    juce::TextButton randomiseButton { "Randomise" };
    juce::TextButton resetButton { "Reset" };

    LevelMeter meter;
    DataStreamDisplay dataStream;

    juce::Label chordLabel;

    juce::Array<int> stylePresetIndices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EasyPanel)
};

} // namespace luthier
