#pragma once

/*  Shared plumbing for the combination (beta-test) harness: CombinationTests.cpp
    and GuiReachabilityTests.cpp.

    It drives the real LuthierAudioProcessor - parameters, structural changes,
    presets, the rhythm engine, snapshots - exactly as a host would, renders
    short MIDI phrases through it, and measures what came out. The measurements
    are deliberately coarse (finite, bounded, present, decays, no runaway CPU):
    they are the questions a beta tester asks of every combination, not the
    per-module physics the unit tests already cover.

    Everything random here is seeded. A failure prints its seed and the exact
    settings, so it can be reproduced by running the one test again.
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Parameters.h"
#include "../Presets/PresetManager.h"
#include "../Capture/PerformanceCapture.h"

#include <chrono>
#include <cmath>
#include <map>
#include <random>
#include <thread>
#include <iomanip>

namespace luthier::combo
{

inline constexpr double kSr    = 48000.0;
inline constexpr int    kBlock = 256;

//==============================================================================
/** The MIDI phrases every combination is played with. */
enum class Phrase
{
    single, chord, bend, hammerOn, palmMute, harmonic, fastRepeat, sustainPedal,
    numPhrases
};

inline const char* phraseName (Phrase p)
{
    switch (p)
    {
        case Phrase::single:       return "single";
        case Phrase::chord:        return "chord";
        case Phrase::bend:         return "bend";
        case Phrase::hammerOn:     return "hammerOn";
        case Phrase::palmMute:     return "palmMute";
        case Phrase::harmonic:     return "harmonic";
        case Phrase::fastRepeat:   return "fastRepeat";
        case Phrase::sustainPedal: return "sustainPedal";
        default: break;
    }

    return "?";
}

struct TimedMidi
{
    int sample;
    juce::MidiMessage message;
};

