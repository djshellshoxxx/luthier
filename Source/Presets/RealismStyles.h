#pragma once

/*  The style macros of noise-floor.md 3 and sustain-and-decay.md 6.1.

    A style writes its row of values when picked and reads "(modified)" after
    any edit, as squeak_style does. The tables live here, beside nothing else,
    so the parameter layout, the UI and the tests read one copy.

    A negative value means "leave this parameter alone" (the noise floor's Off
    style keeps the single-coil hum where the player put it).
*/

#include <juce_core/juce_core.h>

namespace luthier::RealismStyles
{

//==============================================================================
struct NoiseFloorStyle
{
    const char* name;
    double hum, fluorescent, passiveHiss, cable, radio, groundLoop, ampHiss, microphonics;
};

inline constexpr int kNumNoiseFloorStyles = 5;

inline const NoiseFloorStyle& getNoiseFloorStyle (int index)
{
    static const NoiseFloorStyle styles[kNumNoiseFloorStyles] =
    {
        { "Off",             -1.0,  0.0,  0.0, 0.0, 0.0,  0.0,  0.0, 0.0 },
        { "Studio, treated",  0.08, 0.0,  1.0, 0.0, 0.0,  0.0,  0.3, 0.0 },
        { "Home desk",        0.25, 0.15, 1.0, 0.2, 0.0,  0.15, 0.5, 0.1 },
        { "Club stage",       0.4,  0.35, 1.0, 0.5, 0.1,  0.3,  0.6, 0.3 },
        { "Vintage combo",    0.5,  0.0,  1.0, 0.3, 0.25, 0.1,  0.8, 0.5 },
    };

    return styles[juce::jlimit (0, kNumNoiseFloorStyles - 1, index)];
}

inline juce::StringArray noiseFloorStyleNames()
{
    juce::StringArray names;

    for (int i = 0; i < kNumNoiseFloorStyles; ++i)
        names.add (getNoiseFloorStyle (i).name);

    return names;
}

//==============================================================================
struct SustainStyle
{
    const char* name;
    double transient, attackMs, fastShare, fastRatio, tension, releaseMs, sagMm, ring;
};

inline constexpr int kNumSustainStyles = 6;

inline const SustainStyle& getSustainStyle (int index)
{
    static const SustainStyle styles[kNumSustainStyles] =
    {
        { "Legacy",              0.0, 30.0, 0.0,  0.2,  0.0, 0.0,  0.0, 0.0  },
        { "Electric, natural",   0.5, 25.0, 0.45, 0.20, 1.0, 15.0, 4.0, 0.0  },
        { "Acoustic, natural",   0.7, 35.0, 0.70, 0.12, 1.0, 20.0, 5.0, 0.0  },
        { "Nylon",               0.3, 50.0, 0.60, 0.15, 0.4, 30.0, 6.0, 0.05 },
        { "Compressed / modern", 0.3, 20.0, 0.15, 0.40, 0.6, 10.0, 3.0, 0.0  },
        { "Dead-string thud",    0.1, 15.0, 0.85, 0.08, 1.2, 25.0, 5.0, 0.1  },
    };

    return styles[juce::jlimit (0, kNumSustainStyles - 1, index)];
}

inline juce::StringArray sustainStyleNames()
{
    juce::StringArray names;

    for (int i = 0; i < kNumSustainStyles; ++i)
        names.add (getSustainStyle (i).name);

    return names;
}

} // namespace luthier::RealismStyles
