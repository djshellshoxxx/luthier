#pragma once

/*  The whole instrument, wired together (engine spec 1).

        MIDI -> interpreter -> technique -> tuning -> strings (+ coupling)
             -> body -> pickups -> guitar circuit -> pre-effects -> amp
             -> post-effects -> cabinet -> room -> master

    This class owns every module and nothing else owns any of them. The plugin
    processor talks to it through setters; the UI reads state back through const
    accessors. Everything inside processBlock is allocation-free.
*/

#include "Support/SoundingNotes.h"   // animated-strings.md 4.1
#include "DSP/String/StringEngine.h"
#include "DSP/Coupling/CouplingMatrix.h"
#include "DSP/Body/BodyEngine.h"
#include "DSP/Pickup/PickupEngine.h"
#include "DSP/Whammy/WhammyEngine.h"
#include "DSP/Circuit/GuitarCircuit.h"
#include "DSP/Noise/PlayingNoise.h"
#include "DSP/Noise/FretBuzz.h"
#include "DSP/Noise/ScrapeEngine.h"
#include "DSP/Noise/NoiseFloor.h"   // noise-floor.md
#include "Model/Playing/StabilityModel.h"   // tuning-stability.md
#include "DSP/Slap/SlapEngine.h"
#include "DSP/Slap/BassFingerstyle.h"
#include "Model/Playing/TechniqueTriggers.h"
#include "DSP/Slide/SlideEngine.h"
#include "DSP/Feedback/FeedbackLoop.h"
#include "DSP/Feedback/EBowDriver.h"
#include "Model/Workshop/PartAcoustics.h"
#include "DSP/Effects/EffectsChain.h"
#include "DSP/Effects/SecretEffect.h"
#include "DSP/Amp/AmpEngine.h"
#include "DSP/Amp/CabinetEngine.h"
#include "DSP/Amp/RoomEngine.h"
#include "DSP/Body/AcousticMicModel.h"   // mic-placement.md 3
#include "DSP/Master/MasterBus.h"
#include "DSP/Master/FreezeOverlay.h"
#include "Model/Guitar/GuitarLibrary.h"
#include "Model/Playing/MidiInterpreter.h"
#include "Validator.h"
#include "Support/IrLibrary.h"
#include "Routing/TapBuffers.h"
#include "Routing/MidiOutRouter.h"
#include "Rhythm/RhythmEngine.h"
#include "Character/CharacterEngine.h"
#include "Support/QualityProfile.h"   // cpu-quality-modes
#include "Riffs/RiffPlayer.h"   // riff-library 5.3
#include "Character/EnvironmentModel.h"          // environment.md (REALISM-A)
#include "DSP/String/StringAging.h"              // string-aging.md (REALISM-A)
#include "DSP/Coupling/BodyCouplingBank.h"       // body-coupling.md (REALISM-A)
#include "DSP/String/Harmonics.h"          // REALISM-B: harmonic-realism.md
#include "Model/Playing/RightHand.h"       // REALISM-B: fingerstyle-attack.md, string-interaction.md
#include "DSP/Techniques/TechniqueLayer.h"   // TECHNIQUES: engine-technique-layer.md

#include <array>
#include <atomic>

namespace luthier
{

class PerformanceCapture;
class IrSlot;   // SPEC-SWEEP TM-6

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
    /*  guitar-workshop.md 10: a guitar or part swap is click-free. For the
        scope of one of these the audio thread is parked: it fades the output
        out over 5 ms, then renders silence without touching the engine while
        the message thread rebuilds it, and fades back in over 5 ms when the
        scope ends. That also keeps the rebuild off an engine the audio thread
        is inside. With no audio thread running - not processing lately, or
        the caller is the audio thread itself, as in the offline renderer and
        the tests - the change just applies. Message thread; nests. */
    class ScopedStructuralChange
    {
    public:
        explicit ScopedStructuralChange (LuthierEngine& e) : engine (e) { engine.beginStructuralChange(); }
        ~ScopedStructuralChange() { engine.endStructuralChange(); }

    private:
        LuthierEngine& engine;
        JUCE_DECLARE_NON_COPYABLE (ScopedStructuralChange)
    };

    /** The fade either side of a structural change, each way. */
    static constexpr double kSwapFadeSeconds = 0.005;

    /** Loads a factory instrument: body, strings, pickups, tuning, amp and cab. */
    void setGuitarType (GuitarType type);
    GuitarType getGuitarType() const noexcept { return guitarType; }
    const GuitarSpec& getGuitarSpec() const noexcept { return spec; }

    void setTuningPreset (TuningPreset preset);

    /** A 12-string's courses from spec.tuning: unison top two, octaves below. */
    void applyTwelveStringTuning();
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
    RubricVoicer&    getChordVoicer() noexcept     { return voicer; }
    RubricVoicer&    getRhythmVoicer() noexcept    { return rhythmVoicer; }
    MidiInterpreter& getMidiInterpreter() noexcept { return midi; }
    CouplingMatrix&  getCouplingMatrix() noexcept  { return coupling; }
    BodyEngine&      getBodyEngine() noexcept      { return body; }
    PickupEngine&    getPickupEngine() noexcept    { return pickups; }
    WhammyEngine&    getWhammyEngine() noexcept    { return whammy; }
    GuitarCircuit&   getGuitarCircuit() noexcept   { return circuit; }
    PlayingNoise&    getPlayingNoise() noexcept    { return playingNoise; }
    const PlayingNoise& getPlayingNoise() const noexcept { return playingNoise; }

    /*  pick-noise.md and string-squeak.md settings, from the parameters. The
        pick's material, fingers and position are the engine's own and are
        filled in at each pluck, because the guitar type can change them. */
    void setPickNoise (const PickSettings& settings) noexcept   { playingNoise.setPick (settings); }
    void setSqueak (const SqueakSettings& settings) noexcept    { playingNoise.setSqueak (settings); }

    /*  Starts a note now, bypassing MIDI interpretation and scheduling. For the
        tests and the offline renderer, which need a note on a named string at
        a named fret. Audio thread. */
    void triggerNoteNow (const NoteOnEvent& e) noexcept { triggerNote (e); }

    /*  fret-buzz.md: the setup. The scale length and string count are the
        guitar's and are filled in here; the rest comes from the parameters. */
    void setSetupGeometry (const SetupGeometry& geometry) noexcept;
    FretBuzz& getFretBuzz() noexcept { return fretBuzzModel; }
    const FretBuzz& getFretBuzz() const noexcept { return fretBuzzModel; }

    /*  guitar-workshop.md: builds the instrument from a parts guitar's derived
        acoustics (part-acoustics.md). Message thread, like setGuitarType -
        it allocates. Replaces the compiled guitar's body, pickups, strings
        and termination with what the parts say. */
    void applyWorkshopGuitar (const DerivedAcoustics& derived, GuitarType standsFor = GuitarType::Custom);

