#include "PresetFilterSidebar.h"
#include "PresetBrowserPanel.h"

namespace luthier
{

namespace
{
    constexpr int kChipHeight = 22;
    constexpr int kHeadingHeight = 18;
    constexpr int kGap = 4;

    const juce::StringArray& sourceNames()
    {
        static const juce::StringArray names { "Factory", "User", "Pack", "Extra" };
        return names;
    }

    const juce::StringArray& toneWords()
    {
        // The sound words of 6.3's vocabulary (acoustic, nylon, bass and slide
        // are FAMILY and TECHNIQUES territory).
        static const juce::StringArray words { "warm", "bright", "clean", "crunchy", "high-gain", "fuzzy", "djent",
                                               "spacious", "dry", "twangy", "percussive", "compressed", "fat", "jazz" };
        return words;
    }
}

PresetFilterSidebar::PresetFilterSidebar (PresetBrowserPanel& o) : owner (o)
{
    setTitle ("Filters");

    addGroup ("SOURCE", sourceNames());
    addGroup ("FAMILY", { "Electric", "Acoustic", "Classical", "Bass" });
    addGroup ("GENRE", ToneDescriptors::genreList());

    juce::StringArray techniques { "Uses Techniques" };

    for (int t = 0; t < PresetFeatures::numTechniques; ++t)
        techniques.add (PresetFeatures::getTechniqueName (t));

    addGroup ("TECHNIQUES", techniques);
    addGroup ("TONE", toneWords());
    addGroup ("LIBRARY", { "Favourites", "Recent" });

    ratingLabel.setText ("RATING >=", juce::dontSendNotification);
    ratingLabel.setFont (Fonts::label());
    ratingLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible (ratingLabel);

    ratingBox.addItem ("Any", 1);

    for (int s = 1; s <= 5; ++s)
        ratingBox.addItem (juce::String (s) + (s == 1 ? " star" : " stars"), s + 1);

    ratingBox.setTitle ("Minimum rating");
    ratingBox.onChange = [this]
    {
        if (updating)
            return;

        owner.getFilters().minRating = juce::jmax (0, ratingBox.getSelectedId() - 1);
        owner.filtersChanged();
    };
    addAndMakeVisible (ratingBox);

    refresh();
}

PresetFilterSidebar::Group& PresetFilterSidebar::addGroup (const juce::String& title, const juce::StringArray& labels)
{
    auto* group = groups.add (new Group());
    group->title = title;

    for (const auto& label : labels)
    {
        auto* chip = group->chips.add (new ChipButton (label));
        chip->setTitle (title.toLowerCase() + " " + label);
        chip->onClick = [this, group, chip] { chipClicked (*group, *chip); };
        addAndMakeVisible (chip);
    }

    return *group;
}

void PresetFilterSidebar::chipClicked (Group& group, ChipButton& chip)
{
    if (updating)
        return;

    auto& f = owner.getFilters();
    const auto text = chip.getButtonText();
    const bool on = chip.getToggleState();

    if (group.title == "SOURCE")
    {
        const int i = sourceNames().indexOf (text);

        if (i >= 0)
            f.sources[(size_t) i] = on;
    }
    else if (group.title == "FAMILY")
    {
        f.family = -1;   // the sidebar uses the multi-select list
        if (on) f.families.addIfNotAlreadyThere (text); else f.families.removeString (text);
    }
    else if (group.title == "GENRE")
    {
        if (on) f.genres.addIfNotAlreadyThere (text); else f.genres.removeString (text);
    }
    else if (group.title == "TECHNIQUES")
    {
        if (text == "Uses Techniques")
            f.usesTechniques = on;
        else
            for (int t = 0; t < PresetFeatures::numTechniques; ++t)
                if (text == PresetFeatures::getTechniqueName (t))
                    f.techniques[(size_t) t] = on;
    }
    else if (group.title == "TONE")
    {
        if (on) f.tones.addIfNotAlreadyThere (text); else f.tones.removeString (text);
    }
    else if (group.title == "LIBRARY")
    {
        if (text == "Favourites") f.favourites = on;
        else                      f.recent = on;
    }

    owner.filtersChanged();
}

void PresetFilterSidebar::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    const auto& f = owner.getFilters();

    for (auto* group : groups)
        for (auto* chip : group->chips)
        {
            const auto text = chip->getButtonText();
            bool on = false;

            if (group->title == "SOURCE")          on = f.sources[(size_t) juce::jmax (0, sourceNames().indexOf (text))];
            else if (group->title == "FAMILY")     on = f.families.contains (text)
                                                        || (f.family >= 0 && text == PresetFeatures::getFamilyName (f.family));
            else if (group->title == "GENRE")      on = f.genres.contains (text);
            else if (group->title == "TONE")       on = f.tones.contains (text);
            else if (group->title == "LIBRARY")    on = text == "Favourites" ? f.favourites : f.recent;
            else if (group->title == "TECHNIQUES")
            {
                on = text == "Uses Techniques" && f.usesTechniques;

                for (int t = 0; t < PresetFeatures::numTechniques; ++t)
                    if (text == PresetFeatures::getTechniqueName (t))
                        on = f.techniques[(size_t) t];
            }

            chip->setToggleState (on, juce::dontSendNotification);
        }

    ratingBox.setSelectedId (f.minRating + 1, juce::dontSendNotification);
}

ChipButton* PresetFilterSidebar::findChip (const juce::String& groupTitle, const juce::String& text) const
{
    for (auto* group : groups)
        if (group->title == groupTitle)
            for (auto* chip : group->chips)
                if (chip->getButtonText() == text)
                    return chip;

    return nullptr;
}

int PresetFilterSidebar::layoutGroups (int width, bool apply)
{
    int y = 4;

    for (auto* group : groups)
    {
        group->top = y;
        y += kHeadingHeight;
        int x = 0;

        for (auto* chip : group->chips)
        {
            const int w = juce::jmin (width, chip->getIdealWidth());

            if (x > 0 && x + w > width)
            {
                x = 0;
                y += kChipHeight + kGap;
            }

            if (apply)
                chip->setBounds (x, y, w, kChipHeight);

            x += w + kGap;
        }

        y += kChipHeight + 2 * kGap;
    }

    if (apply)
    {
        ratingLabel.setBounds (0, y, 70, kChipHeight);
        ratingBox.setBounds (72, y, juce::jmax (40, width - 72), kChipHeight);
    }

    return y + kChipHeight + 8;
}

int PresetFilterSidebar::getIdealHeight (int width) const
{
    return const_cast<PresetFilterSidebar*> (this)->layoutGroups (width, false);
}

void PresetFilterSidebar::resized()
{
    layoutGroups (getWidth(), true);
}

void PresetFilterSidebar::paint (juce::Graphics& g)
{
    g.setColour (Palette::textMuted);

    for (auto* group : groups)
        Fonts::drawTrackedText (g, group->title, { 0, group->top, getWidth(), kHeadingHeight - 2 },
                                juce::Justification::centredLeft);
}

} // namespace luthier
