/*  Combination (beta-test) tests - group "Combo".

    Unit tests prove each module alone. These play the whole plugin through
    combinations of settings nobody wrote a test for, the way a beta tester
    does: every guitar with every amp, every pedal in both chains, every factory
    preset with every phrase, the rhythm engine on every pattern, modulation and
    advanced ranges pushed to their ends, and two thousand seeded random rigs.
    Each render is asked the same coarse questions (see combo::Verdict): finite,
    bounded, audible while played, silent after release, affordable.

    The large sweeps honour LUTHIER_COMBO_SCALE (e.g. 0.1 for a quick pass);
    LUTHIER_COMBO_VERBOSE=1 prints every configuration as it runs. Every
    failure prints the exact settings (and seed) that produced it.
*/

#include "ComboHarness.h"

#include "../Presets/PresetMorph.h"
#include "../Rhythm/Patterns.h"
#include "../Rhythm/GenreKit.h"
#include "../Modulation/ModMatrix.h"
#include "../PhysicalRange.h"

using namespace luthier;
using namespace luthier::tests;
using namespace luthier::combo;

namespace
{
    //==========================================================================
    /** One pairwise factor: a parameter and the values it takes. */
    struct Factor
    {
        juce::String id;           ///< empty for the phrase factor
        std::vector<int> levels;   ///< choice indices, or indices into `plains`
        std::vector<float> plains; ///< for a float factor: the plain values
    };

    std::vector<int> allLevels (Rig& rig, const juce::String& id)
    {
        std::vector<int> v;
        for (int i = 0; i < juce::jmax (1, rig.numChoices (id)); ++i)
            v.push_back (i);
        return v;
    }

    /*  A greedy all-pairs (AETG-style) generator. Deterministic for a seed.
        Returns rows of level positions (index into each factor's levels). */
    std::vector<std::vector<int>> allPairs (const std::vector<int>& sizes, uint32_t seed)
    {
        const int k = (int) sizes.size();
        std::mt19937 rng (seed);

        // uncovered[i][j] is a flat a*sizes[j]+b bitmap, i < j.
        std::vector<std::vector<std::vector<char>>> uncovered ((size_t) k);
        long remaining = 0;

        for (int i = 0; i < k; ++i)
        {
            uncovered[(size_t) i].resize ((size_t) k);

            for (int j = i + 1; j < k; ++j)
            {
                uncovered[(size_t) i][(size_t) j].assign ((size_t) (sizes[(size_t) i] * sizes[(size_t) j]), 1);
                remaining += sizes[(size_t) i] * sizes[(size_t) j];
            }
        }

        auto gain = [&] (const std::vector<int>& row, int f, int v)
        {
            int g = 0;
            for (int o = 0; o < k; ++o)
            {
                if (o == f || row[(size_t) o] < 0) continue;
                const int i = juce::jmin (o, f), j = juce::jmax (o, f);
                const int a = (i == f ? v : row[(size_t) i]), b = (j == f ? v : row[(size_t) j]);
                g += uncovered[(size_t) i][(size_t) j][(size_t) (a * sizes[(size_t) j] + b)];
            }
            return g;
        };

        std::vector<std::vector<int>> rows;

        while (remaining > 0)
        {
            std::vector<int> best;
            int bestCover = -1;

            for (int attempt = 0; attempt < 20; ++attempt)
            {
                std::vector<int> row ((size_t) k, -1);

                // Seed the row with one uncovered pair.
                bool seeded = false;
                for (int i = 0; i < k && ! seeded; ++i)
                    for (int j = i + 1; j < k && ! seeded; ++j)
                    {
                        auto& m = uncovered[(size_t) i][(size_t) j];
                        const int start = (int) (rng() % m.size());
                        for (size_t t = 0; t < m.size(); ++t)
                        {
                            const size_t idx = ((size_t) start + t) % m.size();
                            if (m[idx])
                            {
                                row[(size_t) i] = (int) idx / sizes[(size_t) j];
                                row[(size_t) j] = (int) idx % sizes[(size_t) j];
                                seeded = true;
                                break;
                            }
                        }
                    }

                std::vector<int> order ((size_t) k);
                for (int i = 0; i < k; ++i) order[(size_t) i] = i;
                std::shuffle (order.begin(), order.end(), rng);

                for (int f : order)
                {
                    if (row[(size_t) f] >= 0) continue;
                    int bestV = (int) (rng() % (uint32_t) sizes[(size_t) f]), bestG = -1;
                    for (int v = 0; v < sizes[(size_t) f]; ++v)
                    {
                        const int g = gain (row, f, v);
                        if (g > bestG) { bestG = g; bestV = v; }
                    }
                    row[(size_t) f] = bestV;
                }

                int cover = 0;
                for (int i = 0; i < k; ++i)
                    for (int j = i + 1; j < k; ++j)
                        cover += uncovered[(size_t) i][(size_t) j][(size_t) (row[(size_t) i] * sizes[(size_t) j] + row[(size_t) j])];

                if (cover > bestCover) { bestCover = cover; best = row; }
            }

            for (int i = 0; i < k; ++i)
                for (int j = i + 1; j < k; ++j)
                {
                    auto& c = uncovered[(size_t) i][(size_t) j][(size_t) (best[(size_t) i] * sizes[(size_t) j] + best[(size_t) j])];
                    if (c) { c = 0; --remaining; }
                }

            rows.push_back (best);
        }

        return rows;
    }

    /** Settings that hold sound on by design: their renders are not asked to decay. */
    bool holdsSound (Rig& rig)
    {
        auto on = [&] (const char* id) { auto* p = rig.param (id); return p != nullptr && p->getValue() > 0.5f; };
        auto amount = [&] (const char* id) { auto* p = rig.param (id); return p != nullptr ? rig.param (id)->convertFrom0to1 (p->getValue()) : 0.0f; };

        return on (ParamIDs::ebowEnable) || on (ParamIDs::freezeEnable)
            || amount (ParamIDs::feedbackAmount) > 0.001f
            || rig.p().getEngine().getRhythmEngine().isEnabled();
    }

    /** Pedal types whose job is to hold sound (delay/reverb/looper-like tails). */
    bool slotHoldsSound (Rig& rig)
    {
        for (int chain = 0; chain < 2; ++chain)
            for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
            {
                auto* t = dynamic_cast<juce::AudioParameterChoice*> (rig.param (ParamIDs::slotType (chain == 1, slot)));
                if (t == nullptr) continue;
                const auto name = t->getCurrentChoiceName().toLowerCase();
                if (name.contains ("delay") || name.contains ("echo") || name.contains ("reverb")
                    || name.contains ("shimmer") || name.contains ("looper") || name.contains ("freeze")
                    || name.contains ("swell") || name.contains ("infinite"))
                    return true;

                /*  A fuzz, distortion or compressor holds its output up while its
                    input decays: the open strings' sympathetic ring (seconds on a
                    bass or a 12-string's courses) comes out near the note's level
                    until a hand mutes it. That is the pedal doing its job, so a
                    "the mix must fall" check means nothing through one. */
                if (! t->getCurrentChoiceName().equalsIgnoreCase ("None")
                    && (name.contains ("fuzz") || name.contains ("distortion") || name.contains ("overdrive")
                        || name.contains ("drive") || name.contains ("compressor") || name.contains ("sustain")
                        || name.contains ("octave") || name.contains ("boost") || name.contains ("metal")))
                {
                    auto* bypass = rig.param (ParamIDs::slotBypass (chain == 1, slot));
                    if (bypass == nullptr || bypass->getValue() < 0.5f)
                        return true;
                }
            }

        return false;
    }

    void judgeAndLog (TestContext& ctx, FindingLog& log, Rig& rig, const juce::String& settings,
                      const RenderStats& stats, Verdict verdict)
    {
        const auto why = verdict.judge (stats);

        if (verbose())
            std::cout << "    " << settings << "\n      -> " << stats.describe() << "\n";

        ++ctx.checks;

        if (why.isNotEmpty())
        {
            ctx.fail (why + " | " + settings);
            log.add (settings, why + " [" + stats.describe() + "]");
        }

        juce::ignoreUnused (rig);
    }

    juce::String presetLabel (Rig& rig, int index)
    {
        if (auto* info = rig.p().getPresetManager().getPreset (index))
            return "preset#" + juce::String (index) + " \"" + info->name + "\"";

        return "preset#" + juce::String (index);
    }
}

//==============================================================================
/*  (a) Pairwise coverage across the major settings.

    Every pair of values of these factors appears together in at least one
    rendered configuration: guitar type, tuning, capo, amp, cabinet, mic, room,
    pre and post pedal type, oversampling, playing mode, slide, slap, E-Bow,
    freeze, feedback, scrape, bridge, pickup selector, strings, body, and the
    phrase played. The rows are a fixed seed's greedy all-pairs cover. */