/** The phrase's events, in sample time, and when its last note is released. */
inline std::vector<TimedMidi> makePhrase (Phrase phrase, int& releasedAt)
{
    using M = juce::MidiMessage;
    std::vector<TimedMidi> e;
    auto s = [] (double seconds) { return (int) (seconds * kSr); };

    switch (phrase)
    {
        case Phrase::single:
            e.push_back ({ 0, M::noteOn (1, 52, (juce::uint8) 100) });
            e.push_back ({ s (0.6), M::noteOff (1, 52) });
            releasedAt = s (0.6);
            break;

        case Phrase::chord:
        {
            const int notes[] = { 40, 47, 52, 56, 59, 64 };
            for (int n : notes) e.push_back ({ 0, M::noteOn (1, n, (juce::uint8) 96) });
            for (int n : notes) e.push_back ({ s (0.8), M::noteOff (1, n) });
            releasedAt = s (0.8);
            break;
        }

        case Phrase::bend:
            e.push_back ({ 0, M::noteOn (1, 60, (juce::uint8) 100) });
            for (int i = 0; i <= 20; ++i)
                e.push_back ({ s (0.1 + 0.015 * i), M::pitchWheel (1, 8192 + (int) (8191.0 * i / 20.0)) });
            for (int i = 0; i <= 20; ++i)
                e.push_back ({ s (0.5 + 0.015 * i), M::pitchWheel (1, 16383 - (int) (8191.0 * i / 20.0)) });
            e.push_back ({ s (0.85), M::noteOff (1, 60) });
            releasedAt = s (0.85);
            break;

        case Phrase::hammerOn:
            e.push_back ({ 0, M::noteOn (1, 55, (juce::uint8) 100) });
            e.push_back ({ s (0.20), M::noteOn (1, 57, (juce::uint8) 40) });
            e.push_back ({ s (0.21), M::noteOff (1, 55) });
            e.push_back ({ s (0.40), M::noteOn (1, 55, (juce::uint8) 40) });   // pull-off
            e.push_back ({ s (0.41), M::noteOff (1, 57) });
            e.push_back ({ s (0.70), M::noteOff (1, 55) });
            releasedAt = s (0.70);
            break;

        case Phrase::palmMute:
            e.push_back ({ 0, M::controllerEvent (1, 67, 127) });
            for (int i = 0; i < 6; ++i)
            {
                e.push_back ({ s (0.12 * i),        M::noteOn  (1, 40, (juce::uint8) 110) });
                e.push_back ({ s (0.12 * i + 0.1),  M::noteOff (1, 40) });
            }
            e.push_back ({ s (0.75), M::controllerEvent (1, 67, 0) });
            releasedAt = s (0.72);
            break;

        case Phrase::harmonic:
            e.push_back ({ 0, M::controllerEvent (1, 73, 127) });
            e.push_back ({ 1, M::noteOn (1, 64, (juce::uint8) 100) });
            e.push_back ({ s (0.3), M::controllerEvent (1, 72, 127) });           // pinch
            e.push_back ({ s (0.3) + 1, M::noteOn (1, 57, (juce::uint8) 120) });
            e.push_back ({ s (0.7), M::noteOff (1, 64) });
            e.push_back ({ s (0.7), M::noteOff (1, 57) });
            e.push_back ({ s (0.7) + 1, M::controllerEvent (1, 73, 0) });
            e.push_back ({ s (0.7) + 1, M::controllerEvent (1, 72, 0) });
            releasedAt = s (0.7);
            break;

        case Phrase::fastRepeat:
            for (int i = 0; i < 16; ++i)
            {
                e.push_back ({ s (0.04 * i),         M::noteOn  (1, 57, (juce::uint8) (70 + (i % 3) * 20)) });
                e.push_back ({ s (0.04 * i + 0.035), M::noteOff (1, 57) });
            }
            releasedAt = s (0.64);
            break;

        case Phrase::sustainPedal:
            e.push_back ({ 0, M::controllerEvent (1, 64, 127) });
            e.push_back ({ 1, M::noteOn (1, 45, (juce::uint8) 90) });
            e.push_back ({ 2, M::noteOn (1, 52, (juce::uint8) 90) });
            e.push_back ({ s (0.2), M::noteOff (1, 45) });
            e.push_back ({ s (0.2), M::noteOff (1, 52) });
            e.push_back ({ s (0.9), M::controllerEvent (1, 64, 0) });
            releasedAt = s (0.9);
            break;

        default:
            releasedAt = 0;
            break;
    }

    std::stable_sort (e.begin(), e.end(), [] (const TimedMidi& a, const TimedMidi& b) { return a.sample < b.sample; });
    return e;
}

//==============================================================================
/** What one render measured. */
struct RenderStats
{
    bool finite = true;
    int firstNonFinite = -1;
    double peak = 0.0;
    double maxWindowRms = 0.0;      ///< loudest 20 ms window up to the release
    double tailRms = 0.0;           ///< last 300 ms of the render
    double earlierTailRms = 0.0;    ///< the 300 ms one second before that: the tail must still be falling
    double idleRms = 0.0;           ///< the same rig with nothing played, just before: its noise floor
    int subnormals = 0;
    double meanBlockMs = 0.0;
    double maxBlockMs = 0.0;
    double tailMeanBlockMs = 0.0;   ///< the blocks after the release: a denormal storm shows here
    double cpuPercent = 0.0;        ///< mean block time over real time
    std::vector<float> mono;

    juce::String describe() const
    {
        return "finite=" + juce::String (finite ? "yes" : "NO")
             + " peak=" + juce::String (peak, 3)
             + " noteRms=" + juce::String (maxWindowRms, 5)
             + " tailRms=" + juce::String (tailRms, 6)
             + " idleRms=" + juce::String (idleRms, 6)
             + " subnormals=" + juce::String (subnormals)
             + " cpu=" + juce::String (cpuPercent, 1) + "%"
             + " maxBlockMs=" + juce::String (maxBlockMs, 2)
             + " tailBlockMs=" + juce::String (tailMeanBlockMs, 3);
    }
};

//==============================================================================
/** One processor, prepared, and the helpers to set it up and play it. */
struct Rig
{
    std::unique_ptr<LuthierAudioProcessor> processor;

