/*  The guitar illustration's remainder (TODO G, VISUAL-WORKSHOP-QA):
    guitar-illustration.md 12.3 (amp defaults per family), 1 (zoom and pan),
    15 (preset-browser thumbnails on a worker with a 200-entry cache), 16
    (reduced-motion rules), 19 (the played-note dot's 60 ms timing), and 6
    (every headstock style drawable). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../DSP/Amp/AmpEngine.h"
#include "../Workshop/FamilyDefaults.h"
#include "../UI/WorkshopPanel.h"
#include "../UI/GuitarBodyComponent.h"
#include "../UI/Guitar/GuitarRenderer.h"
#include "../UI/Guitar/HeadstockOutlines.h"
#include "../Accessibility/Accessibility.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    float plain (LuthierAudioProcessor& p, const char* id)
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id));
        return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
    }

    void setPlain (LuthierAudioProcessor& p, const char* id, float value)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id)))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
        p.getParameterBridge().applyAllNow();
    }
}

//==============================================================================
/*  12.3: the amp follows the family when it does not suit it - a bass gets a
    bass amp, an acoustic a clean DI and some room, an electric back from a DI
    gets a guitar amp - and 12.4: a guitar amp that suits is kept. */
LUTHIER_TEST (FamilySwitch, theAmpFollowsTheFamilyOnlyWhenItDoesNotSuit)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    setPlain (processor, ParamIDs::guitarType, (float) (int) GuitarType::Stratocaster);
    setPlain (processor, ParamIDs::ampModel, (float) (int) AmpModel::MarshallPlexi);

    CHECK (processor.switchGuitarFamily ("bass"));
    CHECK (juce::roundToInt (plain (processor, ParamIDs::ampModel)) == (int) AmpModel::AmpegSVT);
    CHECK (processor.takeGuitarNotices().joinIntoString (" ").contains ("Also set: amp"));

    setPlain (processor, ParamIDs::roomOn, 0.0f);
    CHECK (processor.switchGuitarFamily ("acoustic"));
    CHECK (juce::roundToInt (plain (processor, ParamIDs::ampModel)) == (int) AmpModel::AcousticDI);
    CHECK (plain (processor, ParamIDs::roomOn) > 0.5f);
    CHECK (plain (processor, ParamIDs::roomBlend) >= 0.15f);

    CHECK (processor.switchGuitarFamily ("electric"));
    CHECK (juce::roundToInt (plain (processor, ParamIDs::ampModel)) == (int) AmpModel::FenderTwin);

    // A guitar amp that suits stays (12.4).
    setPlain (processor, ParamIDs::ampModel, (float) (int) AmpModel::VoxAC30);
    CHECK (processor.switchGuitarFamily ("bass"));
    CHECK (processor.switchGuitarFamily ("electric"));
    CHECK (juce::roundToInt (plain (processor, ParamIDs::ampModel)) == (int) AmpModel::FenderTwin);   // back from the bass amp

    setPlain (processor, ParamIDs::ampModel, (float) (int) AmpModel::VoxAC30);
    CHECK (FamilyDefaults::ampModelFor ("electric", (int) AmpModel::VoxAC30) == -1);

    // One undo puts the family and the amp back together.
    CHECK (processor.switchGuitarFamily ("bass"));
    processor.undo();
    processor.getParameterBridge().applyAllNow();
    CHECK (juce::roundToInt (plain (processor, ParamIDs::ampModel)) == (int) AmpModel::VoxAC30);
    CHECK (processor.getCurrentGuitar().family == "electric");
}

//==============================================================================
/*  guitar-illustration.md 15 / 19: thumbnails render on the worker, cold under
    100 ms, a cache hit under 1 ms, the cache holds 200 and evicts the least
    recently used; every factory preset gets its guitar's picture. */
