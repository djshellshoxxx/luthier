#include "Validator.h"
#include "Model/Guitar/StringMaterials.h"

namespace luthier
{

const char* getValidationCheckName (ValidationCheck c) noexcept
{
    switch (c)
    {
        case ValidationCheck::StringTension:     return "String tension";
        case ValidationCheck::FretRange:         return "Fret range";
        case ValidationCheck::Damping:           return "Damping";
        case ValidationCheck::BodyCoupling:      return "Body coupling";
        case ValidationCheck::PickupOutput:      return "Pickup output";
        case ValidationCheck::SignalFinite:      return "Non-finite sample";
        case ValidationCheck::CouplingStability: return "Coupling limiting";
        case ValidationCheck::NumChecks:
        default:                                 return "Unknown";
    }
}

//==============================================================================
void Validator::reset() noexcept
{
    for (auto& c : failureCounts)
        c.store (0);

    writeIndex.store (0);
    harmonicFallbacks.store (0);

    for (auto& r : history)
        r = ValidationRecord {};
}

void Validator::record (ValidationCheck c, int stringIndex, double offending,
                        double corrected, int64_t pos, bool rejected) noexcept
{
    failureCounts[(size_t) c].fetch_add (1, std::memory_order_relaxed);

    const int index = writeIndex.fetch_add (1, std::memory_order_relaxed) % kHistorySize;

    auto& r = history[index];
    r.check = c;
    r.stringIndex = stringIndex;
    r.offendingValue = offending;
    r.correctedValue = corrected;
    r.samplePosition = pos;
    r.rejected = rejected;
}

//==============================================================================
double Validator::checkTension (int stringIndex, double tensionNewtons, double scaleLengthMm,
                                int64_t pos, bool& outAccepted) noexcept
{
    outAccepted = true;

    if (! enabled.load())
        return tensionNewtons;

    // The comfortable playing range, which depends on the instrument: a bass neck
    // is built for two to three times the tension a guitar neck is, so judging a
    // bass by the guitar figure would flag every one of them.
    const auto range = StringMaterials::getTensionRange (scaleLengthMm);

    if (tensionNewtons >= range.comfortableMin && tensionNewtons <= range.comfortableMax)
        return tensionNewtons;

    const double clamped = juce::jlimit (range.absoluteMin, range.absoluteMax, tensionNewtons);

    // Outside even the absolute limits, strict mode refuses the note: this is a
    // tuning the instrument physically cannot be strung for.
    const bool beyondAbsolute = (tensionNewtons < range.absoluteMin
                                 || tensionNewtons > range.absoluteMax);

    if (beyondAbsolute && strict.load())
        outAccepted = false;

    record (ValidationCheck::StringTension, stringIndex, tensionNewtons, clamped, pos, ! outAccepted);

    return clamped;
}

//==============================================================================
double Validator::checkFretRange (int stringIndex, double fretPosition, int maxFrets,
                                  int64_t pos, bool& outAccepted) noexcept
{
    outAccepted = true;

    if (! enabled.load())
        return fretPosition;

    const double upper = (double) juce::jmax (1, maxFrets);

    // A small negative value is a bend below the open string, which is real on a
    // trem-equipped guitar, so it is tolerated rather than corrected.
    if (fretPosition >= -0.5 && fretPosition <= upper + 0.01)
        return fretPosition;

    const double clamped = juce::jlimit (0.0, upper, fretPosition);

    if (strict.load())
        outAccepted = false;

    record (ValidationCheck::FretRange, stringIndex, fretPosition, clamped, pos, ! outAccepted);

    return clamped;
}

//==============================================================================
double Validator::checkDamping (int stringIndex, double t60Seconds, double frequencyHz,
                                int64_t pos) noexcept
{
    if (! enabled.load())
        return t60Seconds;

    // Identity rule 6: higher notes decay faster. A plausible envelope for a
    // guitar runs from about 8 s on an open low E down to well under a second at
    // the top of the neck. Anything far outside that is a model error.
    const double maxPlausible = juce::jlimit (0.4, 25.0, 1400.0 / juce::jmax (20.0, frequencyHz));
    const double minPlausible = 0.02;

    if (t60Seconds >= minPlausible && t60Seconds <= maxPlausible)
        return t60Seconds;

    const double clamped = juce::jlimit (minPlausible, maxPlausible, t60Seconds);

    record (ValidationCheck::Damping, stringIndex, t60Seconds, clamped, pos, false);

    return clamped;
}

//==============================================================================
void Validator::checkBodyCoupling (bool bodyIsActive, int64_t pos) noexcept
{
    if (! enabled.load() || bodyIsActive)
        return;

    // Only reported once in a while: the user may have deliberately engaged the
    // experimental "no body" mode, and a diagnostic per block would be noise.
    static thread_local int64_t lastReport = -1000000;

    if (pos - lastReport < 48000)
        return;

    lastReport = pos;
    record (ValidationCheck::BodyCoupling, -1, 0.0, 0.0, pos, false);
}

//==============================================================================
void Validator::checkPickupOutput (bool anyPickupActive, double signalLevel, int64_t pos) noexcept
{
    if (! enabled.load() || anyPickupActive)
        return;

    static thread_local int64_t lastReport = -1000000;

    if (pos - lastReport < 48000)
        return;

    lastReport = pos;
    record (ValidationCheck::PickupOutput, -1, signalLevel, 0.0, pos, false);
}

//==============================================================================
bool Validator::checkFinite (double value, int64_t pos) noexcept
{
    if (std::isfinite (value))
        return true;

    if (enabled.load())
        record (ValidationCheck::SignalFinite, -1, 0.0, 0.0, pos, false);

    return false;
}

void Validator::reportCouplingLimiting (double amount, int64_t pos) noexcept
{
    if (! enabled.load() || amount <= 0.0)
        return;

    static thread_local int64_t lastReport = -1000000;

    if (pos - lastReport < 24000)
        return;

    lastReport = pos;
    record (ValidationCheck::CouplingStability, -1, amount, 0.0, pos, false);
}

//==============================================================================
int Validator::getFailureCount (ValidationCheck c) const noexcept
{
    return failureCounts[(size_t) juce::jlimit (0, (int) ValidationCheck::NumChecks - 1, (int) c)]
             .load (std::memory_order_relaxed);
}

int Validator::getTotalFailures() const noexcept
{
    int total = 0;

    for (const auto& c : failureCounts)
        total += c.load (std::memory_order_relaxed);

    return total;
}

int Validator::getRecentRecords (ValidationRecord* dest, int maxRecords) const noexcept
{
    if (dest == nullptr || maxRecords <= 0)
        return 0;

    const int written = writeIndex.load (std::memory_order_relaxed);
    const int available = juce::jmin (written, kHistorySize);
    const int count = juce::jmin (available, maxRecords);

    for (int i = 0; i < count; ++i)
    {
        const int index = (written - count + i) % kHistorySize;
        dest[i] = history[(index + kHistorySize) % kHistorySize];
    }

    return count;
}

juce::String Validator::getSummary() const
{
    const int total = getTotalFailures();

    if (total == 0)
        return "All checks passing.";

    juce::StringArray parts;

    for (int i = 0; i < (int) ValidationCheck::NumChecks; ++i)
    {
        const int count = getFailureCount ((ValidationCheck) i);

        if (count > 0)
            parts.add (juce::String (getValidationCheckName ((ValidationCheck) i))
                       + ": " + juce::String (count));
    }

    return juce::String (total) + " correction" + (total == 1 ? "" : "s")
           + " (" + parts.joinIntoString (", ") + ")";
}

} // namespace luthier
