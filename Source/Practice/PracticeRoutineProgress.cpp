#include "PracticeRoutineProgress.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

//==============================================================================
namespace
{
    constexpr int kNumTools = (int) PracticeTool::numTools;

    /** Parses "yyyy-mm-dd" to noon on that day. Noon, so that stepping whole
        days back across a daylight-saving change cannot land on the wrong date. */
    bool parseDate (const juce::String& date, juce::Time& result)
    {
        if (date.length() != 10 || date[4] != '-' || date[7] != '-')
            return false;

        const int year = date.substring (0, 4).getIntValue();
        const int month = date.substring (5, 7).getIntValue();
        const int day = date.substring (8, 10).getIntValue();

        if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31)
            return false;

        result = juce::Time (year, month - 1, day, 12, 0, 0, 0, true);
        return true;
    }

    bool isNumber (const juce::var& v) noexcept
    {
        return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite ((double) v);
    }

    juce::String minutes (double seconds)
    {
        return juce::String (seconds / 60.0, 1);
    }
}

//==============================================================================
double PracticeDayRecord::getTotalSeconds() const noexcept
{
    double total = 0.0;

    for (double s : seconds)
        total += s;

    return total;
}

int PracticeDayRecord::getTotalSessions() const noexcept
{
    int total = 0;

    for (int s : sessions)
        total += s;

    return total;
}

bool PracticeDayRecord::isEmpty() const noexcept
{
    return getTotalSeconds() <= 0.0 && getTotalSessions() == 0 && accuracy.empty();
}

bool PracticeDayRecord::operator== (const PracticeDayRecord& o) const
{
    return date == o.date && seconds == o.seconds && sessions == o.sessions && accuracy == o.accuracy;
}

bool PhraseTempoRecord::operator== (const PhraseTempoRecord& o) const
{
    return phrase == o.phrase && bestBpm == o.bestBpm && history == o.history;
}

//==============================================================================
juce::String PracticeStats::dateKey (juce::Time when)
{
    return when.formatted ("%Y-%m-%d");
}

juce::String PracticeStats::today()
{
    return dateKey (juce::Time::getCurrentTime());
}

juce::String PracticeStats::daysBefore (const juce::String& date, int numDays)
{
    juce::Time t;

    if (! parseDate (date, t))
        return date;

    return dateKey (t - juce::RelativeTime::days ((double) numDays));
}

//==============================================================================
PracticeDayRecord& PracticeStats::dayFor (const juce::String& date)
{
    auto it = std::lower_bound (days.begin(), days.end(), date,
                                [] (const PracticeDayRecord& d, const juce::String& key) { return d.date < key; });

    if (it != days.end() && it->date == date)
        return *it;

    PracticeDayRecord fresh;
    fresh.date = date;
    return *days.insert (it, fresh);
}

const PracticeDayRecord* PracticeStats::findDay (const juce::String& date) const
{
    auto it = std::lower_bound (days.begin(), days.end(), date,
                                [] (const PracticeDayRecord& d, const juce::String& key) { return d.date < key; });

    return (it != days.end() && it->date == date) ? &*it : nullptr;
}

void PracticeStats::addSeconds (const juce::String& date, PracticeTool tool, double seconds)
{
    if (seconds <= 0.0 || ! std::isfinite (seconds) || ! juce::isPositiveAndBelow ((int) tool, kNumTools))
        return;

    dayFor (date).seconds[(size_t) tool] += seconds;
}

void PracticeStats::addTrainerSession (const juce::String& date, PracticeTool tool, const juce::String& exercise,
                                       int asked, int correct)
{
    if (asked <= 0 || ! juce::isPositiveAndBelow ((int) tool, kNumTools))
        return;

    auto& day = dayFor (date);
    day.sessions[(size_t) tool] += 1;

    auto& score = day.accuracy[exercise];
    score.first += asked;
    score.second += juce::jlimit (0, asked, correct);
}

void PracticeStats::recordCleanTempo (const juce::String& date, const juce::String& phrase, double bpm)
{
    if (bpm <= 0.0 || ! std::isfinite (bpm) || phrase.isEmpty())
        return;

    for (auto& p : phrases)
    {
        if (p.phrase == phrase)
        {
            p.bestBpm = juce::jmax (p.bestBpm, bpm);
            p.history.emplace_back (date, bpm);
            return;
        }
    }

    PhraseTempoRecord record;
    record.phrase = phrase;
    record.bestBpm = bpm;
    record.history.emplace_back (date, bpm);
    phrases.push_back (record);
}

