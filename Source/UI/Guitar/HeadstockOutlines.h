#pragma once

/*  Headstock outlines (guitar-illustration.md 6), as data.

    Coordinates per headstock, in millimetres:
      a  from the nut toward the tip (a >= 0);
      b  lateral, NEGATIVE = bass side (the top of the screen), positive = treble.
    Drawn for a 43 mm nut (38 mm on basses): the renderer rescales b near the
    nut to the guitar's real nut width.

    `outline` is closed, from the nut's treble corner round the tip to the nut's
    bass corner, smoothed with the same centripetal Catmull-Rom as the bodies
    (a `corner` point keeps straight tangents). `posts` and `buttons` are in
    string order with index 0 the LOWEST string; the renderer maps them onto
    the engine's order (string 0 = high E).

    First draft by the lead; the outline assistant refines the shapes, keeping
    the types, ids and API.
*/

#include <cstring>

namespace luthier::outlines
{

struct HeadPt { float a, b; bool corner = false; };

enum class HeadLayout { inline6, inlineReverse, threeThree, twoTwo, threeOne, fourInline, slotted, sixSix, headless };

struct HeadstockStyle
{
    const char* id;
    HeadLayout layout;
    const HeadPt* outline; int numOutline;
    const HeadPt* posts; int numPosts;
    const HeadPt* buttons; int numButtons;
    float buttonLengthMm, buttonWidthMm;
    const HeadPt* trussCover; int numTrussCover;
    const HeadPt* slots[2]; int numSlot[2];
    HeadPt stringTree; bool hasStringTree;
};

//==============================================================================
namespace headstock_data
{
    // ---- in-line, tuners along the bass edge -----------------------------------
    inline constexpr HeadPt stratOutline[] = {
        { 0, 21.5f }, { 14, 22 }, { 32, 20 }, { 60, 23 }, { 95, 30 }, { 128, 35 }, { 154, 35 }, { 174, 29 },
        { 186, 16 }, { 190, 0 }, { 186, -15 }, { 175, -25 }, { 158, -30 }, { 130, -31 }, { 90, -30 },
        { 50, -29 }, { 22, -27 }, { 8, -24 }, { 0, -21.5f } };
    inline constexpr HeadPt stratPosts[] = { { 36, -18.5f }, { 58, -19 }, { 80, -19.5f }, { 102, -20 }, { 124, -20.5f }, { 146, -21 } };
    inline constexpr HeadPt stratButtons[] = { { 36, -44 }, { 58, -44 }, { 80, -44 }, { 102, -44 }, { 124, -44 }, { 146, -44 } };

    inline constexpr HeadPt teleOutline[] = {
        { 0, 21.5f }, { 16, 22 }, { 45, 21 }, { 85, 23 }, { 125, 27 }, { 152, 28 }, { 166, 24 },
        { 173, 13 }, { 173, -4 }, { 168, -18 }, { 152, -28 }, { 120, -29 }, { 70, -28 }, { 30, -27 },
        { 10, -24 }, { 0, -21.5f } };
    inline constexpr HeadPt telePosts[] = { { 38, -18.5f }, { 60, -19 }, { 82, -19.5f }, { 104, -20 }, { 126, -20.5f }, { 148, -21 } };
    inline constexpr HeadPt teleButtons[] = { { 38, -43 }, { 60, -43 }, { 82, -43 }, { 104, -43 }, { 126, -43 }, { 148, -43 } };

    inline constexpr HeadPt superOutline[] = {
        { 0, 21.5f }, { 20, 21 }, { 60, 22 }, { 110, 24 }, { 150, 21 }, { 184, 9 }, { 204, -12, true },
        { 190, -24 }, { 150, -30 }, { 90, -30 }, { 40, -28 }, { 10, -25 }, { 0, -21.5f } };
    inline constexpr HeadPt superPosts[] = { { 38, -18.5f }, { 60, -19 }, { 82, -19.5f }, { 104, -20 }, { 126, -20.5f }, { 148, -21 } };
    inline constexpr HeadPt superButtons[] = { { 38, -44 }, { 60, -44 }, { 82, -44 }, { 104, -44 }, { 126, -44 }, { 148, -44 } };

