#include "TuneModel.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

//==============================================================================
namespace
{
    const char* const kEmphasisNames[]     = { "normal", "accent", "ghost" };
    const char* const kArticulationNames[] = { "inherit", "natural", "legato", "staccato", "palm_muted", "let_ring" };
    const char* const kTechniqueNames[]    = { "none", "bend", "slide", "hammer_on", "pull_off", "vibrato", "harmonic" };
    const char* const kSourceNames[]       = { "draw", "auto", "record", "improvise", "sing" };
    const char* const kBassModeNames[]     = { "off", "root", "root_fifth", "walking", "genre", "manual" };
    const char* const kLayerTypeNames[]    = { "pad", "arpeggio", "countermelody", "percussion" };
    const char* const kRoleNames[]         = { "none", "intro", "verse", "chorus", "bridge", "outro" };
    const char* const kStyleNames[]        = { "none", "bluegrass_fiddle", "jazz_sax", "blues_guitar",
                                               "classical_guitar", "country_chicken" };
    const char* const kEditClassNames[]    = { "tune-section-edit", "tune-chord-edit", "tune-melody-edit",
                                               "tune-melody-record", "tune-melody-generate", "tune-other" };

    template <typename Enum, size_t N>
    const char* nameOf (const char* const (&names)[N], Enum value) noexcept
    {
        return names[(size_t) juce::jlimit (0, (int) N - 1, (int) value)];
    }

    template <typename Enum, size_t N>
    bool parseName (const char* const (&names)[N], const juce::String& text, Enum& result)
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (text == names[i])
            {
                result = (Enum) (int) i;
                return true;
            }
        }

        return false;
    }

    bool validSectionIndex (const std::vector<TuneSection>& sections, int index) noexcept
    {
        return juce::isPositiveAndBelow (index, (int) sections.size());
    }

    /** Sorts and de-duplicates note indices, dropping any out of range, so a
        multi-select edit touches each note once. */
    std::vector<int> cleanIndices (std::vector<int> indices, int size)
    {
        indices.erase (std::remove_if (indices.begin(), indices.end(),
                                       [size] (int i) { return ! juce::isPositiveAndBelow (i, size); }),
                       indices.end());
        std::sort (indices.begin(), indices.end());
        indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
        return indices;
    }

    MelodyNote canonicalNote (MelodyNote note)
    {
        note.startBeat = tunetheory::canonical (juce::jmax (0.0, note.startBeat));
        note.durationBeats = tunetheory::canonical (juce::jmax (tunetheory::kBeatResolution, note.durationBeats));
        note.velocity = juce::jlimit (1, 127, note.velocity);

        if (note.pitch.isAbsolute())
            note.pitch.value = juce::jlimit (0, 127, note.pitch.value);

        return note;
    }
}

//==============================================================================
const char* getChordEmphasisName (ChordEmphasis v) noexcept       { return nameOf (kEmphasisNames, v); }
const char* getNoteArticulationName (NoteArticulation v) noexcept { return nameOf (kArticulationNames, v); }
const char* getNoteTechniqueName (NoteTechnique v) noexcept       { return nameOf (kTechniqueNames, v); }
const char* getMelodySourceName (MelodySource v) noexcept         { return nameOf (kSourceNames, v); }
const char* getBassModeName (BassMode v) noexcept                 { return nameOf (kBassModeNames, v); }
const char* getLayerTypeName (LayerType v) noexcept               { return nameOf (kLayerTypeNames, v); }
const char* getSectionRoleName (SectionRole v) noexcept           { return nameOf (kRoleNames, v); }
const char* getMelodyStyleName (MelodyStyle v) noexcept           { return nameOf (kStyleNames, v); }
const char* getTuneEditClassName (TuneEditClass v) noexcept       { return nameOf (kEditClassNames, v); }

const char* getTuneNotePartName (TuneNotePart part) noexcept
{
    switch (part)
    {
        case TuneNotePart::melody:        return "melody";
        case TuneNotePart::bass:          return "bass";
        case TuneNotePart::countermelody: return "countermelody";
        case TuneNotePart::numParts:      break;
    }

    return "";
}

