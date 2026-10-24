// OWNER: WP17
#include "BoardCanvas.h"
#include "../../ReferencePedals/PedalRegistry.h"
#include "BoardSurfacePainter.h"
#include "../Theme.h"
#include "../AnimationPolicy.h"
namespace luthier::reference
{
BoardCanvas::BoardCanvas(BoardModel& m,juce::AudioProcessorValueTreeState* p):model(m),parameters(p),drag(m){addAndMakeVisible(cableLayer);drag.onError=[this](const auto& error){if(onError)onError(error);};model.addListener(this);setTitle("Pedalboard");refresh();}
BoardCanvas::~BoardCanvas(){cancelDrag();stopTimer();model.removeListener(this);}
PedalView* BoardCanvas::viewFor(const InstanceId& id) const {const auto it=views.find(id.toString());return it==views.end()?nullptr:it->second.get();}
void BoardCanvas::closeInspectors(){for(auto& entry:views)entry.second->closeInspector();}
void BoardCanvas::setLearn(bool b){learning=b;for(auto& v:views){v.second->learnMode=b;v.second->repaint();}}
void BoardCanvas::refresh()
{
    const auto order=model.getOrder();
    for(auto it=views.begin();it!=views.end();){if(std::none_of(order.begin(),order.end(),[&](const auto& id){return id.toString()==it->first;}))it=views.erase(it);else ++it;}
    int x=72,maxH=300;const auto settings=model.getBoardSettings();
    layoutSignature=juce::String((int)settings.mode)+"|"+juce::String((int)settings.style)+"|"+juce::String(settings.lanes.numLanes);
    for(const auto& id:order)
    {
        auto& ptr=views[id.toString()];if(!ptr){ptr=std::make_unique<PedalView>(model,id,parameters);addAndMakeVisible(*ptr);}
        auto* p=ptr.get();p->onLearn=[this](auto i,int n,bool c){if(onLearn)onLearn(i,n,c);};p->onFocus=[this](auto& v){if(onFocus)onFocus(v);};p->learnMode=learning;p->refresh();
        p->onNavigate=[this](const InstanceId& current,int direction){const auto ids=model.getOrder();const auto at=std::find(ids.begin(),ids.end(),current);if(at==ids.end()||ids.empty())return;const auto position=juce::jlimit(0,(int)ids.size()-1,(int)std::distance(ids.begin(),at)+direction);if(auto* next=viewFor(ids[(size_t)position]))next->grabKeyboardFocus();};
        p->onDrag=[this](auto& v,auto pos,auto offset,bool end){dragTo(v,pos,offset,end);};p->onCancelDrag=[this]{return cancelDrag();};
        auto instance=model.getInstance(id);const auto size=p->physicalSize();
        const int y=48+(settings.mode==Mode::Series?0:instance->lane*300);
        p->setBounds(x,y,size.x,size.y);p->placeCaptionIn(*this);p->toFront(false);maxH=juce::jmax(maxH,y+size.y+48);x+=size.x+56;
    }
    setSize(juce::jmax(1100,x+72),maxH);cableLayer.setBounds(getLocalBounds());cableLayer.toFront(false);if(ghost)ghost->toFront(false);repaint();
}
void BoardCanvas::settingsChanged()
{
    const auto settings=model.getBoardSettings();const auto key=juce::String((int)settings.mode)+"|"+juce::String((int)settings.style)+"|"+juce::String(settings.lanes.numLanes);
    if(key!=layoutSignature)refresh();
}
void BoardCanvas::instanceChanged(const InstanceId& id)
{
    auto* view=viewFor(id);const auto instance=model.getInstance(id);if(!view||!instance){refresh();return;}
    view->refresh();const auto settings=model.getBoardSettings();const int y=48+(settings.mode==Mode::Series?0:instance->lane*300);
    if(view->getY()!=y||view->getWidth()!=view->physicalSize().x||view->getHeight()!=view->physicalSize().y)refresh();
    else {view->placeCaptionIn(*this);repaint(view->getBounds().expanded(12));}
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
    g.setColour(Palette::textPrimary);g.setFont(juce::FontOptions(11.f));
    for(int i=0;i<3;++i)g.drawText(i==0?"PRE | before AMP":i==1?"LOOP | AMP > CAB": "POST | after CAB",32+i*350,8,330,26,juce::Justification::centred);
    if(drag.active()){g.setColour(Palette::accentBright.withAlpha(.35f));for(int i=0;i<3;++i)g.drawRoundedRectangle({32.f+i*350.f,8.f,330.f,26.f},3,1);}
    for(const auto& id:model.getOrder()){auto* p=viewFor(id);const auto instance=model.getInstance(id);BoardSurfacePainter::paintMount(g,p->getBounds().toFloat(),instance->mounting==Mounting::automatic?BoardSurfacePainter::defaultMount(settings.style):instance->mounting,id.toString().hashCode());}
    if(views.empty()){g.setColour(Palette::textPrimary);g.setFont(18.f);g.drawFittedText("Add a pedal from the browser to build your signal chain",getLocalBounds().reduced(64),juce::Justification::centred,2);}
}
void BoardCanvas::paintCables(juce::Graphics& g)
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
    for(size_t i=1;i<order.size();++i){auto* a=viewFor(order[i-1]);auto* b=viewFor(order[i]);draw(a->getPosition().toFloat()+a->outputJack(),b->getPosition().toFloat()+b->inputJack(),">",false);}
    if(settings.fb.on){auto* a=viewFor(settings.fb.src);auto* b=viewFor(settings.fb.dst);if(a&&b)draw(a->getPosition().toFloat()+a->outputJack(),b->getPosition().toFloat()+b->inputJack(),"FEEDBACK",true);}
    auto* first=viewFor(order.front());auto* last=viewFor(order.back());const auto in=first->getPosition().toFloat()+first->inputJack(),out=last->getPosition().toFloat()+last->outputJack();draw({30,in.y},in,"IN",false);draw(out,{out.x+48,out.y},"OUT",false);
}
}