LUTHIER_TEST (Thumbnails, workerRendersCachesAndEvictsAt200)
{
    PartLibrary library;
    WorkshopGuitar guitar;
    PartLibrary::LoadReport report;
    CHECK (library.loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Single-Cut.luthierguitar"),
                               guitar, report));

    // Cold: the render itself, best of three (a first-ever call pays for fonts and allocations).
    double cold = 1.0e9;
    for (int i = 0; i < 3; ++i)
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto image = GuitarThumbnails::render (guitar);
        cold = juce::jmin (cold, juce::Time::getMillisecondCounterHiRes() - t0);
        CHECK (image.getWidth() == GuitarThumbnails::kWidth && image.getHeight() == GuitarThumbnails::kHeight);
    }
    // Machine-relative wall-clock budget: enforced only under LUTHIER_PERF=1 (the nightly).
    if (luthier::tests::perfRunRequested())
        CHECK_MSG (cold < 100.0, "a cold thumbnail took " + juce::String (cold, 1) + " ms");

    GuitarThumbnails thumbnails;
    CHECK (! thumbnails.get (guitar).isValid());          // queued, not rendered on this thread
    CHECK (thumbnails.waitUntilIdle (4000));

    const auto t1 = juce::Time::getMillisecondCounterHiRes();
    const auto hit = thumbnails.get (guitar);
    const double hitMs = juce::Time::getMillisecondCounterHiRes() - t1;
    CHECK (hit.isValid());
    // Machine-relative wall-clock budget: enforced only under LUTHIER_PERF=1 (the nightly).
    if (luthier::tests::perfRunRequested())
        CHECK_MSG (hitMs < 1.0, "a cache hit took " + juce::String (hitMs, 3) + " ms");

    // 200 distinct guitars fill it; the 201st evicts the least recently used (the first).
    const auto firstKey = GuitarThumbnails::keyFor (guitar);
    auto variant = guitar;

    for (int i = 1; i <= GuitarThumbnails::kCapacity; ++i)
    {
        variant.finish.aging = 0.5 + (double) i / 1000.0;   // none equal to the first guitar's
        thumbnails.get (variant);

        if (i % 25 == 0)
            CHECK (thumbnails.waitUntilIdle (20000));
    }

    CHECK (thumbnails.waitUntilIdle (20000));
    CHECK (thumbnails.getCacheSize() == GuitarThumbnails::kCapacity);
    CHECK_MSG (! thumbnails.isCached (firstKey), "the least recently used thumbnail was not evicted");
}

LUTHIER_TEST (Thumbnails, everyFactoryPresetShowsItsGuitar)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    auto& presets = processor.getPresetManager();

    GuitarThumbnails thumbnails;
    int factory = 0;

    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (const auto* info = presets.getPreset (i); info != nullptr && info->isFactory)
        {
            ++factory;
            thumbnails.getForPreset (info->file);
        }

    CHECK (factory > 10);
    CHECK (thumbnails.waitUntilIdle (60000));

    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (const auto* info = presets.getPreset (i); info != nullptr && info->isFactory)
            CHECK_MSG (thumbnails.getForPreset (info->file).isValid(), info->name + " has no thumbnail");

    // A hundred presets on one guitar share one image: far fewer images than presets.
    CHECK (thumbnails.getCacheSize() <= factory);
}

//==============================================================================
namespace
{
    juce::Image renderComponent (juce::Component& c)
    {
        juce::Image image (juce::Image::ARGB, juce::jmax (1, c.getWidth()), juce::jmax (1, c.getHeight()), true,
                           juce::SoftwareImageType());
        juce::Graphics g (image);
        c.paintEntireComponent (g, true);
        return image;
    }

    juce::uint64 digestOf (const juce::Image& image)
    {
        juce::uint64 sum = 0;
        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < pixels.height; ++y)
            for (int x = 0; x < pixels.width; ++x)
                sum += (juce::uint64) pixels.getPixelColour (x, y).getARGB() * (juce::uint64) (x + 1);
        return sum;
    }

    struct ReducedMotionScope
    {
        bool was = AccessibilitySettings::get().isReducedMotion();
        explicit ReducedMotionScope (bool on) { AccessibilitySettings::get().setReducedMotion (on); }
        ~ReducedMotionScope() { AccessibilitySettings::get().setReducedMotion (was); }
    };
}

/*  19: "played-note dot appears within 60 ms of the audio note-on, decays over
    60 ms". The dot is on at the first frame that sees the note, frames come at
    30 Hz or faster, and after the note it fades to nothing across 60 ms. */
