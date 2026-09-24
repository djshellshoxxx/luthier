#include "PracticeRoutineSetup.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

//==============================================================================
namespace
{
    bool isNumber (const juce::var& v) noexcept
    {
        return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite ((double) v);
    }

    double number (const juce::DynamicObject& o, const char* key, double fallback)
    {
        const auto& v = o.getProperty (key);
        return isNumber (v) ? (double) v : fallback;
    }

    juce::NamedValueSet unknownFields (const juce::DynamicObject& object, std::initializer_list<const char*> known)
    {
        juce::NamedValueSet extra;

        for (const auto& property : object.getProperties())
        {
            bool isKnown = false;

            for (auto* k : known)
                isKnown = isKnown || property.name.toString() == k;

            if (! isKnown)
                extra.set (property.name, property.value);
        }

        return extra;
    }
}

//==============================================================================
juce::File PracticeDefaults::getDefaultsFile()
{
    return practicefiles::getPracticeDirectory().getChildFile ("defaults.json");
}

bool PracticeDefaults::operator== (const PracticeDefaults& o) const
{
    return metronomeTempo == o.metronomeTempo && timeSigNumerator == o.timeSigNumerator
        && timeSigDenominator == o.timeSigDenominator && subdivision == o.subdivision && sound == o.sound
        && accents == o.accents && loopLengthSeconds == o.loopLengthSeconds
        && loopCountInBars == o.loopCountInBars && overdubMode == o.overdubMode
        && trainerKey == o.trainerKey && scaleSet == o.scaleSet && rangeLowNote == o.rangeLowNote
        && rangeHighNote == o.rangeHighNote && questionCount == o.questionCount
        && backingFolder == o.backingFolder && shuffle == o.shuffle && backingLevelDb == o.backingLevelDb
        && practicefiles::sameExtras (extra, o.extra);
}

juce::StringArray PracticeDefaults::applyTo (const PracticeTargets& targets) const
{
    juce::StringArray problems;

    if (auto* m = targets.metronome)
    {
        m->setTempo (metronomeTempo);
        m->setTimeSignature (timeSigNumerator, timeSigDenominator);
        m->setSubdivision (subdivision);
        m->setSound (sound);
        m->resetAccents();

        for (int beat = 0; beat < juce::jmin ((int) accents.size(), Metronome::kMaxBeatsPerBar); ++beat)
            m->setBeatAccent (beat, accents[(size_t) beat]);
    }
    else
    {
        problems.add ("metronome: none to apply defaults to");
    }

    if (auto* l = targets.looper)
        l->getLayer (l->getActiveLayer()).setMode (overdubMode);

    // MODEL-GAPS (TODO 11): the first recording closes itself at the default
    // length; 0 leaves it to the player. loopCountInBars is the drawer's count-in.
    if (auto* l = targets.looper)
        l->setDefaultLengthSamples ((int) std::round (juce::jmax (0.0, loopLengthSeconds) * l->getSampleRate()));

    if (auto* s = targets.scaleTrainer)
    {
        s->setKey (trainerKey);

        if (! scaleSet.empty())
            s->setScale (scaleSet.front());
    }

    // MODEL-GAPS (TODO 11): the trainers' note range and session length.
    if (auto* s = targets.scaleTrainer)
    {
        s->setNoteRange (rangeLowNote, rangeHighNote);
        s->setQuestionCount (questionCount);
    }

    if (auto* e = targets.earTrainer)
    {
        e->setNoteRange (rangeLowNote, rangeHighNote);
        e->setQuestionCount (questionCount);
    }

    if (auto* b = targets.backingTrack)
        b->setLevelDb (backingLevelDb);

    // backingFolder and shuffle are the TRACK tab's file chooser and playlist
    // order: nothing in BackingTrackPlayer to set.

    return problems;
}

