#pragma once

/*  The whole instrument, wired together (engine spec 1).

        MIDI -> interpreter -> technique -> tuning -> strings (+ coupling)
             -> body -> pickups -> cable -> pre-effects -> amp
             -> post-effects -> cabinet -> room -> master

    This class owns every module and nothing else owns any of them. The plugin
    processor talks to it through setters; the UI reads state back through const
    accessors. Everything inside processBlock is allocation-free.
*/

#include "DSP/String/StringEngine.h"
#include "DSP/Coupling/CouplingMatrix.h"
#include "DSP/Body/BodyEngine.h"
#include "DSP/Pickup/PickupEngine.h"
#include "DSP/Whammy/WhammyEngine.h"
#include "DSP/Cable/CableSim.h"
#include "DSP/Effects/EffectsChain.h"
#include "DSP/Effects/SecretEffect.h"
#include "DSP/Amp/AmpEngine.h"
#include "DSP/Amp/CabinetEngine.h"
#include "DSP/Amp/RoomEngine.h"
#include "DSP/Master/MasterBus.h"
#include "Model/Guitar/GuitarLibrary.h"
#include "Model/Playing/MidiInterpreter.h"
#include "Validator.h"
#include "Support/IrLibrary.h"
#include "Routing/TapBuffers.h"
#include "Routing/MidiOutRouter.h"
#include "Rhythm/RhythmEngine.h"
#include "Character/CharacterEngine.h"

#include <array>
#include <atomic>

namespace luthier
{

//==============================================================================
class LuthierEngine
{
public:
    LuthierEngine();
    ~LuthierEngine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void releaseResources();

    double getSampleRate() const noexcept { return sr; }

    //==========================================================================
    /** Loads a factory instrument: body, strings, pickups, tuning, amp and cab. */
    void setGuitarType (GuitarType type);
    GuitarType getGuitarType() const noexcept { return guitarType; }
    const GuitarSpec& getGuitarSpec() const noexcept { return spec; }

    void setTuningPreset (TuningPreset preset);
    void setStringMaterial (StringMaterial m);
    void setStringGauge (StringGauge g);
    void setStringAge (StringAge a);
    StringAge getStringAge() const noexcept { return stringAge; }
    void setCustomStringGauge (int stringIndex, double inches);
    double getCustomStringGauge (int stringIndex) const noexcept
    {
        return customGauges[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)];
    }

    /** Recomputes every string's physics from the current material, gauge, age,
        scale length and tuning. Called whenever any of those change. */
    void refreshStringPhysics();

    /** Loads the body IR that matches the current body configuration. If none
        is available the body engine is switched to modal synthesis, so the
        instrument never loses its body entirely. */
    void reloadBodyIr();

    /** Loads both cabinet IRs for the current cabinet configuration. */
    void reloadCabinetIrs();

    //==========================================================================
    // Sub-engine access, for the parameter layer and the UI.

    TuningEngine&    getTuningEngine() noexcept    { return tuning; }
    TechniqueEngine& getTechniqueEngine() noexcept { return technique; }
    ChordVoicer&     getChordVoicer() noexcept     { return voicer; }
    MidiInterpreter& getMidiInterpreter() noexcept { return midi; }
    CouplingMatrix&  getCouplingMatrix() noexcept  { return coupling; }
    BodyEngine&      getBodyEngine() noexcept      { return body; }
    PickupEngine&    getPickupEngine() noexcept    { return pickups; }
    WhammyEngine&    getWhammyEngine() noexcept    { return whammy; }
    CableSim&        getCableSim() noexcept        { return cable; }
    EffectsChain&    getPreEffects() noexcept      { return preEffects; }
    AmpEngine&       getAmpEngine() noexcept       { return amp; }
    EffectsChain&    getPostEffects() noexcept     { return postEffects; }
    CabinetEngine&   getCabinetEngine() noexcept   { return cabinet; }
    RoomEngine&      getRoomEngine() noexcept      { return room; }
    MasterBus&       getMasterBus() noexcept       { return master; }
    Validator&       getValidator() noexcept       { return validator; }

    StringEngine& getString (int i) noexcept { return strings[(size_t) juce::jlimit (0, kMaxStrings - 1, i)]; }
    const StringEngine& getString (int i) const noexcept { return strings[(size_t) juce::jlimit (0, kMaxStrings - 1, i)]; }

