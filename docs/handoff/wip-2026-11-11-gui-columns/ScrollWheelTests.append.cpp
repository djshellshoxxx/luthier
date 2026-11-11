
//==============================================================================
/*  The owner's report (spec/issues.md): "when i scrolled that changed the value
    of the nob rather than scrolled down to the rest of the effects". The knob
    was fixed first; this holds every other control in the columns to the same
    rule - dropdowns, plain sliders, the mic handles - because any one of them
    under the pointer stops the column dead. */
namespace
{
    juce::MouseWheelDetails wheelOf (float deltaY)
    {
        juce::MouseWheelDetails wheel;
        wheel.deltaX = 0.0f;
        wheel.deltaY = deltaY;
        wheel.isReversed = false;
        wheel.isSmooth = false;
        wheel.isInertial = false;
        return wheel;
    }

    /** Sends a wheel to `c` as JUCE would for a pointer resting on it. */
    void wheelOn (juce::Component& c, juce::ModifierKeys mods, float deltaY)
    {
        auto source = juce::Desktop::getInstance().getMainMouseSource();
        const auto p = c.getLocalBounds().getCentre().toFloat();
        const auto now = juce::Time::getCurrentTime();
        const juce::MouseEvent e (source, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, p, now, 0, false);
        c.mouseWheelMove (e, wheelOf (deltaY));
    }

    /** Showing under `root`, with a size, and able to take the pointer. */
    bool pointerCanReach (juce::Component& c, juce::Component& root)
    {
        if (c.getWidth() <= 0 || c.getHeight() <= 0)
            return false;

        bool self = true, children = true;
        c.getInterceptsMouseClicks (self, children);

        if (! self)
            return false;

        for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        {
            if (! p->isVisible())
                return false;

            if (p == &root)
                return true;

            if (auto* parent = p->getParentComponent())
            {
                parent->getInterceptsMouseClicks (self, children);

                if (! children)
                    return false;
            }
        }

        return false;
    }

    juce::String kindOf (juce::Component& c)
    {
        if (dynamic_cast<juce::ComboBox*> (&c) != nullptr) return "ComboBox";
        if (dynamic_cast<MicHandle*> (&c) != nullptr)      return "MicHandle";
        if (c.findParentComponentOfClass<LuthierKnob>() != nullptr) return "LuthierKnob slider";
        return "Slider";
    }

    void collectWheelControls (juce::Component& root, juce::Array<juce::Component*>& out)
    {
        for (auto* child : root.getChildren())
        {
            if (dynamic_cast<juce::ComboBox*> (child) != nullptr
                || dynamic_cast<juce::Slider*> (child) != nullptr
                || dynamic_cast<MicHandle*> (child) != nullptr)
                out.add (child);

            collectWheelControls (*child, out);
        }
    }

    std::vector<float> parameterValues (LuthierAudioProcessor& processor)
    {
        std::vector<float> values;

        for (auto* prm : processor.getParameters())
            values.push_back (prm->getValue());

        return values;
    }
}

LUTHIER_TEST (ScrollWheel, aWheelOverAnyColumnControlScrollsAndChangesNothing)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 480);   // short, so every column overflows
    panel.setWorkspaceTabNamed ("MOD");   // WORKSHOP would take column 3 over

    std::map<juce::String, int> tried;

    for (int column = 0; column < 3; ++column)
    {
        auto* viewport = dynamic_cast<juce::Viewport*> (panel.getColumnViewport (column));
        CHECK (viewport != nullptr && viewport->getViewedComponent() != nullptr);

        if (viewport == nullptr || viewport->getViewedComponent() == nullptr)
            continue;

        auto& content = *viewport->getViewedComponent();
        CHECK_MSG (content.getHeight() > viewport->getMaximumVisibleHeight(),
                   "column " + juce::String (column + 1) + " does not overflow at 1600x480");

        juce::Array<juce::Component*> controls;
        collectWheelControls (content, controls);

        for (auto* control : controls)
        {
            if (! pointerCanReach (*control, content))
                continue;

            const auto kind = kindOf (*control);
            ++tried[kind];

            const auto where = kind + " \"" + control->getTitle() + control->getName() + "\" in column "
                               + juce::String (column + 1);

            viewport->setViewPosition (0, 0);

            const auto before = parameterValues (processor);
            auto* box = dynamic_cast<juce::ComboBox*> (control);
            auto* slider = dynamic_cast<juce::Slider*> (control);
            const int idBefore = box != nullptr ? box->getSelectedId() : 0;
            const double valueBefore = slider != nullptr ? slider->getValue() : 0.0;

            wheelOn (*control, {}, -1.0f);

            CHECK_MSG (viewport->getViewPositionY() > 0, "the wheel over " + where + " did not scroll the column");
            CHECK_MSG (before == parameterValues (processor), "the wheel over " + where + " changed a parameter");

            if (box != nullptr)
                CHECK_MSG (box->getSelectedId() == idBefore, "the wheel over " + where + " changed its selection");

            if (slider != nullptr)
                CHECK_MSG (juce::approximatelyEqual (slider->getValue(), valueBefore),
                           "the wheel over " + where + " changed its value");
        }
    }

    for (const auto& [kind, count] : tried)
        std::cout << "    wheel over " << count << " x " << kind << std::endl;

    CHECK_MSG (tried["ComboBox"] > 0, "no dropdown found in the columns");
    CHECK_MSG (tried["LuthierKnob slider"] > 0, "no knob found in the columns");
    CHECK_MSG (tried["MicHandle"] > 0, "no mic handle found in the columns");
}

