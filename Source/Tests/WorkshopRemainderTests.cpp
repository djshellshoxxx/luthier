/*  The Workshop bench's remainder (TODO 7, VISUAL-WORKSHOP-QA): per-string
    overrides (workshop-ui.md 3.3, guitar-illustration.md 10 and 13.2), the nut
    slot drag and the pick / slide / capo on the bench (workshop-ui.md 4),
    the WORKSHOP tab padlock (gui-integration.md 21), the spectrum summary for
    screen readers (workshop-ui.md 10) and the slide material in the engine. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/WorkshopPanel.h"
#include "../UI/Guitar/GuitarRenderer.h"
#include "../Model/Workshop/PartAcoustics.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct Bench
    {
        LuthierAudioProcessor processor;

        Bench (GuitarType type = GuitarType::LesPaul)
        {
            processor.prepareToPlay (48000.0, 512);
            loadType (type);
        }

        void loadType (GuitarType type)
        {
            auto* p = processor.getState().getParameter (ParamIDs::guitarType);
            p->setValueNotifyingHost (p->convertTo0to1 ((float) type));
            processor.getParameterBridge().applyAllNow();
        }

        void setPlain (const char* id, float plain)
        {
            if (auto* p = processor.getState().getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (plain));
            processor.getParameterBridge().applyAllNow();
        }

        WorkshopBench& bench() { return processor.getBench(); }
        const WorkshopGuitar& guitar() { return processor.getCurrentGuitar(); }

        std::vector<float> render (std::initializer_list<int> notes = { 40, 47, 52, 55, 59, 64 })
        {
            processor.getEngine().reset();
            juce::AudioBuffer<float> buffer (2, 512);
            std::vector<float> out;

            for (int block = 0; block < 24; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    for (int note : notes)
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

                buffer.clear();
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
            }

            return out;
        }
    };

    double difference (const std::vector<float>& a, const std::vector<float>& b)
    {
        double d = 0.0;
        for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
            d += std::abs ((double) a[i] - (double) b[i]);
        return d;
    }

    juce::uint64 digest (const juce::Image& image)
    {
        juce::uint64 sum = 0;
        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < pixels.height; ++y)
            for (int x = 0; x < pixels.width; ++x)
                sum += (juce::uint64) pixels.getPixelColour (x, y).getARGB() * (juce::uint64) (x + 1);

        return sum;
    }
}

//==============================================================================
/*  3.3: a heavier third on its own - one undo entry in real units, the three
    feedbacks (drawn, read, heard), and the file round trip. */
LUTHIER_TEST (WorkshopStrings, aPerStringOverrideIsSeenReadHeardAndOneEntry)
{
    Bench b;
    const int s = 2;   // the G, engine index 2 (people call it string 3)

    const auto before = b.render();
    // A thousandth of an inch is under a pixel at the fit zoom, so the picture
    // is compared at the illustration's largest test size (section 19: 3072).
    const auto imageBefore = GuitarRenderer::render (b.guitar(), 3072, 1536);
    const auto described = b.bench().describeString (b.guitar(), s);
    const int steps = b.processor.getNumUndoSteps();

    StringOverride o;
    o.gaugeIn = 0.018;
    o.wound = 0;
    CHECK (b.bench().setStringOverride (s, o));

    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK_MSG (b.processor.getUndoDescription() == "Set string 3 to 0.018 plain (was " + described + ")",
               b.processor.getUndoDescription());

    // Read: the engine's gauge; drawn: the picture; heard: the render.
    CHECK (std::abs (b.processor.getEngine().getCustomStringGauge (s) - 0.018) < 1.0e-9);
    CHECK (digest (GuitarRenderer::render (b.guitar(), 3072, 1536)) != digest (imageBefore));
    CHECK (difference (b.render(), before) > 1.0e-3);

    // The same value again is not a change.
    CHECK (! b.bench().setStringOverride (s, o));

    // The file keeps it: per_string_override, people's numbering.
    const auto json = b.guitar().toVar();
    const auto list = json.getProperty ("parts", {}).getProperty ("strings", {}).getProperty ("per_string_override", {});
    CHECK (list.isArray() && list.size() == 1);
    CHECK ((int) list[0].getProperty ("string", 0) == 3);

    WorkshopGuitar reloaded;
    PartLibrary::LoadReport report;
    CHECK (b.processor.getPartLibrary().buildGuitar (b.guitar().toEmbeddedVar(), reloaded, report));
    CHECK (reloaded == b.guitar());

    // Undo puts the set's string back.
    b.processor.undo();
    CHECK (! b.guitar().stringOverrides[(size_t) s].isSet());
}

/*  guitar-illustration.md 10: an overridden string is drawn in its own
    material colour, independently of the set. */
LUTHIER_TEST (WorkshopStrings, anOverriddenStringIsDrawnInItsOwnMaterial)
{
    Bench b;
    auto g = b.guitar();
    g.stringOverrides[5].material = "phosphor_bronze";
    g.stringOverrides[5].wound = 1;

    const auto scene = GuitarRenderer::build (g);
    const GuitarScene::StringLine* low = nullptr;
    const GuitarScene::StringLine* next = nullptr;

    for (auto& line : scene.strings)
    {
        if (line.index == 5) low = &line;
        if (line.index == 4) next = &line;
    }

    CHECK (low != nullptr && next != nullptr);

    if (low != nullptr && next != nullptr)
    {
        CHECK (low->colour == GuitarRenderer::stringColour ("phosphor_bronze", true, false));
        CHECK (next->colour != low->colour);
    }

    CHECK (scene.stringDescriptions.size() >= 6);
    CHECK (scene.stringDescriptions[5].contains ("phosphor bronze wound") && scene.stringDescriptions[5].contains ("overridden"));

    // And the engine hears a bronze low E, not the set's nickel one.
    const auto d = mapSpec (g);
    CHECK (d.stringMaterialOverride[5] == (int) StringMaterial::PhosphorBronze);
    CHECK (d.stringMaterialOverride[4] == -1);
}

/*  13.2: a strings card onto a selected string overrides that string only; a
    card anywhere else replaces the set and its overrides. */
LUTHIER_TEST (WorkshopStrings, aCardOntoAStringOverridesItAndASetClearsOverrides)
{
    Bench b;
    WorkshopPanel panel (b.processor);
    panel.setSize (1100, 700);
    panel.showCategory ("Strings");

    int other = -1;
    for (int i = 0; i < panel.getDrawerParts().size(); ++i)
        if (panel.getDrawerParts()[i]->name != b.guitar().get (GuitarSlot::strings)->name)
        {
            other = i;
            break;
        }

    CHECK (other >= 0);
    if (other < 0)
        return;

    const auto setName = b.guitar().get (GuitarSlot::strings)->name;
    panel.getIllustration().select (GuitarRegion::strings, 5);
    panel.clickCard (other, true);

    CHECK (b.guitar().get (GuitarSlot::strings)->name == setName);
    CHECK (b.guitar().stringOverrides[5].isSet());
    CHECK (panel.getInspectorLines().joinIntoString ("\n").contains ("(override)"));

    // Editing the override from the inspector: "18" means 0.018".
    CHECK (panel.editInspectorField (juce::String (WorkshopPanel::kStringFieldPrefix) + "gauge", "18"));
    CHECK (std::abs (b.guitar().stringOverrides[5].gaugeIn - 0.018) < 1.0e-9);

    // A plain click fits the whole set, and the override goes with the old set.
    panel.clickCard (other, false);
    CHECK (b.guitar().get (GuitarSlot::strings)->name != setName);
    CHECK (! b.guitar().stringOverrides[5].isSet());
}
