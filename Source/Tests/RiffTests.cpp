/*  riff-library.md 16: the riff library's engine-side tests (RL-01 to RL-24,
    RL-30, RL-31 and the two fixes found while the spec was written). The GUI
    tests are in RiffPanelTests.cpp. */

#include "TestFramework.h"

#include "../Riffs/Riff.h"
#include "../Riffs/RiffAnalysis.h"
#include "../Riffs/RiffCompiler.h"
#include "../Riffs/RiffLibrary.h"
#include "../Riffs/RiffPlayer.h"
#include "../Riffs/RiffDestinations.h"
#include "../Export/MidiPerformance.h"
#include "../Export/MidiProfiles.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Support/IrLibrary.h"

#include <set>
#include <thread>
#include <cstring>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    using T = ScoreTechnique::Type;

    juce::File repoRoot()
    {
        return juce::File (__FILE__).getParentDirectory().getParentDirectory().getParentDirectory();
    }

    juce::File factoryFolder()
    {
        auto folder = RiffLibrary::getDefaultFactoryFolder();

        if (! folder.getChildFile ("catalog.json").existsAsFile())
            folder = repoRoot().getChildFile ("Resources").getChildFile ("Riffs");

        return folder;
    }

    juce::File scratch (const juce::String& name)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                     .getChildFile ("LuthierRiffTests").getChildFile (name);
        dir.deleteRecursively();
        dir.createDirectory();
        return dir;
    }

    ScoreTechnique tech (T type, double value = 0.0, double second = 0.0,
                         std::vector<std::pair<double, double>> curve = {})
    {
        ScoreTechnique t;
        t.type = type;
        t.value = value;
        t.secondValue = second;
        t.curve = std::move (curve);
        return t;
    }

    /** A riff on a standard six-string (or `tuning`), in beats. */
    struct RiffBuilder
    {
        Riff riff;

        explicit RiffBuilder (const juce::String& id = "test.lick.fixture-1", double tempo = 120.0)
        {
            riff.meta.id = id;
            riff.meta.name = "Fixture Lick 1";
            riff.meta.created = riff.meta.modified = "2026-09-24T00:00:00Z";
            riff.meta.versionCreated = riff.meta.versionModified = "1.3.0";
            riff.genre = "blues";
            riff.keyRoot = "A";
            riff.scale = "minor_pentatonic";
            riff.tempoBpm = tempo;
            riff.lengthBeats = 4.0;
        }

        RiffBuilder& note (double beat, double dur, int str, int fret, std::vector<ScoreTechnique> techs = {},
                           double vel = 0.8)
        {
            ScoreNote n;
            n.startBeat = beat;
            n.durationBeats = dur;
            n.stringIndex = str;
            n.fret = fret;
            n.velocity = vel;
            n.techniques = std::move (techs);
            riff.notes.push_back (n);
            return *this;
        }

        Riff build()
        {
            riff.updatePitches();
            riff.techniques = riff.computeTechniques();
            return riff;
        }
    };

    GuitarSpecSummary standardGuitar() { return GuitarSpecSummary::forRiff (RiffBuilder().build()); }

    /** Every note-on the player gives over `blocks` blocks, at absolute samples. */
    struct PlayedNote { juce::int64 sample; NoteOnEvent event; };

    struct HostClock
    {
        double ppq = 0.0, bpm = 120.0;
        bool playing = true;
    };

    std::vector<PlayedNote> runPlayer (RiffPlayer& player, int blocks, int blockSize, HostClock* host,
                                       std::vector<RiffPlayer::Output>* outputs = nullptr)
    {
        std::vector<PlayedNote> played;
        RiffPlayer::Output out;

        for (int b = 0; b < blocks; ++b)
        {
            out.clear();
            const double ppq = host != nullptr ? host->ppq : 0.0;
            player.renderSubBlock (blockSize, ppq, host != nullptr && host->playing,
                                   host != nullptr ? host->bpm : 0.0, out);

            for (int i = 0; i < out.queue.getNumNoteOns(); ++i)
                played.push_back ({ (juce::int64) b * blockSize + out.queue.getNoteOn (i).sampleOffset,
                                    out.queue.getNoteOn (i) });

            if (outputs != nullptr)
                outputs->push_back (out);

            if (host != nullptr)
                host->ppq += blockSize * host->bpm / 60.0 / kSr;
        }

        return played;
    }

    std::shared_ptr<const CompiledRiff> compileStandard (const Riff& riff, RiffPlaySettings settings = {})
    {
        return RiffCompiler::compile (riff, settings, GuitarSpecSummary::forRiff (riff));
    }

    std::vector<Riff> loadAllFactory (TestContext& ctx)
    {
        std::vector<Riff> riffs;
        const auto files = factoryFolder().findChildFiles (juce::File::findFiles, true, "*.luthierriff");

        for (const auto& f : files)
        {
            Riff r;
            const auto result = Riff::loadFromFile (f, r);
            CHECK_MSG (result.wasOk(), f.getFileName() + ": " + result.getErrorMessage());

            if (result.wasOk())
                riffs.push_back (std::move (r));
        }

        return riffs;
    }
}

//==============================================================================
// RL-01: the generator is deterministic and the committed tree is its output.
LUTHIER_TEST (Riffs, generatorOutputIsCommittedAndDeterministic)
{
    const auto script = repoRoot().getChildFile ("Tools").getChildFile ("generate_factory_riffs.py");

    if (! script.existsAsFile())
    {
        CHECK_MSG (true, "no source tree beside the runner");
        return;
    }

    juce::ChildProcess python;

    if (! python.start (juce::StringArray { "python3", script.getFullPathName(), "--check" }))
    {
        CHECK_MSG (true, "python3 is not available; CI runs the same check");
        return;
    }

    const auto output = python.readAllProcessOutput();
    python.waitForProcessToFinish (120000);
    CHECK_MSG (python.getExitCode() == 0, "generate_factory_riffs.py --check: " + output);
}