    Rig()
    {
        processor = std::make_unique<LuthierAudioProcessor>();
        processor->prepareToPlay (kSr, kBlock);
    }

    LuthierAudioProcessor& p() { return *processor; }
    juce::AudioProcessorValueTreeState& state() { return processor->getState(); }

    juce::RangedAudioParameter* param (const juce::String& id)
    {
        return state().getParameter (id);
    }

    /** Number of discrete values for a choice or bool parameter, 0 for a float. */
    int numChoices (const juce::String& id)
    {
        auto* prm = param (id);

        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm)) return c->choices.size();
        if (dynamic_cast<juce::AudioParameterBool*> (prm) != nullptr)  return 2;
        if (auto* i = dynamic_cast<juce::AudioParameterInt*> (prm))    return i->getRange().getLength() + 1;
        return 0;
    }

    bool setPlain (const juce::String& id, float plain)
    {
        auto* prm = param (id);

        if (prm == nullptr)
            return false;

        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        return true;
    }

    bool setNormalised (const juce::String& id, float normalised)
    {
        auto* prm = param (id);

        if (prm == nullptr)
            return false;

        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
        return true;
    }

    /** Choice index (or int value, or 0/1 for a bool). */
    bool setIndex (const juce::String& id, int index)
    {
        auto* prm = param (id);

        if (prm == nullptr)
            return false;

        if (auto* i = dynamic_cast<juce::AudioParameterInt*> (prm))
            return setPlain (id, (float) (i->getRange().getStart() + index));

        return setPlain (id, (float) index);
    }

    /** Everything structural goes through the bridge on the message thread - what
        the AsyncUpdater would do a moment later in a host. */
    void apply()
    {
        processor->getParameterBridge().applyAllNow();
    }

    void processSilence (int blocks)
    {
        juce::AudioBuffer<float> buffer (bufferChannels(), kBlock);
        juce::MidiBuffer midi;

        for (int i = 0; i < blocks; ++i)
        {
            buffer.clear();
            processor->processBlock (buffer, midi);
        }
    }

    int bufferChannels() const
    {
        return juce::jmax (2, processor->getTotalNumInputChannels(), processor->getTotalNumOutputChannels());
    }

    /** Plays the phrase and then `tailSeconds` of nothing. `between` is called
        between blocks at the given sample (a preset switch mid-note, say). */
    RenderStats render (Phrase phrase, double tailSeconds,
                        std::function<void (int)> between = {}, int betweenAt = -1)
    {
        int releasedAt = 0;
        auto events = makePhrase (phrase, releasedAt);
        transposeIntoRange (events);

        // The rig's own floor first (hum, hiss, an amp at full gain), so the decay
        // check can tell a string that keeps ringing from a floor that was always there.
        const double idle = renderEvents ({}, 0, 0.25).tailRms;

        auto stats = renderEvents (events, releasedAt, tailSeconds, std::move (between), betweenAt);
        stats.idleRms = idle;
        return stats;
    }

    /** The lowest note the current guitar can sound: its lowest open string, or
        the capo on it (nothing is fretted at or behind a capo, RubricVoicer 4.5,
        so a capo at 12 moves the floor up an octave). */
    int lowestPlayableNote()
    {
        auto& engine = processor->getEngine();
        auto& tuning = engine.getTuningEngine();
        const auto open = PerformanceCapture::getOpenNotes (tuning, engine.getNumStrings());
        int lowest = 127;

        for (int s = 0; s < engine.getNumStrings(); ++s)
            if (open[(size_t) s] > 0)
                lowest = juce::jmin (lowest, open[(size_t) s] + tuning.getCapoFretFor (s));

        return lowest == 127 ? 0 : lowest;
    }

    /*  The phrases are written for a six-string in standard tuning. A pitch no
        string can sound is dropped by design (RubricVoicer, exact mode: "not the
        instrument's to play"), so a Nashville high-strung set, whose low strings
        are an octave up, heard nothing of a phrase built on E2. A player would
        play it an octave up, and so does the harness: whole octaves, so every
        interval and technique in the phrase is kept. */
    void transposeIntoRange (std::vector<TimedMidi>& events)
    {
        int lowestInPhrase = 128;

        for (auto& e : events)
            if (e.message.isNoteOn())
                lowestInPhrase = juce::jmin (lowestInPhrase, e.message.getNoteNumber());

        const int floor = lowestPlayableNote();
        int shift = 0;

        while (lowestInPhrase + shift < floor && lowestInPhrase + shift + 12 <= 127)
            shift += 12;

        if (shift == 0)
            return;

        for (auto& e : events)
            if (e.message.isNoteOnOrOff())
                e.message.setNoteNumber (juce::jlimit (0, 127, e.message.getNoteNumber() + shift));
    }

    RenderStats renderEvents (const std::vector<TimedMidi>& events, int releasedAt, double tailSeconds,
                              std::function<void (int)> between = {}, int betweenAt = -1)
    {
        RenderStats stats;

        const int total = releasedAt + (int) (tailSeconds * kSr);
        stats.mono.assign ((size_t) total, 0.0f);

        juce::AudioBuffer<float> buffer (bufferChannels(), kBlock);
        size_t next = 0;
        double blockMsSum = 0.0, tailMsSum = 0.0;
        int blocks = 0, tailBlocks = 0;
        bool didBetween = false;

        for (int pos = 0; pos < total; pos += kBlock)
        {
            const int n = juce::jmin (kBlock, total - pos);

            if (between && ! didBetween && betweenAt >= 0 && pos >= betweenAt)
            {
                between (pos);
                didBetween = true;
            }

            buffer.setSize (bufferChannels(), n, false, false, true);
            buffer.clear();

            juce::MidiBuffer midi;

            while (next < events.size() && events[next].sample < pos + n)
            {
                midi.addEvent (events[next].message, juce::jmax (0, events[next].sample - pos));
                ++next;
            }

            const auto t0 = std::chrono::steady_clock::now();
            processor->processBlock (buffer, midi);
            const auto t1 = std::chrono::steady_clock::now();

            const double ms = std::chrono::duration<double, std::milli> (t1 - t0).count();
            blockMsSum += ms;
            ++blocks;
            stats.maxBlockMs = juce::jmax (stats.maxBlockMs, ms);

            if (pos > releasedAt + (int) (0.3 * kSr))
            {
                tailMsSum += ms;
                ++tailBlocks;
            }

            const int outChannels = juce::jmin (2, buffer.getNumChannels());

            for (int i = 0; i < n; ++i)
            {
                float mix = 0.0f;

                for (int ch = 0; ch < outChannels; ++ch)
                {
                    const float v = buffer.getSample (ch, i);

                    if (! std::isfinite (v))
                    {
                        if (stats.finite)
                            stats.firstNonFinite = pos + i;

                        stats.finite = false;
                        continue;
                    }

                    if (std::fpclassify (v) == FP_SUBNORMAL)
                        ++stats.subnormals;

                    stats.peak = juce::jmax (stats.peak, (double) std::abs (v));
                    mix += v;
                }

                stats.mono[(size_t) (pos + i)] = mix / (float) outChannels;
            }
        }

        stats.meanBlockMs = blocks > 0 ? blockMsSum / blocks : 0.0;
        stats.tailMeanBlockMs = tailBlocks > 0 ? tailMsSum / tailBlocks : 0.0;
        stats.cpuPercent = 100.0 * stats.meanBlockMs / (1000.0 * kBlock / kSr);

        const int win = (int) (0.02 * kSr);

        for (int start = 0; start + win <= juce::jmax (win, releasedAt); start += win / 2)
            stats.maxWindowRms = juce::jmax (stats.maxWindowRms, windowRms (stats.mono, start, win));

        const int tailLen = juce::jmin (total, (int) (0.3 * kSr));
        stats.tailRms = windowRms (stats.mono, total - tailLen, tailLen);
        stats.earlierTailRms = windowRms (stats.mono, juce::jmax (0, total - tailLen - (int) kSr), tailLen);

        return stats;
    }

    static double windowRms (const std::vector<float>& x, int start, int len)
    {
        if (len <= 0 || start < 0 || (size_t) (start + len) > x.size())
            return 0.0;

        double sum = 0.0;

        for (int i = start; i < start + len; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];

        return std::sqrt (sum / len);
    }

    /** All notes off, silence, and every sound-holding state cleared. */
    void quiet()
    {
        juce::AudioBuffer<float> buffer (bufferChannels(), kBlock);
        juce::MidiBuffer midi;

        for (int ch = 1; ch <= 16; ++ch)
            midi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);

        buffer.clear();
        processor->processBlock (buffer, midi);
        processor->panic();
    }
};

