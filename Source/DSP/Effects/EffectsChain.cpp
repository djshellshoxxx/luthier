#include "EffectsChain.h"
#include "PedalsDrive.h"

namespace luthier
{

EffectsChain::EffectsChain() = default;
EffectsChain::~EffectsChain() = default;

//==============================================================================
void EffectsChain::prepare (double sampleRate, int maxBlockSize)
{
    const juce::ScopedLock sl (swapLock);

    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    workL.assign ((size_t) maxBlock, 0.0);
    workR.assign ((size_t) maxBlock, 0.0);

    for (auto& slot : slots)
    {
        if (slot.pedal != nullptr)
        {
            slot.pedal->prepare (sr, maxBlock);
            slot.pedal->setBypassed (slot.bypassed);
            slot.pedal->setMix (slot.mix);
            slot.pedal->setTempoBpm (tempoBpm);
            slot.pedal->setExpression (expression);
        }
    }

    setOversamplingFactor (oversamplingFactor);
    retired.clear();
}

void EffectsChain::reset() noexcept
{
    const juce::ScopedLock sl (swapLock);

    for (auto& slot : slots)
    {
        if (slot.pedal != nullptr)
        {
            slot.pedal->reset();
            slot.pedal->resetBase();
        }
    }
}

//==============================================================================
void EffectsChain::setSlotType (int slot, PedalType type)
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return;

    auto replacement = Pedal::create (type);

    if (replacement != nullptr)
    {
        replacement->prepare (sr, maxBlock);
        replacement->resetParametersToDefault();
        replacement->setTempoBpm (tempoBpm);
        replacement->setExpression (expression);
        replacement->setBypassed (slots[(size_t) slot].bypassed);
        replacement->setMix (slots[(size_t) slot].mix);

        replacement->setOversamplingFactor (oversamplingFactor);   // SPEC-SWEEP JG-4: virtual, no cast
    }

    {
        const juce::ScopedLock sl (swapLock);

        // Park the outgoing pedal rather than destroying it while the audio thread
        // might still be between samples in it.
        if (slots[(size_t) slot].pedal != nullptr)
            retired.push_back (std::move (slots[(size_t) slot].pedal));

        slots[(size_t) slot].pedal = std::move (replacement);
        slots[(size_t) slot].type = type;
    }

    // Safe here: we are on the message thread and hold no audio-thread state.
    retired.clear();
}

PedalType EffectsChain::getSlotType (int slot) const noexcept
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return PedalType::None;

    return slots[(size_t) slot].type;
}

Pedal* EffectsChain::getPedal (int slot) noexcept
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return nullptr;

    return slots[(size_t) slot].pedal.get();
}

const Pedal* EffectsChain::getPedal (int slot) const noexcept
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return nullptr;

    return slots[(size_t) slot].pedal.get();
}

void EffectsChain::applySlotState (int slot, bool bypassed, double mix,
                                   const float* normalisedParams, int numParams) noexcept
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return;

    // The message thread swaps pedals under this lock and frees the old one
    // straight after; reaching into a slot without it used a freed pedal
    // (pluginval: "pure virtual method called" during automation).
    const juce::ScopedTryLock sl (swapLock);

    if (! sl.isLocked())
        return;

    auto& s = slots[(size_t) slot];
    s.bypassed = bypassed;
    s.mix = juce::jlimit (0.0, 1.0, mix);

    if (s.pedal == nullptr)
        return;

    s.pedal->setBypassed (s.bypassed);
    s.pedal->setMix (s.mix);

    const int n = juce::jmin (numParams, s.pedal->getNumParameters(), Pedal::kMaxParams);

    for (int p = 0; p < n; ++p)
        s.pedal->setParameterNormalised (p, normalisedParams[p]);
}

void EffectsChain::setSlotBypassed (int slot, bool bypassed) noexcept
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return;

    slots[(size_t) slot].bypassed = bypassed;

    if (slots[(size_t) slot].pedal != nullptr)
        slots[(size_t) slot].pedal->setBypassed (bypassed);
}

