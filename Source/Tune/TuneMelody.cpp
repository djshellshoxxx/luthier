#include "TuneMelody.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

using namespace tunetheory;

//==============================================================================
namespace
{
    constexpr double kEps = 1.0e-6;

    /** A section's harmony, resolved once so the generators do not re-derive
        the chord spans for every note. */
    struct SectionContext
    {
        const TuneSection* section = nullptr;
        int sectionIndex = -1;
        int tonic = 0;
        TuneMode mode = TuneMode::ionian;
        double beatsPerBar = 4.0;
        double lengthBeats = 0.0;
        std::vector<ChordSpan> spans;
        ChordCell tonicChord;

        const ChordCell& chordAt (double beat) const
        {
            const int i = findChordSpanAt (spans, beat);

            if (i < 0)
                return tonicChord;

            return section->chords[(size_t) spans[(size_t) i].cellIndex];
        }

        bool isChordStart (double beat) const
        {
            for (const auto& s : spans)
                if (std::abs (s.startBeat - beat) < kEps)
                    return true;

            return false;
        }
    };

    bool makeContext (const Tune& tune, int sectionIndex, SectionContext& ctx)
    {
        ctx.section = tune.getSection (sectionIndex);

        if (ctx.section == nullptr)
            return false;

        ctx.sectionIndex = sectionIndex;
        ctx.tonic = tune.meta.keyTonic;
        ctx.mode = tune.meta.mode;
        ctx.beatsPerBar = tune.getBeatsPerBar();
        ctx.lengthBeats = tune.getSectionLengthBeats (sectionIndex);
        ctx.spans = resolveChordSpans (*ctx.section, ctx.beatsPerBar);
        ctx.tonicChord = makeTonicChord (ctx.tonic, ctx.mode);
        return true;
    }

    /** splitmix64's finaliser over (seed, salt): neighbouring seeds give
        unrelated streams, and each generator salts its own so Auto and the
        countermelody with the same seed are not the same line. */
    uint64_t mixSeed (int seed, int salt) noexcept
    {
        uint64_t z = ((uint64_t) (uint32_t) seed << 32) ^ (uint64_t) (uint32_t) salt ^ 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z = z ^ (z >> 31);
        return z != 0 ? z : 0x9E3779B97F4A7C15ull;
    }

    uint16_t chordToneMask (const ChordCell& cell)
    {
        uint16_t mask = 0;

        for (int pc : getChordTones (cell))
            mask = (uint16_t) (mask | (uint16_t) (1u << pc));

        return mask;
    }

    /** The pitch of class `pc` nearest `target`, inside [low, high] when one is. */
    int nearestWithPitchClass (int pc, int target, int low, int high)
    {
        int best = -1;
        int bestDistance = 1000;

        for (int p = juce::jmax (0, low); p <= juce::jmin (127, high); ++p)
        {
            if (wrapPitchClass (p) != wrapPitchClass (pc))
                continue;

            const int d = std::abs (p - target);

            if (d < bestDistance)
            {
                best = p;
                bestDistance = d;
            }
        }

        if (best < 0)
        {
            best = target + wrapPitchClass (pc - target);

            if (best - target > 6)
                best -= 12;
        }

        return juce::jlimit (0, 127, best);
    }

    /** The chord tone nearest `pitch` inside [low, high]; a tie goes the way
        the line is already moving. */
    int nearestChordTone (int pitch, uint16_t chordMask, int low, int high, int preferDirection)
    {
        if (isInMask (pitch, chordMask) && pitch >= low && pitch <= high)
            return pitch;

        for (int d = 1; d < 12; ++d)
        {
            const int first  = preferDirection < 0 ? pitch - d : pitch + d;
            const int second = preferDirection < 0 ? pitch + d : pitch - d;

            if (first >= low && first <= high && isInMask (first, chordMask))
                return first;

            if (second >= low && second <= high && isInMask (second, chordMask))
                return second;
        }

        return juce::jlimit (low, high, pitch);
    }

    int foldIntoRange (int pitch, int low, int high)
    {
        while (pitch > high && pitch - 12 >= 0)
            pitch -= 12;

        while (pitch < low && pitch + 12 <= 127)
            pitch += 12;

        return juce::jlimit (low, high, pitch);
    }

    bool isOnBeat (double beat) noexcept
    {
        return std::abs (beat - std::round (beat)) < kEps;
    }

    bool isOffbeatEighth (double beat) noexcept
    {
        return std::abs ((beat - std::floor (beat)) - 0.5) < kEps;
    }

