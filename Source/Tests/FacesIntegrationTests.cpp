/*  The amp and pedal faces on the real panels: proposals/visual-polish.md 2-4
    and 7, with gui-integration.md 3.2, 4.3 and 19 - the same controls, attached
    to the same parameters, in the same sections, now sitting on the faces.

    Renders of the Advanced window at 1600x900 and the Easy window at 1200x720,
    in the Default, Light and High-contrast palettes, go to
    %TEMP%/luthier-guitar-renders/_advanced_faces.png and _easy_faces.png for
    review by eye. */

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/AmpFacePanel.h"
#include "../UI/PedalRack.h"
#include "../UI/Faces/AmpFace.h"
#include "../UI/Faces/PedalFace.h"
#include "../UI/Faces/FaceMaterials.h"

#include <iterator>
#include <set>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    const PaletteId palettes[] = { PaletteId::defaultDark, PaletteId::light, PaletteId::highContrast };

    /** Puts palettes in force as the Options page does, and the original back when it goes out of scope. */
    struct PaletteScope
    {
        PaletteId original = AccessibilitySettings::get().getPalette();
        PaletteColours saved = Palette::current();
        bool textured = Palette::textured;

        void use (PaletteId id)
        {
            auto& settings = AccessibilitySettings::get();
            settings.setPalette (id);
            Palette::apply (settings.getColours(), id != PaletteId::highContrast);
        }

        ~PaletteScope()
        {
            AccessibilitySettings::get().setPalette (original);
            Palette::apply (saved, textured);
        }
    };

    const char* const ampKnobIds[faces::numAmpKnobs] = { ParamIDs::ampGain, ParamIDs::ampBass, ParamIDs::ampMid,
                                                         ParamIDs::ampTreble, ParamIDs::ampPresence, ParamIDs::ampMaster };
    const char* const ampSwitchIds[faces::numAmpSwitches] = { ParamIDs::ampBright, ParamIDs::ampMidBoost, ParamIDs::ampStandby };

    void set (LuthierAudioProcessor& processor, const juce::String& id, float plain)
    {
        if (auto* p = processor.getState().getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    /** Software images: Direct2D images read back blank in tests. */
    juce::Image render (juce::Component& c)
    {
        juce::Image image (juce::Image::ARGB, juce::jmax (1, c.getWidth()), juce::jmax (1, c.getHeight()), true,
                           juce::SoftwareImageType());
        juce::Graphics g (image);
        c.paintEntireComponent (g, true);
        return image;
    }

    template <typename T>
    void collect (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);

            collect<T> (*child, found);
        }
    }

    /** Where a knob's slider - the part you turn - sits, in `panel`'s coordinates. */
    juce::Rectangle<float> sliderIn (juce::Component& panel, LuthierKnob& knob)
    {
        auto& slider = knob.getSlider();
        return panel.getLocalArea (&slider, slider.getLocalBounds()).toFloat();
    }

    bool inside (juce::Rectangle<float> outer, juce::Rectangle<float> inner, float slack = 1.0f)
    {
        return outer.expanded (slack).contains (inner);
    }

    float overlap (juce::Rectangle<float> a, juce::Rectangle<float> b)
    {
        const auto i = a.getIntersection (b);
        return i.getWidth() * i.getHeight();
    }

    int differingPixels (const juce::Image& a, const juce::Image& b, juce::Rectangle<int> area)
    {
        area = area.getIntersection (a.getBounds()).getIntersection (b.getBounds());
        int n = 0;

        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x)
                n += a.getPixelAt (x, y) == b.getPixelAt (x, y) ? 0 : 1;

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
        return out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out);
    }

    /** Images one above the other. */
    juce::Image stack (const juce::Array<juce::Image>& rows)
    {
        int w = 1, h = 0;

        for (auto& r : rows)
        {
            w = juce::jmax (w, r.getWidth());
            h += r.getHeight();
        }

        juce::Image out (juce::Image::ARGB, w, juce::jmax (1, h), true, juce::SoftwareImageType());
        juce::Graphics g (out);
        int y = 0;

        for (auto& r : rows)
        {
            g.drawImageAt (r, 0, y);
            y += r.getHeight();
        }

        return out;
    }

    juce::String modelName (AmpModel model)
    {
        return AmpEngine::getModelName (model);
    }

    /** The amp controls on a face: attached to the AMP section's parameters, on the face's own knob and
        switch positions, and big enough to use. */
    void checkAmpFace (TestContext& ctx, AmpFacePanel& face, bool withSwitches, const juce::String& where)
    {
        const auto l = face.getFaceLayout();
        const auto model = modelName (face.getShownModel());

        for (int k = 0; k < faces::numAmpKnobs; ++k)
        {
            auto& knob = face.getKnob ((faces::AmpKnob) k);
            const auto id = juce::String (ampKnobIds[k]);
            const auto s = sliderIn (face, knob);

            CHECK_MSG (knob.getParameterId() == id, where + ": knob " + juce::String (k) + " is attached to "
                                                        + knob.getParameterId() + ", not " + id);
            CHECK_MSG (knob.isVisible(), where + ": " + id + " is hidden");
            CHECK_MSG (inside (l.faceplate, s), where + " (" + model + "): " + id + " leaves the faceplate");
            CHECK_MSG (s.getCentre().getDistanceFrom (l.knobs[(size_t) k].getCentre()) < 1.5f,
                       where + " (" + model + "): " + id + " is not on the face's knob");
            CHECK_MSG (juce::jmin (s.getWidth(), s.getHeight()) >= 32.0f,
                       where + " (" + model + "): " + id + " is " + juce::String (juce::jmin (s.getWidth(), s.getHeight()), 1)
                           + " px, too small to use");
            CHECK_MSG (face.getLocalBounds().contains (knob.getBounds()), where + ": " + id + " hangs outside the panel");
        }

        for (int i = 0; i < faces::numAmpSwitches; ++i)
        {
            auto* toggle = face.getSwitch ((faces::AmpSwitch) i);
            const auto id = juce::String (ampSwitchIds[i]);

            if (! withSwitches)
            {
                CHECK_MSG (toggle == nullptr, where + ": the card grew a " + id + " switch it never had");
                continue;
            }

            CHECK_MSG (toggle != nullptr, where + ": " + id + " is missing");

            if (toggle == nullptr)
                continue;

            CHECK_MSG (toggle->getLearnParameterId() == id, where + ": switch " + juce::String (i) + " is attached to "
                                                                + toggle->getLearnParameterId() + ", not " + id);
            CHECK_MSG (toggle->isVisible(), where + ": " + id + " is hidden");
            CHECK_MSG (inside (l.faceplate, toggle->getBounds().toFloat()), where + " (" + model + "): " + id + " leaves the faceplate");
            CHECK_MSG (faces::getFaceSwitch (toggle->getButton()) != faces::FaceSwitch::none,
                       where + ": " + id + " is not drawn as the amp's switch");
        }
    }

    /** Opens an editor on `processor` at a size, in Advanced mode or not. */
    std::unique_ptr<juce::AudioProcessorEditor> openEditor (TestContext& ctx, LuthierAudioProcessor& processor,
                                                            int width, int height, bool advanced)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

        if (editor == nullptr)
        {
            CHECK_MSG (false, "no editor");
            return {};
        }

        editor->setVisible (true);
        editor->setSize (width, height);

        if (advanced)
        {
            const auto* binding = AccessibilitySettings::get().findShortcut ("toggleAdvanced");
            CHECK (binding != nullptr && editor->keyPressed (binding->key));
        }

        return editor;
    }

    AmpFacePanel* findFace (juce::Component& root, AmpFacePanel::Style style)
    {
        juce::Array<AmpFacePanel*> found;
        collect (root, found);

        for (auto* f : found)
            if (f->getStyle() == style)
                return f;

        return nullptr;
    }

    /** A few pedals in each rack, set the way a user or a preset sets them. */
    void fillRacks (LuthierAudioProcessor& processor)
    {
        const PedalType pre[] = { PedalType::Compressor, PedalType::Overdrive, PedalType::Wah };
        const PedalType post[] = { PedalType::Chorus, PedalType::Delay, PedalType::GraphicEQ, PedalType::RotarySpeaker };

        for (int i = 0; i < (int) std::size (pre); ++i)
            set (processor, ParamIDs::slotType (false, i), (float) (int) pre[i]);

        for (int i = 0; i < (int) std::size (post); ++i)
            set (processor, ParamIDs::slotType (true, i), (float) (int) post[i]);

        processor.getParameterBridge().applyAllNow();
    }
}