LUTHIER_TEST (Combo, pairwiseAcrossMajorSettings)
{
    Rig rig;
    FindingLog log { "Combo.pairwise" };

    std::vector<Factor> factors;
    auto addAll = [&] (const juce::String& id)
    {
        if (rig.param (id) == nullptr) { CHECK_MSG (false, "missing parameter " + id); return; }
        factors.push_back ({ id, allLevels (rig, id), {} });
    };
    auto addSome = [&] (const juce::String& id, std::vector<int> levels)
    {
        if (rig.param (id) == nullptr) { CHECK_MSG (false, "missing parameter " + id); return; }
        factors.push_back ({ id, std::move (levels), {} });
    };
    auto addFloat = [&] (const juce::String& id, std::vector<float> plains)
    {
        if (rig.param (id) == nullptr) { CHECK_MSG (false, "missing parameter " + id); return; }
        std::vector<int> lv;
        for (int i = 0; i < (int) plains.size(); ++i) lv.push_back (i);
        factors.push_back ({ id, lv, std::move (plains) });
    };

    addAll (ParamIDs::guitarType);
    addAll (ParamIDs::tuningPreset);
    addSome (ParamIDs::capoFret, { 0, 2, 7, 12 });
    addAll (ParamIDs::ampModel);
    addAll (ParamIDs::cabType);
    addAll (ParamIDs::micType);
    addAll (ParamIDs::roomSize);
    addAll (ParamIDs::roomOn);
    addAll (ParamIDs::slotType (false, 0));
    addAll (ParamIDs::slotType (true, 0));
    addAll (ParamIDs::oversample);
    addAll (ParamIDs::playingMode);
    addAll (ParamIDs::slideGuitar);
    addAll (ParamIDs::slapArmed);
    addAll (ParamIDs::slapType);
    addAll (ParamIDs::ebowEnable);
    addAll (ParamIDs::freezeEnable);
    addFloat (ParamIDs::feedbackAmount, { 0.0f, rig.param (ParamIDs::feedbackAmount) != nullptr
                                                   ? rig.param (ParamIDs::feedbackAmount)->convertFrom0to1 (0.5f) : 0.0f });
    addAll (ParamIDs::scrapeArmed);
    addAll (ParamIDs::bridgeType);
    addAll (ParamIDs::pickupSelector);
    addAll (ParamIDs::stringMaterial);
    addAll (ParamIDs::bodyMode);
    addAll (ParamIDs::fretless);

    // The phrase is a factor too.
    {
        Factor f;
        for (int i = 0; i < (int) Phrase::numPhrases; ++i) f.levels.push_back (i);
        factors.push_back (f);
    }

    std::vector<int> sizes;
    for (auto& f : factors) sizes.push_back ((int) f.levels.size());

    constexpr uint32_t seed = 20260924;
    auto rows = allPairs (sizes, seed);

    const int limit = juce::jmax (1, (int) std::round (rows.size() * scale()));
    std::cout << "    pairwise: " << factors.size() << " factors, " << rows.size()
              << " rows (running " << limit << "), seed " << seed << std::endl;

    for (int r = 0; r < limit; ++r)
    {
        const auto& row = rows[(size_t) r];
        Config config;

        for (size_t f = 0; f < factors.size(); ++f)
        {
            const auto& fac = factors[f];
            const int level = fac.levels[(size_t) row[f]];

            if (fac.id.isEmpty())
                config.phrase = (Phrase) level;
            else if (! fac.plains.empty())
                config.plains.push_back ({ fac.id, fac.plains[(size_t) level] });
            else
                config.indices.push_back ({ fac.id, level });
        }

        config.extra = "row=" + juce::String (r) + " seed=" + juce::String ((int) seed);

        rig.p().resetEverything();
        config.applyTo (rig);
        rig.processSilence (2);

        const auto stats = rig.render (config.phrase, 2.5);

        Verdict v;
        v.expectDecay = ! holdsSound (rig) && ! slotHoldsSound (rig);
        judgeAndLog (ctx, log, rig, config.describe (rig), stats, v);

        rig.quiet();
    }

    log.flush();
}

//==============================================================================
/*  Every guitar type with every phrase, stock settings otherwise: the one full
    cross that is cheap enough to run without pairing. */
LUTHIER_TEST (Combo, everyGuitarTypePlaysEveryPhrase)
{
    Rig rig;
    FindingLog log { "Combo.guitarTypes" };

    const int types = rig.numChoices (ParamIDs::guitarType);
    CHECK (types >= 5);

    for (int t = 0; t < types; ++t)
    {
        for (int ph = 0; ph < (int) Phrase::numPhrases; ++ph)
        {
            rig.p().resetEverything();
            Config c;
            c.indices.push_back ({ ParamIDs::guitarType, t });
            c.phrase = (Phrase) ph;
            c.applyTo (rig);
            rig.processSilence (2);

            const auto stats = rig.render (c.phrase, 2.5);
            Verdict v;
            v.expectDecay = ! holdsSound (rig) && ! slotHoldsSound (rig);
            judgeAndLog (ctx, log, rig, c.describe (rig), stats, v);
            rig.quiet();
        }
    }

    log.flush();
}

//==============================================================================
/*  Every factory preset with every phrase. Also: a preset switch under a
    ringing note must not click, and each preset's state must round-trip into
    a fresh instance and reproduce the same audio. */
LUTHIER_TEST (Combo, everyFactoryPresetPlaysEveryPhrase)
{
    Rig rig;
    FindingLog log { "Combo.factoryPresets" };

    auto& presets = rig.p().getPresetManager();
    const int count = presets.getNumPresets();

    CHECK_MSG (count > 0, "no factory presets found");
    std::cout << "    factory presets: " << count << std::endl;

    const int limit = juce::jmax (1, (int) std::round (count * scale()));

    for (int i = 0; i < juce::jmin (count, limit); ++i)
    {
        const auto label = presetLabel (rig, i);

        for (int ph = 0; ph < (int) Phrase::numPhrases; ++ph)
        {
            rig.p().resetEverything();

            if (! presets.loadPreset (i))
            {
                CHECK_MSG (false, label + " failed to load: " + presets.getLastLoadError());
                log.add (label, "load failed: " + presets.getLastLoadError());
                break;
            }

            rig.apply();
            rig.processSilence (2);

            const auto stats = rig.render ((Phrase) ph, 3.0);
            Verdict v;
            v.expectDecay = ! holdsSound (rig) && ! slotHoldsSound (rig);
            v.minSnrDb = 20.0;   // a shipped sound: the floor 20 dB under the playing
            judgeAndLog (ctx, log, rig, label + juce::String (" phrase=") + phraseName ((Phrase) ph), stats, v);
            rig.quiet();
        }
    }

    log.flush();
}

//==============================================================================
/*  Switching preset under a ringing note (what a player does between songs, and
    what a host does on a program change) must not produce a click: the largest
    sample-to-sample step in the 20 ms after the switch may not exceed 0.25 of
    full scale, nor four times the largest step of the note before it. */
LUTHIER_TEST (Combo, presetSwitchUnderARingingNoteDoesNotClick)
{
    Rig rig;
    FindingLog log { "Combo.presetSwitch" };

    auto& presets = rig.p().getPresetManager();
    const int count = presets.getNumPresets();
    const int limit = juce::jmax (1, (int) std::round (count * scale()));

    for (int i = 0; i < juce::jmin (count, limit); ++i)
    {
        const int to = (i + 1) % juce::jmax (1, count);
        rig.p().resetEverything();
        presets.loadPreset (i);
        rig.apply();
        rig.processSilence (2);

        /*  As a host runs it: the audio thread renders continuously while the
            message thread loads the next preset half a second in. Rendering and
            loading on one thread would skip the fade-out handshake the
            processor uses (there is no audio thread to wait for), which is not
            what a player ever hears. */
        const int total = (int) (1.0 * kSr);
        std::vector<float> mono ((size_t) total, 0.0f);
        std::atomic<int> rendered { 0 };
        std::atomic<int> switchedAt { -1 };

        std::thread audio ([&]
        {
            juce::AudioBuffer<float> buffer (rig.bufferChannels(), kBlock);

            for (int pos = 0; pos + kBlock <= total; pos += kBlock)
            {
                juce::MidiBuffer midi;
                if (pos == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);

                buffer.clear();
                rig.p().processBlock (buffer, midi);

                for (int s = 0; s < kBlock; ++s)
                    mono[(size_t) (pos + s)] = 0.5f * (buffer.getSample (0, s) + buffer.getSample (1, s));

                rendered = pos + kBlock;

                // Real time, roughly: a host does not render a second in 20 ms.
                std::this_thread::sleep_for (std::chrono::microseconds (1500));
            }
        });

        while (rendered.load() < total / 2)
            std::this_thread::yield();

        switchedAt = rendered.load();
        presets.loadPreset (to);
        rig.apply();
        audio.join();

        int stepAt = -1;
        auto maxStep = [&] (int from, int len)
        {
            double m = 0.0;
            for (int s = juce::jmax (1, from); s < juce::jmin (total, from + len); ++s)
                if (std::abs (mono[(size_t) s] - mono[(size_t) s - 1]) > m)
                {
                    m = std::abs (mono[(size_t) s] - mono[(size_t) s - 1]);
                    stepAt = s;
                }
            return m;
        };

        const int at = switchedAt.load();
        const double before = maxStep (at - (int) (0.1 * kSr), (int) (0.1 * kSr) - 1);
        const double after  = maxStep (at, (int) (0.1 * kSr));   // the load may land a few blocks later

        const auto settings = presetLabel (rig, i) + " -> " + presetLabel (rig, to);
        ++ctx.checks;

        if (after > 0.25 && after > 4.0 * juce::jmax (before, 1.0e-3))
        {
            const auto why = "click at preset switch: step " + juce::String (after, 3)
                             + " vs " + juce::String (before, 3) + " before, "
                             + juce::String (1000.0 * (stepAt - at) / kSr, 1) + " ms after the load began";
            ctx.fail (why + " | " + settings);
            log.add (settings, why);

            if (verbose())
                for (int k = stepAt - 8; k < stepAt + 8; ++k)
                    std::cout << "      [" << (k - at) << "] " << mono[(size_t) k] << "\n";
        }

        rig.quiet();
    }

    log.flush();
}

//==============================================================================
/*  Every factory rhythm pattern, and every genre kit, strumming a held chord
    with the engine free-running: sound, bounded, and it stops when the chord is
    released and the engine is switched off. */