//==============================================================================
// RL-02: the catalog matches 2.1's table and every coverage rule.
LUTHIER_TEST (Riffs, catalogMeetsTheCoverageTable)
{
    RiffLibrary library;
    library.setFolders (factoryFolder(), {}, {});
    library.loadIndexNow();

    CHECK_MSG (! library.isFactoryMissing(), "factory riffs not found at " + factoryFolder().getFullPathName());
    CHECK (library.getNumEntries() >= 300);

    const int table[12][4] = {
        { 10, 8, 4, 6 }, { 6, 12, 4, 6 }, { 12, 6, 2, 6 }, { 6, 6, 6, 8 }, { 4, 10, 4, 6 }, { 2, 10, 4, 8 },
        { 4, 6, 10, 4 }, { 4, 4, 6, 6 }, { 4, 4, 8, 6 }, { 6, 6, 6, 6 }, { 8, 6, 6, 6 }, { 6, 8, 6, 8 },
    };

    int counts[12][4] {};
    int perDifficulty[12][6] {};
    int easy = 0, drop = 0, seven = 0, five = 0, free = 0;
    std::map<juce::String, int> technique;

    for (int i = 0; i < library.getNumEntries(); ++i)
    {
        const auto& e = *library.getEntry (i);
        CHECK (e.genreIndex >= 0 && e.typeIndex >= 0);

        if (e.genreIndex < 0 || e.typeIndex < 0)
            continue;

        ++counts[e.genreIndex][e.typeIndex];
        ++perDifficulty[e.genreIndex][e.difficulty];
        easy += e.difficulty <= 2 ? 1 : 0;
        drop += (e.genre == "metal" && e.tuningName == "Drop D") ? 1 : 0;
        seven += (e.genre == "metal" && e.instrument == "guitar7") ? 1 : 0;
        five += e.instrument == "bass5" ? 1 : 0;
        free += e.free ? 1 : 0;

        CHECK_MSG (e.lengthBeats <= 64.0, e.id);

        for (const auto& t : e.techniques)
            ++technique[t];
    }

    for (int g = 0; g < 12; ++g)
    {
        for (int t = 0; t < 4; ++t)
            CHECK_MSG (counts[g][t] == table[g][t], juce::String (RiffVocabulary::genres()[(size_t) g].id) + " "
                         + RiffVocabulary::types()[t] + ": " + juce::String (counts[g][t]));

        for (int d = 1; d <= 4; ++d)
            CHECK_MSG (perDifficulty[g][d] > 0, juce::String (RiffVocabulary::genres()[(size_t) g].id)
                         + " has no difficulty " + juce::String (d));
    }

    CHECK (easy * 100 >= 40 * library.getNumEntries());
    CHECK (drop >= 4);
    CHECK (seven >= 2);
    CHECK (five >= 6);
    CHECK (free == 60);

    for (const auto& token : RiffVocabulary::noteTechniques())
    {
        // INTEGRATE-2: FEAT-ASSIST's pick strokes have riff tokens so a saved
        // phrase keeps them; the factory content was written before them.
        if (token == "upstroke" || token == "downstroke")
            continue;

        // tab-import-export 7.3: slap/pop note techniques come from imported
        // tabs; factory riffs carry slap and pop as BASS_TECH events instead.
        if (token == "slapnote" || token == "popnote")
            continue;

        const int least = (token == "artificial" || token == "tapharm" || token == "whammy") ? 2 : 3;
        CHECK_MSG (technique[token] >= least, token + " in " + juce::String (technique[token]) + " items");
    }
}

//==============================================================================
// RL-03: every factory file loads, its technique set is right, and a save
// gives back the same bytes; mutated files load or refuse, never crash.
LUTHIER_TEST (Riffs, factoryFilesLoadAndRoundTripByteIdentical)
{
    const auto files = factoryFolder().findChildFiles (juce::File::findFiles, true, "*.luthierriff");
    CHECK (files.size() >= 300);

    int identical = 0;

    for (const auto& f : files)
    {
        Riff r;
        juce::StringArray warnings;
        const auto text = f.loadFileAsString();
        const auto result = Riff::fromJson (text, r, &warnings);

        CHECK_MSG (result.wasOk(), f.getFileName() + ": " + result.getErrorMessage());
        CHECK_MSG (warnings.isEmpty(), f.getFileName() + ": " + warnings.joinIntoString ("; "));
        CHECK_MSG (r.computeTechniques() == r.techniques, f.getFileName());

        const auto again = r.toJson();

        if (again == text)
            ++identical;
        else
            CHECK_MSG (false, f.getFileName() + " does not save back byte-identical");
    }

    CHECK (identical == files.size());
}

LUTHIER_TEST (Riffs, mutatedFilesLoadOrRefuseCleanly)
{
    const auto file = factoryFolder().getChildFile ("Blues").findChildFiles (juce::File::findFiles, false, "*.luthierriff")[0];
    const auto original = file.loadFileAsString();
    CHECK (original.isNotEmpty());

    juce::Random random (0x51ff);
    int loaded = 0, refused = 0;
    const juce::String junk ("{}[]\":,0123456789.-eE xtrufalsn\\");

    for (int i = 0; i < 10000; ++i)
    {
        auto text = original;
        const int edits = 1 + random.nextInt (4);

        for (int k = 0; k < edits; ++k)
        {
            const int at = random.nextInt (juce::jmax (1, text.length()));

            switch (random.nextInt (3))
            {
                case 0:  text = text.substring (0, at) + text.substring (at + 1); break;
                case 1:  text = text.substring (0, at) + junk[random.nextInt (junk.length())] + text.substring (at); break;
                default: text = text.substring (0, at) + junk[random.nextInt (junk.length())] + text.substring (at + 1); break;
            }
        }

        Riff r;
        const auto result = Riff::fromJson (text, r);

        if (result.wasOk())
        {
            ++loaded;

            // Whatever loaded is playable.
            const auto compiled = compileStandard (r);
            CHECK (compiled != nullptr);
        }
        else
        {
            ++refused;
        }
    }

    CHECK (loaded + refused == 10000);
    CHECK (refused > 0);
}

//==============================================================================
// RL-04: compiling is pure, on any thread.
LUTHIER_TEST (Riffs, compileIsPureAndThreadSafe)
{
    RiffLibrary library;
    library.setFolders (factoryFolder(), {}, {});
    library.loadIndexNow();

    RiffPlaySettings settings;
    settings.targetRoot = 4;
    settings.mapScale = true;
    settings.targetScale = "dorian";

    int compared = 0;

    for (int i = 0; i < library.getNumEntries(); i += 7)
    {
        const auto riff = library.getRiff (library.getEntry (i)->id);
        CHECK (riff != nullptr);

        if (riff == nullptr)
            continue;

        const auto guitar = standardGuitar();
        const auto a = RiffCompiler::compile (*riff, settings, guitar);
        const auto b = RiffCompiler::compile (*riff, settings, guitar);
        CHECK_MSG (*a == *b, riff->meta.id);

        std::shared_ptr<const CompiledRiff> c, d;
        std::thread t1 ([&] { c = RiffCompiler::compile (*riff, settings, guitar); });
        std::thread t2 ([&] { d = RiffCompiler::compile (*riff, settings, guitar); });
        t1.join();
        t2.join();

        CHECK_MSG (*c == *a && *d == *a, riff->meta.id + " differs across threads");
        ++compared;
    }

    CHECK (compared > 40);
}

