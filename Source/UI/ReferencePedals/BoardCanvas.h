// OWNER: WP17
#pragma once
#include "PedalView.h"
#include <map>
namespace luthier::reference
{
class BoardCanvas final : public juce::Component, public juce::DragAndDropTarget, private BoardModel::Listener
{
public:
    explicit BoardCanvas (BoardModel&);
    ~BoardCanvas() override;
    PedalView* viewFor (const InstanceId&) const;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void refresh();
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void setCables (bool b) { cables=b;repaint(); }
    void setLearn (bool b);
    std::function<void(InstanceId,int,bool)> onLearn;
    std::function<void(PedalView&)> onFocus;
private:
    void orderChanged() override { refresh(); }
    void instanceChanged (const InstanceId&) override { refresh(); }
    void settingsChanged() override { refresh(); }
    void styleChanged() override { refresh(); }
    BoardModel& model;
    std::map<juce::String,std::unique_ptr<PedalView>> views;
    bool cables=true, learning=false;
};
}