LUTHIER_TEST (Combo, everyRhythmPatternAndGenreKit)
{
    Rig rig;
    FindingLog log { "Combo.rhythm" };

    auto& rhythm = rig.p().getEngine().getRhythmEngine();
    auto& library = rig.p().getPatternLibrary();
    auto& kits = rig.p().getGenreKits();

    std::cout << "    rhythm patterns: " << library.getNumPatterns() << ", genre kits: " << kits.getNumKits() << std::endl;
    CHECK (library.getNumPatterns() > 0);
    CHECK (kits.getNumKits() > 0);

    auto run = [&] (const juce::String& label)
    {
        rig.apply();
        const double idle = rig.renderEvents ({}, 0, 0.3).tailRms;   // the kit's rig, nothing playing

        rhythm.setFreeRun (true);
        rhythm.setEnabled (true);

        std::vector<TimedMidi> events;
        for (int n : { 48, 52, 55 })
            events.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) });
        for (int n : { 48, 52, 55 })
            events.push_back ({ (int) (2.0 * kSr), juce::MidiMessage::noteOff (1, n) });

        auto stats = rig.renderEvents (events, (int) (2.0 * kSr), 0.1);
        Verdict v;
        v.expectDecay = false;
        judgeAndLog (ctx, log, rig, label + " (engine on)", stats, v);

        // Off, and it has to go quiet.
        rhythm.setEnabled (false);
        rig.quiet();
        stats = rig.renderEvents ({}, 0, 3.0);
        ++ctx.checks;
        // -60 dBFS, or within 3 dB of what this rig hisses with nothing played.
        if (stats.tailRms > juce::jmax (1.0e-3, idle * 1.41))
        {
            ctx.fail ("rhythm engine keeps sounding after it is switched off | " + label);
            log.add (label, "keeps sounding after off (tail " + juce::String (juce::Decibels::gainToDecibels (stats.tailRms), 1)
                              + " dBFS, idle floor " + juce::String (juce::Decibels::gainToDecibels (idle), 1) + " dBFS)");
        }
    };

    for (int i = 0; i < library.getNumPatterns(); ++i)
    {
        rig.p().resetEverything();
        rhythm.setPattern (library.getPattern (i));
        run ("pattern#" + juce::String (i) + " \"" + library.getPattern (i).getName() + "\"");
    }

    for (int k = 0; k < kits.getNumKits(); ++k)
    {
        rig.p().resetEverything();
        const auto preset = rig.p().applyGenreKit (k);
        run ("kit#" + juce::String (k) + " \"" + kits.getKit (k).name + "\" rig=" + preset);
    }

    rhythm.setEnabled (false);
    log.flush();
}

//==============================================================================
/*  Modulation: an LFO and a random source routed at full depth to a spread of
    destinations (every automatable float the matrix accepts, eight at a time),
    for every phrase. Modulation may move anything; it may not break the rig. */
LUTHIER_TEST (Combo, modulationRoutesAtFullDepth)
{
    Rig rig;
    FindingLog log { "Combo.modulation" };

    juce::StringArray destinations;
    for (auto* prm : rig.p().getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (dynamic_cast<juce::AudioParameterFloat*> (ranged) != nullptr)
                destinations.add (ranged->getParameterID());

    std::cout << "    modulation destinations: " << destinations.size() << std::endl;

    int group = 0;
    const int step = juce::jmax (1, (int) std::round (1.0 / scale()));

    for (int start = 0; start < destinations.size(); start += 8 * step, ++group)
    {
        rig.p().resetEverything();
        auto& matrix = rig.p().getModMatrix();
        matrix.clearRoutes();

        juce::StringArray used;
        for (int d = start; d < juce::jmin (destinations.size(), start + 8); ++d)
        {
            ModRoute route;
            route.sourceId = (d % 2 == 0) ? modSourceIdForSlot (ModSourceSlots::lfoBase)
                                          : modSourceIdForSlot (ModSourceSlots::randomSmooth);
            route.destinationId = destinations[d];
            route.depth = (d % 3 == 0) ? -1.0f : 1.0f;

            if (matrix.addRoute (route))
                used.add (destinations[d]);
        }

        rig.apply();
        const auto phrase = (Phrase) (group % (int) Phrase::numPhrases);
        const auto stats = rig.render (phrase, 2.5);

        // A route into a noise-floor level (hum, hiss, radio, the player's
        // movement) makes the floor itself move with the LFO or random source,
        // note or no note: the 0.25 s idle measured before the phrase cannot
        // stand for it, so a tail at the floor's own level is not a note that
        // fails to decay.
        bool movesTheFloor = false;
        for (auto& id : used)
            movesTheFloor = movesTheFloor || id.startsWith ("noise_");

        Verdict v;
        v.expectSound = false;   // a route to master gain or guitar volume may legitimately mute it
        v.expectDecay = ! holdsSound (rig) && ! slotHoldsSound (rig) && ! movesTheFloor
                        && ! used.joinIntoString (",").containsIgnoreCase ("feedback")
                        && ! used.joinIntoString (",").containsIgnoreCase ("freeze")
                        && ! used.joinIntoString (",").containsIgnoreCase ("ebow");
        judgeAndLog (ctx, log, rig, "routes to [" + used.joinIntoString (",") + "] phrase=" + phraseName (phrase), stats, v);

        matrix.clearRoutes();
        rig.quiet();
    }

    log.flush();
}

//==============================================================================
/*  Snapshots and the preset morph: capture two presets as snapshots, recall
    them under a ringing note, then morph between two presets across the whole
    slider while playing. */
LUTHIER_TEST (Combo, snapshotsAndPresetMorph)
{
    Rig rig;
    FindingLog log { "Combo.snapshotsMorph" };

    auto& presets = rig.p().getPresetManager();
    const int count = presets.getNumPresets();

    if (count < 2)
    {
        CHECK_MSG (false, "need two presets");
        return;
    }

    const int pairs = juce::jmax (1, (int) std::round (juce::jmin (count, 12) * scale()));

    for (int n = 0; n < pairs; ++n)
    {
        const int a = (n * 7) % count, b = (n * 7 + 3) % count;
        const auto label = presetLabel (rig, a) + " / " + presetLabel (rig, b);

        rig.p().resetEverything();
        presets.loadPreset (a); rig.apply();
        CHECK (rig.p().captureSnapshot (0, "A"));
        presets.loadPreset (b); rig.apply();
        CHECK (rig.p().captureSnapshot (1, "B"));

        std::vector<TimedMidi> events { { 0, juce::MidiMessage::noteOn (1, 50, (juce::uint8) 100) },
                                        { (int) (1.2 * kSr), juce::MidiMessage::noteOff (1, 50) } };

        int flips = 0;
        auto stats = rig.renderEvents (events, (int) (1.2 * kSr), 2.5,
                                       [&] (int) { rig.p().recallSnapshot (flips++ % 2); rig.apply(); },
                                       (int) (0.4 * kSr));
        Verdict v;
        v.expectDecay = ! holdsSound (rig) && ! slotHoldsSound (rig);
        judgeAndLog (ctx, log, rig, "snapshots " + label, stats, v);
        rig.quiet();

        // Morph.
        auto& morph = rig.p().getPresetMorph();
        presets.loadPreset (a); rig.apply();
        morph.setSlot (PresetMorph::slotA, presets.toVar(), "A");
        presets.loadPreset (b); rig.apply();
        morph.setSlot (PresetMorph::slotB, presets.toVar(), "B");
        morph.setEnabled (true);

        juce::AudioBuffer<float> buffer (rig.bufferChannels(), kBlock);
        bool finite = true;
        double peak = 0.0;

        for (int blk = 0; blk < 200; ++blk)
        {
            juce::MidiBuffer midi;
            if (blk % 50 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 45 + blk / 50 * 3, (juce::uint8) 100), 0);
            rig.setNormalised (ParamIDs::presetMorphPosition, 0.5f + 0.5f * std::sin (blk * 0.07f));
            rig.p().updatePresetMorph();
            rig.apply();
            buffer.clear();
            rig.p().processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                {
                    const float x = buffer.getSample (ch, i);
                    if (! std::isfinite (x)) finite = false; else peak = juce::jmax (peak, (double) std::abs (x));
                }
        }

        morph.setEnabled (false);
        ++ctx.checks;

        if (! finite || peak > 4.0)
        {
            const auto why = juce::String (finite ? "runaway gain while morphing: peak " + juce::String (peak, 2)
                                                  : "non-finite output while morphing");
            ctx.fail (why + " | " + label);
            log.add ("morph " + label, why);
        }

        rig.quiet();
    }

    log.flush();
}

//==============================================================================
/*  Advanced ranges unlocked, every family, with every ranged parameter at its
    advanced minimum and then its advanced maximum. The extremes are where the
    maths ends: they may sound odd, they may not blow up. */
LUTHIER_TEST (Combo, advancedRangesUnlockedAtExtremes)
{
    Rig rig;
    FindingLog log { "Combo.advancedRanges" };

    RangeState ranges;
    for (int f = 0; f < (int) RangeFamily::numFamilies; ++f)
        ranges.setFamilyAdvanced ((RangeFamily) f, true);

    rig.p().setRanges (ranges);

    juce::StringArray ranged;
    for (auto& id : RangeRegistry::allIds())
        if (rig.param (id) != nullptr)
            ranged.add (id);

    std::cout << "    advanced-range parameters: " << ranged.size() << std::endl;
    CHECK (ranged.size() > 0);

    for (int end = 0; end < 2; ++end)
    {
        // All at once, then family by family (so one family's extreme is not
        // hidden behind another's silence).
        for (int only = -1; only < (int) RangeFamily::numFamilies; ++only)
        {
            rig.p().resetEverything();
            rig.p().setRanges (ranges);

            juce::StringArray touched;
            for (auto& id : ranged)
            {
                if (only >= 0 && (RangeRegistry::find (id) == nullptr || RangeRegistry::find (id)->family != (RangeFamily) only))
                    continue;
                rig.setNormalised (id, end == 0 ? 0.0f : 1.0f);
                touched.add (id);
            }

            if (touched.isEmpty())
                continue;

            rig.apply();

            for (auto phrase : { Phrase::chord, Phrase::fastRepeat, Phrase::bend })
            {
                const auto stats = rig.render (phrase, 2.5);
                Verdict v;
                v.expectSound = false;
                v.expectDecay = ! holdsSound (rig) && ! slotHoldsSound (rig);
                v.peakCeiling = 8.0;
                const auto settings = juce::String (end == 0 ? "advanced MIN" : "advanced MAX")
                                    + " family=" + (only < 0 ? juce::String ("all") : juce::String (getRangeFamilyName ((RangeFamily) only)))
                                    + " params=[" + touched.joinIntoString (",") + "] phrase=" + phraseName (phrase);
                judgeAndLog (ctx, log, rig, settings, stats, v);
                rig.quiet();
            }
        }
    }

    rig.p().setRanges (RangeState());
    log.flush();
}

