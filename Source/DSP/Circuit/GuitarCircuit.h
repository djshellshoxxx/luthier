#pragma once

/*  The guitar's passive circuit, solved as one network (volume-knob-interaction.md).

    Pickup coil, volume pot, tone pot and cap, treble bleed, cable and amp input
    are one linear circuit, and what makes a guitar go dull as the volume comes
    down is that moving the wiper moves the whole network's resonance. So this
    does not chain filters. It builds the nodal equations of the network and
    solves them.

    Two views of the same netlist:

    - **Discrete**, for the audio thread. Every capacitor and the coil are
      replaced by their trapezoidal companion models - a conductance and a
      history current - which is exactly the bilinear transform of the whole
      network's H(s), without ever writing H(s) down. The nodal matrix only
      changes when a component or a knob does, so its inverse is computed then
      and each sample is one small matrix-vector product plus the history
      updates. Fixed-size arrays throughout: nothing here allocates.

    - **Continuous**, for the visualiser and the tests: the same matrix with
      complex admittances, solved at a frequency. This is the exact H(jw) the
      spec's tests are written against.

    CableSim is removed rather than deprecated (CLAUDE_CODE_BRIEF.md conflict
    list 9). Its two parameters, cable_on and cable_length, now drive this.
*/

#include "../Common/DspCommon.h"
#include <array>
#include <complex>

namespace luthier
{

//==============================================================================
enum class PotTaper { audio = 0, linear, fiftiesWiring, numTapers };
enum class TrebleBleed { none = 0, kinman, fender, custom, numBleeds };
enum class CableQuality { studio = 0, standard, cheap, vintage, numQualities };

/** Everything the network is made of, in SI units. */
struct CircuitComponents
{
    // ---- pickup (from the selected pickup parts) -----------------------------
    /** False for a piezo or a mic: nothing inductive to load, so the circuit is
        a buffered attenuator and tone filter only. */
    bool   hasCoil = true;
    double coilInductance  = 2.5;       ///< H
    double coilResistance  = 6000.0;    ///< ohm
    double coilCapacitance = 200.0e-12; ///< F

    // ---- controls ------------------------------------------------------------
    double volume = 1.0;                ///< wiper position 0..1
    double tone   = 1.0;                ///< wiper position 0..1

    double volumePot = 500.0e3;         ///< ohm
    double tonePot   = 500.0e3;         ///< ohm
    double toneCap   = 22.0e-9;         ///< F
    PotTaper taper   = PotTaper::audio;

    TrebleBleed bleed = TrebleBleed::none;
    double bleedResistance  = 130.0e3;  ///< ohm, custom only
    double bleedCapacitance = 1.1e-9;   ///< F, custom only
    bool   bleedSeries = false;         ///< custom only

    bool active = false;

    // ---- load ----------------------------------------------------------------
    bool   cableOn = true;
    double cableLength = 3.0;           ///< m
    CableQuality cableQuality = CableQuality::standard;
    double ampInputImpedance = 1.0e6;   ///< ohm

    bool operator== (const CircuitComponents& other) const noexcept;
    bool operator!= (const CircuitComponents& other) const noexcept { return ! (*this == other); }
};

//==============================================================================
class GuitarCircuit
{
public:
    static constexpr int kNodes = 5;
    static constexpr int kMaxReactive = 6;

    /** Input impedance of an active guitar's buffer (1.4). */
    static constexpr double kBufferInputImpedance = 10.0e6;

    //==========================================================================
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /*  Takes a new set of components and rebuilds the discrete network if
        anything changed. No allocation: callable from the audio thread between
        blocks, which is where the parameter bridge runs. */
    void setComponents (const CircuitComponents& components) noexcept;
    const CircuitComponents& getComponents() const noexcept { return parts; }

    /** One sample of pickup EMF in, one sample at the amp input out. */
    double process (double emf) noexcept;

    //==========================================================================
    /** The exact continuous response from pickup EMF to amp input. */
    static std::complex<double> response (const CircuitComponents& parts, double hz) noexcept;

    /** |H| in dB at `hz`. */
    static double magnitudeDb (const CircuitComponents& parts, double hz) noexcept;

    /** The frequency of the largest peak of |H| between 500 Hz and 20 kHz, for
        the visualiser's marker and the tests. */
    static double findResonantPeakHz (const CircuitComponents& parts) noexcept;

    //==========================================================================
    /** The wiper's resistance fraction for a knob position under a taper. */
    static double taperFraction (double position, PotTaper taper) noexcept;

    /** Cable capacitance per metre for a quality, in F. */
    static double cableCapacitancePerMetre (CableQuality quality) noexcept;

    /** Total cable capacitance for these components, zero when the cable is off. */
    static double cableCapacitance (const CircuitComponents& parts) noexcept;

    /** The active tone control's corner at a knob position (1.4). Above the
        audio band at 1.0, where the control is out of circuit. */
    static double activeToneCornerHz (const CircuitComponents& parts) noexcept;

    /*  Below this a pot's section is treated as a wire, and a node pair it
        joins as one node. A 1-ohm section between two capacitive nodes is a
        time constant far shorter than a sample, which the trapezoidal rule maps
        to a pole next to z = -1: stable, but it rings at Nyquist and barely
        decays. 1 k in series with 500 k-scale impedances moves nothing
        audible. */
    static constexpr double kShortOhms = 1000.0;

private:
    //==========================================================================
    /*  The netlist, independent of how it is solved. Node -1 is ground. */
    struct Netlist
    {
        struct Resistor { int a, b; double ohms; };
        struct Reactive { int a, b; double value; bool inductor; };

        std::array<Resistor, 8> resistors {};
        std::array<Reactive, kMaxReactive> reactives {};
        int numResistors = 0, numReactives = 0;

        int sourceNode = 0;
        double sourceResistance = 1.0;   ///< the Norton source's conductance is 1/this
        int outputNode = 0;

        void addResistor (int a, int b, double ohms) noexcept;
        void addReactive (int a, int b, double value, bool inductor) noexcept;
    };

    static Netlist buildPassiveNetlist (const CircuitComponents& parts) noexcept;
    static Netlist buildActivePickupNetlist (const CircuitComponents& parts) noexcept;

    static std::complex<double> solveContinuous (const Netlist& net, double hz) noexcept;

    void rebuild() noexcept;

    //==========================================================================
    double sr = 48000.0;
    CircuitComponents parts;
    bool built = false;

    Netlist net;
    std::array<std::array<double, kNodes>, kNodes> inverse {};
    std::array<double, kMaxReactive> conductance {};
    std::array<double, kMaxReactive> history {};

    // Active mode: level after the buffer and the tone control's one-pole.
    bool activePath = false;
    bool passThrough = false;
    double activeGain = 1.0;
    double toneG = 1.0, toneState = 0.0;
    bool toneBypassed = true;
};

} // namespace luthier
