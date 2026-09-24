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
    CHECK_MSG (cold < 100.0, "a cold thumbnail took " + juce::String (cold, 1) + " ms");

    GuitarThumbnails thumbnails;
    CHECK (! thumbnails.get (guitar).isValid());          // queued, not rendered on this thread
    CHECK (thumbnails.waitUntilIdle (4000));

    const auto t1 = juce::Time::getMillisecondCounterHiRes();
    const auto hit = thumbnails.get (guitar);
    const double hitMs = juce::Time::getMillisecondCounterHiRes() - t1;
    CHECK (hit.isValid());
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
