#pragma once

/*  global-search.md 6.1: the "Search" field at the top of the HELP tab.

    It looks and focuses like a text field, but it is a door: a click, or the
    first character typed, opens the palette on the ? (help) scope carrying
    what was typed. The palette does the searching, so there is one search. */

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier::search
{

class HelpSearchField : public juce::TextEditor
{
public:
    HelpSearchField()
    {
        setTitle ("Search everything");
        setTextToShowWhenEmpty ("Search everything...", juce::Colours::grey);
        onTextChange = [this]
        {
            const auto typed = getText();

            if (typed.isEmpty())
                return;

            setText ({}, juce::dontSendNotification);

            if (onOpen)
                onOpen (typed);
        };
        onReturnKey = [this] { if (onOpen) onOpen ({}); };
    }

    /** Opens the palette; the argument is what was typed. */
    std::function<void (const juce::String&)> onOpen;

    void mouseDown (const juce::MouseEvent& e) override
    {
        juce::TextEditor::mouseDown (e);

        if (onOpen)
            onOpen ({});
    }
};

} // namespace luthier::search
