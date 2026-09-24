#include "CommandPalette.h"
#include "SearchCatalog.h"
#include "LiveControls.h"

#include "../Theme.h"
#include "../Widgets.h"
#include "../../PluginProcessor.h"
#include "../../Accessibility/Accessibility.h"
#include "../../Accessibility/Localisation.h"

namespace luthier::search
{
namespace
{
    int scaled (int px) { return AccessibilitySettings::get().scaled (px); }

    const char* glyphFor (ItemKind kind)
    {
        switch (kind)
        {
            case ItemKind::parameter:    return "~";
            case ItemKind::choiceOption: return "=";
            case ItemKind::place:        return "@";
            case ItemKind::command:      return ">";
            case ItemKind::shortcut:     return "K";
            case ItemKind::preset:       return "#";
            case ItemKind::guitar:       return "G";
            case ItemKind::part:         return "P";
            case ItemKind::pedal:        return "F";
            case ItemKind::snapshot:     return "S";
            case ItemKind::help:         return "?";
            case ItemKind::setting:      return "*";
            case ItemKind::provider:
            default: break;
        }

        return "+";
    }

    bool isParameterRow (const CommandPalette::Row& r)
    {
        return r.item != nullptr && (r.item->kind == ItemKind::parameter || r.item->kind == ItemKind::choiceOption);
    }

    juce::KeyPress searchKey()
    {
        if (const auto* b = AccessibilitySettings::get().findShortcut ("search"))
            return b->key;

        return {};
    }
}

//==============================================================================
class CommandPalette::RowComponent : public juce::Component
{
public:
    explicit RowComponent (CommandPalette& p) : owner (p) {}

    void update (int newRow) { row = newRow; repaint(); }

    void paint (juce::Graphics& g) override
    {
        owner.paintRow (g, row, getWidth(), getHeight(), row == owner.selected);
    }

    juce::Rectangle<int> sliderArea() const
    {
        return { getWidth() - scaled (44) - scaled (110), getHeight() / 2 - scaled (4), scaled (44), scaled (8) };
    }

    void mouseMove (const juce::MouseEvent&) override
    {
        if (row != owner.selected && juce::isPositiveAndBelow (row, (int) owner.rows.size())
              && owner.rows[(size_t) row].isSelectable())
            owner.selectRow (row);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! juce::isPositiveAndBelow (row, (int) owner.rows.size()))
            return;

        owner.selectRow (row);

        if (e.mods.isPopupMenu())
        {
            owner.showSecondaryMenu (row);
            return;
        }

        const auto& r = owner.rows[(size_t) row];
        dragging = isParameterRow (r) && sliderArea().expanded (2, 6).contains (e.getPosition());

        if (dragging)
            if (auto* p = owner.navigator.getProcessor().getState().getParameter (r.item->target))
            {
                p->beginChangeGesture();
                mouseDrag (e);
            }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging)
            return;

        const auto& r = owner.rows[(size_t) row];

        if (auto* p = owner.navigator.getProcessor().getState().getParameter (r.item->target))
        {
            const auto area = sliderArea();
            const float v = juce::jlimit (0.0f, 1.0f, (float) (e.x - area.getX()) / (float) juce::jmax (1, area.getWidth()));
            p->setValueNotifyingHost (v);
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragging)
        {
            dragging = false;
            const auto& r = owner.rows[(size_t) row];

            if (auto* p = owner.navigator.getProcessor().getState().getParameter (r.item->target))
            {
                p->endChangeGesture();
                owner.announce (InlineValue::displayText (*p, p->getValue()));
            }

            return;
        }

        // 6.3: a click on a row is Enter.
        if (! e.mods.isPopupMenu() && e.mouseWasClicked() && row == owner.selected)
            owner.activateSelected (ActivationKind::primary);
    }

private:
    CommandPalette& owner;
    int row = -1;
    bool dragging = false;
};

//==============================================================================
class CommandPalette::Model : public juce::ListBoxModel
{
public:
    explicit Model (CommandPalette& p) : owner (p) {}

    int getNumRows() override { return (int) owner.rows.size(); }

    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}

    juce::Component* refreshComponentForRow (int row, bool, juce::Component* existing) override
    {
        auto* c = dynamic_cast<RowComponent*> (existing);

        if (c == nullptr)
        {
            delete existing;
            c = new RowComponent (owner);
        }

        c->update (row);
        return c;
    }

    juce::String getNameForRow (int row) override { return owner.getAccessibleRowTitle (row); }

    void selectedRowsChanged (int row) override
    {
        if (row >= 0 && row != owner.selected)
            owner.selectRow (row);
    }

    void returnKeyPressed (int) override { owner.activateSelected (ActivationKind::primary); }

private:
    CommandPalette& owner;
};

