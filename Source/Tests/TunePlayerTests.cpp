/*  Tune playback on the audio thread (tune-builder.md 2, 3.6, 6, 8): events
    on their samples across block boundaries and loop wraps, no note left
    hanging, edits that wait for the bar line, the host's transport against
    the internal clock, count-in and metronome clicks, where the bass goes,
    and no allocation while it plays. */

#include "TestFramework.h"

#include "../Tune/TunePlayer.h"

#include <map>
#include <set>

#if defined (LUTHIER_ALLOCATION_COUNTER)
namespace luthier::tests
{
    /** Global operator new calls on this thread (CircuitTests.cpp replaces the
        operators; it defines this). */
    long allocationsOnThisThread() noexcept;
}
#endif

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr juce::int64 kBeat = 24000;   // samples a beat at 120 bpm and 48 kHz

    ChordCell chord (const char* symbol, double beats = 4.0)
    {
        ChordCell c;
        ProgressionError error = ProgressionError::none;
        parseChordSymbol (symbol, c, error);
        c.durationBeats = beats;
        return c;
    }

    /** Verse (2 bars: Am, F, with two melody notes) x2, then Chorus (G), at
        120 bpm: 20 beats in all. */
    Tune makeTune (BassMode bass = BassMode::off)
    {
        Tune t;
        t.meta.tempoBpm = 120.0;

        TuneSection verse;
        verse.name = "Verse";
        verse.lengthBars = 2;
        verse.chords = { chord ("Am"), chord ("F") };
        verse.rhythmPatternId = "Folk Down Up";
        verse.genreKitId = "Folk Fingerstyle";
        verse.bass.mode = bass;

        MelodyTrack melody;
        melody.notes.push_back (MelodyNote::make (0.0, 1.0, 72, 100));
        melody.notes.push_back (MelodyNote::make (1.5, 0.5, 74, 90));
        verse.melody = melody;

        TuneSection chorus;
        chorus.name = "Chorus";
        chorus.lengthBars = 1;
        chorus.chords = { chord ("G") };
        chorus.rhythmPatternId = "Classic Strum";
        chorus.genreKitId = "Folk Fingerstyle";

        t.addSection (verse);
        t.addSection (chorus);

        TuneSetlistEntry a;
        a.section = "Verse";
        a.repeats = 2;
        TuneSetlistEntry b;
        b.section = "Chorus";
        t.setSetlist ({ a, b });
        return t;
    }

    std::unique_ptr<TuneTimeline> timelineOf (const Tune& tune, int improvisePass = 0)
    {
        TuneMidiOptions options;
        options.improvisePass = improvisePass;
        return std::make_unique<TuneTimeline> (TuneTimeline::build (tune, options));
    }

    struct Heard
    {
        juce::int64 sample;
        juce::MidiMessage message;
    };

    struct Click
    {
        juce::int64 sample;
        bool downbeat;
    };

    /** Plays blocks through the player, collecting both outputs and the
        clicks at absolute sample positions. `hostFor` describes the host for
        the block starting at a sample. */
    struct Harness
    {
        TunePlayer player;
        std::vector<Heard> engine, out;
        std::vector<Click> clicks;
        juce::int64 samplesDone = 0;
        long allocations = 0;

        explicit Harness (const Tune& tune)
        {
            player.prepare (kSr, 512);
            player.setTimeline (timelineOf (tune), tune.meta.tempoBpm, tune.getBeatsPerBar());
        }

        void run (juce::int64 samples, int blockSize,
                  std::function<TunePlayer::HostInfo (juce::int64)> hostFor = {})
        {
            juce::MidiBuffer toEngine, toOut;
            toEngine.ensureSize (TunePlayer::kRecommendedMidiBytes);
            toOut.ensureSize (TunePlayer::kRecommendedMidiBytes);

            for (juce::int64 done = 0; done < samples; done += blockSize)
            {
                const int n = (int) juce::jmin ((juce::int64) blockSize, samples - done);
                const auto host = hostFor != nullptr ? hostFor (samplesDone) : TunePlayer::HostInfo();

                toEngine.clear();
                toOut.clear();

               #if defined (LUTHIER_ALLOCATION_COUNTER)
                const auto before = allocationsOnThisThread();
               #endif

                player.renderBlock (n, host, toEngine, toOut);

               #if defined (LUTHIER_ALLOCATION_COUNTER)
                allocations += allocationsOnThisThread() - before;
               #endif

                for (const auto m : toEngine)
                    engine.push_back ({ samplesDone + m.samplePosition, m.getMessage() });

                for (const auto m : toOut)
                    out.push_back ({ samplesDone + m.samplePosition, m.getMessage() });

                const auto& blockClicks = player.getBlockClicks();

                for (int i = 0; i < blockClicks.count; ++i)
                    clicks.push_back ({ samplesDone + blockClicks.offsets[(size_t) i], blockClicks.downbeat[(size_t) i] });

                samplesDone += n;
            }
        }
    };

    /** The notes sounding once everything heard has played, as channel * 128 + note. */
    std::multiset<int> soundingNotes (const std::vector<Heard>& heard)
    {
        std::map<int, int> held;

        for (const auto& h : heard)
        {
            const int key = h.message.getChannel() * 128 + h.message.getNoteNumber();

            if (h.message.isNoteOn())
                ++held[key];
            else if (h.message.isNoteOff() && held[key] > 0)
                --held[key];
        }

        std::multiset<int> result;

        for (const auto& kv : held)
            for (int i = 0; i < kv.second; ++i)
                result.insert (kv.first);

        return result;
    }

    int stillSounding (const std::vector<Heard>& heard)
    {
        return (int) soundingNotes (heard).size();
    }

    int countNoteOns (const std::vector<Heard>& heard, int channel)
    {
        int n = 0;

        for (const auto& h : heard)
            n += (h.message.isNoteOn() && h.message.getChannel() == channel) ? 1 : 0;

        return n;
    }

    /** The chord-channel notes the timeline starts at `ppq`. */
    std::set<int> chordNotesAt (const Tune& tune, double ppq)
    {
        std::set<int> notes;

        // Held in a local: a range-for over the temporary's events would read
        // them after the timeline is destroyed.
        const auto timeline = TuneTimeline::build (tune);

        for (const auto& e : timeline.getEvents())
            if (e.message.isNoteOn() && e.part == TunePart::chords && std::abs (e.ppq - ppq) < 1.0e-6)
                notes.insert (e.message.getChannel() * 128 + e.message.getNoteNumber());

        return notes;
    }

    TunePlayer::HostInfo hostAt (double ppq, double bpm, bool playing = true)
    {
        TunePlayer::HostInfo info;
        info.hasPosition = true;
        info.isPlaying = playing;
        info.bpm = bpm;
        info.ppqPosition = ppq;
        return info;
    }
}