    int getNumStrings() const noexcept { return numStrings; }
    void setNumStrings (int n);

    //==========================================================================
    // Instrument-level controls.

    void setBodyAmount (double amount) noexcept;
    void setPluckPosition (double position) noexcept;
    void setPickMaterial (Excitation::Material m) noexcept { pickMaterial = m; }
    void setPickThickness (double t) noexcept { pickThickness = juce::jlimit (0.0, 1.0, t); }
    void setPickAngle (double a) noexcept { pickAngle = juce::jlimit (0.0, 1.0, a); }
    void setNailVsFlesh (double n) noexcept { nailVsFlesh = juce::jlimit (0.0, 1.0, n); }
    void setAttackBrightness (double b) noexcept { attackBrightness = juce::jlimit (0.0, 1.0, b); }
    void setUseFingers (bool useFingers) noexcept;

    void setFretless (bool f) noexcept;
    bool isFretless() const noexcept { return fretless; }

    void setFretAction (double mm) noexcept;
    void setFretBuzzAmount (double amount) noexcept;

    void setNoiseAmounts (double slide, double fret, double release, double bodyKnock, double pickAttack) noexcept;
    void setAmpBuzzAmount (double amount) noexcept;

    void setVibratoRate (double hz) noexcept { vibratoRate = juce::jlimit (0.5, 12.0, hz); }
    void setVibratoShape (Lfo::Shape s) noexcept;
    void setVibratoDepthCents (double cents) noexcept { vibratoDepthCents = juce::jlimit (0.0, 100.0, cents); }

    /** E-Bow style infinite sustain. Implemented the way an E-Bow works: the
        string is driven at its own resonance up to a target level, so it sustains
        without the loop gain ever reaching unity. */
    void setFreeze (bool on) noexcept { freeze = on; }
    bool isFrozen() const noexcept { return freeze; }

    void setFeedbackEnabled (bool on) noexcept { feedbackEnabled = on; }
    void setFeedbackThreshold (double t) noexcept { feedbackThreshold = juce::jlimit (0.0, 1.0, t); }
    void setFeedbackSpeed (double s) noexcept { feedbackSpeed = juce::jlimit (0.0, 1.0, s); }

    /** The hidden effect. Reached only through the easter egg in the UI. */
    SecretEffect& getSecretEffect() noexcept { return secret; }

    void setDoublerEnabled (bool on) noexcept { doublerEnabled = on; }
    void setDoublerAmount (double a) noexcept { doublerAmount = juce::jlimit (0.0, 1.0, a); }

    void setOversamplingFactor (int factor) noexcept;
    int getOversamplingFactor() const noexcept { return oversamplingFactor; }

    void setTempoBpm (double bpm) noexcept;

    /** Host transport position, for the rhythm engine's grid. */
    void setTransportPosition (double ppqPosition, bool isPlaying) noexcept
    {
        hostPpq = ppqPosition;
        hostPlaying = isPlaying;
    }

    /** The rhythm engine sits between the interpreter and the technique engine
        and rewrites the event stream when it is switched on. */
    RhythmEngine& getRhythmEngine() noexcept { return rhythm; }
    const RhythmEngine& getRhythmEngine() const noexcept { return rhythm; }

    /** The instrument's physical imperfections (character-wear.md). Applied at
        note-on for the per-position ones and per block for the drift, never per
        sample - rule 2 of that spec's section 0. */
    CharacterEngine& getCharacterEngine() noexcept { return character; }
    const CharacterEngine& getCharacterEngine() const noexcept { return character; }

    //==========================================================================
    // Routing (routing-io.md). The engine fills tap buffers as it renders and
    // records which strings started and stopped; the processor turns those into
    // host buses and MIDI out. The engine itself knows nothing about either.

    TapBuffers& getTapBuffers() noexcept { return taps; }
    const TapBuffers& getTapBuffers() const noexcept { return taps; }

    const StringActivityQueue& getStringActivity() const noexcept { return stringActivity; }

