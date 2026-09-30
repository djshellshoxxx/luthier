#include "PresetDetailPane.h"
#include "PresetBrowserPanel.h"
#include "../Guitar/GuitarRenderer.h"
#include "../../PluginProcessor.h"

namespace luthier
{

namespace
{
    constexpr int kThumbHeight = 92;

    void styleLabel (juce::Label& l, float size, juce::Colour c, bool bold = false)
    {
        l.setFont (Fonts::ui (size, bold));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (juce::Justification::topLeft);
        l.setMinimumHorizontalScale (1.0f);
    }
}

//==============================================================================
PresetDetailPane::SimilarRow::SimilarRow (PresetDetailPane& p, int e) : pane (p), entry (e)
{
    addAndMakeVisible (glyph);
    glyph.onClick = [this] { pane.owner.togglePreview (entry); };

    auto& index = pane.owner.getLibrary().getIndex();

    if (juce::isPositiveAndBelow (entry, index.size()))
    {
        glyph.setTitle ("Preview " + index[entry].info.name);
        setTitle (pane.owner.describeRow (entry));
    }
}

void PresetDetailPane::SimilarRow::resized()
{
    glyph.setBounds (getLocalBounds().removeFromLeft (20).reduced (1));
}

void PresetDetailPane::SimilarRow::paint (juce::Graphics& g)
{
    auto& index = pane.owner.getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return;

    const auto& e = index[entry];
    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (11.0f));

    auto text = e.info.name;

    if (e.locked)
        text = juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x92 ")) + text;

    g.drawText (text, getLocalBounds().withTrimmedLeft (24), juce::Justification::centredLeft, true);
}

void PresetDetailPane::SimilarRow::mouseDown (const juce::MouseEvent&)
{
    pane.owner.selectEntry (entry);
}

//==============================================================================
PresetDetailPane::PresetDetailPane (PresetBrowserPanel& o) : owner (o)
{
    setTitle ("Preset details");

    styleLabel (nameLabel, 15.0f, Palette::textPrimary, true);
    styleLabel (categoryLabel, 11.0f, Palette::textMuted);
    styleLabel (metaLabel, 11.0f, Palette::textMuted);
    styleLabel (descriptionLabel, 11.5f, Palette::textMuted);
    styleLabel (similarHeading, 10.5f, Palette::textMuted, true);
    similarHeading.setText ("SOUNDS LIKE", juce::dontSendNotification);

    for (auto* l : { &nameLabel, &categoryLabel, &metaLabel, &descriptionLabel, &similarHeading })
        addAndMakeVisible (l);

    addAndMakeVisible (moreLikeThis);
    moreLikeThis.setTooltip ("The 8 presets that sound most like this one (S)");
    moreLikeThis.onClick = [this] { if (entry >= 0) owner.showSoundsLike (entry); };
}

void PresetDetailPane::setEntry (int e)
{
    entry = e;
    auto& index = owner.getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
    {
        entry = -1;
        nameLabel.setText ({}, juce::dontSendNotification);
        categoryLabel.setText ({}, juce::dontSendNotification);
        metaLabel.setText ({}, juce::dontSendNotification);
        descriptionLabel.setText ({}, juce::dontSendNotification);
        chips.clear();
        similarRows.clear();
        thumbnail = {};
        moreLikeThis.setEnabled (false);
        repaint();
        return;
    }

    const auto& item = index[entry];
    nameLabel.setText (item.info.name, juce::dontSendNotification);
    categoryLabel.setText (item.info.category + (item.info.isFactory ? "  -  factory" : "  -  " + juce::String (item.source == PresetIndex::Source::user ? "user" : "extra")),
                           juce::dontSendNotification);

    juce::StringArray techniques;

    for (int t = 0; t < PresetFeatures::numTechniques; ++t)
        if (item.params.techniques[(size_t) t])
            techniques.add (PresetFeatures::getTechniqueName (t));

    juce::String meta;
    meta << "Author: " << (item.info.author.isNotEmpty() ? item.info.author : juce::String ("-")) << "\n"
         << "Guitar: " << item.info.guitarName << "\n"
         << "Amp: " << item.info.ampName;

    if (! techniques.isEmpty())
        meta << "\nTechniques: " << techniques.joinIntoString (", ");

    if (item.approximate)
        meta << "\n~ Rendered with a fallback guitar; loading will show which part is missing.";

    metaLabel.setText (meta, juce::dontSendNotification);
    descriptionLabel.setText (item.info.description, juce::dontSendNotification);
    moreLikeThis.setEnabled (item.isAnalysed());

    rebuildChips();
    updateThumbnail();
    refresh();
}

