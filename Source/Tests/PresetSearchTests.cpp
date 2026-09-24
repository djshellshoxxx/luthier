/*  preset-browser-previews.md 16: search (PB-20 to PB-24), the library prefs
    they filter on, and the performance budget (PB-27).
*/

#include "PresetBrowserTestHelpers.h"
#include "../Presets/Search/PresetSearch.h"
#include "../Presets/PresetLibraryPrefs.h"
#include "../Presets/Preview/PreviewPlayer.h"

using namespace luthier;
using namespace luthier::tests;
using namespace luthier::tests::browser;

namespace
{
    struct Fixture
    {
        Fixture()
        {
            prefsFile = scratch.folder.getChildFile ("preset-library.json");
            PresetLibraryPrefs::get().setFile (prefsFile);
            fillIndexFromCorpus (index, renderer.getRangeSource());
        }

        ~Fixture() { PresetLibraryPrefs::get().setFile ({}); }

        std::vector<PresetSearch::Result> query (const juce::String& text, PresetSearch::Filters f = {},
                                                 PresetSearch::Sort sort = PresetSearch::Sort::relevance)
        {
            return PresetSearch::run (index, text, f, sort, PresetLibraryPrefs::get());
        }

        juce::StringArray names (const std::vector<PresetSearch::Result>& results) const
        {
            juce::StringArray n;

            for (const auto& r : results)
                n.add (index[r.entry].info.name);

            return n;
        }

        const PresetIndex::Entry* entry (const juce::String& name) const
        {
            const int i = index.indexOfName (name);
            return i >= 0 ? &index[i] : nullptr;
        }

        ScratchFolder scratch;
        juce::File prefsFile;
        PreviewRenderer renderer;
        PresetIndex index;
    };
}

//==============================================================================
/*  Not a requirement: prints what the corpus measured, for tuning the rules. */
LUTHIER_TEST (PresetSearchDiagnostics, dumpCorpus)
{
    Fixture f;

    for (const auto& e : f.index.getEntries())
    {
        std::cout << e.info.name.paddedRight (' ', 26)
                  << " fam " << e.params.family << " drive " << juce::String (e.params.drive, 2)
                  << " rev " << juce::String (e.params.reverb, 2) << " dly " << juce::String (e.params.delay, 2)
                  << " cmp " << juce::String (e.params.compression, 2)
                  << " c " << juce::String (e.tone.centroidLog2Hz, 2) << " hi " << juce::String (e.tone.highBand, 3)
                  << " fl " << juce::String (e.tone.flatness, 3) << " tail " << juce::String (e.tone.tailSeconds, 2)
                  << " atk " << juce::String (e.tone.attack, 1) << " crest " << juce::String (e.tone.crestDb, 1)
                  << " | " << e.descriptors.joinIntoString (",") << " | " << e.genres.joinIntoString (",")
                  << std::endl;
    }

    CHECK (f.index.size() == FactoryPresets::getNumPresets());

    for (auto* name : { "8-String Djent", "Modern Metal Chug", "Drop C Riff" })
        if (auto* r = factoryCorpus().find (name))
        {
            juce::String line;
            const auto& clip = r->result.clip;

            for (int ms = 2600; ms < clip.getNumSamples() / 48; ms += 50)
                line << juce::String (juce::Decibels::gainToDecibels (clip.getMagnitude (ms * 48, 480)), 0) << " ";

            std::cout << name << ": " << line << std::endl;
        }
}

//==============================================================================
/*  PB-20: the factory descriptors. */
LUTHIER_TEST (PresetSearch, PB20_factoryDescriptors)
{
    Fixture f;

    auto has = [&] (const char* name, const char* descriptor)
    {
        const auto* e = f.entry (name);
        const bool ok = e != nullptr && e->descriptors.contains (descriptor);

        if (! ok)
            ctx.fail (juce::String (name) + " is not " + descriptor + " ("
                        + (e != nullptr ? e->descriptors.joinIntoString (",") : juce::String ("missing")) + ")");

        ++ctx.checks;
    };

    has ("Jazz Hollowbody", "warm");
    has ("Jazz Hollowbody", "clean");
    has ("Jazz Hollowbody", "jazz");
    has ("8-String Djent", "high-gain");
    has ("8-String Djent", "djent");
    has ("Ambient Swell", "spacious");
    has ("Semi-Hollow Chime", "bright");
    has ("Single-Cut Crunch", "crunchy");

    for (const auto& e : f.index.getEntries())
        if (e.params.family == PresetFeatures::bass)
            CHECK_MSG (! e.descriptors.contains ("djent"), e.info.name + " is a djent bass");

    // Descriptors are derived data: never in a preset file (5.4).
    for (const auto& r : factoryCorpus().bank)
        CHECK (! juce::JSON::toString (r.json).contains ("\"descriptors\""));
}