//==============================================================================
LUTHIER_TEST (TunePlayer, everyEventLandsOnItsSampleAcrossBlockBoundaries)
{
    const auto tune = makeTune();
    Harness h (tune);
    h.player.setLoop (false);
    h.player.play();

    // 441 samples a block: no block boundary falls on a beat.
    h.run (21 * kBeat, 441);

    const auto timeline = TuneTimeline::build (tune);
    std::multimap<juce::int64, juce::String> heard;

    for (const auto& x : h.out)
        heard.emplace (x.sample, juce::String::toHexString (x.message.getRawData(), x.message.getRawDataSize()));

    // Everything the timeline holds, exactly where it is.
    int missing = 0;

    for (const auto& e : timeline.getEvents())
    {
        if (e.message.isMetaEvent())
            continue;

        const auto sample = (juce::int64) std::llround (e.ppq * (double) kBeat);
        const auto bytes = juce::String::toHexString (e.message.getRawData(), e.message.getRawDataSize());
        const auto range = heard.equal_range (sample);
        bool found = false;

        for (auto it = range.first; it != range.second; ++it)
            found = found || it->second == bytes;

        missing += found ? 0 : 1;
    }

    CHECK_MSG (missing == 0, juce::String (missing) + " events not on their sample");
    CHECK (countNoteOns (h.out, 2) == 4);   // two melody notes in each verse

    // Played to the end without Loop, it stops and nothing hangs.
    CHECK (! h.player.isPlaying());
    CHECK (stillSounding (h.engine) == 0);
    CHECK (stillSounding (h.out) == 0);
    CHECK (h.player.getNumSoundingNotes() == 0);
}

