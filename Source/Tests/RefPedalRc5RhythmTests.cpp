// OWNER: WP16/WP24
#include "TestFramework.h"
#include "../ReferencePedals/Common/Rc5Rhythm.h"
#include <array>
#include <limits>
#include <random>
#include <set>
using namespace luthier::reference::common;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST(RefPedalRc5Rhythm, authoredTablesHaveEveryPatternAndVariation)
{
    std::set<std::array<uint16_t,8>> distinct;
    for(int p=1;p<=57;++p)for(int v=0;v<2;++v)
    {
        const auto& grid=Rc5Rhythm::grid(p,v); uint16_t all=0;
        for(auto mask:grid)all|=mask;
        CHECK(all!=0); CHECK(distinct.insert(grid).second);
        CHECK(Rc5Rhythm::name(p)!=nullptr); CHECK(Rc5Rhythm::bars(p)==2||Rc5Rhythm::bars(p)==4);
    }
    CHECK(distinct.size()==114);
}
LUTHIER_TEST(RefPedalRc5Rhythm, ratesPartitionsAndKitsAreDeterministic)
{
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(int kit=1;kit<=7;++kit)
    {
        Rc5Rhythm reference;reference.prepare(sr,0x51a7);reference.select(kit*7,1,kit);reference.setRunning(true);
        std::array<double,4097> expected{},right{};reference.process(expected.data(),right.data(),4097);
        double energy=0;for(double x:expected)energy+=x*x;CHECK(energy>1e-9);
        for(int block:{1,16,37,64,481,2048})
        {
            Rc5Rhythm actual;actual.prepare(sr,0x51a7);actual.select(kit*7,1,kit);actual.setRunning(true);
            std::array<double,4097> left{},r{};auto allocations=luthier::tests::allocationsOnThisThread();
            for(int offset=0;offset<4097;offset+=block)actual.process(left.data()+offset,r.data()+offset,std::min(block,4097-offset));
            CHECK(luthier::tests::allocationsOnThisThread()==allocations);
            bool equal=true;for(int i=0;i<4097;++i)equal&=left[i]==expected[i]&&r[i]==right[i];CHECK(equal);
        }
    }
}
LUTHIER_TEST(RefPedalRc5Rhythm, selectionWaitsForBarAndTempoRetainsPhase)
{
    Rc5Rhythm rhythm;rhythm.prepare(48000,42);rhythm.setTempo(120);rhythm.select(1,0,1);rhythm.setRunning(true);
    std::array<double,12000> l{},r{};rhythm.process(l.data(),r.data(),12000);
    rhythm.select(57,1,7);CHECK(rhythm.currentPattern()==1);
    const double before=rhythm.stepPosition();rhythm.setTempo(240);CHECK(rhythm.stepPosition()==before);
    for(int i=0;i<3;++i){l.fill(0);r.fill(0);rhythm.process(l.data(),r.data(),12000);}
    CHECK(rhythm.currentPattern()==1);
    l.fill(0);r.fill(0);rhythm.process(l.data(),r.data(),12000);CHECK(rhythm.currentPattern()==57);CHECK(rhythm.currentKit()==7);
    rhythm.setRunning(false);rhythm.select(2,0,2);CHECK(rhythm.currentPattern()==2);
}
LUTHIER_TEST(RefPedalRc5Rhythm, randomProcessingAndHostileControlsRemainBounded)
{
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        Rc5Rhythm rhythm;rhythm.prepare(sr,123);std::mt19937 random(0x7257);
        std::array<double,2048> l{},r{};
        for(int b=0;b<2000;++b)
        {
            if(b%13==0){rhythm.select((int)(random()%80)-10,(int)(random()%5)-1,(int)(random()%12)-2);rhythm.setRunning((random()%8)!=0);}
            if(b%17==0){rhythm.setTempo((double)(random()%600)-100);rhythm.setLevel((double)(random()%200)-50);}
            if(b%101==0){rhythm.setTempo(std::numeric_limits<double>::quiet_NaN());rhythm.setLevel(std::numeric_limits<double>::infinity());}
            int n=1+(int)(random()%2048);l.fill(0);r.fill(0);auto alloc=luthier::tests::allocationsOnThisThread();
            rhythm.process(l.data(),r.data(),n);CHECK(luthier::tests::allocationsOnThisThread()==alloc);
            bool safe=true;for(int i=0;i<n;++i)safe&=std::isfinite(l[i])&&std::abs(l[i])<=16&&l[i]==r[i];CHECK(safe);
        }
        rhythm.reset();l.fill(0);r.fill(0);rhythm.process(l.data(),r.data(),2048);CHECK(std::all_of(l.begin(),l.end(),[](double x){return x==0;}));
    }
}
LUTHIER_TEST(RefPedalRc5Rhythm, fractionalClockAndExplicitLocateStayAccurate)
{
    Rc5Rhythm rhythm;rhythm.prepare(48000,42);rhythm.setTempo(120.1234);rhythm.setRunning(true);
    std::array<double,480> l{},r{};for(int b=0;b<100;++b){l.fill(0);r.fill(0);rhythm.process(l.data(),r.data(),480);}
    CHECK_NEAR(rhythm.stepPosition(),120.1234/15,1e-10);
    rhythm.locate(17.25);CHECK_NEAR(rhythm.stepPosition(),5.,1e-12);
    rhythm.locate(-.25);CHECK_NEAR(rhythm.stepPosition(),15.,1e-12);
    rhythm.locate(std::numeric_limits<double>::infinity());CHECK_NEAR(rhythm.stepPosition(),0.,1e-12);
    for(int kit=1;kit<=7;++kit){rhythm.setRunning(false);rhythm.select(1,0,kit);rhythm.setRunning(true);rhythm.process(l.data(),r.data(),480);CHECK(std::isfinite(l[479]));}
    rhythm.process(nullptr,r.data(),480);rhythm.process(l.data(),nullptr,480);rhythm.process(l.data(),r.data(),-1);
}

