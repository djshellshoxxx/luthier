/*  MIDI export and import (midi-export.md), at the level of the model.

    Section 12's tests, as far as they can be run without a host: the Luthier
    and Generic round trips (the Luthier one rendered through the engine for
    every factory preset and nulled at -60 dBFS RMS, the Generic one at -30),
    SysEx redundancy, PPQ scaling, the drag-out file, header disambiguation,
    every byte of a file flipped in turn, and live MIDI out's timing over ten
    thousand events. Plus the pieces those rest on: the tagged payload, the
    .midprofile, ranges, the score conversion.
*/

#include "TestFramework.h"

#include "../Export/LiveMidiOut.h"
#include "../Export/MidiProfiles.h"
#include "../LuthierEngine.h"
#include "../Parameters.h"
#include "../Presets/FactoryPresets.h"
#include "../Presets/PresetManager.h"
#include "../Routing/MidiOutRouter.h"
#include "../Support/MidiCapture.h"

#include <algorithm>
#include <cstring>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
    constexpr double kRenderSeconds = 1.25;

    using Field = LuthierSysExOut::Field;

    //==========================================================================
    /** The engine behind the real APVTS and preset manager, without the
        plugin wrapper: IntegrationTests' harness. */
    class ExportHarness : public juce::AudioProcessor
    {
    public:
        ExportHarness()
            : AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
              apvts (*this, nullptr, "LUTHIER", Parameters::createLayout()),
              bridge (apvts, engine),
              presets (*this, apvts, engine, ranges)
        {
            bridge.cachePointers();
        }

        void prepareToPlay (double sampleRate, int blockSize) override
        {
            engine.prepare (sampleRate, blockSize);
            bridge.applyAllNow();
        }

        void releaseResources() override { engine.releaseResources(); }

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
        {
            bridge.applyToEngine();
            engine.processBlock (buffer, midi);
        }

        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        const juce::String getName() const override { return "LuthierMidiExportHarness"; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 8.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        LuthierEngine engine;
        RangeState ranges;
        juce::AudioProcessorValueTreeState apvts;
        ParameterBridge bridge;
        PresetManager presets;
    };

    /** Humanisation is random per note by design, so a null test turns it off. */
    void silenceRandomness (ExportHarness& harness)
    {
        if (auto* p = harness.apvts.getParameter (ParamIDs::macroHumanize))
            p->setValueNotifyingHost (0.0f);

        if (auto* p = harness.apvts.getParameter (ParamIDs::realismDetune))
            p->setValueNotifyingHost (0.0f);
    }

    /** A performance played through the engine from a clean slate, as mono. */
    std::vector<double> renderPerformance (ExportHarness& harness, const MidiPerformance& performance,
                                           double seconds)
    {
        // reset, apply, reset: the order Presets::audioIsIdenticalAfterARoundTrip
        // settled on for a render that repeats sample for sample.
        harness.engine.reset();
        harness.bridge.applyAllNow();
        harness.engine.reset();

        const int total = (int) (kSr * seconds);
        std::vector<double> mono ((size_t) total, 0.0);

        juce::AudioBuffer<float> block (2, kBlock);
        size_t cursor = 0;

        for (int position = 0; position < total; position += kBlock)
        {
            const int count = juce::jmin (kBlock, total - position);
            block.setSize (2, count, false, false, true);
            block.clear();

            juce::MidiBuffer midi;
            performance.renderBlock (midi, position, count, cursor);
            harness.engine.processBlock (block, midi);

            for (int i = 0; i < count; ++i)
                mono[(size_t) (position + i)] = 0.5 * ((double) block.getSample (0, i)
                                                       + (double) block.getSample (1, i));
        }

        return mono;
    }

    double rmsDbfs (const std::vector<double>& signal)
    {
        return juce::Decibels::gainToDecibels (rms (signal.data(), (int) signal.size()), -400.0);
    }

    /** 12's measure: the RMS of the difference, in dB relative to full scale. */
    double nullDbfs (const std::vector<double>& a, const std::vector<double>& b)
    {
        const size_t n = juce::jmin (a.size(), b.size());
        std::vector<double> difference (n);

        for (size_t i = 0; i < n; ++i)
            difference[i] = a[i] - b[i];

        return rmsDbfs (difference);
    }

    //==========================================================================
    MidiImportResult roundTrip (const MidiPerformance& source, const MidiExportOptions& options,
                                MidiPerformance& back, double rate = kSr)
    {
        const auto bytes = MidiProfiles::exportToMemory (source, options);
        return MidiProfiles::importFromMemory (bytes.getData(), bytes.getSize(), back, rate);
    }

    bool containsText (const juce::MemoryBlock& bytes, const char* text)
    {
        const auto* data = static_cast<const char*> (bytes.getData());
        const auto* end = data + bytes.getSize();
        return std::search (data, end, text, text + std::strlen (text)) != end;
    }

    MidiProfiles::SmfFile parse (const juce::MemoryBlock& bytes)
    {
        MidiProfiles::SmfFile smf;
        juce::String error;
        juce::int64 at = -1;
        MidiProfiles::parseSmf (bytes.getData(), bytes.getSize(), smf, error, at);
        return smf;
    }

    juce::String metaText (const MidiProfiles::SmfEvent& event, const juce::uint8* data)
    {
        if (! event.isMeta() || event.dataSize == 0)
            return {};

        const auto* text = reinterpret_cast<const char*> (data + event.dataOffset);

        return juce::CharPointer_UTF8::isValidString (text, (int) event.dataSize)
                 ? juce::String::fromUTF8 (text, (int) event.dataSize)
                 : juce::String();
    }

    /** Rebuilds a file with only the events `keep` accepts: how these tests
        strip a header, or one of an event's two copies, as a tool would. */
    juce::MemoryBlock rewrite (const juce::MemoryBlock& original,
                               const std::function<bool (size_t track, size_t index,
                                                         const MidiProfiles::SmfEvent&,
                                                         const juce::uint8*)>& keep)
    {
        const auto smf = parse (original);
        const auto* data = static_cast<const juce::uint8*> (original.getData());

        juce::MidiFile file;
        file.setTicksPerQuarterNote (smf.ppq);

        for (size_t t = 0; t < smf.tracks.size(); ++t)
        {
            juce::MidiMessageSequence sequence;
            const auto& events = smf.tracks[t].events;

            for (size_t i = 0; i < events.size(); ++i)
            {
                const auto& event = events[i];

                // The writer adds its own end-of-track.
                if (event.isMeta() && event.metaType == 0x2F)
                    continue;

                if (! keep (t, i, event, data))
                    continue;

                auto message = event.toMessage (data);
                message.setTimeStamp ((double) event.tick);
                sequence.addEvent (message);
            }

            file.addTrack (sequence);
        }

        juce::MemoryOutputStream out;
        file.writeTo (out, 1);
        return out.getMemoryBlock();
    }

    //==========================================================================
    /*  A performance whose timing falls between ticks at every PPQ - 123 bpm
        gives no whole number of samples per tick - with a tempo change, two
        parts, six channels, a bend, controllers, pressure, a program change,
        sections, identifiers and realism. */
    MidiPerformance makeLoosePerformance()
    {
        MidiPerformance performance (kSr);
        performance.setTempo (123.0);
        performance.addTempoChange (2.0, 97.5);
        performance.setTimeSignature (4, 4);

        auto& meta = performance.getMeta();
        meta.title = "Loose fixture";
        meta.copyright = "(c) the test suite";
        meta.guitarName = "Red Tele Custom";
        meta.presetName = "Glassy Clean";
        meta.characterSeed = "4815162342";

        static constexpr int keys[] = { 40, 47, 52, 55, 59, 64, 67 };

        juce::int64 at = 101;

        for (int i = 0; i < 16; ++i)
        {
            const int channel = 1 + (i % 6);
            const int key = keys[i % 7];
            const int part = (i % 5 == 4) ? 1 : 0;
            const auto off = at + 2000 + 377 * (i % 4);

            performance.addMessage (at, juce::MidiMessage::noteOn (channel, key, (juce::uint8) (40 + 5 * i)), part);
            performance.addMessage (off, juce::MidiMessage::noteOff (channel, key), part);

            at += 1733 + 97 * (i % 3) + (i == 13 ? 30011 : 0);   // one gap runs past the tempo change
        }

        for (int step = 0; step <= 16; ++step)
            performance.addMessage (3001 + step * 97, juce::MidiMessage::pitchWheel (1, 8192 + step * 300));

        performance.addMessage (3001 + 17 * 97, juce::MidiMessage::pitchWheel (1, 8192));
        performance.addMessage (0, juce::MidiMessage::programChange (1, 5));
        performance.addMessage (557, juce::MidiMessage::controllerEvent (1, 1, 64));
        performance.addMessage (4411, juce::MidiMessage::controllerEvent (1, 64, 127));
        performance.addMessage (9999, juce::MidiMessage::controllerEvent (1, 64, 0));
        performance.addMessage (12345, juce::MidiMessage::controllerEvent (2, 11, 90));
        performance.addMessage (7777, juce::MidiMessage::channelPressureChange (1, 70));

        auto intro = LuthierEvent::make (LuthierEventClass::section, 0);
        intro.set ("name", "Intro").setInt ("index", 0).set ("edge", "start");
        performance.addEvent (intro);

        auto verse = LuthierEvent::make (LuthierEventClass::section, 15000);
        verse.set ("name", "Verse").setInt ("index", 1).set ("edge", "start");
        performance.addEvent (verse);

        auto strum = LuthierEvent::make (LuthierEventClass::strum, 101);
        strum.set ("dir", "down").setReal ("cv", 212.5).set ("striker", "pick").setInt ("mute", 0).setInt ("mask", 63);
        performance.addEvent (strum);

        auto squeak = LuthierEvent::make (LuthierEventClass::squeak, 5003);
        squeak.set ("trigger", "slide").setReal ("dur", 140.0).setReal ("intensity", 0.6).set ("material", "phosphor");
        performance.addEvent (squeak);

        auto character = LuthierEvent::make (LuthierEventClass::character, 8000);
        character.set ("what", "seed").set ("seed", "4815162342").setReal ("temp", 21.5).setReal ("humidity", 40.0);
        performance.addEvent (character);

        auto range = LuthierEvent::make (LuthierEventClass::ranges, 9000);
        range.set ("param", "pickupHeight").setInt ("on", 1).setReal ("value", 0.5).setReal ("min", 0.0).setReal ("max", 1.0);
        performance.addEvent (range);

        auto pick = LuthierEvent::make (LuthierEventClass::pick, 12001, 1);
        pick.setInt ("ch", 2).setInt ("str", 1).set ("material", "nylon").setReal ("thick", 0.88).setReal ("chirp", 0.25);
        performance.addEvent (pick);

        auto slap = LuthierEvent::make (LuthierEventClass::bassTech, 20002, 1);
        slap.set ("tech", "slap").setInt ("str", 3).setReal ("pos", 0.125);
        performance.addEvent (slap);

        auto workshop = LuthierEvent::make (LuthierEventClass::workshop, 60001);
        workshop.set ("slot", "bridge").set ("fit", "Wraparound 1").set ("was", "ABR-1 = 50% off");
        performance.addEvent (workshop);

        return performance;
    }

    /** What the render tests play: one channel, sub-tick timing, a bend, and
        the controllers the engine listens to by default. */
    MidiPerformance makeRenderPerformance()
    {
        MidiPerformance performance (kSr);
        performance.setTempo (123.0);
        performance.getMeta().title = "Render fixture";

        struct Hit { juce::int64 at; int key; };
        static constexpr Hit hits[] = { { 101, 40 }, { 2377, 47 }, { 4913, 52 }, { 9001, 56 },
                                        { 15013, 59 }, { 21011, 64 }, { 27077, 52 }, { 33331, 55 } };

        for (int i = 0; i < (int) (sizeof (hits) / sizeof (hits[0])); ++i)
        {
            const auto& hit = hits[i];
            performance.addMessage (hit.at, juce::MidiMessage::noteOn (1, hit.key, (juce::uint8) (70 + 7 * i)));
            performance.addMessage (hit.at + 9000 + 311 * i, juce::MidiMessage::noteOff (1, hit.key));
        }

        // Up a whole tone and back, over the held chord.
        for (int step = 0; step <= 24; ++step)
            performance.addMessage (12007 + 173 * step,
                                    juce::MidiMessage::pitchWheel (1, 8192 + (step <= 12 ? step : 24 - step) * 250));

        performance.addMessage (6007, juce::MidiMessage::controllerEvent (1, 64, 127));    // sustain
        performance.addMessage (26003, juce::MidiMessage::controllerEvent (1, 64, 0));
        performance.addMessage (30011, juce::MidiMessage::controllerEvent (1, 1, 90));     // vibrato depth
        performance.addMessage (36013, juce::MidiMessage::controllerEvent (1, 1, 0));
        performance.addMessage (40009, juce::MidiMessage::controllerEvent (1, 11, 100));   // level
        performance.addMessage (18061, juce::MidiMessage::channelPressureChange (1, 60));

        // Realism riding along. The engine derives its own; these are the record.
        auto strum = LuthierEvent::make (LuthierEventClass::strum, 101);
        strum.set ("dir", "down").setReal ("cv", 212.5).set ("striker", "pick").setInt ("mute", 0).setInt ("mask", 63);
        performance.addEvent (strum);

        auto squeak = LuthierEvent::make (LuthierEventClass::squeak, 15013);
        squeak.set ("trigger", "shift").setReal ("dur", 90.0).setReal ("intensity", 0.4).set ("material", "nickel");
        performance.addEvent (squeak);

        return performance;
    }

    /** Every documented field of a class, set away from its default. */
    LuthierEvent fullEvent (LuthierEventClass eventClass, juce::int64 sample, int salt)
    {
        auto event = LuthierEvent::make (eventClass, sample);
        int index = 0;

        for (const auto& spec : LuthierEvents::getFields (eventClass))
        {
            ++index;

            const juce::String fallback (spec.defaultValue);
            juce::int64 whole = 0;
            double real = 0.0;

            if (LuthierEvents::parseInt (fallback, whole))
                event.setInt (spec.key, whole + salt + index);
            else if (LuthierEvents::parseReal (fallback, real))
                event.setReal (spec.key, real + 0.125 * (salt + index) + 1.0 / 3.0);
            else
                event.set (spec.key, "v" + juce::String (salt) + " x=" + juce::String (index) + " 100%");
        }

        return event;
    }

    //==========================================================================
    PerformanceScore makeScore()
    {
        PerformanceScore score;
        score.getMeta().title = "Score fixture";
        score.beginCapture (120.0, 4, 4);

        auto technique = [] (ScoreTechnique::Type type, double value = 0.0, double second = 0.0)
        {
            ScoreTechnique t;
            t.type = type;
            t.value = value;
            t.secondValue = second;
            return t;
        };

        score.noteStarted (5, 0, 40, 82.41, 0.8, 0.0);
        score.addTechnique (5, technique (ScoreTechnique::Type::palmMute));
        score.noteEnded (5, 1.0);

        score.noteStarted (2, 7, 62, 293.66, 0.7, 1.0);
        auto bend = technique (ScoreTechnique::Type::bend, 2.0);
        bend.curve = { { 0.0, 0.0 }, { 0.5, 2.0 }, { 1.0, 2.0 } };
        score.addTechnique (2, bend);
        score.noteEnded (2, 2.0);

        score.noteStarted (1, 5, 64, 329.63, 0.9, 2.0);
        score.addTechnique (1, technique (ScoreTechnique::Type::vibrato, 6.0, 30.0));
        score.addTechnique (1, technique (ScoreTechnique::Type::hammerOn));
        score.noteEnded (1, 3.0);

        score.noteStarted (0, 12, 76, 659.26, 0.6, 3.0);
        score.addTechnique (0, technique (ScoreTechnique::Type::trill, 14.0));
        score.addTechnique (0, technique (ScoreTechnique::Type::slideUp, 15.0));
        score.noteEnded (0, 3.5);

        score.endCapture (4.0);
        return score;
    }

    struct FlatNote
    {
        double start;
        int string, fret, midi;
        double duration, velocity;
        std::vector<ScoreTechnique> techniques;
    };

    std::vector<FlatNote> flatten (const PerformanceScore& score)
    {
        std::vector<FlatNote> notes;

        const auto& meta = score.getMeta();
        const double beatsPerMeasure = (double) meta.timeSignatureNumerator * 4.0
                                         / (double) juce::jmax (1, meta.timeSignatureDenominator);
        const auto& track = score.getTrack (0);

        for (size_t m = 0; m < track.measures.size(); ++m)
            for (const auto* note : track.measures[m].collectNotes())
                notes.push_back ({ (double) m * beatsPerMeasure + note->startBeat, note->stringIndex, note->fret,
                                   note->midiNote, note->durationBeats, note->velocity, note->techniques });

        std::sort (notes.begin(), notes.end(), [] (const FlatNote& a, const FlatNote& b) { return a.start < b.start; });
        return notes;
    }

    juce::String describeTechniques (const std::vector<ScoreTechnique>& techniques)
    {
        juce::StringArray items;

        for (const auto& technique : techniques)
        {
            juce::String item (getTechniqueName (technique.type));
            item << " " << juce::String (technique.value, 4) << " " << juce::String (technique.secondValue, 4);

            for (const auto& [position, semitones] : technique.curve)
                item << " " << juce::String (position, 4) << ":" << juce::String (semitones, 4);

            items.add (item);
        }

        items.sort (false);
        return items.joinIntoString ("; ");
    }
}

