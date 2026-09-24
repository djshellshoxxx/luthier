#pragma once

/*  The Jam kit's room (jam-mode.md 5, "jam_kit_room sends to a 4-line FDN
    room (0.1-0.6 s)"). Four delay lines mixed by a Hadamard matrix, each loop
    damped by a one-pole lowpass and scaled for the room's RT60. The lines are
    sized in prepare for the rate in use, so nothing allocates afterwards.
*/

#include "../Common/DspCommon.h"
#include <array>
#include <vector>

namespace luthier
{

class KitRoom
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    /** RT60 in seconds, clamped to 0.1-0.6 s. */
    void setDecay (double rt60Seconds) noexcept;

    /** Mono in, stereo out (adds to left / right). */
    inline void process (double in, double& left, double& right) noexcept
    {
        std::array<double, 4> out {};

        for (int i = 0; i < 4; ++i)
        {
            auto& line = lines[(size_t) i];
            out[(size_t) i] = line[(size_t) readIndex (i)];
        }

        // Hadamard (4x4, normalised): lossless mixing.
        const double h0 = 0.5 * ( out[0] + out[1] + out[2] + out[3]);
        const double h1 = 0.5 * ( out[0] - out[1] + out[2] - out[3]);
        const double h2 = 0.5 * ( out[0] + out[1] - out[2] - out[3]);
        const double h3 = 0.5 * ( out[0] - out[1] - out[2] + out[3]);
        const double mixed[4] = { h0, h1, h2, h3 };

        for (int i = 0; i < 4; ++i)
        {
            const double fed = damp[(size_t) i].process (mixed[i] * gain[(size_t) i] + in * 0.5);
            lines[(size_t) i][(size_t) writePos[(size_t) i]] = sanitise (fed);
            writePos[(size_t) i] = (writePos[(size_t) i] + 1) % lengths[(size_t) i];
        }

        left  += dcLeft.process (out[0] + out[2]) * 0.5;
        right += dcRight.process (out[1] + out[3]) * 0.5;
    }

private:
    int readIndex (int i) const noexcept { return writePos[(size_t) i]; }

    double sr = 48000.0;
    std::array<std::vector<double>, 4> lines;
    std::array<int, 4> lengths { { 1, 1, 1, 1 } }, writePos {};
    std::array<double, 4> gain {};
    std::array<OnePoleLP, 4> damp;
    DCBlocker dcLeft, dcRight;
};

} // namespace luthier