    int resolveInContext (const SectionContext& ctx, const MelodyNote& note)
    {
        return resolveMelodyPitch (note.pitch, &ctx.chordAt (note.startBeat), ctx.tonic, ctx.mode);
    }

    void sortByStart (std::vector<MelodyNote>& notes)
    {
        std::stable_sort (notes.begin(), notes.end(),
                          [] (const MelodyNote& a, const MelodyNote& b) { return a.startBeat < b.startBeat; });
    }

    bool containsIgnoreCase (const juce::String& haystack, std::initializer_list<const char*> needles)
    {
        for (auto* n : needles)
            if (haystack.containsIgnoreCase (n))
                return true;

        return false;
    }
}

//==============================================================================
MelodyProfile getMelodyProfileForKit (const juce::String& name)
{
    if (containsIgnoreCase (name, { "country", "bluegrass", "nashville" }))
        return MelodyProfile::country;

    if (containsIgnoreCase (name, { "jazz", "bossa", "samba", "gypsy", "latin" }))
        return MelodyProfile::jazz;

    if (containsIgnoreCase (name, { "blues", "delta", "chicago" }))
        return MelodyProfile::blues;

    if (containsIgnoreCase (name, { "folk", "fingerstyle", "classical", "piedmont", "flamenco" }))
        return MelodyProfile::folk;

    return MelodyProfile::standard;
}

double getKitMelodyDensity (const juce::String& name)
{
    if (containsIgnoreCase (name, { "bluegrass", "funk" }))   return 6.0;
    if (containsIgnoreCase (name, { "ambient", "dub" }))      return 2.0;
    if (containsIgnoreCase (name, { "ballad" }))              return 3.0;
    return 4.0;
}

//==============================================================================
int resolveMelodyPitch (const MelodyPitch& pitch, const ChordCell* chord, int tonic, TuneMode mode)
{
    if (pitch.kind == MelodyPitch::Kind::absolute)
        return juce::jlimit (0, 127, pitch.value);

    const ChordCell c = chord != nullptr ? *chord : makeTonicChord (tonic, mode);
    const int rootPitch = 60 + wrapPitchClass (c.root);

    if (pitch.kind == MelodyPitch::Kind::rootOffset)
        return juce::jlimit (0, 127, rootPitch + pitch.value);

    const auto intervals = getChordToneIntervals (c);

    if (intervals.empty())
        return juce::jlimit (0, 127, rootPitch);

    // Past the last chord tone the list repeats a whole number of octaves up,
    // so chord_tone_N always rises with N.
    const int n = juce::jmax (1, pitch.value) - 1;
    const int size = (int) intervals.size();
    const int octaveSpan = 12 * (intervals.back() / 12 + 1);

    return juce::jlimit (0, 127, rootPitch + intervals[(size_t) (n % size)] + octaveSpan * (n / size));
}

int resolveNotePitch (const Tune& tune, int sectionIndex, const MelodyNote& note)
{
    SectionContext ctx;

    if (! makeContext (tune, sectionIndex, ctx))
        return resolveMelodyPitch (note.pitch, nullptr, tune.meta.keyTonic, tune.meta.mode);

    return resolveInContext (ctx, note);
}