bool EffectsChain::isSlotBypassed (int slot) const noexcept
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return true;

    return slots[(size_t) slot].bypassed;
}

void EffectsChain::setSlotMix (int slot, double mix) noexcept
{
    if (! juce::isPositiveAndBelow (slot, kNumSlots))
        return;

    slots[(size_t) slot].mix = juce::jlimit (0.0, 1.0, mix);

    if (slots[(size_t) slot].pedal != nullptr)
        slots[(size_t) slot].pedal->setMix (slots[(size_t) slot].mix);
}

void EffectsChain::moveSlot (int fromSlot, int toSlot)
{
    if (! juce::isPositiveAndBelow (fromSlot, kNumSlots)
        || ! juce::isPositiveAndBelow (toSlot, kNumSlots)
        || fromSlot == toSlot)
        return;

    const juce::ScopedLock sl (swapLock);

    auto moving = std::move (slots[(size_t) fromSlot]);

    if (fromSlot < toSlot)
    {
        for (int i = fromSlot; i < toSlot; ++i)
            slots[(size_t) i] = std::move (slots[(size_t) (i + 1)]);
    }
    else
    {
        for (int i = fromSlot; i > toSlot; --i)
            slots[(size_t) i] = std::move (slots[(size_t) (i - 1)]);
    }

    slots[(size_t) toSlot] = std::move (moving);
}

void EffectsChain::clear()
{
    const juce::ScopedLock sl (swapLock);

    for (auto& slot : slots)
    {
        if (slot.pedal != nullptr)
            retired.push_back (std::move (slot.pedal));

        slot.type = PedalType::None;
        slot.bypassed = false;
        slot.mix = 1.0;
    }

    retired.clear();
}

//==============================================================================
void EffectsChain::setTempoBpm (double bpm) noexcept
{
    tempoBpm = juce::jlimit (20.0, 300.0, bpm);

    for (auto& slot : slots)
        if (slot.pedal != nullptr)
            slot.pedal->setTempoBpm (tempoBpm);
}

void EffectsChain::setExpression (double value) noexcept
{
    expression = juce::jlimit (0.0, 1.0, value);

    for (auto& slot : slots)
        if (slot.pedal != nullptr)
            slot.pedal->setExpression (expression);
}

void EffectsChain::setOversamplingFactor (int factor) noexcept
{
    oversamplingFactor = juce::jlimit (1, 8, factor);

    // SPEC-SWEEP JG-4: a virtual call rather than a dynamic_cast per slot.
    for (auto& slot : slots)
        if (slot.pedal != nullptr)
            slot.pedal->setOversamplingFactor (oversamplingFactor);
}

int EffectsChain::getLatencySamples() const noexcept
{
    int total = 0;

    for (const auto& slot : slots)
        if (slot.pedal != nullptr && ! slot.bypassed)
            total += slot.pedal->getLatencySamples();

    return total;
}

//==============================================================================
void EffectsChain::processStereo (double* left, double* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    const juce::ScopedTryLock sl (swapLock);

    if (! sl.isLocked())
        return;   // A slot is being swapped this instant; pass the block through.

    for (auto& slot : slots)
        if (slot.pedal != nullptr)
            slot.pedal->processWithBypass (left, right, numSamples);
}

void EffectsChain::processBlock (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0 || (int) workL.size() < numSamples)
        return;

    auto* l = buffer.getWritePointer (0);
    auto* r = (numChannels > 1) ? buffer.getWritePointer (1) : l;

    for (int i = 0; i < numSamples; ++i)
    {
        workL[(size_t) i] = (double) l[i];
        workR[(size_t) i] = (double) r[i];
    }

    processStereo (workL.data(), workR.data(), numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        l[i] = (float) workL[(size_t) i];

        if (numChannels > 1)
            r[i] = (float) workR[(size_t) i];
    }
}

} // namespace luthier