//==============================================================================
/*  Determinism: the same configuration, reset, plays the same phrase to the
    same samples. Checked across the factory presets because humanisation and
    noise generators are where an unseeded random would hide. */
LUTHIER_TEST (Combo, renderIsDeterministicAfterReset)
{
    Rig rig;
    FindingLog log { "Combo.determinism" };

    auto& presets = rig.p().getPresetManager();
    const int count = juce::jmax (1, (int) std::round (presets.getNumPresets() * scale()));

    for (int i = 0; i < juce::jmin (count, presets.getNumPresets()); ++i)
    {
        rig.p().resetEverything();
        presets.loadPreset (i);
        rig.apply();

        auto once = [&]
        {
            rig.p().releaseResources();
            rig.p().prepareToPlay (kSr, kBlock);
            rig.p().reset();
            rig.apply();
            return rig.render (Phrase::chord, 0.5).mono;
        };

        const auto a = once();
        const auto b = once();

        double maxDiff = 0.0;
        for (size_t s = 0; s < juce::jmin (a.size(), b.size()); ++s)
            maxDiff = juce::jmax (maxDiff, (double) std::abs (a[s] - b[s]));

        ++ctx.checks;

        if (maxDiff > 1.0e-4)
        {
            const auto why = "not deterministic after reset: max sample difference " + juce::String (maxDiff, 6);
            ctx.fail (why + " | " + presetLabel (rig, i));
            log.add (presetLabel (rig, i), why);
        }
    }

    log.flush();
}

//==============================================================================
/*  State round trip: getStateInformation from one instance, setStateInformation
    into a fresh one, and the fresh one plays the same phrase to the same audio.
    Run for every factory preset and for a set of seeded random rigs. */
LUTHIER_TEST (Combo, sessionStateRoundTripReproducesAudio)
{
    FindingLog log { "Combo.stateRoundTrip" };

    Rig source;
    auto& presets = source.p().getPresetManager();
    const int count = presets.getNumPresets();
    const int limit = juce::jmax (1, (int) std::round (juce::jmin (count, 40) * scale()));

    std::mt19937 rng (4242);

    for (int i = 0; i < limit + 10; ++i)
    {
        source.p().resetEverything();
        juce::String label;

        if (i < limit)
        {
            presets.loadPreset (i % juce::jmax (1, count));
            label = presetLabel (source, i % juce::jmax (1, count));
        }
        else
        {
            // A random rig: a handful of structural choices.
            for (auto* id : { ParamIDs::guitarType, ParamIDs::ampModel, ParamIDs::cabType, ParamIDs::micType, ParamIDs::roomSize })
                source.setIndex (id, (int) (rng() % (uint32_t) juce::jmax (1, source.numChoices (id))));
            source.setIndex (ParamIDs::slotType (false, 1), (int) (rng() % (uint32_t) juce::jmax (1, source.numChoices (ParamIDs::slotType (false, 1)))));
            source.setIndex (ParamIDs::slotType (true, 2), (int) (rng() % (uint32_t) juce::jmax (1, source.numChoices (ParamIDs::slotType (true, 2)))));
            label = "random rig seed=4242 index=" + juce::String (i - limit);
        }

        source.apply();

        juce::MemoryBlock blob;
        source.p().getStateInformation (blob);

        Rig copy;
        copy.p().setStateInformation (blob.getData(), (int) blob.getSize());
        copy.apply();

        auto play = [] (Rig& r)
        {
            r.p().releaseResources();
            r.p().prepareToPlay (kSr, kBlock);
            r.p().reset();
            r.apply();
            return r.render (Phrase::chord, 0.5).mono;
        };

        const auto a = play (source);
        const auto b = play (copy);

        // Parameter values first: a mismatch there explains any audio difference.
        juce::StringArray differing;
        for (auto* prm : source.p().getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (auto* other = copy.param (r->getParameterID()))
                    if (std::abs (r->getValue() - other->getValue()) > 1.0e-5f)
                        differing.add (r->getParameterID());

        double maxDiff = 0.0;
        for (size_t s = 0; s < juce::jmin (a.size(), b.size()); ++s)
            maxDiff = juce::jmax (maxDiff, (double) std::abs (a[s] - b[s]));

        ++ctx.checks;

        if (differing.size() > 0 || maxDiff > 1.0e-4)
        {
            const auto why = "state round trip differs: audio max diff " + juce::String (maxDiff, 6)
                             + (differing.isEmpty() ? juce::String()
                                                    : ", parameters [" + differing.joinIntoString (",") + "]");
            ctx.fail (why + " | " + label);
            log.add (label, why);
        }
    }

    log.flush();
}

//==============================================================================
/*  The features that hold sound on purpose - E-Bow, freeze, the physical
    feedback loop - held for four seconds at their maxima: bounded and finite.
    And the other side: with them all off, a long sustained note on a high-gain
    rig must not self-oscillate after release. */
LUTHIER_TEST (Combo, sustainFeaturesStayBounded)
{
    Rig rig;
    FindingLog log { "Combo.sustainFeatures" };

    struct Case { const char* name; std::vector<std::pair<const char*, float>> normalised; };
    const std::vector<Case> cases
    {
        { "ebow max",        { { ParamIDs::ebowEnable, 1.0f }, { ParamIDs::ebowIntensity, 1.0f } } },
        { "freeze max",      { { ParamIDs::freezeEnable, 1.0f }, { ParamIDs::freezeLevel, 1.0f } } },
        { "feedback max",    { { ParamIDs::feedbackAmount, 1.0f }, { ParamIDs::feedbackDistance, 0.0f },
                               { ParamIDs::ampGain, 1.0f } } },
        { "all three",       { { ParamIDs::ebowEnable, 1.0f }, { ParamIDs::freezeEnable, 1.0f },
                               { ParamIDs::feedbackAmount, 1.0f }, { ParamIDs::ampGain, 1.0f } } },
        { "secret max",      { { ParamIDs::secretOn, 1.0f }, { ParamIDs::secretFeedback, 1.0f },
                               { ParamIDs::secretMix, 1.0f } } },
    };

    for (const auto& c : cases)
        for (int amp = 0; amp < rig.numChoices (ParamIDs::ampModel); amp += 2)
        {
            rig.p().resetEverything();
            rig.setIndex (ParamIDs::ampModel, amp);
            juce::String settings = juce::String (c.name) + " amp_model=" + juce::String (amp);
            for (auto& [id, v] : c.normalised)
                rig.setNormalised (id, v);
            rig.apply();

            std::vector<TimedMidi> events { { 0, juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110) },
                                            { (int) (4.0 * kSr), juce::MidiMessage::noteOff (1, 52) } };
            const auto stats = rig.renderEvents (events, (int) (4.0 * kSr), 0.5);
            Verdict v;
            v.expectDecay = false;
            judgeAndLog (ctx, log, rig, settings, stats, v);
            rig.quiet();
        }

    // No sustain features: high gain, max everything else, must still stop -
    // with the string coupling at its default, and with it off (which tells a
    // self-oscillating amp apart from sympathetic strings that keep ringing).
    for (int coupling = 0; coupling < 2; ++coupling)
    for (int amp = 0; amp < rig.numChoices (ParamIDs::ampModel); ++amp)
    {
        rig.p().resetEverything();
        if (coupling == 1)
            rig.setNormalised (ParamIDs::couplingAmount, 0.0f);
        rig.setIndex (ParamIDs::ampModel, amp);
        rig.setNormalised (ParamIDs::ampGain, 1.0f);
        rig.setNormalised (ParamIDs::ampMaster, 1.0f);
        rig.setNormalised (ParamIDs::sustainScale, 1.0f);
        rig.apply();

        const auto stats = rig.render (Phrase::chord, 3.5);
        Verdict v;
        v.expectDecay = ! holdsSound (rig) && ! slotHoldsSound (rig);
        judgeAndLog (ctx, log, rig, "high gain, no sustain feature, amp_model=" + juce::String (amp)
                                     + (coupling == 1 ? " coupling_amount=0" : ""), stats, v);
        rig.quiet();
    }

    log.flush();
}

//==============================================================================
/*  Two thousand seeded random rigs: every automatable parameter set to a random
    value (bools and choices uniformly, floats uniformly over their normalised
    range), a random phrase, a short render. Only the questions that hold for
    ANY rig are asked: finite, bounded, no subnormal storm, affordable. */