bool parseChordEmphasis (const juce::String& t, ChordEmphasis& r)       { return parseName (kEmphasisNames, t, r); }
bool parseNoteArticulation (const juce::String& t, NoteArticulation& r) { return parseName (kArticulationNames, t, r); }
bool parseNoteTechnique (const juce::String& t, NoteTechnique& r)       { return parseName (kTechniqueNames, t, r); }
bool parseMelodySource (const juce::String& t, MelodySource& r)         { return parseName (kSourceNames, t, r); }
bool parseBassMode (const juce::String& t, BassMode& r)                 { return parseName (kBassModeNames, t, r); }
bool parseLayerType (const juce::String& t, LayerType& r)               { return parseName (kLayerTypeNames, t, r); }
bool parseSectionRole (const juce::String& t, SectionRole& r)           { return parseName (kRoleNames, t, r); }
bool parseMelodyStyle (const juce::String& t, MelodyStyle& r)           { return parseName (kStyleNames, t, r); }

//==============================================================================
bool tuneExtrasEqual (const juce::NamedValueSet& a, const juce::NamedValueSet& b)
{
    if (a.size() != b.size())
        return false;

    for (int i = 0; i < a.size(); ++i)
    {
        if (a.getName (i) != b.getName (i))
            return false;

        const auto& va = a.getValueAt (i);
        const auto& vb = b.getValueAt (i);

        if (va.isObject() || va.isArray() || vb.isObject() || vb.isArray())
        {
            if (juce::JSON::toString (va, true) != juce::JSON::toString (vb, true))
                return false;
        }
        else if (va != vb)
        {
            return false;
        }
    }

    return true;
}

//==============================================================================
ChordCell ChordCell::make (int root, const juce::String& quality, double beats)
{
    ChordCell cell;
    cell.root = tunetheory::wrapPitchClass (root);
    cell.quality = quality;
    cell.durationBeats = tunetheory::canonical (beats);
    return cell;
}

bool ChordCell::isValid() const
{
    if (! juce::isPositiveAndBelow (root, 12) || bass < -1 || bass > 11)
        return false;

    if (! tunetheory::isKnownQuality (quality))
        return false;

    for (const auto& e : extensions)
        if (tunetheory::getExtensionSemitones (e) < 0)
            return false;

    return std::isfinite (durationBeats);
}

bool ChordCell::operator== (const ChordCell& o) const
{
    return root == o.root && quality == o.quality && bass == o.bass
        && extensions == o.extensions && durationBeats == o.durationBeats
        && strumOverride == o.strumOverride && emphasis == o.emphasis
        && locked == o.locked
        && tuneExtrasEqual (extra, o.extra);
}

//==============================================================================
juce::var MelodyPitch::toVar() const
{
    switch (kind)
    {
        case Kind::absolute:
            return juce::var (value);

        case Kind::rootOffset:
            if (value == 0)
                return juce::var ("root");

            return juce::var (juce::String ("root") + (value > 0 ? "+" : "-") + juce::String (std::abs (value)));

        case Kind::chordTone:
            return juce::var (juce::String ("chord_tone_") + juce::String (value));
    }

    return juce::var (value);
}

bool MelodyPitch::fromVar (const juce::var& v, MelodyPitch& result)
{
    if (v.isInt() || v.isInt64() || v.isDouble())
    {
        const double d = (double) v;

        if (! std::isfinite (d) || d < 0.0 || d > 127.0 || d != std::floor (d))
            return false;

        result = absolute ((int) d);
        return true;
    }

    if (! v.isString())
        return false;

    const auto text = v.toString();

    auto readSignedInt = [] (const juce::String& digits, int& out)
    {
        if (digits.isEmpty() || digits.length() > 3 || ! digits.containsOnly ("0123456789"))
            return false;

        out = digits.getIntValue();
        return true;
    };

    if (text == "root")
    {
        result = rootOffset (0);
        return true;
    }

    if (text.startsWith ("root+") || text.startsWith ("root-"))
    {
        int amount = 0;

        if (! readSignedInt (text.substring (5), amount) || amount == 0 || amount > 60)
            return false;

        result = rootOffset (text[4] == '-' ? -amount : amount);
        return true;
    }

    if (text.startsWith ("chord_tone_"))
    {
        int index = 0;

        if (! readSignedInt (text.substring (11), index) || index < 1 || index > 32)
            return false;

        result = chordTone (index);
        return true;
    }

    return false;
}

