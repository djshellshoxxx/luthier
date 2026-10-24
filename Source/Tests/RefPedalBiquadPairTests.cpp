#include "TestFramework.h"
#include "../ReferencePedals/Common/Biquad.h"
#include <cstring>
#include <limits>
using luthier::reference::common::Biquad;
namespace
{
bool sameBits(double a,double b){return std::memcmp(&a,&b,sizeof(double))==0;}
void configure(Biquad& filter,double sr,int mode,double amount)
{
    switch(mode%4)
    {
        case 0:filter.setLowpass(sr,20+amount*14000,.1+amount*5);break;
        case 1:filter.setHighpass(sr,20+amount*14000,.1+amount*5);break;
        case 2:filter.setShelf(sr,100+amount*8000,-20+amount*40,mode%8>=4,.05+amount*.95);break;
        case 3:filter.setPeak(sr,100+amount*8000,-20+amount*40,.1+amount*5);break;
    }
}
}
LUTHIER_TEST(RefPedalBiquadPair, scalarBitOracleThroughCoefficientsResetsAndHostileSamples)
{
    const double hostile[]{0.,-0.,1e-31,-1e-31,1e-30,-1e-30,1e-29,-1e-29,
        std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),std::numeric_limits<double>::denorm_min()};
    Biquad a,b,refA,refB;
    uint32_t rng=73;auto draw=[&](){rng=1664525u*rng+1013904223u;return rng;};
    for(double sr:{8000.,44100.,48000.,88200.,96000.,192000.,384000.})
    {
        for(auto* f:{&a,&b,&refA,&refB})f->reset();
        for(int i=0;i<65536;++i)
        {
            if(i%97==0)
            {
                const int mode=int(draw()%8);const double amount=double(draw()>>8)/16777215;
                configure(a,sr,mode,amount);configure(refA,sr,mode,amount);
                configure(b,sr,mode+1,1-amount);configure(refB,sr,mode+1,1-amount);
            }
            if(i%4093==0)for(auto* f:{&a,&b,&refA,&refB})f->reset();
            const double x=i%23==0?hostile[(i/23)%std::size(hostile)]:double(int32_t(draw()))/2147483648.;
            const double y=i%19==0?hostile[(i/19)%std::size(hostile)]:double(int32_t(draw()))/2147483648.;
            const auto pair=Biquad::processPair(a,b,x,y);
            CHECK(sameBits(pair[0],refA.process(x)));CHECK(sameBits(pair[1],refB.process(y)));
        }
    }
}
LUTHIER_TEST(RefPedalBiquadPair, aliasedFiltersUseSequentialScalarState)
{
    Biquad a,reference;a.setLowpass(48000,3300);reference=a;
    for(int i=0;i<8192;++i)
    {
        const double x=std::sin(i*.17),y=std::cos(i*.11);
        const auto pair=Biquad::processPair(a,a,x,y);
        CHECK(sameBits(pair[0],reference.process(x)));CHECK(sameBits(pair[1],reference.process(y)));
    }
}