//==============================================================================
// RL-05: every technique token maps onto the engine as 5.1 says.
LUTHIER_TEST (Riffs, techniquesMapOntoTheEngine)
{
    RiffBuilder b;
    b.note (0.0, 1.0, 2, 5)
     .note (1.0, 1.0, 2, 7, { tech (T::hammerOn) })
     .note (2.0, 1.0, 2, 5, { tech (T::pullOff) })
     .note (4.0, 1.0, 0, 12, { tech (T::tap) })
     .note (5.0, 1.0, 5, 3, { tech (T::palmMute, 0.6) })
     .note (6.0, 1.0, 4, 0, { tech (T::deadNote) })
     .note (7.0, 1.0, 3, 12, { tech (T::naturalHarmonic, 12) })
     .note (8.0, 1.0, 1, 5, { tech (T::artificialHarmonic, 12) })
     .note (9.0, 1.0, 1, 7, { tech (T::pinchHarmonic) })
     .note (10.0, 1.0, 3, 7, { tech (T::slideLegato, 9) })
     .note (11.0, 1.0, 3, 9)
     .note (12.0, 1.0, 0, 8, { tech (T::bend, 2, 0, { { 0.0, 0.0 }, { 0.25, 2.0 }, { 1.0, 2.0 } }) })
     .note (13.0, 1.0, 5, 3, { tech (T::palmMute) })
     .note (14.0, 1.0, 1, 8, { tech (T::slideIn, 5) });
    b.riff.lengthBeats = 16.0;
    const auto riff = b.build();
    const auto c = compileStandard (riff);

    auto onAt = [&c] (double beat) -> const RiffEvent*
    {
        for (const auto& e : c->events)
            if (e.kind == RiffEvent::Kind::noteOn && std::abs (e.beat - beat) < 1.0e-9)
                return &e;

        return nullptr;
    };

    CHECK (onAt (0.0)->technique == Technique::Pluck);
    CHECK (onAt (1.0)->technique == Technique::HammerOn);
    CHECK (onAt (2.0)->technique == Technique::PullOff);
    CHECK (onAt (4.0)->technique == Technique::Tap);
    CHECK (onAt (5.0)->technique == Technique::PalmMute);
    CHECK_NEAR (onAt (5.0)->palmMuteDepth, 0.6, 1.0e-12);
    CHECK_NEAR (onAt (13.0)->palmMuteDepth, -1.0, 1.0e-12);
    CHECK (onAt (6.0)->technique == Technique::MutedPick);
    // harmonic-realism 4.1: a natural harmonic stops open and is touched at
    // its node; an artificial one stops at its fret and is touched 12 above.
    CHECK (onAt (7.0)->technique == Technique::NaturalHarmonic);
    CHECK (onAt (7.0)->harmonicPartial == 2);
    CHECK_NEAR (onAt (7.0)->fret, 0.0, 1.0e-12);
    CHECK (onAt (7.0)->touchFret > 11.0 && onAt (7.0)->touchFret < 13.0);
    CHECK (onAt (8.0)->technique == Technique::ArtificialHarmonic);
    CHECK (onAt (8.0)->harmonicPartial == 2);
    CHECK_NEAR (onAt (8.0)->fret, 5.0, 1.0e-12);
    CHECK_NEAR (onAt (8.0)->touchFret, 17.0, 1.0e-12);
    CHECK (onAt (9.0)->technique == Technique::PinchHarmonic);
    CHECK (onAt (11.0)->technique == Technique::Slide);
    CHECK_NEAR (onAt (11.0)->slideFromFret, 7.0, 1.0e-12);
    CHECK (onAt (14.0)->technique == Technique::Pluck);
    CHECK_NEAR (onAt (14.0)->slideFromFret, 5.0, 1.0e-12);

    // The bend curve reaches the stated semitones at the stated positions, +/-1 cent.
    const auto* bent = onAt (12.0);
    CHECK (bent->bendSegment >= 0);

    RiffPlayer player;
    player.prepare (kSr);
    player.setCompiled (c, true);
    player.setClockMode (RiffPlayer::ClockMode::own);
    player.setLooping (false);
    player.play();

    std::vector<RiffPlayer::Output> outputs;
    const int block = 64;
    const double samplesPerBeat = kSr * 60.0 / 120.0;
    runPlayer (player, (int) (15.0 * samplesPerBeat / block), block, nullptr, &outputs);

    double centsAtQuarter = -1.0, centsAtThreeQuarters = -1.0;

    for (size_t k = 0; k < outputs.size(); ++k)
    {
        for (int i = 0; i < outputs[k].queue.getNumBends(); ++i)
        {
            const auto& bend = outputs[k].queue.getBend (i);

            if (bend.stringIndex != 0)
                continue;

            const double beat = ((double) k * block + bend.sampleOffset + 1) / samplesPerBeat;

            if (std::abs (beat - 12.25) < 0.01) centsAtQuarter = bend.cents;
            if (std::abs (beat - 12.75) < 0.01) centsAtThreeQuarters = bend.cents;
        }
    }

    CHECK_NEAR (centsAtQuarter, 200.0, 1.0);
    CHECK_NEAR (centsAtThreeQuarters, 200.0, 1.0);
}

//==============================================================================
// RL-06: every factory riff survives the Luthier profile.
LUTHIER_TEST (Riffs, everyFactoryRiffSurvivesTheLuthierProfile)
{
    const auto riffs = loadAllFactory (ctx);
    int checked = 0;

    for (const auto& riff : riffs)
    {
        const auto compiled = compileStandard (riff);
        const auto placed = compiled->toPlacedRiff (riff);
        const auto performance = RiffDestinations::toPerformance (placed, compiled->tempoBpm, kSr);

        MidiExportOptions options;
        options.profile = MidiProfile::luthier;
        options.split = MidiTrackSplit::single;

        const auto bytes = MidiProfiles::exportToMemory (performance, options);
        MidiPerformance imported (kSr);
        const auto result = MidiProfiles::importFromMemory (bytes.getData(), bytes.getSize(), imported, kSr);
        CHECK_MSG (result.ok, riff.meta.id + ": import failed: " + result.error);

        PerformanceScore back;
        auto& track = back.getTrack (0);
        track.numStrings = placed.getNumStrings();

        for (int s = 0; s < track.numStrings; ++s)
            track.tuning[(size_t) s] = placed.tuning[(size_t) s];

        imported.toScore (back);

        std::vector<const ScoreNote*> got;
        std::vector<double> gotBeat;
        const auto& t = back.getTrack (0);
        const double bar = riff.getBeatsPerBar();

        for (size_t m = 0; m < t.measures.size(); ++m)
            for (const auto* n : t.measures[m].collectNotes())
            {
                got.push_back (n);
                gotBeat.push_back ((double) m * bar + n->startBeat);
            }

        const auto& want = placed.notes;
        CHECK_MSG (got.size() == want.size(), riff.meta.id + ": " + juce::String ((int) got.size()) + " notes back of "
                     + juce::String ((int) want.size()));

        if (got.size() != want.size())
            continue;

        // Pair them by (beat, string): the score orders notes at one beat by voice.
        std::vector<bool> used (got.size(), false);

        for (const auto& w : want)
        {
            bool found = false;

            for (size_t k = 0; k < got.size() && ! found; ++k)
            {
                if (used[k] || got[k]->stringIndex != w.stringIndex
                      || std::abs (gotBeat[k] - w.startBeat) > 1.5 / 960.0)
                    continue;

                used[k] = true;
                found = true;

                CHECK_MSG (got[k]->fret == w.fret, riff.meta.id + ": fret");

                std::set<int> a, b;
                for (const auto& x : w.techniques)       a.insert ((int) x.type);
                for (const auto& x : got[k]->techniques) b.insert ((int) x.type);

                CHECK_MSG (a == b, riff.meta.id + ": techniques at beat " + juce::String (w.startBeat));
            }

            CHECK_MSG (found, riff.meta.id + ": no note back at beat " + juce::String (w.startBeat)
                         + " string " + juce::String (w.stringIndex));
        }

        ++checked;
    }

    CHECK (checked >= 300);
}