//==============================================================================
CommandPalette::CommandPalette (SearchNavigator& n)
    : navigator (n)
{
    setTitle (SearchCatalog::text ("search.name"));
    setWantsKeyboardFocus (false);
    setAlwaysOnTop (false);

    field.setTitle (SearchCatalog::text ("search.fieldLabel"));
    field.setTextToShowWhenEmpty (SearchCatalog::text ("search.hint"), Palette::textDisabled);
    field.setInputRestrictions (SearchMatcher::kMaxQueryLength);
    field.setSelectAllWhenFocused (false);
    field.setEscapeAndReturnKeysConsumed (false);
    field.addKeyListener (this);
    field.onTextChange = [this]
    {
        if (updatingField)
            return;

        lastTypedMs = juce::Time::getMillisecondCounterHiRes();
        announcePending = true;
        pendingConfirmId.clear();
        allowInlineId.clear();
        rebuildRows();
    };
    addAndMakeVisible (field);

    const auto& names = SearchIndex::getScopeNames();

    for (int i = 0; i < names.size(); ++i)
    {
        auto* chip = chips.add (new juce::TextButton (SearchCatalog::text ("search.scope." + names[i])));
        chip->setClickingTogglesState (true);
        chip->setRadioGroupId (0x5ea4c4);
        chip->setToggleState (i == 0, juce::dontSendNotification);
        chip->onClick = [this, i] { setScope ((SearchIndex::Scope) i); field.grabKeyboardFocus(); };
        chip->addKeyListener (this);
        AccessibleSetup::configureButton (*chip, chip->getButtonText(), "Search scope");
        addAndMakeVisible (chip);
    }

    model = std::make_unique<Model> (*this);
    list.setModel (model.get());
    list.setTitle (SearchCatalog::text ("search.name"));
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setOutlineThickness (0);
    list.addKeyListener (this);
    list.setWantsKeyboardFocus (true);
    addAndMakeVisible (list);

    footerText = SearchCatalog::text ("search.footer");
}

CommandPalette::~CommandPalette()
{
    stopTimer();
    field.removeKeyListener (this);
    list.removeKeyListener (this);

    for (auto* chip : chips)
        chip->removeKeyListener (this);

    list.setModel (nullptr);
}

std::unique_ptr<juce::AccessibilityHandler> CommandPalette::createAccessibilityHandler()
{
    // 14: the palette is a dialog (JUCE: dialogWindow) named "Search".
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::dialogWindow);
}

//==============================================================================
void CommandPalette::open (const juce::String& initialText)
{
    focusBeforeOpen = juce::Component::getCurrentlyFocusedComponent();

    // Without a desktop peer nothing holds real focus; what search last asked
    // to focus stands in for it, so closing still returns there.
    if (focusBeforeOpen == nullptr)
        focusBeforeOpen = navigator.getLastFocusRequest();

    setVisible (true);
    toFront (false);
    updateLayout();

    footerText = SearchCatalog::text ("search.footer");
    footerWarning = false;
    pendingConfirmId.clear();
    allowInlineId.clear();

    updatingField = true;
    field.setText (initialText.substring (0, SearchMatcher::kMaxQueryLength), juce::dontSendNotification);
    field.moveCaretToEnd();
    updatingField = false;

    rebuildRows();
    navigator.requestFocus (&field);

    startTimerHz (10);
    announce (SearchCatalog::text ("search.announce.open"));

    if (initialText.isNotEmpty())
    {
        lastTypedMs = juce::Time::getMillisecondCounterHiRes();
        announcePending = true;
    }
}

void CommandPalette::close (bool restoreFocus)
{
    if (! isVisible())
        return;

    navigator.endNudgeGesture();
    stopTimer();
    setVisible (false);

    // accessibility 1: focus goes back where it was.
    if (restoreFocus && focusBeforeOpen != nullptr)
        navigator.requestFocus (focusBeforeOpen);
}

void CommandPalette::visibilityChanged()
{
    if (! isVisible())
        announcePending = false;
}

void CommandPalette::setQuery (const juce::String& text)
{
    // A copy: `text` may be a row's, and the rows are rebuilt below.
    const juce::String query (text.substring (0, SearchMatcher::kMaxQueryLength));

    // TextEditor's own change message is asynchronous; a query set from code
    // (a recent search, did-you-mean) filters now, as typing does.
    updatingField = true;
    field.setText (query, false);
    field.moveCaretToEnd();
    updatingField = false;

    lastTypedMs = juce::Time::getMillisecondCounterHiRes();
    announcePending = true;
    pendingConfirmId.clear();
    allowInlineId.clear();
    rebuildRows();
}

void CommandPalette::setScope (SearchIndex::Scope newScope)
{
    scope = newScope;

    for (int i = 0; i < chips.size(); ++i)
        chips[i]->setToggleState (i == (int) newScope, juce::dontSendNotification);

    rebuildRows();
}

