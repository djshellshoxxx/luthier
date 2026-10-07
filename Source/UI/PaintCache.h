#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace luthier
{
/*  A component's static painting, rendered once into an image at the screen's
    real pixel density and blitted on every later paint.

    Nothing in a cached layer may change without the key changing: callers fold
    in whatever their static painting depends on (the area is folded in here,
    with the physical scale and Palette::revision). Live content is painted
    after draw(), on top of the image.

    Why: no Luthier component is opaque, so a meter or a string dot repainting
    at 30-60 Hz re-runs every ancestor's paint() under it - panel grain and
    shadows, engraved header plates, the window's cutaway - while notes play. */
class PaintCache
{
public:
    /** `opaque`: the static painting covers every pixel (it starts with a fill),
        so the cache is an RGB image and blits as a plain copy, with no blending. */
    template <typename PaintFn>
    void draw (juce::Graphics& g, juce::Rectangle<int> area, juce::int64 key, PaintFn&& paintStatic, bool opaque = false)
    {
        if (area.isEmpty())
            return;

        const float scale = juce::jlimit (0.25f, 8.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto fullKey = (juce::String (key) + "|" + area.toString() + "|" + juce::String (scale, 3)
                              + "|" + juce::String (Palette::revision)).hashCode64();

        if (! image.isValid() || fullKey != cachedKey)
        {
            image = juce::Image (opaque ? juce::Image::RGB : juce::Image::ARGB,
                                 juce::jmax (1, juce::roundToInt ((float) area.getWidth() * scale)),
                                 juce::jmax (1, juce::roundToInt ((float) area.getHeight() * scale)),
                                 true);
            juce::Graphics ig (image);
            ig.addTransform (juce::AffineTransform::scale (scale).translated (-(float) area.getX() * scale,
                                                                             -(float) area.getY() * scale));
            paintStatic (ig);
            cachedKey = fullKey;
        }

        g.drawImageTransformed (image, juce::AffineTransform::scale (1.0f / scale)
                                           .translated ((float) area.getX(), (float) area.getY()));
    }

    void invalidate() noexcept { cachedKey = -1; }

private:
    juce::Image image;
    juce::int64 cachedKey = -1;
};

} // namespace luthier
