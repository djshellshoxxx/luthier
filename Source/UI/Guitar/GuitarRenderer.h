#pragma once

/*  The guitar illustration (guitar-illustration.md).

    Draws the parts guitar that is playing - a WorkshopGuitar - procedurally,
    from its parts: the body outline from the body style, the neck and
    headstock from the neck part, bridge, tailpiece, pickups, pickguard,
    strings and finish from theirs (ground rules 1 and 3).

    Coordinates are millimetres (section 1): origin at the centre of the
    bridge saddle, X positive toward the headstock, Y positive toward the
    treble side. On screen the headstock is on the left and the bass strings
    at the top, which is how a right-handed player sees the guitar on their
    knee; fitTransform() maps the scene into any rectangle that way.

    The static scene (section 2.1) is built once per guitar and cached by the
    caller under key(); live overlays (2.2) are painted over it each frame and
    never invalidate it.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Model/Workshop/PartLibrary.h"

#include <vector>

namespace luthier
{

//==============================================================================
/** The parts the illustration hit-tests (section 13.1), in z-order bands. */
enum class GuitarRegion
{
    none = 0,
    body, pickguard, soundhole, bridge, tailpiece,
    pickupNeck, pickupMiddle, pickupBridge,
    strings, neck, fretboard, nut, headstock, tuners,
    controls, selector, jack,
    numRegions
};

const char* getGuitarRegionName (GuitarRegion) noexcept;

//==============================================================================
struct GuitarScene
{
    /** One filled / stroked shape of the static scene, in paint order. */
    struct Shape
    {
        juce::Path path;
        juce::FillType fill;                  ///< transparent to skip the fill
        juce::Colour stroke;                  ///< transparent to skip the stroke
        float strokeMm = 0.0f;
        bool strokeIsHairline = false;        ///< stroke at a fixed 1 px, whatever the zoom
    };

    /** A string, drawn per material (section 10) from its anchor to its tuner. */
    struct StringLine
    {
        juce::Point<float> tail, saddle, nut, post;
        juce::Colour colour, winding;
        float widthMm = 0.3f;
        bool wound = false, dashedWinding = false;
        int index = 0;                        ///< 0 = the lowest string
    };

    /** A clickable part (section 13.1): the topmost region containing a point wins. */
    struct Hit
    {
        GuitarRegion region = GuitarRegion::none;
        juce::Path area;
        juce::String description;             ///< section 16's accessible text
    };

    juce::String family = "electric";
    int numStrings = 6;
    double scaleMm = 648.0;

    std::vector<Shape> shapes;                ///< layers 1-13 of section 5
    std::vector<StringLine> strings;          ///< layer 14
    std::vector<Shape> overStrings;           ///< layers 15-25: neck, frets, nut, headstock, tuners, knobs
    std::vector<Hit> hits;                    ///< in z-order, back to front

    /** Fret positions along X (mm from the saddle), fret 0 = the nut. */
    std::vector<float> fretX;

    /** Where each string crosses each fret line, for the live overlays. */
    juce::Point<float> stringAt (int stringIndex, float fret) const noexcept;

    /** The pickups' centres, neck / middle / bridge; empty rectangle if not fitted. */
    std::array<juce::Rectangle<float>, 3> pickupBounds {};

    juce::Rectangle<float> bounds;            ///< everything, mm
    juce::int64 key = 0;                      ///< section 2.1's cache key
};

//==============================================================================
/** What is sounding now (section 2.2): painted over the cached scene. */
struct GuitarOverlay
{
    std::array<float, 12> stringLevel {};     ///< 0..1 per string
    std::array<float, 12> stringFret {};      ///< fretted position, -1 = open / unknown
    float slideFret = -1.0f;                  ///< slide bar position, -1 = off
    int highlightedPickup = -1;               ///< hover (0 neck, 1 middle, 2 bridge)
    GuitarRegion hovered = GuitarRegion::none;
    bool reducedMotion = false;
};

//==============================================================================
class GuitarRenderer
{
public:
    /** Builds the static scene for a guitar. Message thread or worker. */
    static GuitarScene build (const WorkshopGuitar& guitar);

    /** Maps the scene's millimetres into `area`, headstock left, bass up. */
    static juce::AffineTransform fitTransform (const GuitarScene& scene, juce::Rectangle<float> area);

    /** Paints the scene, then the overlay if there is one (section 2.2). */
    static void paint (juce::Graphics& g, const GuitarScene& scene,
                       const juce::AffineTransform& mmToPx, const GuitarOverlay* overlay = nullptr);

    /** The topmost region under a point given in millimetres (section 13.1). */
    static const GuitarScene::Hit* hitTest (const GuitarScene& scene, juce::Point<float> mm);

    /** Renders a guitar to an image, fitted with a margin: thumbnails and tests. */
    static juce::Image render (const WorkshopGuitar& guitar, int width, int height,
                               juce::Colour background = juce::Colours::transparentBlack);

    //==========================================================================
    // Section 10 and 11 tables, public so the tests can hold the renderer to them.
    static juce::Colour stringColour (const juce::String& windingMaterial, bool wound, bool bass);
    static juce::Colour hardwareColour (const juce::String& name);
    static juce::Colour woodColour (const juce::String& wood);
};

} // namespace luthier