void CommandPalette::refresh()
{
    rebuildRows();
}

void CommandPalette::setFooter (const juce::String& text, bool warning)
{
    footerText = text;
    footerWarning = warning;
    repaint (footerArea);
}

void CommandPalette::announce (const juce::String& text)
{
    navigator.announce (text);

    if (onAnnouncement)
        onAnnouncement (text);
}

void CommandPalette::announceResultsNow()
{
    announcePending = false;

    int count = 0;
    const Row* first = nullptr;

    for (const auto& r : rows)
        if (r.type == Row::Type::item)
        {
            ++count;

            if (first == nullptr)
                first = &r;
        }

    if (count == 0 || field.getText().trim().isEmpty())
        announce (count == 0 && field.getText().trim().isNotEmpty() ? SearchCatalog::text ("search.announce.none")
                                                                    : SearchCatalog::text ("search.announce.open"));
    else
        announce (SearchCatalog::text ("search.announce.results", { { "n", juce::String (count) },
                                                                    { "title", first->item->title } }));
}

//==============================================================================
void CommandPalette::rebuildRows()
{
    const auto keepId = getSelected() != nullptr ? getSelected()->itemId : juce::String();

    rows.clear();
    reading = {};

    const auto query = field.getText();

    if (query.trim().isEmpty())
    {
        buildEmptyState();
    }
    else
    {
        reading = navigator.readValue (query);

        if (reading.isValid())
        {
            Row r;
            r.item = reading.item;
            r.itemId = reading.item->id;
            r.valueReading = true;

            if (auto* provider = navigator.getIndex().getProviderFor (*reading.item))
                r.availability = provider->availabilityOf (*reading.item);

            rows.push_back (r);
        }

        for (const auto& result : navigator.getIndex().query (query, scope))
        {
            if (reading.isValid() && result.item == reading.item)
                continue;

            Row r;
            r.item = result.item;
            r.itemId = result.item->id;
            r.availability = result.availability;
            rows.push_back (r);
        }

        if (rows.empty())
        {
            Row none;
            none.type = Row::Type::noResults;
            none.text = SearchCatalog::text ("search.noResults", { { "query", query.trim() } });
            rows.push_back (none);

            if (const auto suggestion = navigator.getIndex().didYouMean (query); suggestion.isNotEmpty())
            {
                Row dym;
                dym.type = Row::Type::didYouMean;
                dym.text = suggestion;
                dym.itemId = "didyoumean";
                rows.push_back (dym);
            }

            Row help;
            help.type = Row::Type::searchHelp;
            help.text = query.trim();
            help.itemId = "searchhelp";
            rows.push_back (help);
        }
    }

    list.updateContent();

    // Keep the selection by id (12), else the first selectable row.
    selected = -1;

    for (int i = 0; i < (int) rows.size() && selected < 0; ++i)
        if (keepId.isNotEmpty() && rows[(size_t) i].itemId == keepId && rows[(size_t) i].isSelectable())
            selected = i;

    for (int i = 0; i < (int) rows.size() && selected < 0; ++i)
        if (rows[(size_t) i].isSelectable() && rows[(size_t) i].type != Row::Type::recentQuery)
            selected = i;

    for (int i = 0; i < (int) rows.size() && selected < 0; ++i)
        if (rows[(size_t) i].isSelectable())
            selected = i;

    if (selected >= 0)
        list.selectRow (selected, true, true);
    else
        list.deselectAllRows();

    updateLayout();
    list.repaint();
}

void CommandPalette::buildEmptyState()
{
    auto addHeader = [this] (const juce::String& key)
    {
        Row h;
        h.type = Row::Type::header;
        h.text = SearchCatalog::text (key);
        rows.push_back (h);
    };

    auto addItem = [this] (const SearchItem* item)
    {
        if (item == nullptr)
            return;

        for (const auto& existing : rows)
            if (existing.item == item)
                return;

        Row r;
        r.item = item;
        r.itemId = item->id;

        if (auto* provider = navigator.getIndex().getProviderFor (*item))
            r.availability = provider->availabilityOf (*item);

        rows.push_back (r);
    };

    // 6.2: with "Remember recent" off, the Recent sections are not shown.
    if (RecentStore::get().isEnabled())
    {
        const auto recent = navigator.getIndex().getRecentItems (8);

        if (! recent.empty())
        {
            addHeader ("search.section.recent");

            for (auto* item : recent)
                addItem (item);
        }

        const auto& queries = RecentStore::get().getQueries();

        if (! queries.isEmpty())
        {
            addHeader ("search.section.recentQueries");

            for (int i = 0; i < juce::jmin (3, queries.size()); ++i)
            {
                Row q;
                q.type = Row::Type::recentQuery;
                q.text = queries[i];
                q.itemId = "query:" + queries[i];
                rows.push_back (q);
            }
        }
    }

    addHeader ("search.section.suggestions");

    for (auto* item : navigator.getSuggestions())
        addItem (item);

    Row hint;
    hint.type = Row::Type::hint;
    hint.text = SearchCatalog::text ("search.hint");
    rows.push_back (hint);
}