LUTHIER_TEST (TunePlayer, loopingWrapsOnTheSampleWithTheEndBeforeTheStart)
{
    const auto tune = makeTune();
    Harness h (tune);
    h.player.setLoop (true);
    h.player.play();

    // Two and a half passes of 20 beats.
    h.run (50 * kBeat, 500);

    // A note of the Am chord that the F chord does not have (F shares A2)
    // marks each Am: beats 0, 8, 20, 28, 40, 48.
    int amOnly = -1;

    for (int n : chordNotesAt (tune, 0.0))
        if (chordNotesAt (tune, 4.0).count (n) == 0 && amOnly < 0)
            amOnly = n;

    CHECK (amOnly >= 0);
    std::vector<juce::int64> amStarts;

    for (const auto& x : h.out)
        if (x.message.isNoteOn() && x.message.getChannel() * 128 + x.message.getNoteNumber() == amOnly)
            amStarts.push_back (x.sample);

    const std::vector<juce::int64> expected { 0, 8 * kBeat, 20 * kBeat, 28 * kBeat, 40 * kBeat, 48 * kBeat };
    juce::String starts;

    for (auto s : amStarts)
        starts << juce::String ((double) s / (double) kBeat, 3) << " ";

    CHECK_MSG ((amStarts == expected), "A2 starts at beats " + starts);

    // 15: after two loops the tune starts on the same sample offset as the first time.
    CHECK (amStarts.size() == 6 && amStarts[4] - amStarts[2] == 20 * kBeat && amStarts[2] - amStarts[0] == 20 * kBeat);

    // At each wrap the chorus's G ends before the verse's Am starts.
    for (size_t i = 0; i + 1 < h.out.size(); ++i)
    {
        const auto& a = h.out[i];
        const auto& b = h.out[i + 1];

        if (a.sample == b.sample && a.message.isNoteOn() && b.message.isNoteOff()
              && a.message.getNoteNumber() == b.message.getNoteNumber() && a.message.getChannel() == b.message.getChannel())
            CHECK_MSG (false, "a note ended after it was restarted at sample " + juce::String (a.sample));
    }

    CHECK (h.player.getPass() == 2);
    CHECK (stillSounding (h.out) <= 8);   // mid-pass: a chord and a melody note at most
}

LUTHIER_TEST (TunePlayer, stopPauseAndSeekLeaveNoNoteHangingAndResumeWhatIsHeld)
{
    const auto tune = makeTune();
    Harness h (tune);
    h.player.play();

    // Half a beat in: the Am chord and the first melody note are sounding.
    h.run (kBeat / 2, 256);
    const auto sounding = soundingNotes (h.out);
    CHECK (! sounding.empty());
    CHECK (h.player.getNumSoundingNotes() > 0);

    h.player.pause();
    h.run (256, 256);
    CHECK (stillSounding (h.engine) == 0);
    CHECK (stillSounding (h.out) == 0);
    CHECK (h.player.getNumSoundingNotes() == 0);

    // Paused, the position holds.
    const double held = h.player.getPositionPpq();
    h.run (4800, 256);
    CHECK_NEAR (h.player.getPositionPpq(), held, 1.0e-9);

    // Play again mid-chord: what the tune holds there starts again at once.
    h.player.play();
    const auto resumedAt = h.samplesDone;
    h.run (256, 256);
    CHECK ((soundingNotes (h.out) == sounding));

    bool restartedOnTheFirstSample = false;

    for (const auto& x : h.out)
        restartedOnTheFirstSample = restartedOnTheFirstSample
                                      || (x.sample == resumedAt && x.message.isNoteOn() && x.message.getNoteNumber() == 45);

    CHECK (restartedOnTheFirstSample);

    // Seek into the chorus while playing: the verse's notes end, the G chord plays.
    h.player.seekToSection (2);
    h.run (4800, 256);
    CHECK (h.player.getPlayingSpan() == 2);
    CHECK (h.player.getPlayingSection() == 1);

    const auto inChorus = soundingNotes (h.engine);
    const auto gChord = chordNotesAt (tune, 16.0);
    juce::String heardText, wantText;

    for (int n : inChorus) heardText << (n / 128) << ":" << (n % 128) << " ";
    for (int n : gChord)   wantText << (n / 128) << ":" << (n % 128) << " ";

    CHECK_MSG ((std::set<int> (inChorus.begin(), inChorus.end()) == gChord),
               "sounding " + heardText + "- the G chord is " + wantText);

    h.player.stop();
    h.run (256, 256);
    CHECK (stillSounding (h.engine) == 0);
    CHECK (stillSounding (h.out) == 0);
    CHECK_NEAR (h.player.getPositionPpq(), 0.0, 1.0e-9);
    CHECK (h.player.getPass() == 0);
}

