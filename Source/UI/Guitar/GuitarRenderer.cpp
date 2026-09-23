#include "GuitarRenderer.h"
#include "BodyOutlines.h"
#include "HeadstockOutlines.h"

#include <cmath>

namespace luthier
{

using Pointf = juce::Point<float>;

//==============================================================================
const char* getGuitarRegionName (GuitarRegion r) noexcept
{
    switch (r)
    {
        case GuitarRegion::body:          return "Body";
        case GuitarRegion::pickguard:     return "Pickguard";
        case GuitarRegion::soundhole:     return "Soundhole";
        case GuitarRegion::bridge:        return "Bridge";
        case GuitarRegion::tailpiece:     return "Tailpiece";
        case GuitarRegion::pickupNeck:    return "Neck pickup";
        case GuitarRegion::pickupMiddle:  return "Middle pickup";
        case GuitarRegion::pickupBridge:  return "Bridge pickup";
        case GuitarRegion::strings:       return "Strings";
        case GuitarRegion::neck:          return "Neck";
        case GuitarRegion::fretboard:     return "Fretboard";
        case GuitarRegion::nut:           return "Nut";
        case GuitarRegion::headstock:     return "Headstock";
        case GuitarRegion::tuners:        return "Tuners";
        case GuitarRegion::controls:      return "Controls";
        case GuitarRegion::selector:      return "Pickup selector";
        case GuitarRegion::jack:          return "Output jack";
        case GuitarRegion::none:
        case GuitarRegion::numRegions:    break;
    }

    return "";
}

//==============================================================================
// Section 10, 11.8 and the wood colours the finishes sit on.

juce::Colour GuitarRenderer::stringColour (const juce::String& material, bool wound, bool bass, bool flatwound)
{
    const auto m = material.toLowerCase();

    if (m.contains ("tape"))                         return juce::Colour (0xff252525);
    if (m == "nylon")                                return juce::Colour (0xfff2e9d8);   // core; the winding is gold
    if (! wound)                                     return juce::Colour (0xffc4c7cc);   // plain steel
    if (flatwound)                                   return juce::Colour (0xffb0b4ba);
    if (m.contains ("phosphor"))                     return juce::Colour (0xffb5824a);
    if (m.contains ("bronze"))                       return juce::Colour (0xffc9a75a);
    if (m.contains ("silk"))                         return juce::Colour (0xffb8b4a8);
    if (m.contains ("stainless"))                    return juce::Colour (0xffd0d4d9);
    if (m == "nickel" || m == "pure_nickel")         return juce::Colour (0xffa9aaaf);
    if (bass)                                        return juce::Colour (0xffa9aaaf);   // bass nickel roundwound
    return juce::Colour (0xffb0b4ba);                                                    // nickel-plated steel
}

juce::Colour GuitarRenderer::hardwareColour (const juce::String& name)
{
    const auto n = name.toLowerCase().replaceCharacter (' ', '_');

    if (n == "chrome")       return juce::Colour (0xffd5d9dd);
    if (n == "gold")         return juce::Colour (0xffc9a24b);
    if (n == "black")        return juce::Colour (0xff2a2d31);
    if (n == "aged_nickel")  return juce::Colour (0xffa5aab0);
    if (n == "aged_gold")    return juce::Colour (0xffb08838);
    return juce::Colour (0xffb9bec4);                                                    // nickel, the default
}

juce::Colour GuitarRenderer::woodColour (const juce::String& wood)
{
    const auto w = wood.toLowerCase();

    if (w.startsWith ("alder"))      return juce::Colour (0xffc89b66);
    if (w.startsWith ("ash"))        return juce::Colour (0xffd9bd8c);
    if (w.startsWith ("basswood"))   return juce::Colour (0xffe2cfa6);
    if (w.startsWith ("cypress"))    return juce::Colour (0xffe6ce9a);
    if (w.startsWith ("mahogany"))   return juce::Colour (0xff7a3f22);
    if (w.startsWith ("sapele"))     return juce::Colour (0xff7d4a2e);
    if (w.startsWith ("maple"))      return juce::Colour (0xffe8c990);
    if (w.startsWith ("rosewood"))   return juce::Colour (0xff4a2616);
    if (w.startsWith ("walnut"))     return juce::Colour (0xff5e3b24);
    if (w.startsWith ("spruce") || w.startsWith ("sitka"))  return juce::Colour (0xffe9cf98);
    if (w.startsWith ("cedar"))      return juce::Colour (0xffc98f5a);
    if (w.startsWith ("ebony"))      return juce::Colour (0xff1a1512);
    if (w.startsWith ("pau"))        return juce::Colour (0xff6b4028);
    if (w.startsWith ("korina"))     return juce::Colour (0xffd8b77a);
    if (w.startsWith ("koa"))        return juce::Colour (0xffa0673a);
    if (w.startsWith ("poplar"))     return juce::Colour (0xffc9bb8e);
    if (w.startsWith ("steel"))      return juce::Colour (0xffb9bec4);
    if (w.startsWith ("brass"))      return juce::Colour (0xffc9a24b);
    return juce::Colour (0xffb0804f);
}

//==============================================================================
namespace
{
    constexpr float kInch = 25.4f;

    juce::Colour parseHex (const juce::String& text, juce::Colour fallback)
    {
        auto t = text.trim();

        if (t.startsWithChar ('#'))
            t = t.substring (1);

        if (t.length() != 6 || ! t.containsOnly ("0123456789abcdefABCDEF"))
            return fallback;

        return juce::Colour ((juce::uint32) (0xff000000u | (juce::uint32) t.getHexValue32()));
    }

    juce::FillType solid (juce::Colour c) { return juce::FillType (c); }

    /** No fill. A default-constructed FillType is solid black, not empty. */
    juce::FillType none() { return juce::FillType (juce::Colours::transparentBlack); }

    /** A metal part lit from the top left: bright edge, body colour, dark band, return. */
    juce::FillType metalFill (juce::Colour base, juce::Rectangle<float> area, bool materials)
    {
        if (! materials)
            return solid (base);

        // Screen top-left is scene +X, -Y, so the light falls from (right, top) of the area in mm.
        juce::ColourGradient grad (base.brighter (0.55f), area.getRight(), area.getY(),
                                   base.darker (0.35f), area.getX(), area.getBottom(), false);
        grad.addColour (0.35, base.brighter (0.1f));
        grad.addColour (0.62, base.darker (0.45f));
        grad.addColour (0.8, base.brighter (0.15f));
        return juce::FillType (grad);
    }

    /** Centripetal Catmull-Rom through points, with optional sharp corners (BodyOutlines.h). */
    juce::Path smoothPath (const std::vector<Pointf>& p, const std::vector<bool>& corner, bool closed, bool angular)
    {
        juce::Path path;
        const int n = (int) p.size();

        if (n < 2)
            return path;

        path.startNewSubPath (p[0]);

        if (angular || n < 3)
        {
            for (int i = 1; i < n; ++i)
                path.lineTo (p[(size_t) i]);

            if (closed)
                path.closeSubPath();

            return path;
        }

        auto at = [&] (int i) -> Pointf
        {
            if (closed)
                return p[(size_t) ((i % n + n) % n)];

            if (i < 0)      return p[0] * 2.0f - p[1];
            if (i >= n)     return p[(size_t) n - 1] * 2.0f - p[(size_t) n - 2];
            return p[(size_t) i];
        };

        auto isCorner = [&] (int i)
        {
            const int k = closed ? ((i % n + n) % n) : juce::jlimit (0, n - 1, i);
            return k < (int) corner.size() && corner[(size_t) k];
        };

        const int segments = closed ? n : n - 1;

        for (int i = 0; i < segments; ++i)
        {
            const auto p1 = at (i), p2 = at (i + 1);
            const auto p0 = isCorner (i) ? p1 : at (i - 1);
            const auto p3 = isCorner (i + 1) ? p2 : at (i + 2);

            float c1x, c1y, c2x, c2y;
            outlines::catmullRomToBezier (p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, c1x, c1y, c2x, c2y);
            path.cubicTo (c1x, c1y, c2x, c2y, p2.x, p2.y);
        }

        if (closed)
            path.closeSubPath();

        return path;
    }

    juce::Path circle (Pointf c, float r)
    {
        juce::Path p;
        p.addEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        return p;
    }

    juce::Path rect (float x1, float y1, float x2, float y2, float corner = 0.0f)
    {
        juce::Path p;
        const auto r = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (x1, x2), juce::jmin (y1, y2),
                                                                   juce::jmax (x1, x2), juce::jmax (y1, y2));
        if (corner > 0.0f)
            p.addRoundedRectangle (r, corner);
        else
            p.addRectangle (r);
        return p;
    }

    juce::Path polygon (std::initializer_list<Pointf> pts)
    {
        juce::Path p;
        bool first = true;

        for (auto q : pts)
        {
            if (first) p.startNewSubPath (q); else p.lineTo (q);
            first = false;
        }

        p.closeSubPath();
        return p;
    }

    juce::Path strokeOf (const juce::Path& source, float width)
    {
        juce::Path out;
        juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (out, source);
        return out;
    }

    bool containsAny (const juce::String& text, std::initializer_list<const char*> words)
    {
        for (auto* w : words)
            if (text.containsIgnoreCase (w))
                return true;
        return false;
    }
} // namespace

//==============================================================================
juce::Point<float> GuitarScene::stringAt (int stringIndex, float fret) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, (int) saddlePoints.size()))
        return {};

    const auto s = saddlePoints[(size_t) stringIndex];
    const auto n = nutPoints[(size_t) stringIndex];

    // Distance from the nut is scale * (1 - 2^(-fret/12)).
    const float fromNut = 1.0f - std::pow (2.0f, -juce::jmax (0.0f, fret) / 12.0f);
    return n + (s - n) * fromNut;
}

juce::Point<float> GuitarScene::noteAt (int stringIndex, int fret) const noexcept
{
    if (fret <= 0)
        return stringAt (stringIndex, 0.0f) + (stringAt (stringIndex, 0.0f) - stringAt (stringIndex, 1.0f)) * 0.0f;

    return (stringAt (stringIndex, (float) fret - 1.0f) + stringAt (stringIndex, (float) fret)) * 0.5f;
}

//==============================================================================
namespace
{
    template <typename P> auto cornerOf (const P& p, int) -> decltype (p.corner, bool()) { return p.corner; }
    template <typename P> bool cornerOf (const P&, long) { return false; }

    /*  Everything section 5 draws, built once per guitar. One of these lives for
        the duration of GuitarRenderer::build. */
    class SceneBuilder
    {
    public:
        SceneBuilder (const WorkshopGuitar& g, GuitarRenderer::Options o, GuitarScene& s)
            : guitar (g), options (o), scene (s)
        {
        }

        void run()
        {
            resolveStyle();
            measureStrings();

            buildShadow();
            buildBody();
            buildPickguard();
            buildHoles();
            buildControls();
            buildBridge();
            buildTailpiece();
            buildPickups();
            buildNeck();
            buildHeadstock();
            buildStrings();
            buildHits();

            // Everything drawn, strokes and shadows included, so the fit never clips.
            juce::Rectangle<float> all;

            auto extent = [] (const GuitarScene::Shape& sh)
            {
                auto b = sh.path.getBounds();
                if (! sh.stroke.isInvisible() && sh.strokeMm > 0.0f)
                    b = b.expanded (sh.strokeMm * 0.5f + 0.5f);
                return b;
            };

            for (auto& sh : scene.shapes)       all = all.getUnion (extent (sh));
            for (auto& sh : scene.overStrings)  all = all.getUnion (extent (sh));
            scene.bounds = all.expanded (4.0f);
        }

    private:
        //======================================================================
        const WorkshopGuitar& guitar;
        GuitarRenderer::Options options;
        GuitarScene& scene;

        const outlines::BodyStyle* style = nullptr;
        juce::String family, styleId;
        float L = 450.0f, W = 330.0f, saddleU = 0.6f;

        juce::Path bodyPath;
        int bodyClip = -1;

        int numStrings = 6, numCourses = 6;
        bool twelve = false, isBass = false, fanned = false, fretless = false;
        int numFrets = 22;
        float nutWidth = 43.0f, bridgeSpan = 52.0f, nutSpan = 36.0f, scale = 648.0f;

        std::vector<Pointf> saddle, nut, tail;      // per engine string
        std::vector<float> gaugeMm;
        std::vector<bool> wound;

        juce::Colour hardware;
        juce::Path bridgeArea, tailpieceArea, neckArea, fretboardArea, nutArea, headstockArea,
                   tunerArea, controlsArea, selectorArea, jackArea, pickguardArea, holeArea, tuner, treePath;

        std::vector<Pointf> postPoints;
        Pointf treeCentre;
        bool hasTree = false;

        //======================================================================
        PartPtr part (GuitarSlot slot) const { return guitar.get (slot); }

        juce::String text (GuitarSlot slot, const char* field, const juce::String& fallback = {}) const
        {
            auto p = part (slot);
            return p != nullptr ? p->text (field, fallback) : fallback;
        }

        double number (GuitarSlot slot, const char* field, double fallback) const
        {
            auto p = part (slot);
            return p != nullptr ? p->number (field, fallback) : fallback;
        }

        juce::String partName (GuitarSlot slot) const
        {
            auto p = part (slot);
            return p != nullptr ? p->name : juce::String();
        }

        Pointf body (outlines::Pt p) const { return { (saddleU - p.u) * L, p.v * W }; }

        void add (juce::Path path, juce::FillType fill, juce::FillType stroke = solid (juce::Colours::transparentBlack),
                  float strokeMm = 0.0f, int clip = -1, bool lit = false)
        {
            if (lit && ! options.materials)
                return;

            GuitarScene::Shape s;
            s.path = std::move (path);
            s.fill = fill;
            s.stroke = stroke;
            s.strokeMm = strokeMm;
            s.clip = clip;
            s.lit = lit;
            scene.shapes.push_back (std::move (s));
        }

        void addOver (juce::Path path, juce::FillType fill, juce::FillType stroke = solid (juce::Colours::transparentBlack),
                      float strokeMm = 0.0f)
        {
            GuitarScene::Shape s;
            s.path = std::move (path);
            s.fill = fill;
            s.stroke = stroke;
            s.strokeMm = strokeMm;
            scene.overStrings.push_back (std::move (s));
        }

        /** A soft drop shadow from the key light (visual-polish.md 1): down and right on screen. */
        void shadow (const juce::Path& path, float offsetMm, float alpha)
        {
            if (! options.materials || options.thumbnail)
                return;

            auto p = path;
            p.applyTransform (juce::AffineTransform::translation (-offsetMm, offsetMm));
            add (p, solid (juce::Colours::black.withAlpha (alpha)), none(), 0.0f, -1, true);
            add (p, solid (juce::Colours::transparentBlack), solid (juce::Colours::black.withAlpha (alpha * 0.5f)),
                 offsetMm * 1.2f, -1, true);
        }

        int addClip (const juce::Path& p)
        {
            scene.clips.push_back (p);
            return (int) scene.clips.size() - 1;
        }

        std::vector<Pointf> mapPoints (const outlines::Pt* pts, int n, std::vector<bool>& corners) const
        {
            std::vector<Pointf> out;
            corners.clear();

            for (int i = 0; i < n; ++i)
            {
                out.push_back (body (pts[i]));
                corners.push_back (cornerOf (pts[i], 0));
            }

            return out;
        }

        juce::Path styleShape (const outlines::Pt* pts, int n, bool closed) const
        {
            std::vector<bool> corners;
            const auto mm = mapPoints (pts, n, corners);
            return smoothPath (mm, corners, closed, style != nullptr && style->angular);
        }

