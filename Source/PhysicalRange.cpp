#include "PhysicalRange.h"
#include "Parameters.h"

namespace luthier
{

//==============================================================================
const char* getRangeFamilyName (RangeFamily family) noexcept
{
    switch (family)
    {
        case RangeFamily::amp:        return "amp";
        case RangeFamily::circuit:    return "circuit";
        case RangeFamily::squeak:     return "squeak";
        case RangeFamily::buzz:       return "buzz";
        case RangeFamily::pick:       return "pick";
        case RangeFamily::slide:      return "slide";
        case RangeFamily::modulation: return "modulation";
        case RangeFamily::numFamilies:
        default:                      return "none";
    }
}

RangeFamily rangeFamilyFromName (const juce::String& name) noexcept
{
    for (int i = 0; i < (int) RangeFamily::numFamilies; ++i)
        if (name == getRangeFamilyName ((RangeFamily) i))
            return (RangeFamily) i;

    return RangeFamily::none;
}

//==============================================================================
bool PhysicalRange::isValid() const noexcept
{
    // advanced-ranges.md 1: the advanced range contains the stock range, both
    // are non-degenerate, and the default is inside stock so a fresh preset
    // does not start out marked.
    return advancedMin <= stockMin
             && stockMin < stockMax
             && stockMax <= advancedMax
             && advancedMin < advancedMax
             && defaultValue >= stockMin
             && defaultValue <= stockMax;
}

juce::NormalisableRange<float> PhysicalRange::makeRange (bool advanced) const
{
    const float lo = advanced ? advancedMin : stockMin;
    const float hi = advanced ? advancedMax : stockMax;

    juce::NormalisableRange<float> range (lo, hi);

    /*  The skew centre is a fraction of the span, so it moves with the range
        and the knob keeps its feel when the range widens. */
    if (skew != 1.0f)
        range.setSkewForCentre (lo + (hi - lo) * skew);

    return range;
}

bool PhysicalRange::contains (float value, bool advanced) const noexcept
{
    return advanced ? (value >= advancedMin && value <= advancedMax)
                    : (value >= stockMin && value <= stockMax);
}

bool PhysicalRange::isOutsideStock (float value) const noexcept
{
    // A small tolerance, because a value written back through a normalisation
    // round trip lands a float epsilon or two off its own boundary.
    constexpr float tolerance = 1.0e-6f;

    return value < stockMin - tolerance || value > stockMax + tolerance;
}

//==============================================================================
namespace
{
    struct Entry { const char* id; PhysicalRange range; };