LUTHIER_TEST (TunePlayer, anEditWhilePlayingWaitsForTheBarLineAndAPausedOneDoesNot)
{
    // One Am held for two bars, so a note crosses the bar line the edit waits for.
    auto tune = makeTune();
    tune.arrangement.sections[0].chords = { chord ("Am", 8.0) };

    Harness h (tune);
    h.player.play();
    h.run (kBeat, 256);   // beat 1 of bar 1

    // The same tune handed over again, as any edit does.
    h.player.setTimeline (timelineOf (tune), 120.0, 4.0);
    const auto editedAt = h.out.size();
    h.run (kBeat, 256);

    // The held chord is not cut before the bar line. (The melody's own notes
    // end when they end.)
    int offsBeforeBar = 0;

    for (size_t i = editedAt; i < h.out.size(); ++i)
        offsBeforeBar += (h.out[i].message.isNoteOff() && h.out[i].message.getChannel() == 1) ? 1 : 0;

    CHECK (offsBeforeBar == 0);

    // At the bar line (beat 4) the old notes end and the held Am starts again,
    // on the same sample, in that order.
    const auto barStarted = h.out.size();
    h.run (3 * kBeat, 256);

    bool endedThenRestarted = false;
    int endedAt = -1;

    for (size_t i = barStarted; i < h.out.size(); ++i)
    {
        const auto& x = h.out[i];

        if (x.sample == 4 * kBeat && x.message.getChannel() == 1 && x.message.getNoteNumber() == 45)
        {
            if (x.message.isNoteOff())
                endedAt = (int) i;
            else if (x.message.isNoteOn() && endedAt >= 0)
                endedThenRestarted = true;
        }
    }

    CHECK (endedThenRestarted);
    CHECK (soundingNotes (h.out).count (1 * 128 + 45) == 1);

    // Paused, an edit is heard at once: the new chords from where it resumes.
    h.player.pause();
    h.run (256, 256);

    auto edited = tune;
    edited.arrangement.sections[0].chords = { chord ("C", 8.0) };
    h.player.setTimeline (timelineOf (edited), 120.0, 4.0);
    h.run (256, 256);

    h.player.play();
    h.run (256, 256);

    const auto now = soundingNotes (h.out);
    CHECK (now.count (1 * 128 + 45) == 0);   // no A2 of the old Am
    CHECK (now.count (1 * 128 + 48) == 1);   // C3, the new chord's root
}

