/*  The guitar illustration: guitar-illustration.md 19. */

#include "TestFramework.h"

#include "../UI/Guitar/GuitarRenderer.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    PartLibrary& library()
    {
        static PartLibrary lib = []
        {
            PartLibrary l;
            l.refreshFrom (PartLibrary::getFactoryPartsFolder(),
                           juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-no-user-parts"));
            return l;
        }();

        return lib;
    }

    juce::Array<juce::File> factoryGuitarFiles()
    {
        juce::Array<juce::File> files;

        for (const auto& entry : juce::RangedDirectoryIterator (PartLibrary::getFactoryGuitarsFolder(), true, "*.luthierguitar"))
            files.add (entry.getFile());

        files.sort();
        return files;
    }

    WorkshopGuitar load (const juce::File& file)
    {
        WorkshopGuitar g;
        PartLibrary::LoadReport report;
        library().loadGuitar (file, g, report);
        return g;
    }

    WorkshopGuitar factory (const juce::String& relativePath)
    {
        return load (PartLibrary::getFactoryGuitarsFolder().getChildFile (relativePath));
    }

    /** Where the renders go for a person to look at (guitar-illustration.md 19, visual-polish.md 1). */
    juce::File renderFolder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-guitar-renders");
        dir.createDirectory();
        return dir;
    }

    bool savePng (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat png;
        return out.openedOk() && png.writeImageToStream (image, out);
    }

    int countOpaque (const juce::Image& image, juce::Rectangle<int> area)
    {
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);
        int n = 0;

        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x)
                if (data.getPixelColour (x, y).getAlpha() > 8)
                    ++n;

        return n;
    }
}

//==============================================================================
LUTHIER_TEST (GuitarIllustration, everyFactoryGuitarRendersWithoutClipping)
{
    const auto files = factoryGuitarFiles();
    CHECK (files.size() >= 20);

    for (const auto& file : files)
    {
        const auto guitar = load (file);

        for (int width : { 128, 384, 768, 1536, 3072 })
        {
            const int height = juce::jmax (48, width * 2 / 5);
            const auto image = GuitarRenderer::render (guitar, width, height);

            const int inside = countOpaque (image, image.getBounds().reduced (2));
            CHECK_MSG (inside > width * height / 20, file.getFileNameWithoutExtension() + " at " + juce::String (width)
                                                         + " px drew almost nothing (" + juce::String (inside) + " px)");

            // Nothing touches the image border: the fit keeps the whole guitar in frame.
            const int border = countOpaque (image, { 0, 0, width, 1 }) + countOpaque (image, { 0, height - 1, width, 1 })
                             + countOpaque (image, { 0, 0, 1, height }) + countOpaque (image, { width - 1, 0, 1, height });
            CHECK_MSG (border == 0, file.getFileNameWithoutExtension() + " clips at " + juce::String (width) + " px");
        }
    }
}

LUTHIER_TEST (GuitarIllustration, everyFactoryGuitarHasItsParts)
{
    for (const auto& file : factoryGuitarFiles())
    {
        const auto guitar = load (file);
        const auto scene = GuitarRenderer::build (guitar);
        const auto name = file.getFileNameWithoutExtension();

        CHECK_MSG ((int) scene.strings.size() == guitar.getStringCount(), name + ": string count");
        CHECK_MSG (scene.headstockStyle.isNotEmpty(), name + ": no headstock");

        auto hasRegion = [&] (GuitarRegion r)
        {
            for (auto& h : scene.hits)
                if (h.region == r)
                    return true;
            return false;
        };

        for (auto r : { GuitarRegion::body, GuitarRegion::bridge, GuitarRegion::neck, GuitarRegion::fretboard,
                        GuitarRegion::nut, GuitarRegion::headstock, GuitarRegion::tuners, GuitarRegion::strings })
            CHECK_MSG (hasRegion (r), name + ": no " + getGuitarRegionName (r));

        // Ground rule 5: every fitted, visible pickup is drawn.
        const GuitarRegion pickupRegions[] = { GuitarRegion::pickupNeck, GuitarRegion::pickupMiddle, GuitarRegion::pickupBridge };

        for (int i = 0; i < 3; ++i)
            if (auto p = guitar.get (WorkshopGuitar::pickupSlot (i)); p != nullptr && p->text ("family") != "piezo")
                CHECK_MSG (hasRegion (pickupRegions[i]), name + ": pickup " + juce::String (i) + " not drawn");
    }
}

