#include "TuneSession.h"
#include "TuneTemplates.h"

namespace luthier
{

TuneSession::TuneSession()
    : tune (TuneTemplateLibrary::instantiate (TuneTemplateLibrary::createBlank())),
      clock ([] { return juce::Time::getMillisecondCounterHiRes(); })
{
    openNotes.fill ({ -1.0, 0 });

    // An untouched session tune carries no timestamps: two fresh instances
    // must save the same state (host-integration 3). saveAs() stamps it.
    tune.meta.created.clear();
    tune.meta.modified.clear();
}

//==============================================================================
bool TuneSession::edit (TuneEditClass editClass, const juce::String& description,
                        const std::function<bool (Tune&)>& change, int target)
{
    if (change == nullptr)
        return false;

    auto before = tune;

    if (! change (tune))
        return false;

    // Nothing changed after all (a method that reported a change it undid).
    if (tune == before)
        return false;

    const double now = clock != nullptr ? clock() : 0.0;

    // action-and-undo 0.3: the same class on the same target within 200 ms is
    // one entry, whose "before" is the state before the first of them.
    const bool groups = ! undoStack.empty() && target >= 0
                          && undoStack.back().editClass == editClass
                          && undoStack.back().target == target
                          && now - undoStack.back().timeMs <= kGroupWindowMs;

    if (groups)
    {
        undoStack.back().timeMs = now;
    }
    else
    {
        UndoEntry entry;
        entry.before = std::move (before);
        entry.description = description;
        entry.editClass = editClass;
        entry.target = target;
        entry.timeMs = now;
        undoStack.push_back (std::move (entry));

        if ((int) undoStack.size() > kMaxUndo)
            undoStack.erase (undoStack.begin());
    }

    redoStack.clear();
    dirty = true;
    changed();
    return true;
}

bool TuneSession::undo()
{
    if (undoStack.empty())
        return false;

    auto entry = std::move (undoStack.back());
    undoStack.pop_back();

    UndoEntry redoEntry;
    redoEntry.before = tune;
    redoEntry.description = entry.description;
    redoEntry.editClass = entry.editClass;
    redoStack.push_back (std::move (redoEntry));

    tune = std::move (entry.before);
    dirty = true;
    changed();
    return true;
}

bool TuneSession::redo()
{
    if (redoStack.empty())
        return false;

    auto entry = std::move (redoStack.back());
    redoStack.pop_back();

    UndoEntry undoEntry;
    undoEntry.before = tune;
    undoEntry.description = entry.description;
    undoEntry.editClass = entry.editClass;
    undoStack.push_back (std::move (undoEntry));

    tune = std::move (entry.before);
    dirty = true;
    changed();
    return true;
}

juce::String TuneSession::getUndoDescription() const
{
    return undoStack.empty() ? juce::String() : undoStack.back().description;
}

juce::String TuneSession::getRedoDescription() const
{
    return redoStack.empty() ? juce::String() : redoStack.back().description;
}

//==============================================================================
void TuneSession::newTune (const Tune& from)
{
    tune = from;
    file = juce::File();
    dirty = false;
    selectedSection = 0;
    undoStack.clear();
    redoStack.clear();
    cancelRecording();
    changed();
}

bool TuneSession::load (const juce::File& source, juce::String& error)
{
    Tune loaded;
    const auto result = TuneFile::load (source, loaded);

    if (! result.ok())
    {
        error = juce::String (getTuneLoadErrorName (result.error)) + ": " + result.message;
        return false;
    }

    newTune (loaded);
    file = source;
    return true;
}

bool TuneSession::save (juce::String& error)
{
    if (file == juce::File())
    {
        error = "The tune has no file yet; use Save As.";
        return false;
    }

    return saveAs (file, error);
}

bool TuneSession::saveAs (const juce::File& destination, juce::String& error)
{
    auto toSave = tune;
    toSave.meta.modified = juce::Time::getCurrentTime().toISO8601 (true);

    if (toSave.meta.created.isEmpty())
        toSave.meta.created = toSave.meta.modified;

    if (! TuneFile::save (toSave, destination, error))
        return false;

    // The stamps are metadata, not an edit: no undo entry, and no replay.
    tune.meta.created = toSave.meta.created;
    tune.meta.modified = toSave.meta.modified;
    file = destination;
    dirty = false;

    if (onChanged != nullptr)
        onChanged();

    return true;
}

juce::String TuneSession::getDisplayTitle() const
{
    const auto title = tune.meta.title.isNotEmpty() ? tune.meta.title : juce::String ("Untitled Tune");
    return dirty ? title + " *" : title;
}

juce::var TuneSession::toState() const
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("tune", TuneFile::toVar (tune));
    object->setProperty ("file", file.getFullPathName());
    object->setProperty ("dirty", dirty);
    return juce::var (object);
}

