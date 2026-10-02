#pragma once

/*  global-search.md 4.4: a value typed after a parameter's name.

    Grammar, applied to the last token (or the last two, for "442 Hz"):
      - a number, optionally with a unit: 7, -3 dB, 442 Hz, 0.442 kHz, 20 ms,
        0.02 s, +50 cents
      - a relative step: +1, -2
      - a percentage: 50%, that fraction of the live normalised range
      - a keyword: on, off, toggle, min, max, default, reset
      - the text of a choice option: "amp model plexi"

    Units are converted to the parameter's own unit (its label) and the number
    then goes through the parameter's own getValueForText, the same text
    function the right-click "Enter value" uses (ui-wiring 1). The live range
    is the clamp, so a parameter locked to its stock range (advanced-ranges 6.3)
    clamps at the stock edge.

    One convention the spec's examples need and the parameters do not carry: a
    unitless parameter declared on 0..1 (a knob on an amp face: gain, bass,
    treble...) is shown and typed on the 0-10 dial printed around the knob, so
    "gain 7" is 0.7 and reads "7.0". A percentage is still the normalised
    fraction. docs/coverage/FEAT-SEARCH.md records the decision.
*/

#include <juce_audio_processors/juce_audio_processors.h>

namespace luthier
{
struct PhysicalRange;
}

namespace luthier::search
{

class SearchIndex;
struct SearchItem;

struct ValueSpec
{
    enum class Kind { none, absolute, relative, percent, keyword, text };

    Kind kind = Kind::none;
    double number = 0.0;
    juce::String unit;       ///< lower case: "hz", "khz", "db", "ms", "s", "cents", "st"
    juce::String keyword;    ///< on / off / toggle / min / max / default
    juce::String text;       ///< the raw tail, for option matching

    bool isValid() const noexcept { return kind != Kind::none; }
};

/** One way of splitting a query into a name and a value. */
struct ValueSplit
{
    juce::String namePart;
    ValueSpec value;
};

/** What applying a value to a parameter would do. */
struct ResolvedValue
{
    bool ok = false;
    float normalised = 0.0f;
    bool clamped = false;
    bool atStockEdge = false;

    juce::String newText;     ///< "7.0", "442 Hz", "Modern RC"
    juce::String oldText;
    juce::String preview;     ///< "Set to 7.0 (now 5.0)"
    juce::String clampText;   ///< "Clamped to 10.0 (max)", or the locked-range notice
};

namespace InlineValue
{
    /** Parses one or two raw tokens ("7", "442", "Hz"...) as a value. */
    ValueSpec parse (const juce::StringArray& tailTokens);

    /** The splits of a raw query worth scoring, longest value first: the last
        two tokens (a number and its unit), then the last one, then up to three
        trailing words as option text. Empty when the query has one token. */
    std::vector<ValueSplit> splits (const juce::String& rawQuery);

    /** The 0-10 dial factor (10) for a unitless 0..1 float, else 1. */
    double displayScale (const juce::RangedAudioParameter& p);

    /** The value as the palette shows it: dial numbers for dial parameters,
        otherwise the parameter's own text and label. */
    juce::String displayText (const juce::RangedAudioParameter& p, float normalised);

    /** The control's arrow-key step in normalised units (4.4 nudges). */
    float arrowStep (const juce::RangedAudioParameter& p, bool fine);

    /** Resolves `value` against `p` at its current value. `lockedRange` is the
        physical range of a parameter held on its stock range, or nullptr: a
        clamp at a stock edge the advanced range goes past says so with
        `lockedNoticeText` (advanced-ranges 6.3). */
    ResolvedValue resolve (const ValueSpec& value, const juce::RangedAudioParameter& p,
                           const PhysicalRange* lockedRange = nullptr, const juce::String& lockedNoticeText = {});

    /** The keyword list, for the grammar tests. */
    const juce::StringArray& keywords();
}

//==============================================================================
/*  4.4's two readings: the whole query as a name, and name-part plus value.
    The value reading wins only when the name part's best match is a Parameter
    or Choice option scoring at least 400 and it outscores the whole-query
    reading - equal confidence is navigation (ground rule 4). */
struct ValueReading
{
    const SearchItem* item = nullptr;     ///< the parameter (or option) the name part found
    juce::String parameterId;
    juce::String namePart;
    ValueSpec value;
    ResolvedValue resolved;

    bool isValid() const noexcept { return item != nullptr && resolved.ok; }
};

namespace InlineValue
{
    using ParameterLookup = std::function<juce::RangedAudioParameter* (const juce::String&)>;
    /** The PhysicalRange of a parameter on its stock range, else nullptr. */
    using StockLockedTest = std::function<const PhysicalRange* (const juce::String&)>;

    constexpr double kMinimumNameScore = 400.0;

    ValueReading read (SearchIndex& index, const juce::String& rawQuery, const ParameterLookup& lookup,
                       const StockLockedTest& stockLocked = {}, const juce::String& lockedNoticeText = {});
}

} // namespace luthier::search