juce::var PracticeDefaults::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", kSchemaVersion);
    root->setProperty ("magic", juce::String (kMagic));

    auto* metronome = new juce::DynamicObject();
    metronome->setProperty ("tempo_bpm", metronomeTempo);
    metronome->setProperty ("time_sig", juce::String (timeSigNumerator) + "/" + juce::String (timeSigDenominator));
    metronome->setProperty ("subdivision", practicekeys::subdivision (subdivision));
    metronome->setProperty ("sound", practicekeys::sound (sound));

    juce::Array<juce::var> accentItems;

    for (auto a : accents)
        accentItems.add (juce::String (practicekeys::accent (a)));

    metronome->setProperty ("accents", juce::var (accentItems));
    root->setProperty ("metronome", juce::var (metronome));

    auto* looper = new juce::DynamicObject();
    looper->setProperty ("length_s", loopLengthSeconds);
    looper->setProperty ("count_in_bars", loopCountInBars);
    looper->setProperty ("layer_mode", practicekeys::layerMode (overdubMode));
    root->setProperty ("looper", juce::var (looper));

    auto* trainers = new juce::DynamicObject();
    trainers->setProperty ("key", trainerKey);

    juce::Array<juce::var> scaleItems;

    for (auto s : scaleSet)
        scaleItems.add (juce::String (practicekeys::scale (s)));

    trainers->setProperty ("scales", juce::var (scaleItems));
    trainers->setProperty ("range_low", rangeLowNote);
    trainers->setProperty ("range_high", rangeHighNote);
    trainers->setProperty ("questions", questionCount);
    root->setProperty ("trainers", juce::var (trainers));

    auto* backing = new juce::DynamicObject();
    backing->setProperty ("folder", backingFolder);
    backing->setProperty ("shuffle", shuffle);
    backing->setProperty ("level_db", backingLevelDb);
    root->setProperty ("backing_track", juce::var (backing));

    for (const auto& field : extra)
        if (! root->hasProperty (field.name))
            root->setProperty (field.name, field.value);

    return juce::var (root);
}

bool PracticeDefaults::fromVar (const juce::var& state, PracticeDefaults& result, juce::String& error)
{
    const auto* root = state.getDynamicObject();

    if (root == nullptr || root->getProperty ("magic").toString() != kMagic)
    {
        error = "not a practice defaults file";
        return false;
    }

    if (root->hasProperty ("schema") && (! isNumber (root->getProperty ("schema"))
                                          || (int) root->getProperty ("schema") > kSchemaVersion))
    {
        error = "schema " + root->getProperty ("schema").toString() + " is not one this version reads";
        return false;
    }

    // Out-of-range or unreadable values fall back to the default for that
    // field: a defaults file is a convenience, and one bad value should not
    // cost the player all the others.
    PracticeDefaults d;

    if (const auto* m = root->getProperty ("metronome").getDynamicObject())
    {
        d.metronomeTempo = juce::jlimit (20.0, 300.0, number (*m, "tempo_bpm", d.metronomeTempo));

        const auto sig = m->getProperty ("time_sig").toString();
        const int n = sig.upToFirstOccurrenceOf ("/", false, false).getIntValue();
        const int den = sig.fromFirstOccurrenceOf ("/", false, false).getIntValue();

        if (n >= 1 && n <= 32 && (den == 2 || den == 4 || den == 8 || den == 16))
        {
            d.timeSigNumerator = n;
            d.timeSigDenominator = den;
        }

        practicekeys::parse (m->getProperty ("subdivision").toString(), d.subdivision);
        practicekeys::parse (m->getProperty ("sound").toString(), d.sound);

        if (const auto* list = m->getProperty ("accents").getArray())
        {
            std::vector<BeatAccent> accents;

            for (const auto& item : *list)
            {
                BeatAccent a = BeatAccent::normal;
                practicekeys::parse (item.toString(), a);
                accents.push_back (a);
            }

            d.accents = accents;
        }
    }

    if (const auto* l = root->getProperty ("looper").getDynamicObject())
    {
        d.loopLengthSeconds = juce::jlimit (0.0, (double) Looper::kMaxLoopSeconds, number (*l, "length_s", 0.0));
        d.loopCountInBars = juce::jlimit (0, 16, (int) number (*l, "count_in_bars", d.loopCountInBars));
        practicekeys::parse (l->getProperty ("layer_mode").toString(), d.overdubMode);
    }

    if (const auto* t = root->getProperty ("trainers").getDynamicObject())
    {
        d.trainerKey = juce::jlimit (0, 11, (int) number (*t, "key", 0.0));
        d.rangeLowNote = juce::jlimit (0, 127, (int) number (*t, "range_low", d.rangeLowNote));
        d.rangeHighNote = juce::jlimit (d.rangeLowNote, 127, (int) number (*t, "range_high", d.rangeHighNote));
        d.questionCount = juce::jlimit (1, 500, (int) number (*t, "questions", d.questionCount));

        if (const auto* list = t->getProperty ("scales").getArray())
        {
            std::vector<ScaleType> scales;

            for (const auto& item : *list)
            {
                ScaleType s;

                if (practicekeys::parse (item.toString(), s))
                    scales.push_back (s);
            }

            d.scaleSet = scales;
        }
    }

    if (const auto* b = root->getProperty ("backing_track").getDynamicObject())
    {
        d.backingFolder = b->getProperty ("folder").toString();
        d.shuffle = (bool) b->getProperty ("shuffle");
        d.backingLevelDb = juce::jlimit (-60.0, 12.0, number (*b, "level_db", d.backingLevelDb));
    }

    d.extra = unknownFields (*root, { "schema", "magic", "metronome", "looper", "trainers", "backing_track" });
    result = d;
    return true;
}

