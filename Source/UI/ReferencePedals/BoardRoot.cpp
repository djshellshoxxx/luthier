// OWNER: WP17
#include "BoardRoot.h"
#include "../Theme.h"
#include "../AnimationPolicy.h"
#include "../../PluginProcessor.h"
#include "../../ReferencePedals/Midi/MidiRouter.h"
#include "BoardMidiPanel.h"
namespace luthier::reference
{
BoardRoot::BoardRoot(BoardModel& m,LuthierAudioProcessor* p):model(m),processor(p),canvas(m,p?&p->getState():nullptr),browser(m)
{
    setWantsKeyboardFocus(true);setTitle("Pedalboard");setFocusContainerType(FocusContainerType::focusContainer);
    viewport.setViewedComponent(&canvas,false);viewport.setScrollBarsShown(true,true);viewport.setScrollBarThickness(12);
    viewport.onZoom=[this](float factor,juce::Point<float> cursor)
    {
        const auto oldZoom=zoom;const auto oldPos=viewport.getViewPosition().toFloat();zoomTo(zoom*factor);
        const auto next=(oldPos+cursor)*(zoom/oldZoom)-cursor;viewport.setViewPosition(next.roundToInt());saveView();
    };
    toolbarViewport.setViewedComponent(&toolbar,false);toolbarViewport.setScrollBarsShown(false,true);toolbarViewport.setScrollBarThickness(10);
    for(auto* c:std::array<juce::Component*,4>{&viewport,&toolbarViewport,&browser,&status})addAndMakeVisible(c);
    for(auto* c:std::array<juce::Component*,20>{&style,&add,&undo,&redo,&mode,&enabled,&mix,&mixLaw,&tempo,&learn,&cables,&fit,&minus,&zoomLabel,&plus,&settings,&dock,&popout,&keepOnTop,&toolbar})if(c!=&toolbar)toolbar.addAndMakeVisible(c);
    toolbar.setSize(1500,60);int x=4;
    auto place=[&](juce::Component& c,int w){c.setBounds(x,4,w,40);x+=w+4;};
    for(int i=0;i<7;++i)style.addItem(toString((BoardStyle)i),i+1);style.setTitle("Board style");place(style,100);
    place(add,55);place(undo,50);place(redo,50);
    for(int i=0;i<4;++i)mode.addItem(toString((Mode)i),i+1);mode.setTitle("Routing mode");place(mode,90);place(enabled,95);
    mix.setSliderStyle(juce::Slider::LinearHorizontal);mix.setTextBoxStyle(juce::Slider::TextBoxRight,false,52,20);mix.setRange(0,1,.001);mix.setDoubleClickReturnValue(true,1);mix.setTitle("Board wet/dry mix");mix.setTooltip("Universal wet/dry mix. Double click: 100% wet.");mix.textFromValueFunction=[](double n){return juce::String((int)std::round(n*100))+"% wet";};place(mix,160);
    mixLaw.addItem("Equal power",1);mixLaw.addItem("Linear mix",2);mixLaw.addItem("Third law",3);mixLaw.setTitle("Mix law");place(mixLaw,105);
    tempo.setRange(20,300,.1);tempo.setSliderStyle(juce::Slider::LinearHorizontal);tempo.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,20);tempo.setTextValueSuffix(" BPM");tempo.setTitle("Board tempo BPM");place(tempo,120);
    place(learn,100);place(cables,75);place(fit,40);place(minus,28);place(zoomLabel,40);place(plus,28);place(settings,65);place(dock,45);place(popout,65);place(keepOnTop,110);toolbar.setSize(x,60);
    add.onClick=[this]{showAddMenu();};undo.onClick=[this]{model.undo();};redo.onClick=[this]{model.redo();};
    style.onChange=[this]{if(!refreshing)model.apply(cmd::SetStyle{(BoardStyle)(style.getSelectedId()-1)});};
    mode.onChange=[this]{if(!refreshing)model.apply(cmd::SetMode{(Mode)(mode.getSelectedId()-1)});};
    mixLaw.onChange=[this]{if(!refreshing)model.apply(cmd::SetMixLaw{(MixLaw)(mixLaw.getSelectedId()-1)});};
    mix.onDragStart=[this]
    {
        ++mixGesture;
        if(learn.getToggleState()&&processor)
        {
            learnBoard=true;learnId=InstanceId::null();learnParam=0;learnContinuous=true;learnStart=juce::Time::getMillisecondCounterHiRes();
            processor->getEngine().getReferencePedals().midi().armLearn({0xff,0,true});
            status.setText("Waiting for MIDI for board wet/dry mix; Escape cancels",juce::dontSendNotification);startTimerHz(10);
        }
    };mix.onValueChange=[this]{if(!refreshing&&!mixAttachment)model.apply(cmd::SetMix{(float)mix.getValue(),mixGesture});};
    enabled.onClick=[this]{if(!enabledAttachment)model.apply(cmd::SetBoardEnabled{enabled.getToggleState()});};
    tempo.onValueChange=[this]{if(!refreshing&&!tempoAttachment)model.apply(cmd::SetBoardParam{"tempoBpm",tempo.getValue()});};
    if(processor)
    {
        auto& state=processor->getState();
        if(state.getParameter("ref_board_mix"))mixAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,"ref_board_mix",mix);
        if(state.getParameter("ref_board_enabled"))enabledAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state,"ref_board_enabled",enabled);
        if(state.getParameter("ref_board_tempo_bpm"))tempoAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,"ref_board_tempo_bpm",tempo);
    }
    cables.onClick=[this]{canvas.setCables(cables.getToggleState());saveView();};
    fit.onClick=[this]{zoomTo(juce::jlimit(.5f,2.f,(float)viewport.getWidth()/canvas.getWidth()));};minus.onClick=[this]{zoomTo(zoom-.1f);};plus.onClick=[this]{zoomTo(zoom+.1f);};
    keepOnTop.onClick=[this]{if(auto* w=findParentComponentOfClass<juce::DocumentWindow>())w->setAlwaysOnTop(keepOnTop.getToggleState());};
    dock.onClick=[this]{if(onDock)onDock();};popout.onClick=[this]{if(onPopout)onPopout();};settings.onClick=[this]{showSettings();};
    learn.onClick=[this]{canvas.setLearn(learn.getToggleState());if(!learn.getToggleState()){learnId=InstanceId::null();learnBoard=false;if(!pendingView)stopTimer();if(processor)processor->getEngine().getReferencePedals().midi().cancelLearn();}status.setText(learn.getToggleState()?"Learn: click a control, then send MIDI CC / velocity / aftertouch / pitch bend":"MIDI learn cancelled",juce::dontSendNotification);};
    canvas.onLearn=[this](auto id,int param,bool continuous)
    {
        if(!processor){status.setText("MIDI input unavailable",juce::dontSendNotification);return;}
        const auto instance=model.getInstance(id);if(!instance)return;
        if(param<0){param=PedalRegistry::laneMap(instance->modelId.c_str()).engageParam;if(param<0){status.setText("This pedal has no learnable engage descriptor",juce::dontSendNotification);return;}}
        learnBoard=false;learnId=id;learnParam=param;learnContinuous=continuous;learnStart=juce::Time::getMillisecondCounterHiRes();
        processor->getEngine().getReferencePedals().midi().armLearn({(uint8_t)instance->slot,(uint16_t)param,continuous});
        status.setText("Waiting for MIDI for "+juce::String(instance->modelId)+"; Escape cancels",juce::dontSendNotification);startTimerHz(10);
    };
    canvas.onFocus=[this](PedalView& p){model.setMidiFocus(p.id());const auto r=p.getBounds();viewport.setViewPosition(juce::jmax(0,(int)(r.getX()*zoom)-24),juce::jmax(0,(int)(r.getY()*zoom)-24));};
    const auto ui=BoardUiState::read(model.getBoardSettings().ui);cables.setToggleState(ui.cables,juce::dontSendNotification);canvas.setCables(ui.cables);zoomTo(ui.zoom);viewport.setViewPosition(ui.scrollX,ui.scrollY);
    viewport.onScroll=[this]{saveView();};
    model.addListener(this);refresh();
}
BoardRoot::~BoardRoot(){canvas.closeInspectors();canvas.onFocus=nullptr;canvas.onLearn=nullptr;viewport.onScroll=nullptr;stopTimer();flushView();model.removeListener(this);toolbarViewport.setViewedComponent(nullptr,false);viewport.setViewedComponent(nullptr,false);}
void BoardRoot::refresh()
{
    const juce::ScopedValueSetter<bool> guard(refreshing,true);const auto s=model.getBoardSettings();
    style.setSelectedId((int)s.style+1,juce::dontSendNotification);mode.setSelectedId((int)s.mode+1,juce::dontSendNotification);mixLaw.setSelectedId((int)s.mixLaw+1,juce::dontSendNotification);
    if(!mixAttachment)mix.setValue(s.mix,juce::dontSendNotification);if(!enabledAttachment)enabled.setToggleState(s.enabled,juce::dontSendNotification);if(!tempoAttachment)tempo.setValue(s.tempoBpm,juce::dontSendNotification);
    undo.setEnabled(model.canUndo());redo.setEnabled(model.canRedo());browser.refresh();const auto info=model.getTopologyInfo();
    status.setText(juce::String(model.getInstanceCount())+" pedals · latency "+juce::String(info.latencyTotal)+" samples · "+info.note,juce::dontSendNotification);
}
void BoardRoot::saveView()
{
    pendingView=true;viewChangedAt=juce::Time::getMillisecondCounterHiRes();
    if(isShowing())startTimerHz(10);else flushView();
}
void BoardRoot::flushView()
{
    pendingView=false;
    auto s=BoardUiState::read(model.getBoardSettings().ui);s.zoom=zoom;s.scrollX=viewport.getViewPositionX();s.scrollY=viewport.getViewPositionY();s.cables=cables.getToggleState();
    model.setUiState(s.write(model.getBoardSettings().ui));
}
void BoardRoot::zoomTo(float z){zoom=juce::jlimit(.5f,2.f,z);canvas.setTransform(juce::AffineTransform::scale(zoom));zoomLabel.setText(juce::String((int)(zoom*100))+"%",juce::dontSendNotification);saveView();}
void BoardRoot::setCompact(bool b){compact=b;resized();}
void BoardRoot::resized()
{
    auto r=getLocalBounds();toolbarViewport.setBounds(r.removeFromTop(64));status.setVisible(getHeight()>=360&&!compact);status.setBounds(status.isVisible()?r.removeFromBottom(20):juce::Rectangle<int>());
    const bool showBrowser=!compact&&getWidth()>=760;browser.setVisible(showBrowser);if(showBrowser)browser.setBounds(r.removeFromLeft(200));viewport.setBounds(r);
}
void BoardRoot::paint(juce::Graphics& g){AnimationPolicy::notePaint(*this);g.fillAll(Palette::backgroundDeep);}
void BoardRoot::showAddMenu()
{
    juce::PopupMenu menu;std::vector<std::string> ids;for(const auto* d:PedalRegistry::all()){ids.emplace_back(d->modelId);menu.addItem((int)ids.size(),d->displayName,model.getInstanceCount()<kMaxSlots);}
    juce::Component::SafePointer<BoardRoot> self(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&add),[self,ids](int n){if(self&&n>0&&n<=(int)ids.size())self->model.apply(cmd::Add{ids[(size_t)n-1]});});
}
void BoardRoot::showSettings(){juce::CallOutBox::launchAsynchronously(std::make_unique<BoardMidiPanel>(model),getLocalArea(&settings,settings.getLocalBounds()),this);}
void BoardRoot::visibilityChanged(){if(!isShowing()){canvas.closeInspectors();stopTimer();if(processor)processor->getEngine().getReferencePedals().midi().cancelLearn();learnId=InstanceId::null();learnBoard=false;learn.setToggleState(false,juce::dontSendNotification);canvas.setLearn(false);saveView();}}
void BoardRoot::timerCallback()
{
    const auto now=juce::Time::getMillisecondCounterHiRes();
    if(pendingView&&now-viewChangedAt>=500)flushView();
    if(!processor||(!learnBoard&&learnId.isNull())){if(!pendingView)stopTimer();return;}
    if(juce::Time::getMillisecondCounterHiRes()-learnStart>20000){processor->getEngine().getReferencePedals().midi().cancelLearn();learnId=InstanceId::null();learnBoard=false;status.setText("MIDI learn timed out",juce::dontSendNotification);if(!pendingView)stopTimer();return;}
    midi::LearnCapture capture;if(processor->getEngine().getReferencePedals().midi().pollLearn(capture)&&capture.valid)
    {
        auto binding=new juce::DynamicObject();binding->setProperty("instanceId",learnId.toString());const auto instance=model.getInstance(learnId);const auto* desc=instance?PedalRegistry::find(instance->modelId.c_str()):nullptr;
        if(!learnBoard&&(!desc||learnParam<0||learnParam>=desc->numParams)){if(!pendingView)stopTimer();learnId=InstanceId::null();learnBoard=false;return;}
        int nextId=1;const auto currentMidi=model.getBoardSettings().midi;
        if(const auto* bindings=currentMidi.getProperty("bindings",juce::var()).getArray())for(const auto& b:*bindings)nextId=juce::jmax(nextId,1+(int)b.getProperty("id",0));
        if(nextId>65535){status.setText("MIDI mapping capacity reached",juce::dontSendNotification);if(!pendingView)stopTimer();learnId=InstanceId::null();learnBoard=false;return;}
        binding->setProperty("id",nextId);binding->setProperty("instanceId",learnBoard?juce::String("board"):learnId.toString());binding->setProperty("paramId",learnBoard?"ref_board_mix":desc->params[learnParam].id);binding->setProperty("chan",capture.chan);
        auto* src=new juce::DynamicObject();src->setProperty("type",(int)capture.type);src->setProperty("num",capture.num);binding->setProperty("src",juce::var(src));
        const auto result=model.apply(cmd::SetMidiBinding{cmd::SetMidiBinding::Op::Add,juce::var(binding)});learnId=InstanceId::null();learnBoard=false;if(!pendingView)stopTimer();if(result)status.setText("MIDI mapping captured",juce::dontSendNotification);
    }
}
bool BoardRoot::keyPressed(const juce::KeyPress& key)
{
    const auto mods=key.getModifiers();const int c=key.getKeyCode();
    if(mods.isCommandDown())
    {
        if(mods.isShiftDown()&&(c=='B'||c=='b')){if(onToggle)onToggle();return true;}
        if(c=='Z'||c=='z'){if(mods.isShiftDown())model.redo();else model.undo();return true;}if(c=='Y'||c=='y'){model.redo();return true;}
        if(c=='F'||c=='f'){if(browser.isVisible())browser.focusSearch();else showAddMenu();return true;}
        if(c=='L'||c=='l'){learn.triggerClick();return true;}if(c=='0'){fit.triggerClick();return true;}if(c=='='||c=='+'){zoomTo(zoom+.1f);return true;}if(c=='-'){zoomTo(zoom-.1f);return true;}
    }
    if(c==juce::KeyPress::escapeKey)
    {
        if(!learnId.isNull()||learnBoard)
        {if(processor)processor->getEngine().getReferencePedals().midi().cancelLearn();learnId=InstanceId::null();learnBoard=false;if(!pendingView)stopTimer();status.setText("Learn: select another control; Escape exits",juce::dontSendNotification);return true;}
        if(learn.getToggleState()){learn.setToggleState(false,juce::dontSendNotification);canvas.setLearn(false);return true;}
        if(onReturnFocus)onReturnFocus();return true;
    }
    return false;
}
}
