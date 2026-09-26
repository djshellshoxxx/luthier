#include "WorkspaceTabStrip.h"

#include "Theme.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

WorkspaceTabStrip::WorkspaceTabStrip()
{
    setInterceptsMouseClicks (false, true);

    leftArrow.onClick = [this] { scrollBy (-1); };
    rightArrow.onClick = [this] { scrollBy (1); };
    menuButton.onClick = [this] { showMenu(); };

    leftArrow.setTooltip ("Earlier workspace tabs");
    rightArrow.setTooltip ("Later workspace tabs");
    menuButton.setTooltip ("Every workspace tab");

    AccessibleSetup::configureButton (leftArrow, "Scroll tabs left", "Shows the workspace tabs before these.");
    AccessibleSetup::configureButton (rightArrow, "Scroll tabs right", "Shows the workspace tabs after these.");
    AccessibleSetup::configureButton (menuButton, "All workspace tabs", "Lists every workspace tab to choose from.");

    for (auto* b : { &leftArrow, &rightArrow, &menuButton })
    {
        b->setWantsKeyboardFocus (true);
        addChildComponent (*b);
    }
}

WorkspaceTabStrip::~WorkspaceTabStrip() = default;

void WorkspaceTabStrip::setTabs (const juce::Array<juce::Button*>& buttons)
{
    tabs.clearQuick();

    for (auto* b : buttons)
    {
        tabs.add (b);
        addAndMakeVisible (*b);
    }

    // The scrolling controls come after the tabs in focus order.
    for (auto* b : { &leftArrow, &rightArrow, &menuButton })
        b->toFront (false);

    selected = juce::jlimit (0, juce::jmax (0, tabs.size() - 1), selected);
    layout();
}

int WorkspaceTabStrip::getNaturalWidth (int index) const
{
    auto* b = tabs[index].getComponent();

    if (b == nullptr)
        return kMinTabWidth;

    const float height = (float) juce::jmax (18, getHeight());
    const auto font = Fonts::ui (juce::jlimit (9.0f, 13.0f, height * 0.42f), true);

    // The look-and-feel draws tab labels upper case and tracked: allow for it.
    const float text = juce::GlyphArrangement::getStringWidth (font, b->getButtonText().toUpperCase()) * 1.18f;
    return juce::jmax (kMinTabWidth, (int) std::ceil (text) + 2 * kTabPadding);
}

bool WorkspaceTabStrip::isTabVisible (int index) const
{
    auto* b = tabs[index].getComponent();
    return b != nullptr && b->isVisible() && getLocalBounds().contains (b->getBounds());
}

void WorkspaceTabStrip::setSelectedIndex (int index)
{
    selected = juce::jlimit (0, juce::jmax (0, tabs.size() - 1), index);
    layout();
}

void WorkspaceTabStrip::scrollBy (int delta)
{
    firstVisible = juce::jlimit (0, juce::jmax (0, tabs.size() - 1), firstVisible + delta);

    // Past the end would leave empty room: layout settles where the last tab is on screen.
    layout (false);
}

juce::StringArray WorkspaceTabStrip::getMenuItems() const
{
    juce::StringArray items;

    for (const auto& t : tabs)
        items.add (t != nullptr ? t->getButtonText() : juce::String());

    return items;
}

void WorkspaceTabStrip::chooseFromMenu (int index)
{
    if (auto* b = tabs[index].getComponent())
    {
        // What a click on the tab does, now (triggerClick would post it).
        if (b->onClick)
            b->onClick();
        else
            b->triggerClick();

        setSelectedIndex (index);

        if (onTabChosen)
            onTabChosen (index);
    }
}

void WorkspaceTabStrip::showMenu()
{
    juce::PopupMenu menu;

    for (int i = 0; i < tabs.size(); ++i)
        if (auto* b = tabs[i].getComponent())
            menu.addItem (i + 1, b->getButtonText(), true, i == selected);

    juce::Component::SafePointer<WorkspaceTabStrip> safe (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->chooseFromMenu (result - 1);
                        });
}

void WorkspaceTabStrip::resized()
{
    layout();
}

void WorkspaceTabStrip::layout (bool revealSelected)
{
    const int count = tabs.size();
    auto area = getLocalBounds();
    const int gap = Metrics::gridHalf;

    if (count == 0 || area.isEmpty())
        return;

    juce::Array<int> natural;
    int total = gap * (count - 1);

    for (int i = 0; i < count; ++i)
    {
        natural.add (getNaturalWidth (i));
        total += natural.getLast();
    }

    overflowing = total > area.getWidth();

    for (auto* b : { &leftArrow, &rightArrow, &menuButton })
        b->setVisible (overflowing);

    if (! overflowing)
    {
        // Everything fits: each tab gets its label's width plus an equal share
        // of the room left over, so the strip is filled and no label is cut.
        firstVisible = 0;
        const int spare = area.getWidth() - total;
        const int extra = spare / count;
        int remainder = spare - extra * count;

        for (int i = 0; i < count; ++i)
        {
            if (auto* b = tabs[i].getComponent())
            {
                const int w = natural[i] + extra + (remainder > 0 ? 1 : 0);
                remainder = juce::jmax (0, remainder - 1);
                b->setVisible (true);
                b->setBounds (area.removeFromLeft (w));
                area.removeFromLeft (gap);
            }
        }

        return;
    }

    leftArrow.setBounds (area.removeFromLeft (kArrowWidth));
    area.removeFromLeft (gap);
    menuButton.setBounds (area.removeFromRight (kMenuWidth));
    area.removeFromRight (gap);
    rightArrow.setBounds (area.removeFromRight (kArrowWidth));
    area.removeFromRight (gap);

    const int room = area.getWidth();

    auto widthFrom = [&natural, gap, count] (int first, int last)
    {
        int w = 0;

        for (int i = first; i <= last && i < count; ++i)
            w += natural[i] + (i > first ? gap : 0);

        return w;
    };

    // The selected tab is always whole and on screen.
    firstVisible = juce::jlimit (0, count - 1, firstVisible);

    if (revealSelected)
    {
        if (selected < firstVisible)
            firstVisible = selected;

        while (firstVisible < selected && widthFrom (firstVisible, selected) > room)
            ++firstVisible;
    }

    // No empty room at the end while earlier tabs are hidden.
    while (firstVisible > 0 && widthFrom (firstVisible - 1, count - 1) <= room)
        --firstVisible;

    int x = area.getX();
    int lastShown = firstVisible - 1;

    for (int i = 0; i < count; ++i)
    {
        auto* b = tabs[i].getComponent();

        if (b == nullptr)
            continue;

        const bool fits = i >= firstVisible && x + natural[i] <= area.getRight();

        if (fits && i == lastShown + 1)
        {
            // Natural widths, left aligned: spreading the spare room would
            // make the tabs jitter as the strip scrolls.
            b->setBounds (x, area.getY(), natural[i], area.getHeight());
            b->setVisible (true);
            x += natural[i] + gap;
            lastShown = i;
        }
        else
        {
            b->setVisible (false);
        }
    }

    leftArrow.setEnabled (firstVisible > 0);
    rightArrow.setEnabled (lastShown < count - 1);
}

void WorkspaceTabStrip::paint (juce::Graphics&)
{
}

} // namespace luthier
