// OWNER: E67 (explicit transfer from coordinator)
#include "TestFramework.h"
#include "../ReferencePedals/PedalRegistry.h"
#include <limits>
using namespace luthier::reference;
namespace
{
ParamDesc discrete(float low,float high,float initial,int steps)
{return {"test_discrete_value","Value","",ParamKind::Discrete,Taper::Linear,low,high,initial,1,steps,nullptr,PF_Automatable,0,-1};}
PedalDesc descriptor(const ParamDesc& p)
{return {"test.discrete","test_discrete_",1,"Discrete","Test",nullptr,ChannelMode::Stereo,BypassMode::True,1,&p,0,false,false,false,1};}
}
LUTHIER_TEST(RefPedalDiscrete, naturalRangesDefaultsAndAllStepsRoundTrip)
{
    for(const auto count:{8,99})
    {
        const auto p=discrete(1,float(count),1,count);CHECK(PedalRegistry::validate(descriptor(p)));CHECK(p.defaultNormalised()==0);CHECK(p.fromNormalised(0)==1);CHECK(p.fromNormalised(1)==count);
        for(int natural=1;natural<=count;++natural){const auto n=p.toNormalised(natural);CHECK_NEAR(n,double(natural-1)/(count-1),1e-15);CHECK_NEAR(p.fromNormalised(n),natural,1e-12);}
        for(int sample=0;sample<=200;++sample){const double n=double(sample)/200;const auto value=p.fromNormalised(n);CHECK(value>=1 && value<=count);CHECK_NEAR(p.toNormalised(value),std::round(n*(count-1))/(count-1),1e-15);}
    }
    const auto bipolar=discrete(-12,12,0,5);CHECK(PedalRegistry::validate(descriptor(bipolar)));CHECK(bipolar.fromNormalised(.25)==-6);CHECK(bipolar.toNormalised(6)==.75);CHECK(bipolar.defaultNormalised()==.5);
}
LUTHIER_TEST(RefPedalDiscrete, hostileValuesClampAndInvalidDescriptorsFail)
{
    auto p=discrete(1,8,1,8);CHECK(p.fromNormalised(-1)==1);CHECK(p.fromNormalised(2)==8);CHECK(p.toNormalised(-100)==0);CHECK(p.toNormalised(100)==1);
    for(double bad:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}){CHECK(p.fromNormalised(bad)==1);CHECK(p.toNormalised(bad)==0);}
    for(auto invalid:{discrete(1,1,1,8),discrete(8,1,1,8),discrete(1,8,1,1),discrete(1,8,1.5f,8),discrete(1,std::numeric_limits<float>::infinity(),1,8),discrete(1,8,std::numeric_limits<float>::quiet_NaN(),8)})CHECK(!PedalRegistry::validate(descriptor(invalid)));
    p.min=std::numeric_limits<float>::quiet_NaN();CHECK(p.fromNormalised(.5)==0);CHECK(p.toNormalised(3)==0);
}
LUTHIER_TEST(RefPedalDiscrete, enumAndContinuousSemanticsStayUnchanged)
{
    auto p=discrete(0,3,1,4);p.kind=ParamKind::Enum;const char* const choices[]{"A","B","C","D"};p.choices=choices;
    CHECK(PedalRegistry::validate(descriptor(p)));CHECK(p.fromNormalised(0)==0);CHECK(p.fromNormalised(1)==3);CHECK(p.fromNormalised(.34)==1);CHECK_NEAR(p.toNormalised(1.5),.5,0);CHECK_NEAR(p.defaultNormalised(),1./3.,1e-7);
    p.kind=ParamKind::Continuous;p.min=100;p.max=10000;p.def=1000;p.taper=Taper::LogA;p.steps=0;
    CHECK_NEAR(p.fromNormalised(.5),1000,1e-12);CHECK_NEAR(p.defaultNormalised(),.5,1e-7);
    p.min=1;p.max=8;p.def=1;p.taper=Taper::Linear;CHECK_NEAR(p.fromNormalised(.123),1+7*.123,1e-15);CHECK(p.defaultNormalised()==0);
}