//==============================================================================
/** The id-to-plain-value settings of one combination, printable. */
struct Config
{
    std::vector<std::pair<juce::String, int>> indices;   ///< choice/bool/int params by index
    std::vector<std::pair<juce::String, float>> plains;  ///< float params, plain value
    Phrase phrase = Phrase::single;
    juce::String extra;

    juce::String describe (Rig& rig) const
    {
        juce::StringArray parts;

        for (const auto& [id, idx] : indices)
        {
            juce::String text (idx);

            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (rig.param (id)))
                if (juce::isPositiveAndBelow (idx, c->choices.size()))
                    text = c->choices[idx];

            parts.add (id + "=" + text);
        }

        for (const auto& [id, v] : plains)
            parts.add (id + "=" + juce::String (v, 3));

        parts.add (juce::String ("phrase=") + phraseName (phrase));

        if (extra.isNotEmpty())
            parts.add (extra);

        return parts.joinIntoString (" ");
    }

    void applyTo (Rig& rig) const
    {
        for (const auto& [id, idx] : indices) rig.setIndex (id, idx);
        for (const auto& [id, v] : plains)   rig.setPlain (id, v);
        rig.apply();
    }
};

//==============================================================================
/** The pass/fail questions every combination is asked. Returns "" on a pass,
    otherwise the reason. */
