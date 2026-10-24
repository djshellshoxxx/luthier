#include "TestFramework.h"
#include "../ReferencePedals/Common/LowLatencyOversampler.h"
#include <cstring>
#include <limits>
using namespace luthier::reference::common;
namespace
{
bool identical(double a,double b){return std::memcmp(&a,&b,sizeof(double))==0;}
struct FullTap
{
    std::array<double,33> taps{};std::array<double,64> up{},down{};unsigned u=0,d=0;
    void prepare(int midpoint)
    {
        const int parity=midpoint%2?0:1;double sum=0;
        for(int k=parity;k<midpoint;k+=2)
        {const double m=k-midpoint,w=.42-.5*std::cos(pi*k/midpoint)+.08*std::cos(2*pi*k/midpoint);taps[k]=taps[2*midpoint-k]=std::sin(.5*pi*m)/(pi*m)*w;sum+=2*taps[k];}
        for(auto& t:taps)t*=.5/sum;taps[midpoint]=.5;
    }
    double filter(std::array<double,64>& history,unsigned& head,double x)
    {history[head]=x;double y=0;for(unsigned k=0;k<taps.size();++k)y+=taps[k]*history[(head-k)&63];head=(head+1)&63;return y;}
    void interpolate(double x,double& a,double& b){a=2*filter(up,u,x);b=2*filter(up,u,0);}
    double decimate(double a,double b){const double y=filter(down,d,a);filter(down,d,b);return y;}
};
struct FullOracle
{
    FullTap first,second;int factor;
    explicit FullOracle(int f):factor(f){first.prepare(15);second.prepare(16);}
    double process(double x)
    {
        if(factor==1)return std::tanh(x);double a,b;first.interpolate(x,a,b);
        if(factor==2){a=std::tanh(a);b=std::tanh(b);}
        else{double c[4];second.interpolate(a,c[0],c[1]);second.interpolate(b,c[2],c[3]);for(auto& v:c)v=std::tanh(v);a=second.decimate(c[0],c[1]);b=second.decimate(c[2],c[3]);}
        return first.decimate(a,b);
    }
};
}
LUTHIER_TEST(RefPedalLowLatencyPair, independentFullConvolutionAndScalarBitOracle)
{
    for(int factor:{1,2,4})for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        LowLatencyOversampler a,b,refA,refB;for(auto* os:{&a,&b,&refA,&refB})os->prepareFactor(sr,factor);
        FullOracle oracleA(factor),oracleB(factor);
        for(int i=0;i<8192;++i)
        {
            const double x=.7*std::sin(i*.931)+.2*std::cos(i*.071),y=.33*std::cos(i*.319)-.19*std::sin(i*.173);
            const auto pair=LowLatencyOversampler::processPair(a,b,x,y,[](double v){return std::tanh(v);},[](double v){return std::tanh(v);});
            CHECK(identical(pair[0],refA.process(x,[](double v){return std::tanh(v);})));
            CHECK(identical(pair[1],refB.process(y,[](double v){return std::tanh(v);})));
            CHECK_NEAR(pair[0],oracleA.process(x),1e-14);CHECK_NEAR(pair[1],oracleB.process(y),1e-14);
        }
    }
}
LUTHIER_TEST(RefPedalLowLatencyPair, callbacksHostileInputResetReprepareAndIndependentRingPositions)
{
    LowLatencyOversampler a,b,refA,refB;int counter=0,referenceCounter=0;
    auto shape=[&](double v){const int tick=++counter;return tick%101==0?std::numeric_limits<double>::quiet_NaN():tick%37==0?std::numeric_limits<double>::infinity():std::tanh(v)+.00001*(tick%7);};
    auto referenceShape=[&](double v){const int tick=++referenceCounter;return tick%101==0?std::numeric_limits<double>::quiet_NaN():tick%37==0?std::numeric_limits<double>::infinity():std::tanh(v)+.00001*(tick%7);};
    const double hostile[]{0.,-0.,1e-31,-1e-31,1e-30,-1e-30,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),1e300,-1e300};
    for(int factor:{1,2,4})for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        for(auto* os:{&a,&b,&refA,&refB})os->prepareFactor(sr,factor);
        for(int i=0;i<8192;++i)
        {
            if(i==2000){a.reset();refA.reset();}
            if(i==3000){b.prepareFactor(sr,factor==2?4:2);refB.prepareFactor(sr,factor==2?4:2);}
            if(i==4000){b.prepareFactor(sr,factor);refB.prepareFactor(sr,factor);}
            if(i%173==0){CHECK(identical(a.process(.17,shape),refA.process(.17,referenceShape)));}
            const double x=i%19==0?hostile[(i/19)%std::size(hostile)]:.9*std::sin(i*.111),y=i%23==0?hostile[(i/23)%std::size(hostile)]:-.3*std::cos(i*.173);
            const auto pair=LowLatencyOversampler::processPair(a,b,x,y,shape,shape);
            const double left=refA.process(x,referenceShape),right=refB.process(y,referenceShape);
            CHECK(identical(pair[0],left));CHECK(identical(pair[1],right));CHECK(counter==referenceCounter);
        }
    }
}
LUTHIER_TEST(RefPedalLowLatencyPair, sameObjectUsesSequentialState)
{
    for(int factor:{1,2,4})
    {
        LowLatencyOversampler a,reference;a.prepareFactor(48000,factor);reference.prepareFactor(48000,factor);
        for(int i=0;i<8192;++i)
        {
            const double x=.7*std::sin(i*.731),y=-.2*std::cos(i*.131);auto shape=[](double v){return std::tanh(v);};
            const auto pair=LowLatencyOversampler::processPair(a,a,x,y,shape,shape);
            const double first=reference.process(x,shape),second=reference.process(y,shape);
            CHECK(identical(pair[0],first));CHECK(identical(pair[1],second));
        }
    }
}