//==============================================================================
/*  2.1: the encoding is compact, and a hex editor shows the fields by name. */
LUTHIER_TEST (MidiExport, eventPayloadsAreTaggedSevenBitText)
{
    auto strum = LuthierEvent::make (LuthierEventClass::strum, 0);
    strum.set ("dir", "down").setReal ("cv", 200.0).set ("striker", "pick").setInt ("mute", 0).setInt ("mask", 63);

    LuthierEvents::WriteOptions exact;
    exact.sampleCorrection = -3;

    CHECK (LuthierEvents::encodePayload (strum, exact) == "STRUM 1 dt=-3 dir=down cv=200 striker=pick mute=0 mask=63");

    // 3's human-readable line, as the Generic profile writes it.
    LuthierEvents::WriteOptions generic;
    generic.writeTiming = false;

    CHECK (LuthierEvents::encodeText (strum, generic) == "LUTHIER: STRUM dir=down cv=200 striker=pick mute=0 mask=63");

    const auto sysEx = LuthierEvents::encodeSysEx (strum, exact);
    const auto* data = sysEx.getSysExData();
    const int size = sysEx.getSysExDataSize();

    CHECK (size > 6);
    CHECK (data[0] == 0x7D && data[1] == 'L' && data[2] == 'T' && data[3] == LuthierEvents::kWireVersion);

    bool printable = true;

    for (int i = 4; i < size - 1; ++i)
        printable = printable && data[i] >= 0x20 && data[i] <= 0x7E;

    CHECK_MSG (printable, "the SysEx payload is not plain text");
    CHECK (juce::String::fromUTF8 (reinterpret_cast<const char*> (data + 4), size - 5)
             == LuthierEvents::encodePayload (strum, exact));

    LuthierEvent back;
    juce::int64 dt = 0;
    juce::String error;

    CHECK_MSG (LuthierEvents::decodeSysEx (data, size, back, dt, error), error);
    CHECK (dt == -3);
    CHECK (back.hasSameContent (strum));

    // One flipped bit in the payload is caught by the checksum.
    std::vector<juce::uint8> damaged (data, data + size);
    damaged[10] ^= 0x01;

    CHECK (! LuthierEvents::decodeSysEx (damaged.data(), size, back, dt, error));
    CHECK_MSG (error.contains ("checksum"), error);

    // Text copies read back the same way, given the schema from LUTHIER-BEGIN.
    CHECK_MSG (LuthierEvents::decodeText (LuthierEvents::encodeText (strum, exact), 1, back, dt, error), error);
    CHECK (dt == -3);
    CHECK (back.hasSameContent (strum));
}

