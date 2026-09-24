#pragma once

/*  Materials and small hardware for the amp and pedal faces
    (proposals/visual-polish.md 2 and 3).

    Every colour comes from the palette in force: the materials are mixes of
    its roles (brass from the plate, tweed from brass and ivory, oxblood from
    the clip red, and so on), so the Light and colour-blind palettes move them
    together and High contrast (Palette::textured == false) flattens them to
    panel, edge and text colours with no grain, sheen or gradients. Black and
    white appear only as shading over those colours.

    The textures are drawn from fixed seeds, so a face renders identically
    every time (visual-polish.md 7) and callers can cache it.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Accessibility/Accessibility.h"

namespace luthier::faces
{

//==============================================================================
struct Materials
{
    bool textured = true;

    // Cabinet coverings
    juce::Colour tolexBlack, tweed, fawn, rust, slate, walnut, oxbloodTolex;

    // Metals and plates
    juce::Colour chrome, nickel, brass, copper, aluminium, darkMetal, ivoryPanel, blackPanel;

    // Grille cloths
    juce::Colour grilleSilver, grilleOxblood, grilleBrown, grilleBlack, grilleGold;

    // Trim, lights and print
    juce::Colour piping, jewelRed, ledGreen, ledAmber, ledCool, tubeGlow;
    juce::Colour inkLight, inkDark;

    /** The materials for the palette in force. */
    static Materials current();
};

//==============================================================================
/** The label colour (light or dark ink) that reads best on a surface; the
    palette's text colours when they reach 4.5:1, else whichever is higher. */
juce::Colour inkFor (juce::Colour surface);

//==============================================================================
// Surfaces. Each fills `area` (clipped to `shape` when one is given).

void fillTolex (juce::Graphics&, const juce::Path& shape, juce::Colour base, juce::uint32 seed);
void fillTweed (juce::Graphics&, const juce::Path& shape, juce::Colour base);
void fillHammertone (juce::Graphics&, const juce::Path& shape, juce::Colour base, juce::uint32 seed);
void fillGloss (juce::Graphics&, const juce::Path& shape, juce::Colour base);
void fillBrushedMetal (juce::Graphics&, const juce::Path& shape, juce::Colour base);
void fillDiamondPlate (juce::Graphics&, const juce::Path& shape, juce::Colour base);
void fillWood (juce::Graphics&, const juce::Path& shape, juce::Colour base, juce::uint32 seed);

enum class Grille { silverSparkle, oxbloodStripe, diamond, basket, blackMesh };
void fillGrille (juce::Graphics&, const juce::Path& shape, Grille, juce::Colour base, juce::uint32 seed);

//==============================================================================
// Hardware.

/** A faceted pilot jewel in a chrome bezel; lit or dark. */
void drawJewel (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour, bool lit);

/** A small round LED with its bezel. */
void drawLed (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour, bool lit);

/** A slotted screw head. */
void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius);

/** A 1/4" jack: chrome nut and dark socket. */
void drawJack (juce::Graphics&, juce::Point<float> centre, float radius);

/** A metal cabinet corner protector at one corner of `cabinet` (0 top-left, clockwise). */
void drawCornerCap (juce::Graphics&, juce::Rectangle<float> cabinet, int corner, float size);

/** A rocker switch, lit when on. */
void drawRocker (juce::Graphics&, juce::Rectangle<float> area, bool on, juce::Colour lamp);

/** A 3PDT stomp switch seen from above: hex nut and domed button. */
void drawFootswitch (juce::Graphics&, juce::Rectangle<float> area, bool down);

/** Valves glowing behind a vent: `drive` 0-1 sets the glow, stale greys them. */
void drawTubeVent (juce::Graphics&, juce::Rectangle<float> area, int numTubes, float drive, bool standby, bool stale);

//==============================================================================
/** Text in the display face (true) or the body face, fitted to `area`. */
void drawPrint (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area, juce::Colour colour,
                float height, bool display, juce::Justification = juce::Justification::centred, int maxLines = 1);

//==============================================================================
/*  Labels that never collide (visual-polish.md 2). A label is printed whole on
    one line at a height that fits its rectangle: the full name when it fits at
    a readable size, the short one when only that does, and nothing when
    neither does - never truncated with an ellipsis, never squeezed into its
    neighbour. */

/** The smallest height a face prints a label at; below it the label hides. */
inline constexpr float minPrintHeight = 6.5f;

struct FittedPrint
{
    juce::String text;
    float height = 0.0f;    ///< font height; 0 when hidden

    bool isVisible() const noexcept { return text.isNotEmpty() && height > 0.0f; }
};

/** The width `text` takes in the display face (true) or the body face at `height`. */
float printWidth (const juce::String& text, float height, bool display);

/** The tallest height, up to `maxHeight` and the area's own height, at which
    `text` fits `area` on one line; 0 when that is below minPrintHeight. */
float fitPrintHeight (const juce::String& text, juce::Rectangle<float> area, float maxHeight, bool display);

/** One label: the full text, else the short form, else hidden. */
FittedPrint fitPrint (const juce::String& text, const juce::String& shortForm, juce::Rectangle<float> area,
                      float maxHeight, bool display);

/** Draws a fitted label in `area`. Nothing when it is hidden. */
void drawFittedPrint (juce::Graphics&, const FittedPrint&, juce::Rectangle<float> area, juce::Colour colour,
                      bool display, juce::Justification = juce::Justification::centred);

//==============================================================================
/** A digest of the palette in force (its roles and whether it is textured), so
    a cached face can tell when it has to be drawn again (visual-polish.md 0.3). */
juce::uint64 paletteDigest();

/** Deterministic noise in 0-1 for seeded textures. */
float hashNoise (juce::uint32 seed, int i) noexcept;

} // namespace luthier::faces
