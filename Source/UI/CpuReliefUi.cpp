#include "CpuReliefUi.h"
#include "UiPreferences.h"
#include "../PluginProcessor.h"

namespace luthier::CpuReliefUi
{

namespace
{
    constexpr const char* kPrefKey = "diagnostics.cpuDropStrings";
}

bool isStringDropAllowed()
{
    return UiPreferences::get().getBool (kPrefKey, true);
}

void setStringDropAllowed (LuthierAudioProcessor& processor, bool allowed)
{
    UiPreferences::get().setBool (kPrefKey, allowed);
    processor.getEngine().getCpuRelief().setStringDropAllowed (allowed);
}

void applySavedChoice (LuthierAudioProcessor& processor)
{
    processor.getEngine().getCpuRelief().setStringDropAllowed (isStringDropAllowed());
}

BannerAction bannerAction (bool due, bool showing) noexcept
{
    if (due && ! showing)
        return BannerAction::post;

    if (! due && showing)
        return BannerAction::withdraw;

    return BannerAction::none;
}

juce::String bannerMessage()
{
    return "CPU limit: the computer cannot keep up, so the least active strings are being "
           "dropped. Raise the buffer size or lower oversampling. "
           "Options -> Diagnostics can turn this off.";
}

} // namespace luthier::CpuReliefUi
