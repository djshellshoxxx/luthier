#include "RiffDestinations.h"

#include "RiffAnalysis.h"
#include "../PluginProcessor.h"
#include "../Tune/TuneSession.h"

#include <cmath>

namespace luthier
{

namespace RiffDestinations
{
    using T = ScoreTechnique::Type;

    GuitarSpecSummary guitarSummary (LuthierAudioProcessor& processor)
    {
        auto& engine = processor.getEngine();
        const auto& tuning = engine.getTuningEngine();
        const auto& spec = engine.getGuitarSpec();

        GuitarSpecSummary g;
        g.numStrings = juce::jlimit (1, kMaxStrings, engine.getNumStrings());
        g.tuning.fill (0);

        for (int s = 0; s < g.numStrings; ++s)
            g.tuning[(size_t) s] = juce::jlimit (0, 127, (int) std::lround (hzToMidi (tuning.getStringTuning (s).openFrequencyHz,
                                                                                     tuning.getConcertA())));

        g.capo = juce::jmax (0, tuning.getCapoFret());
        g.maxFret = juce::jmax (1, tuning.getStringTuning (0).maxFrets - g.capo);
        g.isBass = spec.category == GuitarCategory::Bass;
        g.hasWhammy = spec.bridge != WhammyEngine::BridgeType::Fixed;
        return g;
    }

    MidiPerformance toPerformance (const Riff& placed, double tempoBpm, double sampleRate)
    {
        auto riff = placed;
        riff.tempoBpm = juce::jlimit (Riff::kMinTempo, Riff::kMaxTempo, tempoBpm > 0.0 ? tempoBpm : placed.tempoBpm);

        auto performance = MidiPerformance::fromScore (riff.toScore(), sampleRate);
        performance.getMeta().title = riff.meta.name;

        const bool bass = riff.isBass();
        const int part = 0;

        // midi-export 6: the strum and BASS_TECH events ride with the notes.
        for (const auto& s : riff.strums)
        {
            auto e = LuthierEvent::make (LuthierEventClass::strum,
                                         (juce::int64) std::llround (performance.beatToSample (s.beat)), part);
            e.set ("dir", s.down ? "down" : "up").setReal ("cv", s.cv).set ("striker", s.striker)
             .setReal ("mute", s.mute).setInt ("mask", s.mask);
            performance.addEvent (e);
        }

        for (const auto& b : riff.bassTech)
        {
            auto e = LuthierEvent::make (LuthierEventClass::bassTech,
                                         (juce::int64) std::llround (performance.beatToSample (b.beat)), part);
            e.set ("tech", b.tech).setInt ("str", b.str).setReal ("pos", b.pos).setReal ("force", b.force);
            performance.addEvent (e);
        }

        if (bass)
            performance.getMeta().partNames.set (0, "Bass");

        return performance;
    }

    juce::String dragFileName (const Riff& riff, const CompiledRiff& compiled, double tempoBpm)
    {
        const double bpm = tempoBpm > 0.0 ? tempoBpm : compiled.tempoBpm;
        const auto name = riff.meta.name + " - " + RiffVocabulary::rootName (compiled.placement.targetRoot)
                            + " " + juce::String (juce::roundToInt (bpm));
        return juce::File::createLegalFileName (name) + ".mid";
    }

    juce::File writeDragFile (const Riff& riff, const CompiledRiff& compiled, MidiProfile profile,
                              const juce::File& folder, int ppq, double tempoBpm, juce::String* error)
    {
        const double bpm = tempoBpm > 0.0 ? tempoBpm : compiled.tempoBpm;
        const auto placed = compiled.toPlacedRiff (riff);
        const auto performance = toPerformance (placed, bpm, 48000.0);

        MidiExportOptions options;
        options.profile = profile;
        options.split = MidiTrackSplit::single;
        options.ppq = ppq;

        if (! folder.createDirectory())
        {
            if (error != nullptr)
                *error = "Could not create " + folder.getFullPathName();

            return {};
        }

        const auto file = folder.getChildFile (dragFileName (riff, compiled, bpm));

        if (! MidiProfiles::exportToFile (performance, options, file, error))
            return {};

        return file;
    }

    int pruneDragFolder (const juce::File& folder, int days)
    {
        int removed = 0;
        const auto cutoff = juce::Time::getCurrentTime() - juce::RelativeTime::days ((double) days);

        for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.mid"))
            if (f.getLastModificationTime() < cutoff && f.deleteFile())
                ++removed;

        return removed;
    }

