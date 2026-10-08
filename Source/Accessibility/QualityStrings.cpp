#include "QualityStrings.h"

namespace luthier::QualityStrings
{

std::map<juce::String, juce::String> mergeInto (std::map<juce::String, juce::String> catalog)
{
    auto u = [] (const char* utf8) { return juce::String::fromUTF8 (utf8); };

    const std::pair<const char*, juce::String> entries[] =
    {
        // ---- levels and the footer badge (5) --------------------------------------------
        { "quality.level.high",          "High" },
        { "quality.level.medium",        "Medium" },
        { "quality.level.low",           "Low" },
        { "quality.level.auto",          "Auto" },
        { "quality.badge.high",          "HIGH" },
        { "quality.badge.medium",        "MED" },
        { "quality.badge.low",           "LOW" },
        { "quality.badge.autoHigh",      u ("AUTO\xc2\xb7H") },
        { "quality.badge.autoMedium",    u ("AUTO\xc2\xb7M") },
        { "quality.badge.autoLow",       u ("AUTO\xc2\xb7L") },
        { "quality.badge.autoHeld",      "AUTO (held)" },
        { "quality.badge.name",          "CPU quality" },
        { "quality.badge.tooltip",       "CPU quality: {level}. Luthier is using {percent} % of the audio time. Click to change." },
        { "quality.badge.tooltipIdle",   "CPU quality: {level}. Not playing yet. Click to change." },
        { "quality.badge.latency",       "latency {n} smp" },
        { "quality.badge.stale",         "-" },

        // ---- Options -> AUDIO, QUALITY section (5) --------------------------------------
        { "quality.section",             "QUALITY" },
        { "quality.cpuQuality",          "CPU quality" },
        { "quality.thisInstance",        "This instance" },
        { "quality.override.global",     "Use global setting ({level})" },
        { "quality.override.high",       "High (this instance only)" },
        { "quality.override.medium",     "Medium (this instance only)" },
        { "quality.override.low",        "Low (this instance only)" },
        { "quality.override.auto",       "Auto (this instance only)" },
        { "quality.nowRunning",          "Now running: {level}" },
        { "quality.nowRunningAuto",      "Now running: {level} (Auto)" },
        { "quality.notPlaying",          "Not playing yet" },
        { "quality.share",               "{percent} %" },
        { "quality.shareNote",           "Luthier's own share of the audio time. The host's total load is not visible to a plugin." },
        { "quality.held",                "Auto is holding at {level}: load kept changing. Choose Auto again to resume." },
        { "quality.offlineAtHigh",       "Always render offline at High" },
        { "quality.autoNotify",          "Tell me when Auto changes quality" },
        { "quality.osCapped",            "Running at {factor}x while quality is {level}." },
        { "quality.whatChanges",         "What each level changes" },

        // One sentence per level: the radio items' descriptions (9) and the disclosure.
        { "quality.describe.high",       "Full detail: the oversampling you chose, full-length responses and every string and body detail." },
        { "quality.describe.medium",     "Amp and drive at 2x, shorter body and cabinet tails, 32 body modes, lighter dispersion on high notes, idle strings sleep; UI animation limited." },
        { "quality.describe.low",        "Amp at 2x and drive at 1x, short body and cabinet tails, 20 body modes, 8 room reflections, fewer noise voices, idle strings sleep; all animation off." },
        { "quality.describe.auto",       "Starts at High and steps down when Luthier's CPU load stays high, then back up when it settles." },
        { "quality.disclosure.never",    "No level changes pitch, timing, latency or what a preset contains. Renders are at High." },

        // ---- AppearancePage and DIAGNOSTICS -----------------------------------------------
        { "quality.lowAnimationsOff",    "Animations are off while CPU quality is Low." },
        { "quality.emergencyDrop",       "When Luthier runs out of CPU, drop the quietest string instead of glitching" },
        { "quality.diag.level",          "CPU quality: {level}" },
        { "quality.diag.oversampling",   "Oversampling: amp {amp}x, drive {drive}x (nominal {nominal}x)" },
        { "quality.diag.irs",            "Body response {body} s, cabinet {cabA} s / {cabB} s" },
        { "quality.diag.modes",          "Body modes: {n}" },
        { "quality.diag.sleeping",       "Sleeping strings: {n}" },

        // ---- notices (2.7, 7) ----------------------------------------------------------
        { "quality.notice.autoDown",     u ("Luthier's CPU load is high, so Auto lowered quality to {level}. Options \xe2\x86\x92 Audio.") },
        { "quality.notice.autoUp",       "Auto raised CPU quality to {level}." },
        { "quality.notice.stringDropped",      "CPU limit reached: a string was dropped." },
        { "quality.notice.stringDroppedFirst", "CPU limit reached: a string was dropped. Try CPU quality Auto or Low." },

        // ---- shortcuts (accessibility 2) ------------------------------------------------
        { "quality.shortcut.cycle",      "Cycle CPU quality" }
    };

    for (const auto& [key, text] : entries)
        catalog[key] = text;

    return catalog;
}

} // namespace luthier::QualityStrings
