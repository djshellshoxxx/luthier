#include "TestFramework.h"
#include "../ReferencePedals/Common/FractionalDelay.h"
#include <array>
#include <limits>
using luthier::reference::common::FractionalDelay;
LUTHIER_TEST(RefPedalBandlimitedDelay, preservesOriginalChorusPassbandGate)
{
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,96000.,192000.})for(double phase:{0.,.25,.5,.75,.999})
    {
        FractionalDelay line;line.prepare(128);std::array<double,64> impulse{};
        for(int i=0;i<64;++i){line.push(i==0?1:0);impulse[i]=line.readBandlimited(40+phase);}
        for(double hz:{20.,100.,1000.,5000.,10000.,14000.})
        {
            double real=0,imaginary=0;
            for(int i=0;i<64;++i){real+=impulse[i]*std::cos(2*pi*hz*i/sr);imaginary-=impulse[i]*std::sin(2*pi*hz*i/sr);}
            CHECK(std::abs(20*std::log10(std::hypot(real,imaginary)))<=.5);
        }
        if(phase==0){CHECK_NEAR(impulse[40],1,1e-14);}
        line.reset();for(int i=0;i<200;++i)line.push(1);
        CHECK_NEAR(line.readBandlimited(40+phase),1,1e-14);
    }
}
LUTHIER_TEST(RefPedalBandlimitedDelay, boundaryContinuityAndInvalidDelay)
{
    FractionalDelay line;line.prepare(128);
    for(int i=0;i<300;++i)line.push(std::sin(i*.37));
    CHECK_NEAR(line.readBandlimited(40.999999),line.readBandlimited(41.000001),1e-6);
    CHECK(std::isfinite(line.readBandlimited(std::numeric_limits<double>::quiet_NaN())));
    CHECK(std::isfinite(line.readBandlimited(std::numeric_limits<double>::infinity())));
    FractionalDelay shortLine;shortLine.prepare(2);shortLine.push(1);shortLine.push(0);
    CHECK_NEAR(shortLine.readBandlimited(1),1,1e-14);
}

LUTHIER_TEST(RefPedalBandlimitedDelay, boundedWrapMatchesIntegerHistory)
{
    for(int size:{1,2,7,8,9,16,100,1024})
    {
        luthier::reference::common::FractionalDelay delay;delay.prepare(size);
        for(int i=0;i<3*(size+16);++i)
        {
            delay.push(i+1);
            for(int age:{1,std::max(1,size/2),size})
            {
                const double expected=i>=age?i-age+1:0;
                CHECK_NEAR(delay.read(age),expected,1e-12);
                if(size>=8 && age>=8)CHECK_NEAR(delay.readBandlimited(age),expected,1e-9);
            }
        }
    }
}

LUTHIER_TEST(RefPedalBandlimitedDelay, constantTimeResetMatchesZeroFilledOracle)
{
    using luthier::reference::common::FractionalDelay;
    for(int capacity:{1,7,8,127,4096})
    {
        FractionalDelay reset,zeroFilled;reset.prepare(capacity);zeroFilled.prepare(capacity);
        for(int round=0;round<3;++round)
        {
            reset.reset();
            // A fully overwritten zero history is the old physical-clear oracle.
            for(int i=0;i<capacity+16;++i)zeroFilled.push(0);
            for(int i=0;i<2*(capacity+16)+37;++i)
            {
                for(double age:{1.25,capacity*.5,capacity-.125})
                {
                    CHECK(reset.read(age)==zeroFilled.read(age));
                    CHECK(reset.readBandlimited(age)==zeroFilled.readBandlimited(age));
                }
                const double x=.25*std::sin((i+round*31)*.19);reset.push(x);zeroFilled.push(x);
            }
        }
    }
}