//==============================================================================
/*  The painter defect: at card size the combo faces' labels ran into each other
    ("MIDDL...", "PRESE...", "MIDSTANDBY"). Every label now has its own
    rectangle and is printed whole, short or not at all. */
LUTHIER_TEST (FacesIntegration, ampLabelsFitAndNeverCollide)
{
    struct Size { float w, h; bool switches, live; const char* name; };

    const Size sizes[] = {
        { 240.0f, 96.0f,  true,  false, "card" },
        { 268.0f, 96.0f,  false, true,  "Easy card" },
        { 234.0f, (float) AmpFacePanel::sectionHeight, true, true, "Advanced section" },
        { 480.0f, 220.0f, true,  false, "full" },
        { 480.0f, 220.0f, false, false, "full, no switches" }
    };

    for (int m = 0; m < (int) AmpModel::NumModels; ++m)
    {
        const auto model = (AmpModel) m;

        for (const auto& size : sizes)
        {
            const auto l = faces::layoutAmpFace ({ 0.0f, 0.0f, size.w, size.h }, model, size.switches);
            const auto fitted = faces::fitAmpFaceLabels (l);
            const auto where = modelName (model) + " at " + size.name;

            juce::Array<juce::Rectangle<float>> rects;
            juce::Array<faces::FittedPrint> prints;

            for (size_t k = 0; k < l.labels.size(); ++k)
            {
                rects.add (l.labels[k]);
                prints.add (fitted.knobs[k]);
            }

            if (l.hasSwitches)
                for (size_t s = 0; s < l.switchLabels.size(); ++s)
                {
                    rects.add (l.switchLabels[s]);
                    prints.add (fitted.switches[s]);
                }

            if (! l.inputLabel.isEmpty())
            {
                rects.add (l.inputLabel);
                prints.add (fitted.input);
            }

            // No two label rectangles overlap.
            for (int i = 0; i < rects.size(); ++i)
                for (int j = i + 1; j < rects.size(); ++j)
                    CHECK_MSG (overlap (rects[i], rects[j]) < 0.5f,
                               where + ": labels " + juce::String (i) + " and " + juce::String (j) + " overlap");

            // Every label shown fits its rectangle whole, at a readable size.
            for (int i = 0; i < rects.size(); ++i)
            {
                const auto& p = prints.getReference (i);

                if (! p.isVisible())
                    continue;

                const float width = faces::printWidth (p.text, p.height, false);
                CHECK_MSG (width <= rects[i].getWidth() + 0.01f, where + ": \"" + p.text + "\" is " + juce::String (width, 1)
                                                                     + " px in " + juce::String (rects[i].getWidth(), 1));
                CHECK_MSG (p.height <= rects[i].getHeight() + 0.01f, where + ": \"" + p.text + "\" is taller than its rectangle");
                CHECK_MSG (p.height >= faces::minPrintHeight, where + ": \"" + p.text + "\" is too small to read");

                // A label sits on neither a knob nor a switch.
                for (auto& knob : l.knobs)
                    CHECK_MSG (overlap (rects[i], knob) < 0.5f, where + ": \"" + p.text + "\" is under a knob");

                if (l.hasSwitches)
                    for (auto& sw : l.switches)
                        CHECK_MSG (overlap (rects[i], sw) < 0.5f, where + ": \"" + p.text + "\" is under a switch");
            }

            // The knob labels show - whole or short - at every size the panels use.
            for (size_t k = 0; k < fitted.knobs.size(); ++k)
                CHECK_MSG (fitted.knobs[k].isVisible(), where + ": the " + faces::ampKnobLabel ((faces::AmpKnob) k, false)
                                                            + " label is hidden");

            // Knobs and switches on the faceplate, apart, and the live sizes usable.
            for (size_t k = 0; k < l.knobs.size(); ++k)
            {
                CHECK_MSG (inside (l.faceplate, l.knobs[k], 0.5f), where + ": a knob leaves the faceplate");

                for (size_t k2 = k + 1; k2 < l.knobs.size(); ++k2)
                    CHECK_MSG (overlap (l.knobs[k], l.knobs[k2]) < 0.5f, where + ": knobs overlap");

                if (size.live)
                    CHECK_MSG (l.knobs[k].getWidth() >= 32.0f, where + ": knob " + juce::String ((int) k) + " is "
                                                                   + juce::String (l.knobs[k].getWidth(), 1) + " px");
            }

            if (l.hasSwitches)
                for (size_t s = 0; s < l.switches.size(); ++s)
                {
                    CHECK_MSG (inside (l.faceplate, l.switches[s], 0.5f), where + ": a switch leaves the faceplate");
                    CHECK_MSG (l.switches[s].getWidth() >= 12.0f || ! size.live, where + ": a switch is too small to use");

                    for (auto& knob : l.knobs)
                        CHECK_MSG (overlap (l.switches[s], knob) < 0.5f, where + ": a switch is on a knob");
                }

            CHECK_MSG (inside (l.cabinet, l.pilot, 0.5f), where + ": the pilot leaves the amp");
        }
    }
}