LUTHIER_TEST (TunePlayer, theHostWinsWhenItPlaysAndTheClockRunsWhenItDoesNot)
{
    const auto tune = makeTune();

    // Host at 100 bpm: 28800 samples a beat, whatever the tune's own tempo.
    const double hostSamplesPerBeat = kSr * 60.0 / 100.0;
    auto hostPlaying = [hostSamplesPerBeat] (juce::int64 sample) { return hostAt ((double) sample / hostSamplesPerBeat, 100.0); };

    // Not played: the host playing does not start the tune.
    {
        Harness h (tune);
        h.run (48000, 512, hostPlaying);
        CHECK (h.out.empty());
    }

    // Played, following the host: F starts at the host's beat 4.
    {
        Harness h (tune);
        h.player.play();
        h.run ((juce::int64) (5.0 * hostSamplesPerBeat), 512, hostPlaying);

        CHECK (h.player.isFollowingHost());
        CHECK (h.player.getBlockTransport().followingHost);
        CHECK_NEAR (h.player.getBlockTransport().bpm, 100.0, 1.0e-9);

        juce::int64 fStart = -1;

        for (const auto& x : h.out)
            if (x.message.isNoteOn() && x.message.getChannel() == 1 && x.message.getNoteNumber() == 41 && fStart < 0)
                fStart = x.sample;

        CHECK_MSG (std::abs (fStart - (juce::int64) std::llround (4.0 * hostSamplesPerBeat)) <= 1,
                   "F started at " + juce::String (fStart));

        // The host jumps back to its start: everything ends, and the verse starts again.
        const auto before = h.out.size();
        h.run (512, 512, [] (juce::int64) { return hostAt (0.0, 100.0); });

        bool restarted = false;

        for (size_t i = before; i < h.out.size(); ++i)
            restarted = restarted || (h.out[i].message.isNoteOn() && h.out[i].message.getNoteNumber() == 45);

        CHECK (restarted);
        CHECK (stillSounding (h.out) <= 8);

        // The host stops: the tune pauses with it and nothing hangs...
        h.run (512, 512, [] (juce::int64) { return hostAt (0.02, 100.0, false); });
        CHECK (! h.player.isPlaying());
        CHECK (stillSounding (h.out) == 0);

        // ...and follows it again when it restarts.
        h.run (512, 512, [] (juce::int64) { return hostAt (8.0, 100.0); });
        CHECK (h.player.isPlaying());
        CHECK (h.player.isFollowingHost());

        // The tune's own Pause disarms it.
        h.player.pause();
        h.run (512, 512, [] (juce::int64) { return hostAt (8.1, 100.0, false); });
        h.run (512, 512, [] (juce::int64) { return hostAt (12.0, 100.0); });
        CHECK (! h.player.isPlaying());
    }

    // The host stopped: the tune's own clock at its own 120 bpm.
    {
        Harness h (tune);
        h.player.play();
        h.run ((juce::int64) (4.5 * (double) kBeat), 512, [] (juce::int64) { return hostAt (0.0, 100.0, false); });

        CHECK (! h.player.isFollowingHost());
        CHECK (! h.player.getBlockTransport().followingHost);
        CHECK_NEAR (h.player.getBlockTransport().bpm, 120.0, 1.0e-9);

        bool fAtBeatFour = false;

        for (const auto& x : h.out)
            fAtBeatFour = fAtBeatFour || (x.message.isNoteOn() && x.message.getNoteNumber() == 41 && x.sample == 4 * kBeat);

        CHECK (fAtBeatFour);
    }
}

LUTHIER_TEST (TunePlayer, theBassGoesToTheEngineOnlyForABass)
{
    const auto tune = makeTune (BassMode::root);

    {
        Harness h (tune);
        h.player.setBassToEngine (false);
        h.player.play();
        h.run (kBeat * 8, 512);

        CHECK (countNoteOns (h.out, 3) > 0);        // the bass line is on MIDI out
        CHECK (countNoteOns (h.engine, 3) == 0);    // but a guitar does not play it
        CHECK (countNoteOns (h.engine, 1) > 0);     // everything else does
        CHECK (countNoteOns (h.engine, 2) > 0);
    }

    {
        Harness h (tune);
        h.player.setBassToEngine (true);
        h.player.play();
        h.run (kBeat * 8, 512);

        CHECK (countNoteOns (h.engine, 3) == countNoteOns (h.out, 3));
        CHECK (countNoteOns (h.engine, 3) > 0);
    }

    // The instrument changes mid-note: the note still ends where it started.
    {
        Harness h (tune);
        h.player.setLoop (false);
        h.player.setBassToEngine (true);
        h.player.play();
        h.run (kBeat, 512);
        CHECK (countNoteOns (h.engine, 3) > 0);

        h.player.setBassToEngine (false);
        h.run (kBeat * 20, 512);
        CHECK (stillSounding (h.engine) == 0);
        CHECK (stillSounding (h.out) == 0);
    }
}

