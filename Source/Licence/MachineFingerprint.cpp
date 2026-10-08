#include "MachineFingerprint.h"

#include <juce_cryptography/juce_cryptography.h>

namespace luthier
{

juce::String MachineFingerprint::hashComponent (int index, const juce::String& raw)
{
    const auto material = juce::String (kSalt) + "|" + juce::String (index) + "|" + raw;
    return juce::SHA256 (material.toUTF8()).toHexString().substring (0, 32);
}

juce::StringArray MachineFingerprint::collect()
{
    juce::StringArray raw;
    raw.add (juce::SystemStats::getUniqueDeviceID());

    const auto systemVolume = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    raw.add (juce::String::toHexString (systemVolume.getVolumeSerialNumber()));

    raw.add (juce::SystemStats::getCpuModel() + "|"
             + juce::String (juce::SystemStats::getNumCpus()));

    auto macs = juce::MACAddress::getAllAddresses();
    juce::StringArray macStrings;

    for (const auto& mac : macs)
        if (mac.toString().isNotEmpty() && mac.toString() != "00-00-00-00-00-00")
            macStrings.add (mac.toString().toLowerCase());

    macStrings.sort (true);
    raw.add (macStrings.isEmpty() ? "no-physical-mac" : macStrings[0]);
    raw.add (juce::SystemStats::getComputerName());

    juce::StringArray result;

    for (int i = 0; i < 5; ++i)
        result.add (hashComponent (i, raw[i].isEmpty() ? "unavailable" : raw[i]));

    return result;
}

} // namespace luthier
