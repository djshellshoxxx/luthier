#include "GuitarCircuit.h"

namespace luthier
{

namespace
{
    constexpr int kGround = -1;

    // Node numbering for the passive network (volume-knob-interaction.md 1).
    constexpr int kCoilNode   = 0;   ///< between the coil's resistance and its inductance
    constexpr int kPickupNode = 1;   ///< the pickup output: volume pot input
    constexpr int kWiperNode  = 2;   ///< the volume wiper: cable and amp
    constexpr int kToneNode   = 3;   ///< between the tone pot and the tone cap
    constexpr int kBleedNode  = 4;   ///< between R and C of a series treble bleed

    constexpr double kTopOfBandHz = 22000.0;
}

//==============================================================================
bool CircuitComponents::operator== (const CircuitComponents& o) const noexcept
{
    return hasCoil == o.hasCoil
        && coilInductance == o.coilInductance
        && coilResistance == o.coilResistance
        && coilCapacitance == o.coilCapacitance
        && volume == o.volume && tone == o.tone
        && volumePot == o.volumePot && tonePot == o.tonePot && toneCap == o.toneCap
        && taper == o.taper
        && bleed == o.bleed
        && bleedResistance == o.bleedResistance && bleedCapacitance == o.bleedCapacitance
        && bleedSeries == o.bleedSeries
        && active == o.active
        && cableOn == o.cableOn && cableLength == o.cableLength && cableQuality == o.cableQuality
        && ampInputImpedance == o.ampInputImpedance;
}

//==============================================================================
double GuitarCircuit::taperFraction (double position, PotTaper taper) noexcept
{
    const double p = juce::jlimit (0.0, 1.0, position);

    if (taper == PotTaper::linear)
        return p;

    // Audio (logarithmic) taper: about 10 % of the resistance at half rotation,
    // which is the curve an "A" pot is built to. 50s wiring is a topology, not
    // a taper, and those guitars used audio pots.
    return (std::pow (10.0, 2.0 * p) - 1.0) / 99.0;
}

double GuitarCircuit::cableCapacitancePerMetre (CableQuality quality) noexcept
{
    switch (quality)
    {
        case CableQuality::studio:   return 52.0e-12;
        case CableQuality::cheap:    return 160.0e-12;
        case CableQuality::vintage:  return 220.0e-12;
        case CableQuality::standard:
        case CableQuality::numQualities:
        default:                     return 98.0e-12;
    }
}

double GuitarCircuit::cableCapacitance (const CircuitComponents& p) noexcept
{
    // cable_on off is a zero-length cable, not silence (section 2).
    if (! p.cableOn)
        return 0.0;

    return juce::jmax (0.0, p.cableLength) * cableCapacitancePerMetre (p.cableQuality);
}

double GuitarCircuit::activeToneCornerHz (const CircuitComponents& p) noexcept
{
    /*  1.4: "an active first-order lowpass with the same nominal corner". The
        corner a passive tone control reaches fully rolled off is where the tone
        cap resonates with the coil, so that is the bottom of the sweep; the top
        is out of the audio band. A guitar with no coil is given a typical
        single coil's inductance so the cap still means what it says. */
    const double inductance = p.hasCoil ? juce::jmax (0.01, p.coilInductance) : 2.5;
    const double capacitance = juce::jmax (1.0e-12, p.toneCap + (p.hasCoil ? p.coilCapacitance : 0.0));

    const double bottom = juce::jlimit (50.0, 5000.0,
                                        1.0 / (constants::kTwoPi * std::sqrt (inductance * capacitance)));

    return bottom * std::pow (kTopOfBandHz / bottom, taperFraction (p.tone, p.taper));
}

//==============================================================================
void GuitarCircuit::Netlist::addResistor (int a, int b, double ohms) noexcept
{
    if (numResistors < (int) resistors.size())
        resistors[(size_t) numResistors++] = { a, b, juce::jmax (1.0e-3, ohms) };
}

void GuitarCircuit::Netlist::addReactive (int a, int b, double value, bool inductor) noexcept
{
    if (value > 0.0 && numReactives < (int) reactives.size())
        reactives[(size_t) numReactives++] = { a, b, value, inductor };
}

GuitarCircuit::Netlist GuitarCircuit::buildPassiveNetlist (const CircuitComponents& p) noexcept
{
    Netlist net;

    net.sourceNode = kCoilNode;
    net.sourceResistance = juce::jmax (1.0, p.coilResistance);

    net.addReactive (kCoilNode, kPickupNode, juce::jmax (1.0e-3, p.coilInductance), true);
    net.addReactive (kPickupNode, kGround, p.coilCapacitance, false);

    // ---- volume pot ------------------------------------------------------------
    const double volumePot = juce::jmax (100.0, p.volumePot);
    const double wiper = taperFraction (p.volume, p.taper);
    const double top = volumePot * (1.0 - wiper);
    const double bottom = volumePot * wiper;

    /*  Sections under kShortOhms are wires. Wide open, the wiper is the pickup
        node; fully down, it is ground - the amp input and cable hang off a
        node that no longer carries signal. */
    const bool wiperIsPickup = top < kShortOhms;
    const bool wiperIsGround = ! wiperIsPickup && bottom < kShortOhms;

    const int wiperNode = wiperIsPickup ? kPickupNode : (wiperIsGround ? kGround : kWiperNode);

    if (! wiperIsPickup)
        net.addResistor (kPickupNode, wiperNode, top);

    if (! wiperIsGround && ! wiperIsPickup)
        net.addResistor (kWiperNode, kGround, bottom);

    if (wiperIsPickup)
        net.addResistor (kPickupNode, kGround, volumePot);

    // ---- tone ------------------------------------------------------------------
    // 50s wiring takes the tone control off the wiper instead of the pickup
    // (3.1). Where the wiper is ground, the tone control has nothing to do.
    const int toneTap = (p.taper == PotTaper::fiftiesWiring) ? wiperNode : kPickupNode;

    if (toneTap != kGround)
    {
        const double toneResistance = juce::jmax (0.0, p.tonePot) * taperFraction (p.tone, p.taper);

        if (toneResistance < kShortOhms)
        {
            net.addReactive (toneTap, kGround, p.toneCap, false);
        }
        else
        {
            net.addResistor (toneTap, kToneNode, toneResistance);
            net.addReactive (kToneNode, kGround, p.toneCap, false);
        }
    }

    // ---- treble bleed (1.3) ----------------------------------------------------
    // Across the volume pot's top section, so it only exists while that
    // section does.
    if (! wiperIsPickup && p.bleed != TrebleBleed::none)
    {
        const int to = wiperNode;

        switch (p.bleed)
        {
            case TrebleBleed::kinman:
                net.addResistor (kPickupNode, to, 130.0e3);
                net.addReactive (kPickupNode, to, 1.1e-9, false);
                break;

            case TrebleBleed::fender:
                net.addReactive (kPickupNode, to, 1.0e-9, false);
                break;

            case TrebleBleed::custom:
                if (p.bleedSeries)
                {
                    net.addResistor (kPickupNode, kBleedNode, juce::jmax (1.0, p.bleedResistance));
                    net.addReactive (kBleedNode, to, p.bleedCapacitance, false);
                }
                else
                {
                    net.addResistor (kPickupNode, to, juce::jmax (1.0, p.bleedResistance));
                    net.addReactive (kPickupNode, to, p.bleedCapacitance, false);
                }
                break;

            case TrebleBleed::none:
            case TrebleBleed::numBleeds:
            default:
                break;
        }
    }

    // ---- load ------------------------------------------------------------------
    if (wiperNode != kGround)
    {
        net.addReactive (wiperNode, kGround, cableCapacitance (p), false);
        net.addResistor (wiperNode, kGround, juce::jmax (1.0e3, p.ampInputImpedance));
    }

    net.outputNode = wiperNode;
    return net;
}

GuitarCircuit::Netlist GuitarCircuit::buildActivePickupNetlist (const CircuitComponents& p) noexcept
{
    // The buffer isolates the pickup: it sees its own capacitance and the
    // buffer's input, and nothing downstream (1.4).
    Netlist net;

    net.sourceNode = kCoilNode;
    net.sourceResistance = juce::jmax (1.0, p.coilResistance);

    net.addReactive (kCoilNode, kPickupNode, juce::jmax (1.0e-3, p.coilInductance), true);
    net.addReactive (kPickupNode, kGround, p.coilCapacitance, false);
    net.addResistor (kPickupNode, kGround, kBufferInputImpedance);

    net.outputNode = kPickupNode;
    return net;
}

//==============================================================================
std::complex<double> GuitarCircuit::solveContinuous (const Netlist& net, double hz) noexcept
{
    using C = std::complex<double>;

    if (net.outputNode == kGround)
        return 0.0;

    std::array<std::array<C, kNodes>, kNodes> y {};
    std::array<C, kNodes> rhs {};

    const double w = constants::kTwoPi * juce::jmax (1.0e-3, hz);

    auto stamp = [&y] (int a, int b, C admittance)
    {
        if (a >= 0) y[(size_t) a][(size_t) a] += admittance;
        if (b >= 0) y[(size_t) b][(size_t) b] += admittance;

        if (a >= 0 && b >= 0)
        {
            y[(size_t) a][(size_t) b] -= admittance;
            y[(size_t) b][(size_t) a] -= admittance;
        }
    };

    // A 1 V source through the coil's resistance, as its Norton equivalent.
    stamp (net.sourceNode, kGround, 1.0 / net.sourceResistance);
    rhs[(size_t) net.sourceNode] = 1.0 / net.sourceResistance;

    for (int i = 0; i < net.numResistors; ++i)
        stamp (net.resistors[(size_t) i].a, net.resistors[(size_t) i].b, 1.0 / net.resistors[(size_t) i].ohms);

    for (int i = 0; i < net.numReactives; ++i)
    {
        const auto& r = net.reactives[(size_t) i];
        stamp (r.a, r.b, r.inductor ? C (0.0, -1.0 / (w * r.value)) : C (0.0, w * r.value));
    }

    // Nodes nothing touches are tied down so the matrix stays regular.
    for (int n = 0; n < kNodes; ++n)
        if (std::abs (y[(size_t) n][(size_t) n]) == 0.0)
            y[(size_t) n][(size_t) n] = 1.0;

    // Gaussian elimination with partial pivoting.
    for (int col = 0; col < kNodes; ++col)
    {
        int pivot = col;

        for (int row = col + 1; row < kNodes; ++row)
            if (std::abs (y[(size_t) row][(size_t) col]) > std::abs (y[(size_t) pivot][(size_t) col]))
                pivot = row;

        std::swap (y[(size_t) col], y[(size_t) pivot]);
        std::swap (rhs[(size_t) col], rhs[(size_t) pivot]);

        const C diag = y[(size_t) col][(size_t) col];

        if (std::abs (diag) < 1.0e-300)
            continue;

        for (int row = col + 1; row < kNodes; ++row)
        {
            const C factor = y[(size_t) row][(size_t) col] / diag;

            for (int k = col; k < kNodes; ++k)
                y[(size_t) row][(size_t) k] -= factor * y[(size_t) col][(size_t) k];

            rhs[(size_t) row] -= factor * rhs[(size_t) col];
        }
    }

    std::array<C, kNodes> v {};

    for (int row = kNodes; --row >= 0;)
    {
        C sum = rhs[(size_t) row];

        for (int k = row + 1; k < kNodes; ++k)
            sum -= y[(size_t) row][(size_t) k] * v[(size_t) k];

        const C diag = y[(size_t) row][(size_t) row];
        v[(size_t) row] = std::abs (diag) > 1.0e-300 ? sum / diag : C (0.0);
    }

    return v[(size_t) net.outputNode];
}

std::complex<double> GuitarCircuit::response (const CircuitComponents& p, double hz) noexcept
{
    const bool buffered = p.active || ! p.hasCoil;

    if (! buffered)
        return solveContinuous (buildPassiveNetlist (p), hz);

    std::complex<double> h = p.hasCoil ? solveContinuous (buildActivePickupNetlist (p), hz)
                                       : std::complex<double> (1.0);

    h *= taperFraction (p.volume, p.taper);

    if (p.tone < 0.999)
        h /= std::complex<double> (1.0, hz / activeToneCornerHz (p));

    return h;
}

double GuitarCircuit::magnitudeDb (const CircuitComponents& p, double hz) noexcept
{
    return gainToDb (std::abs (response (p, hz)));
}

double GuitarCircuit::findResonantPeakHz (const CircuitComponents& p) noexcept
{
    double bestHz = 500.0, bestMag = -1.0;

    // Log-spaced, 96 points per octave: fine enough that a 10 % shift in the
    // peak is many steps.
    for (double hz = 500.0; hz <= 20000.0; hz *= std::pow (2.0, 1.0 / 96.0))
    {
        const double mag = std::abs (response (p, hz));

        if (mag > bestMag)
        {
            bestMag = mag;
            bestHz = hz;
        }
    }

    return bestHz;
}

//==============================================================================
void GuitarCircuit::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (8000.0, sampleRate);
    built = false;
    rebuild();
    reset();
}

