#include "JamWidgets.h"
#include "JamUiText.h"
#include "Theme.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

//==============================================================================
JamDots::JamDots()
    : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox)
{
    setRange (1.0, 5.0, 1.0);
    setValue (3.0, juce::dontSendNotification);
    setWantsKeyboardFocus (true);
}

void JamDots::attachTo (LuthierAudioProcessor& processor, const juce::String& parameterId, const juce::String& label)
{
    if (auto* p = processor.getState().getParameter (parameterId))
        setTooltip (label + ": " + p->getName (64) + ". 1 is sparse, 5 is everything the style has.");

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.getState(), parameterId, *this);
    AccessibleSetup::configureSlider (*this, label);
}

int JamDots::valueAtX (float x) const noexcept
{
    const int count = juce::jmax (1, (int) std::round (getMaximum() - getMinimum()) + 1);
    const float cell = (float) getWidth() / (float) count;
    return (int) getMinimum() + juce::jlimit (0, count - 1, (int) (x / juce::jmax (1.0f, cell)));
}

void JamDots::paint (juce::Graphics& g)
{
    const int count = juce::jmax (1, (int) std::round (getMaximum() - getMinimum()) + 1);
    const float cell = (float) getWidth() / (float) count;
    const float d = juce::jmin (cell - 4.0f, (float) getHeight() - 4.0f, 18.0f);
    const int value = (int) std::round (getValue());

    for (int i = 0; i < count; ++i)
    {
        const int v = (int) getMinimum() + i;
        auto dot = juce::Rectangle<float> (d, d).withCentre ({ cell * ((float) i + 0.5f), (float) getHeight() * 0.5f });
        const bool on = v <= value;

        g.setColour (on ? Palette::accent : Palette::panelSunken);
        g.fillEllipse (dot);
        g.setColour (v == value ? Palette::accentBright : Palette::edge);
        g.drawEllipse (dot, v == value ? 2.0f : 1.0f);

        // Numbers as well as fill (12: glyphs, not colour alone).
        g.setColour (on ? Palette::plateText : Palette::textMuted);
        g.setFont (Fonts::ui (juce::jmax (8.0f, d * 0.6f), true));
        g.drawText (juce::String (v), dot, juce::Justification::centred);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Palette::accentBright.withAlpha (0.6f));
        g.drawRect (getLocalBounds(), 1);
    }
}

void JamDots::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::Slider::mouseDown (e);
        return;
    }

    setValue (valueAtX ((float) e.x), juce::sendNotificationSync);
}

void JamDots::mouseDrag (const juce::MouseEvent& e)
{
    setValue (valueAtX ((float) e.x), juce::sendNotificationSync);
}

//==============================================================================
JamPill::JamPill (LuthierAudioProcessor& p, Mode m)
    : juce::Button ("JAM"), processor (p), mode (m)
{
    setTooltip (mode == Mode::live ? "The Jam band. Tap to start or stop it; hold for a fill."
                                   : "The Jam band. The first press arms it (it starts with your first chord); "
                                     "then a press starts or stops it.");
    AccessibleSetup::configureButton (*this, "Jam band", "Arms, starts and stops the backing band");
    refresh();
    startTimerHz (30);
}

JamPill::~JamPill()
{
    stopTimer();
}

void JamPill::mouseDown (const juce::MouseEvent& e)
{
    downAt = juce::Time::getMillisecondCounterHiRes();
    juce::Button::mouseDown (e);
}

void JamPill::clicked()
{
    const bool longPress = downAt > 0.0 && juce::Time::getMillisecondCounterHiRes() - downAt >= kLongPressMs;
    downAt = 0.0;
    press (longPress);
}

void JamPill::press (bool longPress)
{
    auto* enabled = processor.getState().getParameter (ParamIDs::jamEnabled);
    const bool isEnabled = enabled != nullptr && enabled->getValue() > 0.5f;

    if (mode == Mode::live && longPress)
        processor.jamFill();
    else if (mode == Mode::easy && ! isEnabled)
        processor.jamArmToggle();   // 8.2: the first press arms
    else
        processor.jamStartStop();

    refresh();
}

void JamPill::refresh()
{
    auto* enabled = processor.getState().getParameter (ParamIDs::jamEnabled);
    const bool isEnabled = enabled != nullptr && enabled->getValue() > 0.5f;

    JamStatus status;
    state = isEnabled && processor.getJam().getStatusChannel().read (status) ? status.state
          : isEnabled ? JamState::armed : JamState::off;

    const auto text = JamUiText::pillText (state, isEnabled);

    if (text != pillText)
    {
        pillText = text;
        setButtonText (pillText);
        setDescription ("Jam band: " + pillText.toLowerCase());
        repaint();
    }
}