/*  PB-21: queries. */
LUTHIER_TEST (PresetSearch, PB21_queries)
{
    Fixture f;

    const auto warmClean = f.query ("warm clean");
    const auto warmNames = f.names (warmClean);
    CHECK_MSG (warmNames.indexOf ("Jazz Hollowbody") >= 0 && warmNames.indexOf ("Jazz Hollowbody") < 3,
               warmNames.joinIntoString (", "));

    for (const auto& r : warmClean)
        CHECK_MSG (f.index[r.entry].params.drive <= 0.5, f.index[r.entry].info.name);

    const auto djent = f.names (f.query ("djent"));
    CHECK_MSG (djent.size() > 0 && djent[0] == "8-String Djent", djent.joinIntoString (", "));

    auto sorted = [] (juce::StringArray a) { a.sort (true); return a; };

    const auto crunchy = f.names (f.query ("crunchy"));
    CHECK (crunchy.size() > 0);
    CHECK_MSG (sorted (f.names (f.query ("cruchy"))) == sorted (crunchy),
               f.names (f.query ("cruchy")).joinIntoString (", ") + " vs " + crunchy.joinIntoString (", "));

    CHECK (sorted (f.names (f.query ("clean jazz"))) == sorted (f.names (f.query ("jazz clean"))));
    CHECK (f.query ("clean jazz").size() > 0);

    // Multi-word vocabulary and quoted phrases are single tokens (6.4).
    CHECK (PresetSearch::tokenise ("edge of breakup").size() == 1);
    CHECK (PresetSearch::tokenise ("\"single cut\" crunch").size() == 2);
    CHECK (f.names (f.query ("\"single cut\"")).contains ("Single-Cut Crunch"));

    // A localised descriptor word matches as well as the English one (10).
    CHECK (f.query ("mellow").size() == f.query ("warm").size());
}

/*  PB-22: author, guitar and amp matches; a name match outranks a
    description-only match. */
LUTHIER_TEST (PresetSearch, PB22_fieldsAndWeights)
{
    Fixture f;

    // Every factory preset's author is "Luthier Audio".
    CHECK (f.query ("luthier").size() == (size_t) f.index.size());

    // A guitar name and an amp name.
    const auto* chime = f.entry ("Semi-Hollow Chime");
    CHECK (chime != nullptr);

    if (chime != nullptr)
    {
        const auto guitarWord = PresetIndex::wordsOf (chime->params.guitarName)[0];
        CHECK_MSG (f.names (f.query (guitarWord)).contains ("Semi-Hollow Chime"), guitarWord);

        const auto ampWord = PresetIndex::wordsOf (chime->params.ampName).joinIntoString (" ");
        CHECK_MSG (f.names (f.query ("\"" + ampWord + "\"")).contains ("Semi-Hollow Chime"), ampWord);
    }

    // A synthetic pair: the same word in one's name and the other's description.
    PresetIndex::Entry byName;
    byName.info.name = "Bench Wobble";
    byName.key = "test:name";
    byName.parsed = true;
    f.index.addEntryForTesting (byName);

    PresetIndex::Entry byDescription;
    byDescription.info.name = "Something Else";
    byDescription.info.description = "A bench wobble of a sound.";
    byDescription.key = "test:description";
    byDescription.parsed = true;
    f.index.addEntryForTesting (byDescription);

    const auto wobble = f.names (f.query ("wobble"));
    CHECK_MSG (wobble.size() == 2 && wobble[0] == "Bench Wobble", wobble.joinIntoString (", "));
}