    /*  The table (advanced-ranges.md 3).

        Sparse: only the families whose parameters exist today. `squeak`,
        `buzz`, `pick` and `slide` acquire rows when their own specs' modules
        land, and `modulation` never does - its fields are ModMatrix state
        rather than parameters, so it is implemented as setter clamps
        (advanced-ranges.md 2.1).
    */
    const Entry& entryAt (int index)
    {
        static const Entry table[] =
        {
            // --- amp (advanced-ranges.md 3.1) -------------------------------
            // The panel is normalised 0-1 in this build rather than printed
            // 0-10; stock is the knob's full travel and advanced is past the
            // end of it. Rescaling to 0-10 would change the meaning of every
            // saved automation lane for a cosmetic gain.
            { ParamIDs::ampGain,     { 0.0f, 1.0f,  0.0f, 2.0f,  0.35f, 1.0f, RangeFamily::amp } },
            { ParamIDs::ampBass,     { 0.0f, 1.0f, -0.5f, 1.5f,  0.50f, 1.0f, RangeFamily::amp } },
            { ParamIDs::ampMid,      { 0.0f, 1.0f, -0.5f, 1.5f,  0.50f, 1.0f, RangeFamily::amp } },
            { ParamIDs::ampTreble,   { 0.0f, 1.0f, -0.5f, 1.5f,  0.50f, 1.0f, RangeFamily::amp } },
            { ParamIDs::ampPresence, { 0.0f, 1.0f,  0.0f, 2.0f,  0.40f, 1.0f, RangeFamily::amp } },
            { ParamIDs::ampMaster,   { 0.0f, 1.0f,  0.0f, 2.0f,  0.70f, 1.0f, RangeFamily::amp } },

            // --- circuit (advanced-ranges.md 3.2) ---------------------------
            // Stock is 0.5-15 m because that is what this parameter already
            // ships with (advanced-ranges.md 1.0): presets store normalised
            // values, so narrowing a shipped range would silently re-map every
            // saved preset.
            { ParamIDs::cableLength, { 0.5f, 15.0f, 0.0f, 100.0f, 3.0f, 0.4f, RangeFamily::circuit } },

            // The rest of the family, new with GuitarCircuit and so declared
            // fresh. Capacitors are in nF (see Parameters.cpp). The treble
            // bleed's selector is non-physical and has no row.
            { ParamIDs::circuitVolumePot,  { 100.0e3f, 1.0e6f, 1.0e3f,  10.0e6f, 500.0e3f, 0.24f, RangeFamily::circuit } },
            { ParamIDs::circuitTonePot,    { 100.0e3f, 1.0e6f, 1.0e3f,  10.0e6f, 500.0e3f, 0.24f, RangeFamily::circuit } },
            { ParamIDs::circuitToneCap,    { 10.0f,    100.0f, 1.0f,    1000.0f, 22.0f,    0.24f, RangeFamily::circuit } },
            { ParamIDs::ampInputImpedance, { 220.0e3f, 1.0e6f, 10.0e3f, 10.0e6f, 1.0e6f,   0.32f, RangeFamily::circuit } },

            // --- pick (pick-noise.md 7) -------------------------------------
            // Thickness and angle keep their shipped 0-1 declarations (1.0);
            // the advanced ends are where the physical mapping reaches the
            // spec's 0.1-10 mm and 89 degrees.
            { ParamIDs::pickThickness,    { 0.0f, 1.0f, -0.647f, 1.585f, 0.5f,  1.0f, RangeFamily::pick } },
            { ParamIDs::pickAngle,        { 0.0f, 1.0f,  0.0f,   1.483f, 0.35f, 1.0f, RangeFamily::pick } },
            { ParamIDs::pickTipRadius,    { 0.2f, 4.0f,  0.05f,  20.0f,  1.0f,  0.3f, RangeFamily::pick } },
            { ParamIDs::pickClickAmount,  { 0.0f, 1.0f,  0.0f,   4.0f,   0.5f,  1.0f, RangeFamily::pick } },
            { ParamIDs::pickChirpAmount,  { 0.0f, 1.0f,  0.0f,   4.0f,   0.4f,  1.0f, RangeFamily::pick } },
            { ParamIDs::pickScrapeAmount, { 0.0f, 1.0f,  0.0f,   4.0f,   0.25f, 1.0f, RangeFamily::pick } },

            // --- squeak (string-squeak.md 9) ----------------------------------
            { ParamIDs::squeakAmount,     { 0.0f, 1.0f,  0.0f,   4.0f,   0.25f, 1.0f, RangeFamily::squeak } },
            { ParamIDs::squeakMinTravel,  { 1.0f, 4.0f,  0.25f,  12.0f,  1.5f,  1.0f, RangeFamily::squeak } },

            // --- buzz (fret-buzz.md 7) ------------------------------------------
            { ParamIDs::setupActionTreble, { 1.0f,  3.0f, 0.2f,  10.0f, 1.6f,  1.0f, RangeFamily::buzz } },
            { ParamIDs::setupActionBass,   { 1.2f,  3.5f, 0.2f,  12.0f, 2.0f,  1.0f, RangeFamily::buzz } },
            { ParamIDs::setupRelief,       { -0.05f, 0.5f, -0.5f, 2.0f, 0.2f,  1.0f, RangeFamily::buzz } },
            { "setup_nut_depth_1",         { 0.0f,  1.2f, 0.0f,  4.0f,  0.45f, 1.0f, RangeFamily::buzz } },
            { "setup_nut_depth_2",         { 0.0f,  1.2f, 0.0f,  4.0f,  0.45f, 1.0f, RangeFamily::buzz } },
            { "setup_nut_depth_3",         { 0.0f,  1.2f, 0.0f,  4.0f,  0.45f, 1.0f, RangeFamily::buzz } },
            { "setup_nut_depth_4",         { 0.0f,  1.2f, 0.0f,  4.0f,  0.45f, 1.0f, RangeFamily::buzz } },
            { "setup_nut_depth_5",         { 0.0f,  1.2f, 0.0f,  4.0f,  0.45f, 1.0f, RangeFamily::buzz } },
            { "setup_nut_depth_6",         { 0.0f,  1.2f, 0.0f,  4.0f,  0.45f, 1.0f, RangeFamily::buzz } },
            { ParamIDs::setupFretHeight,   { 0.6f,  1.6f, 0.1f,  5.0f,  1.0f,  1.0f, RangeFamily::buzz } },

            // --- slide (slide-guitar.md 7) ----------------------------------
            { ParamIDs::slideSlant,        { -30.0f, 30.0f, -60.0f, 60.0f, 0.0f,  0.5f, RangeFamily::slide } },
            { ParamIDs::slideNoiseAmount,  { 0.0f,   1.0f,  0.0f,   4.0f,  0.4f,  1.0f, RangeFamily::slide } },
            { ParamIDs::slideClankAmount,  { 0.0f,   1.0f,  0.0f,   4.0f,  0.45f, 1.0f, RangeFamily::slide } },

            // ==== BEGIN REALISM-B ranges ====
            // harmonic-realism.md 5: right- and left-hand contact joins pick.
            // Touch pressure's ceiling stays 1: above it (3) is not convex.
            { ParamIDs::harmonicTouchPressure, { 0.2f,   1.0f,   0.0f,  1.0f,   0.6f,    1.0f, RangeFamily::pick } },
            { ParamIDs::harmonicFingerWidth,   { 1.0f,   6.0f,   0.1f,  20.0f,  2.5f,    1.0f, RangeFamily::pick } },
            { ParamIDs::harmonicTouchTime,     { 20.0f,  200.0f, 1.0f,  1000.0f, 70.0f,  1.0f, RangeFamily::pick } },
            { ParamIDs::harmonicBriefTouch,    { 3.0f,   20.0f,  0.5f,  100.0f, 8.0f,    1.0f, RangeFamily::pick } },
            { ParamIDs::pinchThumbOffsetMm,    { 2.0f,   12.0f,  0.0f,  40.0f,  6.0f,    1.0f, RangeFamily::pick } },
            // string-interaction.md 7: the palm is right-hand contact; the
            // fretting finger is squeak's; the pole aperture is circuit's.
            { ParamIDs::palmMuteSpread,        { 20.0f,  60.0f,  5.0f,  120.0f, 35.0f,   1.0f, RangeFamily::pick } },
            { ParamIDs::adjacentMuteAmount,    { 0.0f,   1.0f,   0.0f,  1.0f,   0.6f,    1.0f, RangeFamily::squeak } },
            { ParamIDs::pickupApertureScale,   { 0.5f,   2.0f,   0.1f,  5.0f,   1.0f,    1.0f, RangeFamily::circuit } },
            // fingerstyle-attack.md 6.
            { ParamIDs::fingerFleshReleaseMs,  { 0.04f,  0.20f,  0.01f, 1.0f,   0.0723f, 1.0f, RangeFamily::pick } },
            { ParamIDs::fingerNailReleaseMs,   { 0.015f, 0.06f,  0.005f, 0.2f,  0.0227f, 1.0f, RangeFamily::pick } },
            { ParamIDs::thumbPositionOffset,   { -0.05f, 0.10f, -0.20f, 0.30f,  0.04f,   1.0f, RangeFamily::pick } },
            { ParamIDs::restStrokeDamping,     { 0.0f,   1.0f,   0.0f,  1.0f,   0.8f,    1.0f, RangeFamily::pick } },
            // ==== END REALISM-B ranges ====
        };

        return table[index];
    }