//==============================================================================
double PracticeStats::getSeconds (const juce::String& date) const
{
    const auto* d = findDay (date);
    return d != nullptr ? d->getTotalSeconds() : 0.0;
}

double PracticeStats::getSeconds (const juce::String& date, PracticeTool tool) const
{
    const auto* d = findDay (date);
    return (d != nullptr && juce::isPositiveAndBelow ((int) tool, kNumTools)) ? d->seconds[(size_t) tool] : 0.0;
}

int PracticeStats::getSessions (const juce::String& date, PracticeTool tool) const
{
    const auto* d = findDay (date);
    return (d != nullptr && juce::isPositiveAndBelow ((int) tool, kNumTools)) ? d->sessions[(size_t) tool] : 0;
}

std::vector<PracticeDayRecord> PracticeStats::getRecentDays (const juce::String& lastDate, int numDays) const
{
    std::vector<PracticeDayRecord> result;

    for (int back = juce::jmax (1, numDays) - 1; back >= 0; --back)
    {
        const auto date = daysBefore (lastDate, back);

        if (const auto* d = findDay (date))
        {
            result.push_back (*d);
        }
        else
        {
            PracticeDayRecord empty;
            empty.date = date;
            result.push_back (empty);
        }
    }

    return result;
}

std::array<double, (size_t) PracticeTool::numTools> PracticeStats::getToolTotals (const juce::String& lastDate,
                                                                                   int numDays) const
{
    std::array<double, (size_t) PracticeTool::numTools> totals {};

    for (const auto& d : getRecentDays (lastDate, numDays))
        for (size_t t = 0; t < totals.size(); ++t)
            totals[t] += d.seconds[t];

    return totals;
}

std::vector<std::pair<juce::String, double>> PracticeStats::getAccuracyHistory (const juce::String& exercise) const
{
    std::vector<std::pair<juce::String, double>> result;

    for (const auto& d : days)
    {
        auto it = d.accuracy.find (exercise);

        if (it != d.accuracy.end() && it->second.first > 0)
            result.emplace_back (d.date, (double) it->second.second / (double) it->second.first);
    }

    return result;
}

juce::StringArray PracticeStats::getExercises() const
{
    juce::StringArray names;

    for (const auto& d : days)
        for (const auto& a : d.accuracy)
            names.addIfNotAlreadyThere (a.first);

    names.sort (false);
    return names;
}

double PracticeStats::getBestCleanTempo (const juce::String& phrase) const
{
    for (const auto& p : phrases)
        if (p.phrase == phrase)
            return p.bestBpm;

    return 0.0;
}

int PracticeStats::getStreak (const juce::String& todayDate) const
{
    auto practised = [this] (const juce::String& date)
    {
        const auto* d = findDay (date);
        return d != nullptr && ! d->isEmpty();
    };

    int back = practised (todayDate) ? 0 : 1;
    int streak = 0;

    while (practised (daysBefore (todayDate, back)) && streak < 100000)
    {
        ++streak;
        ++back;
    }

    return streak;
}

void PracticeStats::clear()
{
    days.clear();
    phrases.clear();
}

//==============================================================================
juce::String PracticeStats::toCsv() const
{
    juce::StringArray header { "date", "total_minutes" };

    for (int t = 0; t < kNumTools; ++t)
        header.add (juce::String (getPracticeToolKey ((PracticeTool) t)) + "_minutes");

    header.add ("trainer_sessions");

    juce::StringArray lines;
    lines.add (header.joinIntoString (","));

    for (const auto& d : days)
    {
        if (d.isEmpty())
            continue;

        juce::StringArray row;
        row.add (d.date);
        row.add (minutes (d.getTotalSeconds()));

        for (int t = 0; t < kNumTools; ++t)
            row.add (minutes (d.seconds[(size_t) t]));

        row.add (juce::String (d.getTotalSessions()));
        lines.add (row.joinIntoString (","));
    }

    return lines.joinIntoString ("\n") + "\n";
}