//==============================================================================
std::vector<MelodyNote> generateAutoMelody (const Tune& tune, int sectionIndex, int seed)
{
    SectionContext ctx;

    if (! makeContext (tune, sectionIndex, ctx))
        return {};

    const auto& section = *ctx.section;
    const MelodyTrack track = section.melody.has_value() ? *section.melody : MelodyTrack();

    // A range narrower than an octave has nowhere to step, so it is widened
    // upward to one.
    const int low = juce::jlimit (0, 115, juce::jmin (track.rangeLow, track.rangeHigh));
    const int high = juce::jlimit (low + 12, 127, juce::jmax (track.rangeLow, track.rangeHigh));
    const int centre = (low + high) / 2;

    const auto profile = getMelodyProfileForKit (section.genreKitId);

    uint16_t stepMask = getScaleMask (ctx.tonic, ctx.mode);

    if (profile == MelodyProfile::country)
        stepMask = getPentatonicMask (ctx.tonic, ctx.mode);
    else if (profile == MelodyProfile::blues)
        stepMask = getBluesMask (ctx.tonic);

    RtRandom rng (mixSeed (seed, 0x4D454C4F));

    // ---- locked notes: kept exactly, and heard by the line around them ---------
    struct LockedNote { double start; double end; int pitch; };

    std::vector<MelodyNote> result;
    std::vector<LockedNote> locked;

    for (const auto& n : track.notes)
    {
        if (! n.locked)
            continue;

        result.push_back (n);
        locked.push_back ({ n.startBeat, n.getEndBeat(), resolveInContext (ctx, n) });
    }

    // ---- rhythm (4.1: density, and rests at the cadence points) ---------------------
    const double bar = ctx.beatsPerBar;
    const int numBars = juce::jmax (1, section.lengthBars);
    const double grid = 0.5;

    // The last beat of every bar rests, and so does the section's last bar.
    const double playable = bar - 1.0 >= grid ? bar - 1.0 : bar;
    const int barsWithNotes = numBars > 1 ? numBars - 1 : 1;

    const int slotsPerBar = juce::jmax (1, (int) std::floor (playable / grid + 1.0e-9));
    const int wanted = juce::jlimit (1, slotsPerBar, (int) std::lround (track.density));

    struct Onset { double start; double end; bool chordStart; bool barStart; };
    std::vector<Onset> onsets;

    for (int b = 0; b < barsWithNotes; ++b)
    {
        const double barStart = (double) b * bar;

        std::vector<int> chosen { 0 };
        std::vector<int> free;

        for (int s = 1; s < slotsPerBar; ++s)
        {
            // A chord change inside the bar always gets a note, so the line
            // is heard to move with the harmony.
            if (ctx.isChordStart (canonical (barStart + (double) s * grid)))
                chosen.push_back (s);
            else
                free.push_back (s);
        }

        while ((int) chosen.size() < wanted && ! free.empty())
        {
            const int pick = rng.nextInt ((int) free.size());
            chosen.push_back (free[(size_t) pick]);
            free.erase (free.begin() + pick);
        }

        std::sort (chosen.begin(), chosen.end());

        const double restFrom = barStart + playable;

        for (size_t k = 0; k < chosen.size(); ++k)
        {
            const double start = canonical (barStart + (double) chosen[k] * grid);
            const double end = canonical (k + 1 < chosen.size() ? barStart + (double) chosen[k + 1] * grid
                                                                : restFrom);

            if (start < ctx.lengthBeats - kEps)
                onsets.push_back ({ start, juce::jmin (end, ctx.lengthBeats), ctx.isChordStart (start), chosen[k] == 0 });
        }
    }

    // ---- pitches (4.1: 70% step, 20% chord-tone leap, 10% wider) ------------------------
    int previous = -1;
    double previousAt = -1.0;
    int direction = 0;

    std::vector<MelodyNote> generated;

    for (size_t i = 0; i < onsets.size(); ++i)
    {
        auto onset = onsets[i];
        bool covered = false;

        for (const auto& l : locked)
        {
            // A locked note that started since the last note we know of is the
            // pitch the line continues from.
            if (l.start > previousAt + kEps && l.start <= onset.start + kEps)
            {
                previous = l.pitch;
                previousAt = l.start;
            }

            if (l.start <= onset.start + kEps && l.end > onset.start + kEps)
                covered = true;
            else if (l.start > onset.start + kEps && l.start < onset.end - kEps)
                onset.end = l.start;
        }

        if (covered || onset.end - onset.start < 0.125 - kEps)
            continue;

        const auto& chord = ctx.chordAt (onset.start);
        const uint16_t chordMask = chordToneMask (chord);
        const auto tones = getChordTones (chord);

        int pitch = previous;

        auto step = [&] (int dir)
        {
            int p = moveInMask (previous, dir, stepMask);

            if (p < low || p > high)
                p = moveInMask (previous, -dir, stepMask);

            return p;
        };

        auto stepDirection = [&]() -> int
        {
            if (profile == MelodyProfile::folk)
            {
                const int root = nearestWithPitchClass (chord.root, previous, low, high);

                if (root != previous)
                    return root > previous ? 1 : -1;
            }

            if (profile == MelodyProfile::country && direction != 0 && rng.nextDouble() < 0.65)
                return direction;

            // Leans back toward the middle of the range, so the line does not
            // wander off one end and stay there.
            const double pull = juce::jlimit (-0.3, 0.3, 0.3 * (double) (centre - previous)
                                                           / (double) juce::jmax (1, (high - low) / 2));
            return rng.nextDouble() < 0.5 + pull ? 1 : -1;
        };

        auto leapCandidates = [&] (uint16_t mask, int minDistance, int maxDistance)
        {
            std::vector<int> c;

            for (int p = low; p <= high; ++p)
            {
                const int d = std::abs (p - previous);

                if (d >= minDistance && d <= maxDistance && isInMask (p, mask))
                    c.push_back (p);
            }

            return c;
        };

        if (previous < 0)
        {
            // "Start on a chord tone of the first chord (default third)."
            const int pc = tones.size() > 1 ? tones[1] : tones[0];
            pitch = nearestWithPitchClass (pc, centre, low, high);
        }
        else if (profile == MelodyProfile::jazz && onset.chordStart && tones.size() > 1)
        {
            // Guide tones: the new chord's third or seventh, whichever is closer.
            std::vector<int> guides { tones[1] };

            if (tones.size() > 3)
                guides.push_back (tones[3]);

            int best = previous;
            int bestDistance = 1000;

            for (int pc : guides)
            {
                const int p = nearestWithPitchClass (pc, previous, low, high);

                if (std::abs (p - previous) < bestDistance)
                {
                    best = p;
                    bestDistance = std::abs (p - previous);
                }
            }

            pitch = best;
        }
        else
        {
            const double roll = rng.nextDouble();

            if (roll < 0.7)
            {
                pitch = step (stepDirection());
            }
            else
            {
                auto candidates = roll < 0.9 ? leapCandidates (chordMask, 3, 7)
                                             : leapCandidates (stepMask, 8, 12);

                if (candidates.empty() && roll >= 0.9)
                    candidates = leapCandidates (chordMask, 3, 7);

                pitch = candidates.empty() ? step (stepDirection())
                                           : candidates[(size_t) rng.nextInt ((int) candidates.size())];
            }

            // A chord change lands on a chord tone; country's runs are allowed
            // to pass through.
            if (onset.chordStart && profile != MelodyProfile::country && ! isInMask (pitch, chordMask))
                pitch = nearestChordTone (pitch, chordMask, low, high, pitch - previous);
        }

        pitch = foldIntoRange (pitch, low, high);

        // The note before the closing rest resolves to the chord's root.
        if (i + 1 == onsets.size())
            pitch = nearestWithPitchClass (chord.root, pitch, low, high);

        int velocity = 84;

        if (onset.barStart)
            velocity += 10;
        else if (isOnBeat (onset.start))
            velocity += 4;
        else
            velocity -= 4;

        velocity += rng.nextInt (7) - 3;

        if (previous >= 0 && pitch != previous)
            direction = pitch > previous ? 1 : -1;

        previous = pitch;
        previousAt = onset.start;

        generated.push_back (MelodyNote::make (onset.start, onset.end - onset.start, pitch, velocity));
    }

    result.insert (result.end(), generated.begin(), generated.end());
    sortByStart (result);
    return result;
}

