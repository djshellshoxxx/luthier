#include "RiffTransposer.h"

#include "../Practice/Trainers.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

bool GuitarSpecSummary::operator== (const GuitarSpecSummary& o) const noexcept
{
    return numStrings == o.numStrings && tuning == o.tuning && capo == o.capo
        && maxFret == o.maxFret && isBass == o.isBass && hasWhammy == o.hasWhammy;
}

GuitarSpecSummary GuitarSpecSummary::forRiff (const Riff& riff)
{
    GuitarSpecSummary g;
    g.numStrings = juce::jlimit (1, kMaxStrings, riff.getNumStrings());
    g.tuning.fill (0);

    for (int s = 0; s < g.numStrings; ++s)
        g.tuning[(size_t) s] = riff.tuning[(size_t) s];

    g.capo = riff.capo;
    g.maxFret = 24;
    g.isBass = riff.isBass();
    return g;
}

bool RiffPlaySettings::operator== (const RiffPlaySettings& o) const noexcept
{
    return targetRoot == o.targetRoot && targetScale == o.targetScale && mapScale == o.mapScale
        && juce::exactlyEqual (nominalBpm, o.nominalBpm) && juce::exactlyEqual (levelDb, o.levelDb);
}

namespace RiffTransposer
{
    namespace
    {
        bool scaleTypeOf (const juce::String& name, ScaleType& type)
        {
            const auto index = RiffVocabulary::scales().indexOf (name);

            if (index < 0 || name == "chromatic")
                return false;

            type = (ScaleType) index;   // the list is ScaleType's order
            return true;
        }

        int intervalsOf (const juce::String& name, int* dest)
        {
            ScaleType type;
            return scaleTypeOf (name, type) ? getScaleIntervals (type, dest, 12) : 0;
        }

        bool isPentatonic (const juce::String& s)
        {
            return s == "major_pentatonic" || s == "minor_pentatonic";
        }

        bool linksBack (const ScoreNote& n)
        {
            using T = ScoreTechnique::Type;
            return n.hasTechnique (T::hammerOn) || n.hasTechnique (T::pullOff);
        }

        bool linksForward (const ScoreNote& n)
        {
            using T = ScoreTechnique::Type;
            return n.hasTechnique (T::slideLegato) || n.hasTechnique (T::slideShift) || n.hasTechnique (T::trill);
        }

        bool isDead (const ScoreNote& n) { return n.hasTechnique (ScoreTechnique::Type::deadNote); }
    }

    int keyShift (int fromRoot, int toRoot) noexcept
    {
        int d = (((toRoot - fromRoot) % 12) + 12) % 12;   // 0..11

        if (d > 6)
            d -= 12;

        if (d == 6)
            d = -6;   // a tritone: down (the smaller shift is either; down keeps bends playable)

        return d;
    }

    bool canMapScales (const juce::String& from, const juce::String& to)
    {
        if (from == to || from == "chromatic" || to == "chromatic")
            return false;

        int a[12], b[12];
        const int na = intervalsOf (from, a), nb = intervalsOf (to, b);

        if (na == 0 || nb == 0)
            return false;

        return (na == 7 && nb == 7) || (isPentatonic (from) && isPentatonic (to));
    }

    int mapPitch (int pitch, int rootPitchClass, int d, const juce::String& fromScale,
                  const juce::String& toScale, bool mapScale)
    {
        if (! mapScale || ! canMapScales (fromScale, toScale))
            return pitch + d;

        int from[12], to[12];
        const int n = intervalsOf (fromScale, from);
        intervalsOf (toScale, to);

        const int rel = (((pitch - rootPitchClass) % 12) + 12) % 12;

        int degree = 0;

        for (int k = 0; k < n; ++k)
            if (from[k] <= rel)
                degree = k;

        const int offset = rel - from[degree];
        const int newRel = to[degree] + offset;

        return pitch - rel + newRel + d;
    }