//==============================================================================
/*  gui-integration.md 4.3 and 19: the Advanced AMP section keeps its model
    choice, and its six knobs and three switches are the face's - once each,
    attached to the same parameters, on the face, for every model. */
LUTHIER_TEST (FacesIntegration, theAdvancedAmpSectionHasItsControlsOnTheFace)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto editor = openEditor (ctx, processor, 1600, 900, true);

    if (editor == nullptr)
        return;

    juce::Array<AdvancedPanel*> panels;
    collect (*editor, panels);
    CHECK (panels.size() == 1);

    if (panels.isEmpty())
        return;

    auto& advanced = *panels.getFirst();
    CHECK_MSG (advanced.isVisible(), "the Advanced panel is not showing");

    auto* face = findFace (advanced, AmpFacePanel::Style::section);
    CHECK_MSG (face != nullptr, "the AMP section has no amp face");

    if (face == nullptr)
        return;

    CHECK_MSG (face->isVisible() && face->getWidth() > 200, "the amp face is " + juce::String (face->getWidth()) + " wide");

    // Each amp control exists once in the panel - on the face, not also somewhere else.
    juce::Array<LuthierKnob*> knobs;
    juce::Array<LuthierToggle*> toggles;
    juce::Array<LuthierChoice*> choices;
    collect (advanced, knobs);
    collect (advanced, toggles);
    collect (advanced, choices);

    for (auto* id : ampKnobIds)
    {
        int found = 0;

        for (auto* k : knobs)
            if (k->getParameterId() == id)
                ++found;

        CHECK_MSG (found == 1, juce::String (id) + " appears " + juce::String (found) + " times in the Advanced panel");
    }

    for (auto* id : ampSwitchIds)
    {
        int found = 0;

        for (auto* t : toggles)
            if (t->getLearnParameterId() == id)
                ++found;

        CHECK_MSG (found == 1, juce::String (id) + " appears " + juce::String (found) + " times in the Advanced panel");
    }

    // The model choice stays, in the same column, just above the face.
    LuthierChoice* modelChoice = nullptr;

    for (auto* c : choices)
        if (c->getLearnParameterId() == ParamIDs::ampModel)
            modelChoice = c;

    CHECK_MSG (modelChoice != nullptr, "the AMP section lost its model choice");

    if (modelChoice != nullptr)
    {
        CHECK_MSG (modelChoice->getParentComponent() == face->getParentComponent(), "the face moved out of the AMP section's column");
        CHECK_MSG (modelChoice->getBottom() <= face->getY(), "the face is not below the model choice");
        CHECK_MSG (face->getY() - modelChoice->getBottom() <= Metrics::grid, "something sits between the model choice and the face");
    }

    for (int m = 0; m < (int) AmpModel::NumModels; ++m)
    {
        set (processor, ParamIDs::ampModel, (float) m);
        face->refresh();

        CHECK_MSG (face->getShownModel() == (AmpModel) m, "the face does not follow the model choice");
        checkAmpFace (ctx, *face, true, "Advanced");
    }
}