bool generateMelody (Tune& tune, int sectionIndex)
{
    auto* section = tune.getSection (sectionIndex);

    if (section == nullptr)
        return false;

    const int seed = section->melody.has_value() ? section->melody->seed : 1;
    auto notes = generateAutoMelody (tune, sectionIndex, seed);

    if (! section->melody.has_value())
        section->melody = MelodyTrack();

    if (section->melody->notes == notes && section->melody->source == MelodySource::autoGenerate)
        return false;

    section->melody->notes = std::move (notes);
    section->melody->source = MelodySource::autoGenerate;
    return true;
}

bool regenerateMelody (Tune& tune, int sectionIndex)
{
    auto* section = tune.getSection (sectionIndex);

    if (section == nullptr)
        return false;

    if (! section->melody.has_value())
        section->melody = MelodyTrack();

    auto& seed = section->melody->seed;
    seed = seed >= 0x7fffffff - 1 ? 1 : seed + 1;

    generateMelody (tune, sectionIndex);
    return true;
}

//==============================================================================
std::vector<MelodyNote> generateImprovisedPass (const Tune& tune, int sectionIndex, int pass)
{
    const auto* section = tune.getSection (sectionIndex);

    if (section == nullptr)
        return {};

    const int baseSeed = section->melody.has_value() ? section->melody->seed : 1;
    const int passSeed = (int) (mixSeed (baseSeed, 0x494D5052 + pass) & 0x7fffffffull);

    return generateAutoMelody (tune, sectionIndex, passSeed);
}

bool freezeImprovisedPass (Tune& tune, int sectionIndex, int pass)
{
    if (! tune.isValidSection (sectionIndex))
        return false;

    // Frozen notes are written notes, not locked ones: a later Auto is
    // expected to replace them, exactly as it would replace its own output.
    return tune.setMelodyNotes (sectionIndex, generateImprovisedPass (tune, sectionIndex, pass),
                                MelodySource::draw);
}

