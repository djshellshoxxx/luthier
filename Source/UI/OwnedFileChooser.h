#pragma once

/*  A file chooser that dies with the component that opened it.

    `std::make_shared<FileChooser>` captured in its own `launchAsync` callback
    keeps the chooser alive after its owner has gone: the window closes (or the
    host removes the plugin) while the dialog is open, and the callback then
    runs on a destroyed component and processor (CODEX_COMPLETENESS_LEDGER, MIDI
    import chooser lifetime). Here the owner holds the chooser, so destroying the
    owner destroys the chooser, and JUCE's FileChooser destructor drops the
    callback without calling it. The callback is also guarded by a SafePointer
    for the window between the component's destruction and the member's.

    Message thread only.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

namespace luthier
{

class OwnedFileChooser
{
public:
    /** Tests switch this off so a launched chooser is JUCE's own dialog, not a
        zenity/kdialog process. */
    static bool& useNativeDialogs() noexcept
    {
        static bool useNative = true;
        return useNative;
    }

    /*  Opens a chooser for `owner`, replacing (and so cancelling) any that is
        still open. `onChosen` gets the chosen file; `onCancelled` (optional) runs
        when the user cancels. Neither runs once the owner is gone. */
    void launch (juce::Component& owner,
                 const juce::String& title,
                 const juce::File& initial,
                 const juce::String& patterns,
                 int flags,
                 std::function<void (const juce::File&)> onChosen,
                 std::function<void()> onCancelled = nullptr)
    {
        chooser = std::make_unique<juce::FileChooser> (title, initial, patterns, useNativeDialogs());

        chooser->launchAsync (flags,
                              [safe = juce::Component::SafePointer<juce::Component> (&owner),
                               callback = std::move (onChosen),
                               cancelled = std::move (onCancelled)] (const juce::FileChooser& fc)
        {
            if (safe == nullptr)
                return;

            const auto file = fc.getResult();

            if (file != juce::File())
            {
                if (callback != nullptr)
                    callback (file);
            }
            else if (cancelled != nullptr)
            {
                cancelled();
            }
        });
    }

    /** Whether a chooser has been launched (and not cancelled). */
    bool hasChooser() const noexcept { return chooser != nullptr; }

    /** Cancels an open chooser without calling its callback. */
    void cancel() { chooser.reset(); }

private:
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace luthier