void GuitarCircuit::reset() noexcept
{
    history.fill (0.0);
    toneState = 0.0;
}

void GuitarCircuit::setComponents (const CircuitComponents& newParts) noexcept
{
    if (built && newParts == parts)
        return;

    parts = newParts;
    rebuild();
}

void GuitarCircuit::rebuild() noexcept
{
    const bool buffered = parts.active || ! parts.hasCoil;

    activePath = buffered;
    passThrough = buffered && ! parts.hasCoil;
    activeGain = buffered ? taperFraction (parts.volume, parts.taper) : 1.0;

    toneBypassed = ! buffered || parts.tone >= 0.999;

    if (! toneBypassed)
    {
        const double fc = juce::jmin (activeToneCornerHz (parts), sr * 0.45);
        const double g = std::tan (constants::kPi * fc / sr);
        toneG = g / (1.0 + g);
    }

    const auto previous = net;

    net = passThrough ? Netlist {}
                      : (buffered ? buildActivePickupNetlist (parts) : buildPassiveNetlist (parts));

    /*  The history currents are physical - the current through each coil and
        capacitor - so they carry over a knob move and the sound does not click.
        They only mean something if the element list is the same one, though;
        when the topology changes (a section shorts, the bleed changes type) the
        network starts from rest. */
    bool sameTopology = built && previous.numReactives == net.numReactives;

    for (int i = 0; sameTopology && i < net.numReactives; ++i)
        sameTopology = previous.reactives[(size_t) i].a == net.reactives[(size_t) i].a
                    && previous.reactives[(size_t) i].b == net.reactives[(size_t) i].b
                    && previous.reactives[(size_t) i].inductor == net.reactives[(size_t) i].inductor;

    if (! sameTopology)
        history.fill (0.0);

    // ---- nodal matrix with trapezoidal companions -------------------------------
    std::array<std::array<double, kNodes>, kNodes> g {};

    auto stamp = [&g] (int a, int b, double conductanceValue)
    {
        if (a >= 0) g[(size_t) a][(size_t) a] += conductanceValue;
        if (b >= 0) g[(size_t) b][(size_t) b] += conductanceValue;

        if (a >= 0 && b >= 0)
        {
            g[(size_t) a][(size_t) b] -= conductanceValue;
            g[(size_t) b][(size_t) a] -= conductanceValue;
        }
    };

    const double period = 1.0 / sr;

    if (! passThrough)
    {
        stamp (net.sourceNode, kGround, 1.0 / net.sourceResistance);

        for (int i = 0; i < net.numResistors; ++i)
            stamp (net.resistors[(size_t) i].a, net.resistors[(size_t) i].b, 1.0 / net.resistors[(size_t) i].ohms);

        for (int i = 0; i < net.numReactives; ++i)
        {
            const auto& r = net.reactives[(size_t) i];
            conductance[(size_t) i] = r.inductor ? period / (2.0 * r.value) : 2.0 * r.value / period;
            stamp (r.a, r.b, conductance[(size_t) i]);
        }
    }

    for (int n = 0; n < kNodes; ++n)
        if (g[(size_t) n][(size_t) n] == 0.0)
            g[(size_t) n][(size_t) n] = 1.0;

    // ---- Gauss-Jordan inverse ---------------------------------------------------
    std::array<std::array<double, kNodes>, kNodes> inv {};

    for (int n = 0; n < kNodes; ++n)
        inv[(size_t) n][(size_t) n] = 1.0;

    for (int col = 0; col < kNodes; ++col)
    {
        int pivot = col;

        for (int row = col + 1; row < kNodes; ++row)
            if (std::abs (g[(size_t) row][(size_t) col]) > std::abs (g[(size_t) pivot][(size_t) col]))
                pivot = row;

        std::swap (g[(size_t) col], g[(size_t) pivot]);
        std::swap (inv[(size_t) col], inv[(size_t) pivot]);

        const double diag = g[(size_t) col][(size_t) col];

        if (std::abs (diag) < 1.0e-300)
            continue;

        for (int k = 0; k < kNodes; ++k)
        {
            g[(size_t) col][(size_t) k] /= diag;
            inv[(size_t) col][(size_t) k] /= diag;
        }

        for (int row = 0; row < kNodes; ++row)
        {
            if (row == col)
                continue;

            const double factor = g[(size_t) row][(size_t) col];

            if (factor == 0.0)
                continue;

            for (int k = 0; k < kNodes; ++k)
            {
                g[(size_t) row][(size_t) k] -= factor * g[(size_t) col][(size_t) k];
                inv[(size_t) row][(size_t) k] -= factor * inv[(size_t) col][(size_t) k];
            }
        }
    }

    inverse = inv;
    built = true;
}