//==============================================================================
MelodyNote MelodyNote::make (double start, double duration, int midiNote, int vel)
{
    MelodyNote note;
    note.startBeat = start;
    note.durationBeats = duration;
    note.pitch = MelodyPitch::absolute (midiNote);
    note.velocity = vel;
    return canonicalNote (note);
}

bool MelodyNote::operator== (const MelodyNote& o) const
{
    return startBeat == o.startBeat && durationBeats == o.durationBeats && pitch == o.pitch
        && velocity == o.velocity && articulation == o.articulation && technique == o.technique
        && locked == o.locked && tuneExtrasEqual (extra, o.extra);
}

int MelodyTrack::getNumLockedNotes() const noexcept
{
    return (int) std::count_if (notes.begin(), notes.end(), [] (const MelodyNote& n) { return n.locked; });
}

bool MelodyTrack::operator== (const MelodyTrack& o) const
{
    return notes == o.notes && stringHint == o.stringHint && articulationDefault == o.articulationDefault
        && source == o.source && seed == o.seed && density == o.density
        && rangeLow == o.rangeLow && rangeHigh == o.rangeHigh && followChords == o.followChords
        && tuneExtrasEqual (extra, o.extra);
}

bool BassTrack::operator== (const BassTrack& o) const
{
    return mode == o.mode && notes == o.notes && tuneExtrasEqual (extra, o.extra);
}

bool TuneLayer::operator== (const TuneLayer& o) const
{
    return type == o.type && enabled == o.enabled && volume == o.volume && pan == o.pan
        && patternId == o.patternId && seed == o.seed && notes == o.notes
        && tuneExtrasEqual (extra, o.extra);
}

const TuneLayer* TuneSection::findLayer (LayerType type) const noexcept
{
    for (const auto& layer : layers)
        if (layer.type == type)
            return &layer;

    return nullptr;
}

bool TuneSection::operator== (const TuneSection& o) const
{
    return name == o.name && lengthBars == o.lengthBars && chords == o.chords
        && rhythmPatternId == o.rhythmPatternId && genreKitId == o.genreKitId
        && melody == o.melody && bass == o.bass && layers == o.layers
        && role == o.role && rhythmOn == o.rhythmOn && rhythmLinkedTo == o.rhythmLinkedTo
        && feel == o.feel && strum == o.strum && stateBoundary == o.stateBoundary
        && style == o.style && tuneExtrasEqual (extra, o.extra);
}

bool TuneSetlistEntry::operator== (const TuneSetlistEntry& o) const
{
    return section == o.section && repeats == o.repeats && tuneExtrasEqual (extra, o.extra);
}

bool TuneArrangement::operator== (const TuneArrangement& o) const
{
    return sections == o.sections && setlist == o.setlist;
}

bool TuneVariation::operator== (const TuneVariation& o) const
{
    return name == o.name && arrangement == o.arrangement && tuneExtrasEqual (extra, o.extra);
}

bool TuneMeta::operator== (const TuneMeta& o) const
{
    return title == o.title && artist == o.artist && author == o.author
        && tempoBpm == o.tempoBpm && timeSigNumerator == o.timeSigNumerator
        && timeSigDenominator == o.timeSigDenominator && keyTonic == o.keyTonic && mode == o.mode
        && swingPercent == o.swingPercent && feelPercent == o.feelPercent
        && tags == o.tags && notes == o.notes && created == o.created && modified == o.modified
        && tuneExtrasEqual (extra, o.extra);
}