//==============================================================================
int CommandPalette::getRowHeight() const
{
    // gui-integration 9: 44 px rows in Live Mode.
    return scaled (navigator.isLiveMode() ? 44 : 36);
}

void CommandPalette::updateLayout()
{
    const auto bounds = getLocalBounds();
    const int width = juce::jmin (scaled (640), bounds.getWidth() - 32);
    const int top = scaled (Metrics::headerHeight + 48);
    const int maxHeight = juce::jmax (scaled (140), (int) (bounds.getHeight() * 0.6f));

    const int fieldH = scaled (40), chipH = scaled (24), footerH = scaled (20);
    const int rowH = getRowHeight();
    const int listH = juce::jlimit (rowH, juce::jmax (rowH, maxHeight - fieldH - chipH - footerH),
                                    juce::jmax (1, (int) rows.size()) * rowH);

    box = juce::Rectangle<int> ((bounds.getWidth() - width) / 2, juce::jmin (top, juce::jmax (0, bounds.getHeight() - 100)),
                                width, fieldH + chipH + listH + footerH);

    auto area = box;
    fieldArea = area.removeFromTop (fieldH);
    chipArea = area.removeFromTop (chipH);
    footerArea = area.removeFromBottom (footerH);
    listArea = area;

    field.setBounds (fieldArea.reduced (scaled (30), scaled (6)).withTrimmedRight (scaled (20)));

    auto chipRow = chipArea.reduced (scaled (8), 1);
    const bool rtl = Localisation::get().isRightToLeft();

    for (auto* chip : chips)
    {
        const int w = juce::jmax (scaled (56), chip->getBestWidthForHeight (chipRow.getHeight()));
        chip->setBounds (rtl ? chipRow.removeFromRight (w) : chipRow.removeFromLeft (w));
        (rtl ? chipRow.removeFromRight (scaled (4)) : chipRow.removeFromLeft (scaled (4)));
    }

    list.setRowHeight (rowH);
    list.setBounds (listArea);
}

void CommandPalette::resized()
{
    updateLayout();
}

void CommandPalette::paint (juce::Graphics& g)
{
    // A 40% scrim (6.2).
    g.fillAll (juce::Colours::black.withAlpha (0.4f));

    g.setColour (Palette::panelRaised);
    g.fillRoundedRectangle (box.toFloat(), 6.0f);
    g.setColour (Palette::edgeBright);
    g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 6.0f, 1.0f);

    // The magnifier: a ring and a handle, so it is a shape, not a colour.
    auto glass = fieldArea.withWidth (scaled (30)).reduced (scaled (8)).toFloat();
    g.setColour (Palette::textMuted);
    g.drawEllipse (glass.withSizeKeepingCentre (glass.getWidth() * 0.7f, glass.getWidth() * 0.7f), 1.6f);
    g.drawLine (glass.getCentreX() + glass.getWidth() * 0.25f, glass.getCentreY() + glass.getWidth() * 0.25f,
                glass.getRight(), glass.getBottom(), 1.8f);

    g.setFont (Fonts::ui (10.0f));
    g.drawText ("Esc", fieldArea.removeFromRight (scaled (34)), juce::Justification::centred, false);

    g.setColour (Palette::edge);
    g.drawHorizontalLine (chipArea.getBottom(), (float) box.getX(), (float) box.getRight());
    g.drawHorizontalLine (footerArea.getY(), (float) box.getX(), (float) box.getRight());

    g.setColour (footerWarning ? Palette::warning : Palette::textDisabled);
    g.setFont (Fonts::ui (10.5f));
    g.drawFittedText (footerText, footerArea.reduced (scaled (10), 0), juce::Justification::centredLeft, 1);
}

juce::String CommandPalette::getSubtitle (int row) const
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return {};

    const auto& r = rows[(size_t) row];

    if (r.item == nullptr)
        return {};

    if (r.itemId == pendingConfirmId)
        return pendingConfirmText;

    if (r.valueReading)
        return r.item->kind == ItemKind::parameter || r.item->kind == ItemKind::choiceOption
                 ? (reading.resolved.clamped ? reading.resolved.clampText : reading.resolved.preview)
                 : juce::String();

    return navigator.subtitleFor (*r.item, r.availability);
}