LUTHIER_TEST (GuitarIllustration, contactSheetForReview)
{
    // Not an assertion: the sheet a person looks at (TODO item G). Written to the temp folder.
    const auto files = factoryGuitarFiles();
    const int cellW = 760, cellH = 300, cols = 3;
    const int rows = (files.size() + cols - 1) / cols;

    juce::Image sheet (juce::Image::ARGB, cellW * cols, cellH * rows, true);

    for (int i = 0; i < files.size(); ++i)
    {
        // A Graphics per cell: on Windows the drawing lands in the image when it is destroyed.
        juce::Graphics g (sheet);
        if (i == 0)
            g.fillAll (juce::Colour (0xff2a1e17));

        const auto guitar = load (files[i]);
        const auto cell = juce::Rectangle<int> ((i % cols) * cellW, (i / cols) * cellH, cellW, cellH);
        const auto scene = GuitarRenderer::build (guitar);
        GuitarRenderer::paint (g, scene, GuitarRenderer::fitTransform (scene, cell.toFloat().reduced (14.0f, 24.0f).withTrimmedTop (4.0f)));
        g.setColour (juce::Colour (0xffefe3cc));
        g.setFont (14.0f);
        g.drawText (guitar.name, cell.reduced (8, 4), juce::Justification::topLeft);

        savePng (GuitarRenderer::render (guitar, 1536, 600, juce::Colour (0xff2a1e17)),
                 renderFolder().getChildFile (files[i].getFileNameWithoutExtension() + ".png"));
    }

    CHECK (savePng (sheet, renderFolder().getChildFile ("_contact_sheet.png")));
}

//==============================================================================
namespace
{
    /** What GuitarBodyComponent shows at a size: the scene built at the detail the
        fit deserves, framed automatically, on the panel's background. */
    juce::Image renderLikeComponent (const WorkshopGuitar& guitar, int width, int height, float displayScale, juce::Colour background)
    {
        juce::Image image (juce::Image::ARGB, juce::roundToInt ((float) width * displayScale),
                           juce::roundToInt ((float) height * displayScale), true, juce::SoftwareImageType());
        juce::Graphics g (image);
        g.fillAll (background);
        g.addTransform (juce::AffineTransform::scale (displayScale));

        GuitarScene scene;
        const auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (4.0f);
        const auto t = GuitarRenderer::buildFitted (guitar, area, displayScale, true, scene);
        GuitarRenderer::paint (g, scene, t);
        return image;
    }
}

LUTHIER_TEST (GuitarIllustration, detailFollowsPixelsPerMillimetre)
{
    using Detail = GuitarRenderer::Detail;

    // A preset thumbnail (128 px for a metre of guitar), the Advanced strip, Easy mode.
    CHECK (GuitarRenderer::detailFor (0.12f) == Detail::thumbnail);
    CHECK (GuitarRenderer::detailFor (0.24f) == Detail::thumbnail);
    CHECK (GuitarRenderer::detailFor (0.3f) == Detail::reduced);
    CHECK (GuitarRenderer::detailFor (0.59f) == Detail::reduced);
    CHECK (GuitarRenderer::detailFor (0.85f) == Detail::full);

    // Less is drawn at each step down, and the key tells the levels apart.
    const auto guitar = factory ("Electric/Vintage Single-Cut.luthierguitar");
    GuitarRenderer::Options full, reduced, thumbnail;
    reduced.detail = Detail::reduced;
    thumbnail.detail = Detail::thumbnail;

    const auto sFull = GuitarRenderer::build (guitar, full);
    const auto sReduced = GuitarRenderer::build (guitar, reduced);
    const auto sThumb = GuitarRenderer::build (guitar, thumbnail);

    CHECK_MSG (sReduced.shapes.size() < sFull.shapes.size(),
               "reduced drew " + juce::String ((int) sReduced.shapes.size()) + " shapes, full " + juce::String ((int) sFull.shapes.size()));
    CHECK_MSG (sThumb.shapes.size() < sReduced.shapes.size(),
               "thumbnail drew " + juce::String ((int) sThumb.shapes.size()) + " shapes, reduced " + juce::String ((int) sReduced.shapes.size()));
    CHECK (sFull.key != sReduced.key && sReduced.key != sThumb.key);
    CHECK (sFull.detail == (int) Detail::full && sReduced.detail == (int) Detail::reduced);

    // Nothing blurred at paint time in a thumbnail; a soft shadow in the others.
    int softFull = 0, softThumb = 0;
    for (auto& sh : sFull.shapes)   softFull += sh.shadowMm > 0.0f ? 1 : 0;
    for (auto& sh : sThumb.shapes)  softThumb += sh.shadowMm > 0.0f ? 1 : 0;
    CHECK (softFull > 0 && softThumb == 0);

    // The fitted build picks the level from the size: the strip is reduced, Easy is full.
    GuitarScene strip, easy;
    const auto tStrip = GuitarRenderer::buildFitted (guitar, { 0.0f, 0.0f, 252.0f, 116.0f }, 1.0f, true, strip);
    const auto tEasy = GuitarRenderer::buildFitted (guitar, { 0.0f, 0.0f, 880.0f, 380.0f }, 1.0f, true, easy);
    CHECK_MSG (strip.detail == (int) GuitarRenderer::detailFor (GuitarRenderer::pxPerMm (tStrip)), "strip detail does not match its fit");
    CHECK_MSG (easy.detail == (int) Detail::full, "Easy mode's guitar lost detail");
    CHECK (GuitarRenderer::pxPerMm (tEasy) > GuitarRenderer::pxPerMm (tStrip));

    // A HiDPI strip has twice the pixels, so it earns more detail than a 1x one.
    GuitarScene strip2x;
    GuitarRenderer::buildFitted (guitar, { 0.0f, 0.0f, 252.0f, 116.0f }, 2.0f, true, strip2x);
    CHECK (strip2x.detail <= strip.detail);
}