bool Tune::operator== (const Tune& o) const
{
    return meta == o.meta && arrangement == o.arrangement && variations == o.variations
        && tuneExtrasEqual (extra, o.extra);
}

//==============================================================================
double Tune::getBeatsPerBar() const noexcept
{
    return (double) juce::jmax (1, meta.timeSigNumerator) * 4.0
             / (double) juce::jmax (1, meta.timeSigDenominator);
}

bool Tune::isValidSection (int index) const noexcept
{
    return validSectionIndex (arrangement.sections, index);
}

TuneSection* Tune::getSection (int index) noexcept
{
    return isValidSection (index) ? &arrangement.sections[(size_t) index] : nullptr;
}

const TuneSection* Tune::getSection (int index) const noexcept
{
    return isValidSection (index) ? &arrangement.sections[(size_t) index] : nullptr;
}

int Tune::findSection (const juce::String& name) const noexcept
{
    for (size_t i = 0; i < arrangement.sections.size(); ++i)
        if (arrangement.sections[i].name == name)
            return (int) i;

    return -1;
}

double Tune::getSectionLengthBeats (int index) const noexcept
{
    if (const auto* s = getSection (index))
        return tunetheory::canonical ((double) juce::jmax (0, s->lengthBars) * getBeatsPerBar());

    return 0.0;
}

std::vector<TuneSpan> Tune::getPlayOrder() const
{
    std::vector<TuneSpan> spans;
    double position = 0.0;

    auto add = [&] (int sectionIndex, int setlistIndex, int repeat)
    {
        TuneSpan span;
        span.sectionIndex = sectionIndex;
        span.setlistIndex = setlistIndex;
        span.repeat = repeat;
        span.startBeat = position;
        span.lengthBeats = getSectionLengthBeats (sectionIndex);
        spans.push_back (span);

        // Canonical after every step, so a long tune's positions are the same
        // numbers however they were summed.
        position = tunetheory::canonical (position + span.lengthBeats);
    };

    if (arrangement.setlist.empty())
    {
        for (int i = 0; i < getNumSections(); ++i)
            add (i, -1, 0);

        return spans;
    }

    for (size_t e = 0; e < arrangement.setlist.size(); ++e)
    {
        const auto& entry = arrangement.setlist[e];
        const int index = findSection (entry.section);

        if (index < 0)
            continue;

        for (int r = 0; r < juce::jlimit (1, kMaxRepeats, entry.repeats); ++r)
            add (index, (int) e, r);
    }

    return spans;
}

double Tune::getTotalBeats() const
{
    const auto spans = getPlayOrder();
    return spans.empty() ? 0.0 : tunetheory::canonical (spans.back().startBeat + spans.back().lengthBeats);
}

int Tune::getRhythmSourceIndex (int index) const noexcept
{
    const auto* section = getSection (index);

    if (section == nullptr || section->rhythmLinkedTo.isEmpty())
        return index;

    const int linked = findSection (section->rhythmLinkedTo);
    return linked >= 0 ? linked : index;
}

juce::String Tune::makeUniqueSectionName (const juce::String& base) const
{
    const auto stem = base.trim().isEmpty() ? juce::String ("Section") : base.trim();

    if (findSection (stem) < 0)
        return stem;

    for (int n = 2; n < 1000; ++n)
    {
        const auto candidate = stem + " " + juce::String (n);

        if (findSection (candidate) < 0)
            return candidate;
    }

    return stem + " " + juce::String (juce::Time::currentTimeMillis());
}

juce::StringArray Tune::validate() const
{
    juce::StringArray problems;

    for (size_t i = 0; i < arrangement.sections.size(); ++i)
    {
        const auto& s = arrangement.sections[i];

        for (size_t j = 0; j < i; ++j)
            if (arrangement.sections[j].name == s.name)
                problems.add ("Two sections are named \"" + s.name + "\"; the setlist plays the first.");

        for (size_t c = 0; c < s.chords.size(); ++c)
            if (! s.chords[c].isValid())
                problems.add ("Section \"" + s.name + "\" chord " + juce::String ((int) c + 1) + " is not a chord Luthier knows.");

        if (s.rhythmLinkedTo.isNotEmpty() && findSection (s.rhythmLinkedTo) < 0)
            problems.add ("Section \"" + s.name + "\" links its rhythm to a missing section \"" + s.rhythmLinkedTo + "\".");
    }

    for (const auto& entry : arrangement.setlist)
        if (findSection (entry.section) < 0)
            problems.add ("The setlist names a missing section \"" + entry.section + "\".");

    return problems;
}