LUTHIER_TEST (ScrollWheel, ctrlWheelOverAColumnDropdownStillStepsIt)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 480);
    panel.setWorkspaceTabNamed ("MOD");

    auto* viewport = dynamic_cast<juce::Viewport*> (panel.getColumnViewport (0));
    CHECK (viewport != nullptr);

    if (viewport == nullptr)
        return;

    juce::Array<juce::ComboBox*> boxes;
    collect<juce::ComboBox> (*viewport->getViewedComponent(), boxes);

    juce::ComboBox* box = nullptr;

    for (auto* b : boxes)
        if (b->isVisible() && b->isEnabled() && b->getNumItems() > 2 && b->getSelectedItemIndex() == 0)
        {
            box = b;
            break;
        }

    CHECK_MSG (box != nullptr, "no dropdown at its first item in column 1");

    if (box == nullptr)
        return;

    viewport->setViewPosition (0, 0);

    // Ctrl+wheel down: the selection steps on and the column stays put.
    wheelOn (*box, juce::ModifierKeys::ctrlModifier, -1.0f);

    CHECK_MSG (box->getSelectedItemIndex() > 0, "Ctrl+wheel over a column dropdown did not step it");
    CHECK_MSG (viewport->getViewPositionY() == 0, "Ctrl+wheel over a column dropdown scrolled the column");
}

//==============================================================================
/*  "the scroll bars need to be more colorful and obvious": wide enough to see
    and to grab, accent on a dark track at rest, brighter under the pointer and
    brighter still while dragged, with a tooltip saying what it is for. */
namespace
{
    double luminance (juce::Colour c)
    {
        auto channel = [] (juce::uint8 v)
        {
            const double s = v / 255.0;
            return s <= 0.03928 ? s / 12.92 : std::pow ((s + 0.055) / 1.055, 2.4);
        };

        return 0.2126 * channel (c.getRed()) + 0.7152 * channel (c.getGreen()) + 0.0722 * channel (c.getBlue());
    }

    double contrast (juce::Colour a, juce::Colour b)
    {
        const double la = luminance (a), lb = luminance (b);
        return (juce::jmax (la, lb) + 0.05) / (juce::jmin (la, lb) + 0.05);
    }

