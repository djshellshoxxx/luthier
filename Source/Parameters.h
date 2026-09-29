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
#include "Jam/JamSettings.h"   // FEAT-JAM

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

    // ambiguity-resolutions.md 2.2: the E-Bow's own controls.
    inline constexpr const char* ebowStringMask = "ebow_string_mask";   ///< 0 = strings with a held note
    inline constexpr const char* ebowIntensity  = "ebow_intensity";
    inline constexpr const char* ebowHarmonic   = "ebow_harmonic";

    // ambiguity-resolutions.md 5.2: the preset morph slider, automatable.
    inline constexpr const char* presetMorphPosition = "preset_morph_position";
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

    // string-scraping.md 2: the SCRAPE technique's controls.
    inline constexpr const char* scrapeArmed       = "scrape_armed";
    inline constexpr const char* scrapeTrigger     = "scrape_trigger";
    inline constexpr const char* scrapeDirection   = "scrape_direction";
    inline constexpr const char* scrapeSweepSource = "scrape_sweep_source";
    inline constexpr const char* scrapeTriggerCc   = "scrape_trigger_cc";
    inline constexpr const char* scrapeSweepCc     = "scrape_sweep_cc";
    inline constexpr const char* scrapeStartMm     = "scrape_start_mm";
    inline constexpr const char* scrapeEndMm       = "scrape_end_mm";
    inline constexpr const char* scrapeDuration    = "scrape_duration";
    inline constexpr const char* scrapePressure    = "scrape_pressure";
    inline constexpr const char* scrapeTool        = "scrape_tool";
    inline constexpr const char* scrapeAngle       = "scrape_angle";
    inline constexpr const char* scrapeStringMask  = "scrape_string_mask";
    inline constexpr const char* scrapeRetrigger   = "scrape_retrigger";

    // strum-dynamics.md 7. strum_evenness is not here: it is the rhythm
    // engine's own state (kits set it). strum_speed above is superseded by
    // strum_crossing_sps and stays only because automation is indexed.
    inline constexpr const char* strumCrossingSps     = "strum_crossing_sps";
    inline constexpr const char* strumAcceleration    = "strum_acceleration";
    inline constexpr const char* strumUpVelocityRatio = "strum_up_velocity_ratio";
    inline constexpr const char* strumTilt            = "strum_tilt";
    inline constexpr const char* strumMissProbability = "strum_miss_probability";
    inline constexpr const char* strumStrikerDown     = "strum_striker_down";
    inline constexpr const char* strumStrikerUp       = "strum_striker_up";
    inline constexpr const char* chuckAmount          = "chuck_amount";
    inline constexpr const char* chuckDamping         = "chuck_damping";

    // bass-techniques.md 2-5 and string-slap-technique.md 1 (SlapEngine).
    inline constexpr const char* slapStrength           = "slap_strength";
    inline constexpr const char* slapPositionMm         = "slap_position_mm";
    inline constexpr const char* slapThumbHardness      = "slap_thumb_hardness";
    inline constexpr const char* slapFretContact        = "slap_fret_contact";
    inline constexpr const char* popStrength            = "pop_strength";
    inline constexpr const char* popPositionMm          = "pop_position_mm";
    inline constexpr const char* doubleThumpEnabled     = "double_thump_enabled";
    inline constexpr const char* doubleThumpUpRatio     = "double_thump_up_ratio";
    inline constexpr const char* ghostLevel             = "ghost_level";
    inline constexpr const char* ghostDamping           = "ghost_damping";
    inline constexpr const char* ghostAuto              = "ghost_auto";
    inline constexpr const char* ghostVelocityThreshold = "ghost_velocity_threshold";
    inline constexpr const char* slapArmed              = "slap_armed";
    inline constexpr const char* slapType               = "slap_type";
    inline constexpr const char* slapTrigger            = "slap_trigger";
    inline constexpr const char* slapVelocityZone       = "slap_velocity_zone";
    inline constexpr const char* slapTriggerCc          = "slap_trigger_cc";
    inline constexpr const char* slapGhostCc            = "slap_ghost_cc";
    inline constexpr const char* slapForce              = "slap_force";
    inline constexpr const char* slapPalmPositionMm     = "slap_palm_position_mm";
    inline constexpr const char* slapStringMask         = "slap_string_mask";
    inline constexpr const char* slapGhostMode          = "slap_ghost_mode";
    inline constexpr const char* slapReboundGap         = "slap_rebound_gap";
    inline constexpr const char* slapSnapBack           = "slap_snap_back";
    inline constexpr const char* slapBodyPart           = "slap_body_part";

    // ==== BEGIN REALISM-A params ====
    // string-aging.md 4
    inline constexpr const char* stringAgeHours         = "string_age_hours";
    inline constexpr const char* stringCorrosivity      = "string_corrosivity";
    inline constexpr const char* stringAgeDetail        = "string_age_detail";
    inline constexpr const char* stringCoating          = "string_coating";
    inline constexpr const char* stringAgeAccrual       = "string_age_accrual";
    // environment.md 5
    inline constexpr const char* envTemperatureC        = "env_temperature_c";
    inline constexpr const char* envTunedAtC            = "env_tuned_at_c";
    inline constexpr const char* envHumidityPct         = "env_humidity_pct";
    inline constexpr const char* envProfile             = "env_profile";
    inline constexpr const char* envClock               = "env_clock";
    // body-coupling.md 4
    inline constexpr const char* bodyCouplingAmount     = "body_coupling_amount";
    inline constexpr const char* bodyModeMassScale      = "body_mode_mass_scale";
    inline constexpr const char* bodyModeQScale         = "body_mode_q_scale";
    inline constexpr const char* bodyModeFreqScale      = "body_mode_freq_scale";
    inline constexpr const char* bodyCouplingModes      = "body_coupling_modes";
    // ==== END REALISM-A params ====

    // --- effect slots ----------------------------------------------------------
    /** `post` selects the chain; `slot` 0-7; `param` 0-9. */
    juce::String slotType (bool post, int slot);
    juce::String slotBypass (bool post, int slot);
    juce::String slotMix (bool post, int slot);
    juce::String slotParam (bool post, int slot, int param);

    // ==== BEGIN MODEL-GAPS params ====
    // bass-techniques.md 6 (fingerstyle-attack.md reuses both) and
    // ambiguity-resolutions 8 / routing-io 2's Aux 1 pre / post-circuit toggle.
    inline constexpr const char* fingerAlternationVariation = "finger_alternation_variation";
    inline constexpr const char* restStroke                 = "rest_stroke";
    inline constexpr const char* aux1PreCircuit             = "aux1_pre_circuit";
    // ==== END MODEL-GAPS params ====
    // ==== BEGIN REALISM-B params ====
    // harmonic-realism.md 5 (+8).
    inline constexpr const char* harmonicTouchPressure    = "harmonic_touch_pressure";
    inline constexpr const char* harmonicFingerWidth      = "harmonic_finger_width";
    inline constexpr const char* harmonicTouchTime        = "harmonic_touch_time";
    inline constexpr const char* harmonicBriefTouch       = "harmonic_brief_touch";
    inline constexpr const char* pinchThumbOffsetMm       = "pinch_thumb_offset_mm";
    inline constexpr const char* artificialHarmonicOffset = "artificial_harmonic_offset";
    inline constexpr const char* tappedHarmonicOffset     = "tapped_harmonic_offset";
    inline constexpr const char* harmonicNoteMapping      = "harmonic_note_mapping";

    // string-interaction.md 7 (+7).
    inline constexpr const char* couplingAirAmount        = "coupling_air_amount";
    inline constexpr const char* palmMuteSpread           = "palm_mute_spread";
    inline constexpr const char* adjacentMuteAmount       = "adjacent_mute_amount";
    inline constexpr const char* releaseStaggerMs         = "release_stagger_ms";
    inline constexpr const char* releaseStaggerBias       = "release_stagger_bias";
    inline constexpr const char* pickupApertureScale      = "pickup_aperture_scale";
    inline constexpr const char* mutedThumpLevel          = "muted_thump_level";

    // fingerstyle-attack.md 6 (+14).
    inline constexpr const char* fingerFleshReleaseMs     = "finger_flesh_release_ms";
    inline constexpr const char* fingerNailReleaseMs      = "finger_nail_release_ms";
    inline constexpr const char* thumbPositionOffset      = "thumb_position_offset";
    inline constexpr const char* restStrokeDamping        = "rest_stroke_damping";
    inline constexpr const char* rhStroke                 = "rh_stroke";
    inline constexpr const char* rhStyle                  = "rh_style";
    inline constexpr const char* thumbPalmMute            = "thumb_palm_mute";
    inline constexpr const char* hybridSnap               = "hybrid_snap";

    /** rh_string_tool_1 ... _6; string 1 is the high E (routing-io.md 3). */
    inline const char* rhStringTool (int n) noexcept
    {
        static constexpr const char* ids[] = { "rh_string_tool_1", "rh_string_tool_2", "rh_string_tool_3",
                                               "rh_string_tool_4", "rh_string_tool_5", "rh_string_tool_6" };
        return ids[juce::jlimit (1, 6, n) - 1];
    }

    /*  bass-techniques.md 11's IDs, which fingerstyle-attack.md 6 reuses and
        does not duplicate. Read if another workstream declares them. */
    inline constexpr const char* bassRestStroke             = "rest_stroke";
    // ==== END REALISM-B params ====
    // ==== BEGIN REALISM-C params ====
    // noise-floor.md 3 (noise_amp_buzz, "Single-coil Hum", is ampBuzz above).
    inline constexpr const char* noiseMainsHz         = "noise_mains_hz";
    inline constexpr const char* noisePlayerAngle     = "noise_player_angle";
    inline constexpr const char* noisePlayerDistance  = "noise_player_distance";
    inline constexpr const char* noiseFluorescent     = "noise_fluorescent";
    inline constexpr const char* noisePassiveHiss     = "noise_passive_hiss";
    inline constexpr const char* noiseCableMovement   = "noise_cable_movement";
    inline constexpr const char* noiseRadio           = "noise_radio";
    inline constexpr const char* noiseGroundLoop      = "noise_ground_loop";
    inline constexpr const char* noiseAmpHiss         = "noise_amp_hiss";
    inline constexpr const char* noiseMicrophonics    = "noise_microphonics";
    inline constexpr const char* noiseFloorToAux8     = "noise_floor_to_aux8";
    inline constexpr const char* noiseFloorStyle      = "noise_floor_style";

    // sustain-and-decay.md 6.
    inline constexpr const char* sustainAttackTransient = "sustain_attack_transient";
    inline constexpr const char* sustainAttackTime      = "sustain_attack_time";
    inline constexpr const char* sustainFastShare       = "sustain_fast_share";
    inline constexpr const char* sustainFastRatio       = "sustain_fast_ratio";
    inline constexpr const char* sustainTensionMod      = "sustain_tension_mod";
    inline constexpr const char* sustainReleaseTime     = "sustain_release_time";
    inline constexpr const char* sustainReleaseSag      = "sustain_release_sag";
    inline constexpr const char* sustainReleaseRing     = "sustain_release_ring";
    inline constexpr const char* sustainStyle           = "sustain_style";

    // tuning-stability.md 4.
    inline constexpr const char* stabilityAmount      = "stability_amount";
    inline constexpr const char* stabilitySettling    = "stability_settling";
    inline constexpr const char* stabilityNutBinding  = "stability_nut_binding";
    inline constexpr const char* stabilityBacklash    = "stability_backlash";
    inline constexpr const char* stabilitySaddleCreep = "stability_saddle_creep";
    inline constexpr const char* stabilityBendMemory  = "stability_bend_memory";
    inline constexpr const char* stabilityCapoBias    = "stability_capo_bias";
    inline constexpr const char* stabilityAutoRetune  = "stability_auto_retune";
    // ==== END REALISM-C params ====
    // ==== BEGIN TUNE-HELP-ONBOARDING params ====
    // tune-builder.md 14: section parameters the mod matrix and host automation
    // can move over the tune's timeline. Both at 0 leave every tune as written.
    inline constexpr const char* tuneFeelMod            = "tune_feel_mod";      ///< -1..1, added to each section's feel
    inline constexpr const char* tuneTempoDrift         = "tune_tempo_drift";   ///< -10..10 %, the tune's own clock
    // ==== END TUNE-HELP-ONBOARDING params ====

    // ==== BEGIN FEAT-JAM params ====
    // jam-mode.md 10: appended in this order and never reordered.
    inline constexpr const char* jamEnabled         = "jam_enabled";
    inline constexpr const char* jamPlay            = "jam_play";
    inline constexpr const char* jamFillNow         = "jam_fill_now";
    inline constexpr const char* jamStyle           = "jam_style";
    inline constexpr const char* jamVariation       = "jam_variation";
    inline constexpr const char* jamIntensity       = "jam_intensity";
    inline constexpr const char* jamFillEvery       = "jam_fill_every";
    inline constexpr const char* jamFollow          = "jam_follow";
    inline constexpr const char* jamPredict         = "jam_predict";
    inline constexpr const char* jamChordSource     = "jam_chord_source";
    inline constexpr const char* jamStartMode       = "jam_start_mode";
    inline constexpr const char* jamCountInBars     = "jam_count_in_bars";
    inline constexpr const char* jamStopOnSilence   = "jam_stop_on_silence";
    inline constexpr const char* jamSilenceBars     = "jam_silence_bars";
    inline constexpr const char* jamEnding          = "jam_ending";
    inline constexpr const char* jamDynamicsFollow  = "jam_dynamics_follow";
    inline constexpr const char* jamSwing           = "jam_swing";
    inline constexpr const char* jamHumanise        = "jam_humanise";
    inline constexpr const char* jamKit             = "jam_kit";
    inline constexpr const char* jamKitAuto         = "jam_kit_auto";
    inline constexpr const char* jamKitTuning       = "jam_kit_tuning";
    inline constexpr const char* jamKitDamping      = "jam_kit_damping";
    inline constexpr const char* jamKitRoom         = "jam_kit_room";
    inline constexpr const char* jamKitWidth        = "jam_kit_width";
    inline constexpr const char* jamKitPerspective  = "jam_kit_perspective";
    inline constexpr const char* jamBassVoice       = "jam_bass_voice";
    inline constexpr const char* jamBassTone        = "jam_bass_tone";
    inline constexpr const char* jamVolume          = "jam_volume";
    inline constexpr const char* jamBalance         = "jam_balance";
    inline constexpr const char* jamDrumsPan        = "jam_drums_pan";
    inline constexpr const char* jamBassPan         = "jam_bass_pan";
    inline constexpr const char* jamDrumsMute       = "jam_drums_mute";
    inline constexpr const char* jamBassMute        = "jam_bass_mute";
    inline constexpr const char* jamOutput          = "jam_output";

    /** The 34, in table order (JM-36). */
    inline constexpr const char* const jamParameters[] = {
        jamEnabled, jamPlay, jamFillNow, jamStyle, jamVariation, jamIntensity, jamFillEvery,
        jamFollow, jamPredict, jamChordSource, jamStartMode, jamCountInBars, jamStopOnSilence,
        jamSilenceBars, jamEnding, jamDynamicsFollow, jamSwing, jamHumanise, jamKit, jamKitAuto,
        jamKitTuning, jamKitDamping, jamKitRoom, jamKitWidth, jamKitPerspective, jamBassVoice,
        jamBassTone, jamVolume, jamBalance, jamDrumsPan, jamBassPan, jamDrumsMute, jamBassMute,
        jamOutput };
    inline constexpr int kNumJamParameters = 34;

    /** jam-mode 10: performance controls, kept out of presets, snapshots,
        morph and randomise, and off after a host-state reload. */
    inline bool isJamTransient (const juce::String& id) noexcept
    {
        return id == jamPlay || id == jamFillNow;
    }
    // ==== END FEAT-JAM params ====
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
    static juce::StringArray envProfileNames();   // environment.md 3.1 (REALISM-A)
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
class ParameterBridge : private juce::AsyncUpdater,
                        private juce::AudioProcessorParameter::Listener
{
public:
    ParameterBridge (juce::AudioProcessorValueTreeState& state, LuthierEngine& engine);
    ~ParameterBridge() override;

    /** Caches every raw parameter pointer. Call once, after the APVTS is built. */
    void cachePointers();

    /** Called at the top of processBlock. Real-time safe. */
    void applyToEngine() noexcept;

    // ==== BEGIN FEAT-JAM params ====
    /** jam-mode 10: the Jam parameters as JamSettings. The band belongs to
        the processor (0.2), so this is read every block whether or not the
        engine lock was taken. Real-time safe. */
    JamSettings readJam() const noexcept;
    // ==== END FEAT-JAM params ====

    /** Applies everything including structural changes. Message thread only;
        used after a preset load. */
    void applyAllNow();

    /** True while a structural change is pending. */
    bool isStructuralChangePending() const noexcept { return structuralPending.load(); }

    /*  Held by the message thread while it rebuilds engine structure (a guitar,
        a pedal, a body IR) and try-locked by the audio thread around the block.
        Without it the structural pass ran concurrently with processBlock and
        freed what the audio thread was using: pluginval's Automation test
        aborted with "double free or corruption", in ReverbPedal::rebuildLines
        and RoomEngine::rebuild (docs/audit/BETA_TEST_REPORT.md B-01). */
    juce::CriticalSection& getEngineLock() noexcept { return engineLock; }
    /** Runs a pending structural change now, on the calling (message) thread:
        a host that saves state straight after changing a guitar type must get
        the state that change produces, not the one before it. */
    void flushPendingStructuralChange() { handleUpdateNowIfNeeded(); }

    /** A preset has just written its pedal types AND their parameters: build
        those pedals keeping the parameters. The structural path otherwise
        writes a new pedal's defaults over its parameters - right for a pedal
        the player has just picked, wrong for one a preset brought with its
        settings (every factory preset's pedals loaded at their defaults).
        Message thread. */
    void adoptPedalTypesFromParameters();

private:
    void pushSlotParameters (bool post, int slot);

public:

    /** Points the bridge at the modulation matrix. Every parameter the engine
        reads goes through value(), so this one hook is the whole of the
        matrix's audio-path integration: automation writes the base value, and
        modulation is added on top of it here, exactly as
        modulation-matrix.md section 7 describes. */
    void setModMatrix (ModMatrix* matrix) noexcept { modMatrix = matrix; }
    ModMatrix* getModMatrix() const noexcept { return modMatrix; }

    /** True if `id` was written (by anyone) after the guitar type last was. */
    bool writtenSinceGuitarType (const juce::String& id) const noexcept;

    /** While false, writes are not stamped for writtenSinceGuitarType(): the
        processor's own guitar-parameter writes are the guitar's, not the host's. */
    void setStampingWrites (bool stamp) noexcept { stampingWrites.store (stamp, std::memory_order_relaxed); }


    /** The unmodulated value, for the UI, which shows the control where
        automation put it rather than where modulation has pushed it. */
    float baseValue (const juce::String& id) const noexcept;
    float baseValue (const char* id) const noexcept;

    /** The parameter's index in the processor's parameter list, or -1. */
    int parameterIndex (const juce::String& id) const noexcept;
    int parameterIndex (const char* id) const noexcept;

    /*  guitar-workshop.md 0.6: a guitar type is a shortcut to a factory
        guitar file. When the type changes the bridge asks this to load it;
        returning true means the parts guitar was applied and its parts were
        written into the overlapping parameters, and the bridge re-reads them
        before applying the rest. Returning false (no file, or no loader, as
        in the offline renderer) falls back to the compiled guitar. Message
        thread. */
    std::function<bool (GuitarType)> onLoadGuitarType;

    /*  Called around every structural pass on the message thread, outermost
        pass only: the processor fades its output out before (so a ringing note
        is not cut mid-cycle - a preset switch clicked at 0.32 of full scale,
        BETA_TEST_REPORT B-06) and back in after. */
    std::function<void()> beforeStructuralChange, afterStructuralChange;

private:
    void handleAsyncUpdate() override;
    void applyStructural();

    /** Reads every structural selection into its cache; true if any moved. */
    bool readStructuralValues() noexcept;

    std::atomic<float>* raw (const juce::String& id) const noexcept;
    float value (const juce::String& id) const noexcept;

    /*  engine.md 0 (no allocation on the audio thread): the audio thread reads
        parameters by their const char* IDs through this table, built on the
        message thread in cachePointers(). A juce::String built from a literal
        allocates, and applyToEngine used to build hundreds per block. */
    struct FastEntry
    {
        juce::uint64 hash = 0;
        std::string id;
        std::atomic<float>* pointer = nullptr;
        int index = -1;
    };

    static juce::uint64 hashId (const char* id) noexcept;
    const FastEntry* find (const char* id) const noexcept;
    std::atomic<float>* raw (const char* id) const noexcept;
    float value (const char* id) const noexcept;

    std::vector<FastEntry> fastTable;
    size_t fastMask = 0;

    // The pedal slots' IDs, built once so the audio thread never formats them.
    std::array<std::array<std::string, EffectsChain::kNumSlots>, 2> slotBypassIds, slotMixIds, slotTypeIds;
    std::array<std::array<std::array<std::string, Pedal::kMaxParams>, EffectsChain::kNumSlots>, 2> slotParamIds;
    std::array<std::string, PickupEngine::kMaxPickups> pickupTypeIds, pickupMagnetIds, pickupVolumeIds;
    std::array<std::string, ParamIDs::kNumNutDepths + 1> nutDepthIds;

    juce::AudioProcessorValueTreeState& apvts;
    LuthierEngine& engine;

    juce::HashMap<juce::String, std::atomic<float>*> pointers;
    juce::HashMap<juce::String, int> indices;

    /*  Which was written last on a pedal slot: its type, or its parameters. A
        load, a snapshot recall or a morph writes the type and then the
        settings that go with it; a player picking a pedal writes only the
        type. So a new pedal keeps its parameters when they were written after
        the type, and gets its own defaults when they were not. Counters, so
        the listener is lock-free on whatever thread sets the parameter. */
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int parameterIndex, bool starting) override;

    /*  guitar-workshop 0.6 / host-integration 3: the order the host wrote
        parameters in, per parameter. A guitar type is a shortcut that writes
        the parameters its parts overlap; one the host wrote after the type
        (a session restoring both, automation at the same time) is kept. */
    std::unique_ptr<std::atomic<juce::uint32>[]> lastWrite;
    std::unique_ptr<std::atomic<bool>[]> inGesture;       ///< per parameter: a player is holding it
    std::atomic<bool> guitarTypeByPlayer { false };        ///< the last guitar type write was a player's pick
    std::array<std::array<std::atomic<bool>, EffectsChain::kNumSlots>, 2> typeByPlayer {};
    int numLastWrite = 0;
    int guitarTypeIndex = -1;

    std::vector<int> slotOfParameter;      ///< parameter index -> (chain * slots + slot) * 16 + (param, or 15 for the type)
    std::atomic<juce::uint32> writeSerial { 0 };
    std::atomic<bool> stampingWrites { true };
    std::array<std::array<std::atomic<juce::uint32>, EffectsChain::kNumSlots>, 2> typeWritten {};
    std::array<std::array<std::atomic<juce::uint32>, EffectsChain::kNumSlots>, 2> paramsWritten {};
    juce::Array<juce::AudioProcessorParameter*> watched;

    ModMatrix* modMatrix = nullptr;

    juce::CriticalSection engineLock;
    int structuralDepth = 0;   ///< message thread: nesting of structural passes

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
