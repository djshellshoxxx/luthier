#include "RoutingMatrix.h"

namespace luthier
{

//==============================================================================
const char* getAuxBusName (int index) noexcept
{
    switch (index)
    {
        case (int) AuxBus::di:        return "DI";
        case (int) AuxBus::ampPreCab: return "Amp (pre-cab)";
        case (int) AuxBus::cabMic1:   return "Cab Mic 1";
        case (int) AuxBus::cabMic2:   return "Cab Mic 2";
        case (int) AuxBus::roomMic:   return "Room";
        case (int) AuxBus::wetFx:     return "Wet FX";
        case (int) AuxBus::monitor:   return "Monitor";
        default:                      return "Aux";
    }
}

const char* getAuxBusTapDescription (int index) noexcept
{
    switch (index)
    {
        case (int) AuxBus::di:        return "Raw pickup output, post cable sim, pre-amp";
        case (int) AuxBus::ampPreCab: return "Amp output before the cabinet";
        case (int) AuxBus::cabMic1:   return "Cabinet mic 1 alone, post-cab";
        case (int) AuxBus::cabMic2:   return "Cabinet mic 2 alone, post-cab";
        case (int) AuxBus::roomMic:   return "Room mics alone, post-room";
        case (int) AuxBus::wetFx:     return "Reverb and delay tails alone";
        case (int) AuxBus::monitor:   return "Monitor mix, post-master";
        default:                      return "";
    }
}

const char* getBusLayoutName (BusLayout layout) noexcept
{
    switch (layout)
    {
        case BusLayout::stereoOnly: return "A - Stereo";
        case BusLayout::studio:     return "B - Studio (7 aux)";
        case BusLayout::perString:  return "C - Per-string";
        case BusLayout::full:       return "D - Studio + per-string";
        default:                    return "A - Stereo";
    }
}

//==============================================================================
RoutingMatrix::RoutingMatrix()
{
    for (auto& g : stringGainDb)
        g.store (0.0f);

    for (auto& m : stringMuted)
        m.store (false);
}

void RoutingMatrix::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sr = sampleRate;

    for (auto& a : auxes)
    {
        a.gain.prepare (sampleRate, constants::kParamSmoothSeconds);
        a.gain.snapTo (a.muted.load() ? 0.0
                                      : juce::Decibels::decibelsToGain ((double) a.gainDb.load()));
        a.level.store (0.0f);
    }

    for (size_t s = 0; s < stringGain.size(); ++s)
    {
        stringGain[s].prepare (sampleRate, constants::kParamSmoothSeconds);
        stringGain[s].snapTo (stringMuted[s].load()
                                ? 0.0
                                : juce::Decibels::decibelsToGain ((double) stringGainDb[s].load()));
    }

    sidechainLevel.store (0.0);
}

void RoutingMatrix::reset() noexcept
{
    for (auto& a : auxes)
    {
        a.gain.snapToTarget();
        a.level.store (0.0f);
    }

    for (auto& g : stringGain)
        g.snapToTarget();

    sidechainLevel.store (0.0);
}

//==============================================================================
void RoutingMatrix::setAuxMuted (int bus, bool muted) noexcept
{
    if (juce::isPositiveAndBelow (bus, kNumAuxBuses))
        auxes[(size_t) bus].muted.store (muted, std::memory_order_relaxed);
}

bool RoutingMatrix::isAuxMuted (int bus) const noexcept
{
    return juce::isPositiveAndBelow (bus, kNumAuxBuses)
             && auxes[(size_t) bus].muted.load (std::memory_order_relaxed);
}

void RoutingMatrix::setAuxSoloed (int bus, bool soloed) noexcept
{
    if (juce::isPositiveAndBelow (bus, kNumAuxBuses))
        auxes[(size_t) bus].soloed.store (soloed, std::memory_order_relaxed);
}

