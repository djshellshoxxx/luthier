#pragma once

/*  Advanced mode (build spec, "ADVANCED MODE").

    A compressed guitar-and-fretboard strip across the top, then four scrollable
    columns:

      1  String setup      - one row per string: tuning, material, gauge, age,
                             computed tension, mute, select
      2  Selected string   - every physical property of that one string
      3  Body, pickups and the playing hand
      4  Amp, cabinet, room, both effect chains and humanisation

    Every control here is the same widget as in Easy mode, so right-click, MIDI
    Learn, lock and randomise work identically throughout.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "RoutingPanel.h"
#include "ModMatrixPanel.h"
#include "RhythmPanel.h"
#include "ToneMatchPanel.h"
#include "CharacterPanel.h"
#include "FretboardComponent.h"
#include "GuitarBodyComponent.h"
#include "PedalRack.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** One row of the string list. */
class StringRow : public juce::Component,
                  public juce::SettableTooltipClient,
                  private juce::Timer
{
public:
    StringRow (LuthierAudioProcessor& processor, int stringIndex);
    ~StringRow() override;

    void setSelected (bool selected);
    bool isSelected() const noexcept { return selected; }

    std::function<void (int)> onSelected;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int preferredHeight = 34;

private:
    void timerCallback() override;

    LuthierAudioProcessor& processor;
    int stringIndex;
    bool selected = false;

    juce::String noteText, tensionText;
    double tensionNewtons = 0.0;
    bool tensionPlayable = true;
    bool muted = false;

    juce::Rectangle<int> muteBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StringRow)
};

//==============================================================================
class AdvancedPanel : public juce::Component
{
public:
    explicit AdvancedPanel (LuthierAudioProcessor& processor);
    ~AdvancedPanel() override;

    void setSelectedString (int index);
    int getSelectedString() const noexcept { return selectedString; }

    FretboardComponent& getFretboard() noexcept { return fretboard; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Column : public juce::Component
    {
    public:
        explicit Column (const juce::String& title);

        void addSection (const juce::String& heading);
        void addControl (juce::Component* component, int height);
        void addGap (int height);

        /** Lays the accumulated content out in a single column and returns the
            total height, for the viewport. */
        int layout (int width);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        struct Item
        {
            juce::Component* component = nullptr;
            juce::String heading;
            int height = 0;
            bool isGap = false;
        };

        juce::String title;
        juce::Array<Item> items;
        int contentHeight = 0;
    };

    void buildStringColumn();
    void buildDetailColumn();
    void buildBodyColumn();
    void buildRigColumn();

    LuthierAudioProcessor& processor;

    GuitarBodyComponent guitarBody;
    FretboardComponent fretboard;

    juce::Viewport viewports[4];
    std::unique_ptr<Column> columns[4];

    int selectedString = 0;

    // --- column 1 -----------------------------------------------------------------
    juce::OwnedArray<StringRow> stringRows;
    std::unique_ptr<LuthierChoice> stringMaterial, stringGauge, stringAge;
    std::unique_ptr<LuthierKnob> realismDetune, intonation, sustain;
    std::unique_ptr<LuthierToggle> driftToggle;

    // --- column 2 -----------------------------------------------------------------
    std::unique_ptr<LuthierChoice> temperament;
    std::unique_ptr<LuthierKnob> concertA, couplingAmount, fretAction, fretBuzz;
    std::unique_ptr<LuthierToggle> fretlessToggle, slideGuitarToggle, freezeToggle;
    std::unique_ptr<juce::Label> stringInfoLabel;

    // --- column 3 -----------------------------------------------------------------
    std::unique_ptr<LuthierChoice> bodyMode, bracing, topWood, backWood;
    std::unique_ptr<LuthierKnob> bodyAmount, bodyWidth, bodyDepth, topThickness,
                                 soundhole, bodyAge, airGain;

    std::unique_ptr<LuthierChoice> pickupSelector;
    std::unique_ptr<LuthierChoice> pickupType[3];
    std::unique_ptr<LuthierChoice> pickupMagnet[3];
    std::unique_ptr<LuthierKnob> pickupPosition[3], pickupHeight[3], pickupVolume[3];
    std::unique_ptr<LuthierToggle> coilTap;
    std::unique_ptr<LuthierKnob> piezoMicBlend, guitarTone, guitarVolume;

    std::unique_ptr<LuthierToggle> useFingers;
    std::unique_ptr<LuthierChoice> pickMaterial;
    std::unique_ptr<LuthierKnob> pickThickness, pickAngle, pluckPosition, nailVsFlesh;
    std::unique_ptr<LuthierKnob> slideNoise, fretNoise, releaseNoise, bodyKnock, pickNoise, ampBuzz;

    // --- column 4 -----------------------------------------------------------------
    std::unique_ptr<LuthierChoice> ampModel;
    std::unique_ptr<LuthierKnob> ampGain, ampBass, ampMid, ampTreble, ampPresence, ampMaster;
    std::unique_ptr<LuthierToggle> ampBright, ampMidBoost, ampStandby;

    std::unique_ptr<LuthierToggle> cabOn, dualMic;
    std::unique_ptr<LuthierChoice> cabType, cabSpeaker, micType, micPosition, micDistance,
                                   micType2, micPosition2, micDistance2;
    std::unique_ptr<LuthierKnob> speakerAge, micBlend, micWidth, micPhase;

    std::unique_ptr<LuthierToggle> roomOn;
    std::unique_ptr<LuthierChoice> roomSize, roomMaterial;
    std::unique_ptr<LuthierKnob> roomBlend, roomDecay, roomWidth;

    std::unique_ptr<LuthierToggle> cableOn;
    std::unique_ptr<LuthierKnob> cableLength;

    std::unique_ptr<RoutingPanel> routingPanel;
    std::unique_ptr<ModMatrixPanel> modMatrixPanel;
    std::unique_ptr<RhythmPanel> rhythmPanel;
    std::unique_ptr<ToneMatchPanel> toneMatchPanel;
    std::unique_ptr<CharacterPanel> characterPanel;

    std::unique_ptr<LuthierChoice> bridgeType;
    std::unique_ptr<LuthierKnob> whammyPos, whammyDown, whammyUp, whammySprings, transposeLock;

    std::unique_ptr<PedalRack> preRack, postRack;

    std::unique_ptr<LuthierKnob> humTiming, humVelocity, humDetune, humAttack, humNoise, humStrum;
    std::unique_ptr<LuthierKnob> vibratoRate, vibratoDepth, strumSpeed, bendRange, legatoWindow;
    std::unique_ptr<LuthierChoice> vibratoShape, strumDirection;
    std::unique_ptr<LuthierToggle> feedbackOn, doublerOn, mpeToggle;
    std::unique_ptr<LuthierKnob> feedbackThreshold, feedbackSpeed, doublerAmount;

    std::unique_ptr<LuthierKnob> masterGain;
    std::unique_ptr<LuthierToggle> limiterOn;
    std::unique_ptr<LuthierChoice> oversampling;

    juce::OwnedArray<juce::Component> ownedControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdvancedPanel)
};

} // namespace luthier