    /** The thumb's centre pixel and a track pixel well away from it. */
    std::pair<juce::Colour, juce::Colour> drawBar (LuthierLookAndFeel& lnf, juce::ScrollBar& bar, bool over, bool down)
    {
        constexpr int w = 12, h = 200, thumbStart = 20, thumbSize = 60;
        juce::Image image (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
        {
            juce::Graphics g (image);
            lnf.drawScrollbar (g, bar, 0, 0, w, h, true, thumbStart, thumbSize, over, down);
        }

        return { image.getPixelAt (w / 2, thumbStart + thumbSize / 2), image.getPixelAt (w / 2, 160) };
    }
}

LUTHIER_TEST (ScrollWheel, columnScrollbarsAreWideColouredAndExplainThemselves)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 480);
    panel.setWorkspaceTabNamed ("MOD");

    for (int column = 0; column < 3; ++column)
    {
        auto* viewport = dynamic_cast<juce::Viewport*> (panel.getColumnViewport (column));
        CHECK (viewport != nullptr);

        if (viewport == nullptr)
            continue;

        const auto name = "column " + juce::String (column + 1);

        CHECK_MSG (viewport->getScrollBarThickness() >= 12,
                   name + " scrollbar is " + juce::String (viewport->getScrollBarThickness()) + " px wide");

        auto& bar = viewport->getVerticalScrollBar();
        CHECK_MSG (bar.isVisible(), name + " overflows but shows no scrollbar");
        CHECK_MSG (bar.getWidth() >= 12, name + " scrollbar is laid out narrower than 12 px");

        auto* tip = dynamic_cast<juce::TooltipClient*> (&bar);
        CHECK_MSG (tip != nullptr && tip->getTooltip().isNotEmpty(), name + " scrollbar has no tooltip");

        // The content keeps the width it was laid out for whether or not the
        // bar shows: nothing is drawn under the bar.
        if (auto* content = viewport->getViewedComponent())
            CHECK_MSG (content->getRight() <= bar.getX(),
                       name + " content runs under its scrollbar (" + juce::String (content->getRight())
                         + " > " + juce::String (bar.getX()) + ")");
    }

    LuthierLookAndFeel lnf;
    juce::ScrollBar bar (true);

    const auto rest = drawBar (lnf, bar, false, false);
    const auto over = drawBar (lnf, bar, true, false);
    const auto down = drawBar (lnf, bar, true, true);

    CHECK_MSG (contrast (rest.first, rest.second) >= 3.0,
               "the resting thumb has a contrast of " + juce::String (contrast (rest.first, rest.second), 2)
                 + " against its track (3:1 wanted)");
    CHECK_MSG (rest.first.getSaturation() > 0.3f, "the resting thumb is not coloured");
    CHECK_MSG (over.first != rest.first, "the thumb does not change under the pointer");
    CHECK_MSG (down.first != over.first, "the thumb does not change while dragged");
}

LUTHIER_TEST (ScrollWheel, theArrowsAtEachEndShowOnlyWhereThereIsMore)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    AdvancedPanel panel (processor);
    panel.setVisible (true);
    panel.setSize (1600, 480);
    panel.setWorkspaceTabNamed ("MOD");

    auto* viewport = dynamic_cast<ScrollHintViewport*> (panel.getColumnViewport (0));
    CHECK (viewport != nullptr);

    if (viewport == nullptr || viewport->getViewedComponent() == nullptr)
        return;

    const int bottom = viewport->getViewedComponent()->getHeight() - viewport->getMaximumVisibleHeight();
    CHECK (bottom > 0);

    viewport->setViewPosition (0, 0);
    CHECK_MSG (! viewport->isTopHintShowing() && viewport->isBottomHintShowing(), "at the top: only the arrow below");

    viewport->setViewPosition (0, bottom / 2);
    CHECK_MSG (viewport->isTopHintShowing() && viewport->isBottomHintShowing(), "halfway: both arrows");

    viewport->setViewPosition (0, bottom);
    CHECK_MSG (viewport->isTopHintShowing() && ! viewport->isBottomHintShowing(), "at the end: only the arrow above");

    // A click on the arrow above pages back up.
    viewport->pageBy (-1);
    CHECK (viewport->getViewPositionY() < bottom);

    // Each arrow says what it is for, to the pointer and to a screen reader,
    // and is a button a screen reader can press.
    int arrows = 0;

    for (auto* child : viewport->getChildren())
    {
        const auto title = child->getTitle();

        if (! title.startsWith ("More "))
            continue;

        ++arrows;

        auto* tip = dynamic_cast<juce::TooltipClient*> (child);
        CHECK_MSG (tip != nullptr && tip->getTooltip().isNotEmpty(), title + " has no tooltip");

        auto handler = child->createAccessibilityHandler();
        CHECK_MSG (handler != nullptr && handler->getTitle().isNotEmpty(), title + " has no accessible name");
        CHECK_MSG (handler != nullptr && handler->getRole() == juce::AccessibilityRole::button,
                   title + " is not an accessible button");
        CHECK_MSG (handler != nullptr && handler->getActions().contains (juce::AccessibilityActionType::press),
                   title + " cannot be pressed by a screen reader");
    }

    CHECK_MSG (arrows == 2, "expected an arrow at each end, found " + juce::String (arrows));

    // A column taller than its content shows neither.
    panel.setSize (1600, 6000);
    viewport->setViewPosition (0, 0);
    CHECK_MSG (! viewport->isTopHintShowing() && ! viewport->isBottomHintShowing(),
               "a column that fits shows an arrow");
}
