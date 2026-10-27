// OWNER: WP15/WP16
#include "TestFramework.h"
#include "../ReferencePedals/Common/PagedLooperCore.h"
#include <limits>
using luthier::reference::common::PagedLooperCore;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }

LUTHIER_TEST(RefPedalLooperWindow, minimumCoverageMatchesIndependentEnumerationOfEveryCursor)
{
    constexpr auto unlimited=std::numeric_limits<uint64_t>::max();
    bool correct=true;int cases=0;uint64_t badFrames=0,badExpected=0,badActual=0;int badPage=0,badAhead=0;
    const auto allocations=luthier::tests::allocationsOnThisThread();
    for(int page:{16,17,32})for(uint64_t frames=1;frames<=uint64_t(page*9+1);++frames)
        for(int ahead=0;ahead<=7;++ahead)
    {
        const auto pages=frames/uint64_t(page)+(frames%uint64_t(page)!=0);
        uint64_t expected=unlimited;
        if(uint64_t(ahead+1)<pages)
            for(uint64_t cursor=0;cursor<frames;++cursor)
            {
                const auto currentPage=cursor/uint64_t(page);
                uint64_t covered=std::min(frames,(currentPage+1)*uint64_t(page))-cursor;
                for(int offset=1;offset<=ahead;++offset)
                {
                    const auto index=(currentPage+uint64_t(offset))%pages;
                    covered+=std::min(uint64_t(page),frames-index*uint64_t(page));
                }
                expected=std::min(expected,covered);
            }
        const auto actual=PagedLooperCore::minimumForwardWindowFrames(frames,page,ahead);++cases;
        if(actual!=expected && correct)
        {correct=false;badFrames=frames;badPage=page;badAhead=ahead;badExpected=expected;badActual=actual;}
    }
    const bool allocated=luthier::tests::allocationsOnThisThread()!=allocations;
    CHECK(!allocated);CHECK(cases==4704);
    CHECK_MSG(correct,"frames="+juce::String(badFrames)+" page="+juce::String(badPage)
        +" ahead="+juce::String(badAhead)+" expected="+juce::String(badExpected)+" actual="+juce::String(badActual));
}

LUTHIER_TEST(RefPedalLooperWindow, ninetyMinutePartialPageExposesOldNominalOverestimate)
{
    constexpr uint64_t frames=1036800000,required=384000;
    CHECK(uint64_t(32768)*12>=required); // previous nominal assertion passes
    CHECK(PagedLooperCore::minimumForwardWindowFrames(frames,32768,12)==380929);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(frames,32768,12)<required);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(frames,32768,13)==413697);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(frames,32768,13)>=required);
    // Even a one-frame final page retains the two-second minimum with A13.
    CHECK(PagedLooperCore::minimumForwardWindowFrames(uint64_t(32768)*20+1,32768,13)==393218);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(uint64_t(32768)*20+1,32768,13)>=required);
}

LUTHIER_TEST(RefPedalLooperWindow, invalidAndExtremeGeometryCannotOverflow)
{
    constexpr auto last=std::numeric_limits<uint64_t>::max();
    CHECK(PagedLooperCore::minimumForwardWindowFrames(0,16,3)==0);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(32,0,3)==0);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(32,-1,3)==0);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(32,16,-1)==0);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(32,16,0)==1);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(32,16,1)==last);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(last,1,0)==1);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(last,1,23)==24);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(last,65536,13)==851968);
    const auto maximum=std::numeric_limits<int>::max();
    const auto expected=uint64_t(1)+uint64_t(maximum-1)*uint64_t(maximum)+last%uint64_t(maximum);
    CHECK(PagedLooperCore::minimumForwardWindowFrames(last,maximum,maximum)==expected);
}

LUTHIER_TEST(RefPedalLooperWindow, neutralMinimumMatchesIndependentReverseEnumerationOfEveryCursor)
{
    constexpr auto unlimited=std::numeric_limits<uint64_t>::max();
    bool correct=true;int cases=0;uint64_t badFrames=0,badExpected=0,badActual=0;int badPage=0,badAhead=0;
    const auto allocations=luthier::tests::allocationsOnThisThread();
    for(int page:{16,17,32})for(uint64_t frames=1;frames<=uint64_t(page*9+1);++frames)
        for(int ahead=0;ahead<=7;++ahead)
    {
        const auto pages=frames/uint64_t(page)+(frames%uint64_t(page)!=0);
        uint64_t expected=unlimited;
        if(uint64_t(ahead+1)<pages)
            for(uint64_t cursor=0;cursor<frames;++cursor)
            {
                uint64_t index=cursor/uint64_t(page),covered=cursor%uint64_t(page)+1;
                for(int offset=1;offset<=ahead;++offset)
                {
                    index=index==0?pages-1:index-1;
                    covered+=std::min(uint64_t(page),frames-index*uint64_t(page));
                }
                expected=std::min(expected,covered);
            }
        const auto actual=PagedLooperCore::minimumWindowFrames(frames,page,ahead);++cases;
        if(actual!=expected && correct)
        {correct=false;badFrames=frames;badPage=page;badAhead=ahead;badExpected=expected;badActual=actual;}
    }
    const bool allocated=luthier::tests::allocationsOnThisThread()!=allocations;
    CHECK(!allocated);CHECK(cases==4704);
    CHECK_MSG(correct,"frames="+juce::String(badFrames)+" page="+juce::String(badPage)
        +" ahead="+juce::String(badAhead)+" expected="+juce::String(badExpected)+" actual="+juce::String(badActual));
}