LUTHIER_TEST (MidiExport, realsAndTextSurviveTheWireExactly)
{
    for (const double value : { 0.6, 1.0 / 3.0, -123.456, 440.0, 1.0e-9, 6.02214076e23, 0.1 + 0.2, -0.0 })
    {
        double back = 1.0;
        const auto text = LuthierEvents::formatReal (value);

        CHECK_MSG (LuthierEvents::parseReal (text, back) && juce::exactlyEqual (back, value),
                   juce::String (value, 17) + " came back as " + text);
    }

    // The shortest form wins when it is exact, so the file stays readable.
    CHECK (LuthierEvents::formatReal (0.6) == "0.6");
    CHECK (LuthierEvents::formatReal (200.0) == "200");

    // Spaces, '=', '%' and letters outside ASCII escape to one token and back.
    const auto name = juce::String::fromUTF8 ("Caf\xc3\xa9 = 50% off");
    const auto escaped = LuthierEvents::escapeValue (name);
    juce::String unescaped;

    CHECK (! escaped.containsAnyOf (" ="));
    CHECK (LuthierEvents::unescapeValue (escaped, unescaped));
    CHECK (unescaped == name);

    // A malformed escape is refused, not guessed at.
    CHECK (! LuthierEvents::unescapeValue ("50%", unescaped));
    CHECK (! LuthierEvents::unescapeValue ("%00", unescaped));
}

//==============================================================================
/*  2.1 and 2.2: every class, every field, round trips; a later version's class
    comes back whole. */
LUTHIER_TEST (MidiExport, everyEventClassRoundTripsWithEveryField)
{
    MidiPerformance source (kSr);
    source.addMessage (0, juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100));
    source.addMessage (40000, juce::MidiMessage::noteOff (1, 52));

    for (int c = 0; c < LuthierEvents::kNumClasses; ++c)
        source.addEvent (fullEvent ((LuthierEventClass) c, 1000 + 333 * c, c + 1));

    // A class from a later version, in tagged form.
    auto future = LuthierEvent::make (LuthierEventClass::unknown, 9001);
    future.className = "FUTURE_THING";
    future.schemaVersion = 3;
    future.set ("wobble", "0.5").set ("colour", "teal");
    source.addEvent (future);

    MidiPerformance back;
    const auto result = roundTrip (source, MidiExportOptions {}, back);

    CHECK_MSG (result.ok, result.error);
    CHECK (result.detectedProfile == MidiProfile::luthier);
    CHECK_MSG (result.defaultedFields.isEmpty(), result.defaultedFields.joinIntoString (", "));
    CHECK (result.warnings.joinIntoString ("\n").contains ("FUTURE_THING"));

    juce::String why;
    CHECK_MSG (source.isEquivalentTo (back, &why), why);

    for (int c = 0; c < LuthierEvents::kNumClasses; ++c)
    {
        const auto eventClass = (LuthierEventClass) c;
        CHECK_MSG (back.countEvents (eventClass) == 1,
                   juce::String (LuthierEvents::getClassName (eventClass)) + " did not come back once");
    }

    // A later version's class that is not tagged text at all: kept as bytes.
    MidiPerformance withBlob (kSr);
    withBlob.addMessage (0, juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100));
    withBlob.addMessage (24000, juce::MidiMessage::noteOff (1, 52));

    std::vector<juce::uint8> raw { 0x7D, 'L', 'T', 0x02, 0x01, 0x02, 0x7F, 0x00, 0x33 };
    raw.push_back (LuthierEvents::checksum (raw.data() + 4, raw.size() - 4));

    LuthierEvent blob;
    blob.eventClass = LuthierEventClass::unknown;
    blob.className = "FUTURE_BLOB";
    blob.schemaVersion = 7;
    blob.sample = 500;    // tick 20 at 960 PPQ and 120 bpm: an opaque event keeps its tick
    blob.opaqueSysEx.append (raw.data(), raw.size());
    withBlob.addEvent (blob);

    for (const bool sysEx : { true, false })
    {
        MidiExportOptions options;
        options.sysExRedundancy = sysEx;

        MidiPerformance blobBack;
        const auto blobResult = roundTrip (withBlob, options, blobBack);

        CHECK_MSG (blobResult.ok, blobResult.error);
        CHECK_MSG (withBlob.isEquivalentTo (blobBack, &why), why);
        CHECK (blobResult.warnings.joinIntoString ("\n").contains ("FUTURE_BLOB"));
    }
}

//==============================================================================
/*  2.2 and 12 "PPQ scaling": exact at every PPQ and every split. */
LUTHIER_TEST (MidiExport, luthierProfileIsSampleExactAtEveryPpqAndSplit)
{
    const auto source = makeLoosePerformance();

    for (const int ppq : { 96, 480, 960, 3840 })
    {
        for (const auto split : { MidiTrackSplit::single, MidiTrackSplit::perSection,
                                  MidiTrackSplit::perInstrument, MidiTrackSplit::perString })
        {
            const juce::String where = "PPQ " + juce::String (ppq) + ", split "
                                         + MidiProfiles::getSplitName (split) + ": ";

            MidiExportOptions options;
            options.ppq = ppq;
            options.split = split;

            MidiPerformance back;
            const auto result = roundTrip (source, options, back);

            CHECK_MSG (result.ok, where + result.error);
            CHECK (result.detectedProfile == MidiProfile::luthier);
            CHECK (result.ppq == ppq);
            CHECK_MSG (result.warnings.isEmpty(), where + result.warnings.joinIntoString ("; "));

            juce::String why;
            CHECK_MSG (source.isEquivalentTo (back, &why), where + why);

            const int expectedTracks = split == MidiTrackSplit::single        ? 2
                                     : split == MidiTrackSplit::perSection    ? 3
                                     : split == MidiTrackSplit::perInstrument ? 3
                                                                              : 7;
            CHECK_MSG (result.numTracks == expectedTracks,
                       where + juce::String (result.numTracks) + " tracks");

            // Track 0's meta and the identifiers come back too (11: the
            // Luthier profile keeps them unless told not to).
            const auto& meta = back.getMeta();
            CHECK (meta.title == "Loose fixture");
            CHECK (meta.copyright == "(c) the test suite");
            CHECK (meta.guitarName == "Red Tele Custom");
            CHECK (meta.presetName == "Glassy Clean");
            CHECK (meta.characterSeed == "4815162342");
            CHECK_NEAR (back.getTempoAt (0.0), 123.0, 1.0e-3);
            CHECK_NEAR (back.getTempoAt (3.0), 97.5, 1.0e-3);
            CHECK (back.getTimeSignatureAt (0.0).numerator == 4);
        }
    }
}

/*  4.1: a split names its tracks after what is in them. */
LUTHIER_TEST (MidiExport, trackSplitsNameTheirTracks)
{
    const auto source = makeLoosePerformance();

    auto namesFor = [&source] (MidiTrackSplit split)
    {
        MidiExportOptions options;
        options.split = split;

        const auto bytes = MidiProfiles::exportToMemory (source, options);
        const auto smf = parse (bytes);
        const auto* data = static_cast<const juce::uint8*> (bytes.getData());

        juce::StringArray names;

        for (size_t t = 1; t < smf.tracks.size(); ++t)
            for (const auto& event : smf.tracks[t].events)
                if (event.isMeta() && event.metaType == 0x03)
                {
                    names.add (metaText (event, data));
                    break;
                }

        return names.joinIntoString ("|");
    };

    CHECK_MSG (namesFor (MidiTrackSplit::perSection) == "Intro|Verse", namesFor (MidiTrackSplit::perSection));
    CHECK_MSG (namesFor (MidiTrackSplit::perInstrument) == "Guitar|Bass", namesFor (MidiTrackSplit::perInstrument));
    CHECK_MSG (namesFor (MidiTrackSplit::perString).startsWith ("String 1|String 2"), namesFor (MidiTrackSplit::perString));
}

