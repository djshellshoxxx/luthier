#pragma once

/*  The cabinet's voice tables (mic-placement.md 2.2).

    The speaker and mic tables used to live in CabinetEngine.cpp's anonymous
    namespace. The placement model, the live response plot and the acoustic
    mic model all need them too, so they live here, with the two tables the
    placement model adds: each mic's polar pattern and HF directivity, and
    each cabinet's speaker layout.
*/

#include "CabinetEngine.h"

namespace luthier
{

//==============================================================================
struct SpeakerVoice
{
    const char* name;
    double lowCornerHz;    ///< Below this the cone stops moving air.
    double bodyHz;         ///< The chest-thump resonance.
    double bodyDb;
    double presenceHz;     ///< Cone breakup: the bite.
    double presenceDb;
    double topRollHz;      ///< Above this a guitar speaker is essentially deaf.
};

inline constexpr SpeakerVoice kSpeakers[(size_t) SpeakerType::NumSpeakers] =
{
    { "British 25 W Vintage",  95.0, 200.0,  3.0, 2100.0,  5.5, 4600.0 },
    { "British 60 W Modern",   88.0, 180.0,  2.5, 2600.0,  7.0, 5200.0 },
    { "British 30 W Heavy",    82.0, 165.0,  3.5, 2300.0,  6.0, 5000.0 },
    { "British 75 W",          92.0, 210.0,  2.0, 3000.0,  4.5, 5400.0 },
    { "American Alnico 12",   105.0, 240.0,  2.2, 1900.0,  5.0, 5600.0 },
    { "British Alnico 15 W",  110.0, 260.0,  1.8, 2400.0,  6.5, 6200.0 },
    { "American 300 W Heavy",  70.0, 150.0,  1.2, 3200.0,  3.0, 5800.0 },
    { "Bass Ceramic",          42.0,  95.0,  2.6, 1400.0,  2.0, 3600.0 }
};

struct MicVoice
{
    const char* name;
    double proximityHz;    ///< Where the proximity bump sits.
    double proximityDb;
    double presenceHz;
    double presenceDb;
    double topHz;
};

inline constexpr MicVoice kMics[(size_t) MicType::NumMics] =
{
    { "Classic Dynamic",   140.0, 2.0,  5500.0,  5.0, 14000.0 },
    { "Broadcast Dynamic", 120.0, 2.5,  4200.0,  2.5, 15000.0 },
    { "Wide Dynamic",      130.0, 3.5,  3800.0,  3.5, 16000.0 },
    { "Large Condenser",    95.0, 1.5,  9000.0,  3.0, 20000.0 },
    { "Ribbon",            110.0, 3.0,  6000.0, -3.5,  9000.0 },
    { "Studio Condenser",   90.0, 1.2, 10000.0,  3.5, 20000.0 },
    { "Kick Dynamic",       70.0, 4.0,  3200.0,  3.0,  9000.0 }
};

//==============================================================================
/** A mic's pickup pattern: `a + (1 - a) cos(theta)`, and how fast its top end
    falls off-axis relative to a small dynamic (mic-placement.md 2.2). */
struct MicPolar
{
    double a;   ///< 0.5 cardioid, 0 figure-8.
    double s;   ///< HF directivity scale.
};

inline constexpr MicPolar kMicPolar[(size_t) MicType::NumMics] =
{
    { 0.5, 1.0  },   // Classic Dynamic
    { 0.5, 1.0  },   // Broadcast Dynamic
    { 0.5, 1.0  },   // Wide Dynamic
    { 0.5, 1.25 },   // Large Condenser
    { 0.0, 0.8  },   // Ribbon: figure-8
    { 0.5, 1.2  },   // Studio Condenser
    { 0.5, 1.1  }    // Kick Dynamic
};

//==============================================================================
/** Speaker count, size and heights per cabinet (mic-placement.md 2.1), and the
    cabinet depth make_irs.py uses for its wall reflection. */
struct CabGeometry
{
    int numSpeakers;
    int columns;              ///< Speakers per row, left to right.
    double sizeInches;        ///< 10, 12 or 15.
    double rowHeightsM[4];    ///< Speaker-centre height above the floor, top row first.
    double depthM;
    bool openBack;
};

inline constexpr CabGeometry kCabGeometry[(size_t) CabinetType::NumCabinets] =
{
    { 1, 1, 12.0, { 0.45, 0.0,  0.0,  0.0  }, 0.28, true  },   // 1x12 open
    { 1, 1, 12.0, { 0.45, 0.0,  0.0,  0.0  }, 0.30, false },   // 1x12 closed
    { 2, 2, 12.0, { 0.35, 0.0,  0.0,  0.0  }, 0.30, true  },   // 2x12 open
    { 2, 2, 12.0, { 0.35, 0.0,  0.0,  0.0  }, 0.32, false },   // 2x12 closed
    { 4, 2, 12.0, { 0.62, 0.25, 0.0,  0.0  }, 0.36, false },   // 4x12
    { 4, 2, 12.0, { 0.62, 0.25, 0.0,  0.0  }, 0.36, false },   // 4x12 vintage
    { 1, 1, 15.0, { 0.30, 0.0,  0.0,  0.0  }, 0.42, false },   // 1x15 bass
    { 4, 2, 10.0, { 0.50, 0.22, 0.0,  0.0  }, 0.40, false },   // 4x10 bass
    { 8, 2, 10.0, { 0.90, 0.66, 0.42, 0.18 }, 0.46, false },   // 8x10 bass
    { 1, 1, 12.0, { 0.45, 0.0,  0.0,  0.0  }, 0.30, true  }    // Acoustic DI (no speaker to mic)
};

/** Effective cone and dust-cap radius in millimetres for a speaker size. */
inline double coneRadiusMm (double sizeInches) noexcept
{
    return sizeInches <= 10.5 ? 90.0 : sizeInches >= 14.0 ? 140.0 : 110.0;
}

inline double capRadiusMm (double sizeInches) noexcept
{
    return sizeInches <= 10.5 ? 30.0 : sizeInches >= 14.0 ? 42.0 : 38.0;
}

inline const CabGeometry& cabGeometry (CabinetType t) noexcept
{
    return kCabGeometry[(size_t) juce::jlimit (0, (int) CabinetType::NumCabinets - 1, (int) t)];
}

inline const SpeakerVoice& speakerVoice (SpeakerType t) noexcept
{
    return kSpeakers[(size_t) juce::jlimit (0, (int) SpeakerType::NumSpeakers - 1, (int) t)];
}

inline const MicVoice& micVoice (MicType t) noexcept
{
    return kMics[(size_t) juce::jlimit (0, (int) MicType::NumMics - 1, (int) t)];
}

inline const MicPolar& micPolar (MicType t) noexcept
{
    return kMicPolar[(size_t) juce::jlimit (0, (int) MicType::NumMics - 1, (int) t)];
}

/** The 1-based speaker a mic is on, falling back to speaker 1 when the stored
    index is past the cabinet's count (mic-placement.md 2.1). */
inline int resolveSpeaker (CabinetType t, int speaker) noexcept
{
    return (speaker >= 1 && speaker <= cabGeometry (t).numSpeakers) ? speaker : 1;
}

/** Height of a (resolved, 1-based) speaker's centre above the floor. */
inline double speakerHeightM (CabinetType t, int speaker) noexcept
{
    const auto& g = cabGeometry (t);
    const int s = resolveSpeaker (t, speaker);
    const int row = juce::jlimit (0, 3, (s - 1) / juce::jmax (1, g.columns));
    return g.rowHeightsM[row] > 0.0 ? g.rowHeightsM[row] : g.rowHeightsM[0];
}

} // namespace luthier
