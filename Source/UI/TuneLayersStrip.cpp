#include "TuneLayersStrip.h"
#include "../Tune/TuneMelody.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    constexpr int kRow = 26;
    constexpr int kGap = 4;
    constexpr int kLayerGroup = 8000;   // + section * 8 + layer * 2 + (0 volume, 1 pan)

    const char* bassLabelFor (BassMode m)
    {
        switch (m)
        {
            case BassMode::off:       return "Off";
            case BassMode::root:      return "Root";
            case BassMode::rootFifth: return "Root-Fifth";
            case BassMode::walking:   return "Walking";
            case BassMode::genre:     return "Genre";
            case BassMode::manual:    return "Manual (edit in the roll)";
            case BassMode::numModes:  break;
        }

        return "";
    }
}

TuneLayersStrip::TuneLayersStrip (TuneSession& s, juce::StringArray fingerpickPatterns)
    : session (s), patterns (std::move (fingerpickPatterns))
{
    addAndMakeVisible (bassBox);

    for (int m = 0; m < (int) BassMode::numModes; ++m)
        bassBox.addItem (bassLabelFor ((BassMode) m), m + 1);

    bassBox.setTooltip ("The section's bass line (tune-builder 6). It plays through the instrument when it is a bass, "
                        "and goes out as MIDI otherwise.");
    AccessibleSetup::configureComboBox (bassBox, "Bass line");
    bassBox.onChange = [this] { if (! updating && bassBox.getSelectedId() > 0) setBassMode ((BassMode) (bassBox.getSelectedId() - 1)); };

    const char* tips[] =
    {
        "Pad: sustained chord tones on a soft picked layer",
        "Arpeggio: a fingerpick pattern over the chords",
        "Countermelody: a second line that answers the melody",
        "Percussion: chucks and palm mutes as rhythm (no drums)"
    };

    for (int i = 0; i < 4; ++i)
    {
        auto& row = rows[(size_t) i];
        const auto type = (LayerType) i;

        addAndMakeVisible (row.toggle);
        addAndMakeVisible (row.volume);
        addAndMakeVisible (row.pan);

        row.toggle.setTooltip (tips[i]);
        AccessibleSetup::configureButton (row.toggle.getButton(), tips[i]);

        row.volume.setRange (0.0, 1.0, 0.01);
        row.pan.setRange (-1.0, 1.0, 0.01);
        row.pan.setDoubleClickReturnValue (true, 0.0);
        row.volume.setTooltip ("Layer volume");
        row.pan.setTooltip ("Layer pan");
        AccessibleSetup::configureSlider (row.volume, juce::String (getLayerTypeName (type)) + " volume");
        AccessibleSetup::configureSlider (row.pan, juce::String (getLayerTypeName (type)) + " pan");

        row.toggle.getButton().onClick = [this, type, &row] { setLayerEnabled (type, row.toggle.getButton().getToggleState()); };
        row.volume.onValueChange = [this, type, &row] { if (! updating) setLayerVolume (type, row.volume.getValue()); };
        row.pan.onValueChange = [this, type, &row] { if (! updating) setLayerPan (type, row.pan.getValue()); };
    }

    addAndMakeVisible (arpBox);
    arpBox.setTooltip ("The arpeggio's fingerpick pattern");
    AccessibleSetup::configureComboBox (arpBox, "Arpeggio pattern");

    for (int i = 0; i < patterns.size(); ++i)
        arpBox.addItem (patterns[i], i + 1);

    arpBox.onChange = [this] { if (! updating && arpBox.getSelectedId() > 0) setArpeggioPattern (patterns[arpBox.getSelectedId() - 1]); };

    addAndMakeVisible (regenerateButton);
    regenerateButton.setTooltip ("A new countermelody (edit it in the roll's Counter target)");
    AccessibleSetup::configureButton (regenerateButton, "Regenerate countermelody");
    regenerateButton.onClick = [this] { regenerateCountermelody(); };

    // FEAT-JAM (jam-mode 11).
    jamNote.setFont (Fonts::ui (10.0f));
    jamNote.setColour (juce::Label::textColourId, Palette::warning);
    jamNote.setTooltip ("The Jam band's drums are playing, so the tune's percussion layer is left out.");
    addChildComponent (jamNote);

    refresh();
}

void TuneLayersStrip::setPercussionReplaced (bool replaced)
{
    if (jamNote.isVisible() == replaced)
        return;

    jamNote.setVisible (replaced);
    resized();
}

int TuneLayersStrip::getPreferredHeight() const
{
    return 5 * (kRow + kGap);
}

void TuneLayersStrip::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    const auto* s = session.getTune().getSection (session.getSelectedSection());

    for (auto* c : getChildren())
        c->setEnabled (s != nullptr);

    if (s == nullptr)
        return;

    bassBox.setSelectedId ((int) s->bass.mode + 1, juce::dontSendNotification);

    for (int i = 0; i < 4; ++i)
    {
        auto& row = rows[(size_t) i];
        const auto* layer = s->findLayer ((LayerType) i);

        row.toggle.getButton().setToggleState (layer != nullptr && layer->enabled, juce::dontSendNotification);
        row.volume.setValue (layer != nullptr ? layer->volume : 0.8, juce::dontSendNotification);
        row.pan.setValue (layer != nullptr ? layer->pan : 0.0, juce::dontSendNotification);
    }

    const auto* arp = s->findLayer (LayerType::arpeggio);
    arpBox.setSelectedId (arp != nullptr ? patterns.indexOf (arp->patternId) + 1 : 0, juce::dontSendNotification);
    regenerateButton.setEnabled (s->findLayer (LayerType::countermelody) != nullptr);
}