/*  gui-integration.md 3.2: the Easy card keeps the model choice and its six
    knobs (no switches, as before), and at 1200x720 they are big enough to use
    (TODO 2h). */
LUTHIER_TEST (FacesIntegration, theEasyAmpCardHasItsKnobsOnTheFace)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto editor = openEditor (ctx, processor, LuthierAudioProcessorEditor::defaultWidth,
                              LuthierAudioProcessorEditor::defaultHeight, false);

    if (editor == nullptr)
        return;

    juce::Array<EasyPanel*> panels;
    collect (*editor, panels);
    CHECK (panels.size() == 1);

    if (panels.isEmpty())
        return;

    auto& easy = *panels.getFirst();
    auto* face = findFace (easy, AmpFacePanel::Style::card);
    CHECK_MSG (face != nullptr, "the Easy amp card has no amp face");

    if (face == nullptr)
        return;

    CHECK_MSG (easy.getRigArea().contains (face->getBounds()), "the amp face is outside the rig strip");
    CHECK_MSG (face->getHeight() >= 80, "the amp face is only " + juce::String (face->getHeight()) + " px tall");

    juce::Array<LuthierChoice*> choices;
    collect (easy, choices);
    bool modelChoice = false;

    for (auto* c : choices)
        modelChoice = modelChoice || c->getLearnParameterId() == ParamIDs::ampModel;

    CHECK_MSG (modelChoice, "the amp card lost its model choice");

    for (int m = 0; m < (int) AmpModel::NumModels; ++m)
    {
        set (processor, ParamIDs::ampModel, (float) m);
        face->refresh();
        checkAmpFace (ctx, *face, false, "Easy");
    }
}