bool RoutingMatrix::isAuxSoloed (int bus) const noexcept
{
    return juce::isPositiveAndBelow (bus, kNumAuxBuses)
             && auxes[(size_t) bus].soloed.load (std::memory_order_relaxed);
}

void RoutingMatrix::setAuxGainDb (int bus, double db) noexcept
{
    if (juce::isPositiveAndBelow (bus, kNumAuxBuses))
        auxes[(size_t) bus].gainDb.store ((float) juce::jlimit (-60.0, 12.0, db),
                                          std::memory_order_relaxed);
}

double RoutingMatrix::getAuxGainDb (int bus) const noexcept
{
    return juce::isPositiveAndBelow (bus, kNumAuxBuses)
             ? (double) auxes[(size_t) bus].gainDb.load (std::memory_order_relaxed)
             : 0.0;
}

bool RoutingMatrix::isAnyAuxSoloed() const noexcept
{
    for (const auto& a : auxes)
        if (a.soloed.load (std::memory_order_relaxed))
            return true;

    return false;
}

bool RoutingMatrix::isAuxAudible (int bus) const noexcept
{
    if (! juce::isPositiveAndBelow (bus, kNumAuxBuses))
        return false;

    if (isAnyAuxSoloed())
        return auxes[(size_t) bus].soloed.load (std::memory_order_relaxed);

    return ! auxes[(size_t) bus].muted.load (std::memory_order_relaxed);
}

double RoutingMatrix::getAuxLevel (int bus) const noexcept
{
    return juce::isPositiveAndBelow (bus, kNumAuxBuses)
             ? (double) auxes[(size_t) bus].level.load (std::memory_order_relaxed)
             : 0.0;
}

//==============================================================================
void RoutingMatrix::setPerStringGainDb (int stringIndex, double db) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kNumPerStringBuses))
        stringGainDb[(size_t) stringIndex].store ((float) juce::jlimit (-60.0, 12.0, db),
                                                  std::memory_order_relaxed);
}

double RoutingMatrix::getPerStringGainDb (int stringIndex) const noexcept
{
    return juce::isPositiveAndBelow (stringIndex, kNumPerStringBuses)
             ? (double) stringGainDb[(size_t) stringIndex].load (std::memory_order_relaxed)
             : 0.0;
}

void RoutingMatrix::setPerStringMuted (int stringIndex, bool muted) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kNumPerStringBuses))
        stringMuted[(size_t) stringIndex].store (muted, std::memory_order_relaxed);
}

bool RoutingMatrix::isPerStringMuted (int stringIndex) const noexcept
{
    return juce::isPositiveAndBelow (stringIndex, kNumPerStringBuses)
             && stringMuted[(size_t) stringIndex].load (std::memory_order_relaxed);
}

//==============================================================================
void RoutingMatrix::meterSidechain (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (channels == nullptr || numChannels <= 0 || numSamples <= 0)
    {
        sidechainLevel.store (0.0, std::memory_order_relaxed);
        return;
    }

    double peak = 0.0;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (channels[ch] == nullptr)
            continue;

        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax (peak, std::abs ((double) channels[ch][i]));
    }

    // Fast attack, slow release: a meter you can actually read.
    const double previous = sidechainLevel.load (std::memory_order_relaxed);
    const double smoothed = (peak > previous) ? peak : previous * 0.82 + peak * 0.18;

    sidechainLevel.store (sanitise (smoothed), std::memory_order_relaxed);
}

//==============================================================================
void RoutingMatrix::setMidiOutConfig (const MidiOutConfig& cfg)
{
    const juce::ScopedLock sl (midiOutLock);
    midiOut = cfg;
}

MidiOutConfig RoutingMatrix::getMidiOutConfig() const
{
    const juce::ScopedLock sl (midiOutLock);
    return midiOut;
}

