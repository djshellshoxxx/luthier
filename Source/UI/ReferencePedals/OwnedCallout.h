// OWNER: WP17 / WP22
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace luthier::reference
{
/** A nonmodal child popup whose content cannot outlive its owning board. */
class OwnedCallout final : private juce::MouseListener
{
public:
    ~OwnedCallout(){close();}
    void open(std::unique_ptr<juce::Component>,juce::Component& parent,juce::Component& anchor);
    void close(bool restoreFocus=false);
    bool isOpen() const {return box!=nullptr;}
private:
    void mouseDown(const juce::MouseEvent&) override;
    juce::Component::SafePointer<juce::Component> anchor;
    std::unique_ptr<juce::Component> content;
    std::unique_ptr<juce::CallOutBox> box;
};
}
