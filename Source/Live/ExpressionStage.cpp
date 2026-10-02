#include "ExpressionStage.h"

namespace luthier
{

ExpressionStage::ExpressionStage()
{
    tables.resetAll (Table {});
    live = &tables.current();
}

void ExpressionStage::update (const ExpressionCalibrationSet& set)
{
    if (set.getVersion() == builtFromVersion)
        return;

    builtFromVersion = set.getVersion();

    auto& t = tables.writerSlot();
    t.any = false;

    for (int cc = 0; cc < 128; ++cc)
    {
        t.active[(size_t) cc] = set.has (cc);

        if (! t.active[(size_t) cc])
            continue;

        t.any = true;
        const auto calibration = set.get (cc);

        for (int raw = 0; raw < 128; ++raw)
            t.value[(size_t) cc][(size_t) raw] = (juce::uint8) juce::jlimit (0, 127,
                                                   (int) std::lround (calibration.map (raw) * 127.0));
    }

    tables.publish();
    publishedGeneration.fetch_add (1, std::memory_order_release);
}

int ExpressionStage::drainObserved (ExpressionCalibrationSet& set)
{
    const int ready = observedFifo.getNumReady();

    if (ready <= 0)
        return 0;

    int start1, size1, start2, size2;
    observedFifo.prepareToRead (ready, start1, size1, start2, size2);

    const int cc = set.getWizardCc();

    for (int i = 0; i < size1; ++i)
        set.observe (cc, observed[(size_t) (start1 + i)]);

    for (int i = 0; i < size2; ++i)
        set.observe (cc, observed[(size_t) (start2 + i)]);

    observedFifo.finishedRead (size1 + size2);
    return size1 + size2;
}

void ExpressionStage::process (juce::MidiBuffer& midi, juce::MidiBuffer& scratch) noexcept
{
    const auto generation = publishedGeneration.load (std::memory_order_acquire);

    if (generation != appliedGeneration)
    {
        appliedGeneration = generation;
        live = &tables.acquire();
    }

    const int watch = observedCc.load (std::memory_order_relaxed);

    if (midi.isEmpty() || (! live->any && watch < 0))
        return;

    bool remapped = false;

    for (const auto metadata : midi)
    {
        if (metadata.numBytes != 3 || (metadata.data[0] & 0xf0) != 0xb0)
            continue;

        const int cc = metadata.data[1] & 0x7f;
        const int raw = metadata.data[2] & 0x7f;

        // The wizard sees the pedal's raw travel, before any calibration.
        if (cc == watch)
        {
            int start1, size1, start2, size2;
            observedFifo.prepareToWrite (1, start1, size1, start2, size2);

            if (size1 + size2 > 0)
            {
                observed[(size_t) (size1 > 0 ? start1 : start2)] = (juce::int16) raw;
                observedFifo.finishedWrite (1);
            }
        }

        if (live->active[(size_t) cc] && live->value[(size_t) cc][(size_t) raw] != raw)
            remapped = true;
    }

    if (! remapped)
        return;

    scratch.clear();

    for (const auto metadata : midi)
    {
        if (metadata.numBytes == 3 && (metadata.data[0] & 0xf0) == 0xb0
              && live->active[(size_t) (metadata.data[1] & 0x7f)])
        {
            const juce::uint8 bytes[3] = { metadata.data[0], metadata.data[1],
                                           live->value[(size_t) (metadata.data[1] & 0x7f)][(size_t) (metadata.data[2] & 0x7f)] };
            scratch.addEvent (bytes, 3, metadata.samplePosition);
        }
        else
        {
            scratch.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);
        }
    }

    midi.clear();

    for (const auto metadata : scratch)
        midi.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);

    scratch.clear();
}

} // namespace luthier
