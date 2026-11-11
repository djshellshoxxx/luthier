//==============================================================================
//  Column
//==============================================================================
AdvancedPanel::Column::Column (const juce::String& t)
    : title (t)
{
    setOpaque (true);   // paint() fills every pixel: spares the panel's paint under it
}

void AdvancedPanel::Column::addSection (const juce::String& heading)
{
    Item item;
    item.heading = heading;
    item.key = heading;
    item.height = 24;

    // gui-integration 20 (TUNE-HELP-ONBOARDING): a ? on every section heading.
    item.help = helpButtons.add (new PanelHelpButton (heading));
    item.help->onHelp = [this] (const juce::String& topic)
    {
        if (onHelp != nullptr)
            onHelp (topic);
    };

    // The owner's collapsible sections: the heading row folds what follows it.
    item.header = headers.add (new CollapsibleSectionHeader (heading));
    item.header->onToggle = [this, key = item.key] (bool expanded) { setSectionCollapsed (key, ! expanded); };
    addAndMakeVisible (item.header);
    addAndMakeVisible (item.help);

    /*  The section's controls live in a body of their own, so folding hides one
        component and leaves each control's own visibility alone (a StringRow
        hides itself for strings the guitar does not have). Clicks on the gaps
        fall through to the column, which offers the section's Docs. */
    item.body = bodies.add (new juce::Component ("Section: " + heading));
    item.body->setInterceptsMouseClicks (false, true);
    addAndMakeVisible (item.body);

    // global-search 4.2: a jump to a control in a folded section unfolds it.
    search::SearchAnchors::tag (*item.body, "section:" + heading,
                                [this, key = item.key] { setSectionCollapsed (key, false); });

    currentBody = item.body;
    items.add (item);
}

juce::String AdvancedPanel::Column::getSectionAt (int y) const
{
    juce::String heading;

    for (const auto& item : items)
    {
        if (item.y > y)
            break;

        if (item.header != nullptr)
            heading = item.heading;
    }

    return heading;
}

void AdvancedPanel::Column::mouseDown (const juce::MouseEvent& e)
{
    // gui-integration 16: a panel's empty area offers its Docs.
    if (! e.mods.isPopupMenu())
        return;

    const auto section = getSectionAt (e.getEventRelativeTo (this).y);

    if (section.isEmpty())
        return;

    juce::PopupMenu menu;
    menu.addItem (1, "Docs: " + section);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<Column> (this), section] (int result)
    {
        if (safe != nullptr && result == 1 && safe->onHelp != nullptr)
            safe->onHelp (section);
    });
}

void AdvancedPanel::Column::addControl (juce::Component* component, int height)
{
    if (component == nullptr)
        return;

    (currentBody != nullptr ? currentBody : static_cast<juce::Component*> (this))->addAndMakeVisible (component);

    Item item;
    item.component = component;
    item.height = height;
    items.add (item);
}

void AdvancedPanel::Column::renameSection (const juce::String& from, const juce::String& to)
{
    for (auto& item : items)
        if (item.heading == from)
        {
            item.heading = to;

            if (item.header != nullptr)
                item.header->setHeading (to);
        }

    repaint();
}

void AdvancedPanel::Column::addGap (int height)
{
    Item item;
    item.isGap = true;
    item.height = height;
    items.add (item);
}

AdvancedPanel::Column::Item* AdvancedPanel::Column::findSection (const juce::String& headingOrKey)
{
    for (auto& item : items)
        if (item.header != nullptr && (item.key.equalsIgnoreCase (headingOrKey) || item.heading.equalsIgnoreCase (headingOrKey)))
            return &item;

    return nullptr;
}

const AdvancedPanel::Column::Item* AdvancedPanel::Column::findSection (const juce::String& headingOrKey) const
{
    return const_cast<Column*> (this)->findSection (headingOrKey);
}

bool AdvancedPanel::Column::isSectionCollapsed (const juce::String& heading) const
{
    const auto* item = findSection (heading);
    return item != nullptr && item->collapsed;
}

void AdvancedPanel::Column::setSectionCollapsed (const juce::String& heading, bool shouldCollapse)
{
    auto* item = findSection (heading);

    if (item == nullptr)
        return;

    item->header->setExpanded (! shouldCollapse);

    if (item->collapsed == shouldCollapse)
        return;

    item->collapsed = shouldCollapse;

    if (getWidth() > 0)
        layout (getWidth());

    repaint();

    if (onCollapseChanged != nullptr)
        onCollapseChanged();
}