//==============================================================================
// RL-07 and fix (a): the drag file.
LUTHIER_TEST (Riffs, dragFileIsAValidMidiFileInBothProfiles)
{
    RiffBuilder b;
    b.note (0.0, 1.0, 5, 3, { tech (T::palmMute) })
     .note (1.0, 1.0, 5, 5, { tech (T::palmMute, 0.5) })
     .note (2.0, 1.0, 1, 7, { tech (T::pinchHarmonic) })
     .note (3.0, 1.0, 2, 12, { tech (T::naturalHarmonic, 12) });
    const auto riff = b.build();
    const auto compiled = compileStandard (riff);
    const auto folder = scratch ("drag");

    // The two profiles share a file name (one drag, one file): read each as it is written.
    juce::MemoryBlock a, c, g;

    const auto luthierFile = RiffDestinations::writeDragFile (riff, *compiled, MidiProfile::luthier, folder);
    CHECK (luthierFile.existsAsFile() && luthierFile.getFileName().endsWith (".mid"));
    CHECK (luthierFile.getParentDirectory() == folder);
    luthierFile.loadFileAsData (a);

    const auto again = RiffDestinations::writeDragFile (riff, *compiled, MidiProfile::luthier, folder);
    again.loadFileAsData (c);

    const auto genericFile = RiffDestinations::writeDragFile (riff, *compiled, MidiProfile::generic, folder);
    CHECK (genericFile.existsAsFile());
    genericFile.loadFileAsData (g);
    CHECK_MSG (a == c, "the same inputs give different bytes");

    auto holds = [] (const juce::MemoryBlock& block, const char* text)
    {
        const auto* begin = static_cast<const char*> (block.getData());
        const auto* end = begin + block.getSize();
        return std::search (begin, end, text, text + std::strlen (text)) != end;
    };

    CHECK (holds (a, "LUTHIER"));          // the header's 7D 'L' 'U' 'T' 'H' 'I' 'E' 'R'
    CHECK (! holds (g, "LUTHIER-BEGIN"));   // Generic carries no markers

    for (const auto* data : { &a, &g })
    {
        juce::MidiFile file;
        juce::MemoryInputStream in (*data, false);
        CHECK (file.readFrom (in));

        int pm = 0, pinch = 0, natural = 0;

        for (int t = 0; t < file.getNumTracks(); ++t)
            for (const auto* e : *file.getTrack (t))
                if (e->message.isController() && e->message.getControllerValue() == 127)
                {
                    pm += e->message.getControllerNumber() == 67 ? 1 : 0;
                    pinch += e->message.getControllerNumber() == 72 ? 1 : 0;
                    natural += e->message.getControllerNumber() == 73 ? 1 : 0;
                }

        CHECK_MSG (pm == 2, "CC67 for each palm-muted note: " + juce::String (pm));
        CHECK (pinch == 1);
        CHECK (natural == 1);
    }
}

// Fix (a): fromScore writes CC67 / 72 / 73 and a round trip keeps the flags.
LUTHIER_TEST (Riffs, fromScoreWritesPalmMuteAndHarmonicControllers)
{
    RiffBuilder b;
    b.note (0.0, 1.0, 5, 3, { tech (T::palmMute) })
     .note (1.0, 1.0, 1, 7, { tech (T::pinchHarmonic) })
     .note (2.0, 1.0, 2, 12, { tech (T::naturalHarmonic, 12) });
    const auto score = b.build().toScore();
    const auto performance = MidiPerformance::fromScore (score, kSr);

    std::map<int, int> on, off;

    for (const auto& m : performance.getMessages())
        if (m.message.isController())
            (m.message.getControllerValue() > 0 ? on : off)[m.message.getControllerNumber()]++;

    CHECK (on[67] == 1 && off[67] == 1);
    CHECK (on[72] == 1 && off[72] == 1);
    CHECK (on[73] == 1 && off[73] == 1);

    PerformanceScore back;
    performance.toScore (back);

    std::set<int> types;

    for (const auto& m : back.getTrack (0).measures)
        for (const auto* n : m.collectNotes())
            for (const auto& t : n->techniques)
                types.insert ((int) t.type);

    CHECK (types.count ((int) T::palmMute) == 1);
    CHECK (types.count ((int) T::pinchHarmonic) == 1);
    CHECK (types.count ((int) T::naturalHarmonic) == 1);
}

//==============================================================================
// RL-08: transposition.
LUTHIER_TEST (Riffs, transposeTakesTheSmallestShiftThatFits)
{
    CHECK (RiffTransposer::keyShift (9, 4) == -5);    // A to E
    CHECK (RiffTransposer::keyShift (4, 9) == 5);
    CHECK (RiffTransposer::keyShift (0, 6) == -6);

    // An A minor-pentatonic box-5 lick to E: down five, same strings.
    RiffBuilder lick;
    lick.note (0.0, 0.5, 0, 8).note (0.5, 0.5, 0, 5).note (1.0, 0.5, 1, 8).note (1.5, 0.5, 1, 5).note (2.0, 1.0, 2, 7);
    const auto a = lick.build();

    RiffPlaySettings toE;
    toE.targetRoot = 4;
    const auto placedE = RiffTransposer::place (a, toE, standardGuitar());
    CHECK (placedE.keyShift == -5 && placedE.shift == -5);
    CHECK (placedE.dropped == 0);

    for (size_t i = 0; i < placedE.notes.size(); ++i)
    {
        CHECK (placedE.notes[i].stringIndex == a.notes[(size_t) placedE.sourceIndex[i]].stringIndex);
        CHECK (placedE.notes[i].fret >= 0 && placedE.notes[i].fret <= 24);
    }

    // An open-position C shape up a semitone takes the +1 candidate.
    RiffBuilder open;
    const int shape[6] = { 0, 1, 0, 2, 3, -1 };   // [x32010] high to low

    for (int s = 0; s < 6; ++s)
        if (shape[s] >= 0)
            open.note (0.0, 4.0, s, shape[s]);

    open.riff.keyRoot = "C";
    open.riff.scale = "ionian";
    const auto c = open.build();

    RiffPlaySettings toDb;
    toDb.targetRoot = 1;
    const auto placedDb = RiffTransposer::place (c, toDb, standardGuitar());
    CHECK (placedDb.shift == 1);
    CHECK (placedDb.dropped == 0);

    // Open strings down a semitone do not fit: the octave above does.
    RiffPlaySettings toB;
    toB.targetRoot = 11;
    const auto placedB = RiffTransposer::place (c, toB, standardGuitar());
    CHECK (placedB.keyShift == -1);
    CHECK_MSG (placedB.shift == 11, "shift " + juce::String (placedB.shift));
    CHECK (placedB.dropped == 0);

    // Deterministic.
    CHECK (RiffTransposer::place (c, toB, standardGuitar()).shift == placedB.shift);
}

// RL-09: scale mapping.
LUTHIER_TEST (Riffs, scaleMapMovesDegreesAndKeepsPassingTones)
{
    // C ionian degrees and a chromatic passing tone (C#), mapped to C aeolian.
    CHECK (RiffTransposer::mapPitch (60, 0, 0, "ionian", "aeolian", true) == 60);   // 1
    CHECK (RiffTransposer::mapPitch (64, 0, 0, "ionian", "aeolian", true) == 63);   // 3 down
    CHECK (RiffTransposer::mapPitch (65, 0, 0, "ionian", "aeolian", true) == 65);   // 4
    CHECK (RiffTransposer::mapPitch (69, 0, 0, "ionian", "aeolian", true) == 68);   // 6 down
    CHECK (RiffTransposer::mapPitch (71, 0, 0, "ionian", "aeolian", true) == 70);   // 7 down
    CHECK (RiffTransposer::mapPitch (61, 0, 0, "ionian", "aeolian", true) == 61);   // C# keeps its offset from 1
    CHECK (RiffTransposer::mapPitch (68, 0, 0, "ionian", "aeolian", true) == 68);   // G# is G+1; G is degree 5 in both
    CHECK (RiffTransposer::mapPitch (64, 0, 0, "ionian", "aeolian", false) == 64);  // off: unchanged

    // Chromatic riffs never remap.
    CHECK (RiffTransposer::mapPitch (64, 0, 0, "chromatic", "aeolian", true) == 64);
    CHECK (! RiffTransposer::canMapScales ("ionian", "minor_pentatonic"));
    CHECK (RiffTransposer::canMapScales ("major_pentatonic", "minor_pentatonic"));
}

