#pragma once

/*  Validator pipeline (build spec, "Identity constraints" and "Validator pipeline").

    Every note that reaches the engine is checked against the things that make a
    guitar a guitar. A failure does not throw or crash: it nudges the offending
    parameter back into range and records a diagnostic, so the plugin keeps
    playing and the user can see what was corrected in the debug panel.

    The checks, in order:
      1. String physics  - tension consistent with pitch, mass and length
      2. Fret validity   - the note exists on that string
      3. Damping         - the decay envelope is realistic for the pitch
      4. Body coupling   - the string signal is passing through a body model
      5. Pickup output   - the signal has the expected spectral shape

    All of it is real-time safe: fixed-size counters and a lock-free ring of the
    most recent diagnostics.
*/

#include "DSP/Common/DspCommon.h"
#include <array>
#include <atomic>

namespace luthier
{

//==============================================================================
enum class ValidationCheck
{
    StringTension,
    FretRange,
    Damping,
    BodyCoupling,
    PickupOutput,
    SignalFinite,
    CouplingStability,
    NumChecks
};

const char* getValidationCheckName (ValidationCheck c) noexcept;

//==============================================================================
struct ValidationRecord
{
    ValidationCheck check = ValidationCheck::StringTension;
    int    stringIndex = -1;
    double offendingValue = 0.0;
    double correctedValue = 0.0;
    int64_t samplePosition = 0;
    bool   rejected = false;     ///< True if the note was refused outright.
};

//==============================================================================
class Validator
{
public:
    static constexpr int kHistorySize = 64;

    void reset() noexcept;

    void setEnabled (bool e) noexcept { enabled.store (e); }
    bool isEnabled() const noexcept { return enabled.load(); }

    /** When strict, an out-of-range note is rejected instead of corrected. Off by
        default: a musician would rather hear a nudged note than silence. */
    void setStrict (bool s) noexcept { strict.store (s); }
    bool isStrict() const noexcept { return strict.load(); }

    //==========================================================================
    /** Check 1. Returns the tension to use, clamped into the playable range.
        `outAccepted` is false if strict mode would reject the note. */
    double checkTension (int stringIndex, double tensionNewtons, double scaleLengthMm,
                         int64_t pos, bool& outAccepted) noexcept;

    /** Check 2. Returns the fret position to use, clamped to the string's range. */
    double checkFretRange (int stringIndex, double fretPosition, int maxFrets,
                           int64_t pos, bool& outAccepted) noexcept;

    /** Check 3. Confirms the decay time is plausible for the pitch. Returns the
        T60 to use. */
    double checkDamping (int stringIndex, double t60Seconds, double frequencyHz, int64_t pos) noexcept;

    /** Check 4. Identity rule 3: the string signal must pass through a body model. */
    void checkBodyCoupling (bool bodyIsActive, int64_t pos) noexcept;

    /** Check 5. Identity rule 4: with every pickup off the instrument is silent,
        and that has to be visible rather than mysterious. */
    void checkPickupOutput (bool anyPickupActive, double signalLevel, int64_t pos) noexcept;

    /** Catches a non-finite sample escaping any stage. */
    bool checkFinite (double value, int64_t pos) noexcept;

    /** Records that the coupling matrix had to limit. */
    void reportCouplingLimiting (double amount, int64_t pos) noexcept;

    //==========================================================================
    int getFailureCount (ValidationCheck c) const noexcept;
    int getTotalFailures() const noexcept;

    /** Copies the most recent records out, newest last. Returns how many were
        written. Safe to call from the message thread. */
    int getRecentRecords (ValidationRecord* dest, int maxRecords) const noexcept;

    /** True if any check has failed since the last reset. */
    bool hasFailures() const noexcept { return getTotalFailures() > 0; }

    /** A one-line human summary for the diagnostics panel. */
    juce::String getSummary() const;

private:
    void record (ValidationCheck c, int stringIndex, double offending, double corrected,
                 int64_t pos, bool rejected) noexcept;

    std::atomic<bool> enabled { true };
    std::atomic<bool> strict { false };

    std::array<std::atomic<int>, (size_t) ValidationCheck::NumChecks> failureCounts {};

    ValidationRecord history[kHistorySize];
    std::atomic<int> writeIndex { 0 };
};

} // namespace luthier