    inline constexpr HeadPt inline7Outline[] = {
        { 0, 21.5f }, { 20, 21 }, { 70, 22 }, { 125, 24 }, { 170, 21 }, { 204, 9 }, { 226, -12, true },
        { 210, -25 }, { 165, -31 }, { 100, -31 }, { 40, -29 }, { 10, -25 }, { 0, -21.5f } };
    inline constexpr HeadPt inline7Posts[] = { { 36, -19 }, { 57, -19.5f }, { 78, -20 }, { 99, -20.5f }, { 120, -21 }, { 141, -21.5f }, { 162, -22 } };
    inline constexpr HeadPt inline7Buttons[] = { { 36, -45 }, { 57, -45 }, { 78, -45 }, { 99, -45 }, { 120, -45 }, { 141, -45 }, { 162, -45 } };

    inline constexpr HeadPt inline8Outline[] = {
        { 0, 21.5f }, { 20, 21 }, { 75, 22 }, { 140, 24 }, { 190, 21 }, { 226, 9 }, { 248, -13, true },
        { 232, -26 }, { 180, -32 }, { 110, -32 }, { 40, -29 }, { 10, -25 }, { 0, -21.5f } };
    inline constexpr HeadPt inline8Posts[] = { { 34, -19 }, { 55, -19.5f }, { 76, -20 }, { 97, -20.5f }, { 118, -21 }, { 139, -21.5f }, { 160, -22 }, { 181, -22.5f } };
    inline constexpr HeadPt inline8Buttons[] = { { 34, -46 }, { 55, -46 }, { 76, -46 }, { 97, -46 }, { 118, -46 }, { 139, -46 }, { 160, -46 }, { 181, -46 } };

    inline constexpr HeadPt explorerOutline[] = {
        { 0, 21.5f }, { 40, 24 }, { 120, 29 }, { 196, 34, true }, { 212, 22, true }, { 196, -8 },
        { 160, -27, true }, { 90, -30 }, { 30, -28 }, { 0, -21.5f } };
    inline constexpr HeadPt explorerPosts[] = { { 38, -18.5f }, { 60, -19 }, { 82, -19.5f }, { 104, -20 }, { 126, -20.5f }, { 148, -21 } };
    inline constexpr HeadPt explorerButtons[] = { { 38, -44 }, { 60, -44 }, { 82, -44 }, { 104, -44 }, { 126, -44 }, { 148, -44 } };

    // ---- 3+3 ----------------------------------------------------------------------
    inline constexpr HeadPt lpOutline[] = {
        { 0, 21.5f }, { 15, 24 }, { 40, 33 }, { 80, 40 }, { 130, 43 }, { 166, 41 }, { 180, 31 },
        { 176, 13 }, { 168, 0, true }, { 176, -13 }, { 180, -31 }, { 166, -41 }, { 130, -43 },
        { 80, -40 }, { 40, -33 }, { 15, -24 }, { 0, -21.5f } };
    inline constexpr HeadPt lpPosts[] = { { 52, -27 }, { 88, -31 }, { 124, -33 }, { 124, 33 }, { 88, 31 }, { 52, 27 } };
    inline constexpr HeadPt lpButtons[] = { { 52, -54 }, { 88, -57 }, { 124, -59 }, { 124, 59 }, { 88, 57 }, { 52, 54 } };
    inline constexpr HeadPt bellCover[] = { { 6, -6 }, { 6, 6 }, { 30, 11 }, { 36, 6 }, { 36, -6 }, { 30, -11 } };

    inline constexpr HeadPt sgOutline[] = {
        { 0, 21.5f }, { 15, 24 }, { 40, 32 }, { 80, 38 }, { 130, 41 }, { 164, 39 }, { 177, 29 },
        { 173, 12 }, { 165, 0, true }, { 173, -12 }, { 177, -29 }, { 164, -39 }, { 130, -41 },
        { 80, -38 }, { 40, -32 }, { 15, -24 }, { 0, -21.5f } };

    inline constexpr HeadPt acousticOutline[] = {
        { 0, 21.5f }, { 10, 26 }, { 26, 35, true }, { 158, 39 }, { 170, 37, true }, { 173, 18 }, { 174, 0 },
        { 173, -18 }, { 170, -37, true }, { 158, -39 }, { 26, -35, true }, { 10, -26 }, { 0, -21.5f } };
    inline constexpr HeadPt acousticPosts[] = { { 50, -26 }, { 86, -27 }, { 122, -28 }, { 122, 28 }, { 86, 27 }, { 50, 26 } };
    inline constexpr HeadPt acousticButtons[] = { { 50, -52 }, { 86, -53 }, { 122, -54 }, { 122, 54 }, { 86, 53 }, { 50, 52 } };