    RiffPlacement place (const Riff& riff, const RiffPlaySettings& settings, const GuitarSpecSummary& guitar)
    {
        RiffPlacement result;

        const int riffRoot = juce::jmax (0, riff.getRootPitchClass());
        const int targetRoot = settings.targetRoot >= 0 ? settings.targetRoot % 12 : riffRoot;
        const juce::String targetScale = settings.targetScale.isNotEmpty() ? settings.targetScale : riff.scale;

        result.targetRoot = targetRoot;
        result.targetScale = settings.mapScale && canMapScales (riff.scale, targetScale) ? targetScale : riff.scale;

        const int d = keyShift (riffRoot, targetRoot);
        result.keyShift = d;

        const bool riffBass = riff.isBass();
        result.familyOctave = (riffBass && ! guitar.isBass) ? 12 : (! riffBass && guitar.isBass) ? -12 : 0;
        const int stringOffset = (riffBass && ! guitar.isBass) ? 2 : (! riffBass && guitar.isBass) ? -2 : 0;

        if (result.familyOctave > 0)
            result.notices.add ("Bass line played an octave up on this guitar");
        else if (result.familyOctave < 0)
            result.notices.add ("Guitar part played an octave down on this bass");

        const int numStrings = juce::jlimit (1, kMaxStrings, guitar.numStrings);
        const int maxFret = juce::jlimit (1, Riff::kMaxFret, guitar.maxFret);

        // The pitch each note sounds after the key move (octave candidates aside).
        std::vector<int> target;
        target.reserve (riff.notes.size());

        for (const auto& n : riff.notes)
            target.push_back (mapPitch (riff.pitchOf (n.stringIndex, n.fret), riffRoot, d, riff.scale,
                                        targetScale, settings.mapScale) + result.familyOctave);

        auto fretFor = [&guitar] (int pitch, int s) { return pitch - guitar.openPitch (s); };

        auto fits = [&] (size_t i, int octave, int s, int& fret)
        {
            if (! juce::isPositiveAndBelow (s, numStrings))
                return false;

            fret = fretFor (target[i] + octave, s);

            if (isDead (riff.notes[i]))
            {
                fret = juce::jlimit (0, maxFret, fret);
                return true;
            }

            return fret >= 0 && fret <= maxFret;
        };

        // ---- 1. the whole riff along its own strings -----------------------------
        int chosenOctave = 0;
        bool allFit = false;

        // {d, d-12, d+12}, smallest |shift| first.
        const int octaves[3] = { 0, d > 0 ? -12 : 12, d > 0 ? 12 : -12 };

        for (const int octave : octaves)
        {
            bool ok = true;
            int fret = 0;

            for (size_t i = 0; i < riff.notes.size() && ok; ++i)
                ok = fits (i, octave, riff.notes[i].stringIndex + stringOffset, fret);

            if (ok)
            {
                chosenOctave = octave;
                allFit = true;
                break;
            }
        }

        result.shift = d + chosenOctave + result.familyOctave;

        std::vector<int> placedString (riff.notes.size(), -1), placedFret (riff.notes.size(), 0);

        for (size_t i = 0; i < riff.notes.size(); ++i)
        {
            int fret = 0;
            const int s = riff.notes[i].stringIndex + stringOffset;

            if (fits (i, chosenOctave, s, fret))
            {
                placedString[i] = s;
                placedFret[i] = fret;
            }
        }

        // ---- 2. legato chains to a neighbouring string ---------------------------
        if (! allFit)
        {
            // Chains: runs of notes on one string joined by hammer, pull, legato
            // slide or trill, by the notes' order in time.
            std::vector<int> chainOf (riff.notes.size(), -1);
            std::vector<std::vector<size_t>> chains;
            std::vector<size_t> order (riff.notes.size());

            for (size_t i = 0; i < order.size(); ++i)
                order[i] = i;

            std::stable_sort (order.begin(), order.end(), [&riff] (size_t a, size_t b)
            {
                return riff.notes[a].startBeat < riff.notes[b].startBeat;
            });

            std::vector<int> lastOnString ((size_t) kMaxStrings + 4, -1);

            for (auto i : order)
            {
                const auto& n = riff.notes[i];
                const int s = juce::jlimit (0, (int) lastOnString.size() - 1, n.stringIndex);
                const int prev = lastOnString[(size_t) s];

                const bool joined = prev >= 0
                    && std::abs (riff.notes[(size_t) prev].startBeat + riff.notes[(size_t) prev].durationBeats
                                   - n.startBeat) < 1.0e-6
                    && (linksBack (n) || linksForward (riff.notes[(size_t) prev]));

                if (joined)
                {
                    chainOf[i] = chainOf[(size_t) prev];
                    chains[(size_t) chainOf[i]].push_back (i);
                }
                else
                {
                    chainOf[i] = (int) chains.size();
                    chains.push_back ({ i });
                }

                lastOnString[(size_t) s] = (int) i;
            }

            auto overlaps = [&] (size_t i, int s)
            {
                const auto& n = riff.notes[i];

                for (size_t j = 0; j < riff.notes.size(); ++j)
                {
                    if (placedString[j] != s || chainOf[j] == chainOf[i])
                        continue;

                    const auto& o = riff.notes[j];

                    if (o.startBeat < n.startBeat + n.durationBeats - 1.0e-9
                          && n.startBeat < o.startBeat + o.durationBeats - 1.0e-9)
                        return true;
                }

                return false;
            };

            for (const auto& chain : chains)
            {
                bool whole = true;

                for (auto i : chain)
                    whole = whole && placedString[i] >= 0;

                if (whole)
                    continue;

                const int home = riff.notes[chain.front()].stringIndex + stringOffset;
                int bestString = -1, bestFret = 1 << 20, bestDistance = 1 << 20;

                for (int s = 0; s < numStrings; ++s)
                {
                    int maxInChain = -1;
                    bool ok = true;

                    for (auto i : chain)
                    {
                        int fret = 0;

                        if (! fits (i, chosenOctave, s, fret) || overlaps (i, s))
                        {
                            ok = false;
                            break;
                        }

                        maxInChain = juce::jmax (maxInChain, fret);
                    }

                    if (! ok)
                        continue;

                    const int distance = std::abs (s - home);

                    if (distance < bestDistance || (distance == bestDistance && maxInChain < bestFret))
                    {
                        bestString = s;
                        bestFret = maxInChain;
                        bestDistance = distance;
                    }
                }

                for (auto i : chain)
                {
                    if (bestString >= 0)
                    {
                        int fret = 0;
                        fits (i, chosenOctave, bestString, fret);
                        placedString[i] = bestString;
                        placedFret[i] = fret;
                        result.movedChains = true;
                    }
                    else
                    {
                        placedString[i] = -1;   // 3. dropped
                    }
                }
            }
        }

        // ---- out -----------------------------------------------------------------
        for (size_t i = 0; i < riff.notes.size(); ++i)
        {
            if (placedString[i] < 0)
            {
                ++result.dropped;
                continue;
            }

            auto n = riff.notes[i];
            const int s = placedString[i];

            if (guitar.openPitch (s) != riff.pitchOf (n.stringIndex, 0) + result.familyOctave)
                result.retuned = true;

            const int fretDelta = placedFret[i] - n.fret;
            n.stringIndex = s;
            n.fret = placedFret[i];
            n.midiNote = juce::jlimit (0, 127, isDead (riff.notes[i]) ? guitar.openPitch (s) + n.fret
                                                                      : target[i] + chosenOctave);
            n.pitchHz = 440.0 * std::pow (2.0, (n.midiNote - 69) / 12.0);

            // Techniques that name a fret move with the note.
            for (auto& t : n.techniques)
            {
                using T = ScoreTechnique::Type;

                switch (t.type)
                {
                    case T::slideLegato: case T::slideShift: case T::slideIn: case T::slideOut:
                    case T::slideUp: case T::slideDown: case T::trill: case T::naturalHarmonic:
                        t.value = juce::jlimit (0.0, (double) maxFret, t.value + fretDelta);
                        break;
                    default:
                        break;
                }
            }

            result.notes.push_back (std::move (n));
            result.sourceIndex.push_back ((int) i);
        }

        // Sorted by start, then string, with the source index kept in step.
        std::vector<size_t> idx (result.notes.size());

        for (size_t i = 0; i < idx.size(); ++i)
            idx[i] = i;

        std::stable_sort (idx.begin(), idx.end(), [&result] (size_t a, size_t b)
        {
            const auto& x = result.notes[a];
            const auto& y = result.notes[b];
            return x.startBeat != y.startBeat ? x.startBeat < y.startBeat : x.stringIndex < y.stringIndex;
        });

        std::vector<ScoreNote> sortedNotes;
        std::vector<int> sortedSource;

        for (auto i : idx)
        {
            sortedNotes.push_back (std::move (result.notes[i]));
            sortedSource.push_back (result.sourceIndex[i]);
        }

        result.notes = std::move (sortedNotes);
        result.sourceIndex = std::move (sortedSource);

        if (result.retuned && result.familyOctave == 0)
            result.notices.add ("Re-fretted for this guitar's tuning");

        if (result.movedChains)
            result.notices.add ("Some phrases moved to a neighbouring string");

        if (result.dropped > 0)
            result.notices.add (juce::String (result.dropped) + (result.dropped == 1 ? " note" : " notes")
                                + " did not fit this guitar");

        return result;
    }
}

} // namespace luthier