void PresetDetailPane::rebuildChips()
{
    chips.clear();
    auto& index = owner.getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return;

    const auto& item = index[entry];

    auto add = [this] (const juce::String& word, ChipButton::Style style)
    {
        auto* chip = chips.add (new ChipButton (word, style));
        chip->onClick = [this, word] { owner.addToSearch (word); };
        addAndMakeVisible (chip);
    };

    for (const auto& t : item.info.tags)
        add (t, ChipButton::Style::authorTag);

    for (const auto& d : item.descriptors)
        if (! item.info.tags.contains (d, true))
            add (d, ChipButton::Style::autoTag);

    resized();
}

void PresetDetailPane::updateThumbnail()
{
    auto& index = owner.getLibrary().getIndex();

    if (! juce::isPositiveAndBelow (entry, index.size()))
        return;

    const auto& item = index[entry];
    const auto path = LuthierAudioProcessor::getFactoryGuitarPath ((GuitarType) item.params.guitarType);
    const auto key = juce::String (item.params.guitarType);

    if (key == thumbnailKey)
        return;

    thumbnailKey = key;

    if (auto it = thumbnailCache.find (key); it != thumbnailCache.end())
    {
        thumbnail = it->second;
        repaint();
        return;
    }

    thumbnail = {};

    // guitar-illustration.md 15: the reduced-detail render of the preset's guitar.
    const auto file = LuthierAudioProcessor::resolveGuitarFile ("Factory/" + path);
    WorkshopGuitar guitar;
    PartLibrary::LoadReport report;

    if (path.isNotEmpty() && file.existsAsFile() && owner.getProcessor().getPartLibrary().loadGuitar (file, guitar, report))
    {
        GuitarRenderer::Options options;
        options.thumbnail = true;
        options.materials = Palette::textured;
        thumbnail = GuitarRenderer::render (guitar, 240, kThumbHeight, juce::Colours::transparentBlack, options);
    }

    thumbnailCache[key] = thumbnail;
    repaint();
}

void PresetDetailPane::refresh()
{
    similarRows.clear();

    if (entry >= 0 && owner.isAdvancedLayout())
        for (int e : owner.getSoundsLike (entry))
            addAndMakeVisible (similarRows.add (new SimilarRow (*this, e)));

    moreLikeThis.setVisible (! owner.isAdvancedLayout());
    similarHeading.setVisible (owner.isAdvancedLayout());
    refreshPlayState();
    resized();
}

void PresetDetailPane::refreshPlayState()
{
    for (auto* row : similarRows)
    {
        float p = 0.0f;
        juce::String failure;
        row->glyph.setReducedMotion (AccessibilitySettings::get().isReducedMotion());
        row->glyph.setState (owner.glyphStateFor (row->entry, p, failure), p, failure);
    }
}

void PresetDetailPane::resized()
{
    auto r = getLocalBounds().reduced (6);
    r.removeFromTop (kThumbHeight + 6);

    nameLabel.setBounds (r.removeFromTop (22));
    categoryLabel.setBounds (r.removeFromTop (16));
    r.removeFromTop (4);

    // Chips, wrapped.
    int x = r.getX(), y = r.getY();

    for (auto* chip : chips)
    {
        const int w = juce::jmin (r.getWidth(), chip->getIdealWidth());

        if (x > r.getX() && x + w > r.getRight())
        {
            x = r.getX();
            y += 24;
        }

        chip->setBounds (x, y, w, 20);
        x += w + 4;
    }

    r.setTop (chips.isEmpty() ? r.getY() : y + 26);

    const bool advanced = owner.isAdvancedLayout();
    const int similarHeight = advanced ? 18 + similarRows.size() * 20 : 0;

    auto bottom = r.removeFromBottom (advanced ? similarHeight : 28);

    if (advanced)
    {
        similarHeading.setBounds (bottom.removeFromTop (18));

        for (auto* row : similarRows)
            row->setBounds (bottom.removeFromTop (20));
    }
    else
    {
        moreLikeThis.setBounds (bottom.removeFromLeft (juce::jmin (140, bottom.getWidth())));
    }

    r.removeFromBottom (4);
    const int metaHeight = juce::jmin (r.getHeight() / 2, 64);
    metaLabel.setBounds (r.removeFromTop (metaHeight));
    descriptionLabel.setBounds (r);
}

void PresetDetailPane::paint (juce::Graphics& g)
{
    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), Metrics::panelCorner);

    const auto thumbArea = getLocalBounds().reduced (6).removeFromTop (kThumbHeight);

    if (thumbnail.isValid())
        g.drawImage (thumbnail, thumbArea.toFloat(), juce::RectanglePlacement::centred);
    else if (entry >= 0)
    {
        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (10.0f));
        g.drawText (owner.getLibrary().getIndex()[entry].info.guitarName, thumbArea, juce::Justification::centred);
    }
}

} // namespace luthier
