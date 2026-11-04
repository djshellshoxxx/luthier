// OWNER: WP17 / WP22
#include "OwnedCallout.h"
namespace luthier::reference
{
void OwnedCallout::open(std::unique_ptr<juce::Component> body,juce::Component& parent,juce::Component& target)
{
    close();anchor=&target;content=std::move(body);content->setSize(juce::jmin(content->getWidth(),juce::jmax(160,parent.getWidth()-40)),juce::jmin(content->getHeight(),juce::jmax(100,parent.getHeight()-40)));
    box=std::make_unique<juce::CallOutBox>(*content,parent.getLocalArea(&target,target.getLocalBounds()),&parent);box->setVisible(true);box->toFront(false);juce::Desktop::getInstance().addGlobalMouseListener(this);if(content->getWantsKeyboardFocus())content->grabKeyboardFocus();
}
void OwnedCallout::close(bool restoreFocus)
{
    if(!box)return;juce::Desktop::getInstance().removeGlobalMouseListener(this);box.reset();content.reset();if(restoreFocus&&anchor&&anchor->isShowing())anchor->grabKeyboardFocus();anchor=nullptr;
}
void OwnedCallout::mouseDown(const juce::MouseEvent& event)
{
    // A ComboBox's native popup is an independent modal child. Let it finish
    // selecting before deciding whether a later click is outside our content.
    if(!box||juce::Component::getCurrentlyModalComponent())return;
    auto* target=event.originalComponent;if(target==box.get()||box->isParentOf(target)||target==anchor.getComponent()||(anchor&&anchor->isParentOf(target)))return;close();
}
}
