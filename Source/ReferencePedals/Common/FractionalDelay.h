// OWNER: WP7
#pragma once
#include "Smoothers.h"
#include <vector>
namespace luthier::reference::common {
class FractionalDelay {
public:
 void prepare(int maxSamples) {capacity=std::max(1,maxSamples);data.assign((size_t)capacity+4,0);head=0;}
 void reset() noexcept {std::fill(data.begin(),data.end(),0);head=0;}
 void push(double x) noexcept {if(data.empty())return;data[head]=safe(x);head=(head+1)%data.size();}
 // Four-point Lagrange about floor(delay). Call push before read; zero is newest.
 double read(double delay) const noexcept {if(data.empty())return 0;delay=std::clamp(safe(delay),1.0,(double)capacity);int n=(int)delay;double f=delay-n;return safe(at(n-1)*(-f*(f-1)*(f-2)/6)+at(n)*((f+1)*(f-1)*(f-2)/2)+at(n+1)*(-(f+1)*f*(f-2)/2)+at(n+2)*((f+1)*f*(f-1)/6));}
private:
 double at(int age) const noexcept {return data[(head+data.size()-1-(size_t)age)%data.size()];}
 std::vector<double> data;size_t head=0;int capacity=1;
};
}