//==============================================================================
/*  12 "SysEx redundancy": with SysEx off the text metas carry everything; and
    2.3, "the plugin reads either": with the texts gone the SysEx does. */
LUTHIER_TEST (MidiExport, eitherCopyOfAnEventIsEnough)
{
    const auto source = makeLoosePerformance();

    MidiExportOptions noSysEx;
    noSysEx.sysExRedundancy = false;

    const auto textOnly = MidiProfiles::exportToMemory (source, noSysEx);

    int sysExCount = 0;

    for (const auto& track : parse (textOnly).tracks)
        for (const auto& event : track.events)
            if (event.isSysEx())
                ++sysExCount;

    CHECK_MSG (sysExCount == 0, juce::String (sysExCount) + " SysEx events with SysEx redundancy off");

    MidiPerformance back;
    auto result = MidiProfiles::importFromMemory (textOnly.getData(), textOnly.getSize(), back, kSr);
    juce::String why;

    CHECK_MSG (result.ok, result.error);
    CHECK_MSG (source.isEquivalentTo (back, &why), why);

    // Now the other way: keep the SysEx, drop every text copy.
    const auto both = MidiProfiles::exportToMemory (source, MidiExportOptions {});
    const auto sysExOnly = rewrite (both, [] (size_t, size_t, const MidiProfiles::SmfEvent& event, const juce::uint8* data)
    {
        return ! metaText (event, data).startsWith (LuthierEvents::kTextPrefix);
    });

    CHECK (sysExOnly.getSize() < both.getSize());

    MidiPerformance backFromSysEx;
    result = MidiProfiles::importFromMemory (sysExOnly.getData(), sysExOnly.getSize(), backFromSysEx, kSr);

    CHECK_MSG (result.ok, result.error);
    CHECK_MSG (source.isEquivalentTo (backFromSysEx, &why), why);
}

//==============================================================================
/*  3: plain MIDI any reader takes, realism as text for people, the bend range
    up front, and no identifiers (11). */
LUTHIER_TEST (MidiExport, genericProfileIsPlainMidi)
{
    const auto source = makeLoosePerformance();

    MidiExportOptions options;
    options.profile = MidiProfile::generic;
    options.includeRealism = true;

    const auto bytes = MidiProfiles::exportToMemory (source, options);

    // A reader that has never heard of Luthier - JUCE's own - accepts it.
    {
        juce::MemoryInputStream in (bytes, false);
        juce::MidiFile midi;
        CHECK (midi.readFrom (in));
        CHECK (midi.getNumTracks() == 2);
    }

    const auto smf = parse (bytes);
    const auto* data = static_cast<const juce::uint8*> (bytes.getData());

    int sysEx = 0, realism = 0, markers = 0;
    bool sawStrum = false;

    for (const auto& track : smf.tracks)
    {
        for (const auto& event : track.events)
        {
            if (event.isSysEx() || (event.isMeta() && event.metaType == 0x7F))
                ++sysEx;

            const auto text = metaText (event, data);

            if (text.startsWith (LuthierEvents::kTextPrefix))
                ++realism;
            else if (text.startsWith ("LUTHIER"))
                ++markers;

            if (text.startsWith ("LUTHIER: STRUM dir=down cv=212.5 striker=pick"))
                sawStrum = true;
        }
    }

    CHECK_MSG (sysEx == 0, juce::String (sysEx) + " Luthier-only events in a Generic file");
    CHECK_MSG (markers == 0, juce::String (markers) + " Luthier markers in a Generic file");
    CHECK (realism == (int) source.getEvents().size());
    CHECK (sawStrum);

    CHECK (! containsText (bytes, "Tele"));
    CHECK (! containsText (bytes, "Glassy"));
    CHECK (! containsText (bytes, "4815162342"));

    // The pitch-bend range RPN opens the event track, before any note.
    {
        const auto& events = smf.tracks[1].events;
        std::vector<std::pair<int, int>> controllers;

        for (const auto& event : events)
        {
            if (! event.isChannel())
                continue;

            if ((event.status & 0xF0) != 0xB0 || event.tick != 0 || controllers.size() == 6)
                break;

            controllers.emplace_back (event.data1, event.data2);
        }

        const std::vector<std::pair<int, int>> rpn { { 101, 0 }, { 100, 0 }, { 6, 2 }, { 38, 0 }, { 101, 127 }, { 100, 127 } };
        CHECK (controllers == rpn);
    }

    // Back in: Generic, silently, notes and controllers where they were to the
    // tick, realism left behind (12: "realism events lost by design").
    MidiPerformance back;
    const auto result = MidiProfiles::importFromMemory (bytes.getData(), bytes.getSize(), back, kSr);

    CHECK_MSG (result.ok, result.error);
    CHECK (result.detectedProfile == MidiProfile::generic);
    CHECK (result.warnings.isEmpty());
    CHECK (back.getEvents().empty());
    CHECK (result.ignoredRealismTexts == realism);
    CHECK_NEAR (back.getMeta().pitchBendRangeSemitones, 2.0, 1.0e-9);

    const auto& before = source.getMessages();
    const auto& after = back.getMessages();

    CHECK (before.size() == after.size());

    // Half a tick at the slower tempo, plus a sample of rounding.
    const double tolerance = 0.5 * kSr * 60.0 / (97.5 * MidiExportOptions::kDefaultPpq) + 1.0;
    double worst = 0.0;
    int differentBytes = 0;

    for (size_t i = 0; i < juce::jmin (before.size(), after.size()); ++i)
    {
        worst = juce::jmax (worst, std::abs ((double) (before[i].sample - after[i].sample)));

        if (before[i].message.getRawDataSize() != after[i].message.getRawDataSize()
              || std::memcmp (before[i].message.getRawData(), after[i].message.getRawData(),
                              (size_t) before[i].message.getRawDataSize()) != 0)
            ++differentBytes;
    }

    CHECK_MSG (worst <= tolerance, "a Generic message moved " + juce::String (worst, 1) + " samples");
    CHECK (differentBytes == 0);
}

//==============================================================================
/*  12 "Header disambiguation": without the LUTHIER chunk a file is Generic, and
    nobody is warned. */
LUTHIER_TEST (MidiExport, headerStrippedFileLoadsAsGenericWithoutWarning)
{
    const auto source = makeLoosePerformance();
    const auto bytes = MidiProfiles::exportToMemory (source, MidiExportOptions {});

    const auto stripped = rewrite (bytes, [] (size_t track, size_t index, const MidiProfiles::SmfEvent&, const juce::uint8*)
    {
        return ! (track == 0 && index == 0);
    });

    CHECK (containsText (bytes, "profile=luthier"));
    CHECK (stripped.getSize() < bytes.getSize());
    CHECK (! containsText (stripped, "profile=luthier"));

    MidiPerformance back;
    const auto result = MidiProfiles::importFromMemory (stripped.getData(), stripped.getSize(), back, kSr);

    CHECK_MSG (result.ok, result.error);
    CHECK (result.detectedProfile == MidiProfile::generic);
    CHECK_MSG (result.warnings.isEmpty(), result.warnings.joinIntoString ("; "));
    CHECK (result.error.isEmpty());
    CHECK (back.getMessages().size() == source.getMessages().size());
    CHECK (back.getEvents().empty());
}

//==============================================================================
/*  12 "Import corrupt file": every byte flipped, one at a time. Import either
    reads the file or refuses it with the byte it went wrong at; it never
    crashes. */
LUTHIER_TEST (MidiExport, everyFlippedByteIsRefusedGracefully)
{
    const auto bytes = MidiProfiles::exportToMemory (makeLoosePerformance(), MidiExportOptions {});
    const auto size = bytes.getSize();

    int refused = 0, loaded = 0, unnamed = 0;
    juce::String firstUnnamed;

    for (size_t i = 0; i < size; ++i)
    {
        juce::MemoryBlock damaged (bytes);
        static_cast<juce::uint8*> (damaged.getData())[i] ^= 0xFF;

        MidiPerformance back;
        const auto result = MidiProfiles::importFromMemory (damaged.getData(), damaged.getSize(), back, kSr);

        if (result.ok)
        {
            ++loaded;
            continue;
        }

        ++refused;

        if (! result.error.startsWith ("Byte 0x") || result.errorByteOffset < 0
              || result.errorByteOffset > (juce::int64) size)
        {
            ++unnamed;

            if (firstUnnamed.isEmpty())
                firstUnnamed = "flip at " + juce::String ((juce::int64) i) + ": " + result.error;
        }
    }

    CHECK ((size_t) (refused + loaded) == size);
    CHECK_MSG (unnamed == 0, juce::String (unnamed) + " refusals without a byte reference, e.g. " + firstUnnamed);
    CHECK_MSG (refused > (int) size / 4, juce::String (refused) + " of " + juce::String ((juce::int64) size)
                                           + " flips refused");

    // Some flips must be refused: the file's signature, the header's text and
    // an extension event's SysEx.
    const auto smf = parse (bytes);
    std::vector<size_t> mustRefuse { 0, smf.tracks[0].events[0].dataOffset + 12 };

    for (const auto& track : smf.tracks)
        for (const auto& event : track.events)
            if (event.isSysEx() && mustRefuse.size() == 2)
                mustRefuse.push_back (event.dataOffset + 6);

    CHECK (mustRefuse.size() == 3);

    for (const auto offset : mustRefuse)
    {
        juce::MemoryBlock damaged (bytes);
        static_cast<juce::uint8*> (damaged.getData())[offset] ^= 0xFF;

        MidiPerformance back;
        const auto result = MidiProfiles::importFromMemory (damaged.getData(), damaged.getSize(), back, kSr);

        CHECK_MSG (! result.ok, "a flip at byte " + juce::String ((juce::int64) offset) + " was accepted");
    }
}