    /*  TODO 6e / DECISIONS C-09, ui-wiring.md 6.2 (MODEL-GAPS): a part swap that
        keeps the instrument's structure - string count, tuning, family, frets,
        bridge, pickup count and selector, rig defaults and the body's IR -
        is built here on the message thread and swapped in by the audio
        thread at the start of its next block, while the strings keep
        sounding: no park, no fade to silence. The strings keep their pitch
        and their energy; their coefficients change under them.

        Returns false, having changed nothing, when the swap is not one of
        those, when no audio thread is running elsewhere (the change is then
        simply applied by applyWorkshopGuitar), or when the audio thread did
        not take it within 250 ms. The caller then uses applyWorkshopGuitar.
        Message thread; waits for the block boundary, bounded. */
    bool swapPartsAtBlockBoundary (const DerivedAcoustics& derived, GuitarType standsFor = GuitarType::Custom);

    /** Whether a swap from the current guitar to `derived` keeps its structure (above). */
    bool partSwapKeepsStructure (const DerivedAcoustics& derived) const;

    /** Part swaps taken at a block boundary since prepare, for the tests. */
    int getLivePartSwapCount() const noexcept { return livePartSwaps.load (std::memory_order_relaxed); }

    /** A parts guitar's pickup as its part describes it (engine slot order). */
    const PickupSpec& getPartsPickup (int slot) const noexcept
    {
        return partsPickups[(size_t) juce::jlimit (0, 2, slot)];
    }

    /*  workshop-ui.md 4: a pickup dragged on the bench moves while it is
        dragged, without a structural swap. Message thread; the audio thread
        applies it at its next block (a lock-free hand-over, no park).
        `position` is a fraction of the scale from the saddle. */
    void setPickupPlacementLive (int slot, double position, double heightMm) noexcept;

    /** Body and cabinet responses really loaded so far (a rebuild that keeps them loads none). */
    int getIrLoadCount() const noexcept { return body.getIrLoadCount() + cabinet.getIrLoadCount(); }

    /*  gui-integration.md 3.4: input trim into the rig, wet/dry against the DI,
        stereo width. Any thread; smoothed on the audio thread. */
    void setInputGainDb (double db) noexcept   { inputGainTarget.store (juce::Decibels::decibelsToGain (juce::jlimit (-60.0, 36.0, db))); }
    void setOutputMix (double wet) noexcept    { outputMixTarget.store (juce::jlimit (0.0, 1.0, wet)); }
    void setStereoWidth (double width) noexcept { widthTarget.store (juce::jlimit (0.0, 2.0, width)); }

    /** Whether the instrument is a parts guitar rather than a compiled type. */
    bool isWorkshopGuitar() const noexcept { return hasPartsOverride; }

    /** part-acoustics.md 6.2's magnet pull, for the tests: sustain multiplier and cents. */
    double getMagnetSustainScale() const noexcept { return magnetSustain; }
    double getMagnetDetuneCents() const noexcept { return magnetDetuneCents; }

    /** slide-guitar.md: Slide Mode's settings, from the parameters. */
    void setSlideSettings (const SlideSettings& settings) noexcept { slide.setSettings (settings); }
    SlideEngine& getSlideEngine() noexcept { return slide; }

    /*  guitar-workshop.md 2 / TODO 5b (VISUAL-WORKSHOP-QA): the fitted slide
        part's bar - material, mass, length, diameter. Message thread; parks the
        audio thread for the swap like any structural change. */
    void setSlideBar (const SlideBar& bar)
    {
        const ScopedStructuralChange change (*this);
        slide.setBar (bar);
    }
    const SlideEngine& getSlideEngine() const noexcept { return slide; }

    /** pick-noise.md 5: a deliberate rake along the wound strings. */
    void triggerPickScrape (double seconds, bool downward) noexcept;

    /** string-scraping.md: the scrape technique, for the bridge, the UI's buttons and the tests. */
    ScrapeEngine& getScrapeEngine() noexcept { return scrape; }
    const ScrapeEngine& getScrapeEngine() const noexcept { return scrape; }
    void setScrapeSettings (const ScrapeSettings& s) noexcept { scrape.setSettings (s); }

    /** string-slap-technique.md: the slap, and the technique layer's shared MIDI front. */
    SlapEngine& getSlapEngine() noexcept { return slap; }
    const SlapEngine& getSlapEngine() const noexcept { return slap; }
    TechniqueTriggers& getTechniqueTriggers() noexcept { return techniqueTriggers; }
    const TechniqueTriggers& getTechniqueTriggers() const noexcept { return techniqueTriggers; }
    void setSlapSettings (const SlapSettings& s) noexcept
    {
        slap.setSettings (s);
        techniqueTriggers.configure (TechniqueId::slap, s.triggerConfig());
    }

    /*  TECHNIQUES (engine-technique-layer.md 1): mute, tap, microtonal bends,
        the slide's user controls and the cascade resolver. The glue is in
        LuthierEngineTechniques.cpp. */
    TechniqueLayer& getTechniqueLayer() noexcept { return techniqueLayer; }
    const TechniqueLayer& getTechniqueLayer() const noexcept { return techniqueLayer; }
    void setMuteSettings (const MuteSettings& s) noexcept { techniqueLayer.mute.setSettings (s); }
    void setTapSettings (const TapSettings& s) noexcept;
    void setBendSettings (const BendSettings& s) noexcept;
    void setSlideControls (const SlideControlSettings& c) noexcept;

    /** Sets the pick material and whether it is fingers. The two parameters
        are one decision: a finger material is fingers whatever the switch says. */
    void setPickMaterialAndFingers (Excitation::Material material, bool fingers) noexcept;

    /** noise-floor.md: the rig's steady noise sources. */
    NoiseFloor& getNoiseFloor() noexcept { return noiseFloor; }
    const NoiseFloor& getNoiseFloor() const noexcept { return noiseFloor; }
    void setNoiseFloorSettings (const NoiseFloorSettings& s) noexcept { noiseFloor.setSettings (s); }

    /** sustain-and-decay.md 7: the decay's shape, for every string, at block rate. */
    void setSustainShape (const StringEngine::SustainShape& s) noexcept { sustainShape = s; }
    const StringEngine::SustainShape& getSustainShape() const noexcept { return sustainShape; }

    /** SUS-01's test hook: the shape code removed from every string. */
    void setSustainShapeBypassedForTest (bool b) noexcept
    {
        for (auto& str : strings)
            str.setShapeBypassedForTest (b);
    }