//==============================================================================
void RoutingMatrix::updateWantedTaps (TapBuffers& taps, int numStrings) noexcept
{
    const bool aux = layoutHasAux (activeLayout);

    for (int bus = 0; bus < kNumAuxBuses; ++bus)
        taps.setAuxWanted (bus, aux && isAuxAudible (bus));

    taps.setPerStringWanted (layoutHasPerString (activeLayout) && numStrings > 0);
}

void RoutingMatrix::writeMonitorBus (juce::AudioProcessor& processor,
                                     juce::AudioBuffer<float>& buffer,
                                     const juce::AudioBuffer<float>& monitor,
                                     int numSamples) noexcept
{
    if (! layoutHasAux (activeLayout) || monitor.getNumChannels() < 2)
        return;

    constexpr int auxIndex = (int) AuxBus::monitor;

    // Bus 0 is the main output, so aux n lives on bus n + 1.
    const int bus = auxIndex + 1;

    if (bus >= processor.getBusCount (false))
        return;

    auto out = processor.getBusBuffer (buffer, false, bus);

    if (out.getNumChannels() <= 0)
        return;

    const int n = juce::jmin (numSamples, monitor.getNumSamples(), out.getNumSamples());

    if (n <= 0)
        return;

    auto& state = auxes[(size_t) auxIndex];

    const bool audible = isAuxAudible (auxIndex);
    state.gain.setTarget (audible
                            ? juce::Decibels::decibelsToGain (
                                  (double) state.gainDb.load (std::memory_order_relaxed))
                            : 0.0);

    const auto* srcL = monitor.getReadPointer (0);
    const auto* srcR = monitor.getReadPointer (1);

    auto* dstL = out.getWritePointer (0);
    auto* dstR = (out.getNumChannels() > 1) ? out.getWritePointer (1) : nullptr;

    double peak = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double g = state.gain.next();
        const double l = (double) srcL[i] * g;
        const double r = (double) srcR[i] * g;

        dstL[i] = (float) sanitise (l);

        if (dstR != nullptr)
            dstR[i] = (float) sanitise (r);

        peak = juce::jmax (peak, std::abs (l), std::abs (r));
    }

    state.level.store ((float) peak, std::memory_order_relaxed);
}

