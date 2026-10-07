#pragma once

#ifndef LUTHIER_PRO
 #define LUTHIER_PRO 1
#endif

namespace luthier::edition
{
inline constexpr bool isPro = (LUTHIER_PRO != 0);

enum class Feature
{
    workshop,
    tuneBuilder,
    techniquesTab,
    slideMode,
    slap,
    muteGrid,
    microtonal,
    toneMatch,
    notationExport,
    midiExport,
    liveMidiOut,
    advancedRanges,
    studioRouting,
    sidechain,
    deepCharacter,
    environment,
    tuningStability,
    noiseFloor,
    freezeEbow,
    setlists,
    morph,
    monitorMix,
    pedalCalibration,
    controllerProfiles,
    practiceTrainers,
    looperLayers,
    looperSave,
    renderCli,
    licensing
};

constexpr bool has (Feature) noexcept
{
    return isPro;
}

struct Limits
{
    int guitars;
    int ampModels;
    int pedalTypes;
    int rackSlotsPre;
    int rackSlotsPost;
    int snapshotBanks;
    int snapshotsPerBank;
    int lfos;
    int envFollowers;
    int macros;
    int modRoutes;
    int genreKits;
    int looperLayers;
    int looperSeconds;
    int audioExportSeconds;
    int userCabIrs;
};

inline constexpr Limits proLimits
{
    -1, -1, -1, 8, 8, -1, 8, -1, -1, 8, -1, -1, 8, 600, -1, -1
};

inline constexpr Limits freeLimits
{
    6, 7, 15, 4, 4, 1, 4, 2, 1, 2, 4, 8, 1, 60, 300, 1
};

inline constexpr Limits limits = isPro ? proLimits : freeLimits;
inline constexpr const char* productName = isPro ? "Luthier Pro" : "Luthier Free";
inline constexpr const char* contentFolder = isPro ? "Luthier" : "Luthier Free";

constexpr bool isFreeAmpIndex (int index) noexcept
{
    // editions.md 2.2 (owner decision 2026-10-06, Q-3: drawn at random):
    // American Twin, British 800, British Top-Boost 30, California Rectified,
    // German Four-Channel, Classic Bass 300 and Acoustic DI. Indices are the
    // AmpModel enum's order (AmpEngine.h).
    return index == 0 || index == 5 || index == 6 || index == 7
        || index == 9 || index == 11 || index == 12;
}

constexpr bool isFreeGuitarIndex (int index) noexcept
{
    // editions.md 2.1 (owner decision 2026-10-06, Q-3: drawn at random):
    // Classic T-Style, SG, Angular Korina, Baritone Electric, Auditorium and
    // J-Style Bass. Indices are the GuitarType enum's order (GuitarLibrary.h).
    return index == 1 || index == 3 || index == 6 || index == 10
        || index == 12 || index == 20;
}

constexpr bool isFreePedalIndex (int index) noexcept
{
    // editions.md 2.2: 15 of 22 - Compressor, Noise Gate, Wah, Overdrive,
    // Distortion, Fuzz, Boost, Volume, Chorus, Phaser, Tremolo, Delay, Reverb,
    // Spring Reverb, Graphic EQ. Indices are the PedalType enum's order
    // (Pedal.h); 0 is "None", which every slot may hold.
    switch (index)
    {
        case 0: case 1: case 2: case 3: case 7: case 8: case 9: case 10: case 11:
        case 12: case 13: case 15: case 17: case 18: case 19: case 20:
            return true;
        default:
            return false;
    }
}

/*  editions.md 5.1.3: what Free plays for a Pro choice - the nearest Free
    model of the same voicing family, for playback only. The stored value is
    never changed, so a preset round-tripped through Free keeps its Pro values
    (5.1.6). In Pro every function is the identity. */
constexpr int effectiveGuitarIndex (int index) noexcept
{
    if (isPro || isFreeGuitarIndex (index))
        return index;

    switch (index)
    {
        case 0:  return 1;    // Stratocaster -> Classic T-Style
        case 2:  return 3;    // Les Paul -> SG
        case 4:  return 3;    // ES-335 -> SG
        case 5:  return 1;    // Jazzmaster -> Classic T-Style
        case 7:  return 6;    // Ibanez RG -> Angular Korina
        case 8:  return 10;   // Seven-string -> Baritone Electric
        case 9:  return 10;   // Eight-string -> Baritone Electric
        case 11: case 13: case 14: case 15: case 16: case 17: case 18:
                 return 12;   // every other acoustic -> Auditorium
        case 19: case 21: case 22: case 23:
                 return 20;   // every other bass -> J-Style Bass
        default: return index;   // Custom (a user .luthierguitar plays as designed, 5.3)
    }
}

constexpr int effectiveAmpIndex (int index) noexcept
{
    if (isPro || isFreeAmpIndex (index))
        return index;

    switch (index)
    {
        case 1: case 2: case 3: return 0;    // Tweed, Deluxe, Champ -> American Twin
        case 4:  return 5;                   // Plexi -> British 800
        case 8:  return 7;                   // Bogner -> California Rectified
        case 10: return 5;                   // Orange -> British 800
        default: return index;               // Custom
    }
}

/** A Pro pedal type is bypassed in Free (5.1.2: "Rotary pedal bypassed"): the slot plays as None. */
constexpr int effectivePedalIndex (int index) noexcept
{
    return isPro || isFreePedalIndex (index) ? index : 0;
}

} // namespace luthier::edition
