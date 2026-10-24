#include "../../../Source/ReferencePedals/Generic/Boost.h"
#include "../../../Source/ReferencePedals/Generic/Overdrive.h"
#include "../../../Source/ReferencePedals/Generic/Fuzz.h"
#include "../../../Source/ReferencePedals/Generic/Compressor.h"
#include <fstream>
#include <ctime>
#include <time.h>
using namespace luthier::reference;
void capture(IPedal& p,const char* path,int q){p.setParam(0,1);p.setParam(q,1);p.prepare(48000,1,2);std::ofstream out(path,std::ios::binary);for(int i=0;i<96000;++i){double x=std::sin(2*common::pi*5000*i/48000),r=x;p.process(&x,&r,1);if(i>=48000)out.write((char*)&x,sizeof x);}}
int main(){generic::Boost b;generic::Overdrive o;generic::Fuzz f;capture(b,"/tmp/ref_boost.raw",4);capture(o,"/tmp/ref_od.raw",5);capture(f,"/tmp/ref_fuzz.raw",6);
 for(int stereo=0;stereo<2;++stereo){std::array<std::unique_ptr<IPedal>,8> p{std::make_unique<generic::Boost>(),std::make_unique<generic::Overdrive>(),std::make_unique<generic::Fuzz>(),std::make_unique<generic::Compressor>(),std::make_unique<generic::Boost>(),std::make_unique<generic::Overdrive>(),std::make_unique<generic::Fuzz>(),std::make_unique<generic::Compressor>()};
 for(auto& x:p)x->prepare(48000,128,1);double l[128]{},r[128]{};timespec begin{},end{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&begin);for(int block=0;block<750;++block){for(int i=0;i<128;++i){l[i]=.1*std::sin((block*128+i)*.13);r[i]=stereo?.1*std::cos((block*128+i)*.17):l[i];}for(auto& x:p)x->process(l,r,128);}clock_gettime(CLOCK_THREAD_CPUTIME_ID,&end);double sec=end.tv_sec-begin.tv_sec+(end.tv_nsec-begin.tv_nsec)*1e-9;std::printf("8 generic pedals normal48k128 stereo%d: %.6fs CPU/2s audio = %.3f%% core\n",stereo,sec,sec*50);}
}