LUTHIER_TEST (TunePlayer, aCountInWaitsABarAndClicksIt)
{
    const auto tune = makeTune();
    Harness h (tune);
    h.player.setCountInBars (1);
    h.player.play();

    h.run (kBeat, 480);
    CHECK (h.player.isCountingIn());
    CHECK (! h.player.getBlockTransport().running);   // the rhythm engine waits too
    CHECK (countNoteOns (h.out, 1) == 0);

    h.run (kBeat * 4, 480);
    CHECK (! h.player.isCountingIn());
    CHECK (h.player.getBlockTransport().running);

    juce::int64 first = -1;

    for (const auto& x : h.out)
        if (x.message.isNoteOn() && first < 0)
            first = x.sample;

    CHECK (first == 4 * kBeat);   // a 4/4 bar at 120 bpm

    // Four clicks on the count-in's beats, the first a downbeat; none after,
    // with the metronome off.
    CHECK (h.clicks.size() == 4);

    for (size_t i = 0; i < h.clicks.size(); ++i)
    {
        CHECK (h.clicks[i].sample == (juce::int64) i * kBeat);
        CHECK (h.clicks[i].downbeat == (i == 0));
    }
}

LUTHIER_TEST (TunePlayer, theMetronomeClicksTheTunesBeats)
{
    const auto tune = makeTune();
    Harness h (tune);
    h.player.setMetronome (true);
    h.player.play();
    h.run (kBeat * 8 + 1, 441);

    CHECK (h.clicks.size() == 9);

    for (size_t i = 0; i < h.clicks.size(); ++i)
    {
        CHECK (h.clicks[i].sample == (juce::int64) i * kBeat);
        CHECK (h.clicks[i].downbeat == (i % 4 == 0));
    }
}

LUTHIER_TEST (TunePlayer, rhythmChangesArriveForTheMessageThread)
{
    const auto tune = makeTune();
    Harness h (tune);
    h.player.play();
    h.run (512, 512);

    TuneRhythmChange change;
    CHECK (h.player.takePendingRhythmChange (change));
    CHECK (change.sectionIndex == 0 && change.patternId == "Folk Down Up");
    CHECK (! h.player.takePendingRhythmChange (change));

    // Into the chorus at beat 16.
    h.run (kBeat * 16, 512);

    TuneRhythmChange latest;
    bool any = false;

    while (h.player.takePendingRhythmChange (change))
    {
        latest = change;
        any = true;
    }

    CHECK (any);
    CHECK (latest.sectionIndex == 1 && latest.patternId == "Classic Strum");

    // Round the loop: the verse's rhythm comes back.
    h.run (kBeat * 5, 512);
    CHECK (h.player.takePendingRhythmChange (change));
    CHECK (change.sectionIndex == 0);
}

LUTHIER_TEST (TunePlayer, anImprovisedTuneGetsAPrebuiltTimelineEachPass)
{
    auto tune = makeTune();
    tune.arrangement.sections[0].melody->source = MelodySource::improvise;
    tune.arrangement.sections[0].melody->notes.clear();

    Harness h (tune);
    h.player.setLoop (true);
    h.player.play();

    // The message thread is asked for pass 1 before pass 0 ends.
    h.run (512, 512);
    CHECK (h.player.getPassNeedingTimeline() == 1);

    h.player.setNextPassTimeline (timelineOf (tune, 1), 1);
    CHECK (h.player.getPassNeedingTimeline() == -1);

    // Into pass 1, through both of its verses.
    h.run (kBeat * 36, 512);
    CHECK (h.player.getPass() == 1);
    CHECK (h.player.getPassNeedingTimeline() == 2);

    // Each pass improvised its own line: the verses of pass 0 against pass 1's.
    std::vector<int> firstPass, secondPass;

    for (const auto& x : h.out)
    {
        if (! x.message.isNoteOn() || x.message.getChannel() != 2)
            continue;

        if (x.sample < 16 * kBeat)
            firstPass.push_back (x.message.getNoteNumber());
        else if (x.sample >= 20 * kBeat && x.sample < 36 * kBeat)
            secondPass.push_back (x.message.getNoteNumber());
    }

    CHECK (! firstPass.empty());
    CHECK (! secondPass.empty());
    CHECK (firstPass != secondPass);

    // The pass-0 timeline was retired on the audio thread and is freed here.
    h.player.collectGarbage();
    CHECK (h.player.hasTimeline());
}