/*  PB-23: filters. */
LUTHIER_TEST (PresetSearch, PB23_filters)
{
    Fixture f;

    // Family = Bass AND Genre = funk.
    PresetSearch::Filters bassFunk;
    bassFunk.family = PresetFeatures::bass;
    bassFunk.genres.add ("funk");

    for (const auto& r : f.query ({}, bassFunk))
    {
        CHECK (f.index[r.entry].params.family == PresetFeatures::bass);
        CHECK (f.index[r.entry].genres.contains ("funk"));
    }

    CHECK (f.query ({}, bassFunk).size() > 0);

    // Two genres OR together.
    PresetSearch::Filters metal, country, both;
    metal.genres.add ("metal");
    country.genres.add ("country");
    both.genres.addArray ({ "metal", "country" });
    CHECK (f.query ({}, both).size() == f.query ({}, metal).size() + f.query ({}, country).size());

    // "Uses Techniques" + Slap returns exactly the slap-armed presets.
    PresetSearch::Filters slap;
    slap.usesTechniques = true;
    slap.techniques[PresetFeatures::slap] = true;

    juce::StringArray expected;

    for (const auto& e : f.index.getEntries())
        if (e.params.techniques[PresetFeatures::slap])
            expected.add (e.info.name);

    auto got = f.names (f.query ({}, slap));
    got.sort (true);
    expected.sort (true);
    CHECK_MSG (got == expected, got.joinIntoString (",") + " vs " + expected.joinIntoString (","));

    // A synthetic slap-armed entry proves the filter is not vacuous.
    {
        PresetIndex::Entry armed;
        armed.info.name = "Armed Slap";
        armed.key = "test:slap";
        armed.parsed = true;
        armed.params.techniques[PresetFeatures::slap] = true;
        f.index.addEntryForTesting (armed);
        CHECK (f.names (f.query ({}, slap)).contains ("Armed Slap"));

        PresetSearch::Filters any;
        any.usesTechniques = true;
        CHECK (f.names (f.query ({}, any)).contains ("Armed Slap"));
    }

    // Favourites and Recent follow preset-library.json.
    auto& prefs = PresetLibraryPrefs::get();
    const auto jazzKey = f.entry ("Jazz Hollowbody")->key;
    const auto djentKey = f.entry ("8-String Djent")->key;
    prefs.setFavourite (jazzKey, true);
    prefs.noteLoaded (djentKey);

    PresetSearch::Filters fav, recent;
    fav.favourites = true;
    recent.recent = true;
    CHECK (f.names (f.query ({}, fav)) == juce::StringArray { "Jazz Hollowbody" });
    CHECK (f.names (f.query ({}, recent)) == juce::StringArray { "8-String Djent" });

    const auto onDisk = juce::JSON::parse (f.prefsFile);
    CHECK ((bool) onDisk.getProperty ("entries", {}).getProperty (jazzKey, {}).getProperty ("favourite", false));
    CHECK (onDisk.getProperty ("recent", {})[0].toString() == djentKey);

    // Filters persist as UiPreferences text.
    PresetSearch::Filters rich;
    rich.family = PresetFeatures::acoustic;
    rich.genres.add ("folk");
    rich.techniques[PresetFeatures::scrape] = true;
    rich.minRating = 3;
    const auto back = PresetSearch::Filters::fromVar (juce::JSON::parse (juce::JSON::toString (rich.toVar())));
    CHECK (back.family == rich.family && back.genres == rich.genres && back.techniques == rich.techniques
           && back.minRating == 3);
}

/*  PB-24: "Sounds like" for Modern Metal Chug. */
LUTHIER_TEST (PresetSearch, PB24_soundsLike)
{
    Fixture f;
    const int chug = f.index.indexOfName ("Modern Metal Chug");
    CHECK (chug >= 0);

    const auto first = PresetSearch::similar (f.index, chug, {});
    const auto second = PresetSearch::similar (f.index, chug, {});
    const auto names = f.names (first);

    CHECK (first.size() == 8);
    CHECK_MSG (names.contains ("Drop C Riff"), names.joinIntoString (", "));
    CHECK_MSG (names.contains ("8-String Djent"), names.joinIntoString (", "));
    CHECK (! names.contains ("Modern Metal Chug"));

    for (const auto& r : first)
        CHECK_MSG (f.index[r.entry].params.family != PresetFeatures::acoustic
                     && f.index[r.entry].params.family != PresetFeatures::classical,
                   f.index[r.entry].info.name);

    CHECK (f.names (second) == names);

    // A preset with the same sound hash is left out, as is the Source filter's.
    PresetIndex::Entry twin = f.index[chug];
    twin.info.name = "Chug Twin";
    twin.key = "test:twin";
    f.index.addEntryForTesting (twin);
    CHECK (! f.names (PresetSearch::similar (f.index, chug, {})).contains ("Chug Twin"));

    PresetSearch::Filters noFactory;
    noFactory.sources[(size_t) PresetIndex::Source::factory] = false;
    CHECK (PresetSearch::similar (f.index, chug, noFactory).empty());
}