LUTHIER_TEST (GuitarIllustration, croppedFitKeepsHitTestingExact)
{
    using Framing = GuitarRenderer::Framing;
    const juce::Rectangle<float> area (4.0f, 4.0f, 252.0f, 116.0f);

    for (const auto& file : factoryGuitarFiles())
    {
        const auto guitar = load (file);
        const auto name = file.getFileNameWithoutExtension();
        const auto scene = GuitarRenderer::build (guitar);

        const auto whole = GuitarRenderer::fitTransform (scene, area);
        const auto crop = GuitarRenderer::frameTransform (scene, area, Framing::body);
        const auto automatic = GuitarRenderer::frameTransform (scene, area, Framing::automatic);

        // The strip is too small for the whole guitar: automatic framing crops, and gains scale.
        CHECK_MSG (GuitarRenderer::pxPerMm (whole) < 0.5f, name + ": the strip fit is not small");
        CHECK_MSG (GuitarRenderer::pxPerMm (crop) > GuitarRenderer::pxPerMm (whole), name + ": cropping did not help");
        CHECK_MSG (automatic == crop, name + ": automatic framing did not crop");

        // The body stays in frame, its tail against the right edge; the neck leaves on the left.
        const auto bodyPx = scene.bodyBounds.transformedBy (crop);
        CHECK_MSG (bodyPx.getRight() <= area.getRight() + 0.5f && bodyPx.getRight() > area.getRight() - 24.0f,
                   name + ": body right edge at " + juce::String (bodyPx.getRight()) + " px");
        CHECK_MSG (bodyPx.getY() >= area.getY() - 0.5f && bodyPx.getBottom() <= area.getBottom() + 0.5f, name + ": body clipped vertically");

        // (A short-necked acoustic may still fit its nut in: the crop is height-limited,
        // and the width shows as much neck as it has room for.)

        // Every hit region still answers to its own pixels through the inverse transform.
        for (auto& h : scene.hits)
        {
            const auto mm = h.area.getBounds().getCentre();
            if (! h.area.contains (mm))
                continue;

            auto px = mm.transformedBy (crop);
            auto back = px.transformedBy (crop.inverted());
            CHECK_MSG (back.getDistanceFrom (mm) < 0.01f, name + ": " + getGuitarRegionName (h.region) + " round trip drifted");

            const auto* hit = GuitarRenderer::hitTest (scene, back);
            CHECK_MSG (hit != nullptr && hit->area.contains (mm), name + ": " + getGuitarRegionName (h.region) + " lost under the crop");
        }

        // Pickups and controls, the strip's live parts, land inside the strip.
        for (auto& h : scene.hits)
            if (h.region == GuitarRegion::controls || h.region == GuitarRegion::pickupNeck || h.region == GuitarRegion::pickupBridge)
            {
                const auto px = h.area.getBounds().getCentre().transformedBy (crop);
                CHECK_MSG (area.contains (px), name + ": " + getGuitarRegionName (h.region) + " is outside the strip");
            }
    }
}

