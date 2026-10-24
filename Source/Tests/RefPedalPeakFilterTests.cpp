// OWNER: WP7
#include "../ReferencePedals/Common/Biquad.h"
#include "TestFramework.h"
using namespace luthier::reference::common;
LUTHIER_TEST(RefPedalPeakFilter, centreGainAllRatesAndQ)
{
    for (double sr : {44100., 48000., 96000., 192000.})
        for (double hz : {310., 1245., 5000.})
            for (double db : {-10., 0., 10.})
                for (double q : {.7, 2.5})
                {
                    Biquad f;
                    f.setPeak(sr, hz, db, q);
                    double re = 0, im = 0;
                    for (int i = 0; i < (int)(2 * sr); ++i)
                    {
                        const double phase = 2 * pi * hz * i / sr, y = f.process(std::cos(phase));
                        if (i >= (int)sr)
                        {
                            re += y * std::cos(phase);
                            im -= y * std::sin(phase);
                        }
                    }
                    CHECK_NEAR(20 * std::log10(std::hypot(re, im) * 2 / sr), db, 1e-8);
                }
}
