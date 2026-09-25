#pragma once

/*  The TUNE tab's BASS and LAYERS rows (tune-builder.md 6 and 7).
    TUNE-HELP-ONBOARDING workstream.

      BASS    Off / Root / Root-Fifth / Walking / Genre / Manual. Manual is the
              piano roll's Bass target; editing a generated line there makes it
              manual.
      LAYERS  Pad, Arpeggio, Countermelody, Percussion: "Every layer has its own
              on / off, volume, and pan." The arpeggio picks a fingerpick
              pattern; the countermelody is generated on first switch-on (and
              again on Regenerate) and edited in the roll's Counter target.

    Every control is a `tune-section-edit` on the selected section; volume and
    pan drags group within 200 ms per layer.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Widgets.h"
#include "../Tune/TuneSession.h"

namespace luthier
{

class TuneLayersStrip : public juce::Component
{
public:
    TuneLayersStrip (TuneSession& session, juce::StringArray fingerpickPatterns);

    int getPreferredHeight() const;

    /** Re-reads the selected section. */
    void refresh();

    //==========================================================================
    // What the controls do, callable from tests.
    bool setBassMode (BassMode mode);
    bool setLayerEnabled (LayerType type, bool enabled);
    bool setLayerVolume (LayerType type, double volume);
    bool setLayerPan (LayerType type, double pan);
    bool setArpeggioPattern (const juce::String& pattern);
    bool regenerateCountermelody();

    juce::ComboBox& getBassBox() noexcept                     { return bassBox; }
    juce::TextButton& getLayerButton (LayerType t) noexcept   { return rows[(size_t) t].toggle.getButton(); }
    juce::Slider& getVolumeSlider (LayerType t) noexcept      { return rows[(size_t) t].volume; }
    juce::Slider& getPanSlider (LayerType t) noexcept         { return rows[(size_t) t].pan; }
    juce::ComboBox& getArpeggioBox() noexcept                 { return arpBox; }
    juce::TextButton& getRegenerateButton() noexcept          { return regenerateButton; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    bool changeLayer (LayerType type, const juce::String& description,
                      const std::function<void (TuneLayer&)>& edit, int group = -1);

    TuneSession& session;
    juce::StringArray patterns;
    bool updating = false;

    juce::ComboBox bassBox;

    struct Row
    {
        explicit Row (const juce::String& name) : toggle (name) {}
        LuthierToggle toggle;
        juce::Slider volume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
        juce::Slider pan { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    };

    std::array<Row, 4> rows { Row ("PAD"), Row ("ARPEGGIO"), Row ("COUNTER"), Row ("PERC") };
    juce::ComboBox arpBox;
    juce::TextButton regenerateButton { "REGEN" };
    juce::Rectangle<int> bassLabel;
    std::array<juce::Rectangle<int>, 4> volumeLabels, panLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuneLayersStrip)
};

} // namespace luthier
