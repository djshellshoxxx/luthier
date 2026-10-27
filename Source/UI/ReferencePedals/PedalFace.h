// OWNER: WP1 (frozen interface I-6; changes only via errata request to the WP1 owner)
#pragma once

// A pedal's front panel as the board UI sees it (UI_BOARD_STYLES sec 6).
// Procedural vector art only (E18): no bitmaps, no manufacturer logos or photos.
// A face paints its STATIC art (cached by PedalFaceCache) and reports where its
// live controls, jacks, footswitch and LED sit; PedalView lays real components
// on those rectangles. Generic pedals use GenericFace (WP19), branded pedals
// have their own face (WP20, WP21).

#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../ReferencePedals/PedalDesc.h"

namespace luthier::reference
{

// Everything the static art may depend on. Part of the face cache key.
struct FaceState
{
    float norm[kMaxParams] {};       // parameter positions 0..1, descriptor order
    int numParams = 0;
    bool enabled = true;             // footswitch engaged
    bool tailActive = false;         // audio read-back: effect tail still sounding
    bool missing = false;            // unknown model id
    bool highContrast = false;       // Palette::textured == false: flat outlines, no sheen
    float scale = 1.f;               // pixels per design pixel
};

enum class FaceControlKind : uint8_t { Knob, Toggle, Selector, Slider, Button, Treadle };

struct FaceControl
{
    int paramIdx = -1;               // descriptor index
    FaceControlKind kind = FaceControlKind::Knob;
    juce::Rectangle<float> bounds;   // face-local design pixels
    juce::String label;              // physical label text of the pedal
};

struct FaceJack
{
    enum class Kind : uint8_t { In, Out, Send, Return, Aux, Exp, Dc };
    Kind kind = Kind::In;
    juce::Point<float> position;     // cable endpoint, face-local design pixels
};

struct FaceLayout
{
    std::vector<FaceControl> controls;
    std::vector<FaceJack> jacks;
    juce::Rectangle<float> footswitch;
    juce::Rectangle<float> led;
    juce::Colour ledColour = juce::Colour (0xffd8281c);

    void clear() { controls.clear(); jacks.clear(); footswitch = {}; led = {}; }
};

class PedalFace
{
public:
    virtual ~PedalFace() = default;

    // Physical size in millimetres; px = mm * 1.6 at zoom 1 (E17). Default is
    // the compact stompbox (120 x 200 px) for models with no stated size.
    virtual juce::Point<float> designSizeMm() const { return { 75.f, 125.f }; }

    // Static art in design pixels (origin top left, width = designSizeMm().x * 1.6).
    virtual void paintFace (juce::Graphics& g, const FaceState&)
    {
        g.fillAll (juce::Colour (0xff3a3d42));
    }

    // Control, jack, footswitch and LED rectangles in design pixels.
    virtual void layout (FaceLayout& out) { out.clear(); }
};

} // namespace luthier::reference