bool TuneSession::restoreState (const juce::var& state)
{
    Tune restored;

    if (! TuneFile::fromVar (state.getProperty ("tune", {}), restored).ok())
        return false;

    // A restored session is a state boundary like a load (action-and-undo 3.9).
    newTune (restored);

    const auto path = state.getProperty ("file", {}).toString();
    file = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
    dirty = (bool) state.getProperty ("dirty", false);

    if (onChanged != nullptr)
        onChanged();

    return true;
}

void TuneSession::setSelectedSection (int index)
{
    const int clamped = tune.getNumSections() > 0 ? juce::jlimit (0, tune.getNumSections() - 1, index) : 0;

    if (clamped != selectedSection)
    {
        selectedSection = clamped;

        if (onChanged != nullptr)
            onChanged();
    }
}

//==============================================================================
void TuneSession::attachPlayer (TunePlayer* newPlayer)
{
    if (player != nullptr && player != newPlayer)
        player->setRecordArmed (false);

    player = newPlayer;
    rebuildTimeline();
}

void TuneSession::attachRhythm (RhythmEngine* engine, const GenreKitLibrary* kits, const PatternLibrary* patterns)
{
    rhythmEngine = engine;
    genreKits = kits;
    patternLibrary = patterns;
}

void TuneSession::rebuildTimeline()
{
    if (player == nullptr)
        return;

    auto options = midiOptions;
    options.improvisePass = juce::jmax (0, player->getPass());

    player->setTimeline (std::make_unique<TuneTimeline> (TuneTimeline::build (tune, options)),
                         tune.meta.tempoBpm, tune.getBeatsPerBar());
}

void TuneSession::service()
{
    if (player == nullptr)
        return;

    // 4.4: the next pass of an improvised tune, built before the loop gets there.
    const int nextPass = player->getPassNeedingTimeline();

    if (nextPass >= 0)
    {
        auto options = midiOptions;
        options.improvisePass = nextPass;
        player->setNextPassTimeline (std::make_unique<TuneTimeline> (TuneTimeline::build (tune, options)), nextPass);
    }

    player->collectGarbage();

    // 3.5 and 8: the section that just started brings its rhythm with it.
    if (rhythmEngine != nullptr && genreKits != nullptr && patternLibrary != nullptr)
    {
        TuneRhythmChange change;

        while (player->takePendingRhythmChange (change))
        {
            // 14 (TUNE-HELP-ONBOARDING): the Tune Feel modulation rides on the section's feel.
            lastRhythmChange = change;
            hasLastRhythmChange = true;
            change.feel = juce::jlimit (0.0, 1.0, change.feel + feelOffset);
            applyRhythmChange (change, *rhythmEngine, *genreKits, *patternLibrary);
            appliedFeelOffset = feelOffset;
        }

        // A modulated feel moves within a section too: only the humanise amount
        // is re-applied, so the pattern and its phase are left alone.
        if (hasLastRhythmChange && std::abs (feelOffset - appliedFeelOffset) > 0.005)
        {
            const int kitIndex = genreKits->indexOf (lastRhythmChange.genreKitId);
            auto humanise = kitIndex >= 0 ? genreKits->getKit (kitIndex).humanise : rhythmEngine->getHumanise();
            const double feel = juce::jlimit (0.0, 1.0, lastRhythmChange.feel + feelOffset);
            humanise.amount = juce::jlimit (0.0, 2.0, humanise.amount * feel * 2.0);
            rhythmEngine->setHumanise (humanise);
            appliedFeelOffset = feelOffset;
        }
    }

    if (recording)
        drainRecording();
}

void TuneSession::changed()
{
    if (tune.getNumSections() > 0)
        selectedSection = juce::jlimit (0, tune.getNumSections() - 1, selectedSection);
    else
        selectedSection = 0;

    rebuildTimeline();

    if (onChanged != nullptr)
        onChanged();
}