bool PracticeDefaults::save (const juce::File& file, juce::String& error) const
{
    return practicefiles::writeJson (file, toVar(), error);
}

bool PracticeDefaults::load (const juce::File& file, PracticeDefaults& result, juce::String& error)
{
    juce::var parsed;

    if (! practicefiles::readJson (file, parsed, error))
        return false;

    return fromVar (parsed, result, error);
}

//==============================================================================
juce::int64 SessionRecorderSetup::estimateBytes (double minutes, double sampleRate, int channels)
{
    return (juce::int64) std::llround (juce::jmax (0.0, minutes) * 60.0 * juce::jmax (0.0, sampleRate)
                                         * (double) juce::jmax (0, channels) * (double) sizeof (float));
}

juce::String SessionRecorderSetup::describeBytes (juce::int64 bytes)
{
    const double b = (double) juce::jmax ((juce::int64) 0, bytes);

    if (b >= 1.0e9)
        return juce::String (b / 1.0e9, 1) + " GB";

    if (b >= 1.0e6)
        return juce::String ((int) std::lround (b / 1.0e6)) + " MB";

    return juce::String ((int) std::lround (b / 1.0e3)) + " KB";
}

juce::String SessionRecorderSetup::getSizeWarning (double sampleRate) const
{
    const auto size = describeBytes (estimateBytes (ringMinutes, sampleRate));

    return "A " + juce::String ((int) std::lround (ringMinutes)) + "-minute recording buffer takes about "
             + size + " of memory while the session recorder is on. It is off by default, so this memory is "
               "only used once you turn it on.";
}

bool SessionRecorderSetup::applyTo (SessionRecorder& recorder, double sampleRate) const
{
    // MODEL-GAPS (TODO 11): what the take records, and whether stopping saves it
    // (the drawer's SESSION stop calls SessionRecorder::stop).
    recorder.setRecordAudio (recordAudio);
    recorder.setRecordMidi (recordMidi);
    recorder.setAutoSaveOnStop (autoSaveOnStop);
    return recorder.prepare (sampleRate, juce::jlimit (1.0, 240.0, ringMinutes));
}

juce::var SessionRecorderSetup::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("ring_minutes", ringMinutes);
    o->setProperty ("record_audio", recordAudio);
    o->setProperty ("record_midi", recordMidi);
    o->setProperty ("auto_save", autoSaveOnStop);
    return juce::var (o);
}

SessionRecorderSetup SessionRecorderSetup::fromVar (const juce::var& state)
{
    SessionRecorderSetup s;

    if (const auto* o = state.getDynamicObject())
    {
        s.ringMinutes = juce::jlimit (1.0, 240.0, number (*o, "ring_minutes", s.ringMinutes));

        if (o->getProperty ("record_audio").isBool()) s.recordAudio = (bool) o->getProperty ("record_audio");
        if (o->getProperty ("record_midi").isBool())  s.recordMidi = (bool) o->getProperty ("record_midi");
        if (o->getProperty ("auto_save").isBool())    s.autoSaveOnStop = (bool) o->getProperty ("auto_save");

        // Recording neither is not a setting; it is the recorder being off.
        if (! s.recordAudio && ! s.recordMidi)
            s.recordAudio = true;
    }

    return s;
}