juce::var PracticeStats::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", kSchemaVersion);

    juce::Array<juce::var> dayItems;

    for (const auto& d : days)
    {
        if (d.isEmpty())
            continue;

        auto* o = new juce::DynamicObject();
        o->setProperty ("date", d.date);

        auto* seconds = new juce::DynamicObject();
        auto* sessions = new juce::DynamicObject();

        for (int t = 0; t < kNumTools; ++t)
        {
            if (d.seconds[(size_t) t] > 0.0)
                seconds->setProperty (getPracticeToolKey ((PracticeTool) t), d.seconds[(size_t) t]);

            if (d.sessions[(size_t) t] > 0)
                sessions->setProperty (getPracticeToolKey ((PracticeTool) t), d.sessions[(size_t) t]);
        }

        o->setProperty ("seconds", juce::var (seconds));
        o->setProperty ("sessions", juce::var (sessions));

        auto* accuracy = new juce::DynamicObject();

        for (const auto& a : d.accuracy)
        {
            auto* score = new juce::DynamicObject();
            score->setProperty ("asked", a.second.first);
            score->setProperty ("correct", a.second.second);
            accuracy->setProperty (a.first, juce::var (score));
        }

        o->setProperty ("accuracy", juce::var (accuracy));
        dayItems.add (juce::var (o));
    }

    root->setProperty ("days", juce::var (dayItems));

    juce::Array<juce::var> phraseItems;

    for (const auto& p : phrases)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("phrase", p.phrase);
        o->setProperty ("best_bpm", p.bestBpm);

        juce::Array<juce::var> history;

        for (const auto& h : p.history)
        {
            auto* entry = new juce::DynamicObject();
            entry->setProperty ("date", h.first);
            entry->setProperty ("bpm", h.second);
            history.add (juce::var (entry));
        }

        o->setProperty ("history", juce::var (history));
        phraseItems.add (juce::var (o));
    }

    root->setProperty ("tempo_progress", juce::var (phraseItems));
    return juce::var (root);
}

bool PracticeStats::fromVar (const juce::var& history)
{
    const auto* root = history.getDynamicObject();

    if (root == nullptr)
        return false;

    std::vector<PracticeDayRecord> loadedDays;
    std::vector<PhraseTempoRecord> loadedPhrases;

    if (const auto* items = root->getProperty ("days").getArray())
    {
        for (const auto& item : *items)
        {
            const auto* o = item.getDynamicObject();
            juce::Time unused;

            // A day that cannot be read is skipped rather than failing the
            // whole history: losing one day beats losing ninety.
            if (o == nullptr || ! parseDate (o->getProperty ("date").toString(), unused))
                continue;

            PracticeDayRecord d;
            d.date = o->getProperty ("date").toString();

            for (int t = 0; t < kNumTools; ++t)
            {
                const auto* key = getPracticeToolKey ((PracticeTool) t);

                if (const auto* s = o->getProperty ("seconds").getDynamicObject())
                    if (isNumber (s->getProperty (key)))
                        d.seconds[(size_t) t] = juce::jmax (0.0, (double) s->getProperty (key));

                if (const auto* s = o->getProperty ("sessions").getDynamicObject())
                    if (isNumber (s->getProperty (key)))
                        d.sessions[(size_t) t] = juce::jmax (0, (int) s->getProperty (key));
            }

            if (const auto* a = o->getProperty ("accuracy").getDynamicObject())
            {
                for (const auto& property : a->getProperties())
                {
                    if (const auto* score = property.value.getDynamicObject())
                    {
                        const int asked = juce::jmax (0, (int) score->getProperty ("asked"));
                        const int correct = juce::jlimit (0, asked, (int) score->getProperty ("correct"));
                        d.accuracy[property.name.toString()] = { asked, correct };
                    }
                }
            }

            loadedDays.push_back (d);
        }
    }

    std::sort (loadedDays.begin(), loadedDays.end(),
               [] (const PracticeDayRecord& a, const PracticeDayRecord& b) { return a.date < b.date; });

    if (const auto* items = root->getProperty ("tempo_progress").getArray())
    {
        for (const auto& item : *items)
        {
            const auto* o = item.getDynamicObject();

            if (o == nullptr || o->getProperty ("phrase").toString().isEmpty())
                continue;

            PhraseTempoRecord p;
            p.phrase = o->getProperty ("phrase").toString();
            p.bestBpm = isNumber (o->getProperty ("best_bpm")) ? (double) o->getProperty ("best_bpm") : 0.0;

            if (const auto* h = o->getProperty ("history").getArray())
                for (const auto& entry : *h)
                    if (isNumber (entry["bpm"]))
                        p.history.emplace_back (entry["date"].toString(), (double) entry["bpm"]);

            loadedPhrases.push_back (p);
        }
    }

    days = std::move (loadedDays);
    phrases = std::move (loadedPhrases);
    return true;
}