//==============================================================================
/*  visual-polish.md 2: every pedal type's knobs, mix and footswitch sit on its
    face, attached to the slot's parameters, and the type selector stays. */
LUTHIER_TEST (FacesIntegration, everyRackSlotHasItsControlsOnItsFace)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& chain = processor.getEngine().getPostEffects();
    PedalRack rack (processor, true);

    juce::Array<PedalType> types;

    for (int t = 1; t < (int) PedalType::NumTypes; ++t)
        types.add ((PedalType) t);

    for (int first = 0; first < types.size(); first += EffectsChain::kNumSlots)
    {
        for (int s = 0; s < EffectsChain::kNumSlots; ++s)
            chain.setSlotType (s, first + s < types.size() ? types[first + s] : PedalType::None);

        rack.setSize (PedalSlotComponent::nominalWidth, 100);
        rack.refresh();
        rack.setSize (PedalSlotComponent::nominalWidth, rack.getPreferredHeight());
        rack.resized();

        for (int s = 0; s < rack.getNumSlots(); ++s)
        {
            auto* slot = rack.getSlot (s);
            const auto type = slot->getShownType();

            if (type == PedalType::None)
                continue;

            const auto name = juce::String (Pedal::getTypeName (type));
            const auto l = slot->getFaceLayout();
            const auto pedal = Pedal::create (type);
            const int params = juce::jmin (pedal->getNumParameters(), Pedal::kMaxParams);

            CHECK_MSG (slot->getNumParameterKnobs() == params, name + " has " + juce::String (slot->getNumParameterKnobs())
                                                                  + " knobs for " + juce::String (params) + " parameters");
            CHECK_MSG (l.numKnobs == params + 1, name + ": the face has room for " + juce::String (l.numKnobs) + " knobs");
            CHECK_MSG (inside (slot->getLocalBounds().toFloat(), l.enclosure, 0.0f), name + ": the face leaves its slot");

            auto checkKnob = [&] (LuthierKnob& knob, int index, const juce::String& id)
            {
                const auto s2 = sliderIn (*slot, knob);

                CHECK_MSG (knob.getParameterId() == id, name + ": knob " + juce::String (index) + " is attached to "
                                                            + knob.getParameterId() + ", not " + id);
                CHECK_MSG (knob.isVisible(), name + ": " + id + " is hidden");
                CHECK_MSG (inside (l.enclosure, s2), name + ": " + id + " leaves the face");
                CHECK_MSG (slot->getLocalBounds().contains (knob.getBounds()), name + ": " + id + " hangs outside the slot");

                if (l.sliders)
                    CHECK_MSG (s2.getHeight() >= 56.0f, name + ": fader " + juce::String (index) + " has "
                                                            + juce::String (s2.getHeight(), 1) + " px of travel");
                else
                    CHECK_MSG (juce::jmin (s2.getWidth(), s2.getHeight()) >= 30.0f,
                               name + ": knob " + juce::String (index) + " is "
                                   + juce::String (juce::jmin (s2.getWidth(), s2.getHeight()), 1) + " px, too small to use");
            };

            for (int i = 0; i < slot->getNumParameterKnobs(); ++i)
                if (auto* knob = slot->getParameterKnob (i))
                    checkKnob (*knob, i, ParamIDs::slotParam (true, s, i));

            checkKnob (slot->getMixKnob(), params, ParamIDs::slotMix (true, s));

            // Bypass is the footswitch.
            auto& bypass = slot->getBypassToggle();
            CHECK_MSG (bypass.getLearnParameterId() == ParamIDs::slotBypass (true, s), name + ": the footswitch is not bypass");
            CHECK_MSG (bypass.isVisible() && ! l.footswitch.isEmpty(), name + ": no footswitch");
            CHECK_MSG (inside (l.enclosure, bypass.getBounds().toFloat()), name + ": the footswitch leaves the face");
            CHECK_MSG (bypass.getBounds().toFloat().getCentre().getDistanceFrom (l.footswitch.getCentre()) < 1.5f,
                       name + ": bypass is not on the footswitch");
            CHECK (faces::getFaceSwitch (bypass.getButton()) == faces::FaceSwitch::footswitch);

            // The type selector stays.
            juce::Array<LuthierChoice*> choices;
            collect (*slot, choices);
            bool selector = false;

            for (auto* c : choices)
                selector = selector || (c->isVisible() && c->getLearnParameterId() == ParamIDs::slotType (true, s));

            CHECK_MSG (selector, name + ": the type selector is gone");
        }
    }
}

