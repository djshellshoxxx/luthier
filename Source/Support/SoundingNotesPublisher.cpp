#include "SoundingNotesPublisher.h"
#include "../LuthierEngine.h"

namespace luthier
{

void SoundingNotesPublisher::publish (LuthierEngine& engine, std::int64_t blockStartSample, SoundingNotes& target) noexcept
{
    const auto& activity = engine.getStringActivity();

    for (int i = 0; i < activity.size(); ++i)
    {
        const auto& e = activity[i];

        if (e.isNoteOn && juce::isPositiveAndBelow (e.stringIndex, SoundingNotes::kMaxStrings))
            starts[(size_t) e.stringIndex] = blockStartSample + e.sampleOffset;
    }

    const int numStrings = juce::jmin (engine.getNumStrings(), SoundingNotes::kMaxStrings);
    const double concertA = engine.getTuningEngine().getConcertA();

    std::array<int, SoundingNotes::kMaxStrings> notes, bends;

    for (int s = 0; s < numStrings; ++s)
    {
        const int played = engine.getStringMidiNote (s);
        notes[(size_t) s] = played;
        bends[(size_t) s] = 0;

        if (played < 0)
            continue;

        const double hz = engine.getStringFrequency (s);

        if (hz > 1.0 && concertA > 1.0)
        {
            const double exact = 69.0 + 12.0 * std::log2 (hz / concertA);
            const int nearest = (int) std::lround (exact);

            // A stray reading (a string between notes) keeps the played note.
            if (std::abs (nearest - played) <= 24 && juce::isPositiveAndBelow (nearest, 128))
            {
                notes[(size_t) s] = nearest;
                bends[(size_t) s] = juce::jlimit (-50, 50, (int) std::lround ((exact - nearest) * 100.0));
            }
        }
    }

    target.publish (notes.data(), bends.data(), starts.data(), numStrings);
}

} // namespace luthier