juce::String CommandPalette::getAccessibleRowTitle (int row) const
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return {};

    const auto& r = rows[(size_t) row];

    switch (r.type)
    {
        case Row::Type::header:      return r.text;
        case Row::Type::hint:        return r.text;
        case Row::Type::noResults:   return r.text;
        case Row::Type::recentQuery: return r.text + ", " + SearchCatalog::text ("search.section.recentQueries");
        case Row::Type::didYouMean:  return SearchCatalog::text ("search.didYouMean", { { "suggestion", r.text } });
        case Row::Type::searchHelp:  return SearchCatalog::text ("search.searchHelp", { { "query", r.text } });
        case Row::Type::item:
        default: break;
    }

    const auto value = const_cast<SearchNavigator&> (navigator).valueTextFor (*r.item);
    const bool locked = isLocked (r.availability);

    // accessibility 9: minimal verbosity says only title and value.
    if (AccessibilitySettings::get().getVerbosity() == AccessibilitySettings::Verbosity::minimal)
        return value.isNotEmpty() ? r.item->title + ", " + value : r.item->title;

    juce::StringArray parts { r.item->title, getKindName (r.item->kind) };

    if (r.item->breadcrumb.isNotEmpty()) parts.add (r.item->breadcrumb);
    if (value.isNotEmpty())              parts.add (value);
    if (locked)                          parts.add (SearchCatalog::text ("search.proLocked"));

    if (const auto sub = getSubtitle (row); sub.isNotEmpty() && ! locked)
        parts.add (sub);

    return parts.joinIntoString (", ");
}

void CommandPalette::paintRow (juce::Graphics& g, int row, int width, int height, bool isSelectedRow)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;

    const auto& r = rows[(size_t) row];
    auto bounds = juce::Rectangle<int> (0, 0, width, height);
    const bool rtl = Localisation::get().isRightToLeft();

    if (r.type == Row::Type::header)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (10.0f).boldened());
        g.drawText (r.text.toUpperCase(), bounds.reduced (scaled (10), 0), juce::Justification::bottomLeft, false);
        return;
    }

    if (isSelectedRow)
    {
        g.setColour (Palette::accent.withAlpha (0.16f));
        g.fillRect (bounds);
        // Selected is also a shape: a bar at the leading edge (accessibility 0.2).
        g.setColour (Palette::accent);
        g.fillRect (rtl ? bounds.removeFromRight (3) : bounds.removeFromLeft (3));
    }

    bounds = juce::Rectangle<int> (0, 0, width, height).reduced (scaled (8), 0);

    if (r.type != Row::Type::item || r.item == nullptr)
    {
        juce::String text = r.text;

        if (r.type == Row::Type::didYouMean)  text = SearchCatalog::text ("search.didYouMean", { { "suggestion", r.text } });
        if (r.type == Row::Type::searchHelp)  text = SearchCatalog::text ("search.searchHelp", { { "query", r.text } });
        if (r.type == Row::Type::recentQuery) text = "\"" + r.text + "\"";

        g.setColour (r.type == Row::Type::hint || r.type == Row::Type::noResults ? Palette::textMuted : Palette::textPrimary);
        g.setFont (Fonts::ui (12.0f));
        g.drawFittedText (text, bounds, juce::Justification::centredLeft, 1);
        return;
    }

    const auto& item = *r.item;

    // Kind glyph.
    auto glyph = rtl ? bounds.removeFromRight (scaled (22)) : bounds.removeFromLeft (scaled (22));
    g.setColour (Palette::edgeBright);
    g.drawRoundedRectangle (glyph.withSizeKeepingCentre (scaled (18), scaled (18)).toFloat(), 3.0f, 1.0f);
    g.setColour (Palette::textMuted);
    g.setFont (Fonts::mono (10.0f));
    g.drawText (glyphFor (item.kind), glyph, juce::Justification::centred, false);
    (rtl ? bounds.removeFromRight (scaled (6)) : bounds.removeFromLeft (scaled (6)));

    // The right-hand side: value, shortcut chip, lock.
    const bool locked = isLocked (r.availability);
    juce::String right;

    if (locked)
        right = juce::String ("[lock] ") + SearchCatalog::text ("search.pro");
    else if (item.kind == ItemKind::command)
    {
        if (const auto* b = AccessibilitySettings::get().findShortcut (item.target); b != nullptr && b->key.isValid())
            right = b->key.getTextDescription();
    }
    else
        right = navigator.valueTextFor (item);

    auto rightArea = rtl ? bounds.removeFromLeft (scaled (100)) : bounds.removeFromRight (scaled (100));

    if (right.isNotEmpty())
    {
        g.setColour (locked ? Palette::textMuted : Palette::textPrimary);
        g.setFont (item.kind == ItemKind::command ? Fonts::mono (10.5f) : Fonts::ui (11.0f));

        if (item.kind == ItemKind::command && ! locked)
        {
            g.setColour (Palette::edgeBright);
            g.drawRoundedRectangle (rightArea.reduced (scaled (6), height / 2 - scaled (9)).toFloat(), 3.0f, 1.0f);
            g.setColour (Palette::textPrimary);
        }

        g.drawFittedText (right, rightArea, rtl ? juce::Justification::centredLeft : juce::Justification::centredRight, 1);
    }

    // The inline slider on a parameter row (4.4: 44 x 8 px).
    if (isParameterRow (r) && isSelectedRow && ! locked)
        if (auto* p = navigator.getProcessor().getState().getParameter (item.target))
        {
            auto slider = rtl ? bounds.removeFromLeft (scaled (50)) : bounds.removeFromRight (scaled (50));
            auto track = slider.withSizeKeepingCentre (scaled (44), scaled (8)).toFloat();
            const float v = r.valueReading ? reading.resolved.normalised : p->getValue();

            g.setColour (Palette::panelSunken);
            g.fillRoundedRectangle (track, 3.0f);
            g.setColour (Palette::accent);
            g.fillRoundedRectangle (track.withWidth (juce::jmax (2.0f, track.getWidth() * v)), 3.0f);
            g.setColour (Palette::edgeBright);
            g.drawRoundedRectangle (track, 3.0f, 1.0f);
        }

    // Title, with the matched characters in the accent colour and bold.
    const auto subtitle = getSubtitle (row);
    auto titleArea = subtitle.isNotEmpty() ? bounds.removeFromTop (height * 11 / 20) : bounds;

    juce::AttributedString title;
    auto prefixScope = SearchIndex::Scope::all;
    const auto ranges = SearchMatcher::matchedRanges (SearchMatcher::makeQuery (SearchIndex::parseScope (field.getText(), prefixScope)), item.title);
    const auto normal = Fonts::ui (12.5f);
    const auto bold = Fonts::ui (12.5f).boldened();
    int at = 0;

    for (const auto& range : ranges)
    {
        if (range.getStart() > at)
            title.append (item.title.substring (at, range.getStart()), normal, locked ? Palette::textMuted : Palette::textPrimary);

        title.append (item.title.substring (range.getStart(), range.getEnd()), bold, Palette::accentBright);
        at = range.getEnd();
    }

    title.append (item.title.substring (at), normal, locked ? Palette::textMuted : Palette::textPrimary);

    if (item.breadcrumb.isNotEmpty())
        title.append ("   " + item.breadcrumb, Fonts::ui (10.5f), Palette::textDisabled);

    title.setJustification (rtl ? juce::Justification::centredRight : juce::Justification::centredLeft);
    title.setWordWrap (juce::AttributedString::none);
    title.draw (g, titleArea.toFloat());

    if (subtitle.isNotEmpty())
    {
        const bool warn = r.itemId == pendingConfirmId || (r.valueReading && reading.resolved.clamped)
                          || isContextGate (r.availability) || r.availability == Availability::modeUnavailable;
        g.setColour (warn ? Palette::warning : Palette::textMuted);
        g.setFont (Fonts::ui (10.5f));
        g.drawFittedText (subtitle, bounds, rtl ? juce::Justification::topRight : juce::Justification::topLeft, 1);
    }
}