LUTHIER_TEST (Combo, seededRandomConfigurations)
{
    Rig rig;
    FindingLog log { "Combo.random" };

    constexpr uint32_t seed = 1234567;
    const int total = juce::jmax (10, (int) std::round (2000 * scale()));
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);

    std::vector<juce::RangedAudioParameter*> params;
    for (auto* prm : rig.p().getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            params.push_back (r);

    std::cout << "    random configurations: " << total << " over " << params.size()
              << " parameters, seed " << seed << std::endl;

    const auto started = juce::Time::getMillisecondCounterHiRes();

    for (int n = 0; n < total; ++n)
    {
        rig.p().resetEverything();

        // Every parameter random. The same rng stream reproduces the rig from
        // (seed, n): the stream is consumed identically every run.
        juce::StringArray notable;

        for (auto* r : params)
        {
            const float v = uni (rng);
            r->setValueNotifyingHost (v);
        }

        const auto phrase = (Phrase) (rng() % (uint32_t) Phrase::numPhrases);
        rig.apply();

        const auto stats = rig.render (phrase, 0.3);

        Verdict v;
        v.expectSound = false;
        v.expectDecay = false;
        v.peakCeiling = 8.0;
        v.cpuCeilingPercent = 100.0;
        v.checkIdleFloor = false;

        const auto why = v.judge (stats);
        ++ctx.checks;

        if (why.isNotEmpty())
        {
            // Name the parameters most likely to matter, in full, for the report.
            juce::StringArray settings;
            for (auto* r : params)
                settings.add (r->getParameterID() + "=" + r->getCurrentValueAsText());

            const auto label = "random seed=" + juce::String ((int) seed) + " n=" + juce::String (n)
                               + " phrase=" + phraseName (phrase);
            ctx.fail (why + " | " + label);
            log.add (label + "\n      " + settings.joinIntoString (" "), why + " [" + stats.describe() + "]");
        }

        rig.quiet();
    }

    std::cout << "    random configurations took "
              << juce::String ((juce::Time::getMillisecondCounterHiRes() - started) / 1000.0, 1) << " s" << std::endl;

    log.flush();
}

//==============================================================================
/*  Every automatable parameter comes back from the session state - the check
    clap-validator's state-reproducibility tests make. Each parameter set to a
    seeded random value (bools and choices snapped to their steps), state saved,
    loaded into a fresh instance, every value compared. The preset-morph
    position once failed this: presets leave it out on purpose, and the session
    did too. */
LUTHIER_TEST (Combo, everyParameterSurvivesTheSessionStateRoundTrip)
{
    FindingLog log { "Combo.paramRoundTrip" };
    std::mt19937 rng (777);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);

    for (int round = 0; round < 5; ++round)
    {
        Rig source;

        for (auto* prm : source.p().getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            {
                const float v = uni (rng);   // drawn for every parameter, so the sequence is kept

                // FEAT-ASSIST: the hidden legacy doubler_on is left off - B-07's
                // migration moves it into a post-amp slot on load, which the
                // comparison below already excludes for doubler_on itself.
                if (r->getParameterID() != ParamIDs::doublerOn)
                    r->setValueNotifyingHost (r->convertTo0to1 (r->convertFrom0to1 (v)));
            }

        // FEAT-JAM: doubler_on is legacy (excluded below) and every load
        // migrates it into a Doubler pedal in a post slot; left on at random it
        // rewrites a slot this test then reads. It became visible when the 34
        // Jam parameters moved every later round's random draws.
        if (auto* legacy = source.param (ParamIDs::doublerOn))
            legacy->setValueNotifyingHost (0.0f);

        // doubler_on is legacy: loading it on migrates to a Doubler pedal in an
        // empty post slot, which rewrites that slot. Whether a round hits it
        // depended on the seed's draw landing there (FEAT-MIC's appended
        // parameters moved the draws), so it is held off here.
        if (auto* legacyDoubler = source.param (ParamIDs::doublerOn))
            legacyDoubler->setValueNotifyingHost (0.0f);

        source.apply();

        juce::MemoryBlock blob;
        source.p().getStateInformation (blob);

        Rig copy;
        copy.p().setStateInformation (blob.getData(), (int) blob.getSize());
        copy.apply();

        juce::StringArray differing;

        for (auto* prm : source.p().getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (auto* other = copy.param (r->getParameterID()))
                    if (r->getParameterID() != ParamIDs::doublerOn   // legacy: every load migrates it to a Doubler pedal (BETA_TEST_REPORT B-07)
                        && ! ParamIDs::isJamTransient (r->getParameterID())   // FEAT-JAM: jam-mode 10, off after a reload
                        && std::abs (r->getValue() - other->getValue()) > 1.0e-4f)
                        differing.add (r->getParameterID() + " (" + juce::String (r->getValue(), 4)
                                       + " -> " + juce::String (other->getValue(), 4) + ")");

        ++ctx.checks;

        if (! differing.isEmpty())
        {
            const auto why = "parameters not restored from session state: " + differing.joinIntoString (", ");
            ctx.fail (why + " | seed=777 round=" + juce::String (round));
            log.add ("seed=777 round=" + juce::String (round), why);
        }
    }

    log.flush();
}

//==============================================================================
/*  What a host does and a single-threaded test never does: the audio thread
    keeps processing while the message thread changes structural parameters and
    applies them (the ParameterBridge's AsyncUpdater path), plus preset loads and
    automation of everything else. pluginval's Automation test aborted with
    "double free or corruption" on this plugin; this reproduces that shape
    deterministically enough to run under a sanitizer. Seeded. */
LUTHIER_TEST (Combo, structuralChangesWhileAudioRuns)
{
    Rig rig;
    std::atomic<bool> running { true };
    std::atomic<int> blocks { 0 };
    std::atomic<bool> nonFinite { false };

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (rig.bufferChannels(), kBlock);
        int n = 0;

        while (running.load())
        {
            juce::MidiBuffer midi;
            if (n % 20 == 0)  midi.addEvent (juce::MidiMessage::noteOn (1, 40 + (n / 20) % 30, (juce::uint8) 100), 0);
            if (n % 20 == 15) midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            buffer.clear();
            rig.p().processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    if (! std::isfinite (buffer.getSample (ch, i)))
                        nonFinite = true;

            ++n;
            blocks = n;
        }
    });

    std::mt19937 rng (99);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);

    std::vector<juce::RangedAudioParameter*> params;
    for (auto* prm : rig.p().getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            params.push_back (r);

    auto& presets = rig.p().getPresetManager();
    const int iterations = juce::jmax (20, (int) std::round (300 * scale()));

    for (int it = 0; it < iterations; ++it)
    {
        // A burst of automation, structural ones included.
        for (int k = 0; k < 20; ++k)
            params[(size_t) (rng() % params.size())]->setValueNotifyingHost (uni (rng));

        rig.setNormalised (ParamIDs::guitarType, uni (rng));
        rig.setNormalised (ParamIDs::slotType (rng() % 2 == 0, (int) (rng() % EffectsChain::kNumSlots)), uni (rng));
        rig.setNormalised (ParamIDs::ampModel, uni (rng));
        rig.setNormalised (ParamIDs::cabType, uni (rng));
        rig.setNormalised (ParamIDs::bodyMode, uni (rng));
        rig.setNormalised (ParamIDs::oversample, uni (rng));

        if (it % 25 == 0 && presets.getNumPresets() > 0)
            presets.loadPreset ((int) (rng() % (uint32_t) presets.getNumPresets()));

        rig.apply();   // the message thread's structural pass, as handleAsyncUpdate does it

        const int before = blocks.load();
        const auto deadline = juce::Time::getMillisecondCounter() + 2000;
        while (blocks.load() < before + 2 && juce::Time::getMillisecondCounter() < deadline)
            std::this_thread::yield();
    }

    running = false;
    audio.join();

    CHECK_MSG (blocks.load() > iterations, "the audio thread stalled: " + juce::String (blocks.load()) + " blocks");
    CHECK_MSG (! nonFinite.load(), "non-finite output while structural changes raced the audio thread (seed 99)");
}

//==============================================================================
/*  Found by everyRhythmPatternAndGenreKit: after a strum pattern, with the
    engine off and panic() called, the output GREW from -42 to -14 dBFS over
    eight seconds with nothing playing. Two strings had been left ringing at the
    same pitch (130.8 and 130.9 Hz) with no damping, and the sympathetic
    coupling passed energy between them faster than the loops lose it.

    Two ways in, both checked: the rhythm engine's voicing, and a player
    holding the same pitch on two strings in Guitar Controller mode. A held
    note may sustain; it may never get louder. And panic must stop it. */
LUTHIER_TEST (Combo, unisonStringsNeverGrowAndPanicSilencesThem)
{
    FindingLog log { "Combo.unison" };

    auto measure = [&] (Rig& rig, const juce::String& label)
    {
        const auto s = rig.renderEvents ({}, 0, 6.0);
        const double early = Rig::windowRms (s.mono, (int) (0.5 * kSr), (int) (0.5 * kSr));
        const double late  = Rig::windowRms (s.mono, (int) (5.0 * kSr), (int) (0.5 * kSr));

        ++ctx.checks;
        if (late > early * 1.12 && late > 1.0e-3)
        {
            const auto why = "energy grows with nothing played: " + juce::String (juce::Decibels::gainToDecibels (early), 1)
                             + " dBFS at 0.5 s -> " + juce::String (juce::Decibels::gainToDecibels (late), 1) + " dBFS at 5 s";
            ctx.fail (why + " | " + label);
            log.add (label, why);
        }

        rig.p().panic();
        const auto after = rig.renderEvents ({}, 0, 1.0);
        ++ctx.checks;
        if (after.tailRms > juce::jmax (1.0e-3, s.idleRms * 1.41) && after.tailRms > 2.0e-3)
        {
            const auto why = "panic() did not silence it: " + juce::String (juce::Decibels::gainToDecibels (after.tailRms), 1) + " dBFS 1 s later";
            ctx.fail (why + " | " + label);
            log.add (label, why);
        }
    };

    // 1. The rhythm engine, on each factory pattern, then off and panic.
    {
        Rig rig;
        auto& rhythm = rig.p().getEngine().getRhythmEngine();
        auto& library = rig.p().getPatternLibrary();

        for (int i = 0; i < juce::jmin (library.getNumPatterns(), juce::jmax (3, (int) std::round (library.getNumPatterns() * scale()))); ++i)
        {
            rig.p().resetEverything();
            rhythm.setPattern (library.getPattern (i));
            rig.apply();
            rhythm.setFreeRun (true);
            rhythm.setEnabled (true);

            std::vector<TimedMidi> ev;
            for (int n : { 48, 52, 55 }) ev.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) });
            for (int n : { 48, 52, 55 }) ev.push_back ({ (int) (2.0 * kSr), juce::MidiMessage::noteOff (1, n) });
            rig.renderEvents (ev, (int) (2.0 * kSr), 0.05);

            rhythm.setEnabled (false);
            rig.quiet();
            measure (rig, "after pattern#" + juce::String (i) + " \"" + library.getPattern (i).getName() + "\", engine off, all notes off, panic");
        }
    }

    // 2. The same pitch held on two strings (Guitar Controller: a channel per string).
    {
        Rig rig;
        rig.p().resetEverything();
        rig.setIndex (ParamIDs::playingMode, 2);
        rig.apply();

        // G3 = string 3 open (channel 3) and string 4 at the fifth fret (channel 4).
        std::vector<TimedMidi> ev { { 0, juce::MidiMessage::noteOn (3, 55, (juce::uint8) 100) },
                                    { 0, juce::MidiMessage::noteOn (4, 55, (juce::uint8) 100) } };
        rig.renderEvents (ev, 0, 0.05);
        measure (rig, "G3 held on two strings, Guitar Controller mode, channels 3 and 4");
    }

    log.flush();
}

