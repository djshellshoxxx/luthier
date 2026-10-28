// OWNER: WP17 / WP18 / WP22
#include "ComboHarness.h"
#include "RefPedalUiTestSupport.h"
#include "../PluginEditor.h"
#include "../UI/ReferencePedalDrawer.h"
#include "../UI/ReferencePedals/BoardWindow.h"
#include "../UI/ReferencePedals/KnobControl.h"
#include <array>
#include <set>

#if JUCE_MAC
 #include <CoreFoundation/CoreFoundation.h>
#else
 namespace juce::detail { bool dispatchNextMessageOnSystemQueue(bool); }
#endif

namespace
{
using namespace luthier;
using namespace luthier::reference;

template<class T> T* uiChild(juce::Component& root,const std::function<bool(T&)>& predicate=[](T&){return true;})
{
    if(auto* value=dynamic_cast<T*>(&root))if(predicate(*value))return value;
    for(auto* child:root.getChildren())if(auto* value=uiChild<T>(*child,predicate))return value;
    return nullptr;
}
struct HostGestures final : juce::AudioProcessorParameter::Listener
{
    explicit HostGestures(juce::AudioProcessor& value):processor(value),depth((size_t)value.getParameters().size())
    {
        for(auto& entry:depth)entry.store(0);
        for(auto* parameter:processor.getParameters())parameter->addListener(this);
    }
    ~HostGestures() override {for(auto* parameter:processor.getParameters())parameter->removeListener(this);}
    void parameterValueChanged(int,float) override {values.fetch_add(1);}
    void parameterGestureChanged(int index,bool begin) override
    {
        if(index<0||index>=(int)depth.size()){errors.fetch_add(1);return;}
        const int prior=depth[(size_t)index].fetch_add(begin?1:-1);
        if(prior!=(begin?0:1))errors.fetch_add(1);
        gestures.fetch_add(1);
    }
    bool balanced() const {for(const auto& entry:depth)if(entry.load()!=0)return false;return errors.load()==0;}
    juce::AudioProcessor& processor;std::vector<std::atomic<int>> depth;
    std::atomic<int> values{0},gestures{0},errors{0};
};
void drainUiMessages()
{
    // Bounded native dispatch: no sleeping, host simulation or fake callbacks.
    for(int message=0;message<128;++message)
    {
       #if JUCE_MAC
        if(CFRunLoopRunInMode(kCFRunLoopDefaultMode,0,true)!=kCFRunLoopRunHandledSource)break;
       #else
        if(!juce::detail::dispatchNextMessageOnSystemQueue(true))break;
       #endif
    }
}
}

