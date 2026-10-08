#pragma once

#include <array>

namespace luthier::tests::ascii_corpus
{
struct Fixture
{
    const char* name;
    const char* text;
    bool expectStaff;
    const char* expectedTuning;
    bool expectAmbiguousTuning;
    int minLegendEntries;
    int minChordLyricBlocks;
    int minMultiPartBlocks;
    int minDirectives;
};

inline constexpr std::array<Fixture, 24> fixtures {{
    { "01-clean-six-string",
      "e|--0--2--3--|\nB|--1--3--0--|\nG|--0--2--0--|\nD|--2--0--0--|\nA|--3--------|\nE|-----------|\n",
      true, "", false, 0, 0, 0, 0 },

    { "02-header-tuning",
      "Tuning: Drop D\ne|--0--|\nB|--3--|\nG|--2--|\nD|--0--|\nA|--0--|\nD|--0--|\n",
      true, "Drop D", false, 0, 0, 0, 0 },

    { "03-footer-tuning",
      "e|--0--|\nB|--3--|\nG|--2--|\nD|--0--|\nA|--0--|\nD|--0--|\nTuning: Drop D\n",
      true, "Drop D", false, 0, 0, 0, 0 },

    { "04-legend-after-tab",
      "e|--5/7--|\nB|-------|\nG|-------|\nD|-------|\nA|-------|\nE|-------|\nKey:\n/ = slide up\n",
      true, "", false, 1, 0, 0, 0 },

    { "05-legend-before-tab",
      "Legend:\n7B = bend\n7H = harmonic\n7^9 = hammer on\ne|--7B--7H--7^9--|\nB|----------------|\nG|----------------|\nD|----------------|\nA|----------------|\nE|----------------|\n",
      true, "", false, 3, 0, 0, 0 },

    { "06-html-entity-contamination",
      "e|--0--&nbsp;--|\nB|--1--&nbsp;--|\nG|--0--&nbsp;--|\nD|--2--&nbsp;--|\nA|--3--&nbsp;--|\nE|------&nbsp;--|\n",
      true, "", false, 0, 0, 0, 0 },

    { "07-markdown-contamination",
      "**e|--0--|**\n**B|--1--|**\n**G|--0--|**\n**D|--2--|**\n**A|--3--|**\n**E|-----|**\n",
      true, "", false, 0, 0, 0, 0 },

    { "08-wrapped-six-line-staff",
      "e|--0--1--|\nB|--1--3--|\nG|--0--2--|\nD|--2--0--|\nA|--3-----|\nE|--------|\n\ne|--3--5--|\nB|--3--5--|\nG|--4--5--|\nD|--5--7--|\nA|--5--7--|\nE|--3--5--|\n",
      true, "", false, 0, 0, 0, 0 },

    { "09-collapsed-rows",
      "E|--0--| A|--2--| D|--2--| G|--1--| B|--0--| e|--0--|\n",
      true, "", false, 0, 0, 0, 0 },

    { "10-unlabelled-continuation",
      "e|--0--|\nB|--0--|\nG|--1--|\nD|--2--|\nA|--2--|\nE|--0--|\n\n|--3--|\n|--3--|\n|--4--|\n|--5--|\n|--5--|\n|--3--|\n",
      true, "", false, 0, 0, 0, 0 },

    { "11-bass-four-string",
      "G|--0--2--|\nD|--0--2--|\nA|--2--0--|\nE|--3-----|\n",
      true, "", false, 0, 0, 0, 0 },

    { "12-seven-string",
      "e|--0--|\nB|--0--|\nG|--0--|\nD|--2--|\nA|--2--|\nE|--0--|\nB|--0--|\n",
      true, "", false, 0, 0, 0, 0 },

    { "13-alternate-tuning",
      "Tuning: DADGAD\ne|--0--|\nB|--0--|\nG|--0--|\nD|--0--|\nA|--0--|\nD|--0--|\n",
      true, "DADGAD", false, 0, 0, 0, 0 },

    { "14-multiple-guitars",
      "Guitar 1\ne|--0--|\nB|--0--|\nG|--1--|\nD|--2--|\nA|--2--|\nE|--0--|\n\nGuitar 2\ne|--3--|\nB|--3--|\nG|--4--|\nD|--5--|\nA|--5--|\nE|--3--|\n",
      true, "", false, 0, 0, 2, 0 },

    { "15-section-harmonic-instruction",
      "Verse:\nPlay harmonics throughout this section\ne|--12--|\nB|--12--|\nG|--12--|\nD|------|\nA|------|\nE|------|\n",
      true, "", false, 0, 0, 0, 1 },

    { "16-tremolo-trill-prose",
      "Solo:\nTremolo picking throughout, then trill the final note\ne|--7--8--|\nB|---------|\nG|---------|\nD|---------|\nA|---------|\nE|---------|\n",
      true, "", false, 0, 0, 0, 1 },

    { "17-fractional-harmonic-position",
      "e|--2.6--|\nB|-------|\nG|-------|\nD|-------|\nA|-------|\nE|-------|\n",
      true, "", false, 0, 0, 0, 0 },

    { "18-parenthesized-second-guitar",
      "e|--4(7)--(12\\0)--|\nB|----------------|\nG|----------------|\nD|----------------|\nA|----------------|\nE|----------------|\n",
      true, "", false, 0, 0, 0, 0 },

    { "19-chord-over-lyric",
      "A              C\nSmall words under simple chords\nG              D\nAnother short lyric line\n",
      false, "", false, 0, 0, 0, 0 },

    { "20-markup-damaged-embedded-chords",
      "Standard (EADGBE)\nAIn my view, Cdisplaced\nGIn disguise, no one knF#mows\n",
      false, "Standard", false, 0, 1, 0, 0 },

    { "21-malformed-partial-tab",
      "e|--0--2--|\nB|--1--3\nG this row lost its bar\nD|--2--0--|\n",
      true, "", false, 0, 0, 0, 0 },

    { "22-contradictory-tuning",
      "Tuning: Drop D\ne|--0--|\nB|--0--|\nG|--0--|\nD|--0--|\nA|--0--|\nD|--0--|\nTuning: DADGAD\n",
      true, "Drop D", true, 0, 0, 0, 0 },

    { "23-unknown-legend-symbol",
      "Key:\n@ = scrape behind the nut\n? = improvised noise\nSome prose remains intentionally unsupported.\n",
      false, "", false, 0, 0, 0, 0 },

    { "24-intentionally-ambiguous-prose",
      "A garden grows beside Gate 7.\nBuses arrive at 5 and 6.\nDo not interpret these sentences as fretted music.\n",
      false, "", false, 0, 0, 0, 0 }
}};

} // namespace luthier::tests::ascii_corpus
