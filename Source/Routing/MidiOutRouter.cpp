#include "MidiOutRouter.h"
#include "../Support/BoundedMidi.h"
#include <algorithm>

namespace luthier
{

namespace
{
    /** Bytes reserved per block. A MidiBuffer stores three bytes of message plus
        a small header per event, so this covers well over a thousand events -
        far past anything a host will hand us in one block, and cheap enough that
        reserving it costs nothing. */
    constexpr int kReservedBytes = luthier::BoundedMidi::kReserveBytes;   // RT-SAFETY P1: was 16 KB
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
    for (auto& notes : rhythmHeld)
        notes.reset();
    rhythmHeldCount = 0;
    rhythmDrainPending = false;

    overflow.store (0, std::memory_order_relaxed);
}

//==============================================================================
void MidiOutRouter::captureInput (const juce::MidiBuffer& incoming) noexcept
{
    captured.clear();

    // RT-SAFETY P1: never grows the reserved buffer; a burst beyond it keeps
    // its note-offs and the rest is dropped and counted.
    for (const auto metadata : incoming)
        if (! BoundedMidi::add (captured, metadata.data, metadata.numBytes, metadata.samplePosition))
            overflow.fetch_add (1, std::memory_order_relaxed);
}

void MidiOutRouter::setMacroValue (int macroIndex, float value) noexcept
{
    if (juce::isPositiveAndBelow (macroIndex, (int) macroValues.size()))
        macroValues[(size_t) macroIndex].store (juce::jlimit (0.0f, 1.0f, value),
                                                std::memory_order_relaxed);
}

//==============================================================================
void MidiOutRouter::emit (juce::MidiBuffer& dest, const MidiOutConfig& config,
                          const StringActivityQueue& activity, int n) noexcept
{ emitImpl (dest, config, activity, n, -1); }
void MidiOutRouter::emitBounded (juce::MidiBuffer& dest, const MidiOutConfig& config,
                                 const StringActivityQueue& activity, int n, int bytes) noexcept
{ emitImpl (dest, config, activity, n, std::max (0, bytes)); }
void MidiOutRouter::emitImpl (juce::MidiBuffer& midiMessages, const MidiOutConfig& cfg,
                              const StringActivityQueue& stringActivity, int numSamples, int reservedBytes) noexcept
{
    midiMessages.clear();
    if ((! cfg.enabled || ! cfg.rhythmEngine) && rhythmHeldCount > 0)
        rhythmDrainPending = true;
    if (numSamples <= 0)
    {
        rhythm.clear();
        return;
    }

    auto append = [&] (const juce::uint8* bytes, int size, int offset)
    {
        if (reservedBytes < 0)
            return midiMessages.addEvent (bytes, size, offset);
        if (BoundedMidi::add (midiMessages, reservedBytes, bytes, size, offset))
            return true;
        overflow.fetch_add (1, std::memory_order_relaxed);
        return false;
    };

    // A source/global switch must close notes that reached the host. Retain any
    // release that did not fit, even if the source is switched back on meanwhile.
    if (rhythmDrainPending)
    {
        bool remaining = false;
        for (size_t ch = 0; ch < rhythmHeld.size(); ++ch)
            for (size_t note = 0; note < 128; ++note)
                if (rhythmHeld[ch].test (note))
                {
                    const juce::uint8 bytes[] { (juce::uint8) (0x80 | ch), (juce::uint8) note, 0 };
                    if (append (bytes, 3, 0))
                    {
                        rhythmHeld[ch].reset (note);
                        --rhythmHeldCount;
                    }
                    else remaining = true;
                }
        rhythmDrainPending = remaining;
    }
    if (! cfg.enabled)
    {
        rhythm.clear();
        return; // Only outstanding releases, never accidental input echo.
    }
    const int lastSample = juce::jmax (0, numSamples - 1);
    const int channel = juce::jlimit (1, 16, cfg.channel);

    // ---- pass-through ---------------------------------------------------------
    // The original messages at their original timestamps: byte-identical, which
    // is what makes the timestamps exact rather than merely close.
    if (cfg.passThrough)
        for (const auto metadata : captured)
            append (metadata.data, metadata.numBytes, juce::jlimit (0, lastSample, metadata.samplePosition));

    // ---- rhythm engine --------------------------------------------------------
    if (cfg.rhythmEngine && ! rhythmDrainPending)
        for (const auto metadata : rhythm)
        {
            if (! append (metadata.data, metadata.numBytes, juce::jlimit (0, lastSample, metadata.samplePosition)))
            {
                if (BoundedMidi::isRelease (metadata.data, metadata.numBytes))
                {
                    rhythmDrainPending = true;
                    break; // Retry releases before any source writes next block.
                }
                continue;
            }
            if (metadata.numBytes == 3 && metadata.data[1] < 128)
            {
                const auto* bytes = metadata.data;
                auto& held = rhythmHeld[(size_t) (bytes[0] & 0x0f)];
                if ((bytes[0] & 0xf0) == 0x90 && bytes[2] > 0 && ! held.test (bytes[1]))
                {
                    held.set (bytes[1]);
                    ++rhythmHeldCount;
                }
                else if (((bytes[0] & 0xf0) == 0x80 || ((bytes[0] & 0xf0) == 0x90 && bytes[2] == 0)) && held.test (bytes[1]))
                {
                    held.reset (bytes[1]);
                    --rhythmHeldCount;
                }
            }
        }

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

            append (message.getRawData(), message.getRawDataSize(), offset);
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

            const auto message = juce::MidiMessage::controllerEvent (channel, cc, coarse);
            append (message.getRawData(), message.getRawDataSize(), 0);
            lastSentMacro[m] = value;
        }
    }

    rhythm.clear();
}

} // namespace luthier