//==============================================================================
std::vector<MelodyNote> generateCountermelody (const Tune& tune, int sectionIndex, int seed)
{
    SectionContext ctx;

    if (! makeContext (tune, sectionIndex, ctx))
        return {};

    const auto& section = *ctx.section;
    const MelodyTrack track = section.melody.has_value() ? *section.melody : MelodyTrack();

    struct Sounding { double start; double end; int pitch; };
    std::vector<Sounding> main;

    for (const auto& n : track.notes)
        main.push_back ({ n.startBeat, n.getEndBeat(), resolveInContext (ctx, n) });

    const int low = juce::jmax (0, track.rangeLow - 5);
    const int high = juce::jmin (127, track.rangeLow + 19);

    RtRandom rng (mixSeed (seed, 0x434E5452));

    const double grid = 0.5;
    const double lastStart = ctx.lengthBeats - 1.0;
    std::vector<double> onsets;

    for (double t = 0.0; t < lastStart - kEps; t += grid)
    {
        const double at = canonical (t);
        bool melodyStarts = false, melodySounds = false;

        for (const auto& m : main)
        {
            if (std::abs (m.start - at) < kEps)
                melodyStarts = true;
            else if (m.start < at && m.end > at + kEps)
                melodySounds = true;
        }

        // Move where the melody holds or rests; stay out of its way where it
        // moves.
        const double chance = melodyStarts ? 0.1 : (melodySounds ? 0.45 : 0.6);

        if (rng.nextDouble() < chance)
            onsets.push_back (at);
    }

    std::vector<MelodyNote> notes;
    int previous = -1;

    for (size_t i = 0; i < onsets.size(); ++i)
    {
        const double start = onsets[i];
        const double end = juce::jmin (i + 1 < onsets.size() ? onsets[i + 1] : ctx.lengthBeats,
                                       start + 1.5, ctx.lengthBeats);

        const auto& chord = ctx.chordAt (start);
        const uint16_t mask = chordToneMask (chord);

        int pitch;

        if (previous < 0)
        {
            pitch = nearestWithPitchClass (chord.root, (low + high) / 2, low, high);
        }
        else
        {
            std::vector<int> near;

            for (int p = juce::jmax (low, previous - 4); p <= juce::jmin (high, previous + 4); ++p)
                if (p != previous && isInMask (p, mask))
                    near.push_back (p);

            pitch = near.empty() ? nearestChordTone (previous, mask, low, high, 0)
                                 : near[(size_t) rng.nextInt ((int) near.size())];
        }

        // Stay under whatever the melody is holding.
        for (const auto& m : main)
        {
            if (m.start <= start + kEps && m.end > start + kEps && pitch >= m.pitch - 2)
                pitch = m.pitch - 3 >= low ? nearestChordTone (m.pitch - 3, mask, low, m.pitch - 3, -1)
                                           : pitch - 12;
        }

        pitch = foldIntoRange (pitch, low, high);
        previous = pitch;

        notes.push_back (MelodyNote::make (start, end - start, pitch, 66 + rng.nextInt (9)));
    }

    return notes;
}

//==============================================================================
const char* getQuantiseGridName (QuantiseGrid grid) noexcept
{
    switch (grid)
    {
        case QuantiseGrid::quarter:          return "1/4";
        case QuantiseGrid::eighth:           return "1/8";
        case QuantiseGrid::eighthTriplet:    return "1/8T";
        case QuantiseGrid::sixteenth:        return "1/16";
        case QuantiseGrid::sixteenthTriplet: return "1/16T";
        case QuantiseGrid::numGrids:         break;
    }

    return "1/8";
}

double getQuantiseGridBeats (QuantiseGrid grid) noexcept
{
    switch (grid)
    {
        case QuantiseGrid::quarter:          return 1.0;
        case QuantiseGrid::eighth:           return 0.5;
        case QuantiseGrid::eighthTriplet:    return 1.0 / 3.0;
        case QuantiseGrid::sixteenth:        return 0.25;
        case QuantiseGrid::sixteenthTriplet: return 1.0 / 6.0;
        case QuantiseGrid::numGrids:         break;
    }

    return 0.5;
}

double snapBeatToGrid (double beat, double gridBeats) noexcept
{
    if (gridBeats <= 0.0)
        return canonical (beat);

    return canonical (std::round (beat / gridBeats) * gridBeats);
}

int snapPitchToKey (int midiNote, int tonic, TuneMode mode) noexcept
{
    return snapToScale (midiNote, tonic, mode);
}

