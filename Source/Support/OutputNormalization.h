#pragma once

/*  Output normalization, the processor's side (output-normalization.md).

    One object per LuthierAudioProcessor. It owns the settings (on/off and the
    target: session state, never a parameter, section 7), the change tracker,
    the calibrator worker (created on first enable), the status the UI reads,
    and the glue to MasterBus::LoudnessNormalizer on the audio thread.

    Threads:
      message  setEnabled / setTargetLufs / state / structural snapshot /
               status timer (10 Hz) / preset-load prefetch / morph gain
      audio    processBlockStart: the tracker, the offline wait (4.6), the
               master bus's timeline
      worker   NormalizationCalibrator, through captureSoundState and
               handleMeasurement

    Off (the default), the audio thread does one atomic load for the tracker
    and one block-level branch in MasterBus: the audio is bit-identical to the
    build before this feature (ground rule 0.1, ON-02).
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include "ConfigChangeTracker.h"
#include "NormalizationCalibrator.h"
#include "../DSP/Master/LoudnessNormalizer.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

namespace luthier
{

class LuthierAudioProcessor;

class OutputNormalization : private juce::Timer
{
public:
    /** 2.1's target choices, and the default. */
    static constexpr double kTargets[] = { -14.0, -16.0, -18.0, -20.0, -23.0 };
    static constexpr int kNumTargets = 5;
    static constexpr double kDefaultTarget = -18.0;

    /** 5.3 and 6: the UiPreferences keys. */
    static constexpr const char* kPrefDefaultEnabled = "normalization.defaultEnabled";
    static constexpr const char* kPrefDefaultTarget = "normalization.defaultTargetLufs";
    static constexpr const char* kPrefBannerSuppressed = "normalization.bannerSuppressed";

    /** 6: "a new instance with no restored state starts from them". The
        processor cannot read UiPreferences (it is UI code, and the renderer
        has none), so the UI installs this at static-initialisation time. */
    struct Defaults { bool enabled = false; double targetLufs = kDefaultTarget; };
    static std::function<Defaults()>& defaultsProvider();

    explicit OutputNormalization (LuthierAudioProcessor& processor);
    ~OutputNormalization() override;

    //==========================================================================
    // Settings. Message thread.

    /** `fromUser` is true for the switch and the shortcut command: those post
        the 5.3 banner (the UI listens on onEnabledByUser). A restore does not. */
    void setEnabled (bool shouldBeEnabled, bool fromUser = false);
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_acquire); }

    void setTargetLufs (double lufs);
    double getTargetLufs() const noexcept { return targetLufs.load (std::memory_order_acquire); }

    /** Snaps a requested target onto 2.1's list. */
    static double snapTarget (double lufs) noexcept;

    /** Called on the message thread after the switch is turned on from the UI. */
    std::function<void()> onEnabledByUser;

    /** Called on the message thread after any settings change (for the UI). */
    std::function<void()> onSettingsChanged;

    //==========================================================================
    // Session state (section 6). Message thread.

    /** The `normalization` root key. */
    juce::var toVar() const;

    /** A restored session. `hasKey` false (a project saved before this
        feature) loads off, whatever the preference. */
    void restoreFromSession (const juce::var& state, bool hasKey);

    //==========================================================================
    // 4.3: the offline calibration instance's own normalization is forced off,
    // so there is never a nested calibrator.
    void setCalibrationRenderMode (bool shouldBeInCalibrationMode);
    bool isCalibrationRenderMode() const noexcept { return calibrationMode.load(); }

    /** The render loop feeds the phrase here, per block (4.3 direct MIDI). */
    void setCalibrationDirectMidi (const juce::MidiBuffer* block) noexcept { calibrationDirect = block; }

    /** processSlice merges the calibration phrase into the engine's direct MIDI. */
    void mergeCalibrationDirect (juce::MidiBuffer& direct) const;

    //==========================================================================
    // Audio thread.

    /** Once per processSlice, before the engine runs. */
    void processBlockStart (std::int64_t timelineStart, int numSamples, double sampleRate, bool nonRealtime) noexcept;

    //==========================================================================
    // 2.3.1 discrete events. Message thread: re-captures the structural
    // snapshot and fires a request at the next block. `prefetch` also looks
    // the new state up in the caches and, on a hit, snaps the gain with the
    // load (2.2, 4.4 prefetch).
    void notifyConfigurationChanged (bool prefetch = false);

    /** 4.2 step 1: parameter atomics plus the published structural snapshot.
        Worker-safe: never touches the message thread. */
    NormalizationSoundState captureSoundState() const;

    /** A preset var (PresetManager::toVar shape) as a sound state (3.2 morph
        endpoints, 4.4 tune prefetch). */
    NormalizationSoundState soundStateFromPreset (const juce::var& presetVar) const;

    /** The calibrator hands a result back here (worker thread). */
    void handleMeasurement (std::uint32_t requestSerial, const NormalizationCalibrator::Measurement& m);

    /** 4.4: a tune or setlist loaded; calibrate its presets in the background. */
    void prefetchPresets (const juce::Array<juce::var>& presetVars);

    //==========================================================================
    // Status (5.2), for the readout, the badge, Diagnostics and tests.

    enum class State { off, measuring, applied, clamped, estimate, unmeasurable, morphing };

    struct Status
    {
        bool enabled = false;
        double targetLufs = kDefaultTarget;
        State state = State::off;
        double gainDb = 0.0;            ///< the gain the calibration asks for
        double appliedGainDb = 0.0;     ///< where the audio-thread gain is right now
        double measuredLufs = -120.0;
        std::uint8_t flags = 0;
        NormalizationCalibrator::Source source = NormalizationCalibrator::Source::none;
        juce::String hash;
        double truePeakReductionDb = 0.0;
        juce::uint32 updatedMs = 0;     ///< Time::getMillisecondCounter of the last result
        int requests = 0;
    };

    Status getStatus() const;

    /** 5.2's readout and badge text for a status. */
    static juce::String readoutText (const Status& s);
    static juce::String badgeText (const Status& s);
    static juce::String badgeTooltip (const Status& s);
    static juce::String accessibleBadgeName (const Status& s);

    /** 10 (preset previews): the dB a preview clip rendered at -18 LUFS gets
        so it sounds as loud as the loaded preset will; limited so the clip's
        true peak stays at or below -1 dBTP. 0 while normalization is off. */
    double getPreviewGainOffsetDb (double clipTruePeakDbtp) const noexcept;

    //==========================================================================
    ConfigChangeTracker& getTracker() noexcept { return *tracker; }
    NormalizationCalibrator* getCalibrator() noexcept { return calibrator.get(); }
    LoudnessNormalizer& getNormalizer() noexcept;

    /** The live processor, whose parameter list gives every state its shape. */
    juce::AudioProcessor& getShapeProcessor() noexcept;

    /** ON-25: live OutputNormalization objects (one per processor). */
    static int getLiveInstanceCount() noexcept;

    /** ON-06/ON-13: how many calibration requests the tracker has fired. */
    int getNumRequests() const noexcept { return tracker->getNumRequests(); }

    /** ON-17: how many times the audio thread has waited for a result. Only
        ever non-zero in non-realtime renders. */
    static std::atomic<int>& offlineWaitCount() noexcept;

    /** Re-captures the structural snapshot the worker reads, on or off. */
    void refreshStructuralSnapshot() { publishStructural (captureStructural()); }

    /** Tests: run the message-thread poll now. */
    void pollNow() { timerCallback(); }