//==============================================================================
double GuitarCircuit::process (double emf) noexcept
{
    double out = emf;

    if (! passThrough)
    {
        if (net.outputNode == kGround)
        {
            out = 0.0;
        }
        else
        {
            std::array<double, kNodes> rhs {};
            rhs[(size_t) net.sourceNode] = emf / net.sourceResistance;

            // Each companion's history current, injected where it flows.
            for (int i = 0; i < net.numReactives; ++i)
            {
                const auto& r = net.reactives[(size_t) i];
                const double j = r.inductor ? -history[(size_t) i] : history[(size_t) i];

                if (r.a >= 0) rhs[(size_t) r.a] += j;
                if (r.b >= 0) rhs[(size_t) r.b] -= j;
            }

            std::array<double, kNodes> v {};

            for (int row = 0; row < kNodes; ++row)
            {
                double sum = 0.0;

                for (int k = 0; k < kNodes; ++k)
                    sum += inverse[(size_t) row][(size_t) k] * rhs[(size_t) k];

                v[(size_t) row] = sum;
            }

            for (int i = 0; i < net.numReactives; ++i)
            {
                const auto& r = net.reactives[(size_t) i];
                const double across = (r.a >= 0 ? v[(size_t) r.a] : 0.0) - (r.b >= 0 ? v[(size_t) r.b] : 0.0);
                const double twoGv = 2.0 * conductance[(size_t) i] * across;

                double& j = history[(size_t) i];
                j = r.inductor ? twoGv + j : twoGv - j;

                if (std::abs (j) < 1.0e-30)
                    j = 0.0;
            }

            out = v[(size_t) net.outputNode];
        }
    }

    if (activePath)
    {
        out *= activeGain;

        if (! toneBypassed)
        {
            const double vHalf = (out - toneState) * toneG;
            const double lp = vHalf + toneState;
            toneState = lp + vHalf;
            out = lp;
        }
    }

    return sanitise (out);
}

} // namespace luthier