void JamPill::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = r.getHeight() * 0.5f;

    const auto fill = state == JamState::playing  ? Palette::success.withAlpha (0.85f)
                    : state == JamState::counting ? Palette::accent
                    : state == JamState::ending   ? Palette::warning
                    : state == JamState::armed    ? Palette::accentDim
                                                  : Palette::panelSunken;

    g.setColour (down ? fill.darker (0.2f) : highlighted ? fill.brighter (0.1f) : fill);
    g.fillRoundedRectangle (r, corner);
    g.setColour (hasKeyboardFocus (false) ? Palette::accentBright : Palette::edgeBright);
    g.drawRoundedRectangle (r, corner, hasKeyboardFocus (false) ? 2.0f : 1.0f);

    // A glyph as well as the colour: a dot armed, a triangle playing, a bar ending.
    auto glyph = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.3f);
    g.setColour (Palette::textPrimary);

    if (state == JamState::playing || state == JamState::counting)
    {
        juce::Path p;
        p.addTriangle (glyph.getX(), glyph.getY(), glyph.getX(), glyph.getBottom(), glyph.getRight(), glyph.getCentreY());
        g.fillPath (p);
    }
    else if (state == JamState::ending)
        g.fillRect (glyph.reduced (glyph.getWidth() * 0.15f, 0.0f));
    else if (state == JamState::armed)
        g.fillEllipse (glyph.reduced (glyph.getWidth() * 0.2f));
    else
        g.drawEllipse (glyph.reduced (glyph.getWidth() * 0.2f), 1.0f);

    g.setFont (Fonts::ui (juce::jmin (13.0f, r.getHeight() * 0.45f), true));
    g.drawText (pillText, r.withTrimmedRight (4.0f), juce::Justification::centred);
}

//==============================================================================
bool JamShortcuts::handle (LuthierAudioProcessor& processor, const juce::KeyPress& key, juce::Component* focused)
{
    if (dynamic_cast<juce::TextEditor*> (focused) != nullptr)
        return false;

    auto& shortcuts = AccessibilitySettings::get();
    auto is = [&shortcuts, &key] (const char* id)
    {
        const auto* binding = shortcuts.findShortcut (id);
        return binding != nullptr && binding->key == key;
    };

    if (is ("jamStartStop")) { processor.jamStartStop(); return true; }
    if (is ("jamFill"))      { processor.jamFill();      return true; }
    if (is ("jamArm"))       { processor.jamArmToggle(); return true; }
    return false;
}

//==============================================================================
JamStripGroup::JamStripGroup (LuthierAudioProcessor& p)
    : processor (p), pill (p, JamPill::Mode::easy)
{
    addAndMakeVisible (pill);

    style.setLabelVisible (false);
    style.attachTo (processor, ParamIDs::jamStyle, "The Jam band's style");
    addAndMakeVisible (style);

    intensity.attachTo (processor, ParamIDs::jamIntensity, "Band intensity");
    addAndMakeVisible (intensity);

    bandVolume.setTooltip ("Band: the Jam band's volume");
    bandAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.getState(), ParamIDs::jamVolume, bandVolume);
    AccessibleSetup::configureSlider (bandVolume, "Band volume", " dB");
    addAndMakeVisible (bandVolume);
}

void JamStripGroup::resized()
{
    auto r = getLocalBounds();
    const int h = juce::jmin (32, r.getHeight());

    // Narrower than preferred: the style box gives way first, then the controls
    // drop off from the right; one that does not fit is hidden, never hung
    // outside the group. The pill (the band's state) goes last.
    const int styleW = juce::jmax (56, 118 - juce::jmax (0, preferredWidth - getWidth()));

    auto place = [&r] (juce::Component& c, int width, int height, bool gap)
    {
        const int need = width + (gap ? 4 : 0);
        const bool fits = r.getWidth() >= need && r.getHeight() > 0;
        c.setVisible (fits);

        if (! fits)
        {
            r.setWidth (0);   // nothing after a dropped control
            return;
        }

        if (gap)
            r.removeFromLeft (4);

        c.setBounds (r.removeFromLeft (width).withSizeKeepingCentre (width, juce::jmin (height, r.getHeight())));
    };

    place (pill, 88, h, false);
    place (style, styleW, 26, true);
    place (intensity, 76, 22, true);
    place (bandVolume, 40, juce::jmax (1, r.getHeight() - juce::jmin (10, r.getHeight() / 4)), true);
}

void JamStripGroup::paint (juce::Graphics& g)
{
    // The mini-knob's name under it.
    g.setColour (Palette::textMuted);
    g.setFont (Fonts::ui (9.0f));
    if (bandVolume.isVisible())
        g.drawText ("Band", bandVolume.getBounds().withY (bandVolume.getBottom()).withHeight (10), juce::Justification::centred);
}

} // namespace luthier
