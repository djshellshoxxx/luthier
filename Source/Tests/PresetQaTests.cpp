/*  qa-polish.md 2.6 / 3.13 and installer.md 8: the preset round trip to the
    ulp, every-byte corruption, the migration banner and the backup path. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../Presets/FactoryPresets.h"

#include <cstring>
#include <limits>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    juce::String paramIdOf (juce::AudioProcessorParameter* p)
    {
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            return withId->paramID;

        return {};
    }

    /** Distance in units in the last place between two finite floats. */
    long ulpDistance (float a, float b)
    {
        auto ordered = [] (float f)
        {
            juce::int32 i;
            std::memcpy (&i, &f, sizeof (i));
            return i < 0 ? (long) std::numeric_limits<juce::int32>::min() - (long) i : (long) i;
        };

        return std::abs (ordered (a) - ordered (b));
    }
}

//==============================================================================
/*  qa-polish.md 2.6: each factory preset, saved and reloaded, restores every
    parameter with no drift beyond one float ulp ("any parameter drift > float
    ulp fails"). JUCE's float parameters hold the plain value, so a normalised
    value through a skewed range comes back within an ulp, not always bit for
    bit (the morph position is never carried, ambiguity-
    resolutions 5). Everything is scrambled in between, so a parameter that the
    load forgot to write shows up rather than keeping its old value by luck. */
LUTHIER_TEST (Presets, everyFactoryPresetRoundTripsToTheUlp)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    FactoryPresets::setProcessorForRanges (processor.get());
    processor->prepareToPlay (kSr, kBlock);

    auto& presets = processor->getPresetManager();
    juce::Random rng (0x2601);
    int failures = 0;
    juce::String first;

    for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
    {
        const auto& def = FactoryPresets::getPreset (i);
        CHECK_MSG (presets.fromVar (FactoryPresets::toVar (def, *processor)), juce::String (def.name) + " did not load");

        presets.captureExtraState();
        const auto saved = presets.toVar (def.name, def.category);

        juce::Array<float> before;

        for (auto* p : processor->getParameters())
            before.add (p->getValue());

        for (auto* p : processor->getParameters())
            p->setValueNotifyingHost (rng.nextFloat());

        CHECK_MSG (presets.fromVar (saved), juce::String (def.name) + " did not reload");

        int index = 0;

        for (auto* p : processor->getParameters())
        {
            const auto then = before[index++];

            if (paramIdOf (p) == ParamIDs::presetMorphPosition)
                continue;

            if (ulpDistance (p->getValue(), then) > 1)
            {
                if (failures++ == 0)
                    first = juce::String (def.name) + ": " + paramIdOf (p) + " " + juce::String (then, 9)
                              + " -> " + juce::String (p->getValue(), 9);
            }
        }
    }

    CHECK_MSG (failures == 0, juce::String (failures) + " parameter values changed, first " + first);
}

//==============================================================================
/*  qa-polish.md 3.13: every byte of a serialised factory preset flipped in turn.
    Each either refuses or loads; nothing crashes and no parameter leaves [0, 1]. */
LUTHIER_TEST (Presets, everyByteOfAFactoryPresetFlippedIsRefusedOrLoads)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    FactoryPresets::setProcessorForRanges (processor.get());
    processor->prepareToPlay (kSr, kBlock);

    auto& presets = processor->getPresetManager();

    // The smallest factory file on disk is the fullest test per second: the
    // flip loop is quadratic in the size (every flip reparses the whole file).
    const auto& def = FactoryPresets::getPreset (0);
    presets.fromVar (FactoryPresets::toVar (def, *processor));
    presets.captureExtraState();

    const auto original = juce::JSON::toString (presets.toVar (def.name, def.category), true).toStdString();
    CHECK (! original.empty());

    int refused = 0, loaded = 0, bad = 0;

    for (size_t i = 0; i < original.size(); ++i)
    {
        auto damaged = original;
        damaged[i] = (char) ((unsigned char) damaged[i] ^ 0xFF);

        const auto parsed = juce::JSON::parse (juce::String::fromUTF8 (damaged.data(), (int) damaged.size()));

        if (! parsed.isObject() || ! presets.fromVar (parsed))
        {
            ++refused;
            continue;
        }

        ++loaded;

        for (auto* p : processor->getParameters())
        {
            const auto v = p->getValue();

            if (! (std::isfinite (v) && v >= 0.0f && v <= 1.0f))
                ++bad;
        }
    }

    CHECK ((size_t) (refused + loaded) == original.size());
    CHECK_MSG (bad == 0, juce::String (bad) + " parameter values left [0, 1] after a flipped load");
    CHECK_MSG (refused > 0, "no flip was refused, so the loader validates nothing");
}

//==============================================================================
/*  installer.md 8: the version a save replaces goes to
    <presets root>/Backup/<yyyy-mm-dd>/, not beside the file. */
LUTHIER_TEST (Presets, backupsGoToThePresetsRootBackupFolder)
{
    auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                  .getChildFile ("LuthierBackupRootTest");
    root.deleteRecursively();

    const auto presetsRoot = root.getChildFile ("Presets");
    auto target = presetsRoot.getChildFile ("User/Clean/Mine").withFileExtension (PresetManager::kFileExtension);
    target.getParentDirectory().createDirectory();
    target.replaceWithText ("{\"magic\":\"luthier.preset\",\"schemaVersion\":1,\"name\":\"first\"}");

    CHECK (PresetManager::backupFolderFor (target) == presetsRoot.getChildFile ("Backup"));

    PresetManager::backupBeforeOverwrite (target);

    const auto today = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
    CHECK_MSG (presetsRoot.getChildFile ("Backup").getChildFile (today).getChildFile (target.getFileName()).existsAsFile(),
               "the backup is not in Presets/Backup/<date>/");
    CHECK (! target.getParentDirectory().getChildFile ("Backup").exists());

    root.deleteRecursively();
}

//==============================================================================
/*  installer.md 8: "User sees a subtle info banner on the first affected
    load" - one, however many old presets follow. */
LUTHIER_TEST (Editor, aMigratedPresetRaisesOneInfoBanner)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
    auto* window = dynamic_cast<LuthierAudioProcessorEditor*> (editor.get());
    CHECK (window != nullptr);

    if (window == nullptr)
        return;

    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    auto& centre = window->getNotifications();
    window->pollForNotifications();
    centre.clear();

    // A current preset migrates nothing.
    auto& presets = processor->getPresetManager();
    presets.captureExtraState();
    CHECK (presets.fromVar (presets.toVar ("Current", "Test")));
    window->pollForNotifications();
    CHECK_MSG (! centre.contains ("migrated"), "a current preset raised the migration banner");

    // One from an older version, with no ranges block and the retired
    // feedback switch.
    auto old = presets.toVar ("Old", "Test");
    auto* obj = old.getDynamicObject();
    obj->removeProperty ("ranges");
    obj->setProperty ("pluginVersion", "0.9.0");

    CHECK (presets.fromVar (old));
    CHECK (presets.getLastMigration().contains ("ranges"));
    window->pollForNotifications();
    CHECK_MSG (centre.contains ("migrated"), "a migrated load raised no banner");

    while (centre.isShowingNotification())
        centre.dismissCurrent();

    CHECK (presets.fromVar (old));
    window->pollForNotifications();
    CHECK_MSG (! centre.contains ("migrated"), "a second migrated load raised a second banner");
}