struct Verdict
{
    bool expectSound = true;
    bool expectDecay = true;
    double peakCeiling = 4.0;       ///< +12 dBFS: anything above is runaway gain
    double cpuCeilingPercent = 60.0;
    bool checkIdleFloor = true;     ///< off for random rigs, whose noise controls are random too

    /*  The idle floor is judged against the rig's own playing level, not an
        absolute level. A real amp with gain and master both on 10 turns a
        single coil's mains hum into a buzz almost as loud as a played chord -
        the preamp's small-signal gain applies to the hum in full while the
        chord saturates - which is why players of such rigs use a gate. So an
        absolute ceiling (formerly -30 dBFS) fails physically honest extremes.
        Any rig: the floor may not reach the playing level (0 dB). A shipped
        factory sound: 20 dB under it (CombinationTests sets that). */
    double minSnrDb = 0.0;

    juce::String judge (const RenderStats& s) const
    {
        juce::StringArray why;

        if (! s.finite)
            why.add ("non-finite output at sample " + juce::String (s.firstNonFinite));

        if (s.peak > peakCeiling)
            why.add ("runaway gain: peak " + juce::String (s.peak, 2) + " > " + juce::String (peakCeiling, 1));

        if (s.subnormals > 64)
            why.add ("subnormal output samples: " + juce::String (s.subnormals));

        if (expectSound && s.maxWindowRms < 1.0e-4)
            why.add ("silent while notes were playing (noteRms " + juce::String (s.maxWindowRms, 7) + ")");

        if (expectDecay && s.finite && s.maxWindowRms > 1.0e-4)
        {
            /*  What a real guitar does after a note-off: the released string is
                damped within a few hundred ms (sustain-and-decay SUS-08, checked
                string by string in Combo.releasedStringIsDampedQuickly), but the
                open strings it excited through the bridge keep ringing at their
                own T60 - several seconds - 20-40 dB under the note until a hand
                mutes them (engine.md 5.6, string-interaction 0.2). So the mix is
                not asked to vanish; it is asked to be clearly down (20 dB under
                the note, or under -60 dBFS, or at the rig's own floor) and still
                falling (2 dB over the last second: a 12-string's coupled courses ring
                that long). Formerly 30 dB, which failed
                physically honest sympathetic ring. */
            const bool atFloor = s.tailRms < 1.0e-3 || (s.idleRms > 0.0 && s.tailRms < s.idleRms * 1.41);
            const bool down    = s.tailRms < s.maxWindowRms * 0.1;
            const bool falling = s.earlierTailRms <= 0.0 || s.tailRms < s.earlierTailRms * 0.794;
            const bool deepDown = s.tailRms < s.maxWindowRms * 0.05;   // 26 dB under: decayed, falling or not
            const bool quietEnough = atFloor || deepDown || (down && falling);
            if (! quietEnough)
                why.add ("does not decay after release (tail " + juce::String (juce::Decibels::gainToDecibels (s.tailRms), 1)
                         + " dBFS vs note " + juce::String (juce::Decibels::gainToDecibels (s.maxWindowRms), 1) + " dBFS, "
                         + juce::String (juce::Decibels::gainToDecibels (s.earlierTailRms), 1) + " dBFS a second earlier)");
        }

        if (checkIdleFloor && s.idleRms > 1.0e-3 && s.maxWindowRms > 1.0e-4   // below -60 dBFS nobody hears it
            && juce::Decibels::gainToDecibels (s.maxWindowRms / s.idleRms) < minSnrDb)
            why.add ("loud idle noise floor: " + juce::String (juce::Decibels::gainToDecibels (s.idleRms), 1)
                     + " dBFS with nothing played, " + juce::String (juce::Decibels::gainToDecibels (s.maxWindowRms / s.idleRms), 1)
                     + " dB under the playing level (minimum " + juce::String (minSnrDb, 0) + ")");

        if (s.cpuPercent > cpuCeilingPercent)
            why.add ("CPU " + juce::String (s.cpuPercent, 1) + "% of real time > " + juce::String (cpuCeilingPercent, 0) + "%");

        if (s.meanBlockMs > 0.05 && s.tailMeanBlockMs > 4.0 * s.meanBlockMs && s.tailMeanBlockMs > 0.5)
            why.add ("possible denormal storm: tail blocks " + juce::String (s.tailMeanBlockMs, 3)
                     + " ms vs mean " + juce::String (s.meanBlockMs, 3) + " ms");

        return why.joinIntoString ("; ");
    }
};