//==============================================================================
/*  visual-polish.md 0.3 and 7: a face renders once, and again only when what
    it depicts changes; a knob turning is not one of those. Standby and bypass
    are, and they change the pilot and the LED. */
LUTHIER_TEST (FacesIntegration, facesAreDrawnOnceAndAgainOnlyWhenTheyChange)
{
    PaletteScope palette;
    palette.use (PaletteId::defaultDark);

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    set (processor, ParamIDs::ampModel, (float) (int) AmpModel::MarshallPlexi);
    set (processor, ParamIDs::ampStandby, 0.0f);

    AmpFacePanel face (processor, AmpFacePanel::Style::section);
    face.setSize (234, AmpFacePanel::sectionHeight);
    face.refresh();

    const auto first = render (face);
    const auto second = render (face);
    CHECK_MSG (face.getFaceRenderCount() == 1, "the face was drawn " + juce::String (face.getFaceRenderCount()) + " times for two paints");
    CHECK (differingPixels (first, second, first.getBounds()) == 0);

    // A knob turning moves its knob, not the face under it.
    set (processor, ParamIDs::ampGain, 0.95f);
    face.refresh();
    render (face);
    CHECK_MSG (face.getFaceRenderCount() == 1, "turning a knob redrew the whole face");

    // Standby: the face is drawn again, and the pilot shows it.
    const auto pilot = face.getFaceLayout().pilot.getCentre().roundToInt();
    set (processor, ParamIDs::ampStandby, 1.0f);
    face.refresh();
    const auto standby = render (face);
    CHECK (face.getFaceRenderCount() == 2);
    CHECK_MSG (first.getPixelAt (pilot.x, pilot.y) != standby.getPixelAt (pilot.x, pilot.y), "the pilot does not follow Standby");

    // The model, the size and the palette each redraw it once.
    set (processor, ParamIDs::ampModel, (float) (int) AmpModel::FenderTweed);
    face.refresh();
    render (face);
    render (face);
    CHECK (face.getFaceRenderCount() == 3);

    face.setSize (250, AmpFacePanel::sectionHeight);
    render (face);
    CHECK (face.getFaceRenderCount() == 4);

    palette.use (PaletteId::light);
    face.sendLookAndFeelChange();
    render (face);
    render (face);
    CHECK (face.getFaceRenderCount() == 5);
    palette.use (PaletteId::defaultDark);

    // A pedal: the same, with bypass and its LED.
    processor.getEngine().getPostEffects().setSlotType (0, PedalType::Overdrive);
    set (processor, ParamIDs::slotBypass (true, 0), 0.0f);

    PedalSlotComponent slot (processor, true, 0);
    slot.setSize (PedalSlotComponent::nominalWidth, slot.getPreferredHeight());
    slot.refresh();

    const auto active = render (slot);
    render (slot);
    CHECK_MSG (slot.getFaceRenderCount() == 1, "the pedal was drawn " + juce::String (slot.getFaceRenderCount()) + " times for two paints");

    const auto led = slot.getFaceLayout().led.getCentre().roundToInt();
    set (processor, ParamIDs::slotBypass (true, 0), 1.0f);
    slot.refresh();
    const auto bypassed = render (slot);
    CHECK (slot.getFaceRenderCount() == 2);
    CHECK_MSG (active.getPixelAt (led.x, led.y) != bypassed.getPixelAt (led.x, led.y), "the LED does not follow bypass");
}