    inline constexpr HeadPt archtopOutline[] = {
        { 0, 21.5f }, { 15, 25 }, { 45, 35 }, { 95, 42 }, { 150, 46 }, { 182, 44 }, { 194, 34 },
        { 190, 16 }, { 198, 6, true }, { 184, 0 }, { 198, -6, true }, { 190, -16 }, { 194, -34 },
        { 182, -44 }, { 150, -46 }, { 95, -42 }, { 45, -35 }, { 15, -25 }, { 0, -21.5f } };
    inline constexpr HeadPt archtopPosts[] = { { 56, -28 }, { 94, -32 }, { 132, -35 }, { 132, 35 }, { 94, 32 }, { 56, 28 } };
    inline constexpr HeadPt archtopButtons[] = { { 56, -55 }, { 94, -59 }, { 132, -62 }, { 132, 62 }, { 94, 59 }, { 56, 55 } };

    // ---- slotted --------------------------------------------------------------------
    inline constexpr HeadPt classicalOutline[] = {
        { 0, 21.5f }, { 10, 26 }, { 28, 29 }, { 168, 29 }, { 178, 26 }, { 186, 16 }, { 182, 8 },
        { 191, 0 }, { 182, -8 }, { 186, -16 }, { 178, -26 }, { 168, -29 }, { 28, -29 }, { 10, -26 },
        { 0, -21.5f } };
    inline constexpr HeadPt classicalPosts[] = { { 62, -15.5f }, { 97, -15.5f }, { 132, -15.5f }, { 132, 15.5f }, { 97, 15.5f }, { 62, 15.5f } };
    inline constexpr HeadPt classicalButtons[] = { { 62, -40 }, { 97, -40 }, { 132, -40 }, { 132, 40 }, { 97, 40 }, { 62, 40 } };
    inline constexpr HeadPt slotBass[] = { { 45, -20 }, { 150, -20 }, { 150, -11 }, { 45, -11 } };
    inline constexpr HeadPt slotTreble[] = { { 45, 11 }, { 150, 11 }, { 150, 20 }, { 45, 20 } };

    inline constexpr HeadPt gypsyOutline[] = {
        { 0, 21.5f }, { 10, 25 }, { 28, 28 }, { 160, 29 }, { 176, 25 }, { 184, 12 }, { 184, -12 },
        { 176, -25 }, { 160, -29 }, { 28, -28 }, { 10, -25 }, { 0, -21.5f } };

    // ---- 6+6 -------------------------------------------------------------------------
    inline constexpr HeadPt twelveOutline[] = {
        { 0, 23.5f }, { 10, 28 }, { 26, 36, true }, { 215, 40 }, { 226, 38, true }, { 229, 18 }, { 230, 0 },
        { 229, -18 }, { 226, -38, true }, { 215, -40 }, { 26, -36, true }, { 10, -28 }, { 0, -23.5f } };
    inline constexpr HeadPt twelvePosts[] = {
        { 40, -27 }, { 62, -27 }, { 84, -27 }, { 106, -27 }, { 128, -27 }, { 150, -27 },
        { 150, 27 }, { 128, 27 }, { 106, 27 }, { 84, 27 }, { 62, 27 }, { 40, 27 } };
    inline constexpr HeadPt twelveButtons[] = {
        { 40, -51 }, { 62, -51 }, { 84, -51 }, { 106, -51 }, { 128, -51 }, { 150, -51 },
        { 150, 51 }, { 128, 51 }, { 106, 51 }, { 84, 51 }, { 62, 51 }, { 40, 51 } };

    // ---- basses --------------------------------------------------------------------
    inline constexpr HeadPt bass4Outline[] = {
        { 0, 19 }, { 25, 19.5f }, { 60, 24 }, { 115, 32 }, { 165, 38 }, { 204, 38 }, { 222, 28 },
        { 227, 10 }, { 217, -10 }, { 197, -28 }, { 160, -36 }, { 115, -37 }, { 70, -36 }, { 30, -33 },
        { 10, -26 }, { 0, -19 } };
    inline constexpr HeadPt bass4Posts[] = { { 48, -21 }, { 88, -22 }, { 128, -23 }, { 168, -24 } };
    inline constexpr HeadPt bass4Buttons[] = { { 48, -55 }, { 88, -56 }, { 128, -57 }, { 168, -58 } };

