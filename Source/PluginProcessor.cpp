#include "PluginProcessor.h"

LuthierAudioProcessor::LuthierAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}
LuthierAudioProcessor::~LuthierAudioProcessor() = default;

void LuthierAudioProcessor::prepareToPlay (double, int) {}
void LuthierAudioProcessor::releaseResources() {}
bool LuthierAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
void LuthierAudioProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    b.clear();
}
juce::AudioProcessorEditor* LuthierAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
void LuthierAudioProcessor::getStateInformation (juce::MemoryBlock&) {}
void LuthierAudioProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LuthierAudioProcessor(); }