/*  2.3: a flipped bit that keeps the text legal is still caught - by the
    checksum in the SysEx copy, or by the copies disagreeing. */
LUTHIER_TEST (MidiExport, damagedExtensionEventsAreRefused)
{
    const auto source = makeLoosePerformance();

    for (const bool sysEx : { true, false })
    {
        MidiExportOptions options;
        options.sysExRedundancy = sysEx;

        const auto bytes = MidiProfiles::exportToMemory (source, options);
        const auto smf = parse (bytes);
        const auto* data = static_cast<const juce::uint8*> (bytes.getData());

        size_t sysExClassByte = 0, textClassByte = 0;

        for (const auto& track : smf.tracks)
        {
            for (const auto& event : track.events)
            {
                if (sysExClassByte == 0 && event.isSysEx()
                      && LuthierEvents::isLuthierSysEx (data + event.dataOffset, (int) event.dataSize))
                    sysExClassByte = event.dataOffset + 4;    // the class name's first letter

                if (textClassByte == 0 && metaText (event, data).startsWith (LuthierEvents::kTextPrefix))
                    textClassByte = event.dataOffset + 9;     // "LUTHIER: " is nine bytes
            }
        }

        CHECK (textClassByte > 0);
        CHECK (sysEx == (sysExClassByte > 0));

        for (const auto offset : { sysExClassByte, textClassByte })
        {
            if (offset == 0)
                continue;

            juce::MemoryBlock damaged (bytes);
            static_cast<juce::uint8*> (damaged.getData())[offset] ^= 0x01;

            MidiPerformance back;
            const auto result = MidiProfiles::importFromMemory (damaged.getData(), damaged.getSize(), back, kSr);

            CHECK_MSG (! result.ok, "a one-bit change at byte " + juce::String ((juce::int64) offset)
                                      + " was accepted (SysEx " + (sysEx ? "on" : "off") + ")");
            CHECK (result.error.startsWith ("Byte 0x"));
        }
    }
}

//==============================================================================
/*  9 and 10: an advanced range warns before it is applied; a newer schema is
    read as far as it can be and the rest kept; missing fields are listed. */
LUTHIER_TEST (MidiExport, importWarnsOfAdvancedRangesAndNewerSchemas)
{
    MidiPerformance source (kSr);
    source.addMessage (0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100));
    source.addMessage (4800, juce::MidiMessage::noteOff (1, 60));

    auto range = LuthierEvent::make (LuthierEventClass::ranges, 100);
    range.set ("param", "stringTension").setInt ("on", 1).setReal ("value", 1.8).setReal ("min", 0.5).setReal ("max", 1.5);
    source.addEvent (range);

    auto newer = LuthierEvent::make (LuthierEventClass::strum, 200);
    newer.schemaVersion = 2;
    newer.set ("dir", "up").set ("spin", "left");
    source.addEvent (newer);

    MidiPerformance back;
    const auto result = roundTrip (source, MidiExportOptions {}, back);
    const auto warnings = result.warnings.joinIntoString ("\n");

    CHECK_MSG (result.ok, result.error);
    CHECK_MSG (warnings.contains ("Advanced range") && warnings.contains ("stringTension"), warnings);
    CHECK_MSG (warnings.contains ("STRUM events use schema 2"), warnings);
    CHECK (result.defaultedFields.contains ("STRUM.cv"));

    // The field schema 1 does not know survives another save and load.
    MidiPerformance again;
    const auto second = roundTrip (back, MidiExportOptions {}, again);
    juce::String why;

    CHECK_MSG (second.ok, second.error);
    CHECK_MSG (source.isEquivalentTo (again, &why), why);

    for (const auto& event : again.getEvents())
        if (event.eventClass == LuthierEventClass::strum)
            CHECK (event.get ("spin") == "left");
}

/*  0.4: a file read at another sample rate keeps its beats, to the tick. */
LUTHIER_TEST (MidiExport, anotherSampleRateFallsBackToTheTick)
{
    const auto source = makeLoosePerformance();
    const double otherRate = 44100.0;

    MidiPerformance back;
    const auto result = roundTrip (source, MidiExportOptions {}, back, otherRate);

    CHECK_MSG (result.ok, result.error);
    CHECK (result.warnings.joinIntoString (" ").contains ("44100"));
    CHECK (back.getMessages().size() == source.getMessages().size());

    const double tolerance = 0.5 * otherRate * 60.0 / (97.5 * MidiExportOptions::kDefaultPpq) + 1.0;
    double worst = 0.0;

    for (size_t i = 0; i < juce::jmin (back.getMessages().size(), source.getMessages().size()); ++i)
    {
        const double expected = (double) source.getMessages()[i].sample * otherRate / kSr;
        worst = juce::jmax (worst, std::abs ((double) back.getMessages()[i].sample - expected));
    }

    CHECK_MSG (worst <= tolerance, "timing moved " + juce::String (worst, 1) + " samples at 44.1 kHz");
}

//==============================================================================
/*  7 and 8: a .midprofile holds an export configuration and loads back. */
LUTHIER_TEST (MidiExport, midprofileSavesAndLoads)
{
    MidiExportOptions options;
    options.profile = MidiProfile::generic;
    options.ppq = 480;
    options.split = MidiTrackSplit::perString;
    options.includeRealism = true;
    options.sysExRedundancy = false;
    options.stripIdentifiers = true;
    options.setClassIncluded (LuthierEventClass::squeak, false);

    auto file = juce::File::createTempFile (MidiProfiles::kProfileExtension);

    CHECK (MidiProfiles::saveProfile (file, options, "Stems for mixing"));

    MidiExportOptions loaded;
    juce::String name, error;
    juce::StringArray warnings;

    CHECK_MSG (MidiProfiles::loadProfile (file, loaded, &name, &error, &warnings), error);
    CHECK (name == "Stems for mixing");
    CHECK (loaded.profile == MidiProfile::generic);
    CHECK (loaded.ppq == 480);
    CHECK (loaded.split == MidiTrackSplit::perString);
    CHECK (loaded.includeRealism);
    CHECK (! loaded.sysExRedundancy);
    CHECK (loaded.stripIdentifiers);
    CHECK (! loaded.includesClass (LuthierEventClass::squeak));
    CHECK (loaded.includesClass (LuthierEventClass::strum));
    CHECK (loaded.classMask == options.classMask);
    CHECK (warnings.isEmpty());

    const auto json = juce::JSON::parse (file.loadFileAsString());
    CHECK (json["magic"].toString() == MidiProfiles::kProfileMagic);
    CHECK ((int) json["schema"] == MidiProfiles::kProfileSchema);

    // A PPQ out of range is clamped; an unknown class is a warning, not a failure.
    file.replaceWithText (R"({"magic":"luthier.midprofile","schema":1,"config":{"ppq":20000,"classes":["NOTE","NOT_A_CLASS"]}})");
    warnings.clear();

    CHECK_MSG (MidiProfiles::loadProfile (file, loaded, &name, &error, &warnings), error);
    CHECK (loaded.ppq == MidiExportOptions::kMaxPpq);
    CHECK (warnings.size() == 1);
    CHECK (loaded.includesClass (LuthierEventClass::note));
    CHECK (! loaded.includesClass (LuthierEventClass::strum));

    // Not a profile at all: refused, with a reason.
    file.replaceWithText (R"({"magic":"something.else","schema":1,"config":{}})");
    error.clear();

    CHECK (! MidiProfiles::loadProfile (file, loaded, &name, &error, &warnings));
    CHECK (error.isNotEmpty());

    file.deleteFile();
}

/*  7: the classes a profile leaves out are not written. */
LUTHIER_TEST (MidiExport, classMaskLimitsWhatIsWritten)
{
    const auto source = makeLoosePerformance();

    MidiExportOptions options;
    options.setClassIncluded (LuthierEventClass::strum, false);

    MidiPerformance back;
    const auto result = roundTrip (source, options, back);

    CHECK_MSG (result.ok, result.error);
    CHECK (back.countEvents (LuthierEventClass::strum) == 0);
    CHECK (back.countEvents (LuthierEventClass::squeak) == source.countEvents (LuthierEventClass::squeak));
    CHECK (back.getMessages().size() == source.getMessages().size());
}

