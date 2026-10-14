#pragma once

/*  Reference pedals: the contract every circuit model implements.

    spec/reference-pedalboard/ARCHITECTURE.md. A reference pedal is an
    independent processor, not a PedalType: it lives in its own namespace and
    board, so the classic racks (EffectsChain / PedalRack) and their saved
    state are untouched.

    A model is two things:
      - a ModelDescriptor: immutable data (id, controls, face), built once;
      - a ReferencePedalProcessor: the DSP, created on the message thread and
        handed to the audio thread by the board.

    Controls are normalised 0-1 at the interface. A ControlSpec maps that to the
    position of the real pot (taper) and the model maps the pot position to
    circuit values. Nothing on the audio thread allocates, locks or logs.
*/

#include <cstdint>
#include <memory>

namespace luthier::refpedals
{

//==============================================================================
constexpr int kMaxControls = 8;

enum class Placement { Pre, Loop };

enum class Evidence : uint8_t
{
    A,   ///< full schematic analysis, topology-specific model
    B    ///< documented behaviour, hidden circuitry: behavioural model
};

enum class Fidelity : uint8_t
{
    CircuitDerivedPrototype,   ///< circuit known, no hardware capture yet
    HardwareMatched            ///< passed the capture grid (none yet)
};

enum class ControlKind : uint8_t { Knob, Toggle, Selector };

enum class Taper : uint8_t
{
    Linear,       ///< B taper
    Audio,        ///< A taper, 10% at mid travel
    ReverseAudio  ///< C taper
};

struct ControlSpec
{
    const char* id = "";       ///< stable key, saved in state: never rename
    const char* label = "";    ///< legend printed on the face, upper case
    ControlKind kind = ControlKind::Knob;
    Taper taper = Taper::Linear;
    double defaultNormalised = 0.5;
    int numSteps = 0;                          ///< Toggle = 2, Selector >= 2
    const char* const* stepLabels = nullptr;   ///< Selector / Toggle captions
    // Face placement, in 0-1 units of the enclosure. x,y = knob centre.
    float x = 0.5f, y = 0.5f, size = 0.28f;
};

/** Pot position (0-1) for a normalised control value. Audio (A) taper is the
    usual two-segment approximation: 10% of the resistance at half travel. */
inline double potPosition (Taper t, double n) noexcept
{
    n = n < 0.0 ? 0.0 : (n > 1.0 ? 1.0 : n);
    const auto audio = [] (double x) { return x <= 0.5 ? 0.2 * x : 0.1 + 1.8 * (x - 0.5); };

    switch (t)
    {
        case Taper::Linear:       return n;
        case Taper::Audio:        return audio (n);
        case Taper::ReverseAudio: return 1.0 - audio (1.0 - n);
    }

    return n;
}

//==============================================================================
enum class Finish : uint8_t { Gloss, Matte, Hammertone, Brushed };
enum class CapStyle : uint8_t { Chicken, Pointer, SkirtedBlack, SkirtedChrome, Flat };

struct FaceSpec
{
    uint32_t enclosure = 0xff808080;   ///< ARGB
    uint32_t panelInk = 0xffffffff;    ///< legend colour
    uint32_t knobBody = 0xff202020;
    uint32_t knobMark = 0xffffffff;
    uint32_t led = 0xffff2020;
    Finish finish = Finish::Gloss;
    CapStyle cap = CapStyle::SkirtedBlack;
    float aspect = 1.9f;               ///< height / width of the enclosure
    float nameY = 0.80f;               ///< baseline of the printed name, 0-1
    const char* tagline = "";          ///< secondary legend under the name
    bool stripe = false;               ///< decorative band (some pedals have one)
    uint32_t stripeColour = 0;
};

//==============================================================================
struct ModelDescriptor
{
    const char* id = "";            ///< immutable, saved in state
    const char* displayName = "";   ///< generic shipped name (no trademarks)
    const char* family = "";        ///< Overdrive, Fuzz, Phaser, ...
    const char* summary = "";       ///< one-sentence description of the circuit
    Evidence evidence = Evidence::A;
    Fidelity fidelity = Fidelity::CircuitDerivedPrototype;
    Placement defaultPlacement = Placement::Pre;
    bool buffered = false;          ///< buffered vs true bypass (documented per model)
    bool stereoOut = false;
    int numControls = 0;
    ControlSpec controls[kMaxControls];
    FaceSpec face;
};

//==============================================================================
class ReferencePedalProcessor
{
public:
    virtual ~ReferencePedalProcessor() = default;

    virtual const ModelDescriptor& descriptor() const noexcept = 0;

    /** Allocates everything. Message thread. */
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    virtual void reset() noexcept = 0;

    /** Normalised 0-1, smoothed internally. Safe to call between blocks. */
    virtual void setControl (int index, double normalised) noexcept = 0;

    /** In place, both channels always valid; a mono pedal writes L to R. */
    virtual void processStereo (double* left, double* right, int numSamples) noexcept = 0;

    virtual int getLatencySamples() const noexcept { return 0; }
};

} // namespace luthier::refpedals
