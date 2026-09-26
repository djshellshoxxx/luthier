#pragma once

/*  mic-placement.md 4: the discrete mic Position / Distance choices and the
    continuous placement, kept in step.

      - Migration: data that has `mic_position` but no `mic_x` (an old preset,
        host chunk, snapshot or morph endpoint) gains the mapped continuous
        values. Presence of the key is the test; there is no schema bump.
      - The mirror: the nearest discrete choice is written into serialised
        output only (preset JSON, host state), so an older Luthier that loads a
        new preset gets the closest thing it can play. APVTS is never touched.
      - Legacy host writes: a session still automating `mic_position` moves the
        continuous parameters with it, unless one of that mic's continuous
        parameters was written within 250 ms (a restore writes both at once).

    All of it works on the normalised "parameters" object a preset, a snapshot
    and a morph slot share, converting through the live parameter ranges.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace luthier
{

namespace MicPlacementMigration
{
    /** A legacy (position, distance) pair as continuous values. */
    struct Mapped
    {
        double x = 0.35, y = 0.0, angleDeg = 0.0, distCm = 2.5;
        bool rear = false;
    };

    Mapped mapLegacy (int position, int distance) noexcept;

    /** The mirror: the nearest discrete choice for a continuous placement. */
    int mirrorPosition (double x, double y, double angleDeg, bool rear) noexcept;
    int mirrorDistance (double distCm) noexcept;

    /** The ids of one mic's legacy and continuous parameters. */
    struct MicIds
    {
        const char* position; const char* distance;
        const char* x; const char* y; const char* dist; const char* angle;
        const char* speaker; const char* rear;
    };

    const MicIds& idsFor (int mic) noexcept;

    /** Migrates a normalised parameters object in place. Returns which mics
        were migrated (bit 0 mic 1, bit 1 mic 2). */
    int apply (juce::var& parameters, const juce::AudioProcessorValueTreeState& state);
    int apply (juce::var& parameters, const juce::AudioProcessor& processor);

    /** Migrates a ValueTree of PARAM children (id, value in plain units), as
        an APVTS state holds them. Same rules. */
    int apply (juce::ValueTree& paramsTree, const juce::AudioProcessorValueTreeState& state);

    /** Writes the mirror of the current continuous values into a normalised
        parameters object. The Acoustic DI keeps its own legacy choices: they
        pick its voicing, and it has no placement to mirror. */
    void mirror (juce::var& parameters, const juce::AudioProcessorValueTreeState& state);

    /** Writes the mapped continuous values of a legacy choice into the
        parameters, as one would from a Quick combo. Message thread. */
    void writeMapped (juce::AudioProcessorValueTreeState& state, int mic, int position, int distance);

    /** The live legacy values, stored beside the mirror (as the preset's
        `micLegacy` block) so this build restores exactly what it saved while
        an older one reads the mirror. Normalised, keyed by id. */
    juce::var captureLiveLegacy (const juce::AudioProcessorValueTreeState& state);
    void restoreLiveLegacy (const juce::var& block, juce::AudioProcessorValueTreeState& state);

    inline constexpr const char* kLegacyBlockKey = "micLegacy";

    /** The randomiser's stock-range rule (mic-placement.md 9): u <= 1.0 and
        distance <= 30 cm, for every mic whose parameters are not locked. */
    void keepPlacementPlausible (juce::AudioProcessorValueTreeState& state, const juce::StringArray& locked);
}

//==============================================================================
/** Follows legacy host writes (mic-placement.md 4, "Legacy host writes"). */
class MicLegacyAutomation : private juce::AudioProcessorParameter::Listener,
                            private juce::Timer
{
public:
    static constexpr double kWindowMs = 250.0;

    explicit MicLegacyAutomation (juce::AudioProcessorValueTreeState& state);
    ~MicLegacyAutomation() override;

    /** Decides any legacy write older than the window. The timer calls this
        with the real clock; tests call it with their own. Message thread. */
    void processPending (double nowMs);

    int getMappingCount() const noexcept { return mappings; }

    static double now() noexcept { return juce::Time::getMillisecondCounterHiRes(); }

private:
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}
    void timerCallback() override { processPending (now()); }

    juce::AudioProcessorValueTreeState& apvts;

    struct Watched { juce::AudioProcessorParameter* param; int mic; bool legacy; };
    std::vector<Watched> watched;

    std::array<std::atomic<double>, 2> lastLegacyWrite {}, lastContinuousWrite {};
    std::array<std::atomic<bool>, 2> pending {};
    std::atomic<bool> writingMapped { false };
    int mappings = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicLegacyAutomation)
};

} // namespace luthier