/*  visual-polish.md 0.2 and 7: High contrast has no texture, sheen or gradient
    on the faces as they sit in the panels. */
LUTHIER_TEST (FacesIntegration, highContrastFacesAreFlat)
{
    PaletteScope palette;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    set (processor, ParamIDs::ampModel, (float) (int) AmpModel::FenderTweed);
    processor.getEngine().getPostEffects().setSlotType (0, PedalType::Fuzz);

    AmpFacePanel face (processor, AmpFacePanel::Style::section);
    face.setSize (234, AmpFacePanel::sectionHeight);
    face.refresh();

    PedalSlotComponent slot (processor, true, 0);
    slot.setSize (PedalSlotComponent::nominalWidth, slot.getPreferredHeight());
    slot.refresh();

    int textured[2] = {}, flat[2] = {};

    for (auto id : { PaletteId::defaultDark, PaletteId::highContrast })
    {
        palette.use (id);
        face.sendLookAndFeelChange();
        slot.sendLookAndFeelChange();

        auto* counts = id == PaletteId::highContrast ? flat : textured;
        counts[0] = distinctColours (render (face));
        counts[1] = distinctColours (render (slot));
    }

    CHECK_MSG (flat[0] * 3 < textured[0], "the amp face still looks textured in High contrast: " + juce::String (flat[0])
                                              + " colours against " + juce::String (textured[0]));
    CHECK_MSG (flat[1] * 3 < textured[1], "the pedal face still looks textured in High contrast: " + juce::String (flat[1])
                                              + " colours against " + juce::String (textured[1]));
}

/*  visual-polish.md 4: the valves glow with the drive the amp reports, and a
    reading that has stopped moving goes grey as stale. */