LUTHIER_TEST (GuitarIllustration, stripAndEasySizeRendersForReview)
{
    // Not an assertion beyond "it wrote": what the Advanced strip (260 x 124, 1x and 2x)
    // and Easy mode (890 x 390) show, on the panel's background, for a person to judge.
    const auto files = factoryGuitarFiles();
    const auto background = juce::Colour (0xfff0e6d2);
    const int stripW = 260, stripH = 124, easyW = 890, easyH = 390, cols = 4;
    const int rows = (files.size() + cols - 1) / cols;

    juce::Image strips (juce::Image::ARGB, stripW * cols, stripH * rows, true);
    juce::Image strips2x (juce::Image::ARGB, stripW * 2 * cols, stripH * 2 * rows, true);

    for (int i = 0; i < files.size(); ++i)
    {
        const auto guitar = load (files[i]);
        const int cx = (i % cols) * stripW, cy = (i / cols) * stripH;

        juce::Graphics g (strips);
        g.drawImageAt (renderLikeComponent (guitar, stripW, stripH, 1.0f, background), cx, cy);
        g.setColour (juce::Colours::black.withAlpha (0.15f));
        g.drawRect (cx, cy, stripW, stripH);

        juce::Graphics g2 (strips2x);
        g2.drawImageAt (renderLikeComponent (guitar, stripW, stripH, 2.0f, background), cx * 2, cy * 2);
    }

    CHECK (savePng (strips, renderFolder().getChildFile ("_strip_sheet.png")));
    CHECK (savePng (strips2x, renderFolder().getChildFile ("_strip_sheet_2x.png")));

    for (auto* rel : { "Electric/Vintage Single-Cut", "Electric/Vintage Double-Cut", "Acoustic/Dreadnought", "Electric/Semi-Hollow 335" })
    {
        const auto guitar = factory (juce::String (rel) + ".luthierguitar");
        const auto name = juce::String (rel).fromLastOccurrenceOf ("/", false, false);
        CHECK (savePng (renderLikeComponent (guitar, stripW, stripH, 1.0f, background), renderFolder().getChildFile ("_strip_" + name + ".png")));
        CHECK (savePng (renderLikeComponent (guitar, easyW, easyH, 1.0f, background), renderFolder().getChildFile ("_easy_" + name + ".png")));
    }
}

//==============================================================================
LUTHIER_TEST (GuitarIllustration, theKeyChangesWithEveryVisibleChange)
{
    const auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");
    const auto key = GuitarRenderer::keyFor (base, {});

    CHECK (GuitarRenderer::keyFor (base, {}) == key);

    // A part swap.
    auto swapped = base;
    for (const auto& p : library().getParts (PartType::pickup))
        if (p->name != base.get (GuitarSlot::pickupBridge)->name)
        {
            swapped.parts[(size_t) GuitarSlot::pickupBridge] = p;
            break;
        }
    CHECK (GuitarRenderer::keyFor (swapped, {}) != key);

    // A position change of more than half a millimetre.
    auto moved = base;
    moved.placements[1].positionMm += 0.6;
    CHECK (GuitarRenderer::keyFor (moved, {}) != key);

    // A finish change.
    auto refinished = base;
    refinished.finish.colourA = "#123456";
    CHECK (GuitarRenderer::keyFor (refinished, {}) != key);
}

