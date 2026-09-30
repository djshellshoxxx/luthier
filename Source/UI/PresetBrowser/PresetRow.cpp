#include "PresetRow.h"
#include "PresetBrowserPanel.h"
#include "../../PluginProcessor.h"

namespace luthier
{

PresetRow::PresetRow (PresetBrowserPanel& o) : owner (o)
{
    addAndMakeVisible (glyph);
    glyph.onClick = [this] { if (entry >= 0) owner.togglePreview (entry); };

    addAndMakeVisible (heart);
    heart.onClick = [this] { if (entry >= 0) owner.setFavourite (entry, heart.getToggleState()); };

    addAndMakeVisible (stars);
    stars.onChange = [this] (int n) { if (entry >= 0) owner.setRating (entry, n); };

    setInterceptsMouseClicks (true, true);
}

void PresetRow::setEntry (int e, bool isSelected)
{
    entry = e;
    selected = isSelected;

    auto& index = owner.getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return;

    const auto& item = index[entry];
    auto& prefs = PresetLibraryPrefs::get();

    heart.setToggleState (prefs.isFavourite (item.key), juce::dontSendNotification);
    heart.setTitle ("Favourite " + item.info.name);
    stars.setRating (prefs.getRating (item.key));
    stars.setTitle ("Rating for " + item.info.name);
    glyph.setTitle ("Preview " + item.info.name);
    glyph.setReducedMotion (AccessibilitySettings::get().isReducedMotion());

    // 10: the row's name is its summary.
    setTitle (owner.describeRow (entry));
    setDescription (item.info.description);

    refreshPlayState();
    resized();
    repaint();
}

void PresetRow::refreshPlayState()
{
    float p = -1.0f;
    juce::String failure;
    const auto state = owner.glyphStateFor (entry, p, failure);
    glyph.setState (state, p, failure);

    const bool reduced = AccessibilitySettings::get().isReducedMotion();
    const float shown = state == PlayGlyph::State::playing && ! reduced ? p : -1.0f;

    if (std::abs (shown - progress) > 0.005f || (shown < 0.0f) != (progress < 0.0f))
    {
        progress = shown;
        repaint (waveformArea());
    }
}

juce::Rectangle<int> PresetRow::waveformArea() const
{
    auto r = getLocalBounds().reduced (0, 5);
    r.removeFromRight (64);   // stars
    return r.removeFromRight (70).reduced (4, 0);
}

void PresetRow::resized()
{
    auto r = getLocalBounds().reduced (2, 3);
    glyph.setBounds (r.removeFromLeft (22));
    heart.setBounds (r.removeFromLeft (20));
    stars.setBounds (r.removeFromRight (60).reduced (0, 3));
}

void PresetRow::paint (juce::Graphics& g)
{
    auto& index = owner.getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return;

    const auto& item = index[entry];

    if (selected)
    {
        g.setColour (Palette::accent.withAlpha (0.16f));
        g.fillAll();
        g.setColour (Palette::accent);
        g.fillRect (0, 0, 2, getHeight());
    }

    auto r = getLocalBounds().reduced (2, 0);
    r.removeFromLeft (46);          // glyph and heart
    r.removeFromRight (64 + 70);    // stars and waveform

    // A lock marks a Pro preset in the Free build; "~" an approximate render.
    juce::String name = item.info.name;

    if (item.approximate)
        name = "~ " + name;

    if (item.locked)
        name = juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x92 ")) + name;

    g.setColour (selected ? Palette::accent : Palette::textPrimary);
    g.setFont (Fonts::ui (12.5f, selected));

    if (owner.isAdvancedLayout())
    {
        // Name, Category, Guitar, Amp, Tags.
        const int w = r.getWidth();
        auto col = [&r, w] (float fraction) { return r.removeFromLeft ((int) (w * fraction)); };

        g.drawText (name, col (0.30f), juce::Justification::centredLeft, true);
        g.setFont (Fonts::ui (10.5f));
        g.setColour (Palette::textMuted);
        g.drawText (item.info.category, col (0.14f), juce::Justification::centredLeft, true);
        g.drawText (item.info.guitarName, col (0.20f), juce::Justification::centredLeft, true);
        g.drawText (item.info.ampName, col (0.16f), juce::Justification::centredLeft, true);

        juce::StringArray tags (item.info.tags);
        tags.addArray (item.descriptors);
        tags.removeDuplicates (true);
        g.setColour (Palette::secondary);
        g.drawText (tags.joinIntoString (" "), r, juce::Justification::centredLeft, true);
    }
    else
    {
        const auto nameArea = r.removeFromLeft (juce::jmin (r.getWidth() / 2 + 20, 200));
        g.drawText (name, nameArea, juce::Justification::centredLeft, true);

        // The first descriptors, then the author's tags, as quiet text.
        juce::StringArray words (item.descriptors);
        words.addArray (item.info.tags);
        words.removeDuplicates (true);
        g.setFont (Fonts::ui (10.5f));
        g.setColour (Palette::secondary);
        g.drawText (words.joinIntoString (" "), r.reduced (4, 0), juce::Justification::centredLeft, true);
    }

    if (item.preview == PresetIndex::PreviewState::ready || item.preview == PresetIndex::PreviewState::stale
        || item.isAnalysed())
        paintPeaks (g, waveformArea().toFloat(), item.peaks, progress);
    else if (item.preview == PresetIndex::PreviewState::rendering || item.preview == PresetIndex::PreviewState::queued)
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (9.5f));
        g.drawText (AccessibilitySettings::get().isReducedMotion() ? "Preparing" : "Preparing preview...",
                    waveformArea(), juce::Justification::centred, true);
    }
}

void PresetRow::mouseEnter (const juce::MouseEvent&)
{
    if (entry >= 0)
        owner.rowHovered (entry);
}

void PresetRow::mouseExit (const juce::MouseEvent& e)
{
    // Moving onto the row's own glyph or stars is still the row.
    if (entry >= 0 && ! getLocalBounds().contains (e.getEventRelativeTo (this).getPosition()))
        owner.rowUnhovered (entry);
}

void PresetRow::mouseDown (const juce::MouseEvent& e)
{
    // The row lives inside the ListBox's row: select through it.
    owner.keyboardSelecting = false;
    owner.selectEntry (entry, false);
    juce::ignoreUnused (e);
}

void PresetRow::mouseUp (const juce::MouseEvent& e)
{
    // 4.4: with "Click only", a click anywhere on the row plays it.
    if (entry >= 0 && ! owner.getLibrary().getSettings().hoverTrigger && e.mouseWasClicked())
        owner.togglePreview (entry);
}

void PresetRow::mouseDoubleClick (const juce::MouseEvent&)
{
    if (entry >= 0)
        owner.loadEntry (entry);
}

} // namespace luthier