    constexpr int kNumEntries = 32 + 12;   // REALISM-B: +12
}

const PhysicalRange* RangeRegistry::find (const juce::String& parameterId)
{
    for (int i = 0; i < kNumEntries; ++i)
        if (parameterId == entryAt (i).id)
            return &entryAt (i).range;

    return nullptr;
}

juce::StringArray RangeRegistry::idsInFamily (RangeFamily family)
{
    juce::StringArray ids;

    for (int i = 0; i < kNumEntries; ++i)
        if (entryAt (i).range.family == family)
            ids.add (entryAt (i).id);

    return ids;
}

juce::StringArray RangeRegistry::allIds()
{
    juce::StringArray ids;

    for (int i = 0; i < kNumEntries; ++i)
        ids.add (entryAt (i).id);

    return ids;
}

//==============================================================================
namespace
{
    struct Declaration { juce::String id; float min, max; };

    /*  What each float parameter was actually declared with. Filled by
        `floatParam` while the layout is built, read by the test.

        A recorded fact checked at runtime rather than a jassert, because the
        test suite is a Release build and an assertion that compiles away is
        one that never runs. It is also why `floatParam` does not simply take
        its range from the registry: that would make the check compare the
        registry with itself, which is exactly the tautology this replaced. */
    juce::CriticalSection& declarationLock()
    {
        static juce::CriticalSection lock;
        return lock;
    }

