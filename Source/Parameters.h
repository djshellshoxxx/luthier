#pragma once

/*  The automatable parameter set, and the bridge that pushes it into the engine.

    Design note: continuous parameters are polled once per block from their atomic
    pointers and pushed straight into the engine. That is cheaper and far more
    predictable than a listener per parameter, and it means automation always
    arrives in block order.

    Structural changes - a different guitar, a different pedal in a slot - can
    allocate, so they are detected here but applied on the message thread through
    an AsyncUpdater. The audio thread never allocates.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include "LuthierEngine.h"
#include "Modulation/ModMatrix.h"

namespace luthier
{

//==============================================================================
namespace ParamIDs
{
    // --- macros ---------------------------------------------------------------
    inline constexpr const char* macroAttack   = "macro_attack";
    inline constexpr const char* macroBody     = "macro_body";
    inline constexpr const char* macroDrive    = "macro_drive";
    inline constexpr const char* macroTone     = "macro_tone";
    inline constexpr const char* macroSpace    = "macro_space";
    inline constexpr const char* macroHumanize = "macro_humanize";

    /*  modulation-matrix 1.7 asks for eight assignable macros. Six of them are
        the named macros the instrument already had, which drive fixed groups of
        engine controls; the last two exist purely as modulation sources, so a
        user can build their own macro out of routes without having to give up
        one of the six that already does something. */
    inline constexpr const char* macroAssignA  = "macro_assign_a";
    inline constexpr const char* macroAssignB  = "macro_assign_b";

    inline constexpr int kNumMacros = 8;

    /** The six macros in a fixed order, so the MIDI-out CC broadcast and the
        modulation matrix can address them by index. */
    const char* macroByIndex (int index) noexcept;

    // --- instrument -----------------------------------------------------------
    inline constexpr const char* guitarType     = "guitar_type";
    inline constexpr const char* tuningPreset   = "tuning_preset";
    inline constexpr const char* temperament    = "temperament";
    inline constexpr const char* concertA       = "concert_a";

    /*  ambiguity-resolutions 4.5, and gui-integration 19 names TuningEngine as
        its home. A parameter rather than plain engine state because a capo is
        something a player moves between songs and automates between sections,
        and because section 19 puts it in Advanced column 1 GUITAR, which is a
        column of parameters. */
    inline constexpr const char* capoFret       = "capo_fret";

    inline constexpr const char* stringMaterial = "string_material";
    inline constexpr const char* stringGauge    = "string_gauge";
    inline constexpr const char* stringAge      = "string_age";
    inline constexpr const char* realismDetune  = "realism_detune";
    inline constexpr const char* intonationErr  = "intonation_error";
    inline constexpr const char* tuningDrift    = "tuning_drift";
    inline constexpr const char* fretless       = "fretless";
    inline constexpr const char* fretAction     = "fret_action";
    inline constexpr const char* fretBuzz       = "fret_buzz";
    inline constexpr const char* sustainScale   = "sustain_scale";
    inline constexpr const char* couplingAmount = "coupling_amount";

    // --- right hand -----------------------------------------------------------
    inline constexpr const char* useFingers    = "use_fingers";
    inline constexpr const char* pickMaterial  = "pick_material";
    inline constexpr const char* pickThickness = "pick_thickness";
    inline constexpr const char* pickAngle     = "pick_angle";
    inline constexpr const char* pluckPosition = "pluck_position";
    inline constexpr const char* nailVsFlesh   = "nail_vs_flesh";

    // --- string noise ---------------------------------------------------------
    inline constexpr const char* slideNoise    = "noise_slide";
    inline constexpr const char* fretNoise     = "noise_fret";
    inline constexpr const char* releaseNoise  = "noise_release";
    inline constexpr const char* bodyKnock     = "noise_body_knock";
    inline constexpr const char* pickNoise     = "noise_pick";
    inline constexpr const char* ampBuzz       = "noise_amp_buzz";

    // --- body -----------------------------------------------------------------
    inline constexpr const char* bodyMode      = "body_mode";
    inline constexpr const char* bodyAmount    = "body_amount";
    inline constexpr const char* bodyWidth     = "body_width";
    inline constexpr const char* bodyDepth     = "body_depth";
    inline constexpr const char* bodyTopThick  = "body_top_thickness";
    inline constexpr const char* bodySoundhole = "body_soundhole";
    inline constexpr const char* bodyBracing   = "body_bracing";
    inline constexpr const char* bodyTopWood   = "body_top_wood";
    inline constexpr const char* bodyBackWood  = "body_back_wood";
    inline constexpr const char* bodyAge       = "body_age";
    inline constexpr const char* bodyAirGain   = "body_air_gain";

    // --- pickups --------------------------------------------------------------
    inline constexpr const char* pickupSelector = "pickup_selector";
    inline constexpr const char* pickupBlend    = "pickup_blend";
    inline constexpr const char* guitarTone     = "guitar_tone";
    inline constexpr const char* guitarVolume   = "guitar_volume";
    inline constexpr const char* coilTap        = "coil_tap";
    inline constexpr const char* piezoMicBlend  = "piezo_mic_blend";

    juce::String pickupType (int slot);
    juce::String pickupPosition (int slot);
    juce::String pickupHeight (int slot);
    juce::String pickupMagnet (int slot);
    juce::String pickupVolume (int slot);

    // --- performance ----------------------------------------------------------
    inline constexpr const char* playingMode   = "playing_mode";
    inline constexpr const char* mpeEnabled    = "mpe_enabled";
    inline constexpr const char* bendRange     = "bend_range";
    inline constexpr const char* strumSpeed    = "strum_speed";
    inline constexpr const char* strumDir      = "strum_direction";
    inline constexpr const char* chordWindow   = "chord_window";
    inline constexpr const char* vibratoRate   = "vibrato_rate";
    inline constexpr const char* vibratoDepth  = "vibrato_depth";
    inline constexpr const char* vibratoShape  = "vibrato_shape";
    inline constexpr const char* legatoWindow  = "legato_window";
    inline constexpr const char* slideGuitar   = "slide_guitar";

    /*  Sustain (ambiguity-resolutions.md 2).

        Freeze and E-Bow are two different mechanisms with two different enables.
        Freeze captures a window and loops it; E-Bow drives the string at its own
        resonance. The old single `freeze` bool drove the E-Bow mechanism under the
        Freeze name, which is the ambiguity section 2 exists to settle, so it is
        now `ebow_enable` and Freeze is the overlay below. */
    inline constexpr const char* ebowEnable      = "ebow_enable";

    inline constexpr const char* freezeEnable    = "freeze_enable";
    inline constexpr const char* freezeCaptureMs = "freeze_capture_ms";
    inline constexpr const char* freezeLevel     = "freeze_level";
    inline constexpr const char* freezeAttackMs  = "freeze_attack_ms";
    inline constexpr const char* freezeReleaseMs = "freeze_release_ms";
    inline constexpr const char* freezeLpCutoff  = "freeze_lp_cutoff";
    inline constexpr const char* freezeHpCutoff  = "freeze_hp_cutoff";

    // --- whammy ---------------------------------------------------------------
    inline constexpr const char* bridgeType    = "bridge_type";
    inline constexpr const char* whammyPos     = "whammy_position";
    inline constexpr const char* whammyDown    = "whammy_down_range";
    inline constexpr const char* whammyUp      = "whammy_up_range";
    inline constexpr const char* whammySprings = "whammy_springs";
    inline constexpr const char* transposeLock = "transpose_lock";

    // --- cable ----------------------------------------------------------------
    inline constexpr const char* cableOn     = "cable_on";
    inline constexpr const char* cableLength = "cable_length";

    // --- guitar circuit (volume-knob-interaction.md 3) ---------------------------
    inline constexpr const char* cableQuality       = "cable_quality";
    inline constexpr const char* circuitVolumePot   = "circuit_volume_pot";
    inline constexpr const char* circuitTonePot     = "circuit_tone_pot";
    inline constexpr const char* circuitToneCap     = "circuit_tone_cap";
    inline constexpr const char* circuitPotTaper    = "circuit_pot_taper";
    inline constexpr const char* circuitTrebleBleed = "circuit_treble_bleed";
    inline constexpr const char* circuitBleedR      = "circuit_bleed_r";
    inline constexpr const char* circuitBleedC      = "circuit_bleed_c";
    inline constexpr const char* circuitBleedMode   = "circuit_bleed_mode";
    inline constexpr const char* circuitActive      = "circuit_active";
    inline constexpr const char* ampInputImpedance  = "amp_input_impedance";

    // --- pick noise (pick-noise.md 7) ------------------------------------------------
    inline constexpr const char* pickTipRadius    = "pick_tip_radius";
    inline constexpr const char* pickBevel        = "pick_bevel";
    inline constexpr const char* pickWear         = "pick_wear";
    inline constexpr const char* pickClickAmount  = "pick_click_amount";
    inline constexpr const char* pickChirpAmount  = "pick_chirp_amount";
    inline constexpr const char* pickScrapeAmount = "pick_scrape_amount";

    // --- finger squeak (string-squeak.md 9) --------------------------------------------
    inline constexpr const char* squeakAmount      = "squeak_amount";
    inline constexpr const char* squeakProbability = "squeak_probability";
    inline constexpr const char* squeakMoisture    = "squeak_finger_moisture";
    inline constexpr const char* squeakPressure    = "squeak_finger_pressure";
    inline constexpr const char* squeakMinTravel   = "squeak_min_travel";
    inline constexpr const char* squeakStyle       = "squeak_style";

    // --- setup and fret buzz (fret-buzz.md 7) ------------------------------------------
    inline constexpr const char* setupActionTreble = "setup_action_treble";
    inline constexpr const char* setupActionBass   = "setup_action_bass";
    inline constexpr const char* setupRelief       = "setup_relief";
    inline constexpr const char* setupFretHeight   = "setup_fret_height";
    inline constexpr const char* setupBuzzThreshold = "setup_buzz_threshold";
    inline constexpr const char* setupSitarMode    = "setup_sitar_mode";
    inline constexpr const char* setupStyle        = "setup_style";

    // --- slide (slide-guitar.md 7) -------------------------------------------------------
    // slide_enabled is the existing slide_guitar switch, re-pointed.
    inline constexpr const char* slideMode             = "slide_mode";
    inline constexpr const char* slidePressure         = "slide_pressure";
    inline constexpr const char* slideSlant            = "slide_slant";
    inline constexpr const char* slideDampingBehind    = "slide_damping_behind";
    inline constexpr const char* slideNoiseAmount      = "slide_noise_amount";
    inline constexpr const char* slideClankAmount      = "slide_clank_amount";
    inline constexpr const char* slideIntonationAssist = "slide_intonation_assist";

    /** Nut slot depth for string 1 (highest) to 6. A 12-string's pairs share. */
    juce::String setupNutDepth (int stringNumber);
    inline constexpr int kNumNutDepths = 6;

    // --- amp ------------------------------------------------------------------
    inline constexpr const char* ampModel    = "amp_model";
    inline constexpr const char* ampGain     = "amp_gain";
    inline constexpr const char* ampBass     = "amp_bass";
    inline constexpr const char* ampMid      = "amp_mid";
    inline constexpr const char* ampTreble   = "amp_treble";
    inline constexpr const char* ampPresence = "amp_presence";
    inline constexpr const char* ampMaster   = "amp_master";
    inline constexpr const char* ampBright   = "amp_bright";
    inline constexpr const char* ampMidBoost = "amp_mid_boost";
    inline constexpr const char* ampStandby  = "amp_standby";

    // --- cabinet --------------------------------------------------------------
    inline constexpr const char* cabOn         = "cab_on";
    inline constexpr const char* cabType       = "cab_type";
    inline constexpr const char* cabSpeaker    = "cab_speaker";
    inline constexpr const char* cabSpeakerAge = "cab_speaker_age";
    inline constexpr const char* micType       = "mic_type";
    inline constexpr const char* micPosition   = "mic_position";
    inline constexpr const char* micDistance   = "mic_distance";
    inline constexpr const char* dualMic       = "dual_mic";
    inline constexpr const char* micType2      = "mic_type_2";
    inline constexpr const char* micPosition2  = "mic_position_2";
    inline constexpr const char* micDistance2  = "mic_distance_2";
    inline constexpr const char* micBlend      = "mic_blend";
    inline constexpr const char* micWidth      = "mic_width";
    inline constexpr const char* micPhaseAlign = "mic_phase_align";

    // --- room -----------------------------------------------------------------
    inline constexpr const char* roomOn       = "room_on";
    inline constexpr const char* roomSize     = "room_size";
    inline constexpr const char* roomMaterial = "room_material";
    inline constexpr const char* roomBlend    = "room_blend";
    inline constexpr const char* roomDecay    = "room_decay";
    inline constexpr const char* roomWidth    = "room_width";

    // --- master ---------------------------------------------------------------
    inline constexpr const char* masterGain  = "master_gain";

    // gui-integration.md 3.4's tone strip and 3.3's character macro.
    inline constexpr const char* inputGain      = "input_gain";      ///< dB, into the rig
    inline constexpr const char* outputMix      = "output_mix";      ///< wet/dry: 1 = all rig, 0 = all DI
    inline constexpr const char* stereoWidth    = "stereo_width";    ///< 0 mono, 1 as is, 2 wide
    inline constexpr const char* macroCharacter = "macro_character"; ///< character-wear amount

    // ambiguity-resolutions.md 1.2: the physical feedback loop. feedback_on,
    // feedback_threshold and feedback_speed above belong to the heuristic it
    // replaced; they stay in the layout (automation is indexed) but do nothing.
    inline constexpr const char* feedbackAmount     = "feedback_amount";
    inline constexpr const char* feedbackDistance   = "feedback_distance";
    inline constexpr const char* feedbackAngle      = "feedback_angle";
    inline constexpr const char* feedbackFocus      = "feedback_focus";
    inline constexpr const char* feedbackOctaveBias = "feedback_octave_bias";
    inline constexpr const char* limiterOn   = "limiter_on";
    inline constexpr const char* oversample  = "oversampling";

    // --- humanisation ---------------------------------------------------------
    inline constexpr const char* humTiming    = "hum_timing";
    inline constexpr const char* humVelocity  = "hum_velocity";
    inline constexpr const char* humDetune    = "hum_detune";
    inline constexpr const char* humAttack    = "hum_attack";
    inline constexpr const char* humNoise     = "hum_noise";
    inline constexpr const char* humStrum     = "hum_strum";

    // --- feedback and doubler --------------------------------------------------
    inline constexpr const char* feedbackOn    = "feedback_on";
    inline constexpr const char* feedbackThres = "feedback_threshold";
    inline constexpr const char* feedbackSpeed = "feedback_speed";
    inline constexpr const char* doublerOn     = "doubler_on";
    inline constexpr const char* doublerAmount = "doubler_amount";

    // --- the hidden effect ------------------------------------------------------
    inline constexpr const char* secretOn       = "secret_on";
    inline constexpr const char* secretRate     = "secret_rate";
    inline constexpr const char* secretDepth    = "secret_depth";
    inline constexpr const char* secretFeedback = "secret_feedback";
    inline constexpr const char* secretMix      = "secret_mix";

    // --- effect slots ----------------------------------------------------------
    /** `post` selects the chain; `slot` 0-7; `param` 0-9. */
    juce::String slotType (bool post, int slot);
    juce::String slotBypass (bool post, int slot);
    juce::String slotMix (bool post, int slot);
    juce::String slotParam (bool post, int slot, int param);
}

