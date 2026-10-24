// OWNER: WP17
#pragma once
#include "PedalView.h"
#include "DragController.h"
#include "../AnimationPolicy.h"
#include <map>
namespace luthier::reference
{
class BoardCanvas final : public juce::Component, public juce::DragAndDropTarget, private BoardModel::Listener, private juce::Timer
{
public:
    explicit BoardCanvas (BoardModel&, juce::AudioProcessorValueTreeState* = nullptr);
    ~BoardCanvas() override;
    PedalView* viewFor (const InstanceId&) const;
    void paint (juce::Graphics&) override;
    void paintCables (juce::Graphics&);
    bool cancelDrag();
    bool dragActive() const {return drag.active();}
    bool keyPressed(const juce::KeyPress&) override;
    void visibilityChanged() override;
    void refresh();
    void closeInspectors();
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void setCables (bool b) { cables=b;repaint(); }
    void setLearn (bool b);
    std::function<void(InstanceId,int,bool)> onLearn;
    std::function<void(PedalView&)> onFocus;
    std::function<void(const juce::String&)> onError;
private:
    void dragTo(PedalView&,juce::Point<int>,juce::Point<int>,bool);
    DragController::Target dragTarget(juce::Point<int>) const;
    void finishVisualDrag();
    void timerCallback() override;
    void orderChanged() override { refresh(); }
    void instanceChanged (const InstanceId&) override;
    void settingsChanged() override;
    void styleChanged() override { refresh(); }
    void rejected(const juce::String&) override { refresh(); }
    BoardModel& model;
    juce::AudioProcessorValueTreeState* parameters;
    std::map<juce::String,std::unique_ptr<PedalView>> views;
    struct CableLayer final : juce::Component {explicit CableLayer(BoardCanvas& p):owner(p){setInterceptsMouseClicks(false,false);}void paint(juce::Graphics& g) override{owner.paintCables(g);}BoardCanvas& owner;} cableLayer {*this};
    struct Ghost final : juce::Component {juce::Image image;void paint(juce::Graphics&) override;};
    DragController drag;
    std::unique_ptr<Ghost> ghost;
    juce::Point<int> dragPointer,dragOffset;
    AnimationPolicy::Registration paintMetrics {*this,AnimationPolicy::Decorative,"ReferenceBoardCanvas"};
    juce::String layoutSignature;
    bool cables=true, learning=false;
};
}