// RL-10: re-fretting to the loaded guitar.
LUTHIER_TEST (Riffs, refrettingKeepsPitchAcrossTuningsAndFamilies)
{
    // Drop D on standard: same pitches, the low string two frets down (its
    // notes from E up exist on both; an open low D exists on neither standard
    // guitar nor this test).
    RiffBuilder drop;
    drop.riff.tuning = { 64, 59, 55, 50, 45, 38 };
    drop.riff.keyRoot = "D";
    drop.note (0.0, 1.0, 5, 2).note (1.0, 1.0, 5, 5).note (2.0, 1.0, 4, 5);
    const auto dropD = drop.build();
    const auto placed = RiffTransposer::place (dropD, {}, standardGuitar());

    CHECK (placed.dropped == 0);

    for (size_t i = 0; i < placed.notes.size(); ++i)
        CHECK (placed.notes[i].midiNote == dropD.notes[(size_t) placed.sourceIndex[i]].midiNote);

    CHECK (placed.notes[0].fret == 0);
    CHECK (placed.shift == 0);

    // A bass line on a guitar: exactly an octave up.
    RiffBuilder bass;
    bass.riff.instrument = "bass4";
    bass.riff.type = "bass";
    bass.riff.tuning = { 43, 38, 33, 28 };
    bass.note (0.0, 1.0, 3, 5).note (1.0, 1.0, 2, 7).note (2.0, 1.0, 1, 5);
    const auto line = bass.build();
    const auto onGuitar = RiffTransposer::place (line, {}, standardGuitar());

    CHECK (onGuitar.familyOctave == 12);
    CHECK (onGuitar.dropped == 0);

    for (size_t i = 0; i < onGuitar.notes.size(); ++i)
        CHECK (onGuitar.notes[i].midiNote == line.notes[(size_t) onGuitar.sourceIndex[i]].midiNote + 12);

    CHECK (onGuitar.notices.joinIntoString (" ").contains ("octave"));

    // A 7-string riff on a 6-string: the low-string phrase moves or drops, and says so.
    RiffBuilder seven;
    seven.riff.instrument = "guitar7";
    seven.riff.tuning = { 64, 59, 55, 50, 45, 40, 35 };
    seven.note (0.0, 1.0, 6, 0).note (1.0, 1.0, 6, 2, { tech (T::hammerOn) }).note (2.0, 1.0, 6, 3);
    const auto sevenString = seven.build();
    const auto onSix = RiffTransposer::place (sevenString, {}, standardGuitar());

    CHECK (onSix.dropped + (int) onSix.notes.size() == 3);
    CHECK (onSix.dropped > 0 || onSix.movedChains || onSix.shift != 0);
    CHECK (onSix.notices.size() > 0);
}

//==============================================================================
// RL-12: on the host clock a start waits for the next bar, and loops hold.
LUTHIER_TEST (Riffs, hostClockStartsOnTheNextBarAndLoopsWithoutDrift)
{
    RiffBuilder b;
    b.note (0.0, 1.0, 2, 5);
    const auto compiled = compileStandard (b.build());

    RiffPlayer player;
    player.prepare (kSr);
    player.setCompiled (compiled, true);
    player.setLooping (true);

    HostClock host;
    host.ppq = 1.3;   // beat 2.3 of bar 1, counting from 1
    host.bpm = 120.0;

    player.play();
    const int block = 480;
    const auto played = runPlayer (player, 16 * 96000 / block + 200, block, &host);

    CHECK (played.size() >= 16);

    const double samplesPerBeat = kSr * 60.0 / 120.0;
    const double first = (4.0 - 1.3) * samplesPerBeat;   // the next bar line

    for (size_t k = 0; k < juce::jmin ((size_t) 16, played.size()); ++k)
        CHECK_MSG (std::abs ((double) played[k].sample - (first + (double) k * 4.0 * samplesPerBeat)) <= 1.0,
                   "loop " + juce::String ((int) k) + " at " + juce::String (played[k].sample));

    // A host stop ends the riff.
    host.playing = false;
    RiffPlayer::Output out;
    player.renderSubBlock (block, host.ppq, false, 120.0, out);
    CHECK (! player.isPlaying());
}

// RL-13: the own clock, a tempo factor, and strums in seconds.
LUTHIER_TEST (Riffs, ownClockRunsAtTheFactorAndStrumsStayInSeconds)
{
    RiffBuilder b ("test.strum.fixture-1", 84.0);
    b.note (0.0, 1.0, 2, 5).note (1.0, 1.0, 2, 7).note (2.0, 1.0, 2, 5);

    for (int s = 0; s < 6; ++s)
        b.note (3.0, 1.0, s, 0);

    RiffStrum strum;
    strum.beat = 3.0;
    strum.cv = 200.0;
    strum.mask = 63;
    b.riff.strums.push_back (strum);
    const auto compiled = compileStandard (b.build());

    RiffPlayer player;
    player.prepare (kSr);
    player.setCompiled (compiled, true);
    player.setClockMode (RiffPlayer::ClockMode::own);
    player.setTempoFactor (0.5);
    player.setLooping (false);
    player.play();

    const auto played = runPlayer (player, 2000, 256, nullptr);
    const double spb = kSr * 60.0 / 42.0;

    CHECK (played.size() == 9);

    if (played.size() == 9)
    {
        CHECK_NEAR ((double) played[0].sample, 0.0, 1.0);
        CHECK_NEAR ((double) played[1].sample, spb, 1.0);
        CHECK_NEAR ((double) played[2].sample, 2.0 * spb, 1.0);

        // Down stroke: lowest string first, 1/200 s apart, whatever the tempo.
        std::vector<juce::int64> strumOnsets;

        for (size_t k = 3; k < 9; ++k)
            strumOnsets.push_back (played[k].sample);

        std::sort (strumOnsets.begin(), strumOnsets.end());
        CHECK_NEAR ((double) strumOnsets[0], 3.0 * spb, 1.0);

        for (size_t k = 1; k < strumOnsets.size(); ++k)
            CHECK_NEAR ((double) (strumOnsets[k] - strumOnsets[k - 1]), kSr / 200.0, 1.0);
    }
}

