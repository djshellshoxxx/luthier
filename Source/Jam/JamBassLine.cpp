#include "JamBassLine.h"

namespace luthier
{

int JamBassLine::nearest (int pc, int reference, int lo, int hi) noexcept
{
    int best = -1, bestDistance = 1 << 20;

    for (int n = lo; n <= hi; ++n)
    {
        if (((n % 12) + 12) % 12 != pc)
            continue;

        const int d = std::abs (n - reference);

        if (d < bestDistance)
        {
            best = n;
            bestDistance = d;
        }
    }

    return best;
}

int JamBassLine::pitchClassFor (char token, const ChordSymbol& chord) noexcept
{
    if (! chord.isKnown())
        return -1;

    const int root = chord.root;
    const auto mask = getChordTemplate (chord.templateIndex).intervalMask;
    auto has = [mask] (int interval) { return (mask & (1u << interval)) != 0; };

    switch (token)
    {
        case 'R':
        case 'm':
            return chord.isSlash() ? chord.bass : root;

        case '8':
            return root;

        case '3':
            if (has (4)) return (root + 4) % 12;
            if (has (3)) return (root + 3) % 12;
            return root;

        case '5':
            if (has (7)) return (root + 7) % 12;
            if (has (6)) return (root + 6) % 12;
            if (has (8)) return (root + 8) % 12;
            return (root + 7) % 12;

        case '7':
            if (has (10)) return (root + 10) % 12;
            if (has (11)) return (root + 11) % 12;
            if (has (9))  return (root + 9) % 12;   // the diminished seventh
            return root;                           // the octave

        default:
            return -1;
    }
}

int JamBassLine::resolve (char token, const ChordSymbol& chord, const ChordSymbol& next, int walkStepsLeft) noexcept
{
    if (! chord.isKnown())
        return -1;

    const int reference = previous >= 0 ? previous : 36;
    int note = -1;

    auto rootNote = [&] (int pc) { return nearest (pc, reference, kLowest, kRootHighest); };

    switch (token)
    {
        case 'R':
            walkIndex = 0;
            note = rootNote (pitchClassFor ('R', chord));
            break;

        case '8':
        {
            // The octave above the root where it fits, else below.
            const int r = rootNote (chord.root);
            note = (r + 12 <= kHighest) ? r + 12 : r;
            break;
        }

        case '3':
        case '5':
        case '7':
            note = nearest (pitchClassFor (token, chord), reference, kLowest, kHighest);

            // 7 without a seventh is the octave.
            if (token == '7' && pitchClassFor ('7', chord) == chord.root)
            {
                const int r = rootNote (chord.root);
                note = (r + 12 <= kHighest) ? r + 12 : r;
            }
            break;

        case 'm':
            note = previous >= 0 ? previous : rootNote (pitchClassFor ('R', chord));
            break;

        case 'A':
        {
            if (! next.isKnown())
            {
                note = rootNote (pitchClassFor ('R', chord));
                break;
            }

            // A semitone below or a scale step above the next root, whichever
            // is nearer the note sounding now.
            const int target = rootNote (next.isSlash() ? next.bass : next.root);
            const int below = target - 1, above = target + 2;
            const bool belowFits = below >= kLowest, aboveFits = above <= kHighest;

            if (belowFits && (! aboveFits || std::abs (below - reference) <= std::abs (above - reference)))
                note = below;
            else
                note = above;
            break;
        }

        case 'W':
        {
            // Beat 2 a third, beat 3 a fifth, the last step before the target
            // a chromatic approach to the next root (or the root again).
            const auto& towards = next.isKnown() ? next : chord;
            const int target = rootNote (towards.isSlash() ? towards.bass : towards.root);

            if (walkStepsLeft <= 1)
            {
                const int below = target - 1, above = target + 1;
                note = (below >= kLowest && std::abs (below - reference) <= std::abs (above - reference)) ? below : above;
            }
            else
            {
                const char tones[] = { '3', '5', '8', '7' };
                const char t = tones[walkIndex % 4];
                const int pc = pitchClassFor (t, chord);
                note = nearest (pc, reference + (walkIndex % 2 == 0 ? 2 : -1), kLowest, kHighest);
            }

            ++walkIndex;
            break;
        }

        default:
            return -1;
    }

    if (note < 0)
        return -1;

    note = juce::jlimit (kLowest, kHighest, note);
    previous = note;
    return note;
}

} // namespace luthier