        //======================================================================
        void resolveStyle()
        {
            family = guitar.family.isNotEmpty() ? guitar.family.toLowerCase() : juce::String ("electric");
            styleId = guitar.bodyStyle;

            // A body part may name its own style (section 18's drawing metadata).
            if (auto b = part (GuitarSlot::body))
                if (b->illustration.isObject() && b->illustration.hasProperty ("body_style"))
                    styleId = b->illustration.getProperty ("body_style", styleId).toString();

            style = outlines::findBodyStyle (styleId.toRawUTF8());

            if (style == nullptr)
            {
                const char* fallback = family == "acoustic"  ? "dreadnought"
                                     : family == "classical" ? "classical"
                                     : family == "bass"      ? "bass_offset"
                                     : family == "resonator" ? "resonator"
                                                             : "double_cutaway_offset";
                style = outlines::findBodyStyle (fallback);

                if (style == nullptr)
                    style = &outlines::kBodyStyles[0];
            }

            scene.family = family;
            scene.bodyStyle = style->id;

            L = style->lengthMm;
            W = style->widthMm;
            saddleU = style->saddleU;

            hardware = GuitarRenderer::hardwareColour (guitar.hardwareColour);
            isBass = family == "bass";
        }

        //======================================================================
        void measureStrings()
        {
            numStrings = juce::jlimit (1, 12, guitar.getStringCount());
            twelve = numStrings == 12;
            numCourses = twelve ? 6 : numStrings;

            const float defaultScale = isBass ? 864.0f : family == "classical" ? 650.0f
                                     : family == "resonator" ? 635.0f : 648.0f;
            scale = (float) number (GuitarSlot::neck, "scale_length_mm", defaultScale);
            scene.scaleMm = scale;
            scene.numStrings = numStrings;

            fanned = guitar.tags.contains ("fanned") || partName (GuitarSlot::neck).containsIgnoreCase ("multi-scale")
                  || (part (GuitarSlot::neck) != nullptr && part (GuitarSlot::neck)->number ("scale_length_treble_mm", 0.0) > 0.0);

            const auto fretsCount = (int) number (GuitarSlot::frets, "count", -1.0);
            fretless = text (GuitarSlot::frets, "material", "nickel_silver") == "none" || fretsCount == 0;
            numFrets = fretsCount > 0 ? fretsCount : (int) number (GuitarSlot::neck, "frets", isBass ? 20 : 22);
            numFrets = juce::jlimit (12, 36, numFrets);
            scene.numFrets = numFrets;

            // Widths: the nut part's, but never narrower than the strings need.
            const float minNut = isBass ? (numCourses <= 4 ? 38.0f : numCourses == 5 ? 45.0f : 54.0f)
                               : twelve ? 47.0f
                               : numCourses <= 6 ? 42.0f : numCourses == 7 ? 48.0f : 54.0f;
            nutWidth = juce::jmax (minNut, (float) number (GuitarSlot::nut, "width_mm", minNut));

            const float edge = isBass ? 4.0f : 3.3f;
            nutSpan = nutWidth - 2.0f * edge;

            if (isBass)                       bridgeSpan = 19.0f * (float) (numCourses - 1);
            else if (family == "classical")   bridgeSpan = 58.0f;
            else if (family == "acoustic" || family == "resonator") bridgeSpan = 54.0f;
            else                              bridgeSpan = numCourses <= 6 ? 52.0f : 52.0f + 10.0f * (float) (numCourses - 6);

            if (style != nullptr && juce::String (style->id) == "double_cutaway_offset")
                bridgeSpan = 55.0f;

            // Per-course scales (fanned frets): bass course longest, fret 7 perpendicular.
            const float trebleScale = fanned ? (float) juce::jmax (scale - 60.0, number (GuitarSlot::neck, "scale_length_treble_mm", scale - 38.0))
                                             : scale;

            saddle.assign ((size_t) numStrings, {});
            nut.assign ((size_t) numStrings, {});
            gaugeMm.assign ((size_t) numStrings, 0.25f);
            wound.assign ((size_t) numStrings, false);

            const float perpendicular = std::pow (2.0f, -7.0f / 12.0f);
            float meanSaddle = 0.0f;
            std::vector<float> courseScale ((size_t) numCourses), courseSaddleX ((size_t) numCourses);

            for (int c = 0; c < numCourses; ++c)
            {
                // Course 0 is the high E; its scale is the treble one.
                const float t = numCourses > 1 ? (float) c / (float) (numCourses - 1) : 0.0f;
                courseScale[(size_t) c] = trebleScale + (scale - trebleScale) * t;
                courseSaddleX[(size_t) c] = -courseScale[(size_t) c] * perpendicular;
                meanSaddle += courseSaddleX[(size_t) c] / (float) numCourses;
            }

            for (int s = 0; s < numStrings; ++s)
            {
                const int c = twelve ? s / 2 : s;
                const float t = numCourses > 1 ? (float) c / (float) (numCourses - 1) : 0.5f;
                float yS = bridgeSpan * 0.5f - t * bridgeSpan;
                float yN = nutSpan * 0.5f - t * nutSpan;

                if (twelve)
                {
                    // The octave string of a pair sits on the bass side, struck first.
                    const float offset = (s % 2 == 1) ? -1.0f : 1.0f;
                    yS += offset * 1.1f;
                    yN += offset * 0.8f;
                }

                const float sx = fanned ? courseSaddleX[(size_t) c] - meanSaddle : 0.0f;
                saddle[(size_t) s] = { sx, yS };
                nut[(size_t) s] = { sx + courseScale[(size_t) c], yN };
            }

            scene.saddlePoints = saddle;
            scene.nutPoints = nut;

            // Gauges: the part lists them in the engine's order (DECISIONS.md).
            const auto gauges = part (GuitarSlot::strings) != nullptr ? part (GuitarSlot::strings)->numbers ("gauges_in")
                                                                      : juce::Array<double>();
            const auto material = text (GuitarSlot::strings, "winding_material", isBass ? "nickel_plated_steel" : "nickel_plated_steel");

            for (int s = 0; s < numStrings; ++s)
            {
                const double g = juce::isPositiveAndBelow (s, gauges.size()) ? gauges[s]
                                 : (isBass ? 0.045 + 0.02 * s : 0.010 + 0.007 * s);
                gaugeMm[(size_t) s] = (float) g * kInch;

                if (material == "nylon")
                    wound[(size_t) s] = ! twelve && s >= 3;
                else
                    wound[(size_t) s] = isBass || g >= 0.0195;
            }
        }

        //======================================================================
        // Layer 1: the backdrop shadow of the whole silhouette.
        void buildShadow()
        {
            bodyPath = styleShape (style->outline, style->numOutline, true);

            if (auto b = part (GuitarSlot::body))
            {
                // Section 18: a body part may carry its own outline, in the style's u / v.
                const auto custom = b->illustration.getProperty ("outline", {});

                if (auto* arr = custom.getArray(); arr != nullptr && arr->size() >= 6)
                {
                    std::vector<Pointf> pts;
                    std::vector<bool> corners;

                    for (auto& v : *arr)
                        if (auto* pair = v.getArray(); pair != nullptr && pair->size() >= 2)
                        {
                            pts.push_back (body ({ (float) (double) (*pair)[0], (float) (double) (*pair)[1] }));
                            corners.push_back (pair->size() >= 3 && (bool) (*pair)[2]);
                        }

                    if (pts.size() >= 6)
                        bodyPath = smoothPath (pts, corners, true, style->angular);
                }
            }

            bodyClip = addClip (bodyPath);

            if (options.materials && ! options.thumbnail)
            {
                auto p = bodyPath;
                p.applyTransform (juce::AffineTransform::translation (-5.0f, 7.0f));
                add (p, solid (juce::Colours::black.withAlpha (0.18f)), solid (juce::Colours::black.withAlpha (0.10f)), 10.0f, -1, true);
                add (p, solid (juce::Colours::black.withAlpha (0.22f)), none(), 0.0f, -1, true);
            }
        }

        //======================================================================
        juce::Colour bodyWood() const  { return GuitarRenderer::woodColour (text (GuitarSlot::body, "wood", "alder")); }

        juce::Colour topWood() const
        {
            if (part (GuitarSlot::top) != nullptr)
                return GuitarRenderer::woodColour (text (GuitarSlot::top, "wood", "maple"));
            return bodyWood();
        }

        bool isAcousticBody() const
        {
            return family == "acoustic" || family == "classical" || style->hole == outlines::Hole::round
                || style->hole == outlines::Hole::oval || style->hole == outlines::Hole::dShape;
        }

        /** Section 11: the finish on the top, and how much of the wood shows through. */
        void buildBody()
        {
            const auto& f = guitar.finish;
            const auto type = f.type.toLowerCase();
            const auto aging = (float) juce::jlimit (0.0, 1.0, f.aging);

            auto a = parseHex (f.colourA, juce::Colour (0xff7a2e1b));
            auto b = parseHex (f.colourB, juce::Colour (0xfff2c441));

            // 11.7: faded saturation, whites yellowing.
            auto age = [aging] (juce::Colour c)
            {
                if (aging <= 0.0f)
                    return c;
                c = c.withSaturation (c.getSaturation() * (1.0f - 0.3f * aging));
                if (c.getBrightness() > 0.8f)
                    c = c.interpolatedWith (juce::Colour (0xffe8d49a), 0.35f * aging);
                return c;
            };

            a = age (a);
            b = age (b);

            // Layer 2/3: sides show as a thin strip on acoustics (a darker edge band).
            if (isAcousticBody())
                add (bodyPath, solid (bodyWood().darker (0.3f)));

            // Layer 4: the top.
            bool showGrain = true;
            const auto wood = topWood();
            const bool isMetalBody = text (GuitarSlot::body, "wood") == "steel" || type == "metal";

            if (type == "solid" || type == "sparkle")
            {
                add (bodyPath, solid (a));
                showGrain = false;
            }
            else if (type == "metallic" || isMetalBody)
            {
                const auto bb = bodyPath.getBounds();
                juce::ColourGradient grad (a.brighter (0.35f), bb.getRight(), bb.getY(), a.darker (0.35f), bb.getX(), bb.getBottom(), false);
                grad.addColour (0.45, a);
                grad.addColour (0.7, a.darker (0.15f));
                add (bodyPath, options.materials ? juce::FillType (grad) : solid (a));
                showGrain = false;
            }
            else if (type == "burst")
            {
                // Centre colour (b) on the lower bout, the edge colour (a) following the outline.
                const auto centre = body ({ juce::jmin (0.72f, saddleU + 0.06f), 0.03f });
                const float r = 0.62f * juce::jmax (L, W);

                juce::ColourGradient grad (b, centre.x, centre.y, a, centre.x + r, centre.y, true);
                grad.addColour (0.35, b);
                grad.addColour (0.62, b.interpolatedWith (a, 0.35f));
                add (bodyPath, juce::FillType (grad));

                const bool threeTone = f.burstShape.containsIgnoreCase ("3");

                if (threeTone)
                    for (float w : { 90.0f, 70.0f })
                        add (bodyPath, solid (juce::Colours::transparentBlack), solid (juce::Colour (0xffb2401e).withAlpha (0.28f)), w, bodyClip);

                // The edge band follows the outline, as a sprayed burst does.
                for (float w : { 64.0f, 50.0f, 40.0f, 31.0f, 23.0f, 16.0f, 10.0f, 5.0f })
                    add (bodyPath, solid (juce::Colours::transparentBlack), solid (a.withAlpha (0.2f)), w, bodyClip);
            }
            else if (type == "transparent")
            {
                add (bodyPath, solid (wood));
                add (bodyPath, solid (a.withAlpha (0.6f)));
            }
            else
            {
                // natural / oil: the file's colour_a is the wood as finished.
                add (bodyPath, solid (f.colourA.isNotEmpty() ? a : wood));
            }

            // Layer 6: grain, where the finish lets it show.
            if (showGrain && ! options.thumbnail)
                buildGrain (type == "burst" ? 0.10f : 0.08f);

            // 11.6 sparkle: bright flakes at low opacity.
            if (type == "sparkle")
            {
                juce::Random rng ((juce::int64) guitar.seed + 7);
                juce::Path flakes;
                const auto bb = bodyPath.getBounds();

                for (int i = 0; i < 2600; ++i)
                {
                    const Pointf p { bb.getX() + rng.nextFloat() * bb.getWidth(), bb.getY() + rng.nextFloat() * bb.getHeight() };
                    flakes.addEllipse (p.x - 0.4f, p.y - 0.4f, 0.8f, 0.8f);
                }

                add (flakes, solid (b.getBrightness() > 0.2f ? b.withAlpha (0.35f) : juce::Colours::white.withAlpha (0.3f)), none(), 0.0f, bodyClip);
            }

            // Layer 5: the arch / bevel / contour line from the style.
            if (style->numArch > 1)
            {
                const auto arch = styleShape (style->arch, style->numArch, style->archClosed);
                add (arch, solid (juce::Colours::transparentBlack), solid (juce::Colours::white.withAlpha (0.16f)), 3.0f, bodyClip, true);
                add (arch, solid (juce::Colours::transparentBlack), solid (juce::Colours::black.withAlpha (0.10f)), 1.2f, bodyClip, true);
            }

            buildLighting();
            buildBinding();

            if (aging > 0.0f && ! options.thumbnail)
                buildAging (aging, a);
        }

        void buildGrain (float alpha)
        {
            const auto bb = bodyPath.getBounds();
            juce::Random rng ((juce::int64) guitar.seed * 31 + 3);
            const auto topName = partName (GuitarSlot::top).toLowerCase();
            const auto wood = text (GuitarSlot::top, "wood", text (GuitarSlot::body, "wood", "alder")).toLowerCase();
            const auto dark = juce::Colour (0xff2a160a);

            juce::Path lines, bands, light;

            if (topName.contains ("flame") || topName.contains ("quilt") || topName.contains ("spalted"))
            {
                const bool quilt = topName.contains ("quilt");

                if (quilt)
                {
                    for (int i = 0; i < 260; ++i)
                    {
                        const float x = bb.getX() + rng.nextFloat() * bb.getWidth();
                        const float y = bb.getY() + rng.nextFloat() * bb.getHeight();
                        const float r = 4.0f + rng.nextFloat() * 9.0f;
                        (i % 2 == 0 ? bands : light).addEllipse (x - r * 1.4f, y - r, r * 2.8f, r * 2.0f);
                    }
                }
                else
                {
                    // Flame: bands across the width, rippling along the book-match seam.
                    for (float x = bb.getX(); x < bb.getRight(); x += 3.5f + rng.nextFloat() * 5.0f)
                    {
                        auto& target = (rng.nextBool() ? bands : light);
                        const float w = 1.5f + rng.nextFloat() * 3.5f;
                        const float lean = 6.0f + rng.nextFloat() * 8.0f;
                        target.startNewSubPath (x, bb.getY());
                        target.quadraticTo (x + lean, bb.getCentreY(), x, bb.getBottom());
                        target.lineTo (x + w, bb.getBottom());
                        target.quadraticTo (x + w + lean, bb.getCentreY(), x + w, bb.getY());
                        target.closeSubPath();
                    }
                }

                add (bands, solid (dark.withAlpha (alpha * 1.4f)), none(), 0.0f, bodyClip);
                add (light, solid (juce::Colours::white.withAlpha (alpha)), none(), 0.0f, bodyClip);
            }

            // Straight grain along the axis: spruce and cedar tight, ash open with cathedrals.
            const bool ash = wood.startsWith ("ash");
            const float pitch = ash ? 4.5f : (wood.startsWith ("spruce") || wood.startsWith ("cedar")) ? 1.6f : 3.2f;
            const float strength = wood.startsWith ("mahogany") || wood.startsWith ("alder") ? 0.5f : 1.0f;

            for (float y = bb.getY(); y < bb.getBottom(); y += pitch * (0.6f + rng.nextFloat() * 0.8f))
            {
                const float wobble = rng.nextFloat() * 2.0f - 1.0f;
                lines.startNewSubPath (bb.getRight(), y);
                lines.cubicTo (bb.getCentreX() + 60.0f, y + wobble * 2.5f, bb.getCentreX() - 60.0f, y - wobble * 2.5f, bb.getX(), y + wobble);
            }

            if (ash)
                for (int i = 0; i < 9; ++i)
                {
                    const float cy = bb.getCentreY() + (rng.nextFloat() - 0.5f) * 60.0f;
                    const float r = 30.0f + (float) i * 14.0f;
                    lines.addCentredArc (bb.getX() + bb.getWidth() * 0.3f, cy, r * 2.2f, r, 0.0f,
                                         juce::MathConstants<float>::pi * 0.15f, juce::MathConstants<float>::pi * 0.85f, true);
                }

            add (lines, solid (juce::Colours::transparentBlack), solid (dark.withAlpha (alpha * strength)), 0.35f, bodyClip);
        }