    /** tuning-stability.md 5: the event-driven tuning offsets and the retunes. */
    StabilityModel& getStabilityModel() noexcept { return stability; }
    const StabilityModel& getStabilityModel() const noexcept { return stability; }

    /** 2.6: the capo part's pressure and gap (message thread). */
    void setCapoHardware (double pressure, double gapMm) noexcept;

    /** TS-01's test hook: the model removed from the block entirely. */
    void setStabilityBypassedForTest (bool b) noexcept { stabilityBypassed = b; }

    /** The open pitch a stability event compares: open, detune and fine tune. */
    double getStabilityBasePitch (int stringIndex) const noexcept;

    /** NF-01's test hook: the render with the module removed entirely. */
    void setNoiseFloorBypassedForTest (bool b) noexcept { noiseFloorBypassed = b; }

    /** The Aux 8 noise bus for the last block (routing-io.md). */
    const double* getNoiseBusData() const noexcept { return noiseBuffer.data(); }

    /** This block's noise triggers, at their samples (midi-export.md 6). */
    const NoiseEngine& getNoisePool() const noexcept { return playingNoise.getPool(); }

    /*  The circuit's controls and components, from the parameters. The coil
        fields are ignored: the engine fills them from the pickups the switch
        has selected, at the start of each block, because the selector can
        change between two calls to this. */
    void setCircuitControls (const CircuitComponents& controls) noexcept { circuitControls = controls; }

    /** What the circuit is actually running with, coil included - for the
        visualiser, which draws the network the audio is going through. */
    CircuitComponents getLiveCircuitComponents() const noexcept;
    EffectsChain&    getPreEffects() noexcept      { return preEffects; }
    AmpEngine&       getAmpEngine() noexcept       { return amp; }
    EffectsChain&    getPostEffects() noexcept     { return postEffects; }
    CabinetEngine&   getCabinetEngine() noexcept   { return cabinet; }
    RoomEngine&      getRoomEngine() noexcept      { return room; }

    /** mic-placement.md 3: external mics around an acoustic guitar, mixed
        against the pickup by ac_mic_mix (0 = today's pickup path alone). */
    AcousticMicModel& getAcousticMicModel() noexcept { return acMic; }
    void setAcousticMicMix (double mix) noexcept { acMicMixTarget = juce::jlimit (0.0, 1.0, mix); }
    double getAcousticMicMix() const noexcept    { return acMicMixTarget; }
    bool isAcousticMicActive() const noexcept    { return acMicActive; }
    MasterBus&       getMasterBus() noexcept       { return master; }
    FreezeOverlay&   getFreezeOverlay() noexcept   { return freezeOverlay; }
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

    /*  E-Bow (ambiguity-resolutions 2.2): the string is driven at its own
        resonance up to a target level, so it sustains without the loop gain ever
        reaching unity.

        This is not Freeze. Freeze (2.1) captures a window and loops it, and lives
        in FreezeOverlay. The two were one control called "Freeze / E-Bow", which
        is exactly the ambiguity section 2 was written to settle. */
    void setEBow (const EBowSettings& settings) noexcept { ebowDriver.setSettings (settings); }
    bool isEBowing() const noexcept { return ebowDriver.getSettings().enabled; }
    const EBowDriver& getEBow() const noexcept { return ebowDriver; }

    /** ambiguity-resolutions.md 1: the physical feedback loop. */
    void setFeedback (const FeedbackSettings& settings) noexcept { feedbackLoop.setSettings (settings); }
    const FeedbackLoop& getFeedbackLoop() const noexcept { return feedbackLoop; }

    /** The last block's feedback, summed over the strings, per sample. */
    const double* getFeedbackInjection() const noexcept { return feedbackInjection.data(); }

    /** The hidden effect. Reached only through the easter egg in the UI. */
    SecretEffect& getSecretEffect() noexcept { return secret; }


    void setOversamplingFactor (int factor) noexcept;
    int getOversamplingFactor() const noexcept { return oversamplingFactor; }

    //==========================================================================
    // cpu-quality-modes (implemented in LuthierEngineQuality.cpp).

    /** 2.5: sets integers and flags and starts crossfades; never allocates. A
        hard switch (prepare, reset, or 50 ms of output below -90 dBFS) changes
        everything at once. Audio thread (or with the audio thread parked). */
    void applyQuality (const QualityProfile& profile, bool hardSwitch) noexcept;
    const QualityProfile& getQualityProfile() const noexcept { return qualityProfile; }

    /** 7, E3: fades the least-recently-excited ringing string over 10 ms.
        Returns false if nothing was ringing. Audio thread. */
    bool dropLeastRecentString() noexcept;

    /** For Diagnostics and the tests. */
    int getSleepingStringCount() const noexcept;
    int getEffectiveAmpOversampling() const noexcept   { return amp.getEffectiveOversamplingFactor(); }
    int getEffectiveDriveOversampling() const noexcept { return preEffects.getEffectiveOversamplingFactor(); }
    int getHardQualitySwitchCount() const noexcept { return hardQualitySwitches; }

    /*  performance-budget.md 7: above 96 kHz the oversampled modules run at a
        lower internal factor - the user's factor halved above 96 kHz and
        quartered above 176.4 kHz, never below 1x - so the internal rate stays
        near 384 kHz. Transparent: the images it guards against sit above
        20 kHz at those rates anyway. */
    static int effectiveOversamplingFactor (int userFactor, double sampleRate) noexcept;
    int getEffectiveOversamplingFactor() const noexcept { return effectiveOversamplingFactor (oversamplingFactor, sr); }

    void setTempoBpm (double bpm) noexcept;
    double getTempoBpm() const noexcept { return tempoBpm; }   // SPEC-SWEEP: LP-25 (tests)

    /** Host transport position, for the rhythm engine's grid. */
    void setTransportPosition (double ppqPosition, bool isPlaying) noexcept
    {
        hostPpq = ppqPosition;
        hostPlaying = isPlaying;
    }

    /** The grid the rhythm engine strums on this block (FEAT-JAM, JM-46). */
    double getTransportPpq() const noexcept { return hostPpq; }

    /*  notation-export 6.1 / TODO 9 (MODEL-GAPS): the capture the engine reports
        to from triggerNote and applyNoteOff - string, fret and technique as
        played - plus the detector's chord in Poly mode (4), BASS_TECH strikes
        and the slide bar. Null (the default) reports nothing. The capture must
        outlive the engine or be cleared first. */
    void setPerformanceCapture (PerformanceCapture* c) noexcept { perfCapture = c; }

    /** SPEC-SWEEP TM-6 (tone-match 1): the TONE MATCH body IR slot, which
        replaces the body's response while engaged. Owned by the caller, set
        before audio starts; nullptr for none. */
    void setBodyIrSlot (IrSlot* slot) noexcept { bodyIrSlot = slot; }
    PerformanceCapture* getPerformanceCapture() const noexcept { return perfCapture; }

