/*  Amp and pedal faces and model knob caps: proposals/visual-polish.md 2, 3 and 7.
    Renders go to %TEMP%/luthier-face-renders/ for review by eye. */

#include "TestFramework.h"

#include "../UI/Faces/AmpFace.h"
#include "../UI/Faces/PedalFace.h"
#include "../UI/Faces/FaceMaterials.h"
#include "../Accessibility/Accessibility.h"

#include <iterator>
#include <set>

using namespace luthier;
using namespace luthier::faces;
using namespace luthier::tests;

namespace
{
    /** Restores the palette in force when it goes out of scope. */
    struct PaletteGuard
    {
        PaletteColours saved = Palette::current();
        bool textured = Palette::textured;
        ~PaletteGuard() { Palette::apply (saved, textured); }
    };

    void usePalette (PaletteId id)
    {
        Palette::apply (AccessibilitySettings::buildPalette (id), id != PaletteId::highContrast);
    }

    const PaletteId palettes[] = { PaletteId::defaultDark, PaletteId::light, PaletteId::highContrast };

    juce::String fileName (PaletteId id)
    {
        return juce::String (getPaletteName (id)).replaceCharacter (' ', '_');
    }

    juce::File renderFolder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-face-renders");
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

    /** Software images: Direct2D images read back blank in tests. */
    juce::Image blank (int w, int h)
    {
        return juce::Image (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    }

    struct Coverage { int inside = 0, outside = 0, area = 0; };

    Coverage coverage (const juce::Image& image, juce::Rectangle<int> bounds)
    {
        Coverage c;
        c.area = bounds.getWidth() * bounds.getHeight();
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                if (data.getPixelColour (x, y).getAlpha() > 8)
                    ++(bounds.contains (x, y) ? c.inside : c.outside);

        return c;
    }

    int differingPixels (const juce::Image& a, const juce::Image& b)
    {
        const juce::Image::BitmapData da (a, juce::Image::BitmapData::readOnly), db (b, juce::Image::BitmapData::readOnly);
        int n = 0;

        for (int y = 0; y < a.getHeight(); ++y)
            for (int x = 0; x < a.getWidth(); ++x)
                n += da.getPixelColour (x, y) == db.getPixelColour (x, y) ? 0 : 1;

        return n;
    }

    int distinctColours (const juce::Image& image)
    {
        std::set<juce::uint32> seen;
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
                seen.insert (data.getPixelColour (x, y).getARGB());

        return (int) seen.size();
    }

    constexpr int margin = 12;

    juce::Image renderAmp (AmpModel model, int w, int h, const AmpFaceState& state = {})
    {
        auto image = blank (w + margin * 2, h + margin * 2);
        {
            juce::Graphics g (image);
            paintAmpFace (g, juce::Rectangle<int> (margin, margin, w, h).toFloat(), model, state);
        }
        return image;
    }

    juce::Image renderPedal (PedalType type, int w, int h, const PedalFaceState& state)
    {
        auto image = blank (w + margin * 2, h + margin * 2);
        {
            juce::Graphics g (image);
            paintPedalFace (g, juce::Rectangle<int> (margin, margin, w, h).toFloat(), type, state);
        }
        return image;
    }

    /** A sheet of images in a grid on the palette's background. */
    juce::Image sheet (const juce::Array<juce::Image>& cells, int columns)
    {
        if (cells.isEmpty())
            return {};

        const int cw = cells[0].getWidth(), ch = cells[0].getHeight();
        const int rows = (cells.size() + columns - 1) / columns;
        auto out = blank (cw * columns, ch * rows);
        {
            juce::Graphics g (out);
            g.fillAll (Palette::current().background);

            for (int i = 0; i < cells.size(); ++i)
                g.drawImageAt (cells[i], (i % columns) * cw, (i / columns) * ch);
        }
        return out;
    }

    // The same list as TrademarkTests.cpp (qa-polish.md 11): keep the two in step.
    const juce::StringArray& brands()
    {
        static const juce::StringArray list {
            "Fender", "Gibson", "Stratocaster", "Strat", "Telecaster", "Tele", "Les Paul", "ES-335", "ES335",
            "Explorer", "Flying V", "Firebird", "Thunderbird", "Jazzmaster", "Jaguar", "Precision", "Jazz Bass",
            "Rickenbacker", "Ibanez", "Music Man", "StingRay", "EMG", "Seymour", "Duncan", "DiMarzio",
            "Floyd Rose", "Bigsby", "Kluson", "Grover", "Schaller", "Gotoh", "BadAss", "Badass",
            "Tune-o-Matic", "ABR-1", "Marshall", "Vox", "Mesa", "Boogie", "Peavey", "Orange", "Hiwatt",
            "Soldano", "Bogner", "Diezel", "Ampeg", "SVT", "JCM", "AC30", "Deluxe", "Champ", "Bassman",
            "Twin Reverb", "Celestion", "Greenback", "Jensen", "EVM", "Shure", "SM57", "SM7B", "Royer",
            "Neumann", "Sennheiser", "MD421", "AKG", "C414", "D112", "U87", "Tube Screamer", "Big Muff",
            "Klon", "Boss", "MXR", "Electro-Harmonix", "Uni-Vibe", "Leslie", "D'Addario", "NYXL",
            "Ernie Ball", "Elixir", "Selmer", "Dobro", "Gretsch", "Epiphone", "PRS", "Danelectro",
            "TransTrem", "Kinman", "Alnico Blue"
        };
        return list;
    }

    /** The brand a text uses as a whole word, case-insensitively (faces print in capitals). */
    juce::String brandIn (const juce::String& text)
    {
        const auto upper = text.toUpperCase();

        for (const auto& b : brands())
        {
            const auto key = b.toUpperCase();

            for (int at = upper.indexOf (key); at >= 0; at = upper.indexOf (at + 1, key))
            {
                const bool startOk = at == 0 || ! juce::CharacterFunctions::isLetter (upper[at - 1]);
                const bool endOk = ! juce::CharacterFunctions::isLetter (upper[at + key.length()]);

                if (startOk && endOk)
                    return b;
            }
        }

        return {};
    }
}

//==============================================================================
LUTHIER_TEST (Faces, everyAmpFaceDrawsInsideItsBoundsInEveryPalette)
{
    PaletteGuard guard;

    for (auto id : palettes)
    {
        usePalette (id);
        juce::Array<juce::Image> cells, small;

        for (int i = 0; i < (int) AmpModel::NumModels; ++i)
        {
            const auto model = (AmpModel) i;
            const auto name = juce::String (AmpEngine::getModelName (model));

            // The Advanced panel's size, and the Easy amp card's.
            for (auto size : { juce::Point<int> (480, 220), juce::Point<int> (240, 96) })
            {
                AmpFaceState state;
                state.drive = 0.6f;
                const auto image = renderAmp (model, size.x, size.y, state);
                const auto c = coverage (image, { margin, margin, size.x, size.y });

                CHECK_MSG (c.outside == 0, name + " (" + fileName (id) + ") draws " + juce::String (c.outside)
                                               + " px outside its bounds at " + juce::String (size.x));
                CHECK_MSG (c.inside > c.area * 8 / 10, name + " (" + fileName (id) + ") covers only "
                                                           + juce::String (c.inside) + " of " + juce::String (c.area) + " px");

                if (size.x != 480)
                    small.add (image);

                if (size.x == 480)
                {
                    cells.add (image);

                    if (id == PaletteId::defaultDark)
                        CHECK (savePng (image, renderFolder().getChildFile ("amp_" + name.replaceCharacter (' ', '_') + ".png")));
                }
            }
        }

        CHECK (savePng (sheet (cells, 3), renderFolder().getChildFile ("amps_" + fileName (id) + ".png")));
        CHECK (savePng (sheet (small, 4), renderFolder().getChildFile ("amps_card_" + fileName (id) + ".png")));
    }
}

LUTHIER_TEST (Faces, everyPedalFaceDrawsInsideItsBoundsInEveryPalette)
{
    PaletteGuard guard;

    for (auto id : palettes)
    {
        usePalette (id);
        juce::Array<juce::Image> upright, onItsSide;

        for (int i = 0; i < (int) PedalType::NumTypes; ++i)
        {
            const auto type = (PedalType) i;
            const auto name = juce::String (Pedal::getTypeName (type));
            const auto state = PedalFaceState::defaultsFor (type);

            for (auto size : { juce::Point<int> (150, 250), juce::Point<int> (420, 110) })
            {
                const auto image = renderPedal (type, size.x, size.y, state);
                const auto c = coverage (image, { margin, margin, size.x, size.y });
                const int wanted = type == PedalType::None ? c.area / 3 : c.area / 2;

                CHECK_MSG (c.outside == 0, name + " (" + fileName (id) + ") draws " + juce::String (c.outside) + " px outside its bounds");
                CHECK_MSG (c.inside > wanted, name + " (" + fileName (id) + ") covers only " + juce::String (c.inside)
                                                  + " of " + juce::String (c.area) + " px at " + juce::String (size.x));

                (size.x < size.y ? upright : onItsSide).add (image);

                if (id == PaletteId::defaultDark && size.x < size.y)
                    CHECK (savePng (image, renderFolder().getChildFile ("pedal_" + name.replaceCharacter (' ', '_') + ".png")));
            }
        }

        CHECK (savePng (sheet (upright, 6), renderFolder().getChildFile ("pedals_" + fileName (id) + ".png")));
        CHECK (savePng (sheet (onItsSide, 3), renderFolder().getChildFile ("pedals_rack_" + fileName (id) + ".png")));
    }
}

LUTHIER_TEST (Faces, facesRenderIdenticallyTwice)
{
    // visual-polish.md 7: lit surfaces are the same on every render, so they can be cached.
    PaletteGuard guard;
    usePalette (PaletteId::defaultDark);

    for (int i = 0; i < (int) AmpModel::NumModels; ++i)
        CHECK_MSG (differingPixels (renderAmp ((AmpModel) i, 300, 140), renderAmp ((AmpModel) i, 300, 140)) == 0,
                   juce::String (AmpEngine::getModelName ((AmpModel) i)) + " renders differently twice");

    for (int i = 0; i < (int) PedalType::NumTypes; ++i)
    {
        const auto state = PedalFaceState::defaultsFor ((PedalType) i);
        CHECK_MSG (differingPixels (renderPedal ((PedalType) i, 120, 200, state), renderPedal ((PedalType) i, 120, 200, state)) == 0,
                   juce::String (Pedal::getTypeName ((PedalType) i)) + " renders differently twice");
    }
}

LUTHIER_TEST (Faces, thePilotFollowsStandbyAndTheLedFollowsBypass)
{
    // visual-polish.md 7: Standby off/on and bypass on/off change the pilot light and pedal LEDs.
    PaletteGuard guard;
    usePalette (PaletteId::defaultDark);

    for (int i = 0; i < (int) AmpModel::NumModels; ++i)
    {
        const auto model = (AmpModel) i;
        AmpFaceState on, standby;
        standby.standby = true;

        const auto l = layoutAmpFace (juce::Rectangle<float> ((float) margin, (float) margin, 480.0f, 220.0f), model);
        const auto a = renderAmp (model, 480, 220, on), b = renderAmp (model, 480, 220, standby);
        const auto pilot = l.pilot.getCentre().roundToInt();

        CHECK_MSG (a.getPixelAt (pilot.x, pilot.y) != b.getPixelAt (pilot.x, pilot.y),
                   juce::String (AmpEngine::getModelName (model)) + ": the pilot light does not follow Standby");
    }

    for (int i = 1; i < (int) PedalType::NumTypes; ++i)
    {
        const auto type = (PedalType) i;
        auto active = PedalFaceState::defaultsFor (type), bypassed = active;
        bypassed.bypassed = true;

        const auto l = layoutPedalFace (juce::Rectangle<float> ((float) margin, (float) margin, 150.0f, 250.0f), type, active.numKnobs);
        const auto led = l.led.getCentre().roundToInt();

        CHECK_MSG (renderPedal (type, 150, 250, active).getPixelAt (led.x, led.y) != renderPedal (type, 150, 250, bypassed).getPixelAt (led.x, led.y),
                   juce::String (Pedal::getTypeName (type)) + ": the LED does not follow bypass");
    }
}

LUTHIER_TEST (Faces, knobValuesReachTheFace)
{
    PaletteGuard guard;
    usePalette (PaletteId::defaultDark);

    AmpFaceState low, high;
    low.knobs.fill (0.1f);
    high.knobs.fill (0.9f);

    for (int i = 0; i < (int) AmpModel::NumModels; ++i)
        CHECK_MSG (differingPixels (renderAmp ((AmpModel) i, 480, 220, low), renderAmp ((AmpModel) i, 480, 220, high)) > 200,
                   juce::String (AmpEngine::getModelName ((AmpModel) i)) + ": knob values do not show");

    // A face with live knobs on it leaves the knob areas to them.
    AmpFaceState bare;
    bare.drawKnobs = false;
    CHECK (differingPixels (renderAmp (AmpModel::MarshallPlexi, 480, 220, bare), renderAmp (AmpModel::MarshallPlexi, 480, 220, high)) > 200);
}

LUTHIER_TEST (Faces, layoutsKeepEveryControlOnTheFace)
{
    const juce::Rectangle<float> bounds (0.0f, 0.0f, 480.0f, 220.0f);

    for (int i = 0; i < (int) AmpModel::NumModels; ++i)
    {
        const auto l = layoutAmpFace (bounds, (AmpModel) i);
        const auto name = juce::String (AmpEngine::getModelName ((AmpModel) i));

        for (int k = 0; k < numAmpKnobs; ++k)
        {
            CHECK_MSG (l.faceplate.contains (l.knobs[(size_t) k]), name + ": knob " + juce::String (k) + " leaves the faceplate");
            CHECK_MSG (l.knobs[(size_t) k].getWidth() >= 36.0f, name + ": knob " + juce::String (k) + " is too small to use");

            if (k > 0)
                CHECK_MSG (! l.knobs[(size_t) k].intersects (l.knobs[(size_t) k - 1]), name + ": knobs overlap");
        }

        for (auto& s : l.switches)
            CHECK_MSG (l.faceplate.contains (s), name + ": a switch leaves the faceplate");

        CHECK_MSG (l.faceplate.contains (l.pilot), name + ": the pilot leaves the faceplate");
    }

    for (int i = 1; i < (int) PedalType::NumTypes; ++i)
        for (auto size : { juce::Rectangle<float> (0.0f, 0.0f, 150.0f, 250.0f), juce::Rectangle<float> (0.0f, 0.0f, 420.0f, 110.0f) })
        {
            const auto state = PedalFaceState::defaultsFor ((PedalType) i);
            const auto l = layoutPedalFace (size, (PedalType) i, state.numKnobs);

            for (int k = 0; k < l.numKnobs; ++k)
                CHECK_MSG (l.enclosure.contains (l.knobs[(size_t) k]),
                           juce::String (Pedal::getTypeName ((PedalType) i)) + ": knob " + juce::String (k) + " leaves the enclosure");
        }
}

LUTHIER_TEST (Faces, highContrastFacesAreFlat)
{
    // visual-polish.md 0.2: High contrast turns textures and sheen off.
    PaletteGuard guard;

    usePalette (PaletteId::defaultDark);
    const int textured = distinctColours (renderAmp (AmpModel::FenderTweed, 480, 220));

    usePalette (PaletteId::highContrast);
    const int flat = distinctColours (renderAmp (AmpModel::FenderTweed, 480, 220));

    CHECK_MSG (flat * 3 < textured, "High contrast still looks textured: " + juce::String (flat)
                                        + " colours against " + juce::String (textured));
}

LUTHIER_TEST (Faces, noFaceTextNamesABrand)
{
    // factory-content.md 0.1 and qa-polish.md 11 apply to everything the faces print.
    for (int i = 0; i < (int) AmpModel::NumModels; ++i)
        for (const auto& text : ampFaceTexts ((AmpModel) i))
            CHECK_MSG (brandIn (text).isEmpty(), "amp face text \"" + text + "\" uses the trademark " + brandIn (text));

    for (int i = 0; i < (int) PedalType::NumTypes; ++i)
        for (const auto& text : pedalFaceTexts ((PedalType) i))
            CHECK_MSG (brandIn (text).isEmpty(), "pedal face text \"" + text + "\" uses the trademark " + brandIn (text));
}

LUTHIER_TEST (Faces, everyKnobCapRendersAndKeepsTheArc)
{
    PaletteGuard guard;

    const KnobCap caps[] = { KnobCap::bell, KnobCap::skirtedNumbers, KnobCap::pointer, KnobCap::goldCap,
                             KnobCap::chickenHead, KnobCap::chickenHeadBlack, KnobCap::speed, KnobCap::chromeDome,
                             KnobCap::chromeSkirt, KnobCap::softTouch, KnobCap::ribbed, KnobCap::witchHat, KnobCap::creamRibbed };

    for (auto id : palettes)
    {
        usePalette (id);
        const int cell = 72;
        auto image = blank (cell * (int) std::size (caps), cell);

        {
            juce::Graphics g (image);
            g.fillAll (Palette::current().panel);

            for (size_t i = 0; i < std::size (caps); ++i)
                paintKnob (g, juce::Rectangle<float> ((float) i * (float) cell, 0.0f, (float) cell, (float) cell).reduced (8.0f),
                           caps[i], 0.65f, true);
        }

        CHECK (savePng (image, renderFolder().getChildFile ("knob_caps_" + fileName (id) + ".png")));

        // The value arc is the standard one: the accent at the arc's start, on every cap.
        for (size_t i = 0; i < std::size (caps); ++i)
        {
            const auto area = juce::Rectangle<float> ((float) i * (float) cell, 0.0f, (float) cell, (float) cell).reduced (8.0f);
            const float r = knobRadiusIn (area) + Metrics::arcGap + Metrics::arcThickness * 0.5f;
            const auto at = area.getCentre().getPointOnCircumference (r, knobAngleFor (0.1f)).roundToInt();
            CHECK_MSG (image.getPixelAt (at.x, at.y) != Palette::current().panel, "cap " + juce::String ((int) i) + " hides its value arc");
        }
    }

    // The LookAndFeel wears the cap on a real slider.
    usePalette (PaletteId::defaultDark);
    FaceKnobLookAndFeel lnf (KnobCap::chickenHead);
    juce::Slider knob (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
    knob.setLookAndFeel (&lnf);
    knob.setRange (0.0, 1.0);
    knob.setValue (0.3, juce::dontSendNotification);
    knob.setBounds (0, 0, 64, 64);

    auto a = blank (64, 64), b = blank (64, 64);
    {
        juce::Graphics g (a);
        knob.paintEntireComponent (g, false);
    }
    lnf.setCap (KnobCap::bell);
    {
        juce::Graphics g (b);
        knob.paintEntireComponent (g, false);
    }
    knob.setLookAndFeel (nullptr);

    CHECK (differingPixels (a, b) > 50);
}