LUTHIER_TEST (FacesIntegration, theValvesGlowWithTheDriveAndGreyWhenStale)
{
    PaletteScope palette;
    palette.use (PaletteId::defaultDark);

    LuthierAudioProcessor processor;
    set (processor, ParamIDs::ampModel, (float) (int) AmpModel::MarshallPlexi);
    set (processor, ParamIDs::ampGain, 1.0f);
    set (processor, ParamIDs::ampMaster, 1.0f);
    set (processor, ParamIDs::ampStandby, 0.0f);
    processor.prepareToPlay (kSr, kBlock);

    AmpFacePanel face (processor, AmpFacePanel::Style::section);
    face.setSize (234, AmpFacePanel::sectionHeight);
    face.refresh();

    CHECK_MSG (face.getFaceState().drive == 0.0f && ! face.getFaceState().driveStale, "an amp that has played nothing glows");

    const auto vent = face.getFaceLayout().vent.getSmallestIntegerContainer();
    CHECK_MSG (! vent.isEmpty(), "the head has no vent");

    const auto idle = render (face);

    // A loud chord through the amp.
    juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), processor.getTotalNumInputChannels(), 2), kBlock);

    for (int b = 0; b < 40; ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (b == 0)
            for (int note : { 40, 47, 52, 55, 59, 64 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 127), 0);

        processor.processBlock (buffer, midi);
    }

    face.refresh();
    const float sag = (float) processor.getEngine().getAmpEngine().getSagAmount();

    CHECK_MSG (face.getFaceState().drive > 0.0f, "the valves do not glow with a sag of " + juce::String (sag, 4));
    CHECK_MSG (! face.getFaceState().driveStale, "a fresh reading is marked stale");

    const auto live = render (face);
    CHECK_MSG (differingPixels (idle, live, vent) > 0, "the vent looks the same idle and driven");

    // The audio stops; the reading holds still, and goes stale.
    juce::Thread::sleep (juce::roundToInt (AmpFacePanel::staleAfterSeconds * 1000.0) + 200);
    face.refresh();
    CHECK_MSG (face.getFaceState().driveStale, "a reading that stopped moving did not grey out");

    const auto stale = render (face);
    CHECK_MSG (differingPixels (live, stale, vent) > 0, "the stale vent looks the same as the live one");
    CHECK_MSG (face.getFaceRenderCount() == 1, "the glow redrew the face under it " + juce::String (face.getFaceRenderCount() - 1) + " times");
}

//==============================================================================
/*  For a person to look at: both windows with the faces in, in every palette. */
LUTHIER_TEST (FacesIntegration, rendersOfBothWindowsInEveryPalette)
{
    PaletteScope palette;
    juce::Array<juce::Image> advancedRows, easyRows;

    for (auto id : palettes)
    {
        palette.use (id);

        {
            LuthierAudioProcessor processor;
            processor.prepareToPlay (kSr, kBlock);
            set (processor, ParamIDs::ampModel, (float) (int) AmpModel::MarshallPlexi);
            fillRacks (processor);

            auto editor = openEditor (ctx, processor, 1600, 900, true);

            if (editor != nullptr)
            {
                CHECK (findFace (*editor, AmpFacePanel::Style::section) != nullptr);
                advancedRows.add (render (*editor));
            }
        }

        {
            LuthierAudioProcessor processor;
            processor.prepareToPlay (kSr, kBlock);
            set (processor, ParamIDs::ampModel, (float) (int) AmpModel::FenderDeluxe);
            fillRacks (processor);

            auto editor = openEditor (ctx, processor, LuthierAudioProcessorEditor::defaultWidth,
                                      LuthierAudioProcessorEditor::defaultHeight, false);

            if (editor != nullptr)
            {
                CHECK (findFace (*editor, AmpFacePanel::Style::card) != nullptr);
                easyRows.add (render (*editor));
            }
        }
    }

    CHECK (advancedRows.size() == 3 && easyRows.size() == 3);
    CHECK (savePng (stack (advancedRows), renderFolder().getChildFile ("_advanced_faces.png")));
    CHECK (savePng (stack (easyRows), renderFolder().getChildFile ("_easy_faces.png")));
}