LUTHIER_TEST(RefPedalRc5Rhythm, hostileSelectorsClampConsistentlyAndKitsHaveDistinctAudio)
{
    CHECK(Rc5Rhythm::grid(-100,-1)==Rc5Rhythm::grid(1,0));
    CHECK(Rc5Rhythm::grid(100,100)==Rc5Rhythm::grid(57,1));
    std::array<double,7> fingerprints{};
    for(int kit=1;kit<=7;++kit)
    {
        Rc5Rhythm rhythm;rhythm.prepare(48000,123);rhythm.select(1,0,kit);rhythm.setRunning(true);
        std::array<double,4096> l{},r{};rhythm.process(l.data(),r.data(),(int)l.size());
        for(size_t i=0;i<l.size();++i)fingerprints[kit-1]+=l[i]*l[i]*(1.+i*.001);
        CHECK(fingerprints[kit-1]>1e-4);
        for(int previous=0;previous<kit-1;++previous)CHECK(std::abs(fingerprints[previous]-fingerprints[kit-1])>1e-4);
    }
}

LUTHIER_TEST(RefPedalRc5Rhythm, shorterPhraseSelectionPreservesFillAtBarBoundaryAndSeek)
{
    for(bool seek:{false,true})
    {
        Rc5Rhythm actual,reference;actual.prepare(48000,42);reference.prepare(48000,42);
        for(auto* rhythm:{&actual,&reference}){rhythm->select(2,0,1);rhythm->setRunning(true);rhythm->locate(8);}
        actual.select(1,0,1);
        std::array<double,480> a{},ar{},b{},br{};
        if(seek)actual.locate(12);
        else for(int block=0;block<200;++block)
        {
            a.fill(0);ar.fill(0);b.fill(0);br.fill(0);
            actual.process(a.data(),ar.data(),480);reference.process(b.data(),br.data(),480);
        }
        reference.setRunning(false);reference.select(1,0,1);reference.setRunning(true);reference.locate(12);
        bool equal=true;
        for(int block=0;block<200;++block)
        {
            a.fill(0);ar.fill(0);b.fill(0);br.fill(0);
            actual.process(a.data(),ar.data(),480);reference.process(b.data(),br.data(),480);
            for(size_t i=0;i<a.size();++i)equal&=a[i]==b[i]&&ar[i]==br[i];
        }
        CHECK(equal);
    }
}