LUTHIER_TEST(RefPedalUiLifecycle, tenThousandNativeOperationsAndEditorDestruction)
{
    using namespace luthier;using namespace luthier::reference;
    constexpr uint32_t initialSeed=0x55494235;uint32_t seed=initialSeed;
    auto next=[&]{seed=1664525u*seed+1013904223u;return seed;};
    combo::Rig rig;auto& model=rig.p().getEngine().getReferencePedals().model();
    for(const auto& id:model.getOrder())if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Remove{id}))return;
    for(int i=0;i<4;++i)if(!luthier::tests::requireReferenceEdit(ctx,model,cmd::Add{"generic.boost"}))return;
    struct Rejections final : BoardModel::Listener
    {
        explicit Rejections(BoardModel& value):owner(value){}
        ~Rejections() override {owner.removeListener(this);}
        BoardModel& owner;
        void rejected(const juce::String& reason) override {++count;last=reason;if(onRejected)onRejected(reason);}
        int count=0;juce::String last;std::function<void(const juce::String&)> onRejected;
    } rejections(model);
    HostGestures hostGestures(rig.p());
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::Component::SafePointer<BoardRoot> root;
    std::array<int,19> operations{};int editorsCreated=0,editorsDestroyed=0,nativeCloses=0,dragCancels=0,acceptedDrops=0,changedDrops=0;
    auto createEditor=[&]()
    {
        editor.reset(rig.p().createEditor());CHECK(editor!=nullptr);if(!editor)return false;
        editor->setSize(1600,1000);editor->addToDesktop(0);editor->setVisible(true);
        root=uiChild<BoardRoot>(*editor);CHECK(root!=nullptr);if(!root)return false;
        drainUiMessages();CHECK(editor->getPeer()!=nullptr);if(!editor->getPeer())return false;
        CHECK((bool)root->onDock);if(!root->onDock)return false;root->onDock();
        CHECK(root->findParentComponentOfClass<Drawer>()!=nullptr);if(!root->findParentComponentOfClass<Drawer>())return false;
        rejections.onRejected=[&](const juce::String& reason){CHECK(root!=nullptr);if(root)CHECK(root->statusText()==reason);};
        model.addListener(&rejections);++editorsCreated;return true;
    };
    auto destroyEditor=[&]()
    {
        juce::Component::SafePointer<BoardWindow> window=root?root->findParentComponentOfClass<BoardWindow>():nullptr;
        model.removeListener(&rejections);rejections.onRejected=nullptr;
        auto priorRoot=root;editor.reset();++editorsDestroyed;drainUiMessages();
        CHECK(priorRoot==nullptr);CHECK(window==nullptr);root=nullptr;
    };
    if(!createEditor())return;
    const auto started=juce::Time::getMillisecondCounterHiRes();
    std::cout<<"Native UI stress seed=0x55494235 target=10000; real processor/editor/peers, unattended native display"<<std::endl;
    for(int operation=0;operation<10000;++operation)
    {
        const int failuresBefore=ctx.failures;
        const int kind=operation%500==499?18:operation<18?operation:(int)((next()>>16)%18);
        // "context:" lines are kept in the CI failure recap (scripts/ci_build.*):
        // native window geometry and the last model rejection explain platform-
        // specific dock/duplicate outcomes without the full log.
        const auto describe=[&]
        {
            juce::String text;
            if(editor){text<<" editor "<<editor->getBounds().toString();if(auto* peer=editor->getPeer())text<<" peer "<<peer->getBounds().toString();}
            if(auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())text<<" display "<<display->userArea.toString()<<" scale "<<display->scale;
            if(root){auto* parent=root->getParentComponent();text<<" rootParent "<<(root->findParentComponentOfClass<Drawer>()?"Drawer":root->findParentComponentOfClass<BoardWindow>()?"BoardWindow":parent?"other":"none")<<" status '"<<root->statusText()<<"'";}
            text<<" instances "<<model.getInstanceCount()<<" rejections "<<rejections.count<<" last '"<<rejections.last<<"'";
            return text;
        };
        struct FailureContext
        {
            luthier::tests::TestContext& context;int failuresBefore,operation,kind;uint32_t seed;std::function<juce::String()> describe;
            ~FailureContext(){if(context.failures!=failuresBefore)context.failureMessages.add("    context: Native UI operation "+juce::String(operation)+" kind "+juce::String(kind)+" initial seed0x55494235 current seed "+juce::String::toHexString((juce::int64)seed)+describe());}
        } failureContext{ctx,failuresBefore,operation,kind,seed,describe};
        CHECK(root!=nullptr);if(!root)return;
        auto order=model.getOrder();CHECK(!order.empty());if(order.empty())return;
        const auto id=order[(next()>>16)%order.size()];auto* view=root->boardCanvas().viewFor(id);
        CHECK(view!=nullptr);if(!view)return;
        if(kind==2||kind==3||kind==4||kind==6||kind==15||kind==17){view->grabKeyboardFocus();drainUiMessages();order=model.getOrder();}
        switch(kind)
        {
            case 0: // Native rotary keyboard path, including its APVTS gesture.
            {
                root->boardCanvas().closeInspectors();root->keyPressed({juce::KeyPress::escapeKey,juce::ModifierKeys(),0});
                auto* knob=uiChild<KnobControl>(*view,[view](auto& value){return value.getParentComponent()==view;});CHECK(knob!=nullptr);if(!knob)return;
                knob->grabKeyboardFocus();drainUiMessages();
                const auto before=knob->getValue();const int direction=before>.8?juce::KeyPress::downKey:juce::KeyPress::upKey;
                CHECK(knob->keyPressed({direction,juce::ModifierKeys(),0}));CHECK(knob->getValue()!=before);
                if(auto* parameter=rig.state().getParameter(knob->getComponentID()))CHECK_NEAR(parameter->getValue(),knob->getValue(),.001);
                break;
            }
            case 1: // Natural-unit editor is the actual nonmodal control flow.
            {
                root->boardCanvas().closeInspectors();root->keyPressed({juce::KeyPress::escapeKey,juce::ModifierKeys(),0});
                auto* knob=uiChild<KnobControl>(*view,[view](auto& value){return value.getParentComponent()==view;});CHECK(knob!=nullptr);if(!knob)return;
                knob->grabKeyboardFocus();drainUiMessages();knob->showValueEditor();auto* entry=uiChild<juce::TextEditor>(*knob->getTopLevelComponent(),[](auto& value){return value.getTitle().contains("natural value");});
                CHECK(entry!=nullptr);if(!entry)return;entry->setText("12 dB",false);CHECK((bool)entry->onReturnKey);if(!entry->onReturnKey)return;entry->onReturnKey();
                CHECK(juce::Component::getCurrentlyModalComponent()==nullptr);break;
            }
            case 2: CHECK(view->keyPressed({juce::KeyPress::spaceKey,juce::ModifierKeys(),0}));break;
            case 3: CHECK(view->keyPressed({juce::KeyPress::rightKey,juce::ModifierKeys(juce::ModifierKeys::altModifier),0}));break;
            case 4: CHECK(view->keyPressed({juce::KeyPress::leftKey,juce::ModifierKeys(juce::ModifierKeys::altModifier|juce::ModifierKeys::shiftModifier),0}));break;
            case 5:
            {
                auto* style=uiChild<juce::ComboBox>(*root,[](auto& value){return value.getTitle()=="Board style";});CHECK(style!=nullptr);if(!style)return;
                const int selected=1+(style->getSelectedId()%7);style->setSelectedId(selected,juce::sendNotificationSync);CHECK((int)model.getBoardSettings().style==selected-1);break;
            }
            case 6: CHECK(view->keyPressed({'I',juce::ModifierKeys(),0}));break;
            case 7: root->boardCanvas().closeInspectors();root->openSettings();CHECK(juce::Component::getCurrentlyModalComponent()==nullptr);break;
            case 8:
            {
                root->openSettings();auto* gain=uiChild<juce::Slider>(*root,[](auto& value){return value.getComponentID()=="ref_board_fb_gain";});CHECK(gain!=nullptr);if(!gain)return;
                gain->grabKeyboardFocus();drainUiMessages();
                {juce::Slider::ScopedDragNotification gesture(*gain);gain->setValue(gain->getValue()>.5?.2:.7,juce::sendNotificationSync);}
                CHECK_NEAR(rig.state().getRawParameterValue("ref_board_fb_gain")->load(),gain->getValue(),.001);break;
            }
            case 9: root->openSettings();CHECK(root->keyPressed({juce::KeyPress::escapeKey,juce::ModifierKeys(),0}));break;
            case 10: root->onDock();CHECK(root->findParentComponentOfClass<Drawer>()!=nullptr);break;
            case 11: root->onPopout();CHECK(root->findParentComponentOfClass<BoardWindow>()!=nullptr);break;
            case 12:
            {
                root->onPopout();auto* window=root->findParentComponentOfClass<BoardWindow>();CHECK(window!=nullptr);if(!window)return;
                CHECK(window->getPeer()!=nullptr);window->closeButtonPressed();++nativeCloses;CHECK(!window->isShowing());root->onDock();break;
            }
            case 13: editor->setSize((next()&1)?1280:1600,(next()&1)?900:1000);break;
            case 14: // Actual browser Add button, never a test-only model alias.
            {
                root->onPopout();auto* browser=uiChild<PedalBrowser>(*root);CHECK(browser!=nullptr);if(!browser)return;
                auto* search=uiChild<juce::TextEditor>(*browser);auto* add=uiChild<juce::TextButton>(*browser);
                CHECK(search&&add);if(!search||!add)return;
                if(order.size()==kMaxSlots){CHECK(view->keyPressed({juce::KeyPress::deleteKey,juce::ModifierKeys(),0}));drainUiMessages();}
                search->setText("boost",false);if(search->onTextChange)search->onTextChange();
                const int before=model.getInstanceCount();CHECK(add->isEnabled());CHECK((bool)add->onClick);if(!add->isEnabled()||!add->onClick)return;add->onClick();CHECK(model.getInstanceCount()==before+1);break;
            }
            case 15:
                if(order.size()>1)CHECK(view->keyPressed({juce::KeyPress::deleteKey,juce::ModifierKeys(),0}));
                else CHECK(view->keyPressed({'D',juce::ModifierKeys(juce::ModifierKeys::commandModifier),0}));
                break;
            case 16: // Real Canvas drag/drop or cancellation, with no artificial timer wait.
            {
                root->boardCanvas().closeInspectors();root->keyPressed({juce::KeyPress::escapeKey,juce::ModifierKeys(),0});
                auto* viewport=root->boardCanvas().findParentComponentOfClass<juce::Viewport>();CHECK(viewport!=nullptr);if(!viewport)return;viewport->setViewPosition(0,0);
                view=root->boardCanvas().viewFor(order[(next()>>16)%juce::jmin((size_t)2,order.size())]);CHECK(view!=nullptr);if(!view)return;
                const bool first=view->id()==order.front();const auto destinationId=first&&order.size()>1?order[1]:order.front();
                auto* destination=root->boardCanvas().viewFor(destinationId);CHECK(destination!=nullptr);if(!destination)return;
                CHECK((bool)view->onDrag);if(!view->onDrag)return;
                const auto target=destination->getBounds().getCentre()+juce::Point<int>(first?1:-1,0);view->onDrag(*view,target,{20,20},false);
                CHECK(root->boardCanvas().dragActive());
                // The real host begin callback can settle older position intent.
                // This is the authoritative transaction baseline for rollback.
                const auto before=model.getOrder();const bool nowFirst=view->id()==before.front();
                auto* dropDestination=root->boardCanvas().viewFor(nowFirst&&before.size()>1?before[1]:before.front());CHECK(dropDestination!=nullptr);if(!dropDestination)return;
                viewport->setViewPosition(0,0);const auto dropTarget=dropDestination->getBounds().getCentre()+juce::Point<int>(nowFirst?1:-1,0);
                if(next()&1){CHECK(root->boardCanvas().keyPressed({juce::KeyPress::escapeKey,juce::ModifierKeys(),0}));CHECK(model.getOrder()==before);++dragCancels;}
                else{view->onDrag(*view,dropTarget,{20,20},true);++acceptedDrops;if(before.size()>1){CHECK(model.getOrder()!=before);if(model.getOrder()!=before)++changedDrops;}}
                CHECK(!root->boardCanvas().dragActive());break;
            }
            case 17: // Reach the real capacity rejection by keyboard duplication.
            {
                for(int attempt=0;attempt<kMaxSlots&&model.getInstanceCount()<kMaxSlots;++attempt){const int count=model.getInstanceCount();CHECK(view->keyPressed({'D',juce::ModifierKeys(juce::ModifierKeys::commandModifier),0}));drainUiMessages();CHECK(model.getInstanceCount()==count+1);if(model.getInstanceCount()!=count+1)return;}
                CHECK(model.getInstanceCount()==kMaxSlots);if(model.getInstanceCount()!=kMaxSlots)return;
                // State capture settles earlier real host position intent before the isolated rejection.
                juce::MemoryBlock settledState;rig.p().getStateInformation(settledState);CHECK(settledState.getSize()>0);
                const auto before=model.getOrder();const int rejectedBefore=rejections.count;CHECK(view->keyPressed({'D',juce::ModifierKeys(juce::ModifierKeys::commandModifier),0}));drainUiMessages();CHECK(model.getOrder()==before);CHECK(rejections.count==rejectedBefore+1);CHECK(rejections.last.containsIgnoreCase("invalid"));break;
            }
            case 18:
                if(editorsDestroyed%2==0)root->onPopout();else root->onDock();
                destroyEditor();if(!createEditor())return;break;
        }
        drainUiMessages();if(operation%8==0)rig.processSilence(1);
        ++operations[(size_t)kind];order=model.getOrder();std::set<juce::String> identities;
        CHECK(order.size()>=1&&order.size()<=kMaxSlots);
        for(const auto& current:order){CHECK(identities.insert(current.toString()).second);CHECK(root->boardCanvas().viewFor(current)!=nullptr);}
        CHECK(root->boardCanvas().getParentComponent()!=nullptr);CHECK(juce::Component::getCurrentlyModalComponent()==nullptr);
        if(ctx.failures!=failuresBefore)return;
        if(operation%1000==999)std::cout<<"Native UI completed "<<operation+1<<" operations, editors "<<editorsCreated<<"/"<<editorsDestroyed<<", closes "<<nativeCloses<<std::endl;
    }
    destroyEditor();CHECK(editorsCreated==21);CHECK(editorsDestroyed==21);CHECK(nativeCloses>=300);CHECK(dragCancels>=100);CHECK(acceptedDrops>=100);CHECK(changedDrops>=100);
    CHECK(hostGestures.balanced());CHECK(hostGestures.gestures.load()>0);CHECK(hostGestures.values.load()>0);
    int completed=0;for(size_t kind=0;kind<operations.size();++kind){CHECK(operations[kind]>=(kind==18?20:300));completed+=operations[kind];}CHECK(completed==10000);
    std::cout<<"Native UI operations="<<completed<<" seed=0x55494235 elapsedMs="<<juce::Time::getMillisecondCounterHiRes()-started<<" editorDestructions="<<editorsDestroyed<<" nativeCloses="<<nativeCloses<<" dragCancels="<<dragCancels<<" acceptedDrops="<<acceptedDrops<<" changedDrops="<<changedDrops<<" hostGestures="<<hostGestures.gestures.load()<<" hostValues="<<hostGestures.values.load()<<" kinds=";
    constexpr const char* labels[]{"knob","naturalValue","bypass","keyboardReorder","placement","style","inspector","settingsOpen","settingsGain","settingsClose","dock","popout","nativeClose","resize","browserAdd","removeOrDuplicate","drag","capacityError","editorDestroy"};
    for(size_t kind=0;kind<operations.size();++kind)std::cout<<labels[kind]<<"="<<operations[kind]<<",";std::cout<<std::endl;
}
