#include "TestFramework.h"
#include "../ReferencePedals/Common/MildTanh.h"
#include <limits>
LUTHIER_TEST(RefPedalMildTanh, narrowPolynomialPreservesTanhCurve)
{
    using luthier::reference::common::mildTanh;
    double maximum=0;bool odd=true;
    for(int i=0;i<=1000000;++i)
    {
        const double x=.25*i/1000000.;maximum=std::max(maximum,std::abs(mildTanh(x)-std::tanh(x)));
        odd=odd && mildTanh(-x)==-mildTanh(x);
    }
    CHECK(maximum<1.4e-12);CHECK(odd);
    for(double x:{-.2500000001,-.25,-.2499999999,-0.,0.,.2499999999,.25,.2500000001,-1e300,1e300})
        CHECK_NEAR(mildTanh(x),std::tanh(x),1.4e-12);
    CHECK(mildTanh(std::numeric_limits<double>::infinity())==0);
    CHECK(mildTanh(std::numeric_limits<double>::quiet_NaN())==0);
}
