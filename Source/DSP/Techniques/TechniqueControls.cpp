#include "TechniqueControls.h"

namespace luthier
{

void TechniqueControls::reset() noexcept
{
    for (auto& row : ccs)
        row.fill (0.0f);

    for (auto& row : ccSeen)
        row.fill (false);

    // Controllers that rest at their centre.
    for (auto& row : ccs)
        row[74] = 0.5f;

    bends.fill (0.0f);
    bendSeen.fill (false);
    pressures.fill (0.0f);
    pressureSeen.fill (false);
}

void TechniqueControls::processMidi (const juce::MidiBuffer& midi) noexcept
{
    for (const auto m : midi)
    {
        const auto* d = m.data;

        if (m.numBytes < 2 || d[0] >= 0xf0)
            continue;

        const int type = d[0] & 0xf0;
        const int ch = (d[0] & 0x0f) + 1;

        if (type == 0xb0 && m.numBytes >= 3)
        {
            const float v = (float) d[2] / 127.0f;
            ccs[(size_t) ch][d[1] & 0x7f] = ccs[0][d[1] & 0x7f] = v;
            ccSeen[(size_t) ch][d[1] & 0x7f] = ccSeen[0][d[1] & 0x7f] = true;
        }
        else if (type == 0xe0 && m.numBytes >= 3)
        {
            const int raw = (d[1] & 0x7f) | ((d[2] & 0x7f) << 7);
            const float v = juce::jlimit (-1.0f, 1.0f, (float) (raw - 8192) / 8191.0f);
            bends[(size_t) ch] = bends[0] = v;
            bendSeen[(size_t) ch] = bendSeen[0] = true;
        }
        else if (type == 0xd0)
        {
            const float v = (float) d[1] / 127.0f;
            pressures[(size_t) ch] = pressures[0] = v;
            pressureSeen[(size_t) ch] = pressureSeen[0] = true;
        }
        else if (type == 0xa0 && m.numBytes >= 3)
        {
            // Poly aftertouch reads as the channel's pressure.
            const float v = (float) d[2] / 127.0f;
            pressures[(size_t) ch] = pressures[0] = v;
            pressureSeen[(size_t) ch] = pressureSeen[0] = true;
        }
    }
}

double TechniqueControls::pitchBend (int channel) const noexcept
{
    return bends[(size_t) juce::jlimit (0, 16, channel)];
}

double TechniqueControls::cc (int number, int channel) const noexcept
{
    return ccs[(size_t) juce::jlimit (0, 16, channel)][(size_t) juce::jlimit (0, 127, number)];
}

double TechniqueControls::pressure (int channel) const noexcept
{
    return pressures[(size_t) juce::jlimit (0, 16, channel)];
}

bool TechniqueControls::hasValue (ControlSource source, int ccNumber, int channel) const noexcept
{
    const auto ch = (size_t) juce::jlimit (0, 16, channel);

    switch (source)
    {
        case ControlSource::modWheel:   return ccSeen[ch][1];
        case ControlSource::expression: return ccSeen[ch][11];
        case ControlSource::mpeY:       return ccSeen[ch][74];
        case ControlSource::customCc:   return ccSeen[ch][(size_t) juce::jlimit (0, 127, ccNumber)];
        case ControlSource::pitchBend:  return bendSeen[ch];
        case ControlSource::aftertouch:
        case ControlSource::mpeZ:       return pressureSeen[ch];
        case ControlSource::fretboard:  return fretboardSlide.load() >= 0.0;
        case ControlSource::none:
        case ControlSource::numSources:
        default:                        return false;
    }
}

double TechniqueControls::read (ControlSource source, int ccNumber, int channel, bool bipolar) const noexcept
{
    auto fromUnipolar = [bipolar] (double v) { return bipolar ? (v - 0.5) * 2.0 : v; };

    switch (source)
    {
        case ControlSource::modWheel:   return fromUnipolar (cc (1, channel));
        case ControlSource::expression: return fromUnipolar (cc (11, channel));
        case ControlSource::mpeY:       return fromUnipolar (cc (74, channel));
        case ControlSource::customCc:   return fromUnipolar (cc (ccNumber, channel));
        case ControlSource::aftertouch: return fromUnipolar (pressure (0));
        case ControlSource::mpeZ:       return fromUnipolar (pressure (channel));

        case ControlSource::pitchBend:
        {
            const double b = pitchBend (channel);
            return bipolar ? b : juce::jmax (0.0, b);
        }

        case ControlSource::fretboard:
        {
            const double v = juce::jmax (0.0, fretboardSlide.load());
            return fromUnipolar (v);
        }

        case ControlSource::none:
        case ControlSource::numSources:
        default:
            return 0.0;
    }
}

} // namespace luthier
