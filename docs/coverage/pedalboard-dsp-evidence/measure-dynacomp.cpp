#include "../../../Source/ReferencePedals/Models/mxr_dynacomp.h"
#include <fstream>
#include <time.h>
using namespace luthier::reference;
void capture(double sens,double hz,double amplitude,const char*path){models::MxrDynaComp p;p.setParam(5,0);p.setParam(0,sens);p.prepare(48000,128,2);std::ofstream out(path,std::ios::binary);for(int i=0;i<144000;++i){double x=amplitude*std::sin(2*common::pi*hz*i/48000),r=x;p.process(&x,&r,1);if(i>=96000)out.write((char*)&x,sizeof x);}std::printf("capture sens%.2f hz%.0f GR%.6f\n",sens,hz,p.gainReductionDb());}
int main(){capture(.5,1000,.1,"/tmp/dyna_thd.raw");capture(1,1000,.1,"/tmp/dyna_thd_max.raw");capture(.5,5000,1,"/tmp/dyna_alias.raw");
for(float sens:{.25f,.5f,.75f,1.f}){for(double db:{-50.,-40.,-30.,-20.,-10.,0.}){models::MxrDynaComp p;p.setParam(5,0);p.setParam(0,sens);p.prepare(48000,128,2);double pow=0;for(int i=0;i<144000;++i){double x=common::dbGain(db)*std::sin(2*common::pi*1000*i/48000),r=x;p.process(&x,&r,1);if(i>=96000)pow+=x*x;}std::printf("static sens%.2f input%.0fdBpk output%.6fdBpk GR%.6f\n",sens,db,10*std::log10(2*pow/48000),p.gainReductionDb());}}
for(int slow=0;slow<2;++slow){models::MxrDynaComp p;p.setParam(5,0);p.setParam(4,.5f);p.setParam(2,slow);p.prepare(48000,1,2);std::array<double,48000> rise{},fall{};double base=0;
for(int i=0;i<48000;++i){double x=common::dbGain(-30)*std::sin(2*common::pi*1000*i/48000),r=x;p.process(&x,&r,1);}base=p.gainReductionDb();
for(int i=0;i<48000;++i){double x=common::dbGain(-10)*std::sin(2*common::pi*1000*i/48000),r=x;p.process(&x,&r,1);rise[i]=p.gainReductionDb();}
for(int i=0;i<48000;++i){double x=common::dbGain(-30)*std::sin(2*common::pi*1000*i/48000),r=x;p.process(&x,&r,1);fall[i]=p.gainReductionDb();}
int at=0,rt=0;while(at<48000 && rise[at]<base+.6321205588*(rise.back()-base))++at;while(rt<48000 && fall[rt]>base+.3678794412*(rise.back()-base))++rt;
std::printf("burst slow%d baselineGR%.6f finalGR%.6f attack63 %.6fms release63 %.6fms\n",slow,base,rise.back(),at/48.,rt/48.);}
models::MxrDynaComp p;p.setParam(5,0);p.prepare(48000,128,2);double l[128]{},r[128]{};timespec start{},end{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&start);for(int b=0;b<750;++b){for(int i=0;i<128;++i){l[i]=.1*std::sin((b*128+i)*.13);r[i]=.1*std::cos((b*128+i)*.17);}p.process(l,r,128);}clock_gettime(CLOCK_THREAD_CPUTIME_ID,&end);double cpu=end.tv_sec-start.tv_sec+(end.tv_nsec-start.tv_nsec)*1e-9;std::printf("Dyna stereo2x48k128 CPU %.6fs/2s =%.6f%%\n",cpu,cpu*50);}
