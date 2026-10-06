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
    // editions.md 2.2: Blackface Twin, Deluxe, Plexi, AC30, Rectifier,
    // SVT and Acoustic DI. Indices are kept in the engine's existing order.
    return index == 0 || index == 2 || index == 3 || index == 4
        || index == 6 || index == 10 || index == 12;
}

constexpr bool isFreePedalIndex (int index) noexcept
{
    // The first fifteen stock pedal choices are the Free set by contract.
    return index >= 0 && index < 15;
}

} // namespace luthier::edition
