#pragma once

/*  bass-techniques.md 8: "Bass defaults differ everywhere" - MODEL-GAPS.

        | Spec                  | Guitar          | Bass            |
        | strum-dynamics 4      | 200 sps / 0.04  | 100 sps / 0.01  |  (StrumSettings::retargetDefaults)
        | string-squeak 7       | pressure 0.50   | 0.35            |
        | fret-buzz 6.1         | Player-friendly | Factory low     |
        | part-acoustics 3      | 628-648 mm      | 864 mm          |  (the neck part: every factory bass neck)
        | pick-noise 2          | 0.73 mm         | 1.14 mm         |
        | Compressor default    | off             | on, 2:1         |
        | pluck position (6)    | 0.16            | 0.12            |  (nearer the bridge)

    "These are defaults, not constraints." A family change moves what is still
    on the old family's default to the new one's; anything the user set stays.
    The strum's column already works that way (retargetStrumDefaults); this is
    the rest of the table, applied from the same place on a guitar load.

    Message thread. Written without gestures, like the rest of a guitar load.
*/

#include <juce_audio_processors/juce_audio_processors.h>

namespace luthier
{

struct BassFamilyDefaults
{
    double squeakPressure = 0.50;
    int setupStyle = 1;              ///< index into getSetupStyle: 1 Player-friendly, 0 Factory low
    double pickThicknessMm = 0.73;   ///< what the family's pick is (the parameter is normalised)
    double pluckPosition = 0.16;
    bool compressor = false;         ///< pre-amp slot 1 holds a 2:1 compressor
    double compressorRatio = 2.0;

    static BassFamilyDefaults forFamily (bool bass) noexcept;

    /** The pick_thickness parameter's value for a thickness, the inverse of
        Parameters::pickThicknessMm. */
    static double pickThicknessNormalised (double mm) noexcept;

    /** Moves what is still at `fromBass`'s default to `toBass`'s. */
    /** `keep` (optional) names parameters to leave alone whatever their value:
        the ones a host wrote together with the guitar type (a session restore
        or automation), which a family switch must not overwrite. */
    static void retarget (juce::AudioProcessorValueTreeState& state, bool fromBass, bool toBass,
                          const std::function<bool (const juce::String&)>& keep = {});
};

} // namespace luthier