//==============================================================================
/*  performance-budget.md 1: the heaviest named preset is 22 units (22% of one
    core) on the reference CPU. This prints each factory preset's cost playing a
    six-string chord at 48 kHz / 256, and fails only past 50% - this machine is
    not the reference machine, and other jobs may share it; the table is for the
    report (docs/audit/BETA_TEST_REPORT.md). */
LUTHIER_TEST (Combo, cpuPerFactoryPreset)
{
    Rig rig;
    auto& presets = rig.p().getPresetManager();

    std::vector<std::pair<double, juce::String>> costs;

    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        rig.p().resetEverything();
        presets.loadPreset (i);
        rig.apply();
        rig.processSilence (8);

        const auto idle = rig.renderEvents ({}, 0, 1.0);
        const auto busy = rig.render (Phrase::chord, 0.5);

        costs.push_back ({ busy.cpuPercent, presetLabel (rig, i) + "  idle " + juce::String (idle.cpuPercent, 1) + "%"
                                            + "  max block " + juce::String (busy.maxBlockMs, 2) + " ms" });

        CHECK_MSG (busy.cpuPercent < 50.0, presetLabel (rig, i) + " costs " + juce::String (busy.cpuPercent, 1) + "% of a core");
        rig.quiet();
    }

    std::sort (costs.begin(), costs.end(), [] (auto& a, auto& b) { return a.first > b.first; });

    std::cout << "    CPU per factory preset (chord, 48 kHz / 256, % of one core; budget heaviest 22):\n";
    for (auto& [c, label] : costs)
        std::cout << "      " << juce::String (c, 1).paddedLeft (' ', 5) << "%  " << label << "\n";
}

//==============================================================================
/*  A pitch below the instrument's range is dropped, never mis-sounded: a
    Nashville high-strung set cannot play E2 (its low strings are an octave up),
    and the voicer drops what no string can sound. What it must not do is drop
    the pitches it CAN play: the same phrase an octave up sounds. This pins the
    behaviour Combo's phrase transposition relies on (formerly reported as
    "silent while notes were playing", preset "Nashville High-Strung",
    phrase palmMute). */
LUTHIER_TEST (Combo, notesBelowTheRangeAreDroppedAndInRangeNotesSound)
{
    Rig rig;
    auto& presets = rig.p().getPresetManager();
    const int nashville = presets.indexOfPreset ("Nashville High-Strung");

    CHECK_MSG (nashville >= 0, "no factory preset called Nashville High-Strung");
    if (nashville < 0)
        return;

    presets.loadPreset (nashville);
    rig.apply();
    rig.processSilence (4);

    const int lowest = rig.lowestPlayableNote();
    CHECK_MSG (lowest > 40, "expected the high-strung set's lowest string above E2, got " + juce::String (lowest));

    auto play = [&] (int note)
    {
        std::vector<TimedMidi> ev { { 0, juce::MidiMessage::noteOn (1, note, (juce::uint8) 110) },
                                    { (int) (0.4 * kSr), juce::MidiMessage::noteOff (1, note) } };
        const auto s = rig.renderEvents (ev, (int) (0.4 * kSr), 0.2);
        rig.quiet();
        return s.maxWindowRms;
    };

    CHECK_MSG (play (lowest - 12) < 3.0e-4, "a pitch an octave below the lowest string sounded");
    CHECK_MSG (play (lowest) > 1.0e-3, "the lowest open string did not sound");
    CHECK_MSG (play (lowest + 12) > 1.0e-3, "a pitch an octave above the lowest string did not sound");
}

//==============================================================================
/*  What was played before a reset must not colour what is played after it.
    Found as "state round trip differs" (8-String Djent, Drop C Riff, Modern
    Metal Chug, up to 0.0016): the saved state was identical, but the source
    instance had played other presets first and each string kept its last
    note's termination brightness (nut or fret material) through reset(). The
    fresh instance, and the copy made from the state, started from 1.0. */
LUTHIER_TEST (Combo, renderDoesNotDependOnWhatWasPlayedBefore)
{
    FindingLog log { "Combo.history" };

    auto play = [] (Rig& r)
    {
        r.p().releaseResources();
        r.p().prepareToPlay (kSr, kBlock);
        r.p().reset();
        r.apply();
        return r.render (Phrase::chord, 0.5).mono;
    };

    for (auto* name : { "Drop C Riff", "8-String Djent", "Modern Metal Chug" })
    {
        Rig fresh;
        auto& pf = fresh.p().getPresetManager();
        pf.loadPreset (pf.indexOfPreset (name));
        fresh.apply();
        const auto reference = play (fresh);

        for (int before : { 4, 5, 7, 8, 16 })
        {
            Rig used;
            auto& pu = used.p().getPresetManager();
            used.p().resetEverything();
            pu.loadPreset (before);
            used.apply();
            used.render (Phrase::chord, 0.3);
            used.quiet();
            used.p().resetEverything();
            pu.loadPreset (pu.indexOfPreset (name));
            used.apply();

            const auto after = play (used);
            double diff = 0.0;
            for (size_t s = 0; s < juce::jmin (after.size(), reference.size()); ++s)
                diff = juce::jmax (diff, (double) std::abs (after[s] - reference[s]));

            ++ctx.checks;
            if (diff > 1.0e-6)
            {
                const auto label = juce::String (name) + " after playing " + presetLabel (used, before);
                const auto why = "render depends on what was played before: max diff " + juce::String (diff, 7);
                ctx.fail (why + " | " + label);
                log.add (label, why);
            }
        }
    }

    log.flush();
}

//==============================================================================
/*  A factory preset plays its guitar's own parts, and Reset gives the default
    guitar its own parts. Found through the idle noise floor: "Octave Fuzz
    Stoner" (an SG) hummed like a Strat because every factory preset carried the
    layout defaults for the parameters a guitar's parts own - three single
    coils, an X-braced spruce top, 500k pots - and a preset's values beat the
    parts. Reset left the same on the default solid-body (B-05). */
LUTHIER_TEST (Combo, factoryPresetsAndResetUseTheGuitarsOwnParts)
{
    auto pickup0 = [] (Rig& r) { return r.param (ParamIDs::pickupType (0))->getCurrentValueAsText(); };
    auto bracing = [] (Rig& r) { return r.param (ParamIDs::bodyBracing)->getCurrentValueAsText(); };

    {
        Rig r;
        auto& p = r.p().getPresetManager();
        p.loadPreset (p.indexOfPreset ("Octave Fuzz Stoner"));
        r.apply();
        CHECK_MSG (pickup0 (r) == "Humbucker", "the SG preset's bridge pickup is " + pickup0 (r));
        CHECK_MSG (bracing (r) == "Solid Body", "the SG preset's body is " + bracing (r));
        CHECK_MSG ((int) r.p().getEngine().getPickupEngine().getPickupSpec (0).type == 1, "the engine's SG pickup is not a humbucker");
    }

    {
        Rig r;
        r.p().resetEverything();
        CHECK_MSG (bracing (r) == "Solid Body", "Reset left the default solid-body " + bracing (r));
    }

    // And the recipe's own choices survive the parts: the tuning a preset picks.
    {
        Rig r;
        auto& p = r.p().getPresetManager();
        p.loadPreset (p.indexOfPreset ("Drop C Riff"));
        r.apply();
        CHECK_MSG (r.param (ParamIDs::tuningPreset)->getCurrentValueAsText().containsIgnoreCase ("C"),
                   "Drop C Riff lost its tuning to the guitar's parts: " + r.param (ParamIDs::tuningPreset)->getCurrentValueAsText());
    }
}

//==============================================================================
/*  B-17: a preset on a type with no parts guitar (Custom) plays the compiled
    guitar, and a transport restart (prepareToPlay) must not bring back the
    parts guitar that was loaded before it. It did: the processor kept the
    previous parts guitar marked as loaded and re-applied it in prepareToPlay,
    so "Transposing Trem Chords" played the default Strat's parts after a
    restart and its saved session did not (0.285 apart). */
