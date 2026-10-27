#include <juce_dsp/juce_dsp.h>
#define private public
#include "Source/DSP/Amp/AmpEngine.h"
#undef private
#include <fstream>
#include <iostream>
#include <vector>
#include <iomanip>
#include <array>
using namespace luthier;
static double rms(const std::vector<double>&a,int start,int count){double e=0;for(int i=start;i<start+count;++i)e+=a[i]*a[i];return std::sqrt(e/count);}
int main(){
 std::ifstream f("/workspace/luthier-evidence/release-survey-b5/original-stage-data/r48000-t19.f32",std::ios::binary);
 std::vector<std::array<float,5>> input;std::array<float,5> frame;while(f.read((char*)frame.data(),sizeof(frame)))input.push_back(frame);
 if(input.empty())return 2;
 std::cout<<std::setprecision(17);
 for(int mode=0;mode<5;++mode){
 AmpEngine amp;amp.prepare(48000,256);amp.setGain(double(.35f)+double(.3f)*.55);amp.setMaster(double(.7f));amp.setPresence(double(.4f));amp.reset();
 std::vector<double> y(input.size()),di(input.size()),ref(input.size());double maxerr=0,err=0;
 for(size_t i=0;i<input.size();++i){
  double x=input[i][1];
  if(mode==1&&i>=24000)x=0;
  if(mode==2)x*=.001;
  if(mode==3&&i==24000)amp.reset();
  if(mode==4){amp.voicing.sagAmount=0;amp.supplyVoltage=1;amp.sagFollower.reset();}
  y[i]=amp.processSample(x);di[i]=x;ref[i]=input[i][2];
  const double d=y[i]-ref[i];maxerr=std::max(maxerr,std::abs(d));err+=d*d;
  if(i==23999||i==35999||i==37439)std::cout<<"STATE mode="<<mode<<" n="<<i<<" gain="<<amp.gainSmooth.getCurrent()<<" master="<<amp.masterSmooth.getCurrent()<<" voltage="<<amp.supplyVoltage<<" sagEnv="<<amp.sagFollower.env<<" os="<<amp.oversampler.getFactor()<<'\n';
 }
 double errBefore=0,errAfter=0;for(int j=0;j<1440;++j){errBefore=std::max(errBefore,std::abs(y[22560+j]-ref[22560+j]));errAfter=std::max(errAfter,std::abs(y[36000+j]-ref[36000+j]));}
 std::ofstream raw("/workspace/luthier-evidence/release-amp-cause-280/mode"+std::to_string(mode)+".f64",std::ios::binary);raw.write((char*)y.data(),y.size()*sizeof(double));
 std::cout<<"WINDOW_ERROR mode="<<mode<<" beforeMax="<<errBefore<<" afterMax="<<errAfter<<'\n';
 const double before=rms(y,22560,1440),after=rms(y,36000,1440);
 std::cout<<"RESULT mode="<<mode<<" before="<<before<<" after="<<after<<" drop="<<20*log10(before/after)<<" inputDrop="<<20*log10(rms(di,22560,1440)/rms(di,36000,1440))<<" referenceBefore="<<rms(ref,22560,1440)<<" referenceAfter="<<rms(ref,36000,1440)<<" maxError="<<maxerr<<" rmsError="<<sqrt(err/y.size())<<'\n';
 }
}
