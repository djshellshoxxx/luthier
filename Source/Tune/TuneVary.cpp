#include "TuneVary.h"
#include "TuneHarmony.h"
#include "TuneMelody.h"

namespace luthier
{

int createSectionVariation (Tune& tune, int sectionIndex, int seed)
{
    const auto* original = tune.getSection (sectionIndex);

    if (original == nullptr)
        return -1;

    auto copy = *original;
    const auto originalName = original->name;
    copy.name = originalName + " (vary)";

    // The last chord: its first common substitution, when there is one.
    if (! copy.chords.empty())
    {
        const int last = (int) copy.chords.size() - 1;
        const auto subs = suggestSubstitutions (copy.chords, last, tune.meta.keyTonic, tune.meta.mode);

        if (! subs.empty() && ! subs.front().cells.empty())
        {
            auto replacement = subs.front().cells;
            const double beats = copy.chords[(size_t) last].durationBeats;

            // The same length in all: a two-chord substitution shares the cell's beats.
            if (beats > 0.0)
                for (auto& c : replacement)
                    c.durationBeats = beats / (double) replacement.size();

            copy.chords.erase (copy.chords.end() - 1);
            copy.chords.insert (copy.chords.end(), replacement.begin(), replacement.end());
        }
    }

    copy.feel = tunetheory::canonical (juce::jlimit (0.0, 1.0, copy.feel + 0.06));
    copy.strum = tunetheory::canonical (juce::jlimit (0.0, 1.0, copy.strum - 0.06));

    const int index = tune.addSection (copy, sectionIndex + 1);

    if (index < 0)
        return -1;

    // The melody: locked notes kept, the rest regenerated with a new seed.
    if (auto* s = tune.getSection (index); s != nullptr && s->melody.has_value()
          && s->melody->getNumLockedNotes() < (int) s->melody->notes.size())
    {
        s->melody->seed = s->melody->seed + seed;
        s->melody->notes = generateAutoMelody (tune, index, s->melody->seed);
    }

    // With a setlist, the variation plays after the original's last entry.
    if (! tune.arrangement.setlist.empty())
    {
        auto setlist = tune.arrangement.setlist;
        int at = -1;

        for (int i = 0; i < (int) setlist.size(); ++i)
            if (setlist[(size_t) i].section == originalName)
                at = i;

        TuneSetlistEntry entry;
        entry.section = tune.arrangement.sections[(size_t) index].name;
        setlist.insert (at >= 0 ? setlist.begin() + at + 1 : setlist.end(), entry);
        tune.setSetlist (setlist);
    }

    return index;
}

} // namespace luthier