//==============================================================================
class Parameters
{
public:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** Human-readable names for every choice list, shared by the layout and the UI. */
    static juce::StringArray guitarTypeNames();
    static juce::StringArray tuningNames();
    static juce::StringArray temperamentNames();

    /** "Off", then "Fret 1" to "Fret 12". Twelve is where a capo stops being a
        capo and starts being a different instrument. */
    static juce::StringArray capoNames();
    static juce::StringArray stringMaterialNames();
    static juce::StringArray stringGaugeNames();
    static juce::StringArray stringAgeNames();
    static juce::StringArray pickMaterialNames();
    static juce::StringArray bodyModeNames();
    static juce::StringArray bracingNames();
    static juce::StringArray woodNames();
    static juce::StringArray pickupTypeNames();
    static juce::StringArray potTaperNames();
    static juce::StringArray trebleBleedNames();
    static juce::StringArray bleedModeNames();
    static juce::StringArray cableQualityNames();
    static juce::StringArray squeakStyleNames();
    static juce::StringArray setupStyleNames();
    static juce::StringArray slideModeNames();

    /** slide-guitar.md 3: lap steel and dobro damp fully behind the bar. */
    static double defaultDampingBehind (int slideModeIndex) noexcept;

    /*  The pick's existing thickness and angle parameters are declared 0-1
        (advanced-ranges.md 1.0 keeps them that way); these are the physical
        values pick-noise.md 2 is written in. Both extend past the ends for the
        advanced range. */
    static double pickThicknessMm (double normalised) noexcept;
    static double pickAngleDegrees (double normalised) noexcept;

