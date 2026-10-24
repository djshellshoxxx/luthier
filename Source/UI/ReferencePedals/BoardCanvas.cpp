// OWNER: WP17
#include "BoardCanvas.h"
#include "../../ReferencePedals/PedalRegistry.h"
#include "BoardSurfacePainter.h"
#include "../Theme.h"
#include "../AnimationPolicy.h"
namespace luthier::reference
{
BoardCanvas::BoardCanvas(BoardModel& m,juce::AudioProcessorValueTreeState* p):model(m),parameters(p){model.addListener(this);setTitle("Pedalboard");refresh();}
BoardCanvas::~BoardCanvas(){model.removeListener(this);}
PedalView* BoardCanvas::viewFor(const InstanceId& id) const {const auto it=views.find(id.toString());return it==views.end()?nullptr:it->second.get();}
void BoardCanvas::closeInspectors(){for(auto& entry:views)entry.second->closeInspector();}
void BoardCanvas::setLearn(bool b){learning=b;for(auto& v:views){v.second->learnMode=b;v.second->repaint();}}
void BoardCanvas::refresh()
{
    const auto order=model.getOrder();
    for(auto it=views.begin();it!=views.end();){if(std::none_of(order.begin(),order.end(),[&](const auto& id){return id.toString()==it->first;}))it=views.erase(it);else ++it;}
    int x=72,maxH=300;const auto settings=model.getBoardSettings();
    for(const auto& id:order)
    {
        auto& ptr=views[id.toString()];if(!ptr){ptr=std::make_unique<PedalView>(model,id,parameters);addAndMakeVisible(*ptr);}
        auto* p=ptr.get();p->onLearn=[this](auto i,int n,bool c){if(onLearn)onLearn(i,n,c);};p->onFocus=[this](auto& v){if(onFocus)onFocus(v);};p->learnMode=learning;p->refresh();
        p->onNavigate=[this](const InstanceId& current,int direction){const auto ids=model.getOrder();const auto at=std::find(ids.begin(),ids.end(),current);if(at==ids.end()||ids.empty())return;const auto position=juce::jlimit(0,(int)ids.size()-1,(int)std::distance(ids.begin(),at)+direction);if(auto* next=viewFor(ids[(size_t)position]))next->grabKeyboardFocus();};
        p->onDrag=[this](auto& v,juce::Point<int> pos,bool){const auto orderNow=model.getOrder();int n=0;for(const auto& other:orderNow)if(other!=v.id()&&viewFor(other)->getBounds().getCentreX()<pos.x)++n;model.apply(cmd::Move{v.id(),n});};
        auto instance=model.getInstance(id);const auto size=p->physicalSize();
        const int y=48+(settings.mode==Mode::Series?0:instance->lane*300);
        p->setBounds(x,y,size.x,size.y);p->placeCaptionIn(*this);p->toFront(false);maxH=juce::jmax(maxH,y+size.y+48);x+=size.x+56;
    }
    setSize(juce::jmax(1100,x+72),maxH);repaint();
}
bool BoardCanvas::isInterestedInDragSource(const SourceDetails& d){return d.description.toString().startsWith("ref-pedal:");}
void BoardCanvas::itemDropped(const SourceDetails& d)
{
    const auto id=d.description.toString().fromFirstOccurrenceOf("ref-pedal:",false,false);int index=0;
    for(const auto& other:model.getOrder())if(viewFor(other)->getBounds().getCentreX()<d.localPosition.x)++index;
    model.apply(cmd::Add{id.toStdString(),index});
}
void BoardCanvas::paint(juce::Graphics& g)
{
    AnimationPolicy::notePaint(*this);const auto settings=model.getBoardSettings();
    BoardSurfacePainter::paint(g,getLocalBounds().toFloat().reduced(20),settings.style);
    for(const auto& id:model.getOrder()){auto* p=viewFor(id);const auto instance=model.getInstance(id);BoardSurfacePainter::paintMount(g,p->getBounds().toFloat(),instance->mounting==Mounting::automatic?BoardSurfacePainter::defaultMount(settings.style):instance->mounting,id.toString().hashCode());}
    if(views.empty()){g.setColour(Palette::textPrimary);g.setFont(18.f);g.drawFittedText("Add a pedal from the browser to build your signal chain",getLocalBounds().reduced(64),juce::Justification::centred,2);}
}
void BoardCanvas::paintOverChildren(juce::Graphics& g)
{
    if(!cables)return;const auto order=model.getOrder();if(order.empty())return;
    const auto settings=model.getBoardSettings();
    auto draw=[&](juce::Point<float> a,juce::Point<float> b,juce::String label,bool feedback)
    {
        juce::Path path;path.startNewSubPath(a);const float bend=feedback?-70.f:32.f;path.cubicTo(a.x+25,a.y+bend,b.x-25,b.y+bend,b.x,b.y);
        g.setColour(Palette::backgroundDeep);g.strokePath(path,juce::PathStrokeType(5));g.setColour(feedback?Palette::warning:Palette::accent);g.strokePath(path,juce::PathStrokeType(2));
        const auto mid=path.getPointAlongPath(path.getLength()*.5f);g.drawLine(mid.x-4,mid.y-3,mid.x+2,mid.y,1.5f);g.drawLine(mid.x+2,mid.y,mid.x-4,mid.y+3,1.5f);
        g.setColour(Palette::textPrimary);g.setFont(10.f);g.drawText(label,(int)mid.x-30,(int)mid.y-18,60,14,juce::Justification::centred);
    };
    for(size_t i=1;i<order.size();++i){auto* a=viewFor(order[i-1]);auto* b=viewFor(order[i]);draw(a->getPosition().toFloat()+a->outputJack(),b->getPosition().toFloat()+b->inputJack(),"→",false);}
    if(settings.fb.on){auto* a=viewFor(settings.fb.src);auto* b=viewFor(settings.fb.dst);if(a&&b)draw(a->getPosition().toFloat()+a->outputJack(),b->getPosition().toFloat()+b->inputJack(),"FEEDBACK",true);}
    auto* first=viewFor(order.front());auto* last=viewFor(order.back());const auto in=first->getPosition().toFloat()+first->inputJack(),out=last->getPosition().toFloat()+last->outputJack();draw({30,in.y},in,"IN",false);draw(out,{out.x+48,out.y},"OUT",false);
}
}
