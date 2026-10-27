// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include <vector>
#include <array>
#include <cstdint>
namespace luthier::reference::common {
class FractionalDelay {
public:
 void prepare(int maxSamples) {capacity=std::max(1,maxSamples);data.assign((size_t)capacity+16,0);head=validSamples=0;kernel=&preparedKernel();}
 uint64_t ownedMemoryBytes() const noexcept {return uint64_t(data.capacity())*sizeof(double);}
 void reset() noexcept {head=validSamples=0;}
 void push(double x) noexcept {if(data.empty())return;data[head]=safe(x);if(++head==data.size())head=0;if(validSamples<data.size())++validSamples;}
 // Four-point Lagrange about floor(delay). Call push before read; zero is newest.
 double read(double delay) const noexcept {if(data.empty())return 0;delay=std::clamp(safe(delay),1.0,(double)capacity);int n=(int)delay;double f=delay-n;return safe(at(n-1)*(-f*(f-1)*(f-2)/6)+at(n)*((f+1)*(f-1)*(f-2)/2)+at(n+1)*(-(f+1)*f*(f-2)/2)+at(n+2)*((f+1)*f*(f-1)/6));}
 // E40: 16-tap Blackman-windowed sinc interpolation. The shared immutable
 // table is constructed by prepare() off the audio thread, never by read().
 double readBandlimited(double delay) const noexcept
 {
  if(capacity<8 || kernel==nullptr)return read(delay);
  delay=std::clamp(safe(delay),8.0,(double)capacity);
  const int n=(int)delay;const double position=(delay-n)*256;
  const int row=std::min(255,(int)position);const double fraction=position-row;
  double result=0;
  for(int j=0;j<16;++j)
   result+=at(n+j-7)*((*kernel)[row][j]+fraction*((*kernel)[row+1][j]-(*kernel)[row][j]));
  return safe(result);
 }
private:
 using Kernel=std::array<std::array<double,16>,257>;
 static const Kernel& preparedKernel()
 {
  static const Kernel coefficients=[]
  {
   Kernel result{};
   for(int row=0;row<=256;++row)
   {
    const double fraction=row/256.0;double sum=0;
    for(int j=0;j<16;++j)
    {
     const double x=j-7-fraction;
     const double sinc=std::abs(x)<1e-12 ? 1:std::sin(pi*x)/(pi*x);
     const double window=.42+.5*std::cos(pi*x/8)+.08*std::cos(2*pi*x/8);
     result[row][j]=sinc*window;sum+=result[row][j];
    }
    for(auto& coefficient:result[row])coefficient/=sum;
   }
   return result;
  }();
  return coefficients;
 }
 const Kernel* kernel=nullptr;
 double at(int age) const noexcept
 {
  // read/readBandlimited clamp ages below size(), so one wrap suffices.
  if((size_t)age>=validSamples)return 0;
  const auto distance=(size_t)age+1;
  return data[head>=distance ? head-distance : head+data.size()-distance];
 }
 std::vector<double> data;size_t head=0,validSamples=0;int capacity=1;
};
}