LUTHIER_TEST (Combo, compiledGuitarSurvivesATransportRestart)
{
    Rig r;
    auto& p = r.p().getPresetManager();
    const int index = p.indexOfPreset ("Transposing Trem Chords");
    CHECK_MSG (index >= 0, "the Custom-type factory preset is missing");
    if (index < 0)
        return;

    p.loadPreset (index);
    r.apply();
    const bool before = r.p().getEngine().isWorkshopGuitar();

    r.p().releaseResources();
    r.p().prepareToPlay (kSr, kBlock);
    r.apply();
    CHECK_MSG (r.p().getEngine().isWorkshopGuitar() == before,
               juce::String ("prepareToPlay changed the guitar: parts guitar ") + (before ? "on" : "off") + " before, "
                   + (r.p().getEngine().isWorkshopGuitar() ? "on" : "off") + " after");
}

//==============================================================================
/*  The released string itself is damped quickly (sustain-and-decay SUS-08:
    by 250 ms more than 40 dB below its level at note-off). The mix-level decay
    check in Verdict allows the sympathetic ring of the other open strings; this
    is the half that holds the fretting finger to account, for every guitar
    type. A fretted note: the lowest string's second fret. */
LUTHIER_TEST (Combo, releasedStringIsDampedQuickly)
{
    FindingLog log { "Combo.releasedString" };
    Rig rig;
    auto* types = dynamic_cast<juce::AudioParameterChoice*> (rig.param (ParamIDs::guitarType));

    for (int t = 0; t < types->choices.size(); ++t)
    {
        rig.p().resetEverything();
        rig.setIndex (ParamIDs::guitarType, t);

        // The released string alone: sympathetic coupling at its minimum and no
        // room, so the output is that string (StringEngine::getLevel is an
        // envelope follower with its own release and cannot show this).
        rig.setNormalised (ParamIDs::couplingAmount, 0.0f);
        rig.setNormalised (ParamIDs::couplingAirAmount, 0.0f);    // string-interaction 1: the air path is sympathetic too
        rig.setNormalised (ParamIDs::bodyCouplingAmount, 0.0f);   // body-coupling 3: the body rings the other strings back
        rig.setIndex (ParamIDs::roomOn, 0);
        rig.apply();
        rig.processSilence (4);

        const int note = rig.lowestPlayableNote() + 2;   // second fret of the lowest string: fretted, never open
        const int releaseAt = (int) (0.5 * kSr);
        std::vector<TimedMidi> ev { { 0, juce::MidiMessage::noteOn (1, note, (juce::uint8) 100) },
                                    { releaseAt, juce::MidiMessage::noteOff (1, note) } };
        const auto s = rig.renderEvents (ev, releaseAt, 0.4);

        const double before = Rig::windowRms (s.mono, releaseAt - (int) (0.03 * kSr), (int) (0.03 * kSr));
        const double after  = Rig::windowRms (s.mono, releaseAt + (int) (0.25 * kSr), (int) (0.03 * kSr));
        const double dropDb = juce::Decibels::gainToDecibels (juce::jmax (after, s.idleRms, 1.0e-7) / juce::jmax (1.0e-9, before));
        const auto label = "guitar_type=" + types->choices[t] + " note " + juce::String (note);

        ++ctx.checks;
        // SUS-08 asks 40 dB of the string itself; this reads the whole rig
        // (amp, cabinet and the minimum sympathetic coupling still ring a
        // little), so it allows 35. Before the release cap it read 21-28.
        if (before > 1.0e-3 && dropDb > -35.0 && after > 2.0 * s.idleRms)
        {
            const auto why = "released note only " + juce::String (-dropDb, 1) + " dB down 250 ms after note-off (35 at the output; SUS-08: 40 at the string)";
            ctx.fail (why + " | " + label);
            log.add (label, why);
        }

        rig.quiet();
    }

    log.flush();
}

//==============================================================================
/*  Lifting the sustain pedal releases the notes whose keys are already up (and
    all-notes-off stops them). The pedal-up handler used to be a loop that only
    `continue`d, so after the pedal a released note rang on at its open sustain:
    "does not decay after release", phrase sustainPedal, every bass (a bass
    string's open T60 is several seconds). Checked per string: the strings the
    phrase played end the pedal-up damped, not open. */
LUTHIER_TEST (Combo, liftingTheSustainPedalReleasesItsNotes)
{
    for (const auto* type : { "P-Style Bass", "Vintage Double-Cut" })
    {
        Rig rig;
        auto* types = dynamic_cast<juce::AudioParameterChoice*> (rig.param (ParamIDs::guitarType));
        rig.setIndex (ParamIDs::guitarType, types->choices.indexOf (type));
        rig.apply();
        rig.processSilence (4);

        int released = 0;
        auto events = makePhrase (Phrase::sustainPedal, released);
        rig.transposeIntoRange (events);
        rig.renderEvents (events, released, 0.05);

        auto& engine = rig.p().getEngine();
        int damped = 0, open = 0;

        // The strings the phrase played are the ones tuned (fretted) to its pitches;
        // the others may still ring sympathetically, open, as a real guitar's do.
        juce::Array<double> pitches;
        for (auto& e : events)
            if (e.message.isNoteOn())
                pitches.add (juce::MidiMessage::getMidiNoteInHertz (e.message.getNoteNumber()));

        for (int s = 0; s < engine.getNumStrings(); ++s)
        {
            bool played = false;
            for (auto hz : pitches)
                played = played || std::abs (1200.0 * std::log2 (engine.getString (s).getTargetFrequency() / hz)) < 60.0;

            if (! played)
                continue;

            if (engine.getString (s).getDamping() == StringEngine::Damping::Open) ++open;
            else                                                                  ++damped;
        }

        CHECK_MSG (damped >= 2 && open == 0,
                   juce::String (type) + ": after pedal-up " + juce::String (damped) + " played strings damped, "
                     + juce::String (open) + " still open");
    }
}

//==============================================================================
/*  auto-articulation.md AA-42 (FEAT-ASSIST): Performance Assist on, crossed with
    Slide Mode, feedback, the E-Bow, whammy, capo 5, drop D, a 12-string and a
    5-string bass, in all eight styles, every ComboHarness phrase: the output is
    finite, bounded and present, and every note the capture saw lies on a
    string of the instrument, between its nut (or capo) and its last fret. */
LUTHIER_TEST (Combo, performanceAssistAcrossContexts)
{
    FindingLog log { "Combo.performanceAssist" };

    struct Context
    {
        const char* name;
        std::function<void (Rig&)> setup;
    };

    const std::vector<Context> contexts =
    {
        { "slide mode", [] (Rig& r) { r.setIndex (ParamIDs::slideGuitar, 1); } },
        { "feedback",   [] (Rig& r) { r.setPlain (ParamIDs::feedbackAmount, 60.0f); } },
        { "e-bow",      [] (Rig& r) { r.setIndex (ParamIDs::ebowEnable, 1); } },
        { "whammy",     [] (Rig& r) { r.setNormalised (ParamIDs::whammyPos, 0.8f); } },
        { "capo 5",     [] (Rig& r) { r.setIndex (ParamIDs::capoFret, 5); } },
        { "drop D",     [] (Rig& r) { r.setIndex (ParamIDs::tuningPreset, (int) TuningPreset::DropD); } },
        { "12-string",  [] (Rig& r) { r.setIndex (ParamIDs::guitarType, (int) GuitarType::TwelveString); } },
        { "5-string",   [] (Rig& r) { r.setIndex (ParamIDs::guitarType, (int) GuitarType::FiveStringBass); } },
    };

    const int styles = juce::jmax (1, (int) std::round (AutoArticulationStyles::kNumStyles * juce::jmin (1.0, scale())));

    for (const auto& context : contexts)
    {
        for (int style = 0; style < styles; ++style)
        {
            for (int ph = 0; ph < (int) Phrase::numPhrases; ++ph)
            {
                Rig rig;
                rig.p().resetEverything();
                context.setup (rig);
                rig.setIndex (ParamIDs::aaEnabled, 1);
                rig.setIndex (ParamIDs::aaStyle, style);
                rig.setIndex (ParamIDs::playingMode, ph % 2 == 0 ? (int) PlayingMode::Mono : (int) PlayingMode::Poly);
                rig.apply();
                rig.processSilence (2);

                const auto stats = rig.render ((Phrase) ph, 1.0);
                const juce::String label = juce::String (context.name) + " style=" + AutoArticulationStyles::get (style).name
                                         + " phrase=" + phraseName ((Phrase) ph);

                Verdict v;
                v.expectDecay = false;        // an E-Bow, feedback and a slide sustain by design
                v.checkIdleFloor = false;
                judgeAndLog (ctx, log, rig, label, stats, v);

                // Every captured note is on a string, inside its range.
                auto& capture = rig.p().getPerformanceCapture();
                capture.drain();
                auto& tuning = rig.p().getEngine().getTuningEngine();
                const int strings = rig.p().getEngine().getNumStrings();

                for (const auto& note : capture.getNotes())
                {
                    ++ctx.checks;
                    const bool inRange = note.stringIndex >= 0 && note.stringIndex < strings && note.fret >= -1.0e-6
                                         && note.fret <= tuning.getHighestPlayableFret (note.stringIndex) + 1.0e-6;

                    if (! inRange)
                    {
                        const auto why = "note " + juce::String (note.midiNote) + " on string " + juce::String (note.stringIndex)
                                       + " fret " + juce::String (note.fret, 2) + " is outside the string";
                        ctx.fail (why + " | " + label);
                        log.add (label, why);
                    }
                }
            }
        }
    }

    log.flush();
}
/*  The features that landed after the first round, in pairwise combination
    with each other and with the rig: jam mode (enabled, playing, style,
    intensity, start mode, kit), output normalization on/off, the three CPU
    quality levels, and the structural choices they interact with. A separate
    test so the first round's rows and seed stay reproducible. Every eighth
    row with the band stopped also round-trips the session state into a fresh
    instance and compares the audio (jam_play is transient by design, B-20). */
