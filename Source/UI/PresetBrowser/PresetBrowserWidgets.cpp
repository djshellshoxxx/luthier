#include "PresetBrowserWidgets.h"

namespace luthier
{

//==============================================================================
ChipButton::ChipButton (const juce::String& text, Style s)
    : juce::Button (text), style (s)
{
    setClickingTogglesState (s == Style::filter);
    setTitle (text);
    setDescription (s == Style::autoTag ? "Detected from the sound" : juce::String());

    if (s == Style::autoTag)
        setTooltip ("Detected from the sound");
    else if (s == Style::authorTag)
        setTooltip ("A tag the preset's author chose. Click to search for it.");
}

int ChipButton::getIdealWidth() const
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (Fonts::ui (11.0f), getButtonText(), 0.0f, 0.0f);
    return (int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 18;
}

void ChipButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f, 1.5f);
    const float corner = r.getHeight() * 0.5f;

    // Filled for "on" filter chips and author tags; outlined for auto tags and
    // "off" filter chips. The difference is fill against outline, not colour.
    const bool filled = style == Style::authorTag || (style == Style::filter && getToggleState());
    const auto base = style == Style::autoTag ? Palette::secondary : Palette::accent;

    if (filled)
    {
        g.setColour (base.withAlpha (down ? 0.95f : (highlighted ? 0.85f : 0.75f)));
        g.fillRoundedRectangle (r, corner);
        g.setColour (Palette::backgroundDeep);
    }
    else
    {
        g.setColour (highlighted ? base.withAlpha (0.18f) : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (r, corner);
        g.setColour (base.withAlpha (0.9f));
        g.drawRoundedRectangle (r, corner, style == Style::autoTag ? 1.0f : 1.2f);
        g.setColour (Palette::textPrimary);
    }

    g.setFont (Fonts::ui (11.0f, filled));
    g.drawText (getButtonText(), getLocalBounds().reduced (8, 0), juce::Justification::centred, true);
}

//==============================================================================
PlayGlyph::PlayGlyph() : juce::Button ("Preview")
{
    setWantsKeyboardFocus (true);
}

void PlayGlyph::setState (State s, float p, const juce::String& failure)
{
    const bool changed = s != state || (s == State::playing && ! reducedMotion && std::abs (p - progress) > 0.001f);
    state = s;
    progress = p;
    setVisible (s != State::hidden);

    if (s == State::failed)
        setTooltip ("Preview could not be rendered: " + failure + ". The preset can still be loaded.");
    else if (s == State::preparing)
        setTooltip ("Preparing preview...");
    else
        setTooltip ("Preview (Space)");

    if (changed)
        repaint();
}

void PlayGlyph::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    const auto c = r.getCentre();
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    const auto colour = highlighted ? Palette::accentBright : Palette::accent;

    switch (state)
    {
        case State::idle:
        {
            juce::Path triangle;
            triangle.addTriangle (c.x - size * 0.25f, c.y - size * 0.32f, c.x - size * 0.25f, c.y + size * 0.32f,
                                  c.x + size * 0.33f, c.y);
            g.setColour (colour);
            g.fillPath (triangle);
            break;
        }

        case State::playing:
        {
            g.setColour (colour);
            g.fillRect (juce::Rectangle<float> (size * 0.44f, size * 0.44f).withCentre (c));

            // A sweep round the stop square, or a static ring under reduced motion.
            if (reducedMotion)
            {
                g.drawEllipse (juce::Rectangle<float> (size * 0.9f, size * 0.9f).withCentre (c), 1.2f);
            }
            else
            {
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, size * 0.45f, size * 0.45f, 0.0f, 0.0f,
                                   juce::MathConstants<float>::twoPi * juce::jlimit (0.0f, 1.0f, progress), true);
                g.strokePath (arc, juce::PathStrokeType (1.6f));
            }
            break;
        }

        case State::preparing:
        {
            g.setColour (Palette::textMuted);

            if (reducedMotion)
            {
                g.setFont (Fonts::ui (8.0f));
                g.drawText ("...", getLocalBounds(), juce::Justification::centred);
            }
            else
            {
                const float phase = (float) (juce::Time::getMillisecondCounter() % 1000) / 1000.0f;
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, size * 0.4f, size * 0.4f, 0.0f,
                                   phase * juce::MathConstants<float>::twoPi,
                                   phase * juce::MathConstants<float>::twoPi + 4.0f, true);
                g.strokePath (arc, juce::PathStrokeType (1.6f));
            }
            break;
        }

        case State::failed:
            g.setColour (Palette::warning);
            g.setFont (Fonts::ui (size * 0.8f, true));
            g.drawText ("!", getLocalBounds(), juce::Justification::centred);
            break;

        case State::hidden:
        default:
            break;
    }
}