private:
    void timerCallback() override;
    void ensureCalibrator();
    juce::var captureStructural();
    void publishStructural (const juce::var& structural);
    void publishGainFor (double measuredLufs, std::uint8_t flags);
    void updateMorph();

    LuthierAudioProcessor& processor;

    std::atomic<bool> enabled { false };
    std::atomic<double> targetLufs { kDefaultTarget };
    std::atomic<bool> calibrationMode { false };

    std::unique_ptr<ConfigChangeTracker> tracker;
    std::unique_ptr<NormalizationCalibrator> calibrator;

    const juce::MidiBuffer* calibrationDirect = nullptr;

    // The structural snapshot the worker reads (4.2 step 1).
    mutable std::mutex structuralLock;
    juce::var structuralSnapshot;
    juce::int64 structuralFingerprint = 0;

    // The last measurement, so a target change needs no new render.
    mutable std::mutex statusLock;
    Status status;
    bool haveMeasurement = false;
    double lastMeasuredLufs = -120.0;
    std::uint8_t lastFlags = 0;

    std::atomic<std::uint32_t> publishSerial { 0 };
    std::atomic<std::uint32_t> lastFiredSerial { 0 };
    std::atomic<bool> loadRequestPending { false };

    // 3.2: preset-morph endpoints.
    juce::String morphHash[2];
    double morphLufs[2] { 0.0, 0.0 };
    bool morphKnown[2] { false, false };
    bool morphPending[2] { false, false };
    mutable std::mutex morphLock;
    int pollTick = 0;
    double lastMorphGainDb = 1.0e9;
    bool morphing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputNormalization)
};

} // namespace luthier