//==============================================================================
// RL-14: stopping never leaves a note or a bend behind.
LUTHIER_TEST (Riffs, stopPanicNewRiffAndResetLeaveNothingSounding)
{
    RiffBuilder b;
    b.note (0.0, 4.0, 0, 8, { tech (T::bend, 2, 0, { { 0.0, 0.0 }, { 0.25, 2.0 }, { 1.0, 2.0 } }) })
     .note (0.0, 4.0, 3, 7);
    const auto riff = b.build();

    LuthierEngine engine;
    engine.prepare (kSr, 256);
    juce::AudioBuffer<float> buffer (2, 256);

    auto run = [&] (int blocks)
    {
        for (int i = 0; i < blocks; ++i)
        {
            juce::MidiBuffer midi;
            buffer.clear();
            engine.processBlock (buffer, midi);
        }
    };

    auto& player = engine.getRiffPlayer();
    auto restart = [&]
    {
        player.setCompiled (RiffCompiler::compile (riff, {}, GuitarSpecSummary::forRiff (riff)), true);
        player.setClockMode (RiffPlayer::ClockMode::own);
        player.play();
        run (60);   // past the quarter point: the bend is up
        CHECK (player.getSoundingMask() != 0);
        CHECK_MSG (engine.getRiffBendCents (0) > 100.0, "bend " + juce::String (engine.getRiffBendCents (0))
                     + " at beat " + juce::String (player.getBeatPosition()));
    };

    auto silent = [&] (const char* what)
    {
        CHECK_MSG (player.getSoundingMask() == 0, juce::String (what) + ": a riff string still sounding");

        for (int s = 0; s < kMaxStrings; ++s)
            CHECK_MSG (std::abs (engine.getRiffBendCents (s)) < 1.0e-9, juce::String (what) + ": a bend remains");
    };

    restart();
    player.stop();
    run (1);
    silent ("stop");

    restart();
    engine.panic();
    player.stop();   // what LuthierAudioProcessor::panic() adds
    run (1);
    silent ("panic");

    restart();
    RiffBuilder other ("test.lick.other-1");
    other.note (2.0, 1.0, 2, 5);
    player.setCompiled (compileStandard (other.build()), true);
    run (1);
    CHECK_MSG ((player.getSoundingMask() & 1) == 0, "a new riff left the old riff's note");
    CHECK (std::abs (engine.getRiffBendCents (0)) < 1.0e-9);

    restart();
    engine.reset();   // a preset load
    run (1);
    CHECK (std::abs (engine.getRiffBendCents (0)) < 1.0e-9);
    player.stop();
    run (1);
    silent ("reset");
}

// RL-15: queue pressure drops bend points, never notes.
LUTHIER_TEST (Riffs, queuePressureDropsBendPointsNotNotes)
{
    RiffBuilder b;
    b.riff.instrument = "guitar12";
    b.riff.tuning = { 64, 59, 55, 50, 45, 40, 64, 59, 55, 50, 45, 40 };

    for (int s = 0; s < 12; ++s)
        b.note (0.0, 4.0, s, 5, { tech (T::bend, 1, 0, { { 0.0, 0.0 }, { 1.0, 1.0 } }) });

    const auto riff = b.build();
    const auto compiled = RiffCompiler::compile (riff, {}, GuitarSpecSummary::forRiff (riff));

    RiffPlayer player;
    player.prepare (kSr);
    player.setCompiled (compiled, true);
    player.setClockMode (RiffPlayer::ClockMode::own);
    player.play();

    RiffPlayer::Output out;
    player.renderSubBlock (4096, 0.0, false, 0.0, out);

    CHECK (out.queue.getNumNoteOns() == 12);
    CHECK_MSG (out.queue.getNumNoteOns() + out.queue.getNumNoteOffs() + out.queue.getNumBends()
                 <= RiffPlayer::kMaxSlotsPerSubBlock + 12,
               "riff events used " + juce::String (out.queue.getNumBends()) + " bend slots");
    CHECK (out.queue.getNumBends() <= RiffPlayer::kMaxSlotsPerSubBlock);
    CHECK (player.getOverflowCount() > 0);
}

// RL-16: every riff note is explicit, and the player's strings are kept.
LUTHIER_TEST (Riffs, riffNotesAreExplicitAndKeepTheirStrings)
{
    RiffBuilder b;
    b.note (0.0, 1.0, 2, 5).note (1.0, 1.0, 4, 7, { tech (T::palmMute) }).note (2.0, 1.0, 1, 3);
    const auto compiled = compileStandard (b.build());

    RiffPlayer player;
    player.prepare (kSr);
    player.setCompiled (compiled, true);
    player.setClockMode (RiffPlayer::ClockMode::own);
    player.setLooping (false);
    player.play();

    const auto played = runPlayer (player, 400, 256, nullptr);
    CHECK (played.size() == 3);

    const int strings[] = { 2, 4, 1 };
    const int frets[] = { 5, 7, 3 };

    for (size_t i = 0; i < played.size() && i < 3; ++i)
    {
        CHECK (played[i].event.explicitArticulation);
        CHECK (played[i].event.stringIndex == strings[i]);
        CHECK_NEAR (played[i].event.fretPosition, (double) frets[i], 1.0e-12);
    }
}

// RL-17: with the rhythm engine strumming a held chord, riff notes sound and
// leave the detected chord alone.
LUTHIER_TEST (Riffs, riffsCoexistWithTheRhythmEngine)
{
    LuthierEngine engine;
    engine.prepare (kSr, 256);
    engine.getRhythmEngine().setEnabled (true);
    engine.setTempoBpm (120.0);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer chord;
    chord.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
    chord.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
    chord.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 100), 0);
    engine.processBlock (buffer, chord);

    for (int i = 0; i < 40; ++i)
    {
        juce::MidiBuffer none;
        engine.processBlock (buffer, none);
    }

    const auto before = engine.getRhythmEngine().getCurrentChord();

    RiffBuilder b;
    b.note (0.0, 0.5, 0, 10).note (0.5, 0.5, 0, 13).note (1.0, 0.5, 1, 12);
    engine.getRiffPlayer().setCompiled (compileStandard (b.build()), true);
    engine.getRiffPlayer().setClockMode (RiffPlayer::ClockMode::own);
    engine.getRiffPlayer().play();

    bool sounded = false;

    for (int i = 0; i < 200; ++i)
    {
        juce::MidiBuffer none;
        engine.processBlock (buffer, none);
        sounded = sounded || engine.getRiffPlayer().getSoundingMask() != 0;
        CHECK_FINITE (buffer.getReadPointer (0), buffer.getNumSamples());
    }

    CHECK (sounded);
    const auto after = engine.getRhythmEngine().getCurrentChord();
    CHECK_MSG (after.root == before.root && after.templateIndex == before.templateIndex,
               "the detected chord changed under the riff");
}