LUTHIER_TEST (GuitarIllustration, stringColoursFollowSection10)
{
    struct Row { const char* material; bool wound, bass, flat; juce::uint32 hex; };

    const Row rows[] = {
        { "nickel_plated_steel", false, false, false, 0xffc4c7cc },   // plain steel
        { "nickel_plated_steel", true,  false, false, 0xffb0b4ba },
        { "nickel",              true,  false, false, 0xffa9aaaf },
        { "stainless",           true,  false, false, 0xffd0d4d9 },
        { "bronze_8020",         true,  false, false, 0xffc9a75a },
        { "phosphor_bronze",     true,  false, false, 0xffb5824a },
        { "silk_steel",          true,  false, false, 0xffb8b4a8 },
        { "nickel_plated_steel", true,  false, true,  0xffb0b4ba },   // flatwound
        { "tapewound",           true,  false, false, 0xff252525 },
        { "nylon",               false, false, false, 0xfff2e9d8 },
        { "nickel_plated_steel", true,  true,  false, 0xffa9aaaf },   // bass nickel roundwound
        { "stainless",           true,  true,  false, 0xffd0d4d9 },
        { "stainless",           true,  true,  true,  0xffb0b4ba },   // bass flatwound
    };

    for (auto& r : rows)
    {
        const auto c = GuitarRenderer::stringColour (r.material, r.wound, r.bass, r.flat);
        const auto want = juce::Colour (r.hex);
        const int d = std::abs ((int) c.getRed() - (int) want.getRed()) + std::abs ((int) c.getGreen() - (int) want.getGreen())
                    + std::abs ((int) c.getBlue() - (int) want.getBlue());
        CHECK_MSG (d <= 3, juce::String (r.material) + " drew " + c.toDisplayString (false) + ", want " + want.toDisplayString (false));
    }

    // And the scene uses them: a 12-53 phosphor bronze set's wound strings are amber-brown.
    const auto dread = GuitarRenderer::build (factory ("Acoustic/Dreadnought.luthierguitar"));
    int bronze = 0, plain = 0;

    for (auto& s : dread.strings)
    {
        if (s.wound)
            bronze += s.colour == juce::Colour (0xffb5824a) ? 1 : 0;
        else
            plain += s.colour == juce::Colour (0xffc4c7cc) ? 1 : 0;
    }

    CHECK_MSG (bronze == 4 && plain == 2, "dreadnought: " + juce::String (bronze) + " bronze, " + juce::String (plain) + " plain");
}

LUTHIER_TEST (GuitarIllustration, hitTestingFindsThePartOnTop)
{
    for (const auto& file : factoryGuitarFiles())
    {
        const auto guitar = load (file);
        const auto scene = GuitarRenderer::build (guitar);
        const auto name = file.getFileNameWithoutExtension();

        // 10 000 random clicks: each lands on the topmost region that contains it.
        juce::Random rng (42);
        int mismatches = 0;

        for (int i = 0; i < 10000; ++i)
        {
            const juce::Point<float> p { scene.bounds.getX() + rng.nextFloat() * scene.bounds.getWidth(),
                                         scene.bounds.getY() + rng.nextFloat() * scene.bounds.getHeight() };
            const auto* hit = GuitarRenderer::hitTest (scene, p);

            const GuitarScene::Hit* expected = nullptr;
            for (auto& h : scene.hits)
                if (h.area.contains (p))
                    expected = &h;

            if (hit != expected)
                ++mismatches;
        }

        CHECK_MSG (mismatches == 0, name + ": " + juce::String (mismatches) + " clicks hit the wrong part");

        // A pickup's centre, off the strings, selects the pickup.
        const GuitarRegion regions[] = { GuitarRegion::pickupNeck, GuitarRegion::pickupMiddle, GuitarRegion::pickupBridge };

        for (int i = 0; i < 3; ++i)
        {
            const auto& area = scene.pickupAreas[(size_t) i];
            if (area.isEmpty())
                continue;

            // Between the two middle strings, where they cross the pickup.
            const auto b = area.getBounds();
            const float x = b.getCentreX();
            const int mid = juce::jmax (1, scene.numStrings / 2);

            auto yAt = [&] (int s)
            {
                const auto sp = scene.saddlePoints[(size_t) s], np = scene.nutPoints[(size_t) s];
                return sp.y + (np.y - sp.y) * ((x - sp.x) / (np.x - sp.x));
            };

            const juce::Point<float> p { x, (yAt (mid) + yAt (mid - 1)) * 0.5f };

            if (! area.contains (p))
                continue;   // a split pickup's gap

            const auto* hit = GuitarRenderer::hitTest (scene, p);

            CHECK_MSG (hit != nullptr && hit->region == regions[i],
                       name + ": pickup " + juce::String (i) + " click hit " + (hit != nullptr ? getGuitarRegionName (hit->region) : "nothing"));
        }
    }
}