    inline constexpr HeadPt bass5Outline[] = {
        { 0, 19 }, { 25, 19.5f }, { 70, 25 }, { 135, 33 }, { 195, 38 }, { 236, 37 }, { 254, 26 },
        { 258, 8 }, { 247, -12 }, { 225, -29 }, { 185, -37 }, { 130, -38 }, { 75, -37 }, { 30, -33 },
        { 10, -26 }, { 0, -19 } };
    inline constexpr HeadPt bass5Posts[] = { { 46, -21 }, { 84, -22 }, { 122, -23 }, { 160, -24 }, { 198, -25 } };
    inline constexpr HeadPt bass5Buttons[] = { { 46, -56 }, { 84, -57 }, { 122, -58 }, { 160, -59 }, { 198, -60 } };

    inline constexpr HeadPt bass22Outline[] = {
        { 0, 19 }, { 15, 24 }, { 40, 34, true }, { 150, 38 }, { 162, 30, true }, { 164, 0 },
        { 162, -30, true }, { 150, -38 }, { 40, -34, true }, { 15, -24 }, { 0, -19 } };
    inline constexpr HeadPt bass22Posts[] = { { 58, -25 }, { 112, -27 }, { 112, 27 }, { 58, 25 } };
    inline constexpr HeadPt bass22Buttons[] = { { 58, -58 }, { 112, -60 }, { 112, 60 }, { 58, 58 } };

    inline constexpr HeadPt bass31Outline[] = {
        { 0, 19 }, { 15, 23 }, { 40, 34, true }, { 90, 36 }, { 160, 36 }, { 176, 26 }, { 178, 0 },
        { 172, -26 }, { 160, -36 }, { 90, -38 }, { 40, -34, true }, { 15, -23 }, { 0, -19 } };
    inline constexpr HeadPt bass31Posts[] = { { 58, -25 }, { 104, -27 }, { 150, -28 }, { 70, 26 } };
    inline constexpr HeadPt bass31Buttons[] = { { 58, -58 }, { 104, -60 }, { 150, -61 }, { 70, 58 } };

    inline constexpr HeadPt headlessOutline[] = { { 0, 21.5f, true }, { 14, 21.5f, true }, { 14, -21.5f, true }, { 0, -21.5f, true } };
    inline constexpr HeadPt headlessPosts[] = { { 8, -15 }, { 8, -9 }, { 8, -3 }, { 8, 3 }, { 8, 9 }, { 8, 15 } };