//==============================================================================
/*  PB-27: the performance budget at 5,000 synthetic entries. */
LUTHIER_TEST (PresetSearch, PB27_performance)
{
    Fixture f;

    // 5,000 entries cloned from the factory analyses with varied names.
    PresetIndex big;
    big.setCalibration (f.index.getCalibration());
    juce::Random random (0x5000);
    const juce::StringArray words { "warm", "vintage", "stack", "glass", "night", "river", "chug", "sparkle",
                                    "velvet", "thunder", "cabin", "neon", "dust", "harbor", "static" };

    for (int i = 0; i < 5000; ++i)
    {
        auto e = f.index[i % f.index.size()];
        e.info.name = words[random.nextInt (words.size())] + " " + words[random.nextInt (words.size())] + " " + juce::String (i);
        e.key = "synthetic:" + juce::String (i);
        e.soundHash = juce::String (i);
        e.tone.centroidLog2Hz += random.nextDouble() * 0.5;
        big.addEntryForTesting (e);
    }

    // Search: every prefix of a typed query is one keystroke.
    const juce::String typed = "warm clean chug";
    double worst = 0.0;

    for (int n = 1; n <= typed.length(); ++n)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto results = PresetSearch::run (big, typed.substring (0, n), {}, PresetSearch::Sort::relevance,
                                                PresetLibraryPrefs::get());
        worst = juce::jmax (worst, juce::Time::getMillisecondCounterHiRes() - start);
        juce::ignoreUnused (results);
    }

    CHECK_MSG (worst <= 16.0, "search keystroke " + juce::String (worst, 2) + " ms");

    // Similarity.
    {
        double best = 1.0e9;

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto start = juce::Time::getMillisecondCounterHiRes();
            const auto near = PresetSearch::similar (big, 17, {});
            best = juce::jmin (best, juce::Time::getMillisecondCounterHiRes() - start);
            CHECK (near.size() == 8);
        }

        CHECK_MSG (best <= 2.0, "similarity " + juce::String (best, 3) + " ms");
    }

    // The preview mix: 0.02 units or less. A unit (performance-budget.md) is
    // 1 % of one core's real-time budget; the player's cost per block against
    // the block's duration.
    {
        PreviewPlayer player;
        player.prepare (48000.0);

        auto clip = std::make_unique<PreviewClip>();
        clip->audio.setSize (2, 48000 * 4);
        clip->audio.clear();

        for (int i = 0; i < clip->audio.getNumSamples(); ++i)
            clip->audio.setSample (0, i, 0.1f * std::sin (i * 0.05f));

        player.start (clip.get());

        juce::AudioBuffer<float> out (2, 256);
        const int blocks = 700;
        const auto start = juce::Time::getMillisecondCounterHiRes();

        for (int b = 0; b < blocks; ++b)
        {
            out.clear();
            player.processBlock (out, 256);
        }

        const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - start;
        const double audioMs = blocks * 256.0 / 48.0;
        const double units = 100.0 * elapsedMs / audioMs;
        std::cout << "    preview mix " << juce::String (units, 4) << " units (budget 0.02 on the reference CPU)" << std::endl;
        CHECK_MSG (units <= 0.02 * 2.0, "preview mix " + juce::String (units, 4) + " units");
        player.dropAll();
    }

    // Renders (measured when the corpus was built): a steady-state preset in
    // 1.0 s, the heaviest factory presets in 2.0 s (11), on the reference CPU
    // (performance-budget.md: a Ryzen 5 5600X class core). As in
    // Combo.cpuPerFactoryPreset, this shared CI machine is not that CPU: the
    // table is printed for the report and the gate is twice the budget.
    // The median stands for "steady state".
    constexpr double nonReferenceAllowance = 2.0;
    std::vector<double> times;

    for (const auto& r : factoryCorpus().bank)
    {
        times.push_back (r.result.renderMs);

        if (r.name == "8-String Djent" || r.name == "Physics Showcase")
            CHECK_MSG (r.result.renderMs <= 2000.0 * nonReferenceAllowance, r.name + " " + juce::String (r.result.renderMs, 0) + " ms");
    }

    std::sort (times.begin(), times.end());
    const double medianMs = times[times.size() / 2];
    std::cout << "    median factory render " << juce::String (medianMs, 1) << " ms, slowest "
              << juce::String (times.back(), 1) << " ms" << std::endl;
    CHECK_MSG (medianMs <= 1000.0 * nonReferenceAllowance, "median render " + juce::String (medianMs, 1) + " ms");
}