//==============================================================================
void CommandPalette::selectRow (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()) || ! rows[(size_t) row].isSelectable())
        return;

    if (row != selected)
    {
        pendingConfirmId.clear();
        selected = row;
    }

    list.selectRow (row, false, true);
    list.repaint();
}

const CommandPalette::Row* CommandPalette::getSelected() const
{
    return juce::isPositiveAndBelow (selected, (int) rows.size()) ? &rows[(size_t) selected] : nullptr;
}

void CommandPalette::moveSelection (int delta, bool wrap)
{
    const int n = (int) rows.size();

    if (n == 0)
        return;

    int row = selected < 0 ? (delta > 0 ? -1 : n) : selected;
    const int step = delta > 0 ? 1 : -1;
    int remaining = std::abs (delta);
    int guard = 0;

    while (remaining > 0 && guard++ < n * 4)
    {
        int next = row + step;

        if (next < 0 || next >= n)
        {
            if (! wrap)
                break;

            next = (next + n) % n;
        }

        row = next;

        if (rows[(size_t) row].isSelectable())
            --remaining;
    }

    if (juce::isPositiveAndBelow (row, n) && rows[(size_t) row].isSelectable())
        selectRow (row);
}

bool CommandPalette::keyPressed (const juce::KeyPress& key)
{
    return handleKey (key);
}

bool CommandPalette::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return handleKey (key);
}