std::vector<MelodyNote> quantiseRecording (const std::vector<RecordedNote>& notes, QuantiseGrid grid,
                                           const Tune& tune, int sectionIndex,
                                           bool followChordChanges, bool snapToKey)
{
    SectionContext ctx;
    const bool haveContext = makeContext (tune, sectionIndex, ctx);
    const double g = getQuantiseGridBeats (grid);

    std::vector<MelodyNote> out;

    for (const auto& raw : notes)
    {
        const double start = juce::jmax (0.0, snapBeatToGrid (raw.startBeat, g));
        double end = snapBeatToGrid (raw.endBeat, g);

        // A note released before the next grid line still sounds for one step.
        if (end <= start + kEps)
            end = canonical (start + g);

        int pitch = juce::jlimit (0, 127, raw.pitch);

        if (snapToKey)
            pitch = snapPitchToKey (pitch, tune.meta.keyTonic, tune.meta.mode);

        const int velocity = juce::jlimit (1, 127, raw.velocity);

        std::vector<std::pair<double, double>> parts { { start, end } };

        if (followChordChanges && haveContext)
        {
            parts.clear();
            double partStart = start;

            for (const auto& span : ctx.spans)
            {
                if (span.startBeat > partStart + kEps && span.startBeat < end - kEps)
                {
                    parts.push_back ({ partStart, span.startBeat });
                    partStart = span.startBeat;
                }
            }

            parts.push_back ({ partStart, end });
        }

        for (size_t p = 0; p < parts.size(); ++p)
        {
            int partPitch = pitch;

            // The first part is what was played; the parts after a chord
            // change are re-fitted to the chord they now sit under.
            if (p > 0)
                partPitch = nearestChordTone (pitch, chordToneMask (ctx.chordAt (parts[p].first)), 0, 127, 0);

            auto note = MelodyNote::make (parts[p].first, parts[p].second - parts[p].first, partPitch, velocity);
            note.locked = true;
            out.push_back (note);
        }
    }

    sortByStart (out);
    return out;
}

//==============================================================================
const char* getMelodyStyleDisplayName (MelodyStyle style) noexcept
{
    switch (style)
    {
        case MelodyStyle::none:             return "None";
        case MelodyStyle::bluegrassFiddle:  return "Bluegrass fiddle";
        case MelodyStyle::jazzSax:          return "Jazz sax";
        case MelodyStyle::bluesGuitar:      return "Blues guitar";
        case MelodyStyle::classicalGuitar:  return "Classical guitar";
        case MelodyStyle::countryChicken:   return "Country chicken pickin'";
        case MelodyStyle::numStyles:        break;
    }

    return "None";
}

std::vector<MelodyNote> applyMelodyStyle (const std::vector<MelodyNote>& notes, MelodyStyle style)
{
    if (style == MelodyStyle::none)
        return notes;

    auto out = notes;
    constexpr double swingShift = 1.0 / 6.0;   // an offbeat eighth moved to the triplet

    for (size_t i = 0; i < out.size(); ++i)
    {
        auto& n = out[i];
        const auto& original = notes[i];
        const double duration = original.durationBeats;

        switch (style)
        {
            case MelodyStyle::bluegrassFiddle:
                // The off-beat drive, short notes bowed short, slides into
                // the long ones, and the beat pushed slightly.
                if (isOffbeatEighth (original.startBeat))
                    n.velocity += 10;

                if (duration <= 0.5 + kEps && n.articulation == NoteArticulation::inherit)
                    n.articulation = NoteArticulation::staccato;

                if (duration >= 1.5 - kEps && n.technique == NoteTechnique::none)
                    n.technique = NoteTechnique::slide;

                if (isOnBeat (original.startBeat) && original.startBeat > 0.0)
                    n.startBeat -= 0.02;
                break;

            case MelodyStyle::jazzSax:
                // Swung eighths, legato, accented offbeats, vibrato on long notes.
                if (isOffbeatEighth (original.startBeat))
                {
                    n.startBeat += swingShift;
                    n.durationBeats = juce::jmax (0.0625, duration - swingShift);
                    n.velocity += 6;
                }
                else if (isOffbeatEighth (original.getEndBeat()))
                {
                    n.durationBeats = duration + swingShift;
                }

                if (n.articulation == NoteArticulation::inherit)
                    n.articulation = NoteArticulation::legato;

                if (duration >= 1.5 - kEps && n.technique == NoteTechnique::none)
                    n.technique = NoteTechnique::vibrato;
                break;

            case MelodyStyle::bluesGuitar:
                // Laid back, vibrato on the held notes, small steps slurred.
                n.startBeat += 0.03;

                if (duration >= 1.0 - kEps && n.technique == NoteTechnique::none)
                {
                    n.technique = NoteTechnique::vibrato;
                }
                else if (i > 0 && n.technique == NoteTechnique::none
                           && original.pitch.isAbsolute() && notes[i - 1].pitch.isAbsolute()
                           && std::abs (notes[i - 1].getEndBeat() - original.startBeat) < 0.01)
                {
                    const int interval = original.pitch.value - notes[i - 1].pitch.value;

                    if (interval >= 1 && interval <= 2)
                        n.technique = NoteTechnique::hammerOn;
                    else if (interval <= -1 && interval >= -2)
                        n.technique = NoteTechnique::pullOff;
                }
                break;

            case MelodyStyle::classicalGuitar:
                // Even and legato: the dynamics drawn in toward the middle.
                if (n.articulation == NoteArticulation::inherit)
                    n.articulation = NoteArticulation::legato;

                n.velocity = (n.velocity + 80) / 2;
                break;

            case MelodyStyle::countryChicken:
                // Snapped short notes, the shortest muted, offbeats popped.
                if (duration <= 0.5 + kEps && n.articulation == NoteArticulation::inherit)
                    n.articulation = duration <= 0.25 + kEps ? NoteArticulation::palmMuted
                                                             : NoteArticulation::staccato;

                if (isOffbeatEighth (original.startBeat))
                    n.velocity += 8;
                break;

            case MelodyStyle::none:
            case MelodyStyle::numStyles:
                break;
        }

        n.startBeat = canonical (juce::jmax (0.0, n.startBeat));
        n.durationBeats = canonical (juce::jmax (0.015625, n.durationBeats));
        n.velocity = juce::jlimit (1, 127, n.velocity);
    }

    return out;
}