//==============================================================================
int Tune::addSection (TuneSection section, int insertAt)
{
    if (getNumSections() >= kMaxSections)
        return -1;

    section.name = makeUniqueSectionName (section.name);
    section.lengthBars = juce::jlimit (1, kMaxBars, section.lengthBars);

    const int index = juce::isPositiveAndBelow (insertAt, getNumSections() + 1) ? insertAt : getNumSections();
    arrangement.sections.insert (arrangement.sections.begin() + index, std::move (section));
    return index;
}

bool Tune::removeSection (int index)
{
    if (! isValidSection (index))
        return false;

    const auto name = arrangement.sections[(size_t) index].name;
    arrangement.sections.erase (arrangement.sections.begin() + index);

    // A duplicate name (only possible from a hand-edited file) keeps its
    // entries: they still resolve to the surviving section.
    if (findSection (name) < 0)
    {
        auto& list = arrangement.setlist;
        list.erase (std::remove_if (list.begin(), list.end(),
                                    [&name] (const TuneSetlistEntry& e) { return e.section == name; }),
                    list.end());

        for (auto& s : arrangement.sections)
            if (s.rhythmLinkedTo == name)
                s.rhythmLinkedTo.clear();
    }

    return true;
}

bool Tune::moveSection (int fromIndex, int toIndex)
{
    if (! isValidSection (fromIndex) || ! isValidSection (toIndex) || fromIndex == toIndex)
        return false;

    auto moved = std::move (arrangement.sections[(size_t) fromIndex]);
    arrangement.sections.erase (arrangement.sections.begin() + fromIndex);
    arrangement.sections.insert (arrangement.sections.begin() + toIndex, std::move (moved));
    return true;
}

int Tune::duplicateSection (int index)
{
    if (! isValidSection (index))
        return -1;

    return addSection (arrangement.sections[(size_t) index], index + 1);
}

bool Tune::renameSection (int index, const juce::String& newName)
{
    const auto name = newName.trim();

    if (! isValidSection (index) || name.isEmpty())
        return false;

    const auto oldName = arrangement.sections[(size_t) index].name;

    if (name == oldName || findSection (name) >= 0)
        return false;

    arrangement.sections[(size_t) index].name = name;

    for (auto& entry : arrangement.setlist)
        if (entry.section == oldName)
            entry.section = name;

    for (auto& s : arrangement.sections)
        if (s.rhythmLinkedTo == oldName)
            s.rhythmLinkedTo = name;

    return true;
}

bool Tune::setSectionLength (int index, int bars)
{
    auto* s = getSection (index);
    const int clamped = juce::jlimit (1, kMaxBars, bars);

    if (s == nullptr || s->lengthBars == clamped)
        return false;

    s->lengthBars = clamped;
    return true;
}

bool Tune::setSectionRole (int index, SectionRole role)
{
    auto* s = getSection (index);

    if (s == nullptr || s->role == role)
        return false;

    s->role = role;
    return true;
}

bool Tune::setSectionRhythm (int index, const juce::String& patternId, const juce::String& genreKitId)
{
    auto* s = getSection (index);

    if (s == nullptr || (s->rhythmPatternId == patternId && s->genreKitId == genreKitId))
        return false;

    s->rhythmPatternId = patternId;
    s->genreKitId = genreKitId;
    return true;
}

bool Tune::setSectionRhythmOn (int index, bool on)
{
    auto* s = getSection (index);

    if (s == nullptr || s->rhythmOn == on)
        return false;

    s->rhythmOn = on;
    return true;
}