    /*  ambiguity-resolutions 8 / routing-io 2 (MODEL-GAPS): Aux 1 (DI) taps the
        pickup after the guitar's circuit (the default, what enters the amp) or
        before it, so the volume knob's effect on feedback can be measured. */
    void setAuxDiPreCircuit (bool pre) noexcept { auxDiPreCircuit.store (pre, std::memory_order_relaxed); }
    bool isAuxDiPreCircuit() const noexcept { return auxDiPreCircuit.load (std::memory_order_relaxed); }

    /** bass-techniques.md 6 (MODEL-GAPS): finger alternation and the rest stroke. */
    void setBassFingerstyle (const BassFingerstyleSettings& s) noexcept { bassFingers.setSettings (s); }
    const BassFingerstyle& getBassFingerstyle() const noexcept { return bassFingers; }

    /** The rhythm engine sits between the interpreter and the technique engine
        and rewrites the event stream when it is switched on. */
    RhythmEngine& getRhythmEngine() noexcept { return rhythm; }

    /** riff-library 5.3: the riff audition player, played into the strings
        after the direct notes of each sub-block. */
    RiffPlayer& getRiffPlayer() noexcept { return riffPlayer; }
    const RiffPlayer& getRiffPlayer() const noexcept { return riffPlayer; }
    /** The cents a riff note's bend holds a string at (tests). */
    double getRiffBendCents (int stringIndex) const noexcept
    {
        return riffBendCents[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)];
    }
    const RhythmEngine& getRhythmEngine() const noexcept { return rhythm; }

    /** The instrument's physical imperfections (character-wear.md). Applied at
        note-on for the per-position ones and per block for the drift, never per
        sample - rule 2 of that spec's section 0. */
    CharacterEngine& getCharacterEngine() noexcept { return character; }
    const CharacterEngine& getCharacterEngine() const noexcept { return character; }

    // ==== BEGIN REALISM-A engine API ====
    /** string-aging.md: the set's age, per string. */
    StringAging& getStringAging() noexcept { return aging; }
    const StringAging& getStringAging() const noexcept { return aging; }

    /** environment.md: temperature, humidity and their lags. */
    EnvironmentModel& getEnvironment() noexcept { return environment; }
    const EnvironmentModel& getEnvironment() const noexcept { return environment; }

    /** body-coupling.md: the bridge admittance bank. */
    BodyCouplingBank& getBodyCoupling() noexcept { return bodyCoupling; }
    const BodyCouplingBank& getBodyCoupling() const noexcept { return bodyCoupling; }

    /*  body-coupling.md 4: the runtime scales (block rate, any thread) and the
        coupling amount. The frequency and Q scales reach the radiated body too
        (3, "Scaling"); the mass scale reaches only the bank. */
    void setBodyModeScales (double freqScale, double qScale, double massScale) noexcept
    {
        bodyFreqScale = juce::jlimit (0.25, 4.0, freqScale);
        bodyQScale = juce::jlimit (0.05, 10.0, qScale);
        bodyMassScale = juce::jlimit (0.01, 50.0, massScale);
    }

    /** The product the bank and the body receive this block. */
    BodyCouplingScaling getBodyCouplingScaling() const noexcept;

    /** Re-designs the bank from the body, the bridge and the strings. Message thread. */
    void rebuildBodyCoupling();
    void applyRealismStringInfo (int stringIndex, const StringSpec& spec) noexcept;   // REALISM-A

    /** The bridge the bank was last designed with. */
    const BridgeCoupling& getBridgeCoupling() const noexcept { return bridgeCoupling; }

    /** environment.md 3.4: the host's playhead, seconds, when it is playing. */
    void setHostTimeSeconds (double seconds, bool isPlaying) noexcept
    {
        hostTimeSeconds = seconds;
        hostTimePlaying = isPlaying;
    }

    /*  body-coupling.md 5: the CHARACTER tab's Tap button - one body tap, heard
        through the body and driving the bank so the open strings answer. Any
        thread; applied at the next block. */
    void requestBodyTap (double force) noexcept { pendingBodyTap.store (juce::jlimit (0.0, 1.0, force)); }

    /** The strings' wave impedances and open pitches, for the wolf map. Message thread. */
    double getStringWaveImpedance (int s) const noexcept { return getString (s).getPhysical().waveImpedance; }

        /** The SETUP geometry the buzz model is really using (requested plus environment). */
    const SetupGeometry& getRequestedSetup() const noexcept { return requestedSetup; }
    // ==== END REALISM-A engine API ====

    //==========================================================================
    // Routing (routing-io.md). The engine fills tap buffers as it renders and
    // records which strings started and stopped; the processor turns those into
    // host buses and MIDI out. The engine itself knows nothing about either.

    TapBuffers& getTapBuffers() noexcept { return taps; }
    const TapBuffers& getTapBuffers() const noexcept { return taps; }

    const StringActivityQueue& getStringActivity() const noexcept { return stringActivity; }

    // RE-41, rhythm-engine.md 9: the rhythm engine's own emitted stream, empty
    // whenever it is not driving, for the MIDI-out RHYTHM source.
    const PlayEventQueue& getRhythmEvents() const noexcept { return rhythmEvents; }

    /** Points the engine at this block's sidechain input. The pointers belong to
        the caller and must outlive the processBlock call; passing nullptr (or a
        zero channel count) means "no sidechain this block", which is the normal
        case. */
    void setSidechainInput (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Internal re-amp (routing-io 5B): the sidechain replaces the string
        engine's contribution at the amp input. Off by default. */
    void setSidechainToAmp (bool on) noexcept { sidechainToAmp = on; }
    bool isSidechainToAmp() const noexcept { return sidechainToAmp; }

    /** performance-budget.md 4: the same switch as setAuxDiPreCircuit (MODEL-GAPS). */
    void setDiPreCircuit (bool pre) noexcept { setAuxDiPreCircuit (pre); }
    bool isDiPreCircuit() const noexcept { return isAuxDiPreCircuit(); }

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

    /** Notes for the next processBlock that are always played as written, even
        while the rhythm engine is driving (which otherwise replaces every
        interpreted note with its own stream). The tune builder's melody, bass
        and layers come this way; its chords go in the normal MIDI, where the
        rhythm engine can strum them (tune-builder 8). The buffer must outlive
        the next processBlock call; pass nullptr for none. Audio thread. */
    void setDirectMidi (const juce::MidiBuffer* direct) noexcept { directMidi = direct; }

    /** SPEC-SWEEP (RE-41, routing-io MIDI out): where the rhythm engine's strokes
        go as MIDI for the next processBlock - note on/off, channel = string + 1,
        at their samples. A stroke scheduled past the block's end is held and
        written in the block it lands in. nullptr for none. Audio thread. */
    void setRhythmMidiOut (juce::MidiBuffer* out) noexcept { rhythmMidiOut = out; }

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

    /** SPEC-SWEEP (character-wear 7 test): the sustain multiplier the note now on
        string i started with (dead spot x fret wear x nut x slide x parts). */
    double getNoteSustainScale (int i) const noexcept
    {
        return juce::isPositiveAndBelow (i, kMaxStrings) ? noteSustainScale[(size_t) i] : 1.0;
    }
    double getStringFrequency (int i) const noexcept;
    int    getStringMidiNote (int i) const noexcept;
    double getStringFret (int i) const noexcept;
    double getStringTensionNewtons (int i) const noexcept;
    const StringSpec& getStringSpec (int i) const noexcept;

    juce::String getLastChordName() const { return midi.getLastChordName(); }
    bool consumeMidiActivity() noexcept { return midi.consumeActivityFlag(); }

    double getCpuEstimate() const noexcept { return cpuEstimate.load (std::memory_order_relaxed); }

    // animated-strings.md 4.1: the per-string display snapshot, published once per
    // sub-block whatever the display settings (the piano roll shares it).
    const SoundingNotes& getSoundingNotes() const noexcept { return soundingNotes; }

    /** How many times publishSoundingNotes has run (AS-17's test counter). */
    uint64_t getSoundingNotesPublishCount() const noexcept { return soundingPublishCount.load (std::memory_order_relaxed); }
    // ==== BEGIN REALISM-B engine ====
    // harmonic-realism.md, string-interaction.md, fingerstyle-attack.md.
    // Implemented in LuthierEngineRealismB.cpp.

    /** harmonic-realism.md 5: the touch the contacts are built from. */
    void setHarmonicTouch (const HarmonicTouchSettings& s) noexcept { harmonicTouch = s; }

    /** Ends a note now, as triggerNoteNow starts one (tests, offline renderer). Audio thread. */
    void releaseNoteNow (const NoteOffEvent& e) noexcept { applyNoteOff (e); }
    const HarmonicTouchSettings& getHarmonicTouch() const noexcept { return harmonicTouch; }

    /** string-interaction.md 7. */
    void setStringInteraction (const StringInteractionSettings& s) noexcept;
    const StringInteractionSettings& getStringInteraction() const noexcept { return interaction; }

    /** fingerstyle-attack.md 6. */
    void setRightHand (const RightHandSettings& s) noexcept { rightHand = s; }
    const RightHandSettings& getRightHand() const noexcept { return rightHand; }

    /** The last note's excitation and resolved tool on a string, for the tests
        (FA-09's "asserted on the recorded Params") and the illustration. */
    const Excitation::Params& getLastExcitation (int s) const noexcept { return lastExcitation[(size_t) juce::jlimit (0, kMaxStrings - 1, s)]; }
    RhTool getLastTool (int s) const noexcept { return lastTool[(size_t) juce::jlimit (0, kMaxStrings - 1, s)]; }
    bool getLastWasRest (int s) const noexcept { return lastRest[(size_t) juce::jlimit (0, kMaxStrings - 1, s)]; }

    /*  The fretboard's contact ring (harmonic-realism.md 7): where a string is
        being touched, as a fret from the nut (-1 none), how much of the touch
        is left (1 landing .. 0 lifted) and whether it missed (off-node).
        Written by the audio thread per note, read by the UI; a torn read
        only misdraws one frame. */
    struct ContactDisplay { std::atomic<float> fret { -1.0f }; std::atomic<float> life { 0.0f }; std::atomic<bool> missed { false }; };
    const ContactDisplay& getContactDisplay (int s) const noexcept { return contactDisplay[(size_t) juce::jlimit (0, kMaxStrings - 1, s)]; }

    /** string-interaction.md 9: the palm's weight on each string now (0-1), for the mute-zone shading. */
    float getPalmWeight (int s) const noexcept { return palmWeightDisplay[(size_t) juce::jlimit (0, kMaxStrings - 1, s)].load (std::memory_order_relaxed); }
    // ==== END REALISM-B engine ====

    // ==== BEGIN FEAT-ASSIST ====
    /*  Performance Assist (auto-articulation.md 4.2). The glue is in
        LuthierEngineAssist.cpp. */
    void setAutoArticulation (const AutoArticulationSettings& s) noexcept { midi.setAutoArticulation (s); }
    AutoArticulator& getAutoArticulator() noexcept { return midi.getAutoArticulator(); }
    const AutoArticulator& getAutoArticulator() const noexcept { return midi.getAutoArticulator(); }

    /** Why Assist is not running, for the notice line (5). */
    AssistBypass getAssistBypass() const noexcept;

    /** 5: the Luthier-profile import player's notes are pre-articulated. */
    void setAssistPreArticulated (bool pre) noexcept { assistPreArticulated = pre; }
    // ==== END FEAT-ASSIST ====

private:
    /** Moves a block's events onto the schedule, converting their offsets to
        absolute sample positions. */
    void scheduleEvents (const PlayEventQueue& queue, int numSamples) noexcept;

    /** Fires everything due at or before `absoluteSample`. */
    void fireScheduledEvents (int64_t absoluteSample) noexcept;
    void triggerNote (const NoteOnEvent& e) noexcept;
    void applyNoteOff (const NoteOffEvent& e) noexcept;
    void updatePerBlockModulation (int numSamples) noexcept;

    /** animated-strings.md 4.1: the end-of-sub-block store into soundingNotes. Audio thread, never waits. */
    void publishSoundingNotes() noexcept;
    void resetSoundingState() noexcept;
    void advanceRealism (int numSamples) noexcept;   // REALISM-A: aging, environment, body coupling
    void refreshAgingJitter() noexcept;              // REALISM-A
    void pushAgingFactors() noexcept;                // REALISM-A
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
    /*  ambiguity-resolutions 4: two rubric voicers, because the style, the
        pitch mode and the previous voicing each belong to their caller - the
        interpreter places exactly the pitches played, the rhythm engine any
        chord tones in its style. */
    RubricVoicer voicer;
    RubricVoicer rhythmVoicer;
    MidiInterpreter midi;
    PlayEventQueue events;
    PlayEventQueue rhythmEvents;
    PlayEventQueue directEvents;

    // riff-library 5.3: the riff player, its sub-block's events, and the bend
    // each string's riff note holds (added to the MIDI bend per block).
    RiffPlayer riffPlayer;
    RiffPlayer::Output riffOut;
    std::array<double, kMaxStrings> riffBendCents {};
    int subBlockOffset = 0;   ///< samples into the host block this sub-block starts at
    void playRiffEvents (int numSamples) noexcept;
    bool schedulingRiff = false;   ///< scheduleEvents is taking the riff player's queue
    bool firingRiff = false;       ///< the event being fired came from the riff player
    const juce::MidiBuffer* directMidi = nullptr;      ///< for the current processBlock
    const juce::MidiBuffer* directForSubBlock = nullptr;
    juce::MidiBuffer directSlice;
    /*  CODEX-RTSAFETY P1: the oversized-host-block path sliced host MIDI into a
        local juce::MidiBuffer and called ensureSize on it in the callback. Both
        slice buffers are members now, reserved in prepare. */
    juce::MidiBuffer sliceMidi;
    RhythmEngine rhythm;
    CharacterEngine character;

    // ==== BEGIN REALISM-A state ====
    StringAging aging;
    EnvironmentModel environment;
    BodyCouplingBank bodyCoupling;
    std::array<double, kMaxStrings> bridgeWaves {};
    BridgeCoupling bridgeCoupling;
    BridgeCoupling partsBridge;
    double bodyFreqScale = 1.0, bodyQScale = 1.0, bodyMassScale = 1.0;
    double hostTimeSeconds = -1.0;
    bool hostTimePlaying = false;
    uint64_t agingSeed = 0;
    bool agingSeedValid = false;
    SetupGeometry setupWithGuitar;              ///< requestedSetup with the guitar's scale and strings
    SetupGeometry setupScratch;                 ///< requested + environment deltas
    std::atomic<bool> setupChanged { true };
    std::atomic<double> pendingBodyTap { 0.0 };
    // ==== END REALISM-A state ====

    // SPEC-SWEEP (RE-41): the rhythm engine's strokes as MIDI out.
    struct PendingRhythmMidi { juce::uint8 bytes[3]; int samplesFromNow; };
    static constexpr int kMaxPendingRhythmMidi = 128;
    std::array<PendingRhythmMidi, kMaxPendingRhythmMidi> pendingRhythmMidi {};
    int numPendingRhythmMidi = 0;
    juce::MidiBuffer* rhythmMidiOut = nullptr;
    void writeRhythmMidi (const PlayEventQueue& strokes) noexcept;
    void flushRhythmMidi (int numSamples) noexcept;

    /** The drift last written into the tuning engine, so a block that did not
        move it does not rewrite it. */
    std::array<double, kMaxStrings> lastAppliedDrift {};

    // SPEC-SWEEP: CW-18 / CW-19 - character-wear 5's jack and piezo saddles.
    std::array<double, kMaxStrings> saddleGain {};   ///< per-saddle piezo gain, set per block
    std::vector<double> piezoSumBuffer;              ///< the saddle-weighted string sum
    double jackGainNow = 1.0;                        ///< ramped towards CharacterEngine::getJackGain
    double jackRampCoeff = 0.01;                     ///< a 3 ms one-pole
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
        bool fingerAlternated = false;   ///< bass-techniques 6: already given its finger's timing
        bool fromRiff = false;           ///< riff-library 5.3: a riff audition event
        bool staggered = false;   ///< REALISM-B: string-interaction.md 4 delayed this note-off
        bool cancelled = false;   ///< REALISM-B: a new note on the string took it first

        int kind = 0;                    ///< FEAT-ASSIST: kDampingLift is auto-articulation.md 3.6's mute lift
        juce::uint32 serial = 0;         ///< FEAT-ASSIST: the note the lift belongs to
    };

    static constexpr int kDampingLift = 1;   // FEAT-ASSIST

    static constexpr int kMaxScheduledEvents = 192;
    std::array<ScheduledEvent, kMaxScheduledEvents> scheduled {};
    int numScheduled = 0;

    // --- instrument ----------------------------------------------------------
    std::array<StringEngine, kMaxStrings> strings;
    std::array<StringSpec, kMaxStrings> stringSpecs {};
    StringAge stringAge = StringAge::BrokenIn;
    std::array<double, kMaxStrings> customGauges {};

    // workshop-ui.md 3.3 (VISUAL-WORKSHOP-QA): a parts guitar's per-string
    // material and plain/wound overrides; -1 = the set's.
    std::array<int, kMaxStrings> partsStringMaterial {}, partsStringWound {};
    CouplingMatrix coupling;
    BodyEngine body;
    PickupEngine pickups;
    WhammyEngine whammy;

    // --- signal chain ---------------------------------------------------------
    GuitarCircuit circuit;
    CircuitComponents circuitControls;
    EffectsChain preEffects;
    AmpEngine amp;
    EffectsChain postEffects;
    CabinetEngine cabinet;
    RoomEngine room;

    // mic-placement.md 3: the acoustic external mics.
    AcousticMicModel acMic;
    ExpSmoother acMicMixSmooth;
    double acMicMixTarget = 0.0;
    bool acMicActive = false;
    std::vector<double> acMicBuffer;
    SecretEffect secret;
    MasterBus master;
    FreezeOverlay freezeOverlay;

    // --- per-block scratch (all pre-allocated) --------------------------------
    std::vector<double> stringSumBuffer;
    std::vector<double> magneticBuffer;
    std::vector<double> instrumentBuffer;
    std::vector<double> preCircuitBuffer;         ///< MODEL-GAPS: Aux 1's pre-circuit tap

    /*  CODEX-RTSAFETY P1: the pre/post pedal and hidden-effect stereo scratch were
        `static thread_local std::vector` grown with resize() inside the render,
        which allocated on the first block of each new host audio thread and when
        the hidden effect was first enabled mid-playback. Owned here and sized to
        maxBlock in prepare, they never allocate in the callback. */
    std::vector<double> pedalScratchL, pedalScratchR;
    std::vector<double> secretScratchL, secretScratchR;
    std::atomic<bool> auxDiPreCircuit { false };
    juce::AudioBuffer<float> bodyBuffer;
    juce::AudioBuffer<float> workBuffer;

    /** Holds the post-amp signal before the post-amp effects, so Aux 6 can be
        the difference between the two - the tails on their own. */
    juce::AudioBuffer<float> wetDryBuffer;

    std::array<double, kMaxStrings> bridgeOutputs {};
    std::array<double, kMaxStrings> couplingInputs {};
    std::array<double, kMaxStrings> stringOutputs {};

    // Playing noise (pick-noise.md 1.2): the click into each string's
    // excitation input, everything else onto its output before the body.
    PlayingNoise playingNoise;
    std::array<double, kMaxStrings> excitationNoise {}, surfaceNoise {};
    std::vector<double> noiseBuffer;
    juce::uint32 shiftCount = 0;

    FretBuzz fretBuzzModel;
    SlideEngine slide;

    // noise-floor.md 4: owned next to playingNoise.
    NoiseFloor noiseFloor;
    bool noiseFloorBypassed = false;

    // sustain-and-decay.md 7, and 3's clock restart when the E-Bow engages.
    StringEngine::SustainShape sustainShape;
    std::array<bool, kMaxStrings> ebowWasDriving {};
    bool feedbackWasOn = false;

    // tuning-stability.md 5.
    StabilityModel stability;
    bool stabilityBypassed = false;
    double partsTunerRatio = 18.0, partsTunerStability = 0.85, partsNutFriction = 0.35;
    bool partsTunerLocking = false;
    double capoPressure = 0.7, capoGapMm = 6.0;
    void refreshStabilityHardware() noexcept;
    void runStability (int numSamples) noexcept;

    /*  string-scraping.md 3: after the MIDI, before the strings. Its keyswitches
        come out of the MIDI (into scrapeMidi) before the rhythm engine and the
        interpreter see it. */
    ScrapeEngine scrape;
    juce::MidiBuffer scrapeMidi;

    /*  engine-technique-layer.md 3.1: the technique layer's MIDI front, then
        the slap (string-slap-technique.md 2), alongside the scrape. */
    TechniqueTriggers techniqueTriggers;
    juce::MidiBuffer techniqueMidi;
    SlapEngine slap;
    std::vector<double> slapBodyDrive;
    std::array<bool, kMaxStrings> scrapeWasActive {};

    // ---- MODEL-GAPS: capture reporting and fingerstyle bass --------------------
    PerformanceCapture* perfCapture = nullptr;
    IrSlot* bodyIrSlot = nullptr;           // SPEC-SWEEP TM-6
    std::vector<float> bodyIrInput;         // SPEC-SWEEP TM-6: the body's excitation, kept for the IR
    juce::int64 hostBlockStart = 0;
    ChordSymbol lastCapturedChord;
    double lastCapturedBarFret = -2.0;
    int lastCapturedBarPressure = -1;
    BassFingerstyle bassFingers;
    bool firingAlternated = false;
    std::array<juce::int64, kMaxStrings> lastPluckSample {};

    /** The capture's offset for the event being applied, from the host block's start. */
    int captureOffset() const noexcept { return (int) juce::jmax ((juce::int64) 0, blockStartSample + activeSampleOffset - hostBlockStart); }

    /** Reports a slap strike as a BASS_TECH event. */
    void captureBassTechnique (const SlapStrike& strike) noexcept;

    /** The chord (Poly mode) and the slide bar, once a block. */
    void captureBlockState() noexcept;
    // --- TECHNIQUES (engine-technique-layer.md; LuthierEngineTechniques.cpp) ----------
    TechniqueLayer techniqueLayer;
    std::array<juce::int64, kMaxStrings> lastStrikeSample {};
    void techniqueBeginBlock (int numSamples, const juce::MidiBuffer& played) noexcept;
    void techniqueStampEvents (PlayEventQueue& queue, bool fromRhythm) noexcept;
    void techniqueStrike (const NoteOnEvent& e, int s, bool slapStruck) noexcept;
    bool techniqueNoteOff (int s) noexcept;
    double techniqueFret (int s) const noexcept;
    double techniqueBendCents (int s, double interpreterBend) noexcept;
    void playTapEvent (const TapEvent& e) noexcept;

    /** Applies one of the slap's due actions (a strike, a palm slap, a body tap). */
    void applySlapAction (const SlapAction& a) noexcept;

    /** Plays a slap strike on its string at the string's current pitch. */
    void playSlapStrike (const SlapStrike& strike, double pitchHz, double fret) noexcept;

    /*  Each note's own sustain multiplier - dead spots, fret wear, the nut, a
        slide's damping - set when it starts. Per-block modulation multiplies
        into this rather than overwriting it, which it used to do, silently
        undoing every character-wear sustain change a block after the note
        began. */
    std::array<double, kMaxStrings> noteSustainScale {};

    // A parts guitar's values the compiled spec has no slot for.
    void applySpec();
    bool hasPartsOverride = false;
    BodyConfig partsBody;
    std::array<PickupSpec, 3> partsPickups {};

    // setPickupPlacementLive's hand-over: written by the message thread, read at block start.
    std::array<std::atomic<double>, 3> livePickupPosition {}, livePickupHeight {};
    std::atomic<int> livePickupDirty { 0 };   ///< bit per slot
    void applyLivePickupPlacements() noexcept;
    double partsSustain = 1.0;
    double fretBrightnessFactor = 1.0, nutBrightnessFactor = 1.0;
    double magnetSustain = 1.0, magnetDetuneCents = 0.0;
    std::array<double, kMaxStrings> stringDelays {};

    // --- articulation state ----------------------------------------------------
    std::array<double, kMaxStrings> currentFret {};
    std::array<double, kMaxStrings> targetFret {};
    std::array<int, kMaxStrings> stringMidiNote {};
    std::array<Lfo, kMaxStrings> vibratoLfo;
    std::array<double, kMaxStrings> vibratoAmount {};

    // animated-strings.md 4.1: what the display snapshot needs of each note.
    SoundingNotes soundingNotes;
    std::atomic<uint64_t> soundingPublishCount { 0 };
    std::array<int64_t, kMaxStrings> noteStartSample {};
    std::array<float, kMaxStrings> notePluckPosition {};
    std::array<uint8_t, kMaxStrings> noteStopKind {};
    std::array<double, kMaxStrings> slideStopFret {};     ///< the bar's contact the block used; < 0 = not under a bar
    std::array<double, kMaxStrings> fingerBendCents {};   ///< bend + vibrato, no whammy or slide (2.4)
    std::array<double, kMaxStrings> pitchOffsetCents {};  ///< the whole offset from the note, for the piano roll's key

    Excitation::Material pickMaterial = Excitation::Material::PickCelluloid;
    double pluckPosition = 0.16;

    /** The setup as last asked for. Its scale length and string count come from
        the guitar, so it is re-derived whenever those change (setNumStrings). */
    SetupGeometry requestedSetup;
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

    EBowDriver ebowDriver;

    FeedbackLoop feedbackLoop;
    std::vector<double> feedbackInjection;


    // gui-integration.md 3.4's tone strip (set from the parameters; smoothed per sample).
    std::atomic<double> inputGainTarget { 1.0 }, outputMixTarget { 1.0 }, widthTarget { 1.0 };
    double inputGainNow = 1.0, outputMixNow = 1.0, widthNow = 1.0;
    std::vector<double> dryBuffer;

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

    // --- click-free structural changes (ScopedStructuralChange) --------------------
    void beginStructuralChange();
    void endStructuralChange();

    /** Audio thread: the fade for the block just rendered, and the hand-offs. */
    void applySwapFade (juce::AudioBuffer<float>& buffer) noexcept;

    enum SwapState : int { swapIdle = 0, swapFadingOut, swapParked, swapFadingIn };
    std::atomic<int> swapState { swapIdle };
    std::atomic<juce::Thread::ThreadID> audioThreadId { nullptr };
    std::atomic<juce::uint32> lastProcessMs { 0 };
    double swapPhase = 1.0;          ///< audio thread; 1 = full level, 0 = silent
    // TODO 6e (MODEL-GAPS): the block-boundary part swap. The message thread
    // allocates and frees; the audio thread only takes and hands back.
    struct PendingPartSwap
    {
        DerivedAcoustics derived;
        GuitarType standsFor = GuitarType::Custom;
    };

    std::atomic<PendingPartSwap*> pendingPartSwap { nullptr };
    std::atomic<PendingPartSwap*> retiredPartSwap { nullptr };
    std::atomic<int> livePartSwaps { 0 };

    /** Audio thread, at a block boundary: the allocation-free half of applyWorkshopGuitar. */
    void applyPartSwapLive (const PendingPartSwap& swap) noexcept;

    /** Whether an audio thread other than the caller's is rendering. */
    bool isAudioRunningElsewhere() const noexcept;

    int structuralDepth = 0;         ///< message thread
    bool structuralParked = false;   ///< message thread: this scope parked the audio thread

    /*  MIDI that arrives while parked is kept and played at the start of the
        first block after it (DECISIONS C-09), not dropped. Bounded: past the
        reserve, further events wait for nothing and are lost. */
    static constexpr int kParkedMidiBytes = 16384;
    juce::MidiBuffer parkedMidi;

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

    // ---- cpu-quality-modes --------------------------------------------------------
    QualityProfile qualityProfile;
    std::array<double, kMaxStrings> qualityNotePeak {};
    std::array<bool, kMaxStrings> qualityRingOutEligible {};
    std::array<juce::int64, kMaxStrings> qualityLastExcite {};
    juce::int64 qualitySilentSamples = 0;
    int hardQualitySwitches = 0;
    void applyOversamplingForQuality (bool crossfade) noexcept;
    void qualityNoteOn (int stringIndex) noexcept;
    void qualityNoteOff (int stringIndex, bool heldOn) noexcept;
    void qualityPerBlock() noexcept;
    void qualityAfterBlock (const juce::AudioBuffer<float>& output) noexcept;
    // ==== BEGIN REALISM-B engine state ====
    HarmonicTouchSettings harmonicTouch;
    StringInteractionSettings interaction;
    RightHandSettings rightHand;
    Excitation::Material chosenPickMaterial = Excitation::Material::PickCelluloid;

    std::array<Excitation::Params, kMaxStrings> lastExcitation {};
    std::array<RhTool, kMaxStrings> lastTool {};
    std::array<bool, kMaxStrings> lastRest {};
    std::array<ContactDisplay, kMaxStrings> contactDisplay;
    std::array<int, kMaxStrings> contactDisplaySamples {};
    std::array<int, kMaxStrings> contactDisplayTotal {};
    std::array<std::atomic<float>, kMaxStrings> palmWeightDisplay {};

    /*  Damping another mechanism put on a string that is not its note's own
        (string-interaction.md 2 and 3, fingerstyle-attack.md 2): what to go
        back to, and why it is held. */
    struct BorrowedDamping
    {
        bool held = false;
        StringEngine::Damping prior = StringEngine::Damping::Open;
        double priorAmount = 1.0;
        juce::uint32 adjacentSources = 0;   ///< strings whose fretting finger lies here
        int restFrom = -1;                   ///< string whose rest stroke landed here
        bool palm = false;                   ///< under the palm (spread)
    };
    std::array<BorrowedDamping, kMaxStrings> borrowed {};

    void borrowDamping (int s, StringEngine::Damping d, double amount) noexcept;
    void releaseBorrowIfFree (int s) noexcept;
    void clearBorrowed (int s) noexcept;

    // Palm spread (string-interaction.md 2).
    double palmCentre = -1.0;
    int64_t palmGroupStart = -1000000;
    double palmGroupSum = 0.0;
    int palmGroupCount = 0;
    juce::uint32 struckThisBlock = 0;
    void notePalmStrike (int s) noexcept;
    void updatePalmSpread() noexcept;

    // Release stagger (string-interaction.md 4).
    RtRandom staggerRng { 0x57A66E5ull };
    int stageNoteOffs (const PlayEventQueue& queue, std::array<int64_t, PlayEventQueue::kCapacity>& due) noexcept;

    // Crosstalk (string-interaction.md 5).
    bool crosstalkWasBent = false;
    void updateCrosstalk() noexcept;

    // Alternation (fingerstyle-attack.md 3).
    int alternationPhase = 0;
    int64_t lastFingerNoteSample = -1000000;
    int64_t lastNoteOnSample = -1000000;
    int lastNoteOnString = -1;

    /** What the right hand resolved a note to. */
    struct HandResolution
    {
        RhTool tool = RhTool::global;
        bool rest = false;
        int slapType = -1;       ///< 0 thumb slap, 1 pop, -1 none
        double timingOffsetMs = 0.0;
    };

    HandResolution resolveRightHand (const NoteOnEvent& e, int s) noexcept;
    void applyRightHand (const HandResolution& hand, const NoteOnEvent& e, int s, StringEngine& str,
                         Excitation::Params& p, bool& fingersForNoise) noexcept;
    void applyHarmonicContact (const NoteOnEvent& e, int s, StringEngine& str, double fret,
                               Excitation::Params& p) noexcept;
    void applyAdjacentMute (const NoteOnEvent& e, int s, double fret) noexcept;
    void liftMutesFrom (int s) noexcept;
    void onRealismBNoteOn (int s) noexcept;
    void resetRealismB() noexcept;
    void refreshAirCoupling() noexcept;
    SlapStrike makeToolStrike (const NoteOnEvent& e, int slapType) const noexcept;
    // ==== END REALISM-B engine state ====

    // ==== BEGIN FEAT-ASSIST (auto-articulation.md; LuthierEngineAssist.cpp) ====
    bool assistPreArticulated = false;
    std::array<juce::uint32, kMaxStrings> assistNoteSerial {};
    std::array<juce::int64, kMaxStrings> assistLiftStart {};
    std::array<double, kMaxStrings> assistLiftFrom {};
    std::array<double, kMaxStrings> assistLastPitch {};
    void assistSetContext (bool rhythmPass) noexcept;
    void assistNoteStarted (const NoteOnEvent& e, int s, double fret) noexcept;
    void assistFireLift (const ScheduledEvent& e) noexcept;
    double assistPerBlockCents (int s, int numSamples, double& vib) noexcept;
    void assistReset() noexcept;
    // ==== END FEAT-ASSIST ====

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierEngine)
};

} // namespace luthier
