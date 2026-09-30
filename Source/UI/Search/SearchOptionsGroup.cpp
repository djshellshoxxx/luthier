#include "SearchOptionsGroup.h"
#include "SearchCatalog.h"
#include "SearchIndex.h"
#include "LiveControls.h"

#include "../Theme.h"
#include "../UiPreferences.h"
#include "../../Accessibility/Accessibility.h"

namespace luthier::search
{

bool isAutoSwitchModeOn()
{
    return UiPreferences::get().getBool (kAutoSwitchModeKey, true);
}

void setAutoSwitchMode (bool on)
{
    UiPreferences::get().setBool (kAutoSwitchModeKey, on);
}

void clearRecentSearches()
{
    RecentStore::get().clear();
}

//==============================================================================
SearchOptionsGroup::SearchOptionsGroup()
{
    autoSwitch.setButtonText (SearchCatalog::text ("search.autoSwitchMode"));
    remember.setButtonText (SearchCatalog::text ("search.rememberRecent"));
    clear.setButtonText (SearchCatalog::text ("search.clearRecent"));

    autoSwitch.onClick = [this] { setAutoSwitchMode (autoSwitch.getToggleState()); };
    remember.onClick = [this] { RecentStore::get().setEnabled (remember.getToggleState()); };
    clear.onClick = [] { clearRecentSearches(); };

    AccessibleSetup::configureButton (autoSwitch, autoSwitch.getButtonText(),
                                      "Search switches between Easy and Advanced Mode by itself to show a result");
    AccessibleSetup::configureButton (remember, remember.getButtonText(),
                                      "Search keeps your recent items and searches on this computer");
    AccessibleSetup::configureButton (clear, clear.getButtonText(), "Forgets recent search items and queries");

    // 3.2: non-parameter settings are findable by search.
    SearchAnchors::tagSetting (autoSwitch, "set:ACCESSIBILITY:search.autoSwitchMode", "search.autoSwitchMode");
    SearchAnchors::tagSetting (remember, "set:ACCESSIBILITY:search.rememberRecent", "search.rememberRecent");
    SearchAnchors::tagSetting (clear, "set:ACCESSIBILITY:search.clearRecent", "search.clearRecent");

    for (auto* c : std::initializer_list<juce::Component*> { &autoSwitch, &remember, &clear })
        addAndMakeVisible (c);

    refresh();
}

void SearchOptionsGroup::refresh()
{
    autoSwitch.setToggleState (isAutoSwitchModeOn(), juce::dontSendNotification);
    remember.setToggleState (RecentStore::get().isEnabled(), juce::dontSendNotification);
}

void SearchOptionsGroup::paint (juce::Graphics& g)
{
    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (11.0f).boldened());
    g.drawText (SearchCatalog::text ("search.group").toUpperCase(), getLocalBounds().removeFromTop (18),
                juce::Justification::centredLeft, false);
}

void SearchOptionsGroup::resized()
{
    auto row = getLocalBounds().withTrimmedTop (22).removeFromTop (26);

    autoSwitch.setBounds (row.removeFromLeft (juce::jmin (330, row.getWidth() / 2)));
    row.removeFromLeft (8);
    remember.setBounds (row.removeFromLeft (juce::jmin (230, row.getWidth() / 2)));
    row.removeFromLeft (8);
    clear.setBounds (row.removeFromLeft (juce::jmin (170, row.getWidth())));
}

} // namespace luthier::search