LUTHIER_TEST (GuitarIllustration, agingIsSeededAndStable)
{
    auto guitar = factory ("Electric/Vintage Single-Cut.luthierguitar");
    guitar.finish.aging = 0.7;

    const auto a = GuitarRenderer::render (guitar, 768, 300);
    const auto b = GuitarRenderer::render (guitar, 768, 300);

    const juce::Image::BitmapData da (a, juce::Image::BitmapData::readOnly), db (b, juce::Image::BitmapData::readOnly);
    int diff = 0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
            if (da.getPixelColour (x, y) != db.getPixelColour (x, y))
                ++diff;

    CHECK_MSG (diff == 0, juce::String (diff) + " pixels differ between two renders of the same aged guitar");

    // A different character seed moves the dings.
    auto other = guitar;
    other.seed = guitar.seed + 1;
    const auto c = GuitarRenderer::render (other, 768, 300);
    const juce::Image::BitmapData dc (c, juce::Image::BitmapData::readOnly);
    int moved = 0;
    for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
            if (da.getPixelColour (x, y) != dc.getPixelColour (x, y))
                ++moved;
    CHECK (moved > 0);
}

LUTHIER_TEST (GuitarIllustration, highContrastHasNoLighting)
{
    // visual-polish.md 0.2 and 7: no gradients, sheen or shadows without materials.
    GuitarRenderer::Options flat;
    flat.materials = false;

    for (const auto& file : factoryGuitarFiles())
    {
        const auto scene = GuitarRenderer::build (load (file), flat);

        for (auto& s : scene.shapes)
            CHECK_MSG (! s.lit, file.getFileNameWithoutExtension() + " kept a lighting layer in a flat build");
    }

    const auto lit = GuitarRenderer::build (factory ("Electric/Vintage Single-Cut.luthierguitar"));
    int litCount = 0;
    for (auto& s : lit.shapes)
        litCount += s.lit ? 1 : 0;
    CHECK (litCount > 0);
}

LUTHIER_TEST (GuitarIllustration, accessibleDescriptionsNameTheParts)
{
    const auto guitar = factory ("Electric/Vintage Single-Cut.luthierguitar");
    const auto scene = GuitarRenderer::build (guitar);

    juce::StringArray texts;
    for (auto& h : scene.hits)
        texts.add (h.description);

    CHECK (texts.joinIntoString ("|").contains ("Body: mahogany"));
    CHECK (texts.joinIntoString ("|").contains ("Neck pickup: PAF 57 Alnico 2 7.6k; position 152 mm from saddle"));
    CHECK (texts.joinIntoString ("|").contains ("Bridge: Vintage Adjustable Bridge"));
    CHECK (texts.joinIntoString ("|").contains ("Nut: bone, 43 mm width"));
}

LUTHIER_TEST (GuitarIllustration, fullRenderIsFastEnough)
{
    // Section 17: a static build and paint of a solid-body electric; generous for a shared laptop.
    const auto guitar = factory ("Electric/Vintage Double-Cut.luthierguitar");
    double best = 1.0e9;

    for (int run = 0; run < 3; ++run)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto image = GuitarRenderer::render (guitar, 768, 300);
        best = juce::jmin (best, juce::Time::getMillisecondCounterHiRes() - start);
        juce::ignoreUnused (image);
    }

    CHECK_MSG (best < 120.0, "static render took " + juce::String (best, 1) + " ms");
}

//==============================================================================
LUTHIER_TEST (GuitarIllustration, aFamilySwitchGivesTheTargetFamilysGuitar)
{
    // Section 12 / 19: every family to every other produces a valid guitar with
    // the target family's defaults, keeping the character seed.
    const juce::StringArray families { "electric", "acoustic", "classical", "bass", "resonator" };

    for (const auto& from : families)
    {
        auto start = factory (PartLibrary::getFamilyTemplate (from));
        start.seed = 987654321;

        for (const auto& to : families)
        {
            if (to == from)
                continue;

            WorkshopGuitar out;
            juce::String banner;
            CHECK (library().switchFamily (start, to, out, banner));

            const auto label = from + " -> " + to;
            CHECK_MSG (out.family == to, label + ": family is " + out.family);
            CHECK_MSG (out.seed == start.seed, label + ": character seed lost");
            CHECK_MSG (out.getStringCount() >= 4, label + ": " + juce::String (out.getStringCount()) + " strings");
            CHECK_MSG (banner.startsWith ("Family changed to"), label + ": banner \"" + banner + "\"");

            for (int i = 0; i < kNumGuitarSlots; ++i)
                if (auto p = out.parts[(size_t) i])
                    CHECK_MSG (p->suits (to), label + ": kept a " + p->name + " that does not suit " + to);

            // And it draws as that family.
            const auto scene = GuitarRenderer::build (out);
            CHECK_MSG (scene.family == to, label + ": drew a " + scene.family);
        }
    }
}