//==============================================================================
void TuneSession::applyRhythmChange (const TuneRhythmChange& change, RhythmEngine& engine,
                                     const GenreKitLibrary& kits, const PatternLibrary& patterns)
{
    const int kitIndex = kits.indexOf (change.genreKitId);
    GenreKit kit;

    if (kitIndex >= 0)
    {
        kit = kits.getKit (kitIndex);
        GenreKitLibrary::apply (kit, engine, patterns);
    }

    const int patternIndex = patterns.indexOf (change.patternId);

    if (patternIndex >= 0)
        engine.setPattern (patterns.getPattern (patternIndex));

    // Feel and strum are 0..1 with the kit as written at 0.5.
    auto humanise = kitIndex >= 0 ? kit.humanise : engine.getHumanise();
    humanise.amount = juce::jlimit (0.0, 2.0, humanise.amount * change.feel * 2.0);
    engine.setHumanise (humanise);

    const double baseStrum = kitIndex >= 0 ? kit.strumDurationMs : 22.0;
    engine.setStrumDurationMs (baseStrum * (0.5 + change.strum));

    engine.setEnabled (change.rhythmOn);
}

//==============================================================================
void TuneSession::beginRecording (int sectionIndex)
{
    recording = tune.isValidSection (sectionIndex);
    recordingSection = sectionIndex;
    recorded.clear();
    openNotes.fill ({ -1.0, 0 });
    recordingSpans = tune.getPlayOrder();

    if (player != nullptr)
    {
        // Whatever was played before the take is not part of it.
        std::array<TunePlayer::RecordedEvent, 64> stale;

        while (player->popRecordedEvents (stale.data(), (int) stale.size()) > 0) {}

        player->setRecordArmed (recording);
    }
}

void TuneSession::recordNote (int pitch, int velocity, double startBeat, double endBeat)
{
    if (! recording)
        return;

    RecordedNote n;
    n.pitch = juce::jlimit (0, 127, pitch);
    n.velocity = juce::jlimit (1, 127, velocity);
    n.startBeat = startBeat;
    n.endBeat = juce::jmax (startBeat, endBeat);
    recorded.push_back (n);
}

void TuneSession::drainRecording()
{
    if (player == nullptr)
        return;

    const double sectionBeats = tune.getSectionLengthBeats (recordingSection);
    std::array<TunePlayer::RecordedEvent, 128> events;

    for (;;)
    {
        const int count = player->popRecordedEvents (events.data(), (int) events.size());

        for (int i = 0; i < count; ++i)
        {
            const auto& e = events[(size_t) i];

            if (! juce::isPositiveAndBelow (e.span, (int) recordingSpans.size()) || ! juce::isPositiveAndBelow (e.note, 128))
                continue;

            const auto& span = recordingSpans[(size_t) e.span];
            const bool inSection = span.sectionIndex == recordingSection;
            const double beat = e.ppq - span.startBeat;
            auto& open = openNotes[(size_t) e.note];

            if (e.isNoteOn)
            {
                // Only notes started while the section plays; every occurrence
                // of it folds onto the one section.
                if (inSection)
                    open = { beat, e.velocity };

                continue;
            }

            if (open.first < 0.0)
                continue;

            // A note held past the section's end ends with it.
            const double end = inSection && beat >= open.first ? beat : sectionBeats;
            recordNote (e.note, open.second, open.first, end);
            open.first = -1.0;
        }

        if (count < (int) events.size())
            break;
    }
}

bool TuneSession::finishRecording (QuantiseGrid grid, bool followChordChanges, bool snapToKey)
{
    if (! recording)
        return false;

    drainRecording();

    recording = false;

    if (player != nullptr)
        player->setRecordArmed (false);

    // Notes still held when the take ends are left out: there is no end to quantise.
    openNotes.fill ({ -1.0, 0 });

    if (recorded.empty())
        return false;

    const auto notes = quantiseRecording (recorded, grid, tune, recordingSection, followChordChanges, snapToKey);
    const int section = recordingSection;
    recorded.clear();

    // A take keeps the section's locked notes and adds the played ones
    // (tune-builder 0.3: nothing destructive). action-and-undo 3.9 names it.
    return edit (TuneEditClass::melodyRecord, "Record melody (Record)", [&notes, section] (Tune& t)
    {
        std::vector<MelodyNote> merged;

        if (const auto* s = t.getSection (section))
            if (s->melody.has_value())
                for (const auto& n : s->melody->notes)
                    if (n.locked)
                        merged.push_back (n);

        merged.insert (merged.end(), notes.begin(), notes.end());
        return t.setMelodyNotes (section, merged, MelodySource::record);
    });
}

void TuneSession::cancelRecording()
{
    recording = false;
    recorded.clear();
    openNotes.fill ({ -1.0, 0 });

    if (player != nullptr)
        player->setRecordArmed (false);
}

} // namespace luthier
