#include "ReferencePedalDrawer.h"
#include "../PluginProcessor.h"
#include "Theme.h"
namespace luthier::reference
{
Drawer::Drawer(LuthierAudioProcessor& p):processor(p),root(p.getEngine().getReferencePedals().model(),&p)
{
    const auto ui=BoardUiState::read(p.getEngine().getReferencePedals().model().getBoardSettings().ui);height=ui.dockHeight;
    addAndMakeVisible(root);root.setCompact(true);setVisible(false);setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}
Drawer::~Drawer()=default;
void Drawer::attachBoard(){addAndMakeVisible(root);root.setCompact(true);resized();}
void Drawer::setOpen(bool b) noexcept
{
    open=b;setVisible(b);auto& m=processor.getEngine().getReferencePedals().model();auto ui=BoardUiState::read(m.getBoardSettings().ui);ui.dockOpen=b;ui.dockHeight=height;m.setUiState(ui.write(m.getBoardSettings().ui));
    if(onHeightChanged)onHeightChanged();
}
void Drawer::refresh(){root.boardCanvas().refresh();}
void Drawer::paint(juce::Graphics& g){g.fillAll(Palette::backgroundDeep);g.setColour(Palette::edgeBright);g.fillRect(0,0,getWidth(),4);}
void Drawer::resized(){if(root.getParentComponent()==this)root.setBounds(getLocalBounds().withTrimmedTop(6));}
void Drawer::mouseDown(const juce::MouseEvent& e){if(e.y<8)dragHeight=height;}
void Drawer::mouseDrag(const juce::MouseEvent& e){if(e.getMouseDownY()<8){height=juce::jlimit(160,420,dragHeight-e.getDistanceFromDragStartY());setOpen(open);}}
}
