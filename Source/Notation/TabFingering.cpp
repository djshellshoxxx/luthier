#include "TabFingering.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

namespace
{
    struct Placed
    {
        ScoreNote* note = nullptr;
        double absoluteStart = 0.0;
    };

    double barBeats (const ScoreMeasure& m)
    {
        return (double) juce::jlimit (1, 32, m.timeSignatureNumerator) * 4.0
                 / (double) juce::jlimit (1, 32, m.timeSignatureDenominator);
    }
}

namespace TabFingering
{

bool isPlausible (const PerformanceScore& score, int trackIndex, int maxFret)
{
    const auto& track = score.getTrack (trackIndex);
    const int numStrings = juce::jlimit (1, kMaxStrings, track.numStrings);
    int distinctStrings = 0;
    std::array<bool, kMaxStrings> used {};

    for (const auto& measure : track.measures)
    {
        for (const auto& voice : measure.voices)
        {
            for (const auto& note : voice.notes)
            {
                if (! juce::isPositiveAndBelow (note.stringIndex, numStrings))
                    return false;

                const int open = track.tuning[(size_t) note.stringIndex] + track.capoFret;
                const int fret = note.midiNote - open;

                if (fret < 0 || fret > maxFret || fret != note.fret)
                    return false;

                if (! used[(size_t) note.stringIndex]) { used[(size_t) note.stringIndex] = true; ++distinctStrings; }
            }
        }
    }

    // Everything on one string is what a generic file looks like after a
    // channel-to-string read: not a fingering, just channel 1.
    return distinctStrings >= 2 || score.getTotalNoteCount() <= 1;
}

Result assign (PerformanceScore& score, int trackIndex, int maxFret)
{
    Result result;
    auto& track = score.getTrack (trackIndex);
    const int numStrings = juce::jlimit (1, kMaxStrings, track.numStrings);
    maxFret = juce::jlimit (1, 36, maxFret);

    std::array<int, kMaxStrings> open {};
    for (int s = 0; s < numStrings; ++s)
        open[(size_t) s] = track.tuning[(size_t) s] + track.capoFret;

    // Every note with its absolute start, in time order.
    std::vector<Placed> all;
    double measureStart = 0.0;

    for (auto& measure : track.measures)
    {
        for (auto& voice : measure.voices)
            for (auto& note : voice.notes)
                all.push_back ({ &note, measureStart + note.startBeat });

        measureStart += barBeats (measure);
    }

    std::stable_sort (all.begin(), all.end(), [] (const Placed& a, const Placed& b)
    {
        if (std::abs (a.absoluteStart - b.absoluteStart) > 1.0e-6)
            return a.absoluteStart < b.absoluteStart;
        return a.note->midiNote < b.note->midiNote;
    });

    double hand = -1.0;                              // the running fretted position
    std::array<double, kMaxStrings> busyUntil {};    // when each string is free again
    std::array<bool, kMaxStrings> everUsed {};

    const auto costOf = [&] (int s, int fret, double start, double chordLow) -> double
    {
        double cost = 0.0;

        if (fret == 0)
            cost += 0.5;                                    // open strings are cheap but not free
        else
        {
            if (hand >= 0.0) cost += std::abs ((double) fret - hand);
            cost += 0.15 * (double) fret;                   // prefer the low end of the neck
        }

        if (busyUntil[(size_t) s] > start + 1.0e-6)
            cost += 3.0;                                    // the string is still ringing

        if (chordLow > 0.0 && fret > 0)
        {
            const double stretch = std::abs ((double) fret - chordLow);
            if (stretch > 4.0) cost += 2.0 * (stretch - 4.0);
        }

        return cost;
    };

    const auto clampOnto = [&] (ScoreNote& note)
    {
        // Out of range: the nearest reachable pitch on the string it is closest to.
        int bestString = 0;
        int bestDistance = 1 << 20;

        for (int s = 0; s < numStrings; ++s)
        {
            const int lo = open[(size_t) s], hi = open[(size_t) s] + maxFret;
            const int distance = note.midiNote < lo ? lo - note.midiNote : note.midiNote > hi ? note.midiNote - hi : 0;
            if (distance < bestDistance) { bestDistance = distance; bestString = s; }
        }

        note.stringIndex = bestString;
        note.fret = juce::jlimit (0, maxFret, note.midiNote - open[(size_t) bestString]);
        note.midiNote = open[(size_t) bestString] + note.fret;
        note.pitchHz = 440.0 * std::pow (2.0, (note.midiNote - 69) / 12.0);
        ++result.notesClamped;
    };

    for (size_t i = 0; i < all.size();)
    {
        // The chord: everything starting with this note.
        size_t j = i + 1;
        while (j < all.size() && std::abs (all[j].absoluteStart - all[i].absoluteStart) <= 1.0e-6)
            ++j;

        std::array<bool, kMaxStrings> taken {};
        double chordLow = 0.0;
        double fretSum = 0.0;
        int fretted = 0;

        for (size_t k = i; k < j; ++k)
        {
            auto& note = *all[k].note;
            const double start = all[k].absoluteStart;

            int bestString = -1, bestFret = 0;
            double bestCost = 1.0e9;

            for (int s = 0; s < numStrings; ++s)
            {
                if (taken[(size_t) s])
                    continue;

                const int fret = note.midiNote - open[(size_t) s];
                if (fret < 0 || fret > maxFret)
                    continue;

                const double cost = costOf (s, fret, start, chordLow);
                if (cost < bestCost) { bestCost = cost; bestString = s; bestFret = fret; }
            }

            if (bestString < 0)
            {
                // Nothing free in range: the note keeps its pitch on the nearest
                // string even if that string was already spoken for.
                bool inRangeSomewhere = false;
                for (int s = 0; s < numStrings; ++s)
                {
                    const int fret = note.midiNote - open[(size_t) s];
                    if (fret >= 0 && fret <= maxFret) { inRangeSomewhere = true; bestString = s; bestFret = fret; break; }
                }
                if (! inRangeSomewhere) { clampOnto (note); bestString = note.stringIndex; bestFret = note.fret; }
            }

            note.stringIndex = bestString;
            note.fret = bestFret;
            taken[(size_t) bestString] = true;
            everUsed[(size_t) bestString] = true;
            busyUntil[(size_t) bestString] = start + juce::jmax (0.0625, note.durationBeats);
            ++result.notesFingered;

            if (bestFret > 0)
            {
                if (chordLow <= 0.0 || bestFret < chordLow) chordLow = (double) bestFret;
                fretSum += bestFret;
                ++fretted;
            }
        }

        if (fretted > 0)
        {
            const double centre = fretSum / (double) fretted;
            hand = hand < 0.0 ? centre : 0.5 * hand + 0.5 * centre;
        }

        i = j;
    }

    for (int s = 0; s < numStrings; ++s)
        if (everUsed[(size_t) s]) ++result.stringsUsed;

    return result;
}

} // namespace TabFingering

} // namespace luthier
