#include "MidiOutRouter.h"

namespace luthier
{

namespace
{
    /** Bytes reserved per block. A MidiBuffer stores three bytes of message plus
        a small header per event, so this covers well over a thousand events -
        far past anything a host will hand us in one block, and cheap enough that
        reserving it costs nothing. */
    constexpr int kReservedBytes = 16384;
}

//==============================================================================
MidiOutRouter::MidiOutRouter()
{
    for (auto& m : macroValues)
        m.store (0.0f);

    lastSentMacro.fill (-1.0f);
}

void MidiOutRouter::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    // Reserve now so that clear() leaves capacity behind and the audio thread
    // never has to grow either buffer.
    captured.ensureSize (kReservedBytes);
    rhythm.ensureSize (kReservedBytes);

    captured.clear();
    rhythm.clear();

    reset();
}

void MidiOutRouter::reset() noexcept
{
    captured.clear();
    rhythm.clear();

    // -1 forces every assigned macro to transmit once after a reset, so the
    // receiving device is never left showing a value we think we already sent.
    lastSentMacro.fill (-1.0f);

    overflow.store (0, std::memory_order_relaxed);
}

//==============================================================================
void MidiOutRouter::captureInput (const juce::MidiBuffer& incoming) noexcept
{
    captured.clear();

    for (const auto metadata : incoming)
        captured.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);
}

void MidiOutRouter::setMacroValue (int macroIndex, float value) noexcept
{
    if (juce::isPositiveAndBelow (macroIndex, (int) macroValues.size()))
        macroValues[(size_t) macroIndex].store (juce::jlimit (0.0f, 1.0f, value),
                                                std::memory_order_relaxed);
}

//==============================================================================
void MidiOutRouter::emit (juce::MidiBuffer& midiMessages,
                          const MidiOutConfig& cfg,
                          const StringActivityQueue& stringActivity,
                          int numSamples) noexcept
{
    // Disabled means the plugin emits nothing at all. Leaving the host's own
    // events in the buffer would be an accidental pass-through.
    if (! cfg.enabled)
    {
        midiMessages.clear();
        rhythm.clear();
        return;
    }

    midiMessages.clear();

    const int lastSample = juce::jmax (0, numSamples - 1);
    const int channel = juce::jlimit (1, 16, cfg.channel);

    // ---- pass-through ---------------------------------------------------------
    // The original messages at their original timestamps: byte-identical, which
    // is what makes the timestamps exact rather than merely close.
    if (cfg.passThrough)
        for (const auto metadata : captured)
            midiMessages.addEvent (metadata.data, metadata.numBytes,
                                   juce::jlimit (0, lastSample, metadata.samplePosition));

    // ---- rhythm engine --------------------------------------------------------
    if (cfg.rhythmEngine)
        for (const auto metadata : rhythm)
            midiMessages.addEvent (metadata.data, metadata.numBytes,
                                   juce::jlimit (0, lastSample, metadata.samplePosition));

    // ---- string activity ------------------------------------------------------
    if (cfg.stringActivity)
    {
        const int n = stringActivity.size();

        for (int i = 0; i < n; ++i)
        {
            const auto& e = stringActivity[i];

            if (e.preview)
                continue;   // riff-library 5.3

            const int note = juce::jlimit (0, 127, e.midiNote);
            const int offset = juce::jlimit (0, lastSample, e.sampleOffset);

            const auto message = e.isNoteOn
                                   ? juce::MidiMessage::noteOn (channel, note, e.velocity)
                                   : juce::MidiMessage::noteOff (channel, note);

            midiMessages.addEvent (message, offset);
        }

        if (stringActivity.getDroppedCount() > 0)
            overflow.fetch_add (stringActivity.getDroppedCount(), std::memory_order_relaxed);
    }

    // ---- CC broadcast ---------------------------------------------------------
    if (cfg.ccBroadcast)
    {
        for (size_t m = 0; m < macroValues.size(); ++m)
        {
            const int cc = cfg.macroCc[m];

            if (! juce::isPositiveAndBelow (cc, 128))
                continue;

            const float value = macroValues[m].load (std::memory_order_relaxed);
            const int coarse = juce::jlimit (0, 127, juce::roundToInt (value * 127.0f));
            const int lastCoarse = (lastSentMacro[m] < 0.0f)
                                     ? -1
                                     : juce::jlimit (0, 127, juce::roundToInt (lastSentMacro[m] * 127.0f));

            // Only a change in the transmitted seven-bit value is worth a
            // message. Sending an unchanged CC every block would flood the wire
            // and tell the receiver nothing.
            if (coarse == lastCoarse)
                continue;

            midiMessages.addEvent (juce::MidiMessage::controllerEvent (channel, cc, coarse), 0);
            lastSentMacro[m] = value;
        }
    }

    rhythm.clear();
}

} // namespace luthier
