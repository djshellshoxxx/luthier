#include "Source/ReferencePedals/Models/mxr_phase90_script.h"
#include <array>
#include <cstdio>
#include <ctime>
namespace luthier::tests {long allocationsOnThisThread() noexcept;}
int main()
{
    bool all=true;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        luthier::reference::models::MxrPhase90Script pedal;pedal.prepare(sr,2048,1);pedal.setEnabled(true);
        std::array<double,2048> l{},r{};uint32_t random=73;int frames=0,blocks=0;bool finite=true;double peak=0;
        auto next=[&](){random=1664525u*random+1013904223u;return double(random)/4294967296.;};
        constexpr int sizes[]{1,4,16,64,512,2048};const long before=luthier::tests::allocationsOnThisThread();
        timespec start{},end{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&start);
        while(frames<int(sr*1800))
        {
            const int n=std::min(sizes[blocks%6],int(sr*1800)-frames);
            if(blocks%31==0){const float normalized=float(next());const int parameter=int(next()*pedal.desc().numParams);pedal.setParam(parameter,normalized);pedal.setExpression(0,float(next()));pedal.setEnabled(next()>.1);pedal.onFoot({blocks%2==0,float(next()),float(next()),0});}
            for(int i=0;i<n;++i){l[i]=(next()-.5)*2;r[i]=(next()-.5)*2;}
            pedal.process(l.data(),r.data(),n);
            for(int i=0;i<n;++i){finite=finite&&std::isfinite(l[i])&&std::isfinite(r[i]);peak=std::max(peak,std::max(std::abs(l[i]),std::abs(r[i])));}
            frames+=n;++blocks;
        }
        clock_gettime(CLOCK_THREAD_CPUTIME_ID,&end);const long allocations=luthier::tests::allocationsOnThisThread()-before;
        all=all&&finite&&peak<=16&&allocations==0;
        std::printf("sr%.0f seconds1800 frames%d blocks%d initial_seed73 final_seed%u finite%d peak%.12g allocations%ld cpu_seconds%.6f\n",sr,frames,blocks,random,finite,peak,allocations,end.tv_sec-start.tv_sec+(end.tv_nsec-start.tv_nsec)*1e-9);std::fflush(stdout);
    }
    return all?0:1;
}