bool Tune::linkSectionRhythm (int index, const juce::String& targetName)
{
    auto* s = getSection (index);

    if (s == nullptr || s->rhythmLinkedTo == targetName || targetName == s->name)
        return false;

    if (targetName.isNotEmpty() && findSection (targetName) < 0)
        return false;

    s->rhythmLinkedTo = targetName;
    return true;
}

bool Tune::setRepeatCount (int sectionIndex, int repeats)
{
    if (! isValidSection (sectionIndex))
        return false;

    const int clamped = juce::jlimit (1, kMaxRepeats, repeats);
    const auto& name = arrangement.sections[(size_t) sectionIndex].name;

    if (arrangement.setlist.empty())
    {
        if (clamped == 1)
            return false;

        for (const auto& s : arrangement.sections)
        {
            TuneSetlistEntry entry;
            entry.section = s.name;
            arrangement.setlist.push_back (entry);
        }
    }

    bool changed = false;

    for (auto& entry : arrangement.setlist)
    {
        if (entry.section == name && entry.repeats != clamped)
        {
            entry.repeats = clamped;
            changed = true;
        }
    }

    return changed;
}

bool Tune::setSetlist (std::vector<TuneSetlistEntry> newSetlist)
{
    for (auto& entry : newSetlist)
        entry.repeats = juce::jlimit (1, kMaxRepeats, entry.repeats);

    if (newSetlist == arrangement.setlist)
        return false;

    arrangement.setlist = std::move (newSetlist);
    return true;
}

//==============================================================================
bool Tune::insertChord (int sectionIndex, int chordIndex, const ChordCell& cell)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr)
        return false;

    auto c = cell;
    c.durationBeats = tunetheory::canonical (c.durationBeats);

    const int size = (int) s->chords.size();
    const int at = juce::isPositiveAndBelow (chordIndex, size + 1) ? chordIndex : size;
    s->chords.insert (s->chords.begin() + at, c);
    return true;
}

bool Tune::removeChord (int sectionIndex, int chordIndex)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || ! juce::isPositiveAndBelow (chordIndex, (int) s->chords.size()))
        return false;

    s->chords.erase (s->chords.begin() + chordIndex);
    return true;
}

bool Tune::moveChord (int sectionIndex, int fromIndex, int toIndex)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || fromIndex == toIndex
          || ! juce::isPositiveAndBelow (fromIndex, (int) s->chords.size())
          || ! juce::isPositiveAndBelow (toIndex, (int) s->chords.size()))
        return false;

    auto moved = s->chords[(size_t) fromIndex];
    s->chords.erase (s->chords.begin() + fromIndex);
    s->chords.insert (s->chords.begin() + toIndex, moved);
    return true;
}

bool Tune::setChord (int sectionIndex, int chordIndex, const ChordCell& cell)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || ! juce::isPositiveAndBelow (chordIndex, (int) s->chords.size()))
        return false;

    auto c = cell;
    c.durationBeats = tunetheory::canonical (c.durationBeats);

    if (s->chords[(size_t) chordIndex] == c)
        return false;

    s->chords[(size_t) chordIndex] = c;
    return true;
}

bool Tune::setChordDuration (int sectionIndex, int chordIndex, double beats)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || ! juce::isPositiveAndBelow (chordIndex, (int) s->chords.size()))
        return false;

    auto c = s->chords[(size_t) chordIndex];
    c.durationBeats = beats;
    return setChord (sectionIndex, chordIndex, c);
}

bool Tune::setChords (int sectionIndex, std::vector<ChordCell> cells)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr)
        return false;

    for (auto& c : cells)
        c.durationBeats = tunetheory::canonical (c.durationBeats);

    if (s->chords == cells)
        return false;

    s->chords = std::move (cells);
    return true;
}

bool Tune::setChordLocked (int sectionIndex, int chordIndex, bool locked)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || ! juce::isPositiveAndBelow (chordIndex, (int) s->chords.size()))
        return false;

    auto& c = s->chords[(size_t) chordIndex];

    if (c.locked == locked)
        return false;

    c.locked = locked;
    return true;
}