    /** Points the engine at this block's sidechain input. The pointers belong to
        the caller and must outlive the processBlock call; passing nullptr (or a
        zero channel count) means "no sidechain this block", which is the normal
        case. */
    void setSidechainInput (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Internal re-amp (routing-io 5B): the sidechain replaces the string
        engine's contribution at the amp input. Off by default. */
    void setSidechainToAmp (bool on) noexcept { sidechainToAmp = on; }
    bool isSidechainToAmp() const noexcept { return sidechainToAmp; }

    /** Envelope of the sidechain input, for the modulation matrix's
        SidechainEnvFollower source. Zero when no sidechain is connected. */
    double getSidechainEnvelope() const noexcept { return sidechainEnv.load (std::memory_order_relaxed); }

    /** The summed, normalised string signal exactly as it enters the body. This
        is the "main out pre-body" that routing-io 10 defines the per-string sum
        test against, and it is what the body convolution is applied to.

        Read-only, and valid for getLastSubBlockNumSamples() samples after a
        render. When the host oversteps its promised block size the engine splits
        the block, and this then describes the last slice only. */
    const double* getPreBodyBuffer() const noexcept { return stringSumBuffer.data(); }
    int getLastSubBlockNumSamples() const noexcept { return lastSubBlockNumSamples; }

    /** Per-output latency, as routing-io 7 defines it. */
    int getLatencySamples (AuxBus bus) const noexcept;
    int getPerStringLatencySamples() const noexcept;

    //==========================================================================
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) noexcept;

    /** The real work. processBlock splits anything larger than the block size
        the engine was prepared for and calls this for each piece. */
    void processSubBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) noexcept;

    /** Total reported latency in samples. */
    int getLatencySamples() const noexcept;

    /** Releases every string. */
    void panic() noexcept;

    //==========================================================================
    // Live state for the UI. All lock-free reads.

    double getStringLevel (int i) const noexcept;
    double getStringFrequency (int i) const noexcept;
    int    getStringMidiNote (int i) const noexcept;
    double getStringFret (int i) const noexcept;
    double getStringTensionNewtons (int i) const noexcept;
    const StringSpec& getStringSpec (int i) const noexcept;

    juce::String getLastChordName() const { return midi.getLastChordName(); }
    bool consumeMidiActivity() noexcept { return midi.consumeActivityFlag(); }

    double getCpuEstimate() const noexcept { return cpuEstimate.load (std::memory_order_relaxed); }

