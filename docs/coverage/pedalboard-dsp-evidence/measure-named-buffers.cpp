#include "../../../Source/ReferencePedals/Models/boss_sd1.h"
#include <cmath>
#include <cstdio>
using namespace luthier::reference;
int main(){for(double sr:{44100.,48000.,96000.,192000.}){models::BossSd1 p;p.prepare(sr,1,1);for(double f:{40.,15000.}){p.reset();double energy=0;for(int i=0;i<(int)(2*sr);++i){double x=std::sin(2*common::pi*f*i/sr),r=x;p.processBypass(&x,&r,1);if(i>=(int)sr)energy+=x*x;}std::printf("SD1 coloured bypass sr%.0f f%.0f gain %.9fdB (original ±.1dB40..15k gate)\n",sr,f,10*std::log10(2*energy/sr));}}}
