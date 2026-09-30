#pragma once

/*  cpu-quality-modes.md 3: the machine's performance preferences.

    `Documents/Luthier/config/performance.json`, beside ui.json:

        { "schema": 1, "magic": "luthier.performance",
          "quality": "high", "offline_at_high": true, "auto_notify": true,
          "auto_last_level": "high", "emergency_string_drop": true }

    Not UiPreferences: that store is about the window; this is about the
    machine. A missing or corrupt file gives the defaults (High) and is
    rewritten on the next change, never an error. Writes are temp-and-rename
    (file-formats 13), so a crash mid-write leaves the old file.

    One process-wide instance. A change is broadcast (message thread) to every
    plugin instance in the process; other processes pick changes up through
    reloadIfChanged(), which prepareToPlay and the editor call.
*/

#include "QualityProfile.h"
#include <juce_events/juce_events.h>
#include <atomic>

namespace luthier
{

class PerformanceSettings : public juce::ChangeBroadcaster
{
public:
    static PerformanceSettings& get();

    struct Values
    {
        QualityChoice quality = QualityChoice::High;
        bool offlineAtHigh = true;
        bool autoNotify = true;
        QualityLevel autoLastLevel = QualityLevel::High;
        bool emergencyStringDrop = true;

        bool operator== (const Values& o) const noexcept
        {
            return quality == o.quality && offlineAtHigh == o.offlineAtHigh
                   && autoNotify == o.autoNotify && autoLastLevel == o.autoLastLevel
                   && emergencyStringDrop == o.emergencyStringDrop;
        }
        bool operator!= (const Values& o) const noexcept { return ! (*this == o); }
    };

    //==========================================================================
    // Lock-free getters: any thread.
    QualityChoice getQuality() const noexcept         { return (QualityChoice) quality.load (std::memory_order_relaxed); }
    bool isOfflineAtHigh() const noexcept             { return offlineAtHigh.load (std::memory_order_relaxed); }
    bool isAutoNotify() const noexcept                { return autoNotify.load (std::memory_order_relaxed); }
    QualityLevel getAutoLastLevel() const noexcept    { return (QualityLevel) autoLastLevel.load (std::memory_order_relaxed); }
    bool isEmergencyStringDrop() const noexcept       { return emergencyStringDrop.load (std::memory_order_relaxed); }

    Values getValues() const noexcept;

    //==========================================================================
    // Setters: message thread. Each writes the file and broadcasts if it changed.
    void setQuality (QualityChoice q);
    void setOfflineAtHigh (bool b);
    void setAutoNotify (bool b);
    void setAutoLastLevel (QualityLevel l);
    void setEmergencyStringDrop (bool b);
    void setValues (const Values& v);

    /** DIAGNOSTICS "Reset all settings": the defaults, written. */
    void resetToDefaults();

    //==========================================================================
    static juce::File getDefaultFile();
    juce::File getFile() const;

    /** Tests point the store somewhere harmless (an empty file = the default). */
    void setFileForTesting (const juce::File& f);

    /** Re-reads the file. Missing or corrupt gives the defaults. Returns true
        if the file parsed. Broadcasts when the values changed. */
    bool load();

    /** Re-reads only if the file's modification time moved (another process). */
    void reloadIfChanged();

    bool save() const;

    /** Pure parsing and writing, for the tests (CQ-02). */
    static bool parse (const juce::String& json, Values& out);
    static juce::String toJson (const Values& v);

    /** The number of saves since start, for the tests. */
    int getSaveCount() const noexcept { return saveCount; }

private:
    PerformanceSettings();
    void store (const Values& v) noexcept;
    void changed();

    std::atomic<int> quality { (int) QualityChoice::High };
    std::atomic<bool> offlineAtHigh { true }, autoNotify { true }, emergencyStringDrop { true };
    std::atomic<int> autoLastLevel { (int) QualityLevel::High };

    juce::File fileOverride;
    juce::Time lastModification;
    mutable int saveCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformanceSettings)
};

} // namespace luthier