//==============================================================================
namespace
{
    juce::int64 folderSize (const juce::File& folder)
    {
        juce::int64 total = 0;

        for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*", juce::File::findFiles))
            total += entry.getFileSize();

        return total;
    }

    void newestFirst (std::vector<PracticeLibrary::Item>& items)
    {
        std::sort (items.begin(), items.end(), [] (const PracticeLibrary::Item& a, const PracticeLibrary::Item& b)
        {
            if (a.modified != b.modified)
                return a.modified > b.modified;

            return a.name < b.name;
        });
    }
}

std::vector<PracticeLibrary::Item> PracticeLibrary::listLoops (const juce::File& directory)
{
    std::vector<Item> items;

    if (! directory.isDirectory())
        return items;

    for (const auto& folder : directory.findChildFiles (juce::File::findDirectories, false))
    {
        const auto json = folder.getChildFile ("loop.json");

        if (! json.existsAsFile())
            continue;

        Item item;
        item.name = folder.getFileName();
        item.file = folder;
        item.modified = json.getLastModificationTime();
        item.sizeBytes = folderSize (folder);
        items.push_back (item);
    }

    newestFirst (items);
    return items;
}

std::vector<PracticeLibrary::Item> PracticeLibrary::listSessions (const juce::File& directory)
{
    std::vector<Item> items;

    if (! directory.isDirectory())
        return items;

    for (const auto& file : directory.findChildFiles (juce::File::findFiles, false, "*.wav"))
    {
        Item item;
        item.name = file.getFileNameWithoutExtension();
        item.file = file;
        item.modified = file.getLastModificationTime();
        item.sizeBytes = file.getSize();
        items.push_back (item);
    }

    newestFirst (items);
    return items;
}

bool PracticeLibrary::deleteLoop (const Item& loop)
{
    if (! loop.file.isDirectory() || ! loop.file.getChildFile ("loop.json").existsAsFile())
        return false;

    return loop.file.deleteRecursively();
}

void PracticeLibrary::noteTabOpened (const juce::File& file)
{
    recentTabs.removeAllInstancesOf (file);
    recentTabs.insert (0, file);

    while (recentTabs.size() > kMaxRecentTabs)
        recentTabs.removeLast();
}

void PracticeLibrary::pruneMissing()
{
    for (int i = recentTabs.size(); --i >= 0;)
        if (! recentTabs.getReference (i).existsAsFile())
            recentTabs.remove (i);
}

juce::var PracticeLibrary::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("schema", 1);
    o->setProperty ("magic", "luthier.practicelibrary");

    juce::Array<juce::var> items;

    for (const auto& f : recentTabs)
        items.add (f.getFullPathName());

    o->setProperty ("recent_tabs", juce::var (items));
    return juce::var (o);
}

void PracticeLibrary::fromVar (const juce::var& state)
{
    recentTabs.clear();

    if (const auto* items = state["recent_tabs"].getArray())
        for (const auto& item : *items)
            if (juce::File::isAbsolutePath (item.toString()) && recentTabs.size() < kMaxRecentTabs)
                recentTabs.add (juce::File (item.toString()));
}

bool PracticeLibrary::save (const juce::File& file, juce::String& error) const
{
    return practicefiles::writeJson (file, toVar(), error);
}

bool PracticeLibrary::load (const juce::File& file)
{
    juce::var parsed;
    juce::String error;

    if (! practicefiles::readJson (file, parsed, error) || parsed["magic"].toString() != "luthier.practicelibrary")
        return false;

    fromVar (parsed);
    return true;
}

juce::File PracticeLibrary::getLibraryFile()
{
    return practicefiles::getPracticeDirectory().getChildFile ("library.json");
}

} // namespace luthier