    /** "500k", "1.2M", "220" - how a guitarist writes a resistor. */
    static juce::String formatOhms (double ohms);
    static double parseOhms (const juce::String& text);
    static juce::StringArray magnetNames();
    static juce::StringArray pickupSelectorNames();
    static juce::StringArray playingModeNames();
    static juce::StringArray strumDirectionNames();
    static juce::StringArray vibratoShapeNames();
    static juce::StringArray bridgeTypeNames();
    static juce::StringArray ampModelNames();
    static juce::StringArray cabinetNames();
    static juce::StringArray speakerNames();
    static juce::StringArray micNames();
    static juce::StringArray micPositionNames();
    static juce::StringArray micDistanceNames();
    static juce::StringArray roomSizeNames();
    static juce::StringArray roomMaterialNames();
    static juce::StringArray pedalTypeNames();
    static juce::StringArray oversamplingNames();
};

//==============================================================================
/** Reads the parameter state each block and applies it to the engine. */
class ParameterBridge : private juce::AsyncUpdater
{
public:
    ParameterBridge (juce::AudioProcessorValueTreeState& state, LuthierEngine& engine);
    ~ParameterBridge() override;

    /** Caches every raw parameter pointer. Call once, after the APVTS is built. */
    void cachePointers();

    /** Called at the top of processBlock. Real-time safe. */
    void applyToEngine() noexcept;

