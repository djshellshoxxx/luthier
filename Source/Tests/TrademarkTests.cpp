/*  Shipped names avoid trademarks: factory-content.md 0.1, qa-polish.md 11.
    Tools/trademark_scan.py holds the same list for the source tree. */

#include "TestFramework.h"

#include "../PluginProcessor.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
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
            "Fuzz Face", "Klon", "Boss", "MXR", "Electro-Harmonix", "Uni-Vibe", "Leslie", "D'Addario", "NYXL",
            "Ernie Ball", "Elixir", "Selmer", "Dobro", "Gretsch", "Epiphone", "PRS", "Danelectro",
            "TransTrem", "Kinman", "Alnico Blue",
            "Fuzz Face", "LP Wiring"   // SPEC-SWEEP: FC-1
        };
        return list;
    }

    /** The brand a text uses, as a whole word (so "Telescope" is not "Tele"). */
    juce::String brandIn (const juce::String& text)
    {
        for (const auto& b : brands())
        {
            for (int at = text.indexOf (b); at >= 0; at = text.indexOf (at + 1, b))
            {
                const bool startOk = at == 0 || ! juce::CharacterFunctions::isLetter (text[at - 1]);
                const auto after = text[at + b.length()];
                const bool endOk = after == 0 || ! juce::CharacterFunctions::isLowerCase (after);

                if (startOk && endOk)
                    return b;
            }
        }

        return {};
    }

    void checkList (TestContext& ctx, const juce::String& what, const juce::StringArray& names)
    {
        for (const auto& n : names)
        {
            const auto brand = brandIn (n);
            CHECK_MSG (brand.isEmpty(), what + " \"" + n + "\" uses the trademark " + brand);
        }
    }
}

LUTHIER_TEST (Trademarks, noChoiceListNamesABrand)
{
    checkList (ctx, "guitar type", Parameters::guitarTypeNames());
    checkList (ctx, "amp", Parameters::ampModelNames());
    checkList (ctx, "cabinet", Parameters::cabinetNames());
    checkList (ctx, "speaker", Parameters::speakerNames());
    checkList (ctx, "microphone", Parameters::micNames());
    checkList (ctx, "pedal", Parameters::pedalTypeNames());
    checkList (ctx, "bridge type", Parameters::bridgeTypeNames());
    checkList (ctx, "treble bleed", Parameters::trebleBleedNames());
    checkList (ctx, "slide mode", Parameters::slideModeNames());
    checkList (ctx, "pickup type", Parameters::pickupTypeNames());
    checkList (ctx, "pick material", Parameters::pickMaterialNames());
}

LUTHIER_TEST (Trademarks, noFactoryPresetPartOrGuitarNamesABrand)
{
    LuthierAudioProcessor processor;
    auto& presets = processor.getPresetManager();

    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (const auto* info = presets.getPreset (i); info != nullptr && info->isFactory)
            checkList (ctx, "preset", { info->name, info->description });

    for (int t = 0; t < (int) PartType::numTypes; ++t)
        for (const auto& part : processor.getPartLibrary().getParts ((PartType) t))
            if (part->isFactory)
            {
                checkList (ctx, "part", { part->name });

                // Field values are shown in the Workshop inspector too.
                if (auto* object = part->fields.getDynamicObject())
                    for (auto& prop : object->getProperties())
                        if (prop.value.isString())
                            checkList (ctx, "part field", { prop.value.toString() });
            }

    for (const auto& file : processor.getPartLibrary().getGuitarFiles())
        checkList (ctx, "guitar", { file.getFileNameWithoutExtension() });
}

LUTHIER_TEST (Trademarks, oldNamesStillLoad)
{
    // Sessions saved before the rename keep their parts and guitars.
    LuthierAudioProcessor processor;
    auto& library = processor.getPartLibrary();

    const auto bridge = library.resolve ("Factory/Bridges/ABR-1 Tune-o-Matic.luthierpart", PartType::bridge);
    CHECK (bridge != nullptr && bridge->name == "Vintage Adjustable Bridge");

    const auto tuners = library.resolve ("Factory/Tuners/Kluson 15 to 1.luthierpart", PartType::tuners);
    CHECK (tuners != nullptr && tuners->name == "Vintage Keystone 15 to 1");

    CHECK (PartLibrary::renamedFactoryGuitar ("Acoustic/Selmer-Style.luthierguitar") == "Acoustic/Gypsy Jazz.luthierguitar");

    // SPEC-SWEEP: FC-1.
    const auto wiring = library.resolve ("Factory/Wiring/Modern LP Wiring.luthierpart", PartType::wiring);
    CHECK_MSG (wiring != nullptr && wiring->name == "Modern Single-Cut Wiring",
               "a guitar saved with the old single-cut wiring name lost its wiring");

    auto& presets = processor.getPresetManager();
    const int renamed = presets.indexOfPreset ("Fuzz Face Lead");
    CHECK_MSG (renamed >= 0 && presets.getPreset (renamed)->name == "Germanium Fuzz Lead",
               "the old fuzz preset name no longer finds the preset");
}

/*  Tools/trademark_scan.py's source check, in the suite so a regression fails
    the build rather than waiting for someone to run the script: string literals
    in Source/ (not tests, not comments, not lines marked "legacy name"). */
LUTHIER_TEST (Trademarks, sourceTreeHasNoUnmarkedBrandNames)
{
    const auto source = juce::File (__FILE__).getParentDirectory().getParentDirectory();

    if (! source.getChildFile ("PluginProcessor.cpp").existsAsFile())
        return;   // built from somewhere the sources are not

    juce::StringArray hits;

    for (const auto& entry : juce::RangedDirectoryIterator (source, true, "*.cpp;*.h"))
    {
        const auto file = entry.getFile();

        if (file.getFullPathName().contains ("Tests"))
            continue;

        juce::StringArray lines;
        lines.addLines (file.loadFileAsString());

        for (int n = 0; n < lines.size(); ++n)
        {
            const auto line = lines[n];
            const auto trimmed = line.trimStart();

            if (trimmed.startsWith ("//") || trimmed.startsWith ("*") || trimmed.startsWith ("/*")
                  || line.contains ("legacy name"))
                continue;

            // Each "..." literal on the line.
            for (int at = line.indexOfChar ('"'); at >= 0;)
            {
                int end = at + 1;

                while (end < line.length() && ! (line[end] == '"' && line[end - 1] != '\\'))
                    ++end;

                if (end >= line.length())
                    break;

                const auto literal = line.substring (at + 1, end);
                const auto brand = brandIn (literal);

                if (brand.isNotEmpty())
                    hits.add (file.getRelativePathFrom (source) + ":" + juce::String (n + 1) + " " + brand);

                at = line.indexOfChar (end + 1, '"');
            }
        }
    }

    CHECK_MSG (hits.isEmpty(), "brand names in shipped strings: " + hits.joinIntoString ("; "));
}
