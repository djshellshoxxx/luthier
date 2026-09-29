#include "PracticeRoutineTempo.h"
#include "PracticeRoutineProgress.h"

#include <cmath>

namespace luthier
{

//==============================================================================
PracticeCountIn PracticeCountIn::fromMetronome (const Metronome& metronome, int bars)
{
    PracticeCountIn c;
    const auto sig = metronome.getTimeSignature();

    c.bars = juce::jmax (0, bars);
    c.numerator = sig.numerator;
    c.denominator = sig.denominator;
    c.bpm = metronome.getTempo();
    return c;
}

double PracticeCountIn::getBeats() const noexcept
{
    return (double) getCounts() * 4.0 / (double) juce::jmax (1, denominator);
}

double PracticeCountIn::getSecondsPerCount() const noexcept
{
    return 4.0 / (double) juce::jmax (1, denominator) * 60.0 / juce::jmax (1.0, bpm);
}

double PracticeCountIn::getSeconds() const noexcept
{
    return getBeats() * 60.0 / juce::jmax (1.0, bpm);
}

int PracticeCountIn::getCountAt (double elapsedSeconds) const noexcept
{
    if (elapsedSeconds < 0.0 || isFinishedAt (elapsedSeconds) || getCounts() == 0)
        return 0;

    // A hair of tolerance so that exactly on a count reads as that count.
    const int index = (int) std::floor (elapsedSeconds / getSecondsPerCount() + 1.0e-9);
    return (index % juce::jmax (1, numerator)) + 1;
}

//==============================================================================
double PracticeLoopRegion::wrap (double position) const noexcept
{
    if (! enabled || ! isValid() || position < end)
        return position;

    return start + std::fmod (position - start, getLength());
}

int PracticeLoopRegion::countWraps (double from, double to) const noexcept
{
    if (! enabled || ! isValid() || to < end)
        return 0;

    const double firstPass = juce::jmax (from, start);
    return (int) std::floor ((to - start) / getLength()) - (int) std::floor ((firstPass - start) / getLength());
}

PracticeLoopRegion PracticeLoopRegion::betweenMarkers (const BackingTrackPlayer& player, int fromMarker, int toMarker)
{
    PracticeLoopRegion region;

    if (! juce::isPositiveAndBelow (fromMarker, player.getNumMarkers()))
        return region;

    region.start = player.getMarker (fromMarker).seconds;

    if (toMarker < 0)
        region.end = player.getLengthSeconds();
    else if (juce::isPositiveAndBelow (toMarker, player.getNumMarkers()))
        region.end = player.getMarker (toMarker).seconds;

    region.enabled = region.isValid();
    return region;
}

void PracticeLoopRegion::applyTo (BackingTrackPlayer& player) const
{
    if (isValid())
        player.setLoopSeconds (start, end);

    player.setLoopEnabled (enabled && isValid());
}

juce::var PracticeLoopRegion::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("start", start);
    o->setProperty ("end", end);
    o->setProperty ("enabled", enabled);
    return juce::var (o);
}

PracticeLoopRegion PracticeLoopRegion::fromVar (const juce::var& state)
{
    PracticeLoopRegion r;

    if (auto* o = state.getDynamicObject())
    {
        r.start = (double) o->getProperty ("start");
        r.end = (double) o->getProperty ("end");
        r.enabled = (bool) o->getProperty ("enabled");

        if (! std::isfinite (r.start) || ! std::isfinite (r.end))
            r = {};
    }

    return r;
}

//==============================================================================
juce::var SpeedTrainer::Settings::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("phrase", phrase);
    o->setProperty ("start_bpm", startBpm);
    o->setProperty ("step_percent", stepPercent);
    o->setProperty ("max_bpm", maxBpm);
    o->setProperty ("allowed_misses", allowedMisses);
    o->setProperty ("clean_passes_per_step", cleanPassesPerStep);
    return juce::var (o);
}

SpeedTrainer::Settings SpeedTrainer::Settings::fromVar (const juce::var& state)
{
    Settings s;

    if (auto* o = state.getDynamicObject())
    {
        auto number = [o] (const char* key, double fallback)
        {
            const auto& v = o->getProperty (key);
            return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite ((double) v) ? (double) v : fallback;
        };

        s.phrase = o->getProperty ("phrase").toString();
        s.startBpm = juce::jlimit (20.0, 300.0, number ("start_bpm", s.startBpm));
        s.stepPercent = juce::jlimit (0.5, 50.0, number ("step_percent", s.stepPercent));
        s.maxBpm = juce::jlimit (20.0, 300.0, number ("max_bpm", s.maxBpm));
        s.allowedMisses = juce::jlimit (0, 1000, (int) number ("allowed_misses", s.allowedMisses));
        s.cleanPassesPerStep = juce::jlimit (1, 100, (int) number ("clean_passes_per_step", s.cleanPassesPerStep));
    }

    return s;
}

void SpeedTrainer::start (const Settings& newSettings)
{
    settings = newSettings;
    settings.startBpm = juce::jlimit (20.0, 300.0, settings.startBpm);
    settings.maxBpm = juce::jlimit (settings.startBpm, 300.0, settings.maxBpm);
    settings.stepPercent = juce::jlimit (0.5, 50.0, settings.stepPercent);
    settings.allowedMisses = juce::jmax (0, settings.allowedMisses);
    settings.cleanPassesPerStep = juce::jmax (1, settings.cleanPassesPerStep);

    tempo = settings.startBpm;
    bestClean = 0.0;
    pass = 0;
    cleanAtTempo = 0;
    running = true;
    finished = false;
}

double SpeedTrainer::passCompleted (int misses)
{
    if (! running || finished)
        return tempo;

    ++pass;

    if (misses > settings.allowedMisses)
    {
        running = false;
        finished = true;
        return bestClean > 0.0 ? bestClean : tempo;
    }

    bestClean = juce::jmax (bestClean, tempo);

    if (++cleanAtTempo < settings.cleanPassesPerStep)
        return tempo;

    if (tempo >= settings.maxBpm - 1.0e-9)
    {
        running = false;
        finished = true;
        return tempo;
    }

    // Two decimals: a tempo readout of 88.2 is useful, 88.19999999 is not.
    tempo = juce::jmin (settings.maxBpm, std::round (tempo * (1.0 + settings.stepPercent / 100.0) * 100.0) / 100.0);
    cleanAtTempo = 0;
    return tempo;
}

void SpeedTrainer::applyTo (Metronome& metronome) const
{
    metronome.setFollowsTempo (false);   // SPEC-SWEEP PT-6: the trainer's tempo stays
    metronome.setTempo (tempo);
}

double SpeedTrainer::getTempoRatioFor (double trackBpm) const noexcept
{
    if (trackBpm <= 0.0)
        return 1.0;

    return juce::jlimit (0.25, 2.0, tempo / trackBpm);
}

void SpeedTrainer::recordResult (PracticeStats& stats, const juce::String& date) const
{
    if (bestClean > 0.0)
        stats.recordCleanTempo (date, settings.phrase, bestClean);
}

} // namespace luthier