LUTHIER_TEST (NoteDots, theDotAppearsWithin60msAndFadesOver60ms)
{
    // The timing model, with a clock.
    NoteDots dots;
    CHECK (dots.update (0, 0.8f, 5.0f, 1000.0, false));
    CHECK (dots.alpha[0] == 1.0f && dots.fret[0] == 5.0f);

    dots.update (0, 0.0f, 5.0f, 1100.0, false);   // the first frame after the note
    CHECK_NEAR (dots.alpha[0], 1.0f, 1.0e-6);
    dots.update (0, 0.0f, 5.0f, 1130.0, false);
    CHECK_NEAR (dots.alpha[0], 0.5f, 1.0e-3);
    dots.update (0, 0.0f, 5.0f, 1160.0, false);
    CHECK (dots.alpha[0] == 0.0f);

    // Reduced motion: on or off, never between (16).
    NoteDots still;
    still.update (0, 0.8f, 3.0f, 0.0, true);
    still.update (0, 0.0f, 3.0f, 10.0, true);
    CHECK (still.alpha[0] == 0.0f);

    // Through the plugin: a fretted note played, one frame, the dot is on.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    GuitarBodyComponent body (processor);
    body.setSize (800, 360);

    // Frames at 30 Hz or faster, so the first frame after a note is under 60 ms away.
    CHECK_MSG (body.getFrameIntervalMs() > 0 && body.getFrameIntervalMs() <= 34,
               "frames every " + juce::String (body.getFrameIntervalMs()) + " ms");

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.9f), 0);   // A4: a fretted note
    processor.processBlock (buffer, midi);

    body.updateLiveOverlay (0.0);
    bool anyDot = false;
    for (int s = 0; s < 12; ++s)
        anyDot = anyDot || (body.getOverlay().dotAlpha[(size_t) s] == 1.0f && body.getOverlay().dotFret[(size_t) s] > 0.0f);
    CHECK_MSG (anyDot, "no dot on the first frame after the note-on");
}

/*  12.1 and 16: a family change crossfades over 250 ms; under reduced motion
    there are no animation frames - the change is instant, the changed parts are
    outlined, and two renders apart in time are identical. */
LUTHIER_TEST (ReducedMotion, aGuitarChangeCrossfadesOrIsStaticWithAnOutline)
{
    for (bool reduced : { false, true })
    {
        ReducedMotionScope scope (reduced);
        LuthierAudioProcessor processor;
        AccessibilitySettings::get().setReducedMotion (reduced);   // the processor reloads the user's settings
        processor.prepareToPlay (48000.0, 512);
        GuitarBodyComponent body (processor);
        body.setSize (800, 360);
        renderComponent (body);   // the old picture exists to fade from

        CHECK (processor.switchGuitarFamily ("bass"));
        body.checkForGuitarChange();

        const double now = juce::Time::getMillisecondCounterHiRes();

        if (! reduced)
        {
            CHECK_MSG (body.isAnimating (now), "no crossfade after a family change");
            CHECK (! body.isAnimating (now + SceneCrossfade::kMs + 10.0));
        }
        else
        {
            CHECK_MSG (! body.isAnimating (now), "reduced motion still animates");

            bool outlined = false;
            for (auto c : body.getOverlay().changed)
                outlined = outlined || c;
            CHECK_MSG (outlined, "reduced motion changed the guitar without outlining what changed");

            body.updateLiveOverlay (now);
            const auto a = digestOf (renderComponent (body));
            body.updateLiveOverlay (now + 120.0);
            const auto b = digestOf (renderComponent (body));
            CHECK_MSG (a == b, "two frames 120 ms apart differ under reduced motion");
        }
    }
}

/*  The bench too: a committed swap crossfades, a drag does not, and reduced
    motion outlines instead. */
LUTHIER_TEST (ReducedMotion, theBenchFadesCommittedChangesOnly)
{
    for (bool reduced : { false, true })
    {
        ReducedMotionScope scope (reduced);
        LuthierAudioProcessor processor;
        AccessibilitySettings::get().setReducedMotion (reduced);   // the processor reloads the user's settings
        processor.prepareToPlay (48000.0, 512);
        WorkshopPanel panel (processor);
        panel.setVisible (true);
        panel.setSize (1200, 760);
        auto& ill = panel.getIllustration();

        panel.showCategory ("Bridge");
        int other = -1;
        for (int i = 0; i < panel.getDrawerParts().size(); ++i)
            if (panel.getDrawerParts()[i]->name != processor.getCurrentGuitar().get (GuitarSlot::bridge)->name)
                other = i;

        CHECK (other >= 0);
        if (other < 0)
            continue;

        panel.clickCard (other);
        ill.refresh();

        CHECK (ill.isCrossfading() == ! reduced);
        CHECK (ill.isOutliningChanges() == reduced);
    }
}

