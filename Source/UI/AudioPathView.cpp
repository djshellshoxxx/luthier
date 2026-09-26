#include "AudioPathView.h"
#include "Theme.h"
#include "../PluginProcessor.h"

namespace luthier
{

AudioPathView::AudioPathView (LuthierAudioProcessor& p) : processor (p)
{
    setTitle ("Audio path");
    refresh();
    motion.startTimerHz (*this, 4);   // cpu-quality-modes 6
}

AudioPathView::~AudioPathView()
{
    motion.stopTimer();
}

std::vector<AudioPathView::Stage> AudioPathView::readStages() const
{
    auto& state = processor.getState();

    auto value = [&state] (const juce::String& id) -> float
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id)))
            return p->convertFrom0to1 (p->getValue());
        return 0.0f;
    };

    auto text = [&state] (const juce::String& id)
    {
        if (auto* p = state.getParameter (id))
            return p->getCurrentValueAsText();
        return juce::String();
    };

    std::vector<Stage> out;
    auto& engine = processor.getEngine();

    // cpu-quality-modes 5: what the CPU quality level has changed on the path.
    const int strings = engine.getNumStrings();
    const int asleep = engine.getSleepingStringCount();
    out.push_back ({ "Strings", juce::String (strings) + " strings" + (asleep > 0 ? ", " + juce::String (asleep) + " asleep" : juce::String()), true });

    const int bodyMode = juce::roundToInt (value (ParamIDs::bodyMode));
    const auto& body = engine.getBodyEngine();
    out.push_back ({ "Body", text (ParamIDs::bodyMode) + (body.getRunningModeCount() < body.getNumModes()
                                                            ? juce::String (", ") + juce::String (body.getRunningModeCount()) + " modes" : juce::String()),
                     bodyMode != 3 });

    out.push_back ({ "Pickups", text (ParamIDs::pickupSelector), true });
    out.push_back ({ "Circuit", "volume and tone", true });

    auto rack = [&] (bool post)
    {
        int on = 0;

        for (int i = 0; i < EffectsChain::kNumSlots; ++i)
            if (juce::roundToInt (value (ParamIDs::slotType (post, i))) != 0 && value (ParamIDs::slotBypass (post, i)) < 0.5f)
                ++on;

        return on;
    };

    const int pre = rack (false), post = rack (true);
    out.push_back ({ "Pre FX", pre == 0 ? juce::String ("empty") : juce::String (pre) + (pre == 1 ? " pedal" : " pedals"), pre > 0 });
    out.push_back ({ "Amp", text (ParamIDs::ampModel) + ", " + juce::String (engine.getEffectiveAmpOversampling()) + "x",
                     value (ParamIDs::ampStandby) < 0.5f });
    out.push_back ({ "Post FX", post == 0 ? juce::String ("empty") : juce::String (post) + (post == 1 ? " pedal" : " pedals"), post > 0 });
    out.push_back ({ "Cabinet", text (ParamIDs::cabType), value (ParamIDs::cabOn) > 0.5f });
    out.push_back ({ "Room", text (ParamIDs::roomSize), value (ParamIDs::roomOn) > 0.5f });
    out.push_back ({ "Master", "limiter at -0.3 dBFS", true });
    return out;
}

juce::String AudioPathView::describeFlags() const
{
    // gui-integration 5: "Workshop / Slide / advanced-ranges booleans mirror".
    juce::StringArray families;

    for (int f = 0; f < (int) RangeFamily::numFamilies; ++f)
        if (processor.getRanges().isFamilyAdvanced ((RangeFamily) f))
            families.add (getRangeFamilyName ((RangeFamily) f));

    const bool slide = processor.getState().getRawParameterValue (ParamIDs::slideGuitar)->load() > 0.5f;

    return "CPU quality: " + QualityController::levelName (processor.getAppliedQualityLevel())   // cpu-quality-modes 5
         + (processor.getQualityController().isAuto() ? " (Auto)" : "") + "    "
         + "Workshop edit: " + juce::String (processor.isGuitarEdited() ? "yes" : "no")
         + "    Slide Mode: " + (slide ? "on" : "off")
         + "    Advanced ranges: " + (families.isEmpty() ? juce::String ("off") : families.joinIntoString (", "));
}

void AudioPathView::refresh()
{
    auto next = readStages();
    auto nextFlags = describeFlags();

    bool changed = nextFlags != flags || next.size() != stages.size();

    for (size_t i = 0; ! changed && i < next.size(); ++i)
        changed = next[i].active != stages[i].active || next[i].detail != stages[i].detail;

    if (changed)
    {
        stages = std::move (next);
        flags = nextFlags;

        juce::StringArray lit;
        for (auto& s : stages)
            if (s.active)
                lit.add (s.name);

        setDescription ("Active: " + lit.joinIntoString (", ") + ". " + flags);
        repaint();
    }
}

void AudioPathView::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6
    auto b = getLocalBounds().toFloat();
    auto flagRow = b.removeFromBottom (18.0f);

    const int n = (int) stages.size();
    if (n == 0)
        return;

    // Two rows of boxes joined by arrows, in signal order.
    const int perRow = (n + 1) / 2;
    const float gap = 10.0f;
    const float w = (b.getWidth() - gap * (float) (perRow - 1)) / (float) perRow;
    const float h = juce::jmin (44.0f, (b.getHeight() - gap) * 0.5f);

    for (int i = 0; i < n; ++i)
    {
        // A snake: the second row runs right to left, so the signal reads on from where the first ended.
        const int row = i / perRow, col = row == 0 ? i % perRow : perRow - 1 - i % perRow;
        const juce::Rectangle<float> box (b.getX() + (float) col * (w + gap), b.getY() + (float) row * (h + gap), w, h);
        const auto& s = stages[(size_t) i];

        g.setColour (s.active ? Palette::accent.withAlpha (0.18f) : Palette::panelSunken);
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (s.active ? Palette::accent : Palette::edge);
        g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, s.active ? 1.5f : 1.0f);

        g.setColour (s.active ? Palette::textPrimary : Palette::textDisabled);
        g.setFont (Fonts::ui (11.5f, true));
        g.drawText (s.name, box.withHeight (h * 0.5f).reduced (4.0f, 0.0f), juce::Justification::centredBottom, true);
        g.setFont (Fonts::ui (10.0f));
        g.setColour (s.active ? Palette::textMuted : Palette::textDisabled);
        g.drawText (s.detail, box.withTrimmedTop (h * 0.5f).reduced (4.0f, 0.0f), juce::Justification::centredTop, true);

        // The arrow to the next stage.
        if (i + 1 < n)
        {
            g.setColour (Palette::edgeBright);

            if ((i + 1) % perRow != 0)
            {
                const float y = box.getCentreY();
                if (row == 0)
                    g.drawArrow ({ box.getRight() + 1.0f, y, box.getRight() + gap - 1.0f, y }, 1.0f, 5.0f, 5.0f);
                else
                    g.drawArrow ({ box.getX() - 1.0f, y, box.getX() - gap + 1.0f, y }, 1.0f, 5.0f, 5.0f);
            }
            else
            {
                // Down to the start of the next row.
                const juce::Point<float> from { box.getCentreX(), box.getBottom() + 1.0f };
                g.drawArrow ({ from.x, from.y, from.x, from.y + gap - 2.0f }, 1.0f, 5.0f, 5.0f);
            }
        }
    }

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::mono (10.0f));
    g.drawText (flags, flagRow, juce::Justification::centredLeft, true);
}

} // namespace luthier