namespace luthier::reference
{
void BoardCanvas::Ghost::paint(juce::Graphics& g)
{
    AnimationPolicy::notePaint(*this);
    juce::DropShadow(juce::Colours::black.withAlpha(.55f),14,{0,10}).drawForRectangle(g,getLocalBounds().reduced(14));
    g.setOpacity(.9f);g.drawImage(image,getLocalBounds().reduced(14).toFloat());
}
DragController::Target BoardCanvas::dragTarget(juce::Point<int> position) const
{
    const auto instance=model.getInstance(drag.id());if(!instance)return {};
    const auto settings=model.getBoardSettings();int index=0;
    for(const auto& other:model.getOrder())if(other!=drag.id())if(auto* view=viewFor(other))if(view->getBounds().getCentreX()<position.x)++index;
    const auto placement=position.y<40?(Placement)juce::jlimit(0,2,(position.x-32)/350):instance->placement;
    const int lane=position.y<40?instance->lane:settings.mode==Mode::Series?0:juce::jlimit(0,settings.mode==Mode::AB?1:settings.lanes.numLanes-1,(position.y-48)/300);
    return {index,lane,placement};
}
void BoardCanvas::dragTo(PedalView& view,juce::Point<int> point,juce::Point<int> offset,bool end)
{
    if(!drag.active())
    {
        const auto settings=model.getBoardSettings();if(!drag.begin(view.id(),juce::Time::getMillisecondCounterHiRes(),settings.ui.getProperty("liveReorder",true)))return;
        closeInspectors();ghost=std::make_unique<Ghost>();ghost->image=view.createComponentSnapshot(view.getLocalBounds(),true,1.04f);const float lift=AnimationPolicy::get().mayAnimate(AnimationPolicy::Transition)?1.04f:1.f;ghost->setSize(juce::roundToInt(view.getWidth()*lift)+28,juce::roundToInt(view.getHeight()*lift)+28);ghost->setInterceptsMouseClicks(false,false);addAndMakeVisible(*ghost);view.setAlpha(.35f);dragOffset=offset;startTimer(16);
    }
    dragPointer=point;ghost->setTopLeftPosition(point-dragOffset-juce::Point<int>(14,14));ghost->toFront(false);
    const auto target=dragTarget(point);const auto now=juce::Time::getMillisecondCounterHiRes();
    if(end){drag.finish(target,now);finishVisualDrag();}else if(!drag.update(target,now))finishVisualDrag();
}
void BoardCanvas::finishVisualDrag(){stopTimer();ghost.reset();for(auto& view:views)view.second->setAlpha(1.f);repaint();}
bool BoardCanvas::cancelDrag(){if(!drag.active())return false;const bool result=drag.cancel();if(result)finishVisualDrag();return result;}
bool BoardCanvas::keyPressed(const juce::KeyPress& key){return key.getKeyCode()==juce::KeyPress::escapeKey&&cancelDrag();}
void BoardCanvas::visibilityChanged(){if(!isShowing())cancelDrag();}
void BoardCanvas::timerCallback()
{
    if(!drag.active()){stopTimer();return;}
    if(auto* viewport=findParentComponentOfClass<juce::Viewport>())
    {
        auto pointer=viewport->getLocalPoint(this,dragPointer);const auto old=viewport->getViewPosition();
        auto speed=[](int p,int extent){return p<48?-juce::jlimit(0.f,1.f,(48-p)/48.f)*600.f:p>extent-48?juce::jlimit(0.f,1.f,(p-extent+48)/48.f)*600.f:0.f;};
        const juce::Point<int> delta(juce::roundToInt(speed(pointer.x,viewport->getWidth())*.016f),juce::roundToInt(speed(pointer.y,viewport->getHeight())*.016f));
        viewport->setViewPosition(old+delta);const auto moved=viewport->getViewPosition()-old;dragPointer+=moved;
        if(ghost)ghost->setTopLeftPosition(dragPointer-dragOffset-juce::Point<int>(14,14));
    }
    if(!drag.update(dragTarget(dragPointer),juce::Time::getMillisecondCounterHiRes()))finishVisualDrag();
}
}