void RoutingMatrix::distribute (juce::AudioProcessor& processor,
                                juce::AudioBuffer<float>& buffer,
                                const TapBuffers& taps,
                                int numStrings) noexcept
{
    const int numSamples = juce::jmin (buffer.getNumSamples(), taps.getNumSamples());

    if (numSamples <= 0)
        return;

    const int numOutputBuses = processor.getBusCount (false);
    const bool hasAux = layoutHasAux (activeLayout);

    // Bus 0 is the main output and is already written by the engine. Buses 1..7
    // are the aux pairs when the layout has them; anything after that is a
    // per-string mono bus.
    for (int bus = 1; bus < numOutputBuses; ++bus)
    {
        auto out = processor.getBusBuffer (buffer, false, bus);

        if (out.getNumChannels() <= 0)
            continue;

        const int auxIndex = bus - 1;
        const bool isAux = hasAux && auxIndex < kNumAuxBuses;
        const int stringIndex = hasAux ? bus - 1 - kNumAuxBuses : bus - 1;

        if (isAux)
        {
            // The monitor bus is built after the master rather than tapped, so
            // writeMonitorBus owns it. Falling through to the tap path here would
            // advance its gain smoother a second time, halving the length of its
            // mute ramp. It is cleared rather than skipped, so that a block in
            // which the monitor is switched off leaves silence on the bus instead
            // of the last block the monitor rendered.
            if (auxIndex == (int) AuxBus::monitor)
            {
                out.clear();
                continue;
            }

            auto& state = auxes[(size_t) auxIndex];

            const bool audible = isAuxAudible (auxIndex);
            state.gain.setTarget (audible
                                    ? juce::Decibels::decibelsToGain (
                                          (double) state.gainDb.load (std::memory_order_relaxed))
                                    : 0.0);

            // A tap that was not rendered is silence, but the gain still has to
            // run so that un-muting ramps up from zero instead of jumping.
            const bool rendered = taps.isAuxWanted (auxIndex);

            const auto* srcL = taps.auxRead (auxIndex, 0);
            const auto* srcR = taps.auxRead (auxIndex, 1);

            auto* dstL = out.getWritePointer (0);
            auto* dstR = (out.getNumChannels() > 1) ? out.getWritePointer (1) : nullptr;

            double peak = 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                const double g = state.gain.next();
                const double l = rendered ? (double) srcL[i] * g : 0.0;
                const double r = rendered ? (double) srcR[i] * g : 0.0;

                dstL[i] = (float) sanitise (l);

                if (dstR != nullptr)
                    dstR[i] = (float) sanitise (r);

                peak = juce::jmax (peak, std::abs (l), std::abs (r));
            }

            state.level.store ((float) peak, std::memory_order_relaxed);
        }
        else if (layoutHasPerString (activeLayout)
                   && juce::isPositiveAndBelow (stringIndex, kNumPerStringBuses))
        {
            auto* dst = out.getWritePointer (0);

            // Strings the current guitar does not have output silence, as the
            // spec requires - not stale data from a twelve-string preset.
            const bool rendered = taps.isPerStringWanted() && stringIndex < numStrings;
            const bool live = rendered && ! isPerStringMuted (stringIndex);

            auto& g = stringGain[(size_t) stringIndex];
            g.setTarget (live
                           ? juce::Decibels::decibelsToGain (
                                 (double) stringGainDb[(size_t) stringIndex].load (std::memory_order_relaxed))
                           : 0.0);

            const auto* src = taps.stringRead (stringIndex);

            for (int i = 0; i < numSamples; ++i)
            {
                const double gain = g.next();
                dst[i] = (float) sanitise (rendered ? (double) src[i] * gain : 0.0);
            }

            for (int ch = 1; ch < out.getNumChannels(); ++ch)
                out.clear (ch, 0, numSamples);
        }
        else
        {
            out.clear();
        }
    }
}

//==============================================================================
void RoutingMatrix::setLatencyReport (const LatencyReport& r) noexcept
{
    latMain.store (r.mainOut, std::memory_order_relaxed);
    latDi.store (r.auxDi, std::memory_order_relaxed);
    latPreCab.store (r.auxPreCab, std::memory_order_relaxed);
    latString.store (r.perString, std::memory_order_relaxed);
}

RoutingMatrix::LatencyReport RoutingMatrix::getLatencyReport() const noexcept
{
    LatencyReport r;
    r.mainOut = latMain.load (std::memory_order_relaxed);
    r.auxDi = latDi.load (std::memory_order_relaxed);
    r.auxPreCab = latPreCab.load (std::memory_order_relaxed);
    r.perString = latString.load (std::memory_order_relaxed);
    return r;
}