    //==========================================================================
    TuneInsert addToTune (TuneSession& session, const Riff& riff, int sectionIndex, double startBeat)
    {
        TuneInsert result;
        const auto& tune = session.getTune();

        if (! juce::isPositiveAndBelow (sectionIndex, tune.getNumSections()))
        {
            result.message = "Select a section in the TUNE tab first.";
            return result;
        }

        // Transposed to the tune's key, on the riff's own instrument.
        RiffPlaySettings settings;
        settings.targetRoot = tune.meta.keyTonic;
        const auto compiled = RiffCompiler::compile (riff, settings, GuitarSpecSummary::forRiff (riff));
        const auto& placed = compiled->placement.notes;

        const double barBeats = juce::jmax (1.0, tune.getBeatsPerBar());
        const double sectionBeats = tune.arrangement.sections[(size_t) sectionIndex].lengthBars * barBeats;
        const double start = startBeat >= 0.0 ? juce::jlimit (0.0, sectionBeats, startBeat) : 0.0;
        const double end = juce::jmin (sectionBeats, start + riff.lengthBeats);
        const bool bass = riff.isBass();

        std::vector<MelodyNote> incoming;

        for (const auto& n : placed)
        {
            const double at = start + n.startBeat;

            if (at >= end - 1.0e-9)
            {
                ++result.clipped;
                continue;
            }

            double velocity = n.velocity;
            if (n.hasTechnique (T::ghostNote)) velocity *= 0.45;
            if (n.hasTechnique (T::accent))    velocity *= 1.2;

            auto m = MelodyNote::make (at, juce::jmin (n.durationBeats, end - at), n.midiNote,
                                       juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0)));

            if (n.hasTechnique (T::bend) || n.hasTechnique (T::bendRelease) || n.hasTechnique (T::preBend))
                m.technique = NoteTechnique::bend;
            else if (n.hasTechnique (T::slideLegato) || n.hasTechnique (T::slideShift) || n.hasTechnique (T::slideIn)
                       || n.hasTechnique (T::slideOut) || n.hasTechnique (T::slideUp) || n.hasTechnique (T::slideDown))
                m.technique = NoteTechnique::slide;
            else if (n.hasTechnique (T::hammerOn))
                m.technique = NoteTechnique::hammerOn;
            else if (n.hasTechnique (T::pullOff))
                m.technique = NoteTechnique::pullOff;
            else if (n.hasTechnique (T::vibrato))
                m.technique = NoteTechnique::vibrato;
            else if (n.hasTechnique (T::naturalHarmonic) || n.hasTechnique (T::artificialHarmonic)
                       || n.hasTechnique (T::tapHarmonic) || n.hasTechnique (T::pinchHarmonic))
                m.technique = NoteTechnique::harmonic;

            if (n.hasTechnique (T::palmMute))     m.articulation = NoteArticulation::palmMuted;
            else if (n.hasTechnique (T::letRing)) m.articulation = NoteArticulation::letRing;
            else if (n.hasTechnique (T::staccato)) m.articulation = NoteArticulation::staccato;

            // 6.2: the exact string, fret and techniques, for TuneMidi to prefer.
            juce::Array<juce::var> techs;

            for (const auto& t : n.techniques)
            {
                auto* o = new juce::DynamicObject();
                o->setProperty ("type", RiffVocabulary::tokenForType (t.type));
                o->setProperty ("value", t.value);
                o->setProperty ("second", t.secondValue);
                techs.add (juce::var (o));
            }

            m.extra.set ("riff_str", n.stringIndex);
            m.extra.set ("riff_fret", n.fret);
            m.extra.set ("riff_tech", juce::JSON::toString (juce::var (techs), true));
            incoming.push_back (std::move (m));
        }

        const auto name = riff.meta.name;