//==============================================================================
HeartButton::HeartButton() : juce::Button ("Favourite")
{
    setClickingTogglesState (true);
    setTooltip ("Favourite (F)");
}

void HeartButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float w = r.getWidth(), h = r.getHeight();

    juce::Path heart;
    heart.startNewSubPath (r.getCentreX(), r.getBottom());
    heart.cubicTo (r.getX() - w * 0.1f, r.getY() + h * 0.55f, r.getX() + w * 0.1f, r.getY() - h * 0.15f,
                   r.getCentreX(), r.getY() + h * 0.3f);
    heart.cubicTo (r.getRight() - w * 0.1f, r.getY() - h * 0.15f, r.getRight() + w * 0.1f, r.getY() + h * 0.55f,
                   r.getCentreX(), r.getBottom());
    heart.closeSubPath();

    const auto colour = highlighted ? Palette::accentBright : Palette::accent;

    if (getToggleState())
    {
        g.setColour (colour);
        g.fillPath (heart);
    }
    else
    {
        g.setColour (colour.withAlpha (0.8f));
        g.strokePath (heart, juce::PathStrokeType (1.2f));
    }
}

//==============================================================================
StarRating::StarRating()
{
    setTitle ("Rating");
    setWantsKeyboardFocus (true);
    setTooltip ("Rating: click a star; click the current one to clear it");
}

void StarRating::setRating (int stars)
{
    stars = juce::jlimit (0, 5, stars);

    if (stars != rating)
    {
        rating = stars;
        repaint();

        if (auto* handler = getAccessibilityHandler())
            handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
    }
}

void StarRating::paint (juce::Graphics& g)
{
    const float cell = (float) getWidth() / 5.0f;

    for (int i = 0; i < 5; ++i)
    {
        const auto area = juce::Rectangle<float> ((float) i * cell, 0.0f, cell, (float) getHeight()).reduced (1.5f);
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;

        juce::Path star;
        star.addStar (area.getCentre(), 5, radius * 0.45f, radius);

        if (i < rating)
        {
            g.setColour (Palette::accent);
            g.fillPath (star);
        }
        else
        {
            g.setColour (Palette::textDisabled);
            g.strokePath (star, juce::PathStrokeType (1.0f));
        }
    }
}

void StarRating::mouseUp (const juce::MouseEvent& e)
{
    if (getWidth() <= 0 || ! getLocalBounds().contains (e.getPosition()))
        return;

    const int star = juce::jlimit (1, 5, 1 + e.x * 5 / getWidth());
    setRating (ratingAfterClickOn (star));

    if (onChange)
        onChange (rating);
}

bool StarRating::keyPressed (const juce::KeyPress& key)
{
    int next = rating;

    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::upKey)        next = rating + 1;
    else if (key == juce::KeyPress::leftKey || key == juce::KeyPress::downKey)  next = rating - 1;
    else return false;

    setRating (next);

    if (onChange)
        onChange (rating);

    return true;
}

std::unique_ptr<juce::AccessibilityHandler> StarRating::createAccessibilityHandler()
{
    struct Value : juce::AccessibilityRangedNumericValueInterface
    {
        explicit Value (StarRating& s) : owner (s) {}

        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return owner.getRating(); }

        void setValue (double v) override
        {
            owner.setRating ((int) std::round (v));

            if (owner.onChange)
                owner.onChange (owner.getRating());
        }

        AccessibleValueRange getRange() const override { return { { 0.0, 5.0 }, 1.0 }; }

        StarRating& owner;
    };

    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::slider,
                                                         juce::AccessibilityActions(),
                                                         juce::AccessibilityHandler::Interfaces { std::make_unique<Value> (*this) });
}

//==============================================================================
void paintPeaks (juce::Graphics& g, juce::Rectangle<float> area,
                 const std::array<float, PreviewResult::kNumPeaks>& peaks, float played)
{
    float loudest = 1.0e-6f;

    for (float p : peaks)
        loudest = juce::jmax (loudest, p);

    const float barWidth = area.getWidth() / (float) peaks.size();

    for (size_t i = 0; i < peaks.size(); ++i)
    {
        const float h = juce::jmax (1.0f, area.getHeight() * peaks[i] / loudest);
        const bool isPlayed = played >= 0.0f && (float) i / (float) peaks.size() < played;

        g.setColour (isPlayed ? Palette::accent : Palette::textDisabled);
        g.fillRect (area.getX() + (float) i * barWidth, area.getCentreY() - h * 0.5f,
                    juce::jmax (1.0f, barWidth - 1.0f), h);
    }
}

} // namespace luthier