bool TuneLayersStrip::setBassMode (BassMode mode)
{
    const int index = session.getSelectedSection();
    const bool changed = session.edit (TuneEditClass::sectionEdit, "Bass line",
                                       [index, mode] (Tune& t) { return t.setBassMode (index, mode); });
    refresh();
    return changed;
}

bool TuneLayersStrip::changeLayer (LayerType type, const juce::String& description,
                                   const std::function<void (TuneLayer&)>& edit, int group)
{
    const int index = session.getSelectedSection();
    const auto fallbackPattern = patterns.isEmpty() ? juce::String() : patterns[0];

    const bool changed = session.edit (TuneEditClass::sectionEdit, description,
                                       [index, type, &edit, fallbackPattern] (Tune& t)
    {
        const auto* s = t.getSection (index);

        if (s == nullptr)
            return false;

        TuneLayer layer;
        layer.type = type;

        if (const auto* existing = s->findLayer (type))
            layer = *existing;
        else if (type == LayerType::arpeggio)
            layer.patternId = fallbackPattern;

        const auto before = layer;
        edit (layer);

        // "Countermelody: auto-generated to complement the main melody" - on first switch-on.
        if (type == LayerType::countermelody && layer.enabled && layer.notes.empty())
            layer.notes = generateCountermelody (t, index, layer.seed);

        if (s->findLayer (type) != nullptr && layer == before)
            return false;

        return t.setLayer (index, layer);
    }, group);

    refresh();
    return changed;
}

bool TuneLayersStrip::setLayerEnabled (LayerType type, bool enabled)
{
    return changeLayer (type, enabled ? "Layer on" : "Layer off", [enabled] (TuneLayer& l) { l.enabled = enabled; });
}

bool TuneLayersStrip::setLayerVolume (LayerType type, double volume)
{
    const int group = kLayerGroup + session.getSelectedSection() * 8 + (int) type * 2;
    return changeLayer (type, "Layer volume", [volume] (TuneLayer& l) { l.volume = juce::jlimit (0.0, 1.0, volume); }, group);
}

bool TuneLayersStrip::setLayerPan (LayerType type, double pan)
{
    const int group = kLayerGroup + session.getSelectedSection() * 8 + (int) type * 2 + 1;
    return changeLayer (type, "Layer pan", [pan] (TuneLayer& l) { l.pan = juce::jlimit (-1.0, 1.0, pan); }, group);
}

bool TuneLayersStrip::setArpeggioPattern (const juce::String& pattern)
{
    return changeLayer (LayerType::arpeggio, "Arpeggio pattern", [pattern] (TuneLayer& l) { l.patternId = pattern; });
}

bool TuneLayersStrip::regenerateCountermelody()
{
    const int index = session.getSelectedSection();
    const auto* s = session.getTune().getSection (index);

    if (s == nullptr || s->findLayer (LayerType::countermelody) == nullptr)
        return false;

    return changeLayer (LayerType::countermelody, "Regenerate countermelody", [] (TuneLayer& l)
    {
        ++l.seed;
        l.notes.clear();   // regenerated with the new seed below
        l.enabled = true;
    });
}

void TuneLayersStrip::paint (juce::Graphics& g)
{
    g.setFont (Fonts::label());
    g.setColour (Palette::textMuted);
    g.drawText ("BASS", bassLabel, juce::Justification::centredLeft);

    for (size_t i = 0; i < 4; ++i)
    {
        g.drawText ("VOL", volumeLabels[i], juce::Justification::centredRight);
        g.drawText ("PAN", panLabels[i], juce::Justification::centredRight);
    }
}

void TuneLayersStrip::resized()
{
    auto area = getLocalBounds();

    {
        auto row = area.removeFromTop (kRow);
        bassLabel = row.removeFromLeft (84);
        bassBox.setBounds (row);
        area.removeFromTop (kGap);
    }

    for (size_t i = 0; i < 4; ++i)
    {
        auto row = area.removeFromTop (kRow);
        area.removeFromTop (kGap);

        rows[i].toggle.setBounds (row.removeFromLeft (84).reduced (0, 1));

        auto extra = row.removeFromRight (i == (size_t) LayerType::arpeggio || i == (size_t) LayerType::countermelody
                                            || (i == (size_t) LayerType::percussion && jamNote.isVisible())   // FEAT-JAM
                                            ? juce::jmin (130, row.getWidth() / 3) : 0);

        const int half = row.getWidth() / 2;
        auto vol = row.removeFromLeft (half);
        volumeLabels[i] = vol.removeFromLeft (34);
        rows[i].volume.setBounds (vol);
        panLabels[i] = row.removeFromLeft (34);
        rows[i].pan.setBounds (row);

        if (i == (size_t) LayerType::arpeggio)
            arpBox.setBounds (extra.reduced (2, 1));
        else if (i == (size_t) LayerType::countermelody)
            regenerateButton.setBounds (extra.reduced (2, 1));
        else if (i == (size_t) LayerType::percussion)
            jamNote.setBounds (extra.reduced (2, 1));   // FEAT-JAM
    }
}

} // namespace luthier