    template <typename T, int N> constexpr int count (const T (&)[N]) { return N; }
}

//==============================================================================
inline const HeadstockStyle* findHeadstockStyle (const char* id) noexcept
{
    using namespace headstock_data;

    static const HeadstockStyle styles[] = {
        { "strat_inline6", HeadLayout::inline6, stratOutline, count (stratOutline), stratPosts, 6, stratButtons, 6, 16, 11,
          nullptr, 0, { nullptr, nullptr }, { 0, 0 }, { 96, -7 }, true },
        { "tele_inline6", HeadLayout::inline6, teleOutline, count (teleOutline), telePosts, 6, teleButtons, 6, 16, 11,
          nullptr, 0, { nullptr, nullptr }, { 0, 0 }, { 98, -7 }, true },
        { "superstrat_inline6", HeadLayout::inline6, superOutline, count (superOutline), superPosts, 6, superButtons, 6, 14, 10,
          nullptr, 0, { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "inline7", HeadLayout::inline6, inline7Outline, count (inline7Outline), inline7Posts, 7, inline7Buttons, 7, 14, 10,
          nullptr, 0, { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "inline8", HeadLayout::inline6, inline8Outline, count (inline8Outline), inline8Posts, 8, inline8Buttons, 8, 14, 10,
          nullptr, 0, { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "explorer_hockey", HeadLayout::inline6, explorerOutline, count (explorerOutline), explorerPosts, 6, explorerButtons, 6, 15, 11,
          bellCover, count (bellCover), { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "lp_33", HeadLayout::threeThree, lpOutline, count (lpOutline), lpPosts, 6, lpButtons, 6, 16, 12,
          bellCover, count (bellCover), { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "sg_33", HeadLayout::threeThree, sgOutline, count (sgOutline), lpPosts, 6, lpButtons, 6, 16, 12,
          bellCover, count (bellCover), { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "acoustic_33", HeadLayout::threeThree, acousticOutline, count (acousticOutline), acousticPosts, 6, acousticButtons, 6, 16, 12,
          bellCover, count (bellCover), { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "archtop_33", HeadLayout::threeThree, archtopOutline, count (archtopOutline), archtopPosts, 6, archtopButtons, 6, 17, 12,
          bellCover, count (bellCover), { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "classical_slotted", HeadLayout::slotted, classicalOutline, count (classicalOutline), classicalPosts, 6, classicalButtons, 6, 14, 10,
          nullptr, 0, { slotBass, slotTreble }, { count (slotBass), count (slotTreble) }, {}, false },
        { "gypsy_slotted", HeadLayout::slotted, gypsyOutline, count (gypsyOutline), classicalPosts, 6, classicalButtons, 6, 14, 10,
          nullptr, 0, { slotBass, slotTreble }, { count (slotBass), count (slotTreble) }, {}, false },
        { "resonator_slotted", HeadLayout::slotted, classicalOutline, count (classicalOutline), classicalPosts, 6, classicalButtons, 6, 14, 10,
          nullptr, 0, { slotBass, slotTreble }, { count (slotBass), count (slotTreble) }, {}, false },
        { "twelve_66", HeadLayout::sixSix, twelveOutline, count (twelveOutline), twelvePosts, 12, twelveButtons, 12, 13, 10,
          bellCover, count (bellCover), { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "bass_inline4", HeadLayout::fourInline, bass4Outline, count (bass4Outline), bass4Posts, 4, bass4Buttons, 4, 22, 18,
          nullptr, 0, { nullptr, nullptr }, { 0, 0 }, { 118, -5 }, true },
        { "bass_inline5", HeadLayout::fourInline, bass5Outline, count (bass5Outline), bass5Posts, 5, bass5Buttons, 5, 22, 18,
          nullptr, 0, { nullptr, nullptr }, { 0, 0 }, { 140, -5 }, true },
        { "bass_22", HeadLayout::twoTwo, bass22Outline, count (bass22Outline), bass22Posts, 4, bass22Buttons, 4, 22, 18,
          bellCover, count (bellCover), { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "bass_31", HeadLayout::threeOne, bass31Outline, count (bass31Outline), bass31Posts, 4, bass31Buttons, 4, 22, 18,
          nullptr, 0, { nullptr, nullptr }, { 0, 0 }, {}, false },
        { "headless", HeadLayout::headless, headlessOutline, count (headlessOutline), headlessPosts, 6, nullptr, 0, 0, 0,
          nullptr, 0, { nullptr, nullptr }, { 0, 0 }, {}, false },
    };

    if (id == nullptr)
        return nullptr;

    for (const auto& s : styles)
        if (std::strcmp (s.id, id) == 0)
            return &s;

    return nullptr;
}

/** The headstock a body style carries by default (guitar-illustration.md 6). Never null. */
inline const char* defaultHeadstockFor (const char* bodyStyleId, int numStrings) noexcept
{
    auto is = [bodyStyleId] (const char* s) { return bodyStyleId != nullptr && std::strcmp (bodyStyleId, s) == 0; };
    auto startsWith = [bodyStyleId] (const char* s) { return bodyStyleId != nullptr && std::strncmp (bodyStyleId, s, std::strlen (s)) == 0; };

    if (numStrings >= 12)                                             return "twelve_66";
    if (is ("headless_bass"))                                         return "headless";

    if (startsWith ("bass") || is ("acoustic_bass"))
    {
        if (is ("bass_hollow") || is ("bass_violin") || is ("acoustic_bass"))  return "bass_22";
        if (is ("bass_musicman"))                                    return "bass_31";
        return numStrings >= 5 ? "bass_inline5" : "bass_inline4";
    }

    if (numStrings == 7)                                             return "inline7";
    if (numStrings >= 8)                                             return "inline8";

    if (is ("double_cutaway_offset") || is ("offset") || is ("reverse_firebird"))  return "strat_inline6";
    if (is ("single_cutaway_slab"))                                  return "tele_inline6";
    if (is ("superstrat") || is ("multiscale"))                      return "superstrat_inline6";
    if (is ("angular"))                                              return "explorer_hockey";
    if (is ("double_cutaway_thin"))                                  return "sg_33";
    if (is ("archtop"))                                              return "archtop_33";
    if (is ("single_cutaway_arched") || is ("double_cutaway_semi") || is ("flying_v"))  return "lp_33";
    if (is ("gypsy_jazz"))                                           return "gypsy_slotted";
    if (is ("classical") || is ("flamenco") || is ("cutaway_classical"))  return "classical_slotted";
    if (startsWith ("resonator"))                                    return "resonator_slotted";

    return "acoustic_33";
}

} // namespace luthier::outlines
