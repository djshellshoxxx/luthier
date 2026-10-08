#pragma once

#include "../Edition.h"
#include <juce_gui_basics/juce_gui_basics.h>

/*  editions.md 4.1 / 7.4: the Free lists are the Pro lists filtered, never a
    second copy. A locked choice stays in its combo box, disabled, so the stored
    (Pro) value still shows what the preset asked for while the engine plays the
    nearest Free model (5.1.3). In Pro nothing is locked. */
namespace luthier::EditionLocks
{
    /** Disables every item whose choice index `isFree` rejects. Items are the
        parameter's choices in order, ids 1-based (LuthierChoice::attachTo). */
    template <typename IsFree>
    inline void lockItems (juce::ComboBox& box, IsFree isFree)
    {
        if constexpr (edition::isPro)
            return;

        for (int i = 0; i < box.getNumItems(); ++i)
            box.setItemEnabled (box.getItemId (i), isFree (i));
    }

    inline void lockGuitars (juce::ComboBox& box) { lockItems (box, [] (int i) { return edition::isFreeGuitarIndex (i) || i == 24; }); }   // Custom plays as designed (5.3)
    inline void lockAmps (juce::ComboBox& box)    { lockItems (box, [] (int i) { return edition::isFreeAmpIndex (i); }); }
    inline void lockPedals (juce::ComboBox& box)  { lockItems (box, [] (int i) { return edition::isFreePedalIndex (i); }); }
}