//==============================================================================
/** Findings are collected and printed as one block, in the shape
    docs/audit/BETA_TEST_REPORT.md quotes, then counted as test failures. */
struct FindingLog
{
    juce::String test;
    juce::StringArray lines;
    std::map<juce::String, int> bySymptom;

    void add (const juce::String& settings, const juce::String& symptom)
    {
        auto key = symptom.upToFirstOccurrenceOf (":", false, false)
                          .upToFirstOccurrenceOf ("(", false, false).trim();
        if (++bySymptom[key] <= 6)
            lines.add ("  [" + test + "] " + symptom + "\n      settings: " + settings);
    }

    void flush()
    {
        if (lines.isEmpty())
            return;

        std::cout << "\n---- combination findings: " << test << " ----\n";

        for (auto& l : lines)
            std::cout << l << "\n";

        for (auto& [k, n] : bySymptom)
            std::cout << "  summary: " << n << " x " << k << "\n";

        std::cout << std::flush;
    }
};

/** Settings are printed on failure; LUTHIER_COMBO_VERBOSE=1 prints every one. */
inline bool verbose()
{
    return juce::SystemStats::getEnvironmentVariable ("LUTHIER_COMBO_VERBOSE", {}).isNotEmpty();
}

/** Scales the long sweeps: LUTHIER_COMBO_SCALE=0.1 for a quick pass. */
inline double scale()
{
    const auto v = juce::SystemStats::getEnvironmentVariable ("LUTHIER_COMBO_SCALE", "1").getDoubleValue();
    return v > 0.0 ? v : 1.0;
}

} // namespace luthier::combo
