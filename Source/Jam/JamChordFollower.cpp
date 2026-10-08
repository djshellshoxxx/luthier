#include "JamChordFollower.h"
#include "../Tune/TuneMidi.h"

namespace luthier
{

//==============================================================================
int JamChordMap::findIndexAt (double ppq) const noexcept
{
    if (count <= 0)
        return -1;

    if (loop && lengthPpq > 0.0)
    {
        ppq = std::fmod (ppq, lengthPpq);

        if (ppq < 0.0)
            ppq += lengthPpq;
    }

    // The last entry at or before ppq.
    int lo = 0, hi = count - 1, found = -1;

    while (lo <= hi)
    {
        const int mid = (lo + hi) / 2;

        if (entries[(size_t) mid].ppq <= ppq + 1.0e-9)
        {
            found = mid;
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }

    // Before the first change, a looping tune is still in its last chord.
    if (found < 0 && loop)
        found = count - 1;

    return found;
}

ChordSymbol JamChordMap::chordAt (double ppq) const noexcept
{
    const int i = findIndexAt (ppq);
    return i >= 0 ? entries[(size_t) i].toSymbol() : ChordSymbol {};
}

bool JamChordMap::nextChange (double afterPpq, double beforePpq, Entry& out, double& atPpq) const noexcept
{
    if (count <= 0 || ! (beforePpq > afterPpq))
        return false;

    const double length = (loop && lengthPpq > 0.0) ? lengthPpq : 0.0;
    const double passStart = length > 0.0 ? std::floor (afterPpq / length) * length : 0.0;

    for (int pass = 0; pass < 2; ++pass)
    {
        const double offset = passStart + pass * length;

        for (int i = 0; i < count; ++i)
        {
            const double p = entries[(size_t) i].ppq + offset;

            if (p > afterPpq + 1.0e-9 && p < beforePpq - 1.0e-9)
            {
                out = entries[(size_t) i];
                atPpq = p;
                return true;
            }

            if (p >= beforePpq)
                return false;
        }

        if (length <= 0.0)
            break;
    }

    return false;
}

std::unique_ptr<JamChordMap> JamChordMap::fromTimeline (const TuneTimeline& timeline, bool shouldLoop,
                                                        const std::vector<JamSectionHint>& hints)
{
    auto map = std::make_unique<JamChordMap>();
    map->lengthPpq = timeline.getLengthPpq();
    map->loop = shouldLoop;

    for (size_t i = 0; i < hints.size() && i < (size_t) kMaxSections; ++i)
        map->hints[i] = hints[i];

    const auto& sections = timeline.getSections();
    auto sectionAt = [&sections] (double ppq, bool& isStart)
    {
        isStart = false;

        for (const auto& s : sections)
            if (ppq >= s.ppq - 1.0e-9 && ppq < s.ppq + s.lengthPpq - 1.0e-9)
            {
                isStart = std::abs (ppq - s.ppq) < 1.0e-9;
                return s.sectionIndex;
            }

        return 0;
    };

    ChordDetector detector;
    detector.prepare (48000.0);

    const auto& events = timeline.getEvents();
    std::array<int, 24> held {};
    int numHeld = 0;
    ChordSymbol previous;
    int previousSection = -1;

    for (size_t i = 0; i < events.size();)
    {
        const double ppq = events[i].ppq;

        // Every event at this position: note-offs first (the timeline sorts
        // them first), then the note-ons of the new cell.
        for (; i < events.size() && std::abs (events[i].ppq - ppq) < 1.0e-9; ++i)
        {
            const auto& e = events[i];

            if (e.part != TunePart::chords)
                continue;

            const auto& m = e.message;

            if (m.isNoteOn())
            {
                if (numHeld < (int) held.size())
                    held[(size_t) numHeld++] = m.getNoteNumber();
            }
            else if (m.isNoteOff())
            {
                for (int h = 0; h < numHeld; ++h)
                    if (held[(size_t) h] == m.getNoteNumber())
                    {
                        held[(size_t) h] = held[(size_t) --numHeld];
                        break;
                    }
            }
        }

        const auto symbol = detector.detect (held.data(), numHeld);

        if (! JamChordFollower::isChord (symbol))
            continue;

        bool sectionStart = false;
        const int section = sectionAt (ppq, sectionStart);

        if (symbol == previous && section == previousSection)
            continue;

        if (map->count >= kMaxEntries)
        {
            // 13: truncated here and logged by the caller; anticipation stops.
            map->truncated = true;
            break;
        }

        auto& entry = map->entries[(size_t) map->count++];
        entry.ppq = ppq;
        entry.root = symbol.root;
        entry.bass = symbol.bass;
        entry.templateIndex = symbol.templateIndex;
        entry.sectionIndex = section;
        entry.sectionStart = sectionStart && section != previousSection;

        previous = symbol;
        previousSection = section;
    }

    return map;
}

//==============================================================================
void JamChordFollower::prepare (double sampleRate) noexcept
{
    detector.prepare (sampleRate);
    windowSamples = juce::jmax ((int64_t) 1, (int64_t) std::llround (ChordDetector::kBurstWindowSeconds * sampleRate));
    reset();
}

void JamChordFollower::reset() noexcept
{
    numHeld = 0;
    burstOpen = false;
    burstStart = 0;
}

void JamChordFollower::noteOn (int note, int64_t sample) noexcept
{
    bool already = false;

    for (int i = 0; i < numHeld; ++i)
        already = already || held[(size_t) i] == note;

    if (! already && numHeld < (int) held.size())
        held[(size_t) numHeld++] = note;

    if (! burstOpen)
    {
        burstOpen = true;
        burstStart = sample;
    }
}

void JamChordFollower::noteOff (int note) noexcept
{
    for (int i = 0; i < numHeld; ++i)
        if (held[(size_t) i] == note)
        {
            held[(size_t) i] = held[(size_t) --numHeld];
            return;
        }
}

void JamChordFollower::allNotesOff() noexcept
{
    numHeld = 0;
    burstOpen = false;
}

bool JamChordFollower::isChord (const ChordSymbol& s) noexcept
{
    // detect() already returns Unknown below two pitch classes or below the
    // confidence floor; this is the same rule restated for the follower.
    return s.isKnown() && s.confidence >= ChordDetector::kConfidenceFloor;
}

bool JamChordFollower::poll (int64_t upTo, Change& out) noexcept
{
    if (! burstOpen || burstStart + windowSamples > upTo)
        return false;

    burstOpen = false;
    const auto symbol = detector.detect (held.data(), numHeld);

    if (! isChord (symbol))
        return false;

    out.chord = symbol;
    out.firstNoteSample = burstStart;
    out.detectionSample = burstStart + windowSamples;
    return true;
}

//==============================================================================
void JamPredictor::reset() noexcept
{
    start = count = 0;
    historyStart = 0.0;
    cycleBars = 0;
    cycleLength = 0.0;
}

void JamPredictor::record (double ppq, const ChordSymbol& chord) noexcept
{
    if (count > 0 && at (count - 1).chord == chord)
        return;

    if (count == 0)
        historyStart = ppq;

    if (count == kHistory)
    {
        start = (start + 1) % kHistory;
        --count;
        historyStart = at (0).ppq;
    }

    auto& e = ring[(size_t) ((start + count) % kHistory)];
    e.ppq = ppq;
    e.chord = chord;
    ++count;
}

void JamPredictor::contradict (double ppq, const ChordSymbol& chord) noexcept
{
    reset();
    record (ppq, chord);
}

bool JamPredictor::chordAt (double ppq, ChordSymbol& out) const noexcept
{
    bool found = false;

    for (int i = 0; i < count; ++i)
    {
        if (at (i).ppq <= ppq + 1.0e-6)
        {
            out = at (i).chord;
            found = true;
        }
    }

    return found;
}

void JamPredictor::onBarLine (double barStart, double barLength) noexcept
{
    if (count == 0 || barLength <= 0.0)
    {
        cycleBars = 0;
        return;
    }

    for (int n : { 1, 2, 4, 8, 16 })
    {
        const double cycle = n * barLength;
        const double w1 = barStart - 2.0 * cycle, w2 = barStart - cycle;

        if (historyStart > w1 + 1.0e-6)
            break;   // not heard twice yet; longer cycles need even more

        ChordSymbol c1, c2;

        if (! chordAt (w1, c1) || ! chordAt (w2, c2) || c1 != c2)
            continue;

        // The changes inside each window, compared one to one.
        int n1 = 0, n2 = 0;
        bool same = true;

        for (int i = 0; i < count && same; ++i)
        {
            const auto& e = at (i);

            if (e.ppq > w1 + 1.0e-6 && e.ppq < w2 - 1.0e-6)
            {
                // Its partner one cycle on.
                bool matched = false;

                for (int j = 0; j < count; ++j)
                    if (std::abs (at (j).ppq - (e.ppq + cycle)) < 1.0e-6 && at (j).chord == e.chord)
                        matched = true;

                same = matched;
                ++n1;
            }
            else if (e.ppq > w2 + 1.0e-6 && e.ppq < barStart - 1.0e-6)
            {
                ++n2;
            }
        }

        if (same && n1 == n2 && n2 > 0)
        {
            cycleBars = n;
            cycleLength = cycle;
            return;
        }

        if (same && n1 == n2 && n1 == 0 && c1 != ChordSymbol {})
        {
            // One chord per cycle, changing each cycle is caught above; a held
            // chord predicts nothing, which is harmless.
            continue;
        }
    }

    cycleBars = 0;
}

bool JamPredictor::predictNext (double afterPpq, double beforePpq, double& atPpq, ChordSymbol& chord) const noexcept
{
    if (cycleBars <= 0)
        return false;

    // Changes one cycle back, moved on by a cycle.
    for (int k = 1; k <= 2; ++k)
    {
        const double shift = k * cycleLength;

        for (int i = 0; i < count; ++i)
        {
            const double p = at (i).ppq + shift;

            if (p > afterPpq + 1.0e-6 && p < beforePpq - 1.0e-6)
            {
                // The chord that would be heard there must differ from the one
                // before it, or it is not a change.
                atPpq = p;
                chord = at (i).chord;
                return true;
            }
        }
    }

    return false;
}

} // namespace luthier
