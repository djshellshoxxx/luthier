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
    Validator validator;

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

    std::atomic<double> cpuEstimate { 0.0 };

    RtRandom rng { 0xA11CE5ull };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierEngine)
};

} // namespace luthier