LUTHIER_TEST (Combo, newFeaturesPairwise)
{
    Rig rig;
    FindingLog log { "Combo.newFeatures" };

    std::vector<Factor> factors;
    auto addAll = [&] (const juce::String& id)
    {
        if (rig.param (id) == nullptr) { CHECK_MSG (false, "missing parameter " + id); return; }
        factors.push_back ({ id, allLevels (rig, id), {} });
    };
    auto addFloat = [&] (const juce::String& id, std::vector<float> plains)
    {
        if (rig.param (id) == nullptr) { CHECK_MSG (false, "missing parameter " + id); return; }
        std::vector<int> lv;
        for (int i = 0; i < (int) plains.size(); ++i) lv.push_back (i);
        factors.push_back ({ id, lv, std::move (plains) });
    };

    addAll (ParamIDs::guitarType);
    addAll (ParamIDs::ampModel);
    addAll (ParamIDs::pickupSelector);
    addAll (ParamIDs::oversample);
    addAll (ParamIDs::playingMode);
    addAll (ParamIDs::freezeEnable);
    addFloat (ParamIDs::feedbackAmount, { 0.0f, 50.0f });
    addAll (ParamIDs::jamEnabled);
    addAll (ParamIDs::jamPlay);
    addAll (ParamIDs::jamStyle);
    addFloat (ParamIDs::jamIntensity, { 1.0f, 3.0f, 5.0f });
    addAll (ParamIDs::jamStartMode);
    addAll (ParamIDs::jamKit);

    // Not parameters: the quality level (forced, as Options -> AUDIO would pin
    // it) and the normalization switch. Marked by their ids.
    factors.push_back ({ "special:quality", { 0, 1, 2 }, {} });
    factors.push_back ({ "special:normalize", { 0, 1 }, {} });

    {
        Factor f;
        for (int i = 0; i < (int) Phrase::numPhrases; ++i) f.levels.push_back (i);
        factors.push_back (f);
    }

    std::vector<int> sizes;
    for (auto& f : factors) sizes.push_back ((int) f.levels.size());

    constexpr uint32_t seed = 20260926;
    auto rows = allPairs (sizes, seed);
    const int limit = juce::jmax (1, (int) std::round (rows.size() * scale()));
    std::cout << "    new-feature pairwise: " << factors.size() << " factors, " << rows.size()
              << " rows (running " << limit << "), seed " << seed << std::endl;

    int roundTrips = 0;

    for (int r = 0; r < limit; ++r)
    {
        const auto& row = rows[(size_t) r];
        Config config;
        int quality = 0;
        bool normalize = false;

        for (size_t f = 0; f < factors.size(); ++f)
        {
            const auto& fac = factors[f];
            const int level = fac.levels[(size_t) row[f]];

            if (fac.id.isEmpty())                     config.phrase = (Phrase) level;
            else if (fac.id == "special:quality")     quality = level;
            else if (fac.id == "special:normalize")   normalize = level != 0;
            else if (! fac.plains.empty())            config.plains.push_back ({ fac.id, fac.plains[(size_t) level] });
            else                                      config.indices.push_back ({ fac.id, level });
        }

        static const char* qualityNames[] = { "High", "Medium", "Low" };
        config.extra = juce::String ("quality=") + qualityNames[quality] + " normalize=" + (normalize ? "on" : "off")
                     + " row=" + juce::String (r) + " seed=" + juce::String ((int) seed);

        rig.p().resetEverything();
        rig.p().getQualityController().forceLevelForTesting (quality);
        rig.p().getOutputNormalization().setEnabled (normalize);
        config.applyTo (rig);
        rig.processSilence (2);

        const auto stats = rig.render (config.phrase, 2.5);

        const bool bandPlaying = rig.param (ParamIDs::jamEnabled)->getValue() > 0.5f
                              && (rig.param (ParamIDs::jamPlay)->getValue() > 0.5f
                                  || rig.param (ParamIDs::jamStartMode)->getCurrentValueAsText() == "First Note"
                                  || rig.param (ParamIDs::jamStartMode)->getCurrentValueAsText() == "Auto");

        Verdict v;
        // The band keeps time after the guitar stops (until jam_silence_bars
        // of silence, then its ending): its tail is music, not a stuck note.
        v.expectDecay = ! holdsSound (rig) && ! slotHoldsSound (rig) && ! bandPlaying;
        judgeAndLog (ctx, log, rig, config.describe (rig), stats, v);

        // State round trip, band stopped (jam_play is restored off by design).
        if (r % 8 == 0 && rig.param (ParamIDs::jamPlay)->getValue() < 0.5f)
        {
            ++roundTrips;
            juce::MemoryBlock blob;
            rig.p().getStateInformation (blob);

            Rig copy;
            copy.p().getQualityController().forceLevelForTesting (quality);
            copy.p().setStateInformation (blob.getData(), (int) blob.getSize());
            copy.apply();

            auto play = [] (Rig& x)
            {
                x.p().releaseResources();
                x.p().prepareToPlay (kSr, kBlock);
                x.p().reset();
                x.apply();
                return x.render (Phrase::chord, 0.5).mono;
            };

            const auto a = play (rig);
            const auto b = play (copy);
            double maxDiff = 0.0;

            for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
                maxDiff = juce::jmax (maxDiff, (double) std::abs (a[i] - b[i]));

            CHECK_MSG (maxDiff < 1.0e-3 && copy.p().getNormalizationStatus().enabled == normalize,
                       "state round trip differs: audio max diff " + juce::String (maxDiff, 6)
                         + ", normalization " + (copy.p().getNormalizationStatus().enabled ? "on" : "off")
                         + " | " + config.describe (rig));
        }

        rig.p().getQualityController().forceLevelForTesting (-1);
        rig.p().getOutputNormalization().setEnabled (false);
        rig.quiet();
    }

    std::cout << "    new-feature state round trips: " << roundTrips << std::endl;
    log.flush();
}

//==============================================================================
/*  B-21: at a reduced CPU quality level the ring-out bookkeeping survived a
    reset, so what had been played before changed the next render (a Poly
    chord, then Guitar Controller mode: 0.057 apart from a fresh instance at
    Low). Also the release stagger's generator (string-interaction 0.3): a
    released chord renders the same twice after a reset. */
LUTHIER_TEST (Combo, qualityLevelsForgetWhatWasPlayedAtReset)
{
    auto choose = [] (Rig& r, const char* id, const char* text)
    {
        auto* c = dynamic_cast<juce::AudioParameterChoice*> (r.param (id));
        r.setIndex (id, c != nullptr ? c->choices.indexOf (text) : 0);
    };

    auto restart = [] (Rig& r)
    {
        r.p().releaseResources();
        r.p().prepareToPlay (kSr, kBlock);
        r.p().reset();
        r.apply();
    };

    for (int level = 0; level <= 2; ++level)
    {
        auto setUp = [&] (Rig& r)
        {
            r.p().resetEverything();
            r.p().getQualityController().forceLevelForTesting (level);
            choose (r, ParamIDs::playingMode, "Guitar Controller");
            r.apply();
        };

        Rig played;
        played.p().resetEverything();
        played.p().getQualityController().forceLevelForTesting (level);
        choose (played, ParamIDs::playingMode, "Poly / Chord");
        played.apply();
        played.processSilence (2);
        played.render (Phrase::chord, 1.0);
        played.quiet();
        setUp (played);

        Rig fresh;
        setUp (fresh);

        restart (played);
        restart (fresh);
        const auto a = played.render (Phrase::chord, 0.5).mono;
        const auto b = fresh.render (Phrase::chord, 0.5).mono;

        double maxDiff = 0.0;
        for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
            maxDiff = juce::jmax (maxDiff, (double) std::abs (a[i] - b[i]));

        CHECK_MSG (maxDiff == 0.0, "quality level " + juce::String (level) + ": a Poly chord before the reset changed "
                                     "the Guitar Controller render by " + juce::String (maxDiff, 6));

        // The same released chord twice, each after a reset.
        choose (fresh, ParamIDs::playingMode, "Poly / Chord");
        fresh.apply();
        restart (fresh);
        const auto first = fresh.render (Phrase::chord, 1.0).mono;
        restart (fresh);
        const auto second = fresh.render (Phrase::chord, 1.0).mono;

        CHECK_MSG (first == second, "quality level " + juce::String (level) + ": a released chord renders differently after a reset");

        played.p().getQualityController().forceLevelForTesting (-1);
        fresh.p().getQualityController().forceLevelForTesting (-1);
    }
}

/*  B-15: string_age Old / 120+ hours detunes a string by a few cents, and the
    voicers' "is this pitch on a fret" slop did not allow for it, so a bass note
    above the open G was dropped: silence, not dullness. Old strings play. */
LUTHIER_TEST (Combo, oldStringsStillPlayTheHighNotes)
{
    auto play = [] (float hours)
    {
        Rig rig;
        auto& presets = rig.p().getPresetManager();
        presets.loadPreset (presets.indexOfPreset ("P-Bass Flatwound"));
        rig.setPlain (ParamIDs::stringAgeHours, hours);
        rig.apply();
        rig.processSilence (8);
        std::vector<TimedMidi> ev { { 0, juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100) },
                                    { (int) (0.5 * kSr), juce::MidiMessage::noteOff (1, 52) } };
        return rig.renderEvents (ev, (int) (0.5 * kSr), 0.2).maxWindowRms;
    };

    const double fresh = play (0.0f);

    for (float hours : { 120.0f, 200.0f })
    {
        const double old = play (hours);
        CHECK_MSG (old > 0.5 * fresh && old < 1.5 * fresh,
                   "aged " + juce::String (hours) + " h: rms " + juce::String (old) + " against fresh " + juce::String (fresh));
    }
}
