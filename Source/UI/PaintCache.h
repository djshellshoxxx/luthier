#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace luthier
{
/*  The opaque parent whose static background a child can bake into its own
    cache, so the child is opaque too and the parent is spared the repaint
    under it on every animated frame. Null when the parent is not opaque, or an
    earlier (lower) visible sibling overlaps the child: then the child would
    hide it. The parent's paint() must be static for the child's bounds, which
    the opaque panels' PaintCache-backed paints are. */
inline juce::Component* opaqueParentBehind (const juce::Component& c)
{
    auto* parent = c.getParentComponent();

    if (parent == nullptr || ! parent->isOpaque())
        return nullptr;

    const int index = parent->getIndexOfChildComponent (&c);

    for (int i = 0; i < index; ++i)
        if (auto* s = parent->getChildComponent (i); s != nullptr && s->isVisible() && s->getBounds().intersects (c.getBounds()))
            return nullptr;

    return parent;
}

/** Paints `parent`'s background under `c` into `g`, in `c`'s coordinates. */
inline void paintParentBackgroundUnder (juce::Component& parent, const juce::Component& c, juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState save (g);
    g.setOrigin (-c.getPosition());
    g.reduceClipRegion (c.getBounds());
    parent.paint (g);
}

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