//==============================================================================
BassPattern getGenreBassPattern (const juce::String& name)
{
    if (containsIgnoreCase (name, { "country", "bluegrass", "nashville" }))  return BassPattern::rootFifth;
    if (containsIgnoreCase (name, { "jazz", "gypsy", "swing" }))              return BassPattern::walking;
    if (containsIgnoreCase (name, { "bossa", "samba", "latin" }))             return BassPattern::bossa;
    if (containsIgnoreCase (name, { "blues", "delta", "chicago", "piedmont" })) return BassPattern::boogie;
    if (containsIgnoreCase (name, { "reggae", "rocksteady", "dub", "skank" })) return BassPattern::reggae;
    if (containsIgnoreCase (name, { "funk" }))                                return BassPattern::funk;
    if (containsIgnoreCase (name, { "punk", "metal", "djent", "chug" }))      return BassPattern::drivingEighths;
    return BassPattern::root;
}

std::vector<MelodyNote> generateBassPattern (const Tune& tune, int sectionIndex, BassPattern pattern)
{
    SectionContext ctx;

    if (! makeContext (tune, sectionIndex, ctx))
        return {};

    const double bar = ctx.beatsPerBar;
    const double total = ctx.lengthBeats;

    // A section without chords still gets a bass: the tonic, for its length.
    auto spans = ctx.spans;
    const bool tonicOnly = spans.empty();

    if (tonicOnly && total > 0.0)
        spans.push_back ({ 0, 0.0, total });

    auto chordOf = [&] (size_t spanIndex) -> const ChordCell&
    {
        if (tonicOnly)
            return ctx.tonicChord;

        return ctx.section->chords[(size_t) spans[spanIndex].cellIndex];
    };

    // The bass register: E1 (28) to D#2 (39) for the lowest note of a chord.
    auto lowPitch = [] (int pitchClass) { return 28 + wrapPitchClass (pitchClass - 4); };

    std::vector<MelodyNote> out;

    auto add = [&] (double start, double duration, int pitch, int velocity, double limit)
    {
        const double end = juce::jmin (start + duration, limit, total);

        if (end - start > kEps && start < total - kEps)
            out.push_back (MelodyNote::make (start, end - start, juce::jlimit (0, 127, pitch), velocity));
    };

    struct Hit { double offset; double duration; int interval; int velocity; };

    // Figures per bar, as offsets from where the chord starts (4/4 shapes;
    // anything past a shorter bar or the chord's end is clipped).
    auto playFigure = [&] (const ChordSpan& span, int root, const std::vector<Hit>& figure)
    {
        for (double base = span.startBeat; base < span.endBeat - kEps; base += bar)
            for (const auto& h : figure)
                if (h.offset < bar - kEps)
                    add (canonical (base + h.offset), h.duration, root + h.interval, h.velocity, span.endBeat);
    };

    for (size_t si = 0; si < spans.size(); ++si)
    {
        const auto& span = spans[si];
        const auto& chord = chordOf (si);
        const auto& next = chordOf ((si + 1) % spans.size());

        const int root = lowPitch (chord.root);
        const int bassNote = lowPitch (chord.bass >= 0 ? chord.bass : chord.root);
        const auto intervals = getChordToneIntervals (chord);
        const int third = intervals.size() > 1 ? intervals[1] : 4;
        const int fifth = intervals.size() > 2 ? intervals[2] : 7;

        switch (pattern)
        {
            case BassPattern::root:
                playFigure (span, bassNote, { { 0.0, bar, 0, 100 } });
                break;

            case BassPattern::rootFifth:
            {
                // Half notes in an even meter, a note a bar otherwise: the
                // country waltz alternates root and fifth bar by bar.
                const double step = (bar >= 4.0 - kEps && std::fmod (bar, 2.0) < kEps) ? 2.0 : bar;
                int k = 0;

                for (double t = span.startBeat; t < span.endBeat - kEps; t = canonical (span.startBeat + (double) (++k) * step))
                    add (t, step, (k % 2 == 0) ? bassNote : root + fifth, (k % 2 == 0) ? 100 : 90, span.endBeat);
                break;
            }

            case BassPattern::walking:
            {
                // Quarter notes up and down the chord, the last beat a half
                // step from the next chord's root.
                const int seventh = intervals.size() > 3 ? intervals[3] : 12;
                const int line[8] = { 0, third, fifth, seventh, 12, seventh, fifth, third };
                const int beats = juce::jmax (1, (int) std::floor (span.endBeat - span.startBeat + kEps));
                const int nextRoot = lowPitch (next.bass >= 0 ? next.bass : next.root);
                int previous = bassNote;

                for (int k = 0; k < beats; ++k)
                {
                    int pitch = k == 0 ? bassNote : root + line[k % 8];

                    if (k == beats - 1 && beats >= 2)
                        pitch = nextRoot >= previous ? nextRoot - 1 : nextRoot + 1;

                    add (canonical (span.startBeat + (double) k), 1.0, pitch, k == 0 ? 100 : 88, span.endBeat);
                    previous = pitch;
                }
                break;
            }

            case BassPattern::boogie:
            {
                const int line[8] = { 0, third, 7, 9, 10, 9, 7, third };
                const int beats = juce::jmax (1, (int) std::floor (span.endBeat - span.startBeat + kEps));

                for (int k = 0; k < beats; ++k)
                    add (canonical (span.startBeat + (double) k), 1.0, root + line[k % 8], k % 2 == 0 ? 100 : 86, span.endBeat);
                break;
            }

            case BassPattern::reggae:
                playFigure (span, root, { { 0.0, 1.5, 0, 104 }, { 2.0, 0.5, fifth, 88 }, { 2.5, 1.0, 0, 94 } });
                break;

            case BassPattern::bossa:
                playFigure (span, root, { { 0.0, 1.5, 0, 100 }, { 1.5, 0.5, fifth, 86 },
                                          { 2.0, 1.5, fifth, 94 }, { 3.5, 0.5, 0, 84 } });
                break;

            case BassPattern::funk:
                playFigure (span, root, { { 0.0, 0.5, 0, 108 }, { 0.75, 0.25, 12, 90 }, { 1.5, 0.5, 0, 96 },
                                          { 2.5, 0.5, 10, 92 }, { 3.0, 0.25, 12, 88 }, { 3.5, 0.5, 0, 94 } });
                break;

            case BassPattern::drivingEighths:
            {
                int k = 0;

                for (double t = span.startBeat; t < span.endBeat - kEps; t = canonical (span.startBeat + 0.5 * (double) (++k)))
                    add (t, 0.5, bassNote, k % 2 == 0 ? 104 : 92, span.endBeat);
                break;
            }
        }
    }

    sortByStart (out);
    return out;
}

std::vector<MelodyNote> generateBassLine (const Tune& tune, int sectionIndex)
{
    const auto* section = tune.getSection (sectionIndex);

    if (section == nullptr)
        return {};

    switch (section->bass.mode)
    {
        case BassMode::off:       return {};
        case BassMode::manual:    return section->bass.notes;
        case BassMode::root:      return generateBassPattern (tune, sectionIndex, BassPattern::root);
        case BassMode::rootFifth: return generateBassPattern (tune, sectionIndex, BassPattern::rootFifth);
        case BassMode::walking:   return generateBassPattern (tune, sectionIndex, BassPattern::walking);
        case BassMode::genre:
        {
            // A linked section plays its source's rhythm, so its bass follows
            // the source's kit too.
            const auto* source = tune.getSection (tune.getRhythmSourceIndex (sectionIndex));
            const auto kit = source != nullptr ? source->genreKitId : section->genreKitId;
            return generateBassPattern (tune, sectionIndex, getGenreBassPattern (kit));
        }
        case BassMode::numModes:  break;
    }

    return {};
}

} // namespace luthier