//==============================================================================
const std::vector<MelodyNote>* Tune::getPartNotes (int sectionIndex, TuneNotePart part) const noexcept
{
    const auto* s = getSection (sectionIndex);

    if (s == nullptr)
        return nullptr;

    switch (part)
    {
        case TuneNotePart::melody:
            return s->melody.has_value() ? &s->melody->notes : nullptr;

        case TuneNotePart::bass:
            return s->bass.mode == BassMode::manual ? &s->bass.notes : nullptr;

        case TuneNotePart::countermelody:
            if (const auto* layer = s->findLayer (LayerType::countermelody))
                return &layer->notes;
            return nullptr;

        case TuneNotePart::numParts:
            break;
    }

    return nullptr;
}

bool Tune::setPartNotes (int sectionIndex, TuneNotePart part, std::vector<MelodyNote> notes)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr)
        return false;

    // Canonical, in the caller's order: the roll keeps a note's index across
    // an edit, and the last drawn stays on top where notes overlap.
    for (auto& n : notes)
        n = canonicalNote (n);

    switch (part)
    {
        case TuneNotePart::melody:
        {
            if (! s->melody.has_value())
                s->melody = MelodyTrack();

            if (s->melody->notes == notes)
                return false;

            s->melody->notes = std::move (notes);
            return true;
        }

        case TuneNotePart::bass:
            return setBassNotes (sectionIndex, std::move (notes));

        case TuneNotePart::countermelody:
        {
            TuneLayer layer;

            if (const auto* existing = s->findLayer (LayerType::countermelody))
                layer = *existing;

            layer.type = LayerType::countermelody;
            layer.notes = std::move (notes);
            return setLayer (sectionIndex, layer);
        }

        case TuneNotePart::numParts:
            break;
    }

    return false;
}

//==============================================================================
bool Tune::setMelodyEnabled (int sectionIndex, bool enabled)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || s->melody.has_value() == enabled)
        return false;

    if (enabled)
        s->melody = MelodyTrack();
    else
        s->melody.reset();

    return true;
}

int Tune::addMelodyNote (int sectionIndex, MelodyNote note)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr)
        return -1;

    if (! s->melody.has_value())
        s->melody = MelodyTrack();

    note = canonicalNote (note);
    note.locked = true;

    s->melody->notes.push_back (note);
    return (int) s->melody->notes.size() - 1;
}

bool Tune::setMelodyNote (int sectionIndex, int noteIndex, MelodyNote note)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || ! s->melody.has_value()
          || ! juce::isPositiveAndBelow (noteIndex, (int) s->melody->notes.size()))
        return false;

    note = canonicalNote (note);
    note.locked = true;

    auto& target = s->melody->notes[(size_t) noteIndex];

    if (target == note)
        return false;

    target = note;
    return true;
}

bool Tune::removeMelodyNotes (int sectionIndex, std::vector<int> noteIndices)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || ! s->melody.has_value())
        return false;

    auto& notes = s->melody->notes;
    const auto indices = cleanIndices (std::move (noteIndices), (int) notes.size());

    for (auto it = indices.rbegin(); it != indices.rend(); ++it)
        notes.erase (notes.begin() + *it);

    return ! indices.empty();
}

bool Tune::setMelodyNoteLocked (int sectionIndex, int noteIndex, bool locked)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || ! s->melody.has_value()
          || ! juce::isPositiveAndBelow (noteIndex, (int) s->melody->notes.size()))
        return false;

    auto& note = s->melody->notes[(size_t) noteIndex];

    if (note.locked == locked)
        return false;

    note.locked = locked;
    return true;
}

bool Tune::nudgeMelodyNotes (int sectionIndex, const std::vector<int>& noteIndices,
                             double deltaBeats, int deltaSemitones)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || ! s->melody.has_value())
        return false;

    auto& notes = s->melody->notes;
    bool changed = false;

    for (int i : cleanIndices (noteIndices, (int) notes.size()))
    {
        auto note = notes[(size_t) i];
        note.startBeat += deltaBeats;

        if (note.pitch.isAbsolute())
            note.pitch.value += deltaSemitones;

        note = canonicalNote (note);
        note.locked = true;

        if (note != notes[(size_t) i])
        {
            notes[(size_t) i] = note;
            changed = true;
        }
    }

    return changed;
}