        /** visual-polish.md 1: diffuse falloff from the top-left key light, sheen, edge highlight. */
        void buildLighting()
        {
            if (! options.materials)
                return;

            const auto bb = bodyPath.getBounds();
            const Pointf light { bb.getRight() - bb.getWidth() * 0.25f, bb.getY() + bb.getHeight() * 0.18f };
            const float r = juce::jmax (bb.getWidth(), bb.getHeight()) * 1.05f;

            juce::ColourGradient diffuse (juce::Colours::white.withAlpha (0.10f), light.x, light.y,
                                          juce::Colours::black.withAlpha (0.28f), light.x - r, light.y + r * 0.4f, true);
            diffuse.addColour (0.45, juce::Colours::transparentBlack);
            add (bodyPath, juce::FillType (diffuse), none(), 0.0f, bodyClip, true);

            // Lacquer sheen: gloss > 0.6 a soft band, satin a wide faint one, oil none.
            const float gloss = (float) guitar.finish.gloss;
            const auto type = guitar.finish.type.toLowerCase();

            if (type != "oil" && gloss > 0.25f)
            {
                const bool glossy = gloss > 0.6f;
                const auto c = body ({ saddleU + 0.05f, -0.18f });
                juce::Path band;
                const float bw = glossy ? L * 0.22f : L * 0.38f, bh = glossy ? W * 0.07f : W * 0.16f;
                band.addEllipse (c.x - bw, c.y - bh, bw * 2.0f, bh * 2.0f);
                band.applyTransform (juce::AffineTransform::rotation (-0.35f, c.x, c.y));

                juce::ColourGradient sheen (juce::Colours::white.withAlpha ((glossy ? 0.22f : 0.08f) * gloss), c.x, c.y,
                                            juce::Colours::white.withAlpha (0.0f), c.x + bw, c.y, true);
                add (band, juce::FillType (sheen), none(), 0.0f, bodyClip, true);
            }

            // Specular along the bevelled edge, strongest nearest the light.
            juce::ColourGradient edge (juce::Colours::white.withAlpha (0.45f), bb.getRight(), bb.getY(),
                                       juce::Colours::white.withAlpha (0.0f), bb.getCentreX(), bb.getBottom(), false);
            add (bodyPath, solid (juce::Colours::transparentBlack), juce::FillType (edge), 1.6f, bodyClip, true);
        }

        void buildBinding()
        {
            const auto id = juce::String (style->id);
            const bool acoustic = isAcousticBody();
            const bool bound = acoustic || id.contains ("arched") || id.contains ("semi") || id == "archtop"
                            || id.contains ("hollow") || id.contains ("violin") || id.startsWith ("gypsy");

            if (! bound)
            {
                // Unbound bodies still read their edge: a thin dark line.
                add (bodyPath, solid (juce::Colours::transparentBlack), solid (juce::Colours::black.withAlpha (0.45f)), 0.6f);
                return;
            }

            const auto binding = family == "classical" ? bodyWood().darker (0.4f) : juce::Colour (0xffece2c8);
            add (bodyPath, solid (juce::Colours::transparentBlack), solid (binding), 4.2f, bodyClip);

            // Purfling just inside the binding.
            add (bodyPath, solid (juce::Colours::transparentBlack), solid (juce::Colour (0xff1c120c).withAlpha (0.8f)), 5.8f, bodyClip);
            add (bodyPath, solid (juce::Colours::transparentBlack), solid (binding), 3.6f, bodyClip);
            add (bodyPath, solid (juce::Colours::transparentBlack), solid (juce::Colours::black.withAlpha (0.55f)), 0.6f);
        }

        void buildAging (float aging, juce::Colour finish)
        {
            juce::Random rng ((juce::int64) guitar.seed);
            const auto bb = bodyPath.getBounds();

            // Edge wear: lighter scuffs along the outline.
            juce::Path scuffs;
            const float dashes[] = { 3.0f + 10.0f * aging, 9.0f - 5.0f * aging };
            juce::PathStrokeType (3.0f + 5.0f * aging).createDashedStroke (scuffs, bodyPath, dashes, 2);
            const auto bare = topWood().interpolatedWith (juce::Colour (0xffd8c29a), 0.4f);
            add (scuffs, solid (bare.withAlpha (0.35f * aging)), none(), 0.0f, bodyClip);

            // Dings, seeded by the guitar's character seed so they land in the same places every run.
            juce::Path dings;
            const int count = (int) (aging * 45.0f);

            for (int i = 0, tries = 0; i < count && tries < count * 20; ++tries)
            {
                const Pointf p { bb.getX() + rng.nextFloat() * bb.getWidth(), bb.getY() + rng.nextFloat() * bb.getHeight() };

                if (! bodyPath.contains (p))
                    continue;

                const float r = 0.5f + rng.nextFloat() * 1.4f;
                dings.addEllipse (p.x - r, p.y - r * 0.7f, r * 2.0f, r * 1.4f);
                ++i;
            }

            add (dings, solid (finish.darker (0.6f).withAlpha (0.55f)), none(), 0.0f, bodyClip);

            // Checked lacquer at high aging.
            if (aging > 0.5f)
            {
                juce::Path checks;

                for (int i = 0; i < 60; ++i)
                {
                    const float x = bb.getX() + rng.nextFloat() * bb.getWidth();
                    const float y = bb.getY() + rng.nextFloat() * bb.getHeight();
                    const float len = 8.0f + rng.nextFloat() * 20.0f;
                    checks.startNewSubPath (x, y);
                    checks.lineTo (x + (rng.nextFloat() - 0.5f) * 4.0f, y + len);
                }

                add (checks, solid (juce::Colours::transparentBlack), solid (juce::Colours::black.withAlpha (0.08f * aging)), 0.25f, bodyClip);
            }
        }

        //======================================================================
        juce::Colour pickguardColour (bool& tortoise, bool& threePly) const
        {
            const auto n = partName (GuitarSlot::pickguard).toLowerCase();
            tortoise = n.contains ("tortoise");
            threePly = number (GuitarSlot::pickguard, "plies", 1.0) >= 3.0;

            if (tortoise)                  return juce::Colour (0xff6b2e14);
            if (n.contains ("black"))      return juce::Colour (0xff141414);
            if (n.contains ("cream"))      return juce::Colour (0xffe8d8b2);
            if (n.contains ("mint"))       return juce::Colour (0xffd3e2c6);
            if (n.contains ("aged"))       return juce::Colour (0xffe9dfc6);
            if (n.contains ("red"))        return juce::Colour (0xff9e2a24);
            if (n.contains ("gold"))       return juce::Colour (0xffc9a24b);
            if (n.contains ("pearl"))      return juce::Colour (0xfff1ede4);
            return juce::Colour (0xfff2efe6);
        }

        void buildPickguard()
        {
            auto pg = part (GuitarSlot::pickguard);

            if (pg == nullptr || style->numPickguard < 3 || pg->text ("material", "pvc") == "none"
                || pg->name.equalsIgnoreCase ("none"))
                return;

            bool tortoise = false, threePly = false;
            auto colour = pickguardColour (tortoise, threePly);

            if (guitar.finish.aging > 0.0 && colour.getBrightness() > 0.8f)
                colour = colour.interpolatedWith (juce::Colour (0xffe6d29c), 0.4f * (float) guitar.finish.aging);

            pickguardArea = styleShape (style->pickguard, style->numPickguard, true);
            shadow (pickguardArea, 1.2f, 0.22f);

            add (pickguardArea, solid (colour));

            if (tortoise && ! options.thumbnail)
            {
                // Two-tone stipple: darker blotches over the amber-brown base.
                juce::Random rng (1234);
                juce::Path blotches, amber;
                const auto bb = pickguardArea.getBounds();

                for (int i = 0; i < 420; ++i)
                {
                    const float x = bb.getX() + rng.nextFloat() * bb.getWidth();
                    const float y = bb.getY() + rng.nextFloat() * bb.getHeight();
                    const float r = 1.5f + rng.nextFloat() * 5.5f;
                    (i % 3 == 0 ? amber : blotches).addEllipse (x - r * 1.3f, y - r, r * 2.6f, r * 2.0f);
                }

                const int clip = addClip (pickguardArea);
                add (blotches, solid (juce::Colour (0xff2a0f06).withAlpha (0.55f)), none(), 0.0f, clip);
                add (amber, solid (juce::Colour (0xffb8641e).withAlpha (0.35f)), none(), 0.0f, clip);
            }

            // Bevelled edge: 3-ply shows its middle layer as a contrasting line.
            if (threePly)
            {
                const auto ply = colour.getBrightness() > 0.5f ? juce::Colour (0xff161616) : juce::Colour (0xffeee8da);
                const int clip = addClip (pickguardArea);
                add (pickguardArea, solid (juce::Colours::transparentBlack), solid (ply), 2.6f, clip);
                add (pickguardArea, solid (juce::Colours::transparentBlack), solid (colour), 1.2f, clip);
            }

            add (pickguardArea, solid (juce::Colours::transparentBlack), solid (juce::Colours::black.withAlpha (0.5f)), 0.4f);

            // Screws around the edge.
            if (! options.thumbnail)
            {
                const auto c = pickguardArea.getBounds().getCentre();
                juce::Path screws;

                for (int i = 0; i < style->numPickguard; i += 3)
                {
                    auto p = body (style->pickguard[i]);
                    p = p + (c - p) * (4.0f / juce::jmax (4.0f, p.getDistanceFrom (c)));
                    screws.addEllipse (p.x - 1.3f, p.y - 1.3f, 2.6f, 2.6f);
                }

                add (screws, metalFill (hardware, screws.getBounds(), options.materials),
                     solid (juce::Colours::black.withAlpha (0.4f)), 0.25f);
            }
        }

        //======================================================================
        juce::Path holeShape (outlines::Hole type, Pointf c, float along, float across, bool mirrored) const
        {
            juce::Path p;

            switch (type)
            {
                case outlines::Hole::round:
                    p.addEllipse (c.x - along * 0.5f, c.y - across * 0.5f, along, across);
                    break;

                case outlines::Hole::oval:
                    p.addEllipse (c.x - along * 0.5f, c.y - across * 0.5f, along, across);
                    break;

                case outlines::Hole::dShape:
                {
                    // Flat side toward the bridge (lower X).
                    const float r = across * 0.5f;
                    p.startNewSubPath (c.x - along * 0.5f, c.y - r);
                    p.lineTo (c.x - along * 0.5f, c.y + r);
                    p.lineTo (c.x + along * 0.5f - r, c.y + r);
                    p.addCentredArc (c.x + along * 0.5f - r, c.y, r, r, 0.0f, juce::MathConstants<float>::pi, 0.0f);
                    p.closeSubPath();
                    break;
                }

                case outlines::Hole::fHoles:
                {
                    // An f: round eyes at each end joined by a slender S, leaning its neck end toward the strings.
                    const float len = along, lean = across * 0.35f * (mirrored ? 1.0f : -1.0f);
                    const Pointf top { c.x + len * 0.5f, c.y + lean * 0.6f }, bottom { c.x - len * 0.5f, c.y - lean * 0.6f };
                    const float eyeTop = juce::jmax (2.5f, across * 0.11f), eyeBottom = juce::jmax (3.0f, across * 0.14f);
                    const float slot = juce::jmax (1.2f, across * 0.07f);
                    const float bow = across * 0.28f * (mirrored ? 1.0f : -1.0f);

                    juce::Path s;
                    s.startNewSubPath (top);
                    s.cubicTo (top.x - len * 0.3f, top.y + bow, bottom.x + len * 0.3f, bottom.y - bow, bottom.x, bottom.y);
                    p = strokeOf (s, slot);
                    p.addEllipse (top.x - eyeTop, top.y - eyeTop, eyeTop * 2.0f, eyeTop * 2.0f);
                    p.addEllipse (bottom.x - eyeBottom, bottom.y - eyeBottom, eyeBottom * 2.0f, eyeBottom * 2.0f);

                    // The two nicks at the waist.
                    const auto mid = (top + bottom) * 0.5f;
                    p.addRectangle (mid.x - 0.6f, mid.y - across * 0.22f, 1.2f, across * 0.44f);
                    break;
                }

                case outlines::Hole::resonatorCover:
                    p.addEllipse (c.x - along * 0.5f, c.y - along * 0.5f, along, along);
                    break;

                case outlines::Hole::none:
                    break;
            }

            return p;
        }

        void buildHoles()
        {
            const auto dark = juce::Colour (0xff120a06);

            if (style->hole == outlines::Hole::resonatorCover)
            {
                buildResonatorCover();
            }
            else if (style->hole == outlines::Hole::fHoles)
            {
                const auto c = body (style->holeCentre);
                const auto c2 = body ({ style->holeCentre.u, -style->holeCentre.v });
                holeArea = holeShape (outlines::Hole::fHoles, c, style->holeWidthMm, style->holeHeightMm, false);
                holeArea.addPath (holeShape (outlines::Hole::fHoles, c2, style->holeWidthMm, style->holeHeightMm, true));
                add (holeArea, solid (dark), solid (juce::Colours::black), 0.3f);
            }
            else if (style->hole != outlines::Hole::none)
            {
                const auto c = body (style->holeCentre);
                holeArea = holeShape (style->hole, c, style->holeWidthMm, style->holeHeightMm, false);
                buildRosette (c);

                if (family == "classical" && guitar.name.containsIgnoreCase ("flamenc"))
                    buildGolpeador (c);

                add (holeArea, solid (dark));

                // Layer 9: bracing seen through the hole.
                if (! options.thumbnail)
                {
                    juce::Path brace;
                    const float r = style->holeWidthMm * 0.5f;
                    const auto bracing = text (GuitarSlot::body, "bracing", "x");

                    if (bracing == "fan")
                        for (int i = -2; i <= 2; ++i)
                            brace.addRectangle (c.x - r, c.y + (float) i * r * 0.35f - 1.2f, r * 2.0f, 2.4f);
                    else
                    {
                        brace.addRectangle (c.x - r, c.y - 3.0f, r * 2.0f, 6.0f);
                        brace.applyTransform (juce::AffineTransform::rotation (0.5f, c.x, c.y));
                        juce::Path b2;
                        b2.addRectangle (c.x - r, c.y - 3.0f, r * 2.0f, 6.0f);
                        b2.applyTransform (juce::AffineTransform::rotation (-0.5f, c.x, c.y));
                        brace.addPath (b2);
                    }

                    add (brace, solid (juce::Colour (0xff3a2616).withAlpha (0.5f)), none(), 0.0f, addClip (holeArea));
                }

                add (holeArea, solid (juce::Colours::transparentBlack), solid (juce::Colours::black.withAlpha (0.6f)), 0.5f);
            }

            if (style->upperHoles != outlines::Hole::none)
            {
                juce::Path upper;
                const auto c = body (style->upperHoleCentre);
                const auto c2 = body ({ style->upperHoleCentre.u, -style->upperHoleCentre.v });
                upper.addPath (holeShape (style->upperHoles, c, style->upperHoleWidthMm, style->upperHoleHeightMm, false));
                upper.addPath (holeShape (style->upperHoles, c2, style->upperHoleWidthMm, style->upperHoleHeightMm, true));
                add (upper, solid (dark), metalFill (hardware, upper.getBounds(), options.materials), 1.2f);

                // Screens over a resonator's upper holes.
                if (family == "resonator" && ! options.thumbnail)
                {
                    juce::Path mesh;
                    const auto bb = upper.getBounds();
                    for (float x = bb.getX(); x < bb.getRight(); x += 2.2f)
                        mesh.addRectangle (x, bb.getY(), 0.35f, bb.getHeight());
                    add (mesh, solid (hardware.withAlpha (0.6f)), none(), 0.0f, addClip (upper));
                }

                holeArea.addPath (upper);
            }
        }