/*  11: strip identifiers leaves no guitar name, preset name or seed behind,
    and the import says which fields it had to default. */
LUTHIER_TEST (MidiExport, stripIdentifiersLeavesNoNamesOrSeeds)
{
    const auto source = makeLoosePerformance();

    const auto kept = MidiProfiles::exportToMemory (source, MidiExportOptions {});
    CHECK (containsText (kept, "Tele"));
    CHECK (containsText (kept, "4815162342"));

    MidiExportOptions options;
    options.stripIdentifiers = true;

    const auto stripped = MidiProfiles::exportToMemory (source, options);
    CHECK (! containsText (stripped, "Tele"));
    CHECK (! containsText (stripped, "Glassy"));
    CHECK (! containsText (stripped, "4815162342"));

    MidiPerformance back;
    const auto result = MidiProfiles::importFromMemory (stripped.getData(), stripped.getSize(), back, kSr);

    CHECK_MSG (result.ok, result.error);
    CHECK (back.getMeta().guitarName.isEmpty());
    CHECK (back.getMeta().presetName.isEmpty());
    CHECK (back.getMeta().characterSeed.isEmpty());
    CHECK (result.defaultedFields.contains ("CHARACTER.seed"));
    CHECK (back.getMessages().size() == source.getMessages().size());
}

//==============================================================================
/*  9: notation and MIDI are two serialisations of one performance. A score
    goes out and comes back with its strings, frets and - in the Luthier
    profile - its techniques. */
LUTHIER_TEST (MidiExport, aScoreSurvivesBothProfiles)
{
    const auto score = makeScore();
    const auto original = flatten (score);

    CHECK (original.size() == 4);

    const auto performance = MidiPerformance::fromScore (score, kSr);

    CHECK (performance.countEvents (LuthierEventClass::note) == 4);
    CHECK (performance.countEvents (LuthierEventClass::bend) == 1);
    CHECK (performance.countEvents (LuthierEventClass::vibrato) == 1);
    CHECK (performance.countEvents (LuthierEventClass::slide) == 1);

    int wheels = 0;

    for (const auto& entry : performance.getMessages())
        if (entry.message.isPitchWheel())
            ++wheels;

    CHECK_MSG (wheels == 4, juce::String (wheels) + " pitch-bend messages for a three-point bend and its reset");

    for (const auto profile : { MidiProfile::luthier, MidiProfile::generic })
    {
        const juce::String name (MidiProfiles::getProfileName (profile));

        MidiExportOptions options;
        options.profile = profile;

        MidiPerformance back;
        const auto result = roundTrip (performance, options, back);

        CHECK_MSG (result.ok, name + ": " + result.error);

        PerformanceScore rebuilt;
        back.toScore (rebuilt);

        const auto notes = flatten (rebuilt);

        CHECK_MSG (notes.size() == original.size(), name + ": " + juce::String ((int) notes.size()) + " notes");
        CHECK (rebuilt.getMeta().title == "Score fixture");

        for (size_t i = 0; i < juce::jmin (notes.size(), original.size()); ++i)
        {
            const auto& a = original[i];
            const auto& b = notes[i];
            const auto which = name + " note " + juce::String ((int) i);

            CHECK_MSG (a.string == b.string && a.fret == b.fret && a.midi == b.midi,
                       which + " moved string, fret or pitch");
            CHECK_NEAR (b.start, a.start, 1.0e-6);
            CHECK_NEAR (b.duration, a.duration, 1.0e-3);
            CHECK_NEAR (b.velocity, a.velocity, 1.0 / 127.0);

            if (profile == MidiProfile::luthier)
                CHECK_MSG (describeTechniques (b.techniques) == describeTechniques (a.techniques),
                           which + ": " + describeTechniques (b.techniques) + " against "
                             + describeTechniques (a.techniques));
            else
                CHECK_MSG (b.techniques.empty(), which + " has techniques from a Generic file");
        }
    }
}

//==============================================================================
/*  4.1: ranges. A section, or the last N seconds, sounds as it did in place:
    controllers restated at the start, notes cut at the end closed. */
LUTHIER_TEST (MidiExport, extractRangeRestatesStateAndClosesNotes)
{
    MidiPerformance source (kSr);
    source.setTempo (120.0);

    source.addMessage (100, juce::MidiMessage::controllerEvent (1, 64, 127));
    source.addMessage (200, juce::MidiMessage::pitchWheel (1, 9000));
    source.addMessage (1000, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100));    // began before
    source.addMessage (5000, juce::MidiMessage::noteOff (1, 60));
    source.addMessage (6000, juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100));    // runs past the end
    source.addMessage (20000, juce::MidiMessage::noteOff (1, 62));
    source.addMessage (7000, juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100));    // wholly inside
    source.addMessage (8000, juce::MidiMessage::noteOff (1, 64));

    int index = 0;

    for (const auto& [at, name] : { std::make_pair ((juce::int64) 0, "A"),
                                    std::make_pair ((juce::int64) 4000, "B"),
                                    std::make_pair ((juce::int64) 10000, "C") })
    {
        auto section = LuthierEvent::make (LuthierEventClass::section, at);
        section.set ("name", name).setInt ("index", index++).set ("edge", "start");
        source.addEvent (section);
    }

    CHECK (source.getSectionNames().joinIntoString (",") == "A,B,C");

    const auto range = source.getSectionRange ("B");
    CHECK (range.getStart() == 4000 && range.getEnd() == 10000);

    const auto extract = source.extractRange (range);
    const auto& messages = extract.getMessages();

    CHECK_MSG (messages.size() == 6, juce::String ((int) messages.size()) + " messages in the extract");

    if (messages.size() == 6)
    {
        CHECK (messages[0].sample == 0 && messages[0].message.isController()
                 && messages[0].message.getControllerNumber() == 64 && messages[0].message.getControllerValue() == 127);
        CHECK (messages[1].sample == 0 && messages[1].message.isPitchWheel()
                 && messages[1].message.getPitchWheelValue() == 9000);
        CHECK (messages[2].sample == 2000 && messages[2].message.isNoteOn() && messages[2].message.getNoteNumber() == 62);
        CHECK (messages[3].sample == 3000 && messages[3].message.getNoteNumber() == 64);
        CHECK (messages[4].sample == 4000 && messages[4].message.isNoteOff());
        CHECK (messages[5].sample == 6000 && messages[5].message.isNoteOff()
                 && messages[5].message.getNoteNumber() == 62);
    }

    for (const auto& entry : messages)
        CHECK (entry.message.getNoteNumber() != 60 || ! (entry.message.isNoteOn() || entry.message.isNoteOff()));

    CHECK (extract.getEvents().size() == 1);
    CHECK (extract.getSectionNames().joinIntoString (",") == "B");

    const auto lastTenth = source.getLastSecondsRange (0.1);
    CHECK (lastTenth.getEnd() == source.getLengthInSamples());
    CHECK (lastTenth.getLength() == 4800);

    // The export dialog's range goes through the same extract.
    MidiExportOptions options;
    options.range = range;

    MidiPerformance back;
    const auto result = roundTrip (source, options, back);
    juce::String why;

    CHECK_MSG (result.ok, result.error);
    CHECK_MSG (extract.isEquivalentTo (back, &why), why);
}

/*  4.1: the preview names the opening bar. */
LUTHIER_TEST (MidiExport, previewDescribesTheOpeningBar)
{
    const auto performance = MidiPerformance::fromScore (makeScore(), kSr);
    const auto preview = MidiProfiles::describeOpeningBar (performance, MidiExportOptions {});

    CHECK_MSG (preview.contains ("4/4") && preview.contains ("120.0 bpm"), preview);
    CHECK_MSG (preview.contains ("4 notes (E2 D4 E4 E5)"), preview);
    CHECK_MSG (preview.contains ("NOTE x4"), preview);

    MidiExportOptions generic;
    generic.profile = MidiProfile::generic;

    CHECK_MSG (! MidiProfiles::describeOpeningBar (performance, generic).contains ("NOTE"),
               "the Generic preview lists realism that will not be written");
}

//==============================================================================
/*  12 "Drag-out": the file a drag hands the host is valid MIDI. */
LUTHIER_TEST (MidiExport, dragOutWritesAValidMidiFile)
{
    const auto performance = makeLoosePerformance();

    for (const bool alt : { false, true })
    {
        const auto file = MidiProfiles::writeDragOutFile (performance, alt, MidiExportOptions {});

        CHECK (file.existsAsFile());
        CHECK (file.hasFileExtension (".mid"));

        // The host's side: a standard reader takes it.
        {
            juce::FileInputStream in (file);
            juce::MidiFile midi;

            CHECK (in.openedOk() && midi.readFrom (in));
            CHECK (midi.getNumTracks() >= 2);
        }

        MidiPerformance back;
        const auto result = MidiProfiles::importFromFile (file, back, kSr);

        CHECK_MSG (result.ok, result.error);
        CHECK (result.detectedProfile == (alt ? MidiProfile::generic : MidiProfile::luthier));

        if (! alt)
        {
            juce::String why;
            CHECK_MSG (performance.isEquivalentTo (back, &why), why);
        }

        file.deleteFile();
    }
}

