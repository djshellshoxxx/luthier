// OWNER: WP17
#include "BoardWindow.h"
#include "BoardRoot.h"
#include "BoardUiState.h"
#include "../Theme.h"
namespace luthier::reference
{
BoardWindow::BoardWindow(BoardModel& m,LuthierAudioProcessor& p,juce::Component* anchor)
    :juce::DocumentWindow("Pedalboard",Palette::backgroundDeep,juce::DocumentWindow::closeButton),boardModel(m),audioProcessor(p),anchorComponent(anchor)
{
    setUsingNativeTitleBar(true);setResizable(true,true);setResizeLimits(480,260,4096,2160);setAlwaysOnTop(false);setWantsKeyboardFocus(false);
    const auto state=BoardUiState::read(m.getBoardSettings().ui);
    auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(state.window);
    if(!display&&anchor)display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(anchor->getScreenBounds());
    setBounds(BoardUiState::clampWindow(state.window,display?display->userArea:juce::Rectangle<int>(0,0,1280,800)));restoring=false;
}
BoardWindow::~BoardWindow(){saveGeometry();clearContentComponent();}
void BoardWindow::setBoardContent(BoardRoot& root){setContentNonOwned(&root,false);root.setCompact(false);}
void BoardWindow::showWindow(){if(!getContentComponent())forceHeadlessContent();setVisible(true);toFront(false);saveGeometry();if(onVisibilityChanged)onVisibilityChanged(true);}
void BoardWindow::hideWindow(){saveGeometry();setVisible(false);auto s=BoardUiState::read(boardModel.getBoardSettings().ui);s.popoutOpen=false;boardModel.setUiState(s.write(boardModel.getBoardSettings().ui));if(onVisibilityChanged)onVisibilityChanged(false);}
bool BoardWindow::isShowing() const{return isVisible();}
void BoardWindow::forceHeadlessContent(){if(!getContentComponent()){headlessContent=std::make_unique<BoardRoot>(boardModel,&audioProcessor);setContentNonOwned(headlessContent.get(),false);}}
void BoardWindow::closeButtonPressed(){hideWindow();}
void BoardWindow::moved(){saveGeometry();}
void BoardWindow::resized(){juce::DocumentWindow::resized();saveGeometry();}
void BoardWindow::saveGeometry()
{
    if(restoring)return;auto s=BoardUiState::read(boardModel.getBoardSettings().ui);s.window=getBounds();s.popoutOpen=isVisible();boardModel.setUiState(s.write(boardModel.getBoardSettings().ui));
}
}