private:
    /** Moves a block's events onto the schedule, converting their offsets to
        absolute sample positions. */
    void scheduleEvents (const PlayEventQueue& queue, int numSamples) noexcept;

    /** Fires everything due at or before `absoluteSample`. */
    void fireScheduledEvents (int64_t absoluteSample) noexcept;
    void triggerNote (const NoteOnEvent& e) noexcept;
    void applyNoteOff (const NoteOffEvent& e) noexcept;
    void updatePerBlockModulation (int numSamples) noexcept;
    void processFeedback (double outputLevel) noexcept;
    void rebuildBodyFromSpec();
    void rebuildPickupsFromSpec();

    double sr = 44100.0;
    int maxBlock = 512;

    GuitarType guitarType = GuitarType::Stratocaster;
    GuitarSpec spec {};
    int numStrings = 6;

    // --- model ---------------------------------------------------------------
    TuningEngine tuning;
    TechniqueEngine technique;
    ChordVoicer voicer;
    MidiInterpreter midi;
    PlayEventQueue events;
    PlayEventQueue rhythmEvents;
    RhythmEngine rhythm;
    CharacterEngine character;

    /** The drift last written into the tuning engine, so a block that did not
        move it does not rewrite it. */
    std::array<double, kMaxStrings> lastAppliedDrift {};
    Validator validator;

    double hostPpq = 0.0;
    bool hostPlaying = false;
    double tempoBpm = 120.0;

    /** A strum spreads a chord over tens of milliseconds, which is far longer than
        one buffer. Events therefore have to survive past the block they arrived
        in - firing only what lands inside the current block and discarding the
        rest would silently drop most of every strummed chord. */
    struct ScheduledEvent
    {
        bool isNoteOn = true;
        NoteOnEvent noteOn {};
        NoteOffEvent noteOff {};
        int64_t absoluteSample = 0;
    };

    static constexpr int kMaxScheduledEvents = 192;
    std::array<ScheduledEvent, kMaxScheduledEvents> scheduled {};
    int numScheduled = 0;

    // --- instrument ----------------------------------------------------------
    std::array<StringEngine, kMaxStrings> strings;
    std::array<StringSpec, kMaxStrings> stringSpecs {};
    StringAge stringAge = StringAge::BrokenIn;
    std::array<double, kMaxStrings> customGauges {};
    CouplingMatrix coupling;
    BodyEngine body;
    PickupEngine pickups;
    WhammyEngine whammy;

    // --- signal chain ---------------------------------------------------------
    CableSim cable;
    EffectsChain preEffects;
    AmpEngine amp;
    EffectsChain postEffects;
    CabinetEngine cabinet;
    RoomEngine room;
    SecretEffect secret;
    MasterBus master;

    // --- per-block scratch (all pre-allocated) --------------------------------
    std::vector<double> stringSumBuffer;
    std::vector<double> magneticBuffer;
    std::vector<double> instrumentBuffer;
    juce::AudioBuffer<float> bodyBuffer;
    juce::AudioBuffer<float> workBuffer;

    /** Holds the post-amp signal before the post-amp effects, so Aux 6 can be
        the difference between the two - the tails on their own. */
    juce::AudioBuffer<float> wetDryBuffer;
    std::vector<double> doublerBuffer;
    int doublerSize = 0, doublerMask = 0, doublerIndex = 0;

    std::array<double, kMaxStrings> bridgeOutputs {};
    std::array<double, kMaxStrings> couplingInputs {};
    std::array<double, kMaxStrings> stringOutputs {};
    std::array<double, kMaxStrings> stringDelays {};

    // --- articulation state ----------------------------------------------------
    std::array<double, kMaxStrings> currentFret {};
    std::array<double, kMaxStrings> targetFret {};
    std::array<int, kMaxStrings> stringMidiNote {};
    std::array<Lfo, kMaxStrings> vibratoLfo;
    std::array<double, kMaxStrings> vibratoAmount {};

    Excitation::Material pickMaterial = Excitation::Material::PickCelluloid;
    double pluckPosition = 0.16;
    double pickThickness = 0.5;
    double pickAngle = 0.35;
    double nailVsFlesh = 0.5;
    double attackBrightness = 0.5;
    bool   usingFingers = false;

    bool   fretless = false;
    double fretActionMm = 1.6;
    double fretBuzzAmount = 0.0;

    double slideNoise = 0.35, fretNoise = 0.35, releaseNoise = 0.3;
    double bodyKnockAmount = 0.0, pickAttackNoise = 0.3;

    double vibratoRate = 5.2;
    double vibratoDepthCents = 22.0;

    bool   freeze = false;
    double freezeTargetLevel = 0.09;

    bool   feedbackEnabled = false;
    double feedbackThreshold = 0.65;
    double feedbackSpeed = 0.4;
    double feedbackAmount = 0.0;
    int    feedbackString = -1;
    int    feedbackPartial = 2;
    Biquad feedbackFilter;

    bool   doublerEnabled = false;
    double doublerAmount = 0.5;
    Lfo    doublerLfo;

    double bodyAmount = 0.22;
    int    oversamplingFactor = 4;

    int64_t samplePosition = 0;

    /** Sample offset, within the block being rendered, of the event currently
        being fired. Lets triggerNote and applyNoteOff timestamp their string
        activity exactly, rather than stacking a whole strum on sample zero. */
    int activeSampleOffset = 0;

    /** Absolute sample index of the first sample of the block being rendered. */
    int64_t blockStartSample = 0;
    int lastSubBlockNumSamples = 0;

    std::atomic<double> cpuEstimate { 0.0 };

    // --- routing ----------------------------------------------------------------
    TapBuffers taps;
    StringActivityQueue stringActivity;

    const float* const* sidechainChannels = nullptr;
    int sidechainNumChannels = 0;
    int sidechainNumSamples = 0;
    int sidechainReadOffset = 0;
    bool sidechainToAmp = false;
    std::atomic<double> sidechainEnv { 0.0 };
    EnvelopeFollower sidechainFollower;

    /** Reads one mono sample of sidechain, summing the channels, or zero when
        nothing is connected. */
    inline double readSidechain (int index) const noexcept
    {
        if (sidechainChannels == nullptr || sidechainNumChannels <= 0)
            return 0.0;

        const int i = sidechainReadOffset + index;

        if (i < 0 || i >= sidechainNumSamples)
            return 0.0;

        double sum = 0.0;

        for (int ch = 0; ch < sidechainNumChannels; ++ch)
            if (sidechainChannels[ch] != nullptr)
                sum += (double) sidechainChannels[ch][i];

        return sum / (double) sidechainNumChannels;
    }

    RtRandom rng { 0xA11CE5ull };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierEngine)
};

} // namespace luthier
