/*  The editor's TUNE-tab entry points (tune-builder.md 2; gui-integration 17's
    "New tune  Ctrl+T"). TUNE-HELP-ONBOARDING workstream. The tab answers
    Ctrl+T itself while it has focus; this is the same key from anywhere else.
*/

#include "PluginEditor.h"

namespace luthier
{

TunePanel* LuthierAudioProcessorEditor::openNewTune()
{
    // The TUNE tab lives in Advanced mode's workspace.
    if (! advancedMode)
    {
        if (! isAdvancedModeAvailable())
        {
            notifications.post ({ "new-tune", advancedUnavailableMessage(), Notification::Level::info });
            return nullptr;
        }

        setAdvancedMode (true);
        header.setAdvancedMode (advancedMode);
    }

    if (! advancedPanel.setWorkspaceTabNamed ("TUNE"))
        return nullptr;

    auto* tune = dynamic_cast<TunePanel*> (advancedPanel.getWorkspacePanel (advancedPanel.getWorkspaceTab()));

    if (tune != nullptr && isShowing())
    {
        tune->grabKeyboardFocus();
        tune->showNewTuneMenu();
    }

    return tune;
}

} // namespace luthier