    std::vector<Declaration>& declarations()
    {
        static std::vector<Declaration> list;
        return list;
    }
}

void RangeRegistry::noteDeclaration (const juce::String& parameterId, float min, float max)
{
    const juce::ScopedLock guard (declarationLock());

    for (auto& existing : declarations())
    {
        if (existing.id == parameterId)
        {
            existing.min = min;
            existing.max = max;
            return;
        }
    }

    declarations().push_back ({ parameterId, min, max });
}

juce::StringArray RangeRegistry::findDeclarationMismatches()
{
    const juce::ScopedLock guard (declarationLock());

    juce::StringArray mismatches;

    for (int i = 0; i < kNumEntries; ++i)
    {
        const auto& entry = entryAt (i);

        for (const auto& declared : declarations())
        {
            if (declared.id != entry.id)
                continue;

            if (! juce::approximatelyEqual (declared.min, entry.range.stockMin)
                  || ! juce::approximatelyEqual (declared.max, entry.range.stockMax))
                mismatches.add (juce::String (entry.id)
                                  + " declared " + juce::String (declared.min) + ".."
                                  + juce::String (declared.max) + " but stock is "
                                  + juce::String (entry.range.stockMin) + ".."
                                  + juce::String (entry.range.stockMax));
        }
    }

    return mismatches;
}

//==============================================================================
bool RangeState::isFamilyAdvanced (RangeFamily family) const noexcept
{
    if (family == RangeFamily::none)
        return false;

    return families[(size_t) family];
}

void RangeState::setFamilyAdvanced (RangeFamily family, bool advanced)
{
    if (family == RangeFamily::none)
        return;

    families[(size_t) family] = advanced;

    /*  Unlocking a family makes any per-control unlock inside it redundant,
        and advanced-ranges.md 4 says a redundant entry is dropped rather than
        kept - otherwise locking the family later would silently leave some of
        its controls unlocked. */
    if (advanced)
        for (const auto& id : RangeRegistry::idsInFamily (family))
            perControlUnlocks.removeString (id);
}

bool RangeState::isUnlockedIndividually (const juce::String& parameterId) const
{
    return perControlUnlocks.contains (parameterId);
}

void RangeState::setUnlockedIndividually (const juce::String& parameterId, bool unlocked)
{
    const auto* range = RangeRegistry::find (parameterId);

    if (range == nullptr)
        return;

    if (! unlocked)
    {
        perControlUnlocks.removeString (parameterId);
        return;
    }

    // Redundant inside an already-advanced family.
    if (isFamilyAdvanced (range->family))
        return;

    if (! perControlUnlocks.contains (parameterId))
        perControlUnlocks.add (parameterId);
}

bool RangeState::isParameterAdvanced (const juce::String& parameterId) const
{
    const auto* range = RangeRegistry::find (parameterId);

    if (range == nullptr)
        return false;

    return isFamilyAdvanced (range->family) || isUnlockedIndividually (parameterId);
}

bool RangeState::isAnythingAdvanced() const
{
    for (bool advanced : families)
        if (advanced)
            return true;

    return ! perControlUnlocks.isEmpty();
}

void RangeState::reset()
{
    families.fill (false);
    perControlUnlocks.clear();
}

//==============================================================================
namespace
{
    std::atomic<juce::uint32> rangeGeneration { 0 };
}

juce::uint32 RangeState::getGeneration() noexcept
{
    return rangeGeneration.load();
}

int RangeState::applyTo (juce::AudioProcessorValueTreeState& state) const
{
    ++rangeGeneration;

    int clamped = 0;

    for (const auto& id : RangeRegistry::allIds())
    {
        const auto* physical = RangeRegistry::find (id);

        auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (id));

        if (physical == nullptr || parameter == nullptr)
            continue;

        const bool advanced = isParameterAdvanced (id);

        /*  Read the plain value, swap the range, write the plain value back
            (advanced-ranges.md 1.2, 1.3). Writing it back through the new
            normalisation is what keeps the sound unchanged across a widening
            and makes a narrowing clamp exactly once. */
        const float before = parameter->get();

        parameter->range = physical->makeRange (advanced);

        const float after = juce::jlimit (parameter->range.start,
                                          parameter->range.end,
                                          before);

        if (! juce::approximatelyEqual (before, after))
            ++clamped;

        parameter->setValueNotifyingHost (parameter->range.convertTo0to1 (after));
    }