        void buildRosette (Pointf c)
        {
            const float r = juce::jmax (style->holeWidthMm, style->holeHeightMm) * 0.5f;
            const bool classical = family == "classical";
            const bool oval = style->hole != outlines::Hole::round;

            auto ring = [&] (float inner, float outer, juce::Colour colour)
            {
                juce::Path p;
                const float sx = oval ? style->holeWidthMm / (2.0f * r) : 1.0f;
                const float sy = oval ? style->holeHeightMm / (2.0f * r) : 1.0f;
                p.addEllipse (c.x - (r + outer) * sx, c.y - (r + outer) * sy, 2.0f * (r + outer) * sx, 2.0f * (r + outer) * sy);
                p.addEllipse (c.x - (r + inner) * sx, c.y - (r + inner) * sy, 2.0f * (r + inner) * sx, 2.0f * (r + inner) * sy);
                p.setUsingNonZeroWinding (false);
                add (p, solid (colour));
            };

            if (classical)
            {
                // Mosaic: a wide band of alternating tiles between plain rings.
                ring (2.0f, 3.2f, juce::Colour (0xff1a100a));
                ring (3.2f, 11.0f, juce::Colour (0xff6b3a1c));

                if (! options.thumbnail)
                {
                    juce::Path tiles;
                    for (int i = 0; i < 72; ++i)
                    {
                        const float a0 = juce::MathConstants<float>::twoPi * (float) i / 72.0f;
                        const float a1 = a0 + juce::MathConstants<float>::twoPi / 144.0f;
                        const float rin = r + 4.0f, rout = r + 10.0f;
                        tiles.startNewSubPath (c.x + std::cos (a0) * rin, c.y + std::sin (a0) * rin);
                        tiles.lineTo (c.x + std::cos (a0) * rout, c.y + std::sin (a0) * rout);
                        tiles.lineTo (c.x + std::cos (a1) * rout, c.y + std::sin (a1) * rout);
                        tiles.lineTo (c.x + std::cos (a1) * rin, c.y + std::sin (a1) * rin);
                        tiles.closeSubPath();
                    }
                    add (tiles, solid (juce::Colour (0xffd8b56a)));
                    ring (6.5f, 7.5f, juce::Colour (0xff2c6b4a));
                }

                ring (11.0f, 12.2f, juce::Colour (0xff1a100a));
                return;
            }

            // Steel-string acoustic: concentric black / white rings with a herringbone-ish band.
            ring (3.0f, 4.0f, juce::Colour (0xff1a1410));
            ring (4.6f, 5.4f, juce::Colour (0xffeee6d4));
            ring (5.4f, 7.8f, juce::Colour (0xff1a1410));
            ring (7.8f, 8.6f, juce::Colour (0xffeee6d4));
            ring (9.4f, 10.2f, juce::Colour (0xff1a1410));
        }

        void buildGolpeador (Pointf c)
        {
            // Clear tap plates either side of the soundhole (4.3 Flamenca).
            const float r = style->holeWidthMm * 0.5f;
            juce::Path plate;
            plate.addRoundedRectangle (c.x - r * 1.6f, c.y + r * 0.9f, r * 3.0f, r * 1.5f, r * 0.6f);
            plate.addRoundedRectangle (c.x - r * 1.6f, c.y - r * 2.4f, r * 3.0f, r * 1.5f, r * 0.6f);
            add (plate, solid (juce::Colours::white.withAlpha (0.12f)), solid (juce::Colours::white.withAlpha (0.25f)), 0.4f, bodyClip);
        }

        void buildResonatorCover()
        {
            const auto c = body (style->holeCentre);
            const float d = style->holeWidthMm, r = d * 0.5f;

            holeArea = circle (c, r);
            shadow (holeArea, 1.5f, 0.3f);

            add (holeArea, metalFill (hardware, holeArea.getBounds(), options.materials),
                 solid (hardware.darker (0.5f)), 0.8f);

            if (! options.thumbnail)
            {
                // Perforations: diamonds in rings, and the inner screen.
                juce::Path diamonds;

                for (int ringIndex = 0; ringIndex < 3; ++ringIndex)
                {
                    const float rr = r * (0.52f + 0.14f * (float) ringIndex);
                    const int count = 18 + 6 * ringIndex;

                    for (int i = 0; i < count; ++i)
                    {
                        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) count;
                        const Pointf p { c.x + std::cos (a) * rr, c.y + std::sin (a) * rr };
                        diamonds.addPath (polygon ({ { p.x + 2.2f, p.y }, { p.x, p.y + 2.2f }, { p.x - 2.2f, p.y }, { p.x, p.y - 2.2f } }));
                    }
                }

                add (diamonds, solid (juce::Colour (0xff120a06)));
                add (circle (c, r * 0.36f), metalFill (hardware.darker (0.1f), holeArea.getBounds(), options.materials),
                     solid (hardware.darker (0.5f)), 0.6f);
            }
        }

        //======================================================================
        // Stubs filled in below.
        void buildControls();
        void buildBridge();
        void buildTailpiece();
        void buildPickups();
        void buildNeck();
        void buildHeadstock();
        void buildStrings();
        void buildHits();