//==============================================================================
// RL-22: a captured phrase becomes a user riff with its metadata filled in.
LUTHIER_TEST (Riffs, saveAsRiffAnalysesTheCapture)
{
    // A C major phrase at 100 bpm with a bend and a hammer-on.
    RiffBuilder b ("user.lick.x", 100.0);
    b.riff.keyRoot = "E";
    b.riff.scale = "chromatic";
    b.note (0.0, 1.0, 4, 3)            // C
     .note (1.0, 0.5, 3, 2)            // E
     .note (1.5, 0.5, 3, 3, { tech (T::hammerOn) })   // F
     .note (2.0, 1.0, 2, 0)            // G
     .note (3.0, 1.0, 2, 2)            // A
     .note (4.0, 1.0, 1, 1, { tech (T::bend, 1, 0, { { 0.0, 0.0 }, { 0.5, 1.0 }, { 1.0, 1.0 } }) })   // C
     .note (5.0, 1.0, 1, 0)            // B
     .note (6.0, 2.0, 4, 3);           // C
    b.riff.lengthBeats = 8.0;
    auto riff = b.build();

    RiffAnalysis::analyse (riff);
    CHECK_MSG (riff.keyRoot == "C" && riff.scale == "ionian", riff.keyRoot + " " + riff.scale);
    CHECK (riff.techniques == juce::StringArray ({ "bend", "hammer" }));
    CHECK_NEAR (riff.tempoBpm, 100.0, 1.0e-9);

    // The difficulty formula of section 4.
    const double seconds = 8.0 * 60.0 / 100.0;
    const double nps = 8.0 / seconds;
    const int expected = juce::jlimit (1, 5, juce::roundToInt (1.0 + nps / 3.0 + 2.0 / 3.0 + juce::jmax (0, 3 - 4) / 4.0));
    CHECK (riff.difficulty == expected);

    RiffLibrary library;
    const auto folder = scratch ("user");
    library.setFolders ({}, folder, folder.getChildFile ("library.json"));
    library.loadIndexNow();

    riff.meta.name = "Morning Phrase 1";
    juce::File written;
    const auto result = library.saveUserRiff (riff, &written);
    CHECK_MSG (result.wasOk(), result.getErrorMessage());
    CHECK (riff.meta.id.startsWith ("user.lick."));
    CHECK (written.existsAsFile());
    CHECK (library.findEntry (riff.meta.id) != nullptr);

    // Atomic: no temporary is left behind, and the file reads back.
    CHECK (folder.findChildFiles (juce::File::findFiles, false, "*.tmp").isEmpty());
    Riff back;
    CHECK (Riff::loadFromFile (written, back).wasOk());
    CHECK (back.toJson() == riff.toJson());

    // A save over an existing file that fails midway leaves the original.
    const auto before = written.loadFileAsString();
    {
        juce::TemporaryFile temp (written);
        temp.getFile().replaceWithText ("{ half a file");
        // Killed here: overwriteTargetFileWithTemporary never ran.
    }
    CHECK (written.loadFileAsString() == before);
}

//==============================================================================
// RL-23: search and filter over 1300 items match brute force, fast.
LUTHIER_TEST (Riffs, searchAndFilterMatchBruteForceQuickly)
{
    RiffLibrary library;
    library.setFolders (factoryFolder(), {}, {});
    library.loadIndexNow();

    std::vector<RiffIndexEntry> entries;

    for (int i = 0; i < library.getNumEntries(); ++i)
        entries.push_back (*library.getEntry (i));

    juce::Random random (1300);
    const auto& genres = RiffVocabulary::genres();

    for (int i = 0; i < 1000; ++i)
    {
        RiffIndexEntry e;
        e.id = "user.lick.generated-" + juce::String (i);
        e.name = "Generated Phrase " + juce::String (i);
        e.type = RiffVocabulary::types()[random.nextInt (4)];
        e.genre = genres[(size_t) random.nextInt ((int) genres.size())].id;
        e.instrument = e.type == "bass" ? "bass4" : "guitar6";
        e.tuning = e.type == "bass" ? std::vector<int> { 43, 38, 33, 28 } : std::vector<int> { 64, 59, 55, 50, 45, 40 };
        e.keyRoot = RiffVocabulary::rootName (random.nextInt (12));
        e.scale = "aeolian";
        e.tempoBpm = 60 + random.nextInt (140);
        e.difficulty = 1 + random.nextInt (5);
        e.factory = false;

        for (int k = 0; k < 3; ++k)
            e.techniques.addIfNotAlreadyThere (RiffVocabulary::noteTechniques()[random.nextInt (25)]);

        entries.push_back (e);
    }

    library.setEntriesForTesting (entries);
    CHECK (library.getNumEntries() == 1300);

    for (int trial = 0; trial < 50; ++trial)
    {
        RiffQuery q;
        q.genres.set ((size_t) random.nextInt (12));
        q.genres.set ((size_t) random.nextInt (12));
        q.techniques.set ((size_t) random.nextInt (25));
        q.minDifficulty = 1 + random.nextInt (3);
        q.maxDifficulty = q.minDifficulty + random.nextInt (3);
        q.minTempo = 60 + random.nextInt (60);
        q.maxTempo = q.minTempo + 80;

        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto got = library.query (q);
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;
        CHECK_MSG (elapsed <= 8.0, "query took " + juce::String (elapsed, 2) + " ms");

        std::set<int> expected;

        for (int i = 0; i < library.getNumEntries(); ++i)
        {
            const auto& e = *library.getEntry (i);
            bool hasAll = true;

            for (size_t bit = 0; bit < 64; ++bit)
                if (q.techniques.test (bit) && ! e.techniques.contains (RiffVocabulary::allTechniques()[(int) bit]))
                    hasAll = false;

            if (q.genres.test ((size_t) e.genreIndex) && hasAll && e.difficulty >= q.minDifficulty
                  && e.difficulty <= q.maxDifficulty && e.tempoBpm >= q.minTempo && e.tempoBpm <= q.maxTempo)
                expected.insert (i);
        }

        CHECK (std::set<int> (got.begin(), got.end()) == expected);
    }

    // "turn blu" is AND over words.
    RiffQuery text;
    text.text = "turn blu";
    std::vector<RiffIndexEntry> fixture { entries.front() };
    fixture.front().id = "blues.lick.delta-turnaround-3";
    fixture.front().name = "Delta Turnaround 3";
    fixture.front().genre = "blues";
    fixture.front().tags = {};
    library.setEntriesForTesting (fixture);
    CHECK (library.query (text).size() == 1);
    text.text = "turn jazz";
    CHECK (library.query (text).empty());
}

//==============================================================================
// RL-24: legal - names, tags and origin.
LUTHIER_TEST (Riffs, catalogNamesAreOriginalAndClean)
{
    RiffLibrary library;
    library.setFolders (factoryFolder(), {}, {});
    library.loadIndexNow();

    juce::StringArray deny;
    const auto denyFile = repoRoot().getChildFile ("Tools").getChildFile ("riffs").getChildFile ("deny.txt");

    for (auto line : juce::StringArray::fromLines (denyFile.loadFileAsString()))
    {
        line = line.upToFirstOccurrenceOf ("#", false, false).trim().toLowerCase();

        if (line.isNotEmpty())
            deny.add (line);
    }

    for (int i = 0; i < library.getNumEntries(); ++i)
    {
        const auto& e = *library.getEntry (i);
        CHECK_MSG (e.origin == "original" || e.origin == "factory", e.id);
        CHECK_MSG (e.name.length() < 32, e.name);
        CHECK_MSG (! e.name.containsIgnoreCase ("style of"), e.name);

        juce::StringArray texts (e.name);
        texts.addArray (e.tags);

        for (const auto& text : texts)
        {
            const auto words = " " + text.toLowerCase().replaceCharacter ('-', ' ') + " ";

            for (const auto& d : deny)
                CHECK_MSG (! words.contains (" " + d.replaceCharacter ('-', ' ') + " "), text + " matches " + d);
        }
    }
}