/*  practice-tools 8 / 4.2: the session recorder's capture is a performance at
    the samples it was played. */
LUTHIER_TEST (MidiExport, captureBecomesAPerformanceAtItsOwnSamples)
{
    MidiCapture capture;
    capture.prepare (kSr, 10.0);

    juce::MidiBuffer first;
    first.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 17);
    first.addEvent (juce::MidiMessage::controllerEvent (1, 1, 50), 200);
    capture.capture (first, 1000);

    juce::MidiBuffer later;
    later.addEvent (juce::MidiMessage::noteOff (1, 60), 5);
    capture.capture (later, 9000);

    const auto performance = MidiPerformance::fromCapture (capture, kSr, 120.0);
    const auto& messages = performance.getMessages();

    CHECK (messages.size() == 3);

    if (messages.size() == 3)
    {
        CHECK (messages[0].sample == 0 && messages[0].message.isNoteOn());
        CHECK (messages[1].sample == 183 && messages[1].message.isController());
        CHECK (messages[2].sample == 7988 && messages[2].message.isNoteOff());
    }
}

//==============================================================================
/*  12 "Luthier profile round trip": for every factory preset, export -> import
    -> render matches the source render within -60 dBFS RMS null. */
LUTHIER_TEST (MidiExport, luthierRoundTripNullsEveryFactoryPreset)
{
    ExportHarness harness;
    FactoryPresets::setProcessorForRanges (&harness);
    harness.prepareToPlay (kSr, kBlock);

    const auto source = makeRenderPerformance();

    MidiPerformance imported;
    const auto result = roundTrip (source, MidiExportOptions {}, imported);
    juce::String why;

    CHECK_MSG (result.ok, result.error);
    CHECK_MSG (source.isEquivalentTo (imported, &why), why);

    const int count = FactoryPresets::getNumPresets();
    CHECK (count > 0);

    int rendered = 0;

    for (int i = 0; i < count; ++i)
    {
        const auto& def = FactoryPresets::getPreset (i);
        const juce::String name (def.name);

        if (! harness.presets.fromVar (FactoryPresets::toVar (def, harness)))
        {
            CHECK_MSG (false, name + " failed to load");
            continue;
        }

        silenceRandomness (harness);

        const auto original = renderPerformance (harness, source, kRenderSeconds);
        const auto reimported = renderPerformance (harness, imported, kRenderSeconds);

        CHECK_MSG (rmsDbfs (original) > -120.0, name + " was silent");

        const double null = nullDbfs (original, reimported);

        if (null >= -60.0)
        {
            // Tell an export fault from an engine that does not repeat itself.
            const auto again = renderPerformance (harness, source, kRenderSeconds);

            CHECK_MSG (false, name + ": the round trip nulls at only " + juce::String (null, 1)
                                + " dBFS RMS; the same performance rendered twice nulls at "
                                + juce::String (nullDbfs (original, again), 1) + " dBFS RMS");
        }

        ++rendered;
        harness.engine.panic();
    }

    CHECK (rendered == count);
}

/*  12 "Generic profile round trip": pitch, velocity, bend and CC content
    within -30 dBFS RMS null; the realism is lost by design. */
LUTHIER_TEST (MidiExport, genericRoundTripNullsWithinThirtyDb)
{
    ExportHarness harness;
    FactoryPresets::setProcessorForRanges (&harness);
    harness.prepareToPlay (kSr, kBlock);
    silenceRandomness (harness);

    const auto source = MidiPerformance::fromScore (makeScore(), kSr);

    MidiExportOptions options;
    options.profile = MidiProfile::generic;
    options.includeRealism = true;

    MidiPerformance imported;
    const auto result = roundTrip (source, options, imported);

    CHECK_MSG (result.ok, result.error);
    CHECK (imported.getEvents().empty());
    CHECK (imported.getMessages().size() == source.getMessages().size());

    const auto original = renderPerformance (harness, source, 2.5);
    const auto reimported = renderPerformance (harness, imported, 2.5);

    CHECK_MSG (rmsDbfs (original) > -120.0, "the score fixture rendered silent");

    const double null = nullDbfs (original, reimported);
    CHECK_MSG (null < -30.0, "the Generic round trip nulls at only " + juce::String (null, 1) + " dBFS RMS");
}

//==============================================================================
/*  12 "Live MIDI-out timing": ten thousand events across every source - host
    pass-through, the rhythm engine, tune-builder playback scheduled in beats,
    string activity, CC broadcast, and character and workshop SysEx - come out
    on their own samples. Beat-scheduled events land within a sample of the
    exact position; everything else is exact. */
LUTHIER_TEST (MidiExport, liveMidiOutKeepsTenThousandEventsOnTheirSample)
{
    constexpr int kMaxBlock = 1024;

    MidiOutRouter router;
    router.prepare (kSr, kMaxBlock);

    MidiOutConfig cfg;
    cfg.enabled = true;
    cfg.passThrough = true;
    cfg.rhythmEngine = true;
    cfg.stringActivity = true;
    cfg.ccBroadcast = true;
    cfg.macroCc[0] = 20;
    cfg.macroCc[1] = 21;
    cfg.channel = 3;

    LuthierSysExOut sysEx;
    StringActivityQueue activity;
    juce::Random rng (0xF0220);

    // The tune: a note every 0.01 to 0.21 beats, at 131 bpm.
    const double bpm = 131.0;
    std::vector<std::pair<double, int>> tune;
    double beat = 0.013;

    for (int i = 0; i < 4000; ++i)
    {
        tune.emplace_back (beat, 36 + (i % 48));
        beat += 0.01 + rng.nextDouble() * 0.2;
    }

    size_t tuneCursor = 0;
    std::array<float, 2> macroValue { 0.0f, 0.0f };
    std::array<float, 2> lastSent { -1.0f, -1.0f };

    juce::int64 blockStart = 0;
    double blockStartPpq = 0.0;

    int checked = 0, mismatchedBlocks = 0, tuneEvents = 0, sysExEvents = 0;
    double worstBeatError = 0.0, worstPlayheadError = 0.0;
    juce::String firstMismatch;

    auto key = [] (int offset, const juce::MidiMessage& message)
    {
        return juce::String (offset).paddedLeft ('0', 5) + ":"
                 + juce::String::toHexString (message.getRawData(), message.getRawDataSize(), 0);
    };

    while (checked < 10000)
    {
        const int numSamples = 1 + rng.nextInt (kMaxBlock);
        juce::StringArray expected;

        // ---- the host's own events, echoed --------------------------------------------
        juce::MidiBuffer host;

        for (int e = 0, n = rng.nextInt (6); e < n; ++e)
            host.addEvent (juce::MidiMessage::noteOn (1 + rng.nextInt (16), rng.nextInt (128),
                                                      (juce::uint8) (1 + rng.nextInt (127))),
                           rng.nextInt (numSamples));

        for (const auto metadata : host)
            expected.add (key (metadata.samplePosition, metadata.getMessage()));

        router.captureInput (host);

        // ---- the rhythm engine -------------------------------------------------------
        auto& rhythm = router.getRhythmBuffer();

        for (int e = 0, n = rng.nextInt (4); e < n; ++e)
        {
            const auto message = juce::MidiMessage::controllerEvent (9, rng.nextInt (120), rng.nextInt (128));
            const int offset = rng.nextInt (numSamples);
            rhythm.addEvent (message, offset);
            expected.add (key (offset, message));
        }

        // ---- the tune-builder, scheduled in beats ---------------------------------------
        while (tuneCursor < tune.size())
        {
            const double at = tune[tuneCursor].first;

            // The song's own clock decides the sample; the block's playhead
            // must agree with it.
            const auto sample = LiveMidiClock::ppqToSample (at, 0, 0.0, bpm, kSr);
            const int offset = LiveMidiClock::offsetInBlock (sample, blockStart, numSamples);

            if (offset < 0)
                break;

            const double exact = LiveMidiClock::ppqToSampleExact (at, 0, 0.0, bpm, kSr);
            const double viaPlayhead = LiveMidiClock::ppqToSampleExact (at, blockStart, blockStartPpq, bpm, kSr);

            worstBeatError = juce::jmax (worstBeatError, std::abs ((double) (blockStart + offset) - exact));
            worstPlayheadError = juce::jmax (worstPlayheadError, std::abs (viaPlayhead - exact));

            const auto message = juce::MidiMessage::noteOn (15, tune[tuneCursor].second, (juce::uint8) 100);
            rhythm.addEvent (message, offset);
            expected.add (key (offset, message));

            ++tuneCursor;
            ++tuneEvents;
        }

        // ---- string activity ------------------------------------------------------------
        activity.clear();

        for (int e = 0, n = rng.nextInt (6); e < n; ++e)
        {
            StringActivityEvent a;
            a.sampleOffset = rng.nextInt (numSamples);
            a.stringIndex = rng.nextInt (6);
            a.midiNote = 40 + rng.nextInt (40);
            a.velocity = 0.5f;
            a.isNoteOn = rng.nextBool();
            activity.push (a);

            expected.add (key (a.sampleOffset, a.isNoteOn ? juce::MidiMessage::noteOn (cfg.channel, a.midiNote, a.velocity)
                                                          : juce::MidiMessage::noteOff (cfg.channel, a.midiNote)));
        }

        // ---- CC broadcast: a changed macro goes out on the block's first sample ------------
        for (size_t m = 0; m < 2; ++m)
        {
            if (rng.nextInt (4) == 0)
                macroValue[m] = (float) rng.nextInt (128) / 127.0f;

            router.setMacroValue ((int) m, macroValue[m]);

            const int coarse = juce::roundToInt (macroValue[m] * 127.0f);
            const int last = lastSent[m] < 0.0f ? -1 : juce::roundToInt (lastSent[m] * 127.0f);

            if (coarse != last)
            {
                expected.add (key (0, juce::MidiMessage::controllerEvent (cfg.channel, cfg.macroCc[m], coarse)));
                lastSent[m] = macroValue[m];
            }
        }

        // ---- character and workshop events, as Luthier SysEx -----------------------------
        for (int e = 0, n = rng.nextInt (3); e < n; ++e)
        {
            const int offset = rng.nextInt (numSamples);
            LuthierEvent event;

            if (rng.nextBool())
            {
                const double temp = 18.5 + rng.nextInt (10);
                CHECK (sysEx.push (LuthierEventClass::character, offset,
                                   { Field::makeWord ("what", "environment"), Field::makeReal ("temp", temp),
                                     Field::makeReal ("humidity", 40.25) }));

                event = LuthierEvent::make (LuthierEventClass::character, 0);
                event.set ("what", "environment").setReal ("temp", temp).setReal ("humidity", 40.25);
            }
            else
            {
                CHECK (sysEx.push (LuthierEventClass::workshop, offset,
                                   { Field::makeWord ("slot", "bridge"), Field::makeWord ("fit", "Wraparound 1"),
                                     Field::makeWord ("was", "ABR-1") }));

                event = LuthierEvent::make (LuthierEventClass::workshop, 0);
                event.set ("slot", "bridge").set ("fit", "Wraparound 1").set ("was", "ABR-1");
            }

            // The live encoder writes exactly what the file encoder writes.
            expected.add (key (offset, LuthierEvents::encodeSysEx (event, LuthierEvents::WriteOptions {})));
            ++sysExEvents;
        }

        // ---- emit, as processBlock does: the router, then the SysEx -----------------------
        juce::MidiBuffer out (host);
        router.emit (out, cfg, activity, numSamples);
        sysEx.appendTo (out, numSamples);

        juce::StringArray got;

        for (const auto metadata : out)
            got.add (key (metadata.samplePosition, metadata.getMessage()));

        // Each source is exact; the order of two sources' events on one sample
        // is not specified, so the comparison is of sorted lists.
        expected.sort (false);
        got.sort (false);

        if (expected != got)
        {
            ++mismatchedBlocks;

            if (firstMismatch.isEmpty())
                firstMismatch = "block at " + juce::String (blockStart) + ": expected "
                                  + expected.joinIntoString (" ") + " got " + got.joinIntoString (" ");
        }

        checked += got.size();

        blockStartPpq = LiveMidiClock::sampleToPpq (blockStart + numSamples, blockStart, blockStartPpq, bpm, kSr);
        blockStart += numSamples;
    }

    CHECK (checked >= 10000);
    CHECK (tuneEvents > 100);
    CHECK (sysExEvents > 100);
    CHECK_MSG (mismatchedBlocks == 0, juce::String (mismatchedBlocks) + " blocks differed; " + firstMismatch);
    CHECK_MSG (worstBeatError <= 1.0, "a beat-scheduled event landed " + juce::String (worstBeatError, 3)
                                        + " samples from its exact position");
    CHECK_MSG (worstPlayheadError <= 1.0, "the block playhead drifted " + juce::String (worstPlayheadError, 3)
                                            + " samples from the song clock");
    CHECK (sysEx.getDroppedCount() == 0);
    CHECK (router.getOverflowCount() == 0);
}