//==============================================================================
juce::File PracticeStats::getStatsFile()
{
    return EarTrainer::getStatsFile();
}

bool PracticeStats::load (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        clear();
        return true;
    }

    juce::var parsed;
    juce::String error;

    if (! practicefiles::readJson (file, parsed, error) || parsed.getDynamicObject() == nullptr)
        return false;

    // A stats.json written before this history existed (the ear trainer's
    // alone) is an empty history, not an error.
    if (! parsed.getDynamicObject()->hasProperty (kHistoryKey))
    {
        clear();
        return true;
    }

    return fromVar (parsed.getDynamicObject()->getProperty (kHistoryKey));
}

bool PracticeStats::save (const juce::File& file, juce::String& error) const
{
    // Read what is there now, so the ear trainer's keys are written back as
    // they are. A file that does not parse is not overwritten: file-formats 14
    // never silently replaces a corrupt file.
    juce::var root;

    if (file.existsAsFile())
    {
        if (! practicefiles::readJson (file, root, error) || root.getDynamicObject() == nullptr)
        {
            error = file.getFileName() + ": not a stats file this version can update; left as it is";
            return false;
        }
    }
    else
    {
        root = juce::var (new juce::DynamicObject());
    }

    root.getDynamicObject()->setProperty (kHistoryKey, toVar());
    return practicefiles::writeJson (file, root, error);
}

bool PracticeStats::save() const
{
    juce::String error;
    return save (getStatsFile(), error);
}

//==============================================================================
void PracticeActivityTracker::update (const PracticeTargets& targets, bool practicePanelOpen, double elapsedSeconds,
                                      PracticeStats& stats, const juce::String& date) const
{
    if (! practicePanelOpen || elapsedSeconds <= 0.0)
        return;

    if (targets.metronome != nullptr && targets.metronome->isEnabled())
        stats.addSeconds (date, PracticeTool::metronome, elapsedSeconds);

    if (targets.looper != nullptr && targets.looper->getState() != Looper::State::stopped)
        stats.addSeconds (date, PracticeTool::looper, elapsedSeconds);

    if (targets.backingTrack != nullptr && targets.backingTrack->isPlaying())
        stats.addSeconds (date, PracticeTool::backingTrack, elapsedSeconds);
}

juce::String PracticeActivityTracker::exerciseKey (const ScaleTrainer& trainer)
{
    return juce::String ("scale.") + practicekeys::scale (trainer.getScale())
             + "." + practicekeys::scaleMode (trainer.getMode());
}

juce::String PracticeActivityTracker::exerciseKey (const EarTrainer& trainer)
{
    return juce::String ("ear.") + practicekeys::earExercise (trainer.getExercise());
}

void PracticeActivityTracker::recordScaleTrainer (const ScaleTrainer& trainer, PracticeStats& stats,
                                                  const juce::String& date)
{
    // A reset score starts the count again.
    if (trainer.getAsked() < scaleAskedSeen)
        scaleAskedSeen = scaleCorrectSeen = 0;

    const int asked = trainer.getAsked() - scaleAskedSeen;
    const int correct = trainer.getScore() - scaleCorrectSeen;

    if (asked > 0)
        stats.addTrainerSession (date, PracticeTool::scaleTrainer, exerciseKey (trainer), asked, correct);

    scaleAskedSeen = trainer.getAsked();
    scaleCorrectSeen = trainer.getScore();
}

void PracticeActivityTracker::recordEarTrainer (const EarTrainer& trainer, PracticeStats& stats,
                                                const juce::String& date)
{
    if (trainer.getAsked() < earAskedSeen)
        earAskedSeen = earCorrectSeen = 0;

    const int asked = trainer.getAsked() - earAskedSeen;
    const int correct = trainer.getCorrect() - earCorrectSeen;

    if (asked > 0)
        stats.addTrainerSession (date, PracticeTool::earTrainer, exerciseKey (trainer), asked, correct);

    earAskedSeen = trainer.getAsked();
    earCorrectSeen = trainer.getCorrect();
}

} // namespace luthier
