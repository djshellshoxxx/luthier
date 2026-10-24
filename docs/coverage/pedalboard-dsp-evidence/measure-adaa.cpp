#include "../../../Source/ReferencePedals/Common/TanhADAA2.h"
#include "../../../Source/ReferencePedals/Common/OversamplerAdapter.h"
#include <cstdio>
#include <fstream>
#include <vector>
#include <time.h>
using namespace luthier::reference::common;
int main()
{
 for(int factor:{2,4,8})
 {
  OversamplerAdapter os;os.prepareFactor(48000,factor);TanhADAA2 a;a.prepareFactor(factor);
  const auto path=std::string("/tmp/adaa-chain-")+std::to_string(factor)+".raw";std::ofstream file(path,std::ios::binary);
  for(int i=0;i<96000;++i){double y=os.process(100*std::sin(2*pi*5000*i/48000),[&](double x){return a.process(x);});if(i>=48000)file.write((char*)&y,sizeof y);}
  os.reset();a.reset();double maximum=0;int peak=0;for(int i=0;i<1024;++i){double y=os.process(i==0?1e-8:0,[&](double x){return a.process(x);});if(std::abs(y)>maximum){maximum=std::abs(y);peak=i;}}
  std::printf("factor%d impulsePeak%d reportedCombined%d\n",factor,peak,os.latencySamples()+a.latencyBaseSamples());
 }
 std::vector<double> left(96000),right(96000);for(int i=0;i<96000;++i){left[i]=std::sin(i*.13);right[i]=std::cos(i*.17);}
 for(double gain:{.1,100.})
 {
  OversamplerAdapter os[2];TanhADAA2 a[2];for(int c=0;c<2;++c){os[c].prepareFactor(48000,4);a[c].prepareFactor(4);}
  double sum=0;timespec start{},end{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&start);
  for(int i=0;i<96000;++i){sum+=os[0].process(gain*left[i],[&](double x){return a[0].process(x);});sum+=os[1].process(gain*right[i],[&](double x){return a[1].process(x);});}
  clock_gettime(CLOCK_THREAD_CPUTIME_ID,&end);const double cpu=end.tv_sec-start.tv_sec+(end.tv_nsec-start.tv_nsec)*1e-9;
  std::printf("factor4 stereo gain%.1f threadCPU %.6fs/2s =%.6f%% sum%.9f\n",gain,cpu,cpu*50,sum);
 }
}