    /** Applies everything including structural changes. Message thread only;
        used after a preset load. */
    void applyAllNow();

    /** True while a structural change is pending. */
    bool isStructuralChangePending() const noexcept { return structuralPending.load(); }

    /** Points the bridge at the modulation matrix. Every parameter the engine
        reads goes through value(), so this one hook is the whole of the
        matrix's audio-path integration: automation writes the base value, and
        modulation is added on top of it here, exactly as
        modulation-matrix.md section 7 describes. */
    void setModMatrix (ModMatrix* matrix) noexcept { modMatrix = matrix; }
    ModMatrix* getModMatrix() const noexcept { return modMatrix; }

    /** The unmodulated value, for the UI, which shows the control where
        automation put it rather than where modulation has pushed it. */
    float baseValue (const juce::String& id) const noexcept;

    /** The parameter's index in the processor's parameter list, or -1. */
    int parameterIndex (const juce::String& id) const noexcept;

    /*  guitar-workshop.md 0.6: a guitar type is a shortcut to a factory
        guitar file. When the type changes the bridge asks this to load it;
        returning true means the parts guitar was applied and its parts were
        written into the overlapping parameters, and the bridge re-reads them
        before applying the rest. Returning false (no file, or no loader, as
        in the offline renderer) falls back to the compiled guitar. Message
        thread. */
    std::function<bool (GuitarType)> onLoadGuitarType;

private:
    void handleAsyncUpdate() override;
    void applyStructural();

