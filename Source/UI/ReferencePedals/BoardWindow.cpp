// OWNER: WP17
// STUB (WP1): the interface is frozen in BoardWindow.h. WP17 replaces the
// placeholder content with BoardRoot, persists geometry and adds the dock
// fallback. Constructing a BoardWindow does not show it.

#include "BoardWindow.h"
#include "../../ReferencePedals/BoardModel.h"

namespace luthier::reference
{

BoardWindow::BoardWindow (BoardModel& model, LuthierAudioProcessor& processor, juce::Component* anchor)
    : juce::DocumentWindow ("Pedalboard", juce::Colour (0xff15171a), juce::DocumentWindow::closeButton),
      boardModel (model),
      audioProcessor (processor),
      anchorComponent (anchor)
{
    setResizable (true, true);
    setResizeLimits (480, 260, 4096, 2160);
    setAlwaysOnTop (false);
    juce::ignoreUnused (boardModel, audioProcessor);   // WP17 binds the content to them
}

BoardWindow::~BoardWindow() = default;

void BoardWindow::showWindow()
{
    setVisible (true);
    if (onVisibilityChanged)
        onVisibilityChanged (true);
}

void BoardWindow::hideWindow()
{
    setVisible (false);
    if (onVisibilityChanged)
        onVisibilityChanged (false);
}

bool BoardWindow::isShowing() const { return isVisible(); }

void BoardWindow::forceHeadlessContent()
{
    if (getContentComponent() == nullptr)
    {
        auto* content = new juce::Component();
        content->setSize (1100, 520);
        setContentOwned (content, true);
    }
}

void BoardWindow::closeButtonPressed() { hideWindow(); }

} // namespace luthier::reference