        const bool changed = session.edit (TuneEditClass::melodyEdit, "Insert riff " + name,
            [&] (Tune& t)
            {
                auto& section = t.arrangement.sections[(size_t) sectionIndex];
                std::vector<MelodyNote>* notes = nullptr;

                if (bass)
                {
                    section.bass.mode = BassMode::manual;
                    notes = &section.bass.notes;
                }
                else
                {
                    if (! section.melody.has_value())
                        section.melody = MelodyTrack {};

                    notes = &section.melody->notes;
                }

                // Unlocked notes in the covered range go; locked ones stay.
                std::vector<MelodyNote> kept;

                for (const auto& n : *notes)
                    if (n.locked || n.getEndBeat() <= start + 1.0e-9 || n.startBeat >= end - 1.0e-9)
                        kept.push_back (n);

                for (const auto& n : incoming)
                {
                    bool overlapsLock = false;

                    for (const auto& k : kept)
                        if (k.locked && k.startBeat < n.getEndBeat() - 1.0e-9 && n.startBeat < k.getEndBeat() - 1.0e-9)
                            overlapsLock = true;

                    if (overlapsLock)
                    {
                        ++result.droppedForLocks;
                        continue;
                    }

                    kept.push_back (n);
                    ++result.inserted;
                }

                std::stable_sort (kept.begin(), kept.end(), [] (const MelodyNote& a, const MelodyNote& b)
                {
                    return a.startBeat < b.startBeat;
                });

                *notes = std::move (kept);
                return true;
            }, -1);

        result.ok = changed;
        result.message = changed ? name + " is in section " + juce::String (sectionIndex + 1) + "."
                                 : name + " could not be added to the tune.";

        if (result.droppedForLocks > 0)
            result.message << " " << result.droppedForLocks << " notes kept out by locked notes.";

