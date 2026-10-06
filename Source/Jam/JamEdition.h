#pragma once

#include "../Edition.h"

/*  Jam mode's edition split (jam-mode.md 15, editions.md 2.3), as data.

    editions.md says the split itself is the coordinator's, done once every
    feature is complete (its section 9), and that until then all code is Pro.
    So this is the table the split will use and nothing more: which styles,
    kits, bass voices and outputs Free keeps, and the nearest Free choice a Pro
    preset plays in Free (editions 5.1 - the stored value is kept). The
    compile-time switch LUTHIER_FREE_EDITION is not defined by this tree's
    build; with it off nothing is ever locked.

    Free: Rock, Pop, Blues Shuffle, Ballad; Studio and Vintage kits; Finger and
    Pick bass (and Auto); the main output. Everything else is Pro. */

namespace luthier::JamEdition
{
    inline constexpr bool kIsFree = ! edition::isPro;

    enum class Item { style, kit, bassVoice, output };

    /** Whether Free has this choice (the choice index of its parameter). */
    constexpr bool isInFree (Item item, int index) noexcept
    {
        switch (item)
        {
            case Item::style:     return index == 0 || index == 1 || index == 3 || index == 8;   // Rock, Pop, Blues Shuffle, Ballad
            case Item::kit:       return index == 0 || index == 1;                               // Studio, Vintage
            case Item::bassVoice: return index >= 0 && index <= 2;                               // Auto, Finger, Pick
            case Item::output:    return index == 0;                                             // Main
        }

        return false;
    }

    /** What Free plays for a Pro choice: the nearest in feel. */
    constexpr int nearestFree (Item item, int index) noexcept
    {
        if (isInFree (item, index))
            return index;

        switch (item)
        {
            case Item::style:
            {
                //                            Rock Pop Funk Blues Country Metal Reggae Jazz Ballad EDM User
                constexpr int nearest[] = {   0,   1,  1,   3,    0,      0,    1,     3,   8,     1,  0 };
                return index >= 0 && index < 11 ? nearest[index] : 0;
            }
            case Item::kit:       return index == 3 ? 1 : 0;       // Jazz -> Vintage; Arena, Machine -> Studio
            case Item::bassVoice: return index == 4 ? 1 : 2;       // Upright -> Finger; Muted Pick -> Pick
            case Item::output:    return 0;
        }

        return 0;
    }

    /** True only in a Free build, for a Pro-only choice. */
    constexpr bool isLocked (Item item, int index) noexcept
    {
        return kIsFree && ! isInFree (item, index);
    }

    /** What the lock's upsell panel says (editions 4.1). */
    inline const char* upsellText() noexcept
    {
        return "Available in Luthier Pro: every Jam style and your own, the Arena, Jazz and Machine kits, "
               "Muted Pick and Upright bass, kit tuning and damping, separate outputs, and Jam MIDI out and export.";
    }
}