bool CommandPalette::handleKey (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();

    if (key == juce::KeyPress::escapeKey)
    {
        close (true);
        return true;
    }

    // 6.1: the search key again closes the palette.
    if (const auto k = searchKey(); k.isValid() && key == k)
    {
        close (true);
        return true;
    }

    if (code == juce::KeyPress::F1Key)
    {
        navigator.openHelpTopic ("search", false);
        close (false);
        return true;
    }

    if (code == juce::KeyPress::upKey && ! mods.isAltDown())   { moveSelection (-1, true); return true; }
    if (code == juce::KeyPress::downKey && ! mods.isAltDown()) { moveSelection (1, true);  return true; }
    if (code == juce::KeyPress::pageUpKey)                     { moveSelection (-8, false); return true; }
    if (code == juce::KeyPress::pageDownKey)                   { moveSelection (8, false);  return true; }

    if (mods.isCommandDown() && (code == juce::KeyPress::homeKey || code == juce::KeyPress::endKey))
    {
        const bool end = code == juce::KeyPress::endKey;

        for (int i = end ? (int) rows.size() - 1 : 0; juce::isPositiveAndBelow (i, (int) rows.size()); i += end ? -1 : 1)
            if (rows[(size_t) i].isSelectable())
            {
                selectRow (i);
                break;
            }

        return true;
    }

    if (code == juce::KeyPress::returnKey)
    {
        if (mods.isAltDown())
            showSecondaryMenu (selected);
        else if (mods.isShiftDown())
            activateSelected (ActivationKind::keepOpen);
        else if (mods.isCommandDown() || mods.isCtrlDown())
            activateSelected (ActivationKind::goOnly);
        else
            activateSelected (ActivationKind::primary);

        return true;
    }

    if (mods.isAltDown() && (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey))
    {
        if (const auto* r = getSelected(); r != nullptr && isParameterRow (*r))
        {
            const int direction = (code == juce::KeyPress::rightKey) != Localisation::get().isRightToLeft() ? 1 : -1;

            if (navigator.nudge (r->item->target, direction, mods.isShiftDown()))
            {
                list.repaint();

                if (auto* p = navigator.getProcessor().getState().getParameter (r->item->target))
                    announce (InlineValue::displayText (*p, p->getValue()));
            }
        }

        return true;
    }

    // Tab / Shift+Tab: the field, the chip row, the list (6.3).
    if (code == juce::KeyPress::tabKey)
    {
        auto* focused = juce::Component::getCurrentlyFocusedComponent();
        const bool back = mods.isShiftDown();
        const bool inChips = focused != nullptr && chips.contains (dynamic_cast<juce::TextButton*> (focused));
        const bool inList = focused != nullptr && (focused == &list || list.isParentOf (focused));

        juce::Component* next = nullptr;

        if (inChips)     next = back ? static_cast<juce::Component*> (&field) : &list;
        else if (inList) next = back ? static_cast<juce::Component*> (chips[(int) scope]) : &field;
        else             next = back ? static_cast<juce::Component*> (&list) : chips[(int) scope];

        next->grabKeyboardFocus();
        return true;
    }

    // Left / Right choose a chip while the chip row has focus.
    if (auto* chip = dynamic_cast<juce::TextButton*> (juce::Component::getCurrentlyFocusedComponent());
        chip != nullptr && chips.contains (chip) && (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey))
    {
        const int n = chips.size();
        const int next = ((int) scope + (code == juce::KeyPress::rightKey ? 1 : -1) + n) % n;
        setScope ((SearchIndex::Scope) next);
        chips[next]->grabKeyboardFocus();
        return true;
    }

    return false;
}

//==============================================================================
void CommandPalette::activateSelected (ActivationKind kind)
{
    const auto* r = getSelected();

    if (r == nullptr)
        return;

    switch (r->type)
    {
        case Row::Type::recentQuery:
        case Row::Type::didYouMean:
            setQuery (r->text);
            return;

        case Row::Type::searchHelp:
        {
            const auto q = r->text;
            close (false);
            navigator.performAction ("__searchHelp:" + q);
            return;
        }

        case Row::Type::item:
            confirmOrActivate (kind);
            return;

        default:
            return;
    }
}

void CommandPalette::confirmOrActivate (ActivationKind kind)
{
    const auto* r = getSelected();

    if (r == nullptr || r->item == nullptr)
        return;

    const auto* item = r->item;
    const auto query = field.getText();

    if (r->valueReading && kind != ActivationKind::goOnly)
    {
        auto outcome = navigator.applyValue (reading);

        if (outcome.status == SearchNavigator::Outcome::Status::done)
        {
            navigator.getIndex().recordActivation (item->id, query);

            if (kind == ActivationKind::keepOpen)
            {
                // Shift+Enter: stays open with the value selected.
                setFooter (outcome.message, false);
                rebuildRows();
                const auto text = field.getText();
                const int valueStart = text.length() - reading.value.text.length();
                field.setHighlightedRegion ({ juce::jmax (0, valueStart), text.length() });
                return;
            }

            close (false);
            return;
        }

        handleOutcome (outcome, item);
        return;
    }

    const bool confirmed = pendingConfirmId == item->id;
    const auto outcome = navigator.activate (*item, kind, confirmed);

    if (outcome.status == SearchNavigator::Outcome::Status::done
          || outcome.status == SearchNavigator::Outcome::Status::keptOpen)
        navigator.getIndex().recordActivation (item->id, query);

    handleOutcome (outcome, item);
}