juce::StringArray AdvancedPanel::Column::getCollapsedKeys() const
{
    juce::StringArray keys;

    for (const auto& item : items)
        if (item.header != nullptr && item.collapsed)
            keys.add (item.key);

    return keys;
}

void AdvancedPanel::Column::setCollapsedKeys (const juce::StringArray& keys)
{
    for (auto& item : items)
        if (item.header != nullptr)
        {
            item.collapsed = keys.contains (item.key);
            item.header->setExpanded (! item.collapsed);
        }

    if (getWidth() > 0)
        layout (getWidth());

    repaint();
}

CollapsibleSectionHeader* AdvancedPanel::Column::getSectionHeader (const juce::String& heading) const
{
    const auto* item = findSection (heading);
    return item != nullptr ? item->header : nullptr;
}

juce::Component* AdvancedPanel::Column::getSectionBody (const juce::String& heading) const
{
    const auto* item = findSection (heading);
    return item != nullptr ? item->body : nullptr;
}

int AdvancedPanel::Column::layout (int width)
{
    int y = Metrics::grid;
    Item* section = nullptr;   // the heading the items below belong to
    int bodyTop = 0;

    auto closeSection = [&]
    {
        if (section == nullptr)
            return;

        section->body->setVisible (! section->collapsed);
        section->body->setBounds (0, bodyTop, width, section->collapsed ? 0 : juce::jmax (0, y - bodyTop));
    };

    for (int i = 0; i < items.size(); ++i)
    {
        auto& item = items.getReference (i);

        if (item.header != nullptr)
        {
            closeSection();
            section = &item;
            item.y = y;

            const int helpX = width - Metrics::grid - PanelHelpButton::kSize;

            item.help->setBounds (helpX, y + (item.height - PanelHelpButton::kSize) / 2 - 1,
                                  PanelHelpButton::kSize, PanelHelpButton::kSize);
            item.header->setBounds (Metrics::grid, y, juce::jmax (24, helpX - Metrics::gridHalf - Metrics::grid), item.height - 2);

            y += item.height + Metrics::gridHalf;
            bodyTop = y;
            continue;
        }

        /*  A folded section takes its controls and its inner gaps with it; the
            gap that separates it from the next heading stays, so a column of
            folded sections still reads as a list of headings. */
        if (section != nullptr && section->collapsed)
        {
            bool beforeHeading = item.isGap;

            for (int j = i + 1; beforeHeading && j < items.size(); ++j)
            {
                if (items.getReference (j).header != nullptr)
                    break;

                beforeHeading = items.getReference (j).isGap;
            }

            if (! beforeHeading)
            {
                item.y = y;
                continue;
            }
        }

        item.y = y;

        if (item.component != nullptr)
            item.component->setBounds (Metrics::grid, y - (section != nullptr ? bodyTop : 0),
                                       juce::jmax (40, width - Metrics::grid * 2), item.height);

        y += item.height + Metrics::gridHalf;
    }

    closeSection();

    contentHeight = y + Metrics::grid;
    setSize (width, contentHeight);

    return contentHeight;
}

void AdvancedPanel::Column::resized()
{
    layout (getWidth());
}

void AdvancedPanel::Column::paint (juce::Graphics& g)
{
    // The headings are static between layouts; PaintCache keeps their tracked
    // text and engraved plates out of every control's small repaint (a preset
    // load used to re-lay every heading's glyphs: OPTIMISATION_LOG).
    juce::String layoutKey;

    for (const auto& item : items)
        if (item.header != nullptr)
            layoutKey << item.heading << ':' << item.y << ';';

    backgroundCache.draw (g, getLocalBounds(), layoutKey.hashCode64(), [this] (juce::Graphics& g)
    {
        g.fillAll (Palette::background);

        for (const auto& item : items)
        {
            if (item.header == nullptr)
                continue;

            // The plate stops short of the fold chevron at the row's right.
            LuthierLookAndFeel::drawSectionHeader (
                g, { Metrics::grid, item.y, getWidth() - Metrics::grid * 2 - PanelHelpButton::kSize - 24, item.height },
                item.heading);

            LuthierLookAndFeel::drawSeparator (
                g, { Metrics::grid, item.y + item.height - 2, getWidth() - Metrics::grid * 2, 1 });
        }
    }, true);
}