        // Helpers for the stubs.
        float spanAt (float x) const { return bridgeSpan + (nutSpan - bridgeSpan) * juce::jlimit (0.0f, 1.0f, x / scale); }
        juce::Colour plasticColour() const;
        void knob (Pointf c, int kind);
        void pickup (int index, PartPtr p);
        void saddleBlocks (float depth, float width, float stagger);
        void pinBridge();
        void tieBlock();
        void floatingBridge (bool moustache);
        void tremPlate (bool twoPoint);
        void bassBridge (bool highMass);
        juce::Path fretboardOutline (float margin, float extraAtEnd, float endFret) const;
    };

    //==========================================================================
    juce::Colour SceneBuilder::plasticColour() const
    {
        bool tortoise = false, threePly = false;

        if (part (GuitarSlot::pickguard) != nullptr && ! partName (GuitarSlot::pickguard).equalsIgnoreCase ("none"))
        {
            const auto c = pickguardColour (tortoise, threePly);
            if (c.getBrightness() < 0.2f)
                return juce::Colour (0xff151515);
        }

        const auto base = juce::Colour (0xfff0ebdd);
        return guitar.finish.aging > 0.0 ? base.interpolatedWith (juce::Colour (0xffe6d29c), (float) guitar.finish.aging * 0.6f) : base;
    }

    //==========================================================================
    // Layers 21-24: knobs, switch, jack, strap buttons.
    void SceneBuilder::knob (Pointf c, int kind)
    {
        const float r = kind == 2 ? 9.5f : 9.0f;
        shadow (circle (c, r), 1.2f, 0.3f);

        // Pointer at "10", up and to the right on screen: scene +X, -Y.
        const Pointf dir = Pointf (0.55f, -0.83f);
        juce::Path pointer;
        pointer.startNewSubPath (c + dir * (r * 0.25f));
        pointer.lineTo (c + dir * (r * 0.85f));

        switch (kind)
        {
            case 0:   // top hat, amber with a gold reflector
            {
                add (circle (c, r + 0.8f), solid (juce::Colour (0xff2a1206)));
                add (circle (c, r), metalFill (juce::Colour (0xffc9a24b), circle (c, r).getBounds(), options.materials));
                juce::ColourGradient amber (juce::Colour (0xffb8622a), c.x + 3.0f, c.y - 3.0f, juce::Colour (0xff3a1406), c.x - 6.0f, c.y + 6.0f, true);
                add (circle (c, r * 0.72f), options.materials ? juce::FillType (amber) : solid (juce::Colour (0xff7a3610)));
                add (pointer, none(), solid (juce::Colour (0xffe8d8a8)), 1.2f);
                break;
            }
            case 1:   // bell / skirted plastic
            {
                const auto plastic = plasticColour();
                add (circle (c, r), solid (plastic.darker (0.15f)), solid (plastic.darker (0.5f)), 0.4f);
                add (circle (c, r * 0.72f), metalFill (plastic, circle (c, r).getBounds(), options.materials));
                add (pointer, none(), solid (plastic.getBrightness() > 0.5f ? juce::Colour (0xff222222) : juce::Colour (0xffe8e2d2)), 0.9f);
                break;
            }
            case 2:   // chrome dome, knurled
            {
                add (circle (c, r), metalFill (hardware, circle (c, r).getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.4f);

                if (! options.thumbnail)
                {
                    juce::Path knurl;
                    for (int i = 0; i < 36; ++i)
                    {
                        const float a = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
                        knurl.startNewSubPath (c.x + std::cos (a) * r * 0.82f, c.y + std::sin (a) * r * 0.82f);
                        knurl.lineTo (c.x + std::cos (a) * r, c.y + std::sin (a) * r);
                    }
                    add (knurl, none(), solid (hardware.darker (0.6f).withAlpha (0.6f)), 0.25f);
                }

                add (circle (c, r * 0.55f), metalFill (hardware.brighter (0.1f), circle (c, r).getBounds(), options.materials));
                break;
            }
            default:  // black speed knob
            {
                add (circle (c, r), solid (juce::Colour (0xff121212)), solid (juce::Colours::black), 0.4f);
                if (options.materials)
                    add (circle (c + Pointf (1.8f, -1.8f), r * 0.45f), solid (juce::Colours::white.withAlpha (0.14f)), none(), 0.0f, -1, true);
                add (pointer, none(), solid (juce::Colour (0xffd8d8d8)), 0.9f);
                break;
            }
        }
    }

    void SceneBuilder::buildControls()
    {
        const juce::String id (style->id);
        const auto switching = text (GuitarSlot::wiring, "switching", "3way");

        int wanted = switching == "3way_independent" ? 4
                   : switching == "5way" || switching == "blend" || switching == "independent_volumes" ? 3
                   : switching == "single" ? 1 : 2;

        if (part (GuitarSlot::wiring) == nullptr)
            wanted = style->numControls;

        const int count = juce::jmin (wanted, style->numControls);

        // A Tele-style or J-bass control plate carries the knobs and the switch.
        const bool plate = id == "single_cutaway_slab" || (id.startsWith ("bass_offset") && count > 0);

        if (plate && count > 0)
        {
            juce::Path line;
            line.startNewSubPath (body (style->controls[0]));
            for (int i = 1; i < count; ++i)
                line.lineTo (body (style->controls[i]));
            if (style->hasSelector && id == "single_cutaway_slab")
                line.lineTo (body (style->selector));

            const auto platePath = strokeOf (line, 26.0f);
            shadow (platePath, 1.0f, 0.25f);
            add (platePath, metalFill (hardware, platePath.getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.4f);
            controlsArea.addPath (platePath);
        }

        const int kind = id.contains ("arched") || id.contains ("semi") || id.contains ("thin") || id == "archtop"
                         || id == "angular" || id.contains ("violin") || id.contains ("hollow") ? 0
                       : id == "single_cutaway_slab" || family == "bass" ? 2
                       : id.contains ("superstrat") ? 3
                                                    : 1;

        for (int i = 0; i < count; ++i)
        {
            const auto c = body (style->controls[i]);
            knob (c, kind);
            controlsArea.addPath (circle (c, 10.0f));
            scene.knobCentres.push_back (c);
        }

        if (style->hasSelector && count > 0)
        {
            const auto c = body (style->selector);
            const bool toggle = kind == 0;

            if (toggle)
            {
                // A toggle: a washer, then the tip leaning toward the neck.
                add (circle (c, 10.0f), solid (plasticColour().getBrightness() > 0.5f ? juce::Colour (0xffece2c8) : juce::Colour (0xff151515)),
                     solid (juce::Colours::black.withAlpha (0.4f)), 0.4f);
                add (circle (c, 3.2f), metalFill (hardware, circle (c, 4.0f).getBounds(), options.materials));
                const auto tip = c + Pointf (5.0f, 0.0f);
                shadow (circle (tip, 3.6f), 1.0f, 0.3f);
                add (circle (tip, 3.6f), solid (juce::Colour (0xffeee4c8)), solid (juce::Colour (0xff8c7a58)), 0.3f);
            }
            else
            {
                // A blade: the slot along the axis and the tip.
                add (rect (c.x - 11.0f, c.y - 1.5f, c.x + 11.0f, c.y + 1.5f, 1.2f), solid (juce::Colour (0xff0e0e0e)));
                const auto tip = rect (c.x + 2.0f, c.y - 3.2f, c.x + 8.0f, c.y + 3.2f, 1.6f);
                shadow (tip, 1.0f, 0.3f);
                add (tip, solid (plasticColour().getBrightness() > 0.5f ? juce::Colour (0xfff2ede0) : juce::Colour (0xff1c1c1c)),
                     solid (juce::Colours::black.withAlpha (0.5f)), 0.3f);
            }

            selectorArea = circle (c, 11.0f);
        }

        // Output jack (or the endpin on an acoustic).
        {
            const auto c = body (style->jack);

            if (c.getDistanceFrom (body ({ 0.5f, 0.0f })) > 1.0f)
            {
                const float r = family == "acoustic" || family == "classical" ? 5.0f : 7.0f;
                add (circle (c, r), metalFill (hardware, circle (c, r).getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.4f);
                add (circle (c, r * 0.45f), solid (juce::Colour (0xff101010)));
                jackArea = circle (c, r + 1.0f);
            }
        }

        // Strap buttons.
        for (auto& sb : style->strapButtons)
        {
            const auto c = body (sb);
            add (circle (c, 4.2f), metalFill (hardware, circle (c, 4.2f).getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.3f);
            add (circle (c, 1.6f), solid (hardware.darker (0.4f)));
        }
    }

    //==========================================================================
    // Layers 10 and 11.
    void SceneBuilder::saddleBlocks (float depth, float width, float stagger)
    {
        juce::Path blocks, screws;

        for (int s = 0; s < numStrings; ++s)
        {
            if (twelve && s % 2 == 1)
                continue;

            const auto p = saddle[(size_t) s];
            const float intonation = juce::isPositiveAndBelow (s, guitar.setup.intonationMm.size())
                                   ? (float) guitar.setup.intonationMm[s] : 0.0f;
            const float x = p.x - (wound[(size_t) s] ? stagger : stagger * 0.3f) - intonation;
            const float y = twelve ? (p.y + saddle[(size_t) s + 1].y) * 0.5f : p.y;
            blocks.addRoundedRectangle (x - depth * 0.5f, y - width * 0.5f, depth, width, 0.8f);

            if (! options.thumbnail && depth > 6.0f)
                screws.addEllipse (x - depth * 0.25f - 0.8f, y - 0.8f, 1.6f, 1.6f);
        }

        add (blocks, metalFill (hardware, blocks.getBounds(), options.materials), solid (hardware.darker (0.55f)), 0.3f);
        if (! screws.isEmpty())
            add (screws, solid (hardware.darker (0.5f)));
    }

    void SceneBuilder::tremPlate (bool twoPoint)
    {
        const float half = bridgeSpan * 0.5f + 9.0f;
        auto plate = rect (-36.0f, -half, 7.0f, half, 3.0f);
        shadow (plate, 1.5f, 0.3f);
        add (plate, metalFill (hardware, plate.getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.4f);
        bridgeArea = plate;

        juce::Path pivots;
        if (twoPoint)
        {
            pivots.addPath (circle ({ 4.0f, -half + 3.0f }, 3.4f));
            pivots.addPath (circle ({ 4.0f, half - 3.0f }, 3.4f));
        }
        else
            for (int i = 0; i < 6; ++i)
                pivots.addPath (circle ({ 4.0f, -half + 4.0f + (float) i * (2.0f * half - 8.0f) / 5.0f }, 1.6f));

        add (pivots, metalFill (hardware.brighter (0.2f), pivots.getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.3f);

        saddleBlocks (11.0f, 7.8f, 3.2f);

        // The arm: from the treble corner, swinging out over the lower bout.
        juce::Path arm;
        arm.startNewSubPath (-24.0f, half - 2.0f);
        arm.cubicTo (-30.0f, half + 18.0f, -60.0f, half + 40.0f, -115.0f, half + 58.0f);
        shadow (strokeOf (arm, 3.0f), 2.5f, 0.3f);
        add (strokeOf (arm, 3.0f), metalFill (hardware, arm.getBounds(), options.materials));
        add (circle ({ -115.0f, half + 58.0f }, 3.4f), solid (plasticColour()));

        for (int s = 0; s < numStrings; ++s)
            tail[(size_t) s] = { -31.0f, saddle[(size_t) s].y };
    }

    void SceneBuilder::bassBridge (bool highMass)
    {
        const float half = bridgeSpan * 0.5f + (highMass ? 13.0f : 10.0f);
        auto plate = rect (-42.0f, -half, 12.0f, half, highMass ? 5.0f : 2.0f);
        shadow (plate, 1.6f, 0.3f);
        add (plate, metalFill (hardware, plate.getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.5f);

        if (highMass && ! options.thumbnail)
        {
            // The cast body's raised walls either side of the saddles.
            juce::Path walls;
            walls.addRoundedRectangle (-38.0f, -half + 1.5f, 46.0f, 5.0f, 2.0f);
            walls.addRoundedRectangle (-38.0f, half - 6.5f, 46.0f, 5.0f, 2.0f);
            add (walls, metalFill (hardware.brighter (0.1f), walls.getBounds(), options.materials));
        }

        saddleBlocks (highMass ? 16.0f : 13.0f, highMass ? 14.0f : 11.0f, 3.5f);
        bridgeArea = plate;

        for (int s = 0; s < numStrings; ++s)
            tail[(size_t) s] = { -36.0f, saddle[(size_t) s].y };
    }

    void SceneBuilder::pinBridge()
    {
        const float half = bridgeSpan * 0.5f + 24.0f;
        const auto wood = GuitarRenderer::woodColour (text (GuitarSlot::fretboard, "wood", "rosewood")).darker (0.1f);

        // A belly bridge: flat front, the back bellied between wings.
        juce::Path block;
        block.startNewSubPath (12.0f, -half + 6.0f);
        block.quadraticTo (12.0f, -half, 6.0f, -half);
        block.lineTo (-14.0f, -half);
        block.quadraticTo (-20.0f, -half, -20.0f, -half + 8.0f);
        block.quadraticTo (-26.0f, 0.0f, -20.0f, half - 8.0f);
        block.quadraticTo (-20.0f, half, -14.0f, half);
        block.lineTo (6.0f, half);
        block.quadraticTo (12.0f, half, 12.0f, half - 6.0f);
        block.closeSubPath();

        shadow (block, 1.4f, 0.3f);
        add (block, solid (wood), solid (wood.darker (0.6f)), 0.4f);
        bridgeArea = block;

        // The saddle, compensated: slanted with the bass end further back.
        auto saddlePath = polygon ({ { 1.8f, -bridgeSpan * 0.5f - 4.0f }, { 4.4f, bridgeSpan * 0.5f + 4.0f },
                                     { 1.6f, bridgeSpan * 0.5f + 4.0f }, { -1.2f, -bridgeSpan * 0.5f - 4.0f } });
        add (saddlePath, solid (juce::Colour (0xfff0e8d4)), solid (juce::Colour (0xff9a8e76)), 0.3f);

        juce::Path pins, dots;

        for (int s = 0; s < numStrings; ++s)
        {
            const float stagger = twelve ? ((s % 2 == 1) ? -4.0f : 0.0f) : ((s % 2 == 1) ? -1.5f : 0.0f);
            const Pointf p { -12.0f + stagger, saddle[(size_t) s].y * 1.05f };
            pins.addPath (circle (p, twelve ? 2.2f : 2.8f));
            dots.addPath (circle (p, 0.9f));
            tail[(size_t) s] = p;
        }

        add (pins, solid (juce::Colour (0xfff2ead8)), solid (juce::Colour (0xff5a4e3a)), 0.3f);
        add (dots, solid (juce::Colour (0xff1a1410)));

        for (int s = 0; s < numStrings; ++s)
            saddle[(size_t) s].x = 1.6f + 2.4f * ((saddle[(size_t) s].y + bridgeSpan * 0.5f) / bridgeSpan);
    }

    void SceneBuilder::tieBlock()
    {
        const float half = bridgeSpan * 0.5f + 32.0f;
        const auto wood = juce::Colour (0xff3e2213);

        juce::Path block;
        block.addRoundedRectangle (-30.0f, -half, 40.0f, 2.0f * half, 3.0f);
        juce::Path wings;
        wings.startNewSubPath (-12.0f, -half);
        wings.quadraticTo (-10.0f, -half - 8.0f, 2.0f, -half - 6.0f);
        wings.lineTo (2.0f, -half);
        wings.closeSubPath();
        wings.startNewSubPath (-12.0f, half);
        wings.quadraticTo (-10.0f, half + 8.0f, 2.0f, half + 6.0f);
        wings.lineTo (2.0f, half);
        wings.closeSubPath();
        block.addPath (wings);

        shadow (block, 1.4f, 0.3f);
        add (block, solid (wood), solid (wood.darker (0.6f)), 0.4f);
        bridgeArea = block;

        // Tie block inlay behind the saddle.
        add (rect (-28.0f, -bridgeSpan * 0.5f - 6.0f, -12.0f, bridgeSpan * 0.5f + 6.0f, 1.0f), solid (juce::Colour (0xff6b3a1c)));

        if (! options.thumbnail)
        {
            juce::Path tiles;
            for (float y = -bridgeSpan * 0.5f - 5.0f; y < bridgeSpan * 0.5f + 5.0f; y += 3.0f)
                tiles.addRectangle (-26.0f, y, 12.0f, 1.2f);
            add (tiles, solid (juce::Colour (0xffd8b56a).withAlpha (0.8f)));
        }

        add (rect (-2.0f, -bridgeSpan * 0.5f - 5.0f, 2.2f, bridgeSpan * 0.5f + 5.0f, 0.8f),
             solid (juce::Colour (0xfff0e8d4)), solid (juce::Colour (0xff9a8e76)), 0.3f);

        // Knots over the tie block.
        juce::Path knots;
        for (int s = 0; s < numStrings; ++s)
        {
            const Pointf p { -20.0f, saddle[(size_t) s].y };
            knots.addEllipse (p.x - 3.0f, p.y - 1.6f, 6.0f, 3.2f);
            tail[(size_t) s] = p;
        }
        add (knots, none(), solid (juce::Colour (0xffe6dccb)), 0.7f);
    }

    void SceneBuilder::floatingBridge (bool moustache)
    {
        const float half = bridgeSpan * 0.5f + 20.0f;
        const auto ebony = juce::Colour (0xff1c1612);

        juce::Path foot;

        if (moustache)
        {
            foot.startNewSubPath (4.0f, -half - 10.0f);
            foot.quadraticTo (-6.0f, -half * 0.5f, 0.0f, 0.0f);
            foot.quadraticTo (-6.0f, half * 0.5f, 4.0f, half + 10.0f);
            foot.lineTo (-2.0f, half + 10.0f);
            foot.quadraticTo (-14.0f, half * 0.5f, -6.0f, 0.0f);
            foot.quadraticTo (-14.0f, -half * 0.5f, -2.0f, -half - 10.0f);
            foot.closeSubPath();
        }
        else
        {
            foot.startNewSubPath (6.0f, -half);
            foot.lineTo (6.0f, half);
            foot.quadraticTo (0.0f, half + 6.0f, -6.0f, half);
            foot.lineTo (-6.0f, -half);
            foot.quadraticTo (0.0f, -half - 6.0f, 6.0f, -half);
            foot.closeSubPath();
        }

        shadow (foot, 1.2f, 0.3f);
        add (foot, solid (ebony), solid (juce::Colours::black), 0.3f);
        add (rect (-1.4f, -bridgeSpan * 0.5f - 6.0f, 1.4f, bridgeSpan * 0.5f + 6.0f, 1.0f), solid (ebony.brighter (0.25f)));

        if (! moustache && ! options.thumbnail)
        {
            // Height thumbwheels.
            juce::Path wheels;
            wheels.addPath (circle ({ 0.0f, -half + 5.0f }, 3.0f));
            wheels.addPath (circle ({ 0.0f, half - 5.0f }, 3.0f));
            add (wheels, metalFill (hardware, wheels.getBounds(), options.materials));
        }

        bridgeArea = foot;
    }

    void SceneBuilder::buildBridge()
    {
        tail.assign ((size_t) numStrings, {});
        for (int s = 0; s < numStrings; ++s)
            tail[(size_t) s] = saddle[(size_t) s] + Pointf (-20.0f, 0.0f);

        const auto type = text (GuitarSlot::bridge, "type", family == "acoustic" ? "pin_bridge"
                                                         : family == "classical" ? "tie_block"
                                                         : isBass ? "vintage_bass" : "hardtail");
        const auto name = partName (GuitarSlot::bridge);
        const juce::String id (style->id);

        if (isBass || name.containsIgnoreCase ("bass"))
        {
            bassBridge (name.containsIgnoreCase ("badass") || name.containsIgnoreCase ("high"));
        }
        else if (type == "tune_o_matic" || type == "bigsby")
        {
            const float half = bridgeSpan * 0.5f + 11.0f;
            auto bar = rect (-5.0f, -half, 5.0f, half, 4.5f);
            shadow (bar, 1.2f, 0.3f);
            add (bar, metalFill (hardware, bar.getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.4f);

            juce::Path posts;
            posts.addPath (circle ({ 0.0f, -half + 3.0f }, 3.6f));
            posts.addPath (circle ({ 0.0f, half - 3.0f }, 3.6f));
            add (posts, metalFill (hardware.brighter (0.15f), posts.getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.3f);

            saddleBlocks (4.2f, 5.5f, 1.8f);
            bridgeArea = bar;

            for (int s = 0; s < numStrings; ++s)
                tail[(size_t) s] = { -60.0f, saddle[(size_t) s].y * 1.02f };
        }
        else if (type == "vintage_tremolo" || type == "two_point_tremolo")
        {
            tremPlate (type == "two_point_tremolo");
        }
        else if (type == "floyd_rose")
        {
            const float half = bridgeSpan * 0.5f + 10.0f;
            auto base = rect (-52.0f, -half, 9.0f, half, 3.0f);
            shadow (base, 1.6f, 0.35f);
            add (base, metalFill (hardware.darker (0.05f), base.getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.4f);
            saddleBlocks (14.0f, 7.4f, 0.0f);

            juce::Path fine;
            for (int s = 0; s < numStrings; ++s)
                fine.addPath (circle ({ -45.0f, saddle[(size_t) s].y }, 2.6f));
            add (fine, metalFill (hardware.brighter (0.2f), fine.getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.3f);

            juce::Path arm;
            arm.startNewSubPath (-22.0f, half - 2.0f);
            arm.cubicTo (-28.0f, half + 22.0f, -70.0f, half + 42.0f, -125.0f, half + 55.0f);
            add (strokeOf (arm, 3.2f), metalFill (hardware, arm.getBounds(), options.materials));
            add (circle ({ -125.0f, half + 55.0f }, 3.6f), solid (juce::Colour (0xff151515)));

            bridgeArea = base;
            for (int s = 0; s < numStrings; ++s)
                tail[(size_t) s] = { -40.0f, saddle[(size_t) s].y };
        }
        else if (type == "wraparound")
        {
            const float half = bridgeSpan * 0.5f + 12.0f;
            auto bar = rect (-7.0f, -half, 7.0f, half, 6.0f);
            shadow (bar, 1.4f, 0.3f);
            add (bar, metalFill (hardware, bar.getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.4f);
            bridgeArea = bar;

            for (int s = 0; s < numStrings; ++s)
                tail[(size_t) s] = { -7.0f, saddle[(size_t) s].y };
        }
        else if (type == "pin_bridge")
        {
            pinBridge();
        }
        else if (type == "tie_block")
        {
            tieBlock();
        }
        else if (type == "floating")
        {
            floatingBridge (id.startsWith ("gypsy"));
        }
        else if (type == "resonator_spider" || type == "resonator_biscuit" || family == "resonator")
        {
            // The biscuit saddle sits at the centre of the cover plate.
            auto biscuit = rect (-4.0f, -bridgeSpan * 0.5f - 8.0f, 4.0f, bridgeSpan * 0.5f + 8.0f, 2.0f);
            add (biscuit, solid (juce::Colour (0xff4a2c1a)), solid (juce::Colours::black), 0.3f);
            bridgeArea = biscuit;

            const float tailX = bodyPath.getBounds().getX();
            auto tp = polygon ({ { -40.0f, -bridgeSpan * 0.5f - 6.0f }, { tailX + 18.0f, -12.0f },
                                 { tailX + 18.0f, 12.0f }, { -40.0f, bridgeSpan * 0.5f + 6.0f } });
            shadow (tp, 1.4f, 0.3f);
            add (tp, metalFill (hardware, tp.getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.4f);
            tailpieceArea = tp;

            for (int s = 0; s < numStrings; ++s)
                tail[(size_t) s] = { -40.0f, saddle[(size_t) s].y * 1.1f };
        }
        else
        {
            // Hardtail: a plate, with per-string saddles (fanned: each on its own saddle line).
            const bool teleAshtray = id == "single_cutaway_slab";
            const float half = bridgeSpan * 0.5f + (teleAshtray ? 16.0f : 9.0f);
            float front = 8.0f;

            if (teleAshtray && guitar.get (GuitarSlot::pickupBridge) != nullptr)
                front = (float) guitar.placements[2].positionMm + 24.0f;

            float back = -30.0f, top = -half, bottom = half;

            if (fanned)
            {
                back = juce::jmin (saddle.front().x, saddle.back().x) - 18.0f;
                front = juce::jmax (saddle.front().x, saddle.back().x) + 8.0f;
            }

            auto plate = rect (back, top, front, bottom, teleAshtray ? 2.0f : 3.5f);
            shadow (plate, 1.5f, 0.3f);
            add (plate, metalFill (hardware, plate.getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.4f);

            if (teleAshtray)
            {
                // Three barrel saddles, each carrying two strings.
                juce::Path barrels;
                for (int pair = 0; pair < 3; ++pair)
                {
                    const int s0 = juce::jmin (numStrings - 1, pair * 2), s1 = juce::jmin (numStrings - 1, pair * 2 + 1);
                    const float y0 = saddle[(size_t) s0].y, y1 = saddle[(size_t) s1].y;
                    const float x = -2.0f - (float) pair * 1.5f;
                    barrels.addRoundedRectangle (x - 3.5f, juce::jmin (y0, y1) - 4.0f, 7.0f, std::abs (y1 - y0) + 8.0f, 3.0f);
                }
                add (barrels, metalFill (hardware.brighter (0.1f), barrels.getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.3f);
            }
            else
                saddleBlocks (fanned ? 9.0f : 11.0f, 7.0f, fanned ? 0.0f : 2.5f);

            bridgeArea = plate;

            for (int s = 0; s < numStrings; ++s)
                tail[(size_t) s] = { saddle[(size_t) s].x - 24.0f, saddle[(size_t) s].y };
        }
    }

    void SceneBuilder::buildTailpiece()
    {
        auto tp = part (GuitarSlot::tailpiece);
        const auto bridgeType = text (GuitarSlot::bridge, "type");

        if (bridgeType == "bigsby")
        {
            // A wide plate over the lower bout and a roller bar, with the arm rising to the treble side.
            const float tailX = bodyPath.getBounds().getX();
            const float half = bridgeSpan * 0.5f + 22.0f;
            auto plate = polygon ({ { -55.0f, -half }, { tailX + 6.0f, -half * 0.55f }, { tailX + 6.0f, half * 0.55f }, { -55.0f, half } });
            shadow (plate, 1.8f, 0.35f);
            add (plate, metalFill (hardware, plate.getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.4f);
            add (rect (-78.0f, -half + 6.0f, -64.0f, half - 6.0f, 6.0f), metalFill (hardware.brighter (0.1f), plate.getBounds(), options.materials));

            juce::Path arm;
            arm.startNewSubPath (-70.0f, half - 4.0f);
            arm.cubicTo (-40.0f, half + 10.0f, 10.0f, half + 22.0f, 40.0f, half + 18.0f);
            add (strokeOf (arm, 5.0f), metalFill (hardware, arm.getBounds(), options.materials));
            tailpieceArea = plate;

            for (int s = 0; s < numStrings; ++s)
                tail[(size_t) s] = { -71.0f, saddle[(size_t) s].y * 0.95f };
            return;
        }

        if (tp == nullptr)
            return;

        const auto type = tp->text ("type", "stopbar");

        if (type == "stopbar")
        {
            const float half = bridgeSpan * 0.5f + 13.0f;
            auto bar = rect (-67.0f, -half, -54.0f, half, 6.5f);
            shadow (bar, 1.4f, 0.3f);
            add (bar, metalFill (hardware, bar.getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.4f);

            juce::Path studs;
            studs.addPath (circle ({ -60.5f, -half + 4.0f }, 4.2f));
            studs.addPath (circle ({ -60.5f, half - 4.0f }, 4.2f));
            add (studs, metalFill (hardware.brighter (0.15f), studs.getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.3f);
            tailpieceArea = bar;

            for (int s = 0; s < numStrings; ++s)
                tail[(size_t) s] = { -58.0f, saddle[(size_t) s].y * 0.95f };
        }
        else
        {
            // Trapeze (or a vibrola plate): hinged at the tail, fanning out to the strings.
            const float tailX = bodyPath.getBounds().getX();
            const float front = juce::jmin (-45.0f, tailX + 130.0f);
            const float half = bridgeSpan * 0.5f + 8.0f;

            auto trapeze = polygon ({ { front, -half }, { tailX + 12.0f, -11.0f }, { tailX + 2.0f, -11.0f },
                                      { tailX + 2.0f, 11.0f }, { tailX + 12.0f, 11.0f }, { front, half } });
            shadow (trapeze, 1.8f, 0.35f);
            add (trapeze, metalFill (hardware, trapeze.getBounds(), options.materials), solid (hardware.darker (0.5f)), 0.5f);

            if (! options.thumbnail)
            {
                auto insert = polygon ({ { front - 6.0f, -half + 7.0f }, { tailX + 40.0f, -6.0f }, { tailX + 40.0f, 6.0f }, { front - 6.0f, half - 7.0f } });
                add (insert, solid (juce::Colour (0xff1a1410).withAlpha (0.85f)));
            }

            tailpieceArea = trapeze;

            for (int s = 0; s < numStrings; ++s)
                tail[(size_t) s] = { front - 2.0f, saddle[(size_t) s].y * 0.9f };
        }
    }

    //==========================================================================
    // Layers 12 and 13.
    void SceneBuilder::pickup (int index, PartPtr p)
    {
        const auto fam = p->text ("family", "single_coil");

        if (fam == "piezo")
            return;   // under the saddle: shown by the endpin jack only (section 8)

        const float x = (float) guitar.placements[(size_t) index].positionMm;
        const float span = spanAt (x);
        const juce::String id (style->id);
        const auto cover = p->text ("cover", "none");
        const bool metalCover = cover == "nickel" || cover == "chrome" || cover == "gold";

        float depth = 18.0f, length = 70.0f, corner = 5.0f;

        if (fam == "humbucker")            { depth = 39.0f; length = isBass ? 100.0f : 70.0f; corner = 2.5f; }
        else if (fam == "p90")             { depth = 32.0f; length = 82.0f; corner = 6.0f; }
        else if (fam == "mini_humbucker")  { depth = 30.0f; length = 66.0f; corner = 2.5f; }
        else if (fam == "active")          { depth = 38.0f; length = isBass ? 92.0f : 70.0f; corner = 3.0f; }
        else if (fam == "soundhole")       { depth = 20.0f; length = juce::jmax (80.0f, span + 26.0f); corner = 4.0f; }
        else if (isBass && fam == "single_coil") { depth = 20.0f; length = 90.0f; corner = 9.0f; }

        length = juce::jmax (length, span + (isBass ? 22.0f : 16.0f));

        // A Strat or Tele bridge single-coil leans, treble end toward the bridge.
        float angle = 0.0f;
        if (index == 2 && fam == "single_coil" && ! isBass && (id == "double_cutaway_offset" || id == "single_cutaway_slab" || id == "superstrat"))
            angle = 0.17f;

        const auto t = juce::AffineTransform::rotation (angle, x, 0.0f);

        auto outline = [&] (float d, float l, float c)
        {
            juce::Path r;
            r.addRoundedRectangle (x - d * 0.5f, -l * 0.5f, d, l, c);
            r.applyTransform (t);
            return r;
        };

        juce::Path area;
        juce::Path poles;

        auto polesAt = [&] (float px, float radius, int perString)
        {
            for (int s = 0; s < numCourses; ++s)
            {
                const int si = twelve ? s * 2 : s;
                const float y = saddle[(size_t) si].y + (nut[(size_t) si].y - saddle[(size_t) si].y) * (x / scale);

                for (int k = 0; k < perString; ++k)
                {
                    const float dy = perString == 2 ? (k == 0 ? -2.6f : 2.6f) : 0.0f;
                    poles.addPath (circle ({ px, y + dy }, radius));
                }
            }
        };

        if (fam == "split_coil")
        {
            // Two staggered halves: the bass pair nearer the neck.
            juce::Path bassHalf, trebleHalf;
            bassHalf.addRoundedRectangle (x + 2.0f, -span * 0.5f - 10.0f, 22.0f, span * 0.5f + 10.0f, 5.0f);
            trebleHalf.addRoundedRectangle (x - 24.0f, 0.0f, 22.0f, span * 0.5f + 10.0f, 5.0f);
            area.addPath (bassHalf);
            area.addPath (trebleHalf);
            shadow (area, 1.2f, 0.3f);
            add (area, solid (juce::Colour (0xff141414)), solid (juce::Colours::black), 0.4f);

            for (int s = 0; s < numCourses; ++s)
            {
                const float y = saddle[(size_t) s].y + (nut[(size_t) s].y - saddle[(size_t) s].y) * (x / scale);
                const float px = y < 0.0f ? x + 13.0f : x - 13.0f;
                poles.addPath (circle ({ px, y - 2.5f }, 2.0f));
                poles.addPath (circle ({ px, y + 2.5f }, 2.0f));
            }

            add (poles, metalFill (hardware, poles.getBounds(), options.materials));
            scene.pickupAreas[(size_t) index] = area;
            return;
        }

        // Mounting ring under humbuckers not sitting on a pickguard.
        const bool onGuard = ! pickguardArea.isEmpty() && pickguardArea.contains (x, 0.0f);

        if ((fam == "humbucker" || fam == "mini_humbucker") && ! onGuard && ! isBass)
        {
            auto ring = outline (depth + 9.0f, length + 12.0f, 4.0f);
            const bool creamRing = partName (GuitarSlot::pickguard).containsIgnoreCase ("cream") || id.contains ("arched");
            add (ring, solid (creamRing ? juce::Colour (0xffe9dcbc) : juce::Colour (0xff141414)), solid (juce::Colours::black.withAlpha (0.5f)), 0.4f);

            if (! options.thumbnail)
            {
                juce::Path screws;
                const auto rb = ring.getBounds();
                for (auto c : { rb.getTopLeft(), rb.getTopRight(), rb.getBottomLeft(), rb.getBottomRight() })
                    screws.addPath (circle (c + (rb.getCentre() - c) * (3.0f / juce::jmax (3.0f, c.getDistanceFrom (rb.getCentre()))), 1.2f));
                add (screws, solid (hardware.darker (0.2f)));
            }
        }

        area = outline (depth, length, corner);
        shadow (area, 1.3f, 0.32f);

        if (fam == "humbucker" && ! metalCover)
        {
            // Open humbucker: two bobbins, one with screws and one with slugs.
            auto b1 = outline (depth * 0.48f, length, corner);
            b1.applyTransform (juce::AffineTransform::translation (depth * 0.26f, 0.0f).followedBy (juce::AffineTransform()));
            auto b2 = outline (depth * 0.48f, length, corner);
            b2.applyTransform (juce::AffineTransform::translation (-depth * 0.26f, 0.0f));
            add (area, solid (juce::Colour (0xff0e0e0e)));
            add (b1, solid (juce::Colour (0xff1a1a1a)), solid (juce::Colours::black), 0.3f);
            add (b2, solid (juce::Colour (0xff1a1a1a)), solid (juce::Colours::black), 0.3f);
            polesAt (x + depth * 0.26f, 2.0f, 1);
            polesAt (x - depth * 0.26f, 2.2f, 1);
            poles.applyTransform (t);
            add (poles, metalFill (hardware, poles.getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.2f);
        }
        else if (metalCover || fam == "mini_humbucker" && cover != "none")
        {
            const auto metal = cover == "gold" ? GuitarRenderer::hardwareColour ("gold") : hardware;
            add (area, metalFill (metal, area.getBounds(), options.materials), solid (metal.darker (0.55f)), 0.4f);

            if (fam == "single_coil" || fam == "mini_humbucker")
            {
                // A Tele neck-style cover: no poles; a highlight shows the dome.
                if (options.materials)
                    add (outline (depth * 0.4f, length * 0.9f, corner), solid (juce::Colours::white.withAlpha (0.18f)), none(), 0.0f, -1, true);
            }
        }
        else
        {
            // Plastic or open covers: colour from the guitar, poles for singles and bass pickups.
            juce::Colour c = fam == "active" || isBass || fam == "p90" || cover == "none" ? juce::Colour (0xff151515) : plasticColour();

            if (fam == "p90" && plasticColour().getBrightness() > 0.5f && id.contains ("arched"))
                c = juce::Colour (0xffece2c8);

            add (area, solid (c), solid (juce::Colours::black.withAlpha (0.6f)), 0.4f);

            if (options.materials)
                add (outline (depth * 0.5f, length * 0.92f, corner), solid (juce::Colours::white.withAlpha (0.06f)), none(), 0.0f, -1, true);

            if (fam == "single_coil" || (isBass && fam != "active"))
            {
                polesAt (x, isBass ? 2.4f : 2.2f, isBass && fam == "humbucker" ? 2 : (isBass ? 2 : 1));
                poles.applyTransform (t);
                add (poles, metalFill (hardware.brighter (0.1f), poles.getBounds(), options.materials));
            }
            else if (fam == "active" && ! options.thumbnail)
            {
                // A small brushed badge rather than a brand.
                auto badge = outline (depth * 0.3f, length * 0.28f, 1.0f);
                add (badge, solid (juce::Colour (0xff8a8e94)));
            }
        }

        // Mounting screws at each end.
        if (! options.thumbnail && fam != "soundhole")
        {
            juce::Path screws;
            screws.addPath (circle ({ x, -length * 0.5f - 3.5f }, 1.3f));
            screws.addPath (circle ({ x, length * 0.5f + 3.5f }, 1.3f));
            screws.applyTransform (t);
            add (screws, metalFill (hardware, screws.getBounds(), options.materials));
        }

        scene.pickupAreas[(size_t) index] = area;
    }

    void SceneBuilder::buildPickups()
    {
        for (int i = 0; i < 3; ++i)
            if (auto p = guitar.get (WorkshopGuitar::pickupSlot (i)))
                pickup (i, p);
    }

    //==========================================================================
    // Layers 15-18: neck, fretboard, inlays, frets, nut.
    juce::Path SceneBuilder::fretboardOutline (float margin, float extraAtEnd, float endFret) const
    {
        const int bass = numStrings - 1, treble = 0;
        const auto& scn = scene;

        auto edgePoint = [&] (int s, float fret, float side, float m)
        {
            const auto p = scn.stringAt (s, fret);
            return Pointf (p.x, p.y + side * m);
        };

        const float endMargin = margin + 1.6f;
        auto nb = edgePoint (bass, 0.0f, -1.0f, margin), nt = edgePoint (treble, 0.0f, 1.0f, margin);
        auto eb = edgePoint (bass, endFret, -1.0f, endMargin), et = edgePoint (treble, endFret, 1.0f, endMargin);
        eb.x -= extraAtEnd;
        et.x -= extraAtEnd;

        return polygon ({ nb, eb, et, nt });
    }

    void SceneBuilder::buildNeck()
    {
        const auto neckPart = part (GuitarSlot::neck);
        const auto joint = text (GuitarSlot::neck, "joint", family == "acoustic" || family == "classical" ? "set" : "bolt");
        const auto neckWoodName = text (GuitarSlot::neck, "wood", "maple_hard");
        const auto fbWoodName = text (GuitarSlot::fretboard, "wood", "rosewood");
        const juce::String id (style->id);
        const auto& f = guitar.finish;

        // The neck's colour: a set neck wears the body's finish edge colour, a bolt-on its own lacquered wood.
        auto neckColour = GuitarRenderer::woodColour (neckWoodName);
        if (neckWoodName.startsWith ("maple"))
            neckColour = juce::Colour (0xffe3bd7f);
        if (joint != "bolt" && (f.type == "solid" || f.type == "burst"))
            neckColour = parseHex (f.colourA, neckColour);

        auto fbColour = GuitarRenderer::woodColour (fbWoodName);
        if (fbWoodName.startsWith ("maple"))
            fbColour = juce::Colour (0xffe9c88c);

        const float margin = isBass ? 4.0f : 3.3f;
        const float endFret = (float) numFrets + 0.45f;

        // Neck wood: slightly wider than the board, from the nut into the body to the heel.
        auto neckPath = fretboardOutline (margin + 0.9f, 0.0f, endFret);
        {
            const auto bb = neckPath.getBounds();

            // The heel runs into the body to the style's neck pocket, but never
            // under a pickup: the routes start where the pocket ends.
            float heelX = body ({ style->neckU, 0.0f }).x;

            for (auto& area : scene.pickupAreas)
                if (! area.isEmpty() && area.getBounds().getRight() < bb.getX() + 40.0f)
                    heelX = juce::jmax (heelX, area.getBounds().getRight() + 4.0f);

            const float endX = juce::jmin (bb.getX(), heelX);

            if (endX < bb.getX())
            {
                const float w = spanAt (endX) * 0.5f + margin + 2.2f;
                neckPath = neckPath.createPathWithRoundedCorners (0.0f);
                juce::Path heel;
                heel.addRoundedRectangle (endX, -w, bb.getX() - endX + 4.0f, 2.0f * w, joint == "bolt" ? 2.0f : 6.0f);
                neckPath.addPath (heel);
            }
        }

        shadow (neckPath, 2.0f, 0.3f);
        add (neckPath, solid (neckColour), solid (neckColour.darker (0.55f)), 0.4f);
        neckArea = neckPath;

        // Bolt-on: the pocket seam where the heel meets the body; neck-through: the laminate stripes.
        if (joint == "bolt" && ! options.thumbnail)
            add (neckPath, none(), solid (juce::Colours::black.withAlpha (0.55f)), 0.5f, bodyClip);
        else if (joint == "through" && f.type != "solid" && ! options.thumbnail)
        {
            const float heelX = body ({ style->neckU, 0.0f }).x;
            juce::Path stripes;
            const float w = spanAt (heelX) * 0.5f + margin + 2.0f;
            const auto bb = bodyPath.getBounds();
            stripes.addRectangle (bb.getX(), -w, heelX - bb.getX(), 0.5f);
            stripes.addRectangle (bb.getX(), w - 0.5f, heelX - bb.getX(), 0.5f);
            add (stripes, solid (juce::Colours::black.withAlpha (0.35f)), none(), 0.0f, bodyClip);
        }

        // The fretboard, with the radius as centre-line shading.
        fretboardArea = fretboardOutline (margin, 0.0f, endFret);
        const auto fbb = fretboardArea.getBounds();
        add (fretboardArea, solid (fbColour));

        if (options.materials)
        {
            const double radius = number (GuitarSlot::fretboard, "radius_mm", 305.0);
            const float strength = radius > 2000.0 ? 0.0f : radius < 260.0 ? 0.16f : radius < 350.0 ? 0.10f : 0.06f;

            if (strength > 0.0f)
            {
                juce::ColourGradient crown (juce::Colours::black.withAlpha (strength), fbb.getX(), fbb.getY(),
                                            juce::Colours::black.withAlpha (strength), fbb.getX(), fbb.getBottom(), false);
                crown.addColour (0.5, juce::Colours::white.withAlpha (strength * 0.6f));
                add (fretboardArea, juce::FillType (crown), none(), 0.0f, -1, true);
            }
        }

        // Binding.
        const bool bound = partName (GuitarSlot::fretboard).containsIgnoreCase ("bound")
                        || id.contains ("arched") || id == "archtop" || id.contains ("semi") || family == "acoustic" && twelve;
        if (bound)
            add (fretboardArea, none(), solid (juce::Colour (0xffece2c8)), 1.6f);

        if (! fretless)
        {
            // Layer 16: inlays.
            const bool darkBoard = fbColour.getBrightness() < 0.5f;
            const auto inlayColour = darkBoard ? juce::Colour (0xfff0ebe0) : juce::Colour (0xff1a1a1a);
            const juce::String inlay = family == "classical" ? "none"
                                     : id.contains ("arched") ? "trapezoid"
                                     : id == "archtop" ? "block"
                                     : id.contains ("superstrat") && numStrings >= 7 ? "sharktooth"
                                                                                    : "dot";

            juce::Path inlays;
            const int bassS = numStrings - 1;

            for (int fret : { 3, 5, 7, 9, 12, 15, 17, 19, 21, 24 })
            {
                if (fret > numFrets || inlay == "none")
                    continue;

                const auto pb = scene.stringAt (bassS, (float) fret - 0.5f), pt = scene.stringAt (0, (float) fret - 0.5f);
                const auto c = (pb + pt) * 0.5f;
                const float between = scene.stringAt (bassS, (float) fret - 1.0f).x - scene.stringAt (bassS, (float) fret).x;
                const float width = std::abs (pt.y - pb.y) + 2.0f * margin;

                if (inlay == "dot")
                {
                    const float r = isBass ? 4.0f : 3.0f;
                    if (fret == 12 || fret == 24)
                    {
                        inlays.addPath (circle ({ c.x, c.y - width * 0.25f }, r));
                        inlays.addPath (circle ({ c.x, c.y + width * 0.25f }, r));
                    }
                    else
                        inlays.addPath (circle (c, r));
                }
                else if (inlay == "trapezoid")
                {
                    const float d = between * 0.55f, h = width * 0.36f;
                    inlays.addPath (polygon ({ { c.x + d * 0.5f, c.y - h * 0.8f }, { c.x - d * 0.5f, c.y - h },
                                               { c.x - d * 0.5f, c.y + h }, { c.x + d * 0.5f, c.y + h * 0.8f } }));
                }
                else if (inlay == "block")
                {
                    const float d = between * 0.55f, h = width * 0.38f;
                    inlays.addRectangle (c.x - d * 0.5f, c.y - h, d, h * 2.0f);
                }
                else
                {
                    const float d = between * 0.6f, h = width * 0.38f;
                    inlays.addPath (polygon ({ { c.x - d * 0.5f, c.y - h }, { c.x + d * 0.5f, c.y + h }, { c.x - d * 0.5f, c.y + h } }));
                }
            }

            if (! inlays.isEmpty())
                add (inlays, options.materials && darkBoard ? metalFill (inlayColour, inlays.getBounds(), true) : solid (inlayColour));

            // Layer 17: frets.
            const float wire = juce::jlimit (1.4f, 3.0f, (float) number (GuitarSlot::frets, "width_mm", 2.4));
            const auto fretMetal = text (GuitarSlot::frets, "material", "nickel_silver") == "stainless"
                                 ? juce::Colour (0xffd8dce0) : juce::Colour (0xffcbc5b4);
            juce::Path frets;

            for (int fret = 1; fret <= numFrets; ++fret)
            {
                const auto pb = scene.stringAt (bassS, (float) fret), pt = scene.stringAt (0, (float) fret);
                const float mb = margin + 1.6f * ((float) fret / endFret);
                juce::Path line;
                line.startNewSubPath (pb.x, pb.y - mb);
                line.lineTo (pt.x, pt.y + mb);
                frets.addPath (strokeOf (line, options.thumbnail ? wire * 0.6f : wire));
            }

            add (frets, metalFill (fretMetal, fbb, options.materials), solid (fretMetal.darker (0.6f)), options.thumbnail ? 0.0f : 0.25f);
        }

        // Layer 18: the nut, following the nut line (angled on a fanned neck).
        {
            const auto nb = scene.stringAt (numStrings - 1, 0.0f), nt = scene.stringAt (0, 0.0f);
            const float thick = 5.0f;
            nutArea = polygon ({ { nb.x, nb.y - margin - 0.5f }, { nb.x + thick, nb.y - margin - 0.5f },
                                 { nt.x + thick, nt.y + margin + 0.5f }, { nt.x, nt.y + margin + 0.5f } });
            const auto material = text (GuitarSlot::nut, "material", "bone");
            const auto nutColour = material == "graphite" ? juce::Colour (0xff2a2a2a)
                                 : material == "brass" ? juce::Colour (0xffc9a24b)
                                 : juce::Colour (0xfff0e8d6);

            add (nutArea, material == "brass" ? metalFill (nutColour, nutArea.getBounds(), options.materials) : solid (nutColour),
                 solid (nutColour.darker (0.6f)), 0.3f);

            // A locking nut's clamps (Floyd-style bridge).
            if (text (GuitarSlot::bridge, "type") == "floyd_rose")
            {
                juce::Path clamps;
                const float x = (nb.x + nt.x) * 0.5f + thick;
                const float w = std::abs (nt.y - nb.y);
                for (int i = 0; i < 3; ++i)
                    clamps.addRoundedRectangle (x + 1.0f, nb.y + (float) i * w / 3.0f - 1.0f, 9.0f, w / 3.0f - 1.0f, 1.5f);
                shadow (clamps, 1.0f, 0.3f);
                add (clamps, metalFill (hardware.darker (0.1f), clamps.getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.3f);
                nutArea.addPath (clamps);
            }
        }

        juce::ignoreUnused (neckPart);
    }

    //==========================================================================
    // Layers 19, 20 and 25: headstock, tuners, truss rod cover.
    void SceneBuilder::buildHeadstock()
    {
        juce::String headId;

        if (auto n = part (GuitarSlot::neck))
            if (n->illustration.isObject())
                headId = n->illustration.getProperty ("headstock", {}).toString();

        const outlines::HeadstockStyle* head = headId.isNotEmpty() ? outlines::findHeadstockStyle (headId.toRawUTF8()) : nullptr;

        if (head == nullptr)
            head = outlines::findHeadstockStyle (outlines::defaultHeadstockFor (style->id, numStrings));

        if (head == nullptr || head->numOutline < 3)
            return;

        scene.headstockStyle = head->id;

        const auto nb = scene.stringAt (numStrings - 1, 0.0f), nt = scene.stringAt (0, 0.0f);
        const float nutX = juce::jmax (nb.x, nt.x) + 5.0f;
        const float realHalf = nutWidth * 0.5f;
        const float dataHalf = isBass ? 19.0f : 21.5f;

        auto map = [&] (float a, float b)
        {
            const float blend = juce::jlimit (0.0f, 1.0f, a / 40.0f);
            const float ratio = (realHalf / dataHalf) * (1.0f - blend) + blend * juce::jmax (1.0f, realHalf / dataHalf);
            return Pointf (nutX + a, b * ratio);
        };

        std::vector<Pointf> pts;
        std::vector<bool> corners;

        for (int i = 0; i < head->numOutline; ++i)
        {
            pts.push_back (map (head->outline[i].a, head->outline[i].b));
            corners.push_back (cornerOf (head->outline[i], 0));
        }

        headstockArea = smoothPath (pts, corners, true, false);

        // The face: maple on a bolt-on maple neck, dark veneer otherwise.
        const auto joint = text (GuitarSlot::neck, "joint", "set");
        const auto neckWood = text (GuitarSlot::neck, "wood", "maple_hard");
        juce::Colour face = (joint == "bolt" && neckWood.startsWith ("maple")) ? juce::Colour (0xffe3bd7f)
                          : family == "classical" ? juce::Colour (0xff3a2216)
                                                  : juce::Colour (0xff16110e);

        if (neckWood.startsWith ("maple") && joint == "bolt" && guitar.finish.aging > 0.0)
            face = face.interpolatedWith (juce::Colour (0xffc99a55), (float) guitar.finish.aging * 0.6f);

        shadow (headstockArea, 2.0f, 0.3f);
        add (headstockArea, solid (face), solid (face.darker (0.6f)), 0.4f);

        if (options.materials && face.getBrightness() < 0.3f)
        {
            const auto bb = headstockArea.getBounds();
            juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.12f), bb.getRight(), bb.getY(),
                                        juce::Colours::white.withAlpha (0.0f), bb.getCentreX(), bb.getBottom(), false);
            add (headstockArea, juce::FillType (gloss), none(), 0.0f, -1, true);
        }

        // Slots (classical) with rollers across them.
        for (int k = 0; k < 2; ++k)
        {
            if (head->slots[k] == nullptr || head->numSlot[k] < 3)
                continue;

            std::vector<Pointf> sp;
            std::vector<bool> sc;
            for (int i = 0; i < head->numSlot[k]; ++i)
            {
                sp.push_back (map (head->slots[k][i].a, head->slots[k][i].b));
                sc.push_back (true);
            }
            add (smoothPath (sp, sc, true, true), solid (juce::Colour (0xff0c0806)));
        }

        // Truss rod cover: black with a white edge, as on a set-neck.
        if (head->trussCover != nullptr && head->numTrussCover >= 3)
        {
            std::vector<Pointf> tp;
            std::vector<bool> tc;
            for (int i = 0; i < head->numTrussCover; ++i)
            {
                tp.push_back (map (head->trussCover[i].a, head->trussCover[i].b));
                tc.push_back (false);
            }
            const auto cover = smoothPath (tp, tc, true, false);
            add (cover, solid (juce::Colour (0xff111111)), solid (juce::Colour (0xffe9e3d6)), 0.8f);
        }

        // The maker's mark (6): a small "L", 4% opacity.
        if (! options.thumbnail)
        {
            const auto bb = headstockArea.getBounds();
            juce::Path mark;
            const Pointf c { bb.getRight() - bb.getWidth() * 0.35f, bb.getCentreY() };
            mark.startNewSubPath (c.x, c.y - 6.0f);
            mark.lineTo (c.x, c.y + 4.0f);
            mark.lineTo (c.x - 7.0f, c.y + 4.0f);
            add (strokeOf (mark, 1.6f), solid ((face.getBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white).withAlpha (0.04f)));
        }

        // Tuners: shafts and buttons on the face, posts over the strings.
        const bool classicalRollers = head->layout == outlines::HeadLayout::slotted;
        const bool keystone = partName (GuitarSlot::tuners).containsIgnoreCase ("kluson") && joint != "bolt";
        const bool plasticButtons = keystone || classicalRollers;
        const auto buttonColour = classicalRollers ? juce::Colour (0xfff0e6d2) : keystone ? juce::Colour (0xffe8dcbc) : hardware;

        juce::Path buttons, shafts;

        auto postFor = [&] (int s) -> int
        {
            if (twelve)
                return (5 - s / 2) * 2 + (s % 2);
            return numStrings - 1 - s;
        };

        std::vector<Pointf> posts ((size_t) numStrings);

        for (int s = 0; s < numStrings; ++s)
        {
            const int k = postFor (s);

            if (k < head->numPosts)
                posts[(size_t) s] = map (head->posts[k].a, head->posts[k].b);
            else
                posts[(size_t) s] = map (60.0f + 12.0f * (float) s, 0.0f);

            if (k < head->numButtons && head->buttons != nullptr)
            {
                const auto button = map (head->buttons[k].a, head->buttons[k].b);
                const auto post = posts[(size_t) s];
                const auto dir = (button - post) / juce::jmax (0.001f, button.getDistanceFrom (post));
                const float len = head->buttonLengthMm, wid = head->buttonWidthMm;

                if (! classicalRollers)
                {
                    juce::Path shaft;
                    shaft.startNewSubPath (post);
                    shaft.lineTo (button - dir * (len * 0.5f));
                    shafts.addPath (strokeOf (shaft, 3.2f));
                }

                juce::Path b;
                if (keystone)
                    b = polygon ({ button - dir * (len * 0.5f) + Pointf (-dir.y, dir.x) * (wid * 0.3f),
                                   button + dir * (len * 0.5f) + Pointf (-dir.y, dir.x) * (wid * 0.5f),
                                   button + dir * (len * 0.5f) - Pointf (-dir.y, dir.x) * (wid * 0.5f),
                                   button - dir * (len * 0.5f) - Pointf (-dir.y, dir.x) * (wid * 0.3f) });
                else
                {
                    b.addEllipse (-len * 0.5f, -wid * 0.5f, len, wid);
                    b.applyTransform (juce::AffineTransform::rotation (std::atan2 (dir.y, dir.x)).translated (button.x, button.y));
                }

                buttons.addPath (b);
            }
        }

        if (! shafts.isEmpty())
            add (shafts, metalFill (hardware, shafts.getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.25f);

        if (! buttons.isEmpty())
        {
            shadow (buttons, 1.2f, 0.3f);
            add (buttons, plasticButtons ? solid (buttonColour) : metalFill (buttonColour, buttons.getBounds(), options.materials),
                 solid (buttonColour.darker (0.55f)), 0.35f);
        }

        tunerArea = buttons;

        // Posts and bushings go over the strings.
        tuner.clear();

        for (int s = 0; s < numStrings; ++s)
        {
            const auto p = posts[(size_t) s];

            if (classicalRollers)
            {
                juce::Path roller;
                roller.addRoundedRectangle (p.x - 3.2f, p.y - 9.0f, 6.4f, 18.0f, 2.5f);
                tuner.addPath (roller);
            }
            else
            {
                const float r = isBass ? 5.0f : 3.8f;
                addOver (circle (p, r + 2.0f), metalFill (hardware.darker (0.1f), circle (p, r + 2.0f).getBounds(), options.materials),
                         solid (hardware.darker (0.6f)), 0.3f);
                addOver (circle (p, r), metalFill (hardware.brighter (0.1f), circle (p, r).getBounds(), options.materials));
                addOver (circle (p, r * 0.3f), solid (hardware.darker (0.6f)));
                tuner.addPath (circle (p, r + 2.0f));
            }
        }

        if (classicalRollers)
            add (tuner, solid (juce::Colour (0xfff0e6d2)), solid (juce::Colour (0xff8a7c64)), 0.3f);

        tunerArea.addPath (tuner);
        postPoints = posts;

        // String tree.
        if (head->hasStringTree)
        {
            const auto c = map (head->stringTree.a, head->stringTree.b);
            auto tree = rect (c.x - 3.0f, c.y - 7.0f, c.x + 3.0f, c.y + 7.0f, 2.5f);
            treePath = tree;
            treeCentre = c;
            hasTree = true;
        }
    }

    //==========================================================================
    // Layer 14.
    void SceneBuilder::buildStrings()
    {
        const auto material = text (GuitarSlot::strings, "winding_material", "nickel_plated_steel");
        const bool flat = text (GuitarSlot::strings, "winding", "round") == "flat";
        const bool coated = partName (GuitarSlot::strings).containsIgnoreCase ("coated") || text (GuitarSlot::strings, "winding") == "coated";
        const bool nylon = material == "nylon";

        for (int s = 0; s < numStrings; ++s)
        {
            GuitarScene::StringLine line;
            line.index = s;
            line.tail = tail[(size_t) s];
            line.saddle = saddle[(size_t) s];
            line.nut = nut[(size_t) s];
            line.post = s < (int) postPoints.size() ? postPoints[(size_t) s] : nut[(size_t) s] + Pointf (80.0f, 0.0f);
            line.wound = wound[(size_t) s];
            line.widthMm = gaugeMm[(size_t) s];

            auto colour = GuitarRenderer::stringColour (material, line.wound, isBass, flat);

            if (nylon)
            {
                line.colour = juce::Colour (0xfff2e9d8).withAlpha (line.wound ? 1.0f : 0.92f);
                line.winding = juce::Colour (0xffc9a75a);
                line.dashedWinding = line.wound;
                line.minWidthPx = line.wound ? 2.5f : 2.0f;
            }
            else
            {
                if (coated)
                    colour = colour.withSaturation (colour.getSaturation() * 0.5f);

                line.colour = colour;
                line.winding = material == "silk_steel" ? juce::Colour (0xffb23a3a).withAlpha (0.35f) : colour.darker (0.35f);
                line.dashedWinding = line.wound && ! flat && ! coated && ! material.contains ("tape");
                line.minWidthPx = isBass ? (material.contains ("tape") ? 3.0f : 2.5f)
                                : ! line.wound ? 1.0f
                                : material.contains ("bronze") ? 2.0f : 1.5f;
            }

            scene.strings.push_back (line);
        }

        if (hasTree)
        {
            addOver (treePath, metalFill (hardware, treePath.getBounds(), options.materials), solid (hardware.darker (0.6f)), 0.3f);
        }
    }

    //==========================================================================
    void SceneBuilder::buildHits()
    {
        auto add = [this] (GuitarRegion r, const juce::Path& p, const juce::String& text)
        {
            if (! p.isEmpty())
                scene.hits.push_back ({ r, p, text });
        };

        auto nice = [] (juce::String s) { return s.replaceCharacter ('_', ' ').trim(); };
        const auto& f = guitar.finish;

        // Section 16's strings.
        const auto finishWords = f.type == "burst" ? juce::String ("burst") : nice (f.type);
        add (GuitarRegion::body, bodyPath,
             "Body: " + nice (text (GuitarSlot::body, "wood", "unknown wood")) + ", " + nice (style->id) + ", " + finishWords + ".");
        add (GuitarRegion::pickguard, pickguardArea, "Pickguard: " + partName (GuitarSlot::pickguard) + ".");
        add (GuitarRegion::soundhole, holeArea, style->hole == outlines::Hole::fHoles ? "F-holes." : "Soundhole.");
        add (GuitarRegion::bridge, bridgeArea, "Bridge: " + partName (GuitarSlot::bridge) + ".");
        add (GuitarRegion::tailpiece, tailpieceArea, "Tailpiece: " + (partName (GuitarSlot::tailpiece).isNotEmpty()
                                                                    ? partName (GuitarSlot::tailpiece) : juce::String ("resonator tailpiece")) + ".");

        const char* slotNames[] = { "Neck", "Middle", "Bridge" };
        const GuitarRegion regions[] = { GuitarRegion::pickupNeck, GuitarRegion::pickupMiddle, GuitarRegion::pickupBridge };

        for (int i = 0; i < 3; ++i)
            if (auto p = guitar.get (WorkshopGuitar::pickupSlot (i)))
            {
                const auto& pl = guitar.placements[(size_t) i];
                add (regions[i], scene.pickupAreas[(size_t) i],
                     juce::String (slotNames[i]) + " pickup: " + p->name + "; position " + juce::String (juce::roundToInt (pl.positionMm))
                         + " mm from saddle; height " + juce::String (pl.heightBassMm, 1) + " mm bass / "
                         + juce::String (pl.heightTrebleMm, 1) + " mm treble.");
            }

        add (GuitarRegion::controls, controlsArea, "Controls: " + partName (GuitarSlot::wiring) + ".");
        add (GuitarRegion::selector, selectorArea, "Pickup selector.");
        add (GuitarRegion::jack, jackArea, "Output jack.");
        add (GuitarRegion::neck, neckArea, "Neck: " + nice (text (GuitarSlot::neck, "wood", "")) + " "
                                               + text (GuitarSlot::neck, "joint", "set") + "-neck, "
                                               + text (GuitarSlot::neck, "profile", "C") + " profile.");
        add (GuitarRegion::fretboard, fretboardArea, "Fretboard: " + nice (text (GuitarSlot::fretboard, "wood", "")) + ", "
                                                     + (fretless ? juce::String ("fretless") : juce::String (numFrets) + " frets") + ".");
        add (GuitarRegion::nut, nutArea, "Nut: " + nice (text (GuitarSlot::nut, "material", "bone")) + ", "
                                          + juce::String (juce::roundToInt (nutWidth)) + " mm width.");
        add (GuitarRegion::headstock, headstockArea, "Headstock.");

        // Strings: a thin band along each, so the parts under them stay clickable.
        juce::Path strings;
        for (auto& s : scene.strings)
        {
            juce::Path line;
            line.startNewSubPath (s.saddle);
            line.lineTo (s.nut);
            strings.addPath (strokeOf (line, juce::jmax (2.4f, s.widthMm + 1.6f)));
        }
        add (GuitarRegion::strings, strings, "Strings: " + partName (GuitarSlot::strings) + ".");

        add (GuitarRegion::tuners, tunerArea, "Tuners: " + partName (GuitarSlot::tuners) + ", " + nice (guitar.hardwareColour) + ".");
    }
} // namespace

//==============================================================================
GuitarScene GuitarRenderer::build (const WorkshopGuitar& guitar, Options options)
{
    GuitarScene scene;
    SceneBuilder (guitar, options, scene).run();
    scene.key = keyFor (guitar, options);
    return scene;
}

juce::int64 GuitarRenderer::keyFor (const WorkshopGuitar& guitar, Options options)
{
    auto json = juce::JSON::toString (guitar.toEmbeddedVar(), true);
    json << "|" << (options.materials ? 1 : 0) << (options.thumbnail ? 1 : 0);
    return json.hashCode64();
}

juce::AffineTransform GuitarRenderer::fitTransform (const GuitarScene& scene, juce::Rectangle<float> area)
{
    const auto b = scene.bounds;

    if (b.isEmpty() || area.isEmpty())
        return {};

    // Headstock left: screen x = -X. Bass up: screen y = Y (bass is negative Y).
    const float s = juce::jmin (area.getWidth() / b.getWidth(), area.getHeight() / b.getHeight());
    const float cx = b.getCentreX(), cy = b.getCentreY();

    return juce::AffineTransform (-s, 0.0f, area.getCentreX() + cx * s,
                                  0.0f, s, area.getCentreY() - cy * s);
}

namespace
{
    float scaleOf (const juce::AffineTransform& t)
    {
        return std::sqrt (std::abs (t.getDeterminant()));
    }

    void paintShape (juce::Graphics& g, const GuitarScene& scene, const GuitarScene::Shape& s, float pxPerMm)
    {
        const bool clipped = s.clip >= 0 && s.clip < (int) scene.clips.size();

        if (clipped)
        {
            g.saveState();
            g.reduceClipRegion (scene.clips[(size_t) s.clip]);
        }

        if (! s.fill.isInvisible())
        {
            g.setFillType (s.fill);
            g.fillPath (s.path);
        }

        if (! s.stroke.isInvisible() && (s.strokeMm > 0.0f || s.strokeIsHairline))
        {
            g.setFillType (s.stroke);
            const float w = s.strokeIsHairline ? 1.0f / pxPerMm : juce::jmax (s.strokeMm, 0.6f / pxPerMm);
            g.strokePath (s.path, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (clipped)
            g.restoreState();
    }

    void paintString (juce::Graphics& g, const GuitarScene::StringLine& s, float pxPerMm)
    {
        const float zoom = juce::jlimit (0.3f, 1.0f, pxPerMm / 0.77f);
        const float widthPx = juce::jmax (s.widthMm * pxPerMm, s.minWidthPx * zoom);
        const float w = widthPx / pxPerMm;

        juce::Path line;
        line.startNewSubPath (s.tail);
        line.lineTo (s.saddle);
        line.lineTo (s.nut);
        line.lineTo (s.post);

        const juce::PathStrokeType stroke (w, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);

        // A faint shadow on the wood below, then the string, then its winding.
        if (pxPerMm > 0.5f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            juce::Path shadowLine (line);
            shadowLine.applyTransform (juce::AffineTransform::translation (-w * 0.9f, w * 0.9f));
            g.strokePath (shadowLine, stroke);
        }

        g.setColour (s.colour);
        g.strokePath (line, stroke);

        if (s.dashedWinding && widthPx >= 1.2f)
        {
            juce::Path dashed;
            const float d = juce::jmax (0.35f, 0.5f / pxPerMm);
            const float dashes[] = { d, d };
            juce::PathStrokeType (w * 0.7f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt)
                .createDashedStroke (dashed, line, dashes, 2);
            g.setColour (s.winding);
            g.fillPath (dashed);
        }

        // A highlight along the top of the string.
        if (widthPx >= 1.5f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            juce::Path hl (line);
            hl.applyTransform (juce::AffineTransform::translation (0.0f, -w * 0.2f));
            g.strokePath (hl, juce::PathStrokeType (w * 0.3f));
        }
    }
} // namespace

void GuitarRenderer::paint (juce::Graphics& g, const GuitarScene& scene, const juce::AffineTransform& mmToPx)
{
    const float pxPerMm = scaleOf (mmToPx);

    if (pxPerMm <= 0.0f)
        return;

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (mmToPx);

    for (auto& s : scene.shapes)
        paintShape (g, scene, s, pxPerMm);

    for (auto& s : scene.strings)
        paintString (g, s, pxPerMm);

    for (auto& s : scene.overStrings)
        paintShape (g, scene, s, pxPerMm);
}

void GuitarRenderer::paintOverlay (juce::Graphics& g, const GuitarScene& scene,
                                   const juce::AffineTransform& mmToPx, const GuitarOverlay& overlay)
{
    const float pxPerMm = scaleOf (mmToPx);

    if (pxPerMm <= 0.0f)
        return;

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (mmToPx);

    // 26: played notes - the string glows where it vibrates, a dot where it is stopped.
    for (auto& s : scene.strings)
    {
        const int i = s.index;
        if (! juce::isPositiveAndBelow (i, (int) overlay.stringLevel.size()))
            continue;

        const float level = juce::jlimit (0.0f, 1.0f, overlay.stringLevel[(size_t) i]);

        if (level <= 0.01f)
            continue;

        const float fret = overlay.stringFret[(size_t) i];
        const auto from = fret > 0.0f ? scene.stringAt (i, fret) : s.nut;

        juce::Path vib;
        vib.startNewSubPath (from);
        vib.lineTo (s.saddle);

        const float glow = overlay.reducedMotion ? 2.0f : 2.0f + 3.0f * level;
        g.setColour (overlay.accent.withAlpha (0.35f * level));
        g.strokePath (vib, juce::PathStrokeType (glow / pxPerMm * 2.0f));
        g.setColour (overlay.accent.brighter (0.4f).withAlpha (0.8f * level));
        g.strokePath (vib, juce::PathStrokeType (juce::jmax (s.widthMm, 1.2f / pxPerMm)));

        if (fret > 0.0f)
        {
            const auto p = scene.noteAt (i, juce::roundToInt (fret));
            const float r = 3.0f;
            g.setColour (overlay.accent.withAlpha (0.9f));
            g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        }
    }

    // 28: the slide bar across the strings.
    if (overlay.slideFret >= 0.0f && scene.numStrings > 0)
    {
        const auto a = scene.stringAt (scene.numStrings - 1, overlay.slideFret);
        const auto b = scene.stringAt (0, overlay.slideFret);
        juce::Path bar;
        bar.addRoundedRectangle (juce::jmin (a.x, b.x) - 5.0f, a.y - 8.0f, 10.0f + std::abs (a.x - b.x), (b.y - a.y) + 16.0f, 5.0f);
        g.setColour (juce::Colour (0xffcfe3e8).withAlpha (0.55f));
        g.fillPath (bar);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.strokePath (bar, juce::PathStrokeType (0.8f));
    }

    // 30 / 31: pickup highlight, hover and selection outlines.
    auto outline = [&] (const juce::Path& p, juce::Colour c, float px)
    {
        if (p.isEmpty())
            return;
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (px / pxPerMm));
    };

    if (juce::isPositiveAndBelow (overlay.highlightedPickup, 3))
        outline (scene.pickupAreas[(size_t) overlay.highlightedPickup], overlay.accent, 2.0f);

    for (auto& h : scene.hits)
    {
        if (h.region == overlay.selected && overlay.selected != GuitarRegion::none)
            outline (h.area, overlay.accent, 2.5f);
        else if (h.region == overlay.hovered && overlay.hovered != GuitarRegion::none)
            outline (h.area, overlay.accent.withAlpha (0.6f), 1.5f);
    }
}

const GuitarScene::Hit* GuitarRenderer::hitTest (const GuitarScene& scene, juce::Point<float> mm)
{
    for (auto it = scene.hits.rbegin(); it != scene.hits.rend(); ++it)
        if (it->area.contains (mm))
            return &*it;

    return nullptr;
}

juce::Image GuitarRenderer::render (const WorkshopGuitar& guitar, int width, int height, juce::Colour background, Options options)
{
    // A software image: thumbnails render on a worker thread (section 2.3), and
    // the pixels are read back, which a GPU-backed image does not promise.
    juce::Image image (juce::Image::ARGB, juce::jmax (1, width), juce::jmax (1, height), true, juce::SoftwareImageType());

    {
        juce::Graphics g (image);

        if (! background.isTransparent())
            g.fillAll (background);

        const auto scene = build (guitar, options);
        const float margin = juce::jmax (2.0f, (float) juce::jmin (width, height) * 0.03f);
        paint (g, scene, fitTransform (scene, image.getBounds().toFloat().reduced (margin)));
    }

    return image;
}

} // namespace luthier