void CommandPalette::handleOutcome (const SearchNavigator::Outcome& outcome, const SearchItem* item)
{
    using S = SearchNavigator::Outcome::Status;

    switch (outcome.status)
    {
        case S::done:
            if (isVisible())
            {
                close (false);

                if (outcome.message.isNotEmpty())
                    navigator.postNotice (outcome.message);
            }

            return;

        case S::keptOpen:
            setFooter (outcome.message, outcome.warning);
            rebuildRows();
            return;

        case S::needsConfirm:
            pendingConfirmId = item != nullptr ? item->id : juce::String();
            pendingConfirmText = outcome.message;
            setFooter (outcome.message, true);
            announce (outcome.message);
            list.repaint();
            return;

        case S::refused:
        case S::failed:
        default:
            if (outcome.allowInline && item != nullptr)
                allowInlineId = item->id;

            setFooter (outcome.message, true);

            if (outcome.message.isNotEmpty())
                announce (outcome.message);

            list.repaint();
            return;
    }
}

//==============================================================================
juce::PopupMenu CommandPalette::buildSecondaryMenu (int row)
{
    juce::PopupMenu menu;

    if (! juce::isPositiveAndBelow (row, (int) rows.size()) || rows[(size_t) row].item == nullptr)
        return menu;

    const auto& r = rows[(size_t) row];

    // 5: a parameter row is exactly the control's own right-click menu.
    if (r.item->kind == ItemKind::parameter)
        return buildParameterContextMenu (navigator.getProcessor(), r.item->target);

    if (auto* provider = navigator.getIndex().getProviderFor (*r.item))
    {
        const auto actions = provider->secondaryActions (*r.item);

        for (int i = 0; i < actions.size(); ++i)
            menu.addItem (100 + i, actions[i]);
    }

    if (r.item->kind == ItemKind::choiceOption)
        menu.addItem (98, SearchCatalog::text ("search.action.goTo"));

    if (menu.getNumItems() > 0)
        menu.addSeparator();

    menu.addItem (1, SearchCatalog::text ("search.action.copyId"));
    menu.addItem (2, SearchCatalog::text ("search.action.removeRecent"), RecentStore::get().find (r.item->id) != nullptr);
    return menu;
}

void CommandPalette::runSecondary (int row, int result)
{
    if (result == 0 || ! juce::isPositiveAndBelow (row, (int) rows.size()) || rows[(size_t) row].item == nullptr)
        return;

    const auto* item = rows[(size_t) row].item;

    if (item->kind == ItemKind::parameter)
    {
        applyParameterMenuResult (result, *this, navigator.getProcessor(), item->target, [this] { list.repaint(); });
        return;
    }

    if (result == 1)
    {
        juce::SystemClipboard::copyTextToClipboard (item->id);
        return;
    }

    if (result == 2)
    {
        RecentStore::get().remove (item->id);
        rebuildRows();
        return;
    }

    if (result == 98)
    {
        handleOutcome (navigator.activate (*item, ActivationKind::goOnly, false), item);
        return;
    }

    if (result >= 100)
    {
        const auto outcome = navigator.activate (*item, ActivationKind::secondary, true, result - 100);

        if (outcome.status == SearchNavigator::Outcome::Status::done)
            navigator.getIndex().recordActivation (item->id, field.getText());

        handleOutcome (outcome, item);
    }
}

void CommandPalette::showSecondaryMenu (int row)
{
    auto menu = buildSecondaryMenu (row);

    if (menu.getNumItems() == 0)
        return;

    juce::Component::SafePointer<CommandPalette> safe (this);
    const auto rowArea = list.getRowPosition (row, true);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                  .withTargetScreenArea (rowArea + list.getScreenPosition()),
                        [safe, row] (int result)
    {
        if (safe != nullptr)
            safe->runSecondary (row, result);
    });
}

//==============================================================================
void CommandPalette::mouseDown (const juce::MouseEvent& e)
{
    // 6.2: clicking the scrim closes the palette.
    if (! box.contains (e.getPosition()))
        close (true);
}

void CommandPalette::timerCallback()
{
    // 11: parameter values refresh at 10 Hz, visible rows only (the list
    // repaints only the rows it shows).
    list.repaint();

    if (announcePending && juce::Time::getMillisecondCounterHiRes() - lastTypedMs >= 400.0)
        announceResultsNow();
}

} // namespace luthier::search