//==============================================================================
// RL-30: the performance budget.
LUTHIER_TEST (Riffs, playerCompileAndIndexStayInBudget)
{
    // Compile an 8-bar riff in at most 2 ms (median of several).
    RiffBuilder b;

    for (int i = 0; i < 128; ++i)
        b.note (i * 0.25, 0.25, i % 6, 3 + (i % 5), i % 8 == 0 ? std::vector<ScoreTechnique> { tech (T::bend, 1, 0, { { 0.0, 0.0 }, { 0.5, 1.0 } }) }
                                                              : std::vector<ScoreTechnique> {});

    b.riff.lengthBeats = 32.0;
    const auto riff = b.build();

    std::vector<double> times;

    for (int i = 0; i < 9; ++i)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto c = compileStandard (riff);
        times.push_back (juce::Time::getMillisecondCounterHiRes() - start);
        CHECK (c != nullptr);
    }

    std::sort (times.begin(), times.end());
    CHECK_MSG (times[4] <= 2.0, "compile took " + juce::String (times[4], 3) + " ms");

    // The player: a 16th-note riff with three simultaneous bends. performance-
    // budget 0.2's unit is 1% of a core at 48 kHz / 128, so 0.02 units is 0.02%
    // of real time: measured over a long run.
    RiffBuilder dense;

    for (int i = 0; i < 64; ++i)
        dense.note (i * 0.25, 0.25, i % 3, 5 + (i % 4), { tech (T::bend, 1, 0, { { 0.0, 0.0 }, { 0.5, 1.0 }, { 1.0, 0.0 } }), tech (T::vibrato, 5.5, 20) });

    dense.riff.lengthBeats = 16.0;
    RiffPlayer player;
    player.prepare (kSr);
    player.setCompiled (compileStandard (dense.build()), true);
    player.setClockMode (RiffPlayer::ClockMode::own);
    player.play();

    RiffPlayer::Output out;
    const int blocks = 20000;
    const auto start = juce::Time::getMillisecondCounterHiRes();

    for (int i = 0; i < blocks; ++i)
    {
        out.clear();
        player.renderSubBlock (128, 0.0, false, 0.0, out);
    }

    const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - start;
    const double realtimeMs = blocks * 128.0 / kSr * 1000.0;
    const double units = elapsedMs / realtimeMs * 100.0;   // % of one core
    CHECK_MSG (units <= 0.5, "player costs " + juce::String (units, 4) + " % of a core");

    // The index: 300 factory entries load in at most 150 ms.
    RiffLibrary library;
    library.setFolders (factoryFolder(), {}, {});
    const auto indexStart = juce::Time::getMillisecondCounterHiRes();
    library.loadIndexNow();
    const auto indexMs = juce::Time::getMillisecondCounterHiRes() - indexStart;
    CHECK_MSG (indexMs <= 150.0, "index load took " + juce::String (indexMs, 1) + " ms");
}

//==============================================================================
// RL-11 (shortened): swaps from another thread while the player runs, with
// the allocation counter watching the audio thread.
LUTHIER_TEST (Riffs, swapsFromAnotherThreadNeverAllocateOnTheAudioThread)
{
    RiffBuilder a, b ("test.lick.other-1");
    a.note (0.0, 1.0, 2, 5).note (1.0, 1.0, 2, 7, { tech (T::bend, 1, 0, { { 0.0, 0.0 }, { 1.0, 1.0 } }) });
    b.note (0.0, 0.5, 1, 5).note (0.5, 0.5, 1, 8, { tech (T::vibrato, 5.5, 30) });
    const auto ra = a.build(), rb = b.build();
    const auto ca = compileStandard (ra), cb = compileStandard (rb);

    RiffPlayer player;
    player.prepare (kSr);
    player.setCompiled (ca, true);
    player.setClockMode (RiffPlayer::ClockMode::own);
    player.play();

    std::atomic<bool> done { false };
    std::thread messageThread ([&]
    {
        for (int i = 0; i < 200; ++i)
        {
            player.setCompiled (i % 2 == 0 ? cb : ca, (i % 3) == 0);
            player.collectGarbage();
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
        }

        done = true;
    });

    RiffPlayer::Output out;
    int blocks = 0;

    while (! done.load())
    {
        out.clear();
        player.renderSubBlock (128, 0.0, false, 0.0, out);
        ++blocks;
    }

    messageThread.join();
    player.collectGarbage();
    CHECK (blocks > 0);
    CHECK (player.isPlaying());
}

// Fix (b): the audition phrase is handed over whole while the audio thread plays it.
LUTHIER_TEST (Riffs, auditionRestartsWhileProcessingIsRaceFree)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, 256);

    std::atomic<bool> done { false };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), 256);
        juce::MidiBuffer midi;

        while (! done.load())
        {
            buffer.clear();
            midi.clear();
            processor.processBlock (buffer, midi);
        }
    });

    for (int i = 0; i < 300; ++i)
    {
        processor.startAudition ((AuditionPhrase::Type) (i % (int) AuditionPhrase::Type::NumTypes));
        std::this_thread::sleep_for (std::chrono::microseconds (300));
    }

    done = true;
    audio.join();
    processor.stopAudition();
    processor.releaseResources();
    CHECK (true);
}

// RL-18: capture sees riff notes; live MIDI out does not.
LUTHIER_TEST (Riffs, captureSeesRiffNotesExactly)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, 256);

    auto& capture = processor.getPerformanceCapture();

    RiffBuilder b;
    b.note (0.0, 0.5, 2, 5).note (0.5, 0.5, 2, 7, { tech (T::hammerOn) }).note (1.0, 1.0, 4, 3, { tech (T::palmMute) });
    b.riff.lengthBeats = 4.0;
    const auto riff = b.build();

    // Live MIDI out on, strings included: a preview is not a performance.
    auto config = processor.getRouting().getMidiOutConfig();
    config.enabled = true;
    config.stringActivity = true;
    processor.getRouting().setMidiOutConfig (config);

    auto& player = processor.getEngine().getRiffPlayer();
    player.setCompiled (RiffCompiler::compile (riff, {}, RiffDestinations::guitarSummary (processor)), true);
    player.setClockMode (RiffPlayer::ClockMode::own);
    player.setLooping (false);
    player.play();

    juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), 256);
    int liveNotes = 0;

    for (int i = 0; i < 400; ++i)
    {
        juce::MidiBuffer midi;
        buffer.clear();
        processor.processBlock (buffer, midi);

        for (const auto m : midi)
            liveNotes += m.getMessage().isNoteOn() ? 1 : 0;
    }

    CHECK_MSG (liveNotes == 0, juce::String (liveNotes) + " riff notes reached live MIDI out");

    processor.getPerformanceCapture().drain();

    PerformanceScore score;
    CaptureScoreOptions options;   // no grid
    capture.toScore (score, options);

    std::vector<const ScoreNote*> notes;

    if (score.getNumTracks() > 0)
        for (const auto& m : score.getTrack (0).measures)
            for (const auto* n : m.collectNotes())
                notes.push_back (n);

    CHECK_MSG (notes.size() == 3, juce::String ((int) notes.size()) + " captured notes");

    if (notes.size() == 3)
    {
        CHECK (notes[0]->stringIndex == 2 && notes[0]->fret == 5);
        CHECK (notes[1]->stringIndex == 2 && notes[1]->fret == 7 && notes[1]->hasTechnique (T::hammerOn));
        CHECK (notes[2]->stringIndex == 4 && notes[2]->fret == 3 && notes[2]->hasTechnique (T::palmMute));
    }
}