    return clamped;
}

juce::StringArray RangeState::findValuesOutsideStock (const juce::AudioProcessorValueTreeState& state) const
{
    juce::StringArray outside;

    for (const auto& id : RangeRegistry::allIds())
    {
        const auto* physical = RangeRegistry::find (id);

        if (physical == nullptr)
            continue;

        if (auto* parameter = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (id)))
            if (physical->isOutsideStock (parameter->get()))
                outside.add (id);
    }

    return outside;
}

//==============================================================================
juce::var RangeState::toVar() const
{
    auto* root = new juce::DynamicObject();
    auto* familyObject = new juce::DynamicObject();

    for (int i = 0; i < (int) RangeFamily::numFamilies; ++i)
        familyObject->setProperty (getRangeFamilyName ((RangeFamily) i),
                                   families[(size_t) i] ? "advanced" : "stock");

    root->setProperty ("families", juce::var (familyObject));

    juce::Array<juce::var> unlocks;

    for (const auto& id : perControlUnlocks)
        unlocks.add (id);

    root->setProperty ("per_control_unlocks", unlocks);

    return juce::var (root);
}

void RangeState::fromVar (const juce::var& rangesBlock, const juce::var& storedValues)
{
    reset();

    if (auto* root = rangesBlock.getDynamicObject())
    {
        if (auto* familyObject = root->getProperty ("families").getDynamicObject())
        {
            for (int i = 0; i < (int) RangeFamily::numFamilies; ++i)
            {
                const auto key = juce::String (getRangeFamilyName ((RangeFamily) i));

                // An absent key reads as stock (advanced-ranges.md 4).
                if (familyObject->hasProperty (key))
                    families[(size_t) i] =
                        familyObject->getProperty (key).toString() == "advanced";
            }
        }

        if (const auto* unlocks = root->getProperty ("per_control_unlocks").getArray())
            for (const auto& entry : *unlocks)
                if (RangeRegistry::find (entry.toString()) != nullptr)
                    perControlUnlocks.addIfNotAlreadyThere (entry.toString());

        return;
    }

    /*  No block, or a malformed one: derive per family (advanced-ranges.md
        4.1). A family is advanced if and only if one of its values is actually
        outside stock, so the preset keeps its sound and the padlock tells the
        truth.

        `storedValues` here holds **plain** values, read back from the
        parameters after they have been set - not the normalised numbers the
        preset file stores. A normalised value carries no information about
        which range it was written against: it is always in 0-1 and therefore
        always inside whatever range is live. `deriveFromCurrentValues` below
        is the caller that gets this right.
    */
    auto* values = storedValues.getDynamicObject();

    if (values == nullptr)
        return;

    for (const auto& id : RangeRegistry::allIds())
    {
        if (! values->hasProperty (id))
            continue;

        const auto* physical = RangeRegistry::find (id);

        if (physical == nullptr)
            continue;

        if (physical->isOutsideStock ((float) (double) values->getProperty (id)))
            families[(size_t) physical->family] = true;
    }
}

void RangeState::deriveFromCurrentValues (const juce::AudioProcessorValueTreeState& state)
{
    reset();

    for (const auto& id : findValuesOutsideStock (state))
        if (const auto* physical = RangeRegistry::find (id))
            families[(size_t) physical->family] = true;
}

} // namespace luthier
