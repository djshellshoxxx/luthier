#pragma once

/*  Physically-informed pluck excitation (engine spec 5.5).

    Not white noise and not a click. A pluck displaces the string into a triangle
    with its apex at the pluck point; releasing it launches that shape into the
    waveguide. The shape is then comb-filtered at twice the pluck length, which
    is what puts the real spectral notch at the pluck position's node, and finally
    shaped by the contact material (pick, nail, flesh, thumb...).

    Velocity raises amplitude *and* brightness, because a harder pluck deforms the
    string into a sharper corner. That is identity rule 5 in the build spec.
*/

#include "../Common/DspCommon.h"
#include <vector>

namespace luthier
{

class Excitation
{
public:
    /** What is touching the string. Each has its own contact spectrum. */
    enum class Material
    {
        PickNylon, PickCelluloid, PickDelrin, PickMetal, PickWood, PickFelt,
        Fingernail, Fingertip, Thumb, Thumbpick, Brush, Slide,
        NumMaterials
    };

    /** How the string is being set in motion. Shapes the envelope, not just the filter. */
    enum class Kind
    {
        Pluck,          ///< Normal note-on.
        HammerOn,       ///< Finger lands on the fret: light, thuddier.
        PullOff,        ///< Finger snaps off sideways: light, slightly brighter.
        Tap,            ///< Two-handed tap: harder than a hammer-on.
        Harmonic,       ///< Node touch: energy restricted to one partial.
        PinchHarmonic,  ///< Thumb grazes the string: one upper partial, loud.
        Scrape,         ///< Pick dragged along a wound string.
        Slap,           ///< Bass slap / percussive thumb.
        NumKinds
    };

    struct Params
    {
        Material material      = Material::PickCelluloid;
        Kind     kind          = Kind::Pluck;
        double   pluckPosition = 0.18;   ///< 0 = at the bridge, 0.5 = at the midpoint.
        double   velocity      = 0.8;    ///< 0 to 1.
        double   pickThickness = 0.5;    ///< 0 thin/bright, 1 heavy/warm.
        double   pickAngle     = 0.35;   ///< 0 parallel/aggressive, 1 angled/soft.
        double   delaySamples  = 400.0;  ///< Current string loop length, sizes the pluck.
        double   brightness    = 0.5;    ///< "Attack" macro, 0 soft to 1 sharp.
        double   noiseAmount   = 0.12;   ///< Contact noise blended into the impulse.
        int      harmonicNumber = 0;     ///< Partial to isolate for Harmonic kinds.
        double   nailVsFlesh   = 0.5;    ///< Fingerstyle only: 0 flesh, 1 nail.
    };

    void prepare (double sampleRate);
    void reset() noexcept;

    /** Renders a new excitation into the internal buffer. Real-time safe: the
        buffer is pre-allocated in prepare() and only ever written in place. */
    void trigger (const Params& p, RtRandom& rng) noexcept;

    /** Returns the next excitation sample, or 0 once the impulse is exhausted. */
    inline double next() noexcept
    {
        if (readPos >= length)
            return 0.0;

        return buffer[(size_t) readPos++];
    }

    inline bool isActive() const noexcept { return readPos < length; }
    inline int  remaining() const noexcept { return juce::jmax (0, length - readPos); }

    /** Peak absolute value of the impulse that was last rendered. Used by the
        validator to confirm an excitation actually carries energy. */
    double lastPeak() const noexcept { return peak; }

private:
    struct MaterialSpec
    {
        double lowpassHz;    ///< Contact bandwidth.
        double peakHz;       ///< Characteristic resonance of the contact.
        double peakDb;       ///< How pronounced that resonance is.
        double noiseScale;   ///< Relative contact-noise content.
        double lengthScale;  ///< Contact duration relative to a celluloid pick.
    };

    static const MaterialSpec& specFor (Material m) noexcept;

    double sr = 44100.0;
    std::vector<double> buffer;
    std::vector<double> scratch;
    int length = 0;
    int readPos = 0;
    double peak = 0.0;

    Biquad shaper, resonator, harmonicBand;
    OnePoleHP dcTrim;
};

} // namespace luthier