        return result;
    }

    //==========================================================================
    juce::AudioBuffer<float> render (const Riff& riff, const RiffPlaySettings& settings, GuitarType guitar,
                                     double tempoBpm, double sampleRate, int lengthSamples)
    {
        constexpr int block = 512;

        LuthierEngine engine;
        engine.prepare (sampleRate, block);
        engine.setGuitarType (guitar);
        engine.reset();

        // The engine's own view of the instrument, not the caller's.
        GuitarSpecSummary g;
        {
            const auto& tuning = engine.getTuningEngine();
            g.numStrings = juce::jlimit (1, kMaxStrings, engine.getNumStrings());
            g.tuning.fill (0);

            for (int s = 0; s < g.numStrings; ++s)
                g.tuning[(size_t) s] = (int) std::lround (hzToMidi (tuning.getStringTuning (s).openFrequencyHz,
                                                                    tuning.getConcertA()));

            g.maxFret = tuning.getStringTuning (0).maxFrets;
            g.isBass = engine.getGuitarSpec().category == GuitarCategory::Bass;
        }

        auto& player = engine.getRiffPlayer();
        player.setCompiled (RiffCompiler::compile (riff, settings, g), true);
        player.setClockMode (RiffPlayer::ClockMode::own);
        player.setAbsoluteBpm (tempoBpm);
        player.setLooping (true);
        player.play();

        juce::AudioBuffer<float> out (2, juce::jmax (1, lengthSamples));
        out.clear();
        juce::AudioBuffer<float> buffer (2, block);

        for (int at = 0; at < lengthSamples; at += block)
        {
            const int n = juce::jmin (block, lengthSamples - at);
            juce::MidiBuffer midi;
            buffer.setSize (2, n, false, false, true);
            buffer.clear();
            engine.processBlock (buffer, midi);

            for (int c = 0; c < 2; ++c)
                out.copyFrom (c, at, buffer, c, 0, n);
        }

        return out;
    }

    LooperSend sendToLooper (LuthierAudioProcessor& processor, const Riff& riff,
                             const RiffPlaySettings& settings, double tempoBpm)
    {
        LooperSend result;
        auto& looper = processor.getLooper();

        if (looper.getState() != Looper::State::stopped)
        {
            result.message = "Stop the looper to send " + riff.meta.name + " to it.";
            return result;
        }

        const double rate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
        const double bpm = juce::jlimit (Riff::kMinTempo, Riff::kMaxTempo, tempoBpm > 0.0 ? tempoBpm : riff.tempoBpm);
        const double bars = (double) riff.getNumBars();
        const double seconds = bars * riff.getBeatsPerBar() * 60.0 / bpm;
        result.lengthSamples = (int) std::llround (seconds * rate);
        const auto audio = render (riff, settings, processor.getEngine().getGuitarType(), bpm, rate, result.lengthSamples);

        result.ok = looper.loadLayerAudio (looper.getActiveLayer(), audio);

        if (result.ok)
            result.lengthSamples = looper.getLoopLengthSamples();
        result.message = result.ok ? riff.meta.name + " is on looper layer " + juce::String (looper.getActiveLayer() + 1) + "."
                                   : riff.meta.name + " could not be loaded into the looper.";
        return result;
    }

    //==========================================================================
    bool riffFromCapture (LuthierAudioProcessor& processor, int lastBars, double tempoBpm, Riff& out, juce::String* error)
    {
        auto fail = [error] (const juce::String& why)
        {
            if (error != nullptr)
                *error = why;

            return false;
        };

        auto& capture = processor.getPerformanceCapture();
        capture.drain();

        const double bpm = juce::jlimit (Riff::kMinTempo, Riff::kMaxTempo, tempoBpm > 0.0 ? tempoBpm : 120.0);

        CaptureScoreOptions options;
        options.quantiseBeats = 0.25;          // 7.4: 1/16 at full strength
        options.quantiseStrength = 1.0;

        if (capture.hasMarkedRegion())
            options.sampleRange = capture.getMarkedRegion();
        else
            options.lastSeconds = juce::jmax (1, lastBars) * 4.0 * 60.0 / bpm;

        PerformanceScore score;
        capture.toScore (score, options);

        if (score.getNumTracks() == 0)
            return fail ("Nothing was captured. Play something first.");

        const auto& track = score.getTrack (0);
        const auto& meta = score.getMeta();
        const double barBeats = meta.timeSignatureNumerator * 4.0 / juce::jmax (1, meta.timeSignatureDenominator);

        std::vector<ScoreNote> notes;

        for (size_t m = 0; m < track.measures.size(); ++m)
            for (const auto* n : track.measures[m].collectNotes())
            {
                auto copy = *n;
                copy.startBeat += (double) m * barBeats;
                notes.push_back (std::move (copy));
            }

        if (notes.empty())
            return fail ("Nothing was captured. Play something first.");

        std::sort (notes.begin(), notes.end(), [] (const ScoreNote& a, const ScoreNote& b)
        {
            return a.startBeat != b.startBeat ? a.startBeat < b.startBeat : a.stringIndex < b.stringIndex;
        });

        // From the bar the phrase starts in, at most 8 bars.
        const double origin = std::floor (notes.front().startBeat / barBeats + 1.0e-9) * barBeats;
        const double maxBeats = juce::jmin (Riff::kMaxBeats, 8.0 * barBeats);
        double last = 0.0;

        Riff riff;
        riff.tuning.clear();

        for (int s = 0; s < juce::jlimit (1, kMaxStrings, track.numStrings); ++s)
            riff.tuning.push_back (track.tuning[(size_t) s]);

        riff.capo = track.capoFret;

        for (auto n : notes)
        {
            n.startBeat -= origin;

            if (n.startBeat >= maxBeats - 1.0e-9 || ! juce::isPositiveAndBelow (n.stringIndex, riff.getNumStrings()))
                continue;

            n.durationBeats = juce::jlimit (0.0625, maxBeats - n.startBeat, n.durationBeats);
            n.fret = juce::jlimit (0, Riff::kMaxFret, n.fret);
            n.startBeat = std::round (n.startBeat * 1.0e6) / 1.0e6;
            n.durationBeats = std::round (n.durationBeats * 1.0e6) / 1.0e6;
            last = juce::jmax (last, n.startBeat + n.durationBeats);
            riff.notes.push_back (std::move (n));
        }

        const bool bass = processor.getEngine().getGuitarSpec().category == GuitarCategory::Bass;
        const int strings = riff.getNumStrings();
        riff.instrument = bass ? (strings >= 6 ? "bass6" : strings == 5 ? "bass5" : "bass4")
                               : (strings >= 7 ? "guitar7" : "guitar6");
        riff.type = bass ? "bass" : "lick";
        riff.tuningName = "Standard";
        riff.tempoBpm = std::round (juce::jlimit (Riff::kMinTempo, Riff::kMaxTempo, meta.tempoBpm > 0.0 ? meta.tempoBpm : bpm));
        riff.meterNumerator = meta.timeSignatureNumerator;
        riff.meterDenominator = meta.timeSignatureDenominator;
        riff.lengthBeats = juce::jlimit (barBeats, maxBeats, std::ceil (last / barBeats - 1.0e-9) * barBeats);
        riff.meta.origin = "capture";
        riff.meta.author = {};

        RiffAnalysis::analyse (riff);
        out = std::move (riff);
        return true;
    }

    juce::String auditionAnnouncement (const juce::String& name, int rootPitchClass, double bpm, bool looping)
    {
        return "Playing " + name + " in " + RiffVocabulary::rootName (rootPitchClass) + ", "
                 + juce::String (juce::roundToInt (bpm)) + " bpm" + (looping ? ", looping" : "");
    }
}

} // namespace luthier