    /** Reads every structural selection into its cache; true if any moved. */
    bool readStructuralValues() noexcept;

    std::atomic<float>* raw (const juce::String& id) const noexcept;
    float value (const juce::String& id) const noexcept;

    juce::AudioProcessorValueTreeState& apvts;
    LuthierEngine& engine;

    juce::HashMap<juce::String, std::atomic<float>*> pointers;
    juce::HashMap<juce::String, int> indices;

    ModMatrix* modMatrix = nullptr;

    // Cached structural selections, so a change is detected exactly once.
    int lastGuitarType = -1;
    int lastTuning = -1;
    int lastStringMaterial = -1;
    int lastStringGauge = -1;
    int lastStringAge = -1;
    int lastBodyMode = -1;
    int lastBracing = -1;
    int lastTopWood = -1;
    int lastBackWood = -1;
    int lastAmpModel = -1;
    int lastCabType = -1;
    int lastSpeaker = -1;
    int lastMicType = -1, lastMicPos = -1, lastMicDist = -1;
    int lastMicType2 = -1, lastMicPos2 = -1, lastMicDist2 = -1;
    int lastRoomSize = -1, lastRoomMaterial = -1;
    int lastBridgeType = -1;
    int lastPlayingMode = -1;
    int lastTemperament = -1;

    /*  The capo is structural: it changes what every open string sounds and how
        many frets are left, so the engine has to be told rather than having it
        fall out of the next note. -1 so the first apply always runs. */
    int lastCapoFret = -1;
    int lastOversample = -1;
    int lastPickupType[PickupEngine::kMaxPickups] = { -1, -1, -1 };
    int lastPickupMagnet[PickupEngine::kMaxPickups] = { -1, -1, -1 };
    int lastSlotType[2][EffectsChain::kNumSlots] = {};
    bool structuralInitialised = false;

    std::atomic<bool> structuralPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterBridge)
};

} // namespace luthier