//==============================================================================
juce::var RoutingMatrix::toVar() const
{
    auto* root = new juce::DynamicObject();

    juce::Array<juce::var> auxArray;

    for (int bus = 0; bus < kNumAuxBuses; ++bus)
    {
        auto* a = new juce::DynamicObject();
        a->setProperty ("mute", isAuxMuted (bus));
        a->setProperty ("solo", isAuxSoloed (bus));
        a->setProperty ("gain", getAuxGainDb (bus));
        auxArray.add (juce::var (a));
    }

    root->setProperty ("aux", auxArray);

    juce::Array<juce::var> stringArray;

    for (int s = 0; s < kNumPerStringBuses; ++s)
    {
        auto* st = new juce::DynamicObject();
        st->setProperty ("mute", isPerStringMuted (s));
        st->setProperty ("gain", getPerStringGainDb (s));
        stringArray.add (juce::var (st));
    }

    root->setProperty ("perString", stringArray);
    root->setProperty ("sidechainToAmp", isSidechainToAmp());

    const auto cfg = getMidiOutConfig();

    auto* m = new juce::DynamicObject();
    m->setProperty ("enabled", cfg.enabled);
    m->setProperty ("passThrough", cfg.passThrough);
    m->setProperty ("rhythm", cfg.rhythmEngine);
    m->setProperty ("stringActivity", cfg.stringActivity);
    m->setProperty ("ccBroadcast", cfg.ccBroadcast);
    m->setProperty ("tunePlayback", cfg.tunePlayback);
    m->setProperty ("luthierEvents", cfg.luthierEvents);
    m->setProperty ("workshopChanges", cfg.workshopChanges);
    m->setProperty ("channel", cfg.channel);

    juce::Array<juce::var> ccArray;

    for (auto cc : cfg.macroCc)
        ccArray.add (cc);

    m->setProperty ("macroCc", ccArray);
    root->setProperty ("midiOut", juce::var (m));

    return juce::var (root);
}

void RoutingMatrix::fromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    // Anything the stored state does not mention goes back to its default, so
    // loading a preset never leaves a previous preset's mute behind.
    for (int bus = 0; bus < kNumAuxBuses; ++bus)
    {
        setAuxMuted (bus, false);
        setAuxSoloed (bus, false);
        setAuxGainDb (bus, 0.0);
    }

    for (int s = 0; s < kNumPerStringBuses; ++s)
    {
        setPerStringMuted (s, false);
        setPerStringGainDb (s, 0.0);
    }

    if (auto* auxArray = root->getProperty ("aux").getArray())
    {
        for (int bus = 0; bus < juce::jmin (kNumAuxBuses, auxArray->size()); ++bus)
        {
            if (auto* a = auxArray->getReference (bus).getDynamicObject())
            {
                setAuxMuted (bus, (bool) a->getProperty ("mute"));
                setAuxSoloed (bus, (bool) a->getProperty ("solo"));
                setAuxGainDb (bus, (double) a->getProperty ("gain"));
            }
        }
    }

    if (auto* stringArray = root->getProperty ("perString").getArray())
    {
        for (int s = 0; s < juce::jmin (kNumPerStringBuses, stringArray->size()); ++s)
        {
            if (auto* st = stringArray->getReference (s).getDynamicObject())
            {
                setPerStringMuted (s, (bool) st->getProperty ("mute"));
                setPerStringGainDb (s, (double) st->getProperty ("gain"));
            }
        }
    }

    setSidechainToAmp ((bool) root->getProperty ("sidechainToAmp"));

    MidiOutConfig cfg;

    if (auto* m = root->getProperty ("midiOut").getDynamicObject())
    {
        cfg.enabled = (bool) m->getProperty ("enabled");
        cfg.passThrough = m->hasProperty ("passThrough") ? (bool) m->getProperty ("passThrough") : true;
        cfg.rhythmEngine = (bool) m->getProperty ("rhythm");
        cfg.stringActivity = (bool) m->getProperty ("stringActivity");
        cfg.ccBroadcast = (bool) m->getProperty ("ccBroadcast");
        cfg.tunePlayback = (bool) m->getProperty ("tunePlayback");
        cfg.luthierEvents = (bool) m->getProperty ("luthierEvents");
        cfg.workshopChanges = (bool) m->getProperty ("workshopChanges");
        cfg.channel = juce::jlimit (1, 16, m->hasProperty ("channel") ? (int) m->getProperty ("channel") : 1);

        if (auto* ccArray = m->getProperty ("macroCc").getArray())
            for (int i = 0; i < juce::jmin ((int) cfg.macroCc.size(), ccArray->size()); ++i)
                cfg.macroCc[(size_t) i] = juce::jlimit (-1, 127, (int) ccArray->getReference (i));
    }

    setMidiOutConfig (cfg);
}

} // namespace luthier