LUTHIER_TEST (TunePlayer, skippingMovesBySectionsAndWrapsWithLoop)
{
    const auto tune = makeTune();
    Harness h (tune);
    h.player.setLoop (true);
    h.player.play();
    h.run (512, 512);

    h.player.skipSection (1);
    h.run (512, 512);
    CHECK (h.player.getPlayingSpan() == 1);

    h.player.skipSection (1);
    h.run (512, 512);
    CHECK (h.player.getPlayingSpan() == 2);

    // Forward from the last section, with Loop on, is the first.
    h.player.skipSection (1);
    h.run (512, 512);
    CHECK (h.player.getPlayingSpan() == 0);

    // Back from more than a beat in goes to the section's own start.
    h.run (kBeat * 2, 512);
    h.player.skipSection (-1);
    h.run (256, 256);
    CHECK (h.player.getPlayingSpan() == 0);
    CHECK (h.player.getPositionPpq() < 0.1);
}

LUTHIER_TEST (TunePlayer, recordingPlacesMidiInInTheTune)
{
    const auto tune = makeTune();
    TunePlayer player;
    player.prepare (kSr, 512);
    player.setTimeline (timelineOf (tune), 120.0, 4.0);
    player.setRecordArmed (true);
    player.play();

    juce::MidiBuffer toEngine, toOut, incoming;
    toEngine.ensureSize (TunePlayer::kRecommendedMidiBytes);
    toOut.ensureSize (TunePlayer::kRecommendedMidiBytes);

    // 24000 samples a beat: a note on at beat 1 (sample 24000, offset 0 of
    // block 47 of 512 is 24064; offset 448 of block 46 is 24000).
    for (int block = 0; block < 60; ++block)
    {
        toEngine.clear();
        toOut.clear();
        incoming.clear();
        player.renderBlock (512, {}, toEngine, toOut);

        if (block == 46)
            incoming.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90), 448);

        if (block == 58)
            incoming.addEvent (juce::MidiMessage::noteOff (1, 64), 0);

        player.captureInput (incoming);
    }

    std::array<TunePlayer::RecordedEvent, 8> events;
    const int count = player.popRecordedEvents (events.data(), (int) events.size());

    CHECK (count == 2);
    CHECK (events[0].isNoteOn && events[0].note == 64 && events[0].velocity == 90);
    CHECK_NEAR (events[0].ppq, 1.0, 1.0e-6);
    CHECK (events[0].span == 0);
    CHECK (! events[1].isNoteOn);
    CHECK_NEAR (events[1].ppq, 58.0 * 512.0 / 24000.0, 1.0e-6);

    // Disarmed, nothing is taken.
    player.setRecordArmed (false);
    incoming.clear();
    incoming.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    player.renderBlock (512, {}, toEngine, toOut);
    player.captureInput (incoming);
    CHECK (player.popRecordedEvents (events.data(), (int) events.size()) == 0);
}

/*  engine.md 0: no allocation on the audio thread - through loop wraps,
    releases, the chase after a seek and a timeline swapped in at a bar line. */
LUTHIER_TEST (TunePlayer, rendersWithoutAllocating)
{
   #if defined (LUTHIER_ALLOCATION_COUNTER)
    const auto tune = makeTune (BassMode::walking);
    Harness h (tune);
    h.player.setLoop (true);
    h.player.setMetronome (true);
    h.player.play();
    h.run (kBeat * 30, 512);

    h.player.seekToSection (1);
    h.run (kBeat * 2, 512);

    h.player.setTimeline (timelineOf (tune), 120.0, 4.0);
    h.run (kBeat * 6, 512);

    h.player.pause();
    h.run (kBeat, 512);

    CHECK_MSG (h.allocations == 0, juce::String (h.allocations) + " allocations while rendering");
   #else
    // The global operator counter is not built into this test runner.
    CHECK (true);
   #endif
}
