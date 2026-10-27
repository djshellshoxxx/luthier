#include "Source/ReferencePedals/Models/boss_ce2.h"
#include "Source/ReferencePedals/Models/boss_ce2w.h"
#include "Source/ReferencePedals/Models/boss_dd3t.h"
#include "Source/ReferencePedals/Models/mxr_phase90_script.h"
#include <complex>
#include <cstdio>
using namespace luthier::reference;constexpr double pi=3.14159265358979323846;
template<class P> void run(const char* name,P& p,double db,int select){p.setEnabled(true);p.prepare(48000,1,1);std::complex<double> harmonics[10]{};double amp=pow(10,db/20);for(int i=0;i<96000;++i){double x=amp*sin(2*pi*1000*i/48000),l=x,r=x;p.process(&l,&r,1);double y=select==1?(l-r)*.5:select==2?l-x:l;if(i>=48000)for(int h=1;h<10;++h)harmonics[h]+=y*std::exp(std::complex<double>(0,-2*pi*1000*h*i/48000));}double sum=0;for(int h=2;h<10;++h)sum+=norm(harmonics[h]);printf("THD %s %.1fdB %.6f%% h3%.4f%%\n",name,db,100*sqrt(sum)/abs(harmonics[1]),100*abs(harmonics[3])/abs(harmonics[1]));}
int main(){for(double db:{-20.,-18.,-12.,-10.,-6.,-3.,0.}){models::BossCe2 ce;ce.setParam(1,0);run("CE2",ce,db,0);models::BossCe2w cw;cw.setParam(1,0);cw.setParam(7,0);run("CE2WStandard wet",cw,db,1);cw.setParam(2,.5);run("CE2W CE1 wet",cw,db,1);models::BossDd3t dd;dd.setParam(1,0);dd.setParam(10,1);run("DD3T wet",dd,db,2);models::MxrPhase90Script ph;ph.setParam(3,0);ph.setParam(5,0);run("Phase90",ph,db,0);}}