bool Tune::setMelodyNotes (int sectionIndex, std::vector<MelodyNote> notes, MelodySource source)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr)
        return false;

    for (auto& n : notes)
        n = canonicalNote (n);

    if (! s->melody.has_value())
        s->melody = MelodyTrack();
    else if (s->melody->notes == notes && s->melody->source == source)
        return false;

    s->melody->notes = std::move (notes);
    s->melody->source = source;
    return true;
}

//==============================================================================
bool Tune::setBassMode (int sectionIndex, BassMode mode)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr || s->bass.mode == mode)
        return false;

    s->bass.mode = mode;
    return true;
}

bool Tune::setBassNotes (int sectionIndex, std::vector<MelodyNote> notes)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr)
        return false;

    for (auto& n : notes)
        n = canonicalNote (n);

    if (s->bass.notes == notes && s->bass.mode == BassMode::manual)
        return false;

    s->bass.notes = std::move (notes);
    s->bass.mode = BassMode::manual;
    return true;
}

bool Tune::setLayer (int sectionIndex, const TuneLayer& layer)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr)
        return false;

    auto l = layer;
    l.volume = tunetheory::canonical (juce::jlimit (0.0, 1.0, l.volume));
    l.pan = tunetheory::canonical (juce::jlimit (-1.0, 1.0, l.pan));

    for (auto& n : l.notes)
        n = canonicalNote (n);

    for (auto& existing : s->layers)
    {
        if (existing.type == l.type)
        {
            if (existing == l)
                return false;

            existing = l;
            return true;
        }
    }

    s->layers.push_back (l);
    return true;
}

bool Tune::removeLayer (int sectionIndex, LayerType type)
{
    auto* s = getSection (sectionIndex);

    if (s == nullptr)
        return false;

    const auto before = s->layers.size();
    s->layers.erase (std::remove_if (s->layers.begin(), s->layers.end(),
                                     [type] (const TuneLayer& l) { return l.type == type; }),
                     s->layers.end());
    return s->layers.size() != before;
}

//==============================================================================
bool Tune::setTempo (double bpm)
{
    const double clamped = tunetheory::canonical (juce::jlimit (kMinTempo, kMaxTempo, bpm));

    if (clamped == meta.tempoBpm)
        return false;

    meta.tempoBpm = clamped;
    return true;
}

bool Tune::setTimeSignature (int numerator, int denominator)
{
    const bool validDenominator = denominator == 1 || denominator == 2 || denominator == 4
                                    || denominator == 8 || denominator == 16 || denominator == 32;

    if (! validDenominator || numerator < 1 || numerator > 32)
        return false;

    if (meta.timeSigNumerator == numerator && meta.timeSigDenominator == denominator)
        return false;

    meta.timeSigNumerator = numerator;
    meta.timeSigDenominator = denominator;
    return true;
}

bool Tune::setKey (int tonic, TuneMode mode)
{
    const int pc = tunetheory::wrapPitchClass (tonic);

    if (meta.keyTonic == pc && meta.mode == mode)
        return false;

    meta.keyTonic = pc;
    meta.mode = mode;
    return true;
}

//==============================================================================
int Tune::storeVariation (const juce::String& name)
{
    TuneVariation v;
    v.name = name.trim().isEmpty() ? juce::String ("Variation ") + juce::String ((int) variations.size() + 1)
                                   : name.trim();
    v.arrangement = arrangement;
    variations.push_back (std::move (v));
    return (int) variations.size() - 1;
}

bool Tune::swapWithVariation (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) variations.size()))
        return false;

    std::swap (arrangement, variations[(size_t) index].arrangement);
    return true;
}

bool Tune::removeVariation (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) variations.size()))
        return false;

    variations.erase (variations.begin() + index);
    return true;
}

} // namespace luthier