//==============================================================================
/*  1: "The whole guitar always fits at fit zoom. User can zoom in with
    Ctrl-scroll up to 4x. Pan follows." The millimetre under the pointer stays
    under it; zooming back out to 1x recentres. */
LUTHIER_TEST (BenchZoom, ctrlScrollZoomsToFourTimesAboutThePointer)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    WorkshopPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1200, 760);
    auto& ill = panel.getIllustration();

    auto source = juce::Desktop::getInstance().getMainMouseSource();
    auto wheelAt = [&] (juce::Point<float> p, float delta)
    {
        const juce::MouseEvent e (source, p, juce::ModifierKeys::commandModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                  &ill, &ill, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false);
        juce::MouseWheelDetails wheel {};
        wheel.deltaY = delta;
        ill.mouseWheelMove (e, wheel);
    };

    // Fit: the whole guitar inside the view.
    const auto fitBounds = juce::Rectangle<float> (ill.toPx (ill.getScene().bounds.getTopLeft()),
                                                   ill.toPx (ill.getScene().bounds.getBottomRight()));
    CHECK (ill.getLocalBounds().toFloat().expanded (1.0f).contains (fitBounds));

    const juce::Point<float> pointer { ill.getWidth() * 0.3f, ill.getHeight() * 0.45f };
    const auto under = ill.toMm (pointer);

    wheelAt (pointer, 1.0f);
    CHECK (ill.getZoom() > 1.0f);
    CHECK (ill.toPx (under).getDistanceFrom (pointer) < 1.0f);

    for (int i = 0; i < 30; ++i)
        wheelAt (pointer, 1.0f);

    CHECK_NEAR (ill.getZoom(), 4.0f, 1.0e-4);
    CHECK (ill.toPx (under).getDistanceFrom (pointer) < 1.5f);

    for (int i = 0; i < 40; ++i)
        wheelAt (pointer, -1.0f);

    CHECK_NEAR (ill.getZoom(), 1.0f, 1.0e-4);
    const auto refit = juce::Rectangle<float> (ill.toPx (ill.getScene().bounds.getTopLeft()),
                                               ill.toPx (ill.getScene().bounds.getBottomRight()));
    CHECK (refit.getCentre().getDistanceFrom (fitBounds.getCentre()) < 0.5f);
}

/*  6: every headstock layout the spec names has a drawable style, and each
    factory guitar's headstock outline is closed and carries a post per string. */
LUTHIER_TEST (Headstocks, everyLayoutIsDrawableAndEveryGuitarHasItsPosts)
{
    using namespace outlines;

    for (auto layout : { HeadLayout::threeThree, HeadLayout::inline6, HeadLayout::inlineReverse, HeadLayout::fourInline,
                         HeadLayout::twoTwo, HeadLayout::slotted, HeadLayout::sixSix, HeadLayout::threeOne, HeadLayout::headless })
    {
        bool found = false;

        for (int i = 0; i < kNumHeadstockStyles; ++i)
            found = found || kHeadstockStyles[i].layout == layout;

        CHECK_MSG (found, "no headstock style for layout " + juce::String ((int) layout));
    }

    PartLibrary library;

    for (const auto& file : library.getGuitarFiles())
    {
        WorkshopGuitar g;
        PartLibrary::LoadReport report;

        if (! library.loadGuitar (file, g, report))
            continue;

        const auto scene = GuitarRenderer::build (g);
        CHECK_MSG (scene.headstockStyle.isNotEmpty() || g.get (GuitarSlot::neck)->text ("headstock").contains ("headless"),
                   file.getFileName() + " drew no headstock");

        for (auto& s : scene.strings)
            CHECK_MSG (s.post != s.nut, file.getFileName() + ": string " + juce::String (s.index + 1) + " has no tuner post");
    }
}