/*  12 "Live MIDI-out SysEx": a host that does not know Luthier drops the
    SysEx and loses nothing else; another Luthier instance reads it. */
LUTHIER_TEST (MidiExport, liveSysExIsDroppedByOtherHostsAndReadByLuthier)
{
    LuthierSysExOut out;
    juce::MidiBuffer block;

    const auto note = juce::MidiMessage::noteOn (1, 60, 0.8f);
    const auto wheel = juce::MidiMessage::pitchWheel (1, 9000);

    block.addEvent (note, 3);
    block.addEvent (wheel, 40);

    CHECK (out.push (LuthierEventClass::squeak, 10,
                     { Field::makeWord ("trigger", "shift"), Field::makeReal ("dur", 140.0),
                       Field::makeReal ("intensity", 0.6), Field::makeWord ("material", "phosphor") }));
    CHECK (out.push (LuthierEventClass::character, 40,
                     { Field::makeWord ("what", "seed"), Field::makeInt ("seed", -9007199254740993LL) }, 1));

    out.appendTo (block, 64);
    CHECK (out.getNumPending() == 0);

    // A host that does not know Luthier.
    juce::MidiBuffer plain;

    for (const auto metadata : block)
        if (! metadata.getMessage().isSysEx())
            plain.addEvent (metadata.getMessage(), metadata.samplePosition);

    std::vector<std::pair<int, juce::MidiMessage>> kept;

    for (const auto metadata : plain)
        kept.emplace_back (metadata.samplePosition, metadata.getMessage());

    CHECK (kept.size() == 2);

    if (kept.size() == 2)
    {
        CHECK (kept[0].first == 3 && kept[0].second.getRawDataSize() == note.getRawDataSize()
                 && std::memcmp (kept[0].second.getRawData(), note.getRawData(), (size_t) note.getRawDataSize()) == 0);
        CHECK (kept[1].first == 40 && kept[1].second.getRawDataSize() == wheel.getRawDataSize()
                 && std::memcmp (kept[1].second.getRawData(), wheel.getRawData(), (size_t) wheel.getRawDataSize()) == 0);
    }

    // Another Luthier instance.
    std::vector<std::pair<int, LuthierEvent>> received;

    for (const auto metadata : block)
    {
        const auto message = metadata.getMessage();

        if (! message.isSysEx())
            continue;

        LuthierEvent event;
        juce::int64 dt = 0;
        juce::String error;

        CHECK_MSG (LuthierEvents::decodeSysEx (message.getSysExData(), message.getSysExDataSize(), event, dt, error),
                   error);
        received.emplace_back (metadata.samplePosition, event);
    }

    CHECK (received.size() == 2);

    if (received.size() == 2)
    {
        const auto& squeak = received[0].second;

        CHECK (received[0].first == 10);
        CHECK (squeak.eventClass == LuthierEventClass::squeak);
        CHECK (squeak.get ("trigger") == "shift");
        CHECK_NEAR (squeak.getReal ("dur"), 140.0, 1.0e-9);
        CHECK_NEAR (squeak.getReal ("intensity"), 0.6, 1.0e-9);

        // 3's example line, word for word.
        LuthierEvents::WriteOptions text;
        text.writeTiming = false;

        CHECK_MSG (LuthierEvents::encodeText (squeak, text)
                     == "LUTHIER: SQUEAK trigger=shift dur=140 intensity=0.6 material=phosphor",
                   LuthierEvents::encodeText (squeak, text));

        const auto& character = received[1].second;

        CHECK (received[1].first == 40);
        CHECK (character.eventClass == LuthierEventClass::character);
        CHECK (character.part == 1);
        CHECK (character.getInt ("seed") == -9007199254740993LL);
    }

    // Someone else's SysEx is left alone rather than misread.
    const juce::uint8 roland[] = { 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41 };
    LuthierEvent notOurs;
    juce::int64 dt = 0;
    juce::String error;

    CHECK (! LuthierEvents::isLuthierSysEx (roland, (int) sizeof (roland)));
    CHECK (! LuthierEvents::decodeSysEx (roland, (int) sizeof (roland), notOurs, dt, error));
    CHECK (error.isNotEmpty());
}

/*  6: a block's playhead turns beats into samples and back. */
LUTHIER_TEST (MidiExport, liveClockPlacesBeatsOnTheirSample)
{
    // Beat 1 at 120 bpm is half a second: sample 24000.
    CHECK (LiveMidiClock::ppqToSample (1.0, 0, 0.0, 120.0, kSr) == 24000);

    // The same position from a later block's playhead.
    CHECK (LiveMidiClock::ppqToSample (1.0, 12000, 0.5, 120.0, kSr) == 24000);
    CHECK_NEAR (LiveMidiClock::sampleToPpq (24000, 12000, 0.5, 120.0, kSr), 1.0, 1.0e-12);

    // A block owns its first sample and not the one after its last.
    CHECK (LiveMidiClock::offsetInBlock (512, 512, 256) == 0);
    CHECK (LiveMidiClock::offsetInBlock (767, 512, 256) == 255);
    CHECK (LiveMidiClock::offsetInBlock (768, 512, 256) == -1);
    CHECK (LiveMidiClock::offsetInBlock (511, 512, 256) == -1);
}
