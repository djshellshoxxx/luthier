#include "TechniqueUi.h"
#include "../../PluginProcessor.h"
#include "../../Accessibility/Accessibility.h"

namespace luthier
{

//==============================================================================
namespace TechniqueTable
{
    const TechniqueInfo& get (TechniqueSlot slot) noexcept
    {
        static const TechniqueInfo table[] =
        {
            { TechniqueSlot::scrape, "SCRAPE", "Scrape", ParamIDs::scrapeArmed, CascadeTechnique::scrape,
              "Scrape: drag the pick or a nail along the wound strings" },
            { TechniqueSlot::slide,  "SLIDE",  "Slide",  ParamIDs::slideGuitar, CascadeTechnique::slide,
              "Slide: Slide Mode, with its position source, range and scripted gestures" },
            { TechniqueSlot::slap,   "SLAP",   "Slap",   ParamIDs::slapArmed,   CascadeTechnique::slap,
              "Slap: thumb slap, finger pop, palm slap or body tap" },
            { TechniqueSlot::mute,   "MUTE",   "Mute",   ParamIDs::muteArmed,   CascadeTechnique::mute,
              "Mute: the master mute mode, the live mute grid and chuka" },
            { TechniqueSlot::tap,    "TAP",    "Tap",    ParamIDs::tapArmed,    CascadeTechnique::tap,
              "Tap: two-hand tapping, from MIDI channel 2, keyswitch 19 or the fretboard" },
            { TechniqueSlot::bend,   "BEND",   "Bend",   ParamIDs::bendArmed,   CascadeTechnique::bend,
              "Bend: microtonal bend sources, vibrato, quantise and pre-bend" },
        };

        return table[juce::jlimit (0, count - 1, (int) slot)];
    }

    bool isArmed (LuthierAudioProcessor& p, TechniqueSlot slot)
    {
        return TechniqueUndo::getPlain (p, get (slot).armParameterId) > 0.5f;
    }

    int stringMask (LuthierAudioProcessor& p, TechniqueSlot slot)
    {
        const int n = p.getEngine().getNumStrings();

        switch (slot)
        {
            case TechniqueSlot::scrape: return juce::roundToInt (TechniqueUndo::getPlain (p, ParamIDs::scrapeStringMask));
            case TechniqueSlot::slap:   return juce::roundToInt (TechniqueUndo::getPlain (p, ParamIDs::slapStringMask));
            case TechniqueSlot::slide:  return slideContactMaskFor (juce::roundToInt (TechniqueUndo::getPlain (p, ParamIDs::slideContact)), n);
            case TechniqueSlot::mute:
            case TechniqueSlot::tap:
            case TechniqueSlot::bend:
            case TechniqueSlot::numSlots:
            default:                    return 0;
        }
    }

    juce::uint32 fireCount (LuthierAudioProcessor& p, TechniqueSlot slot)
    {
        auto& e = p.getEngine();
        const auto& layer = e.getTechniqueLayer();

        switch (slot)
        {
            case TechniqueSlot::scrape: return e.getScrapeEngine().getFiredCount();
            case TechniqueSlot::slap:   return e.getSlapEngine().getFiredCount();
            case TechniqueSlot::mute:   return layer.mute.getFireCount();
            case TechniqueSlot::tap:    return layer.tap.getFireCount();

            case TechniqueSlot::slide:
                // The bar on the strings: a new count each time it moves.
                return e.getSlideEngine().getOverlayFret() >= 0.0
                         ? (juce::uint32) juce::roundToInt (e.getSlideEngine().getOverlayFret() * 10.0) + 1 : 0;

            case TechniqueSlot::bend:
            {
                double total = 0.0;

                for (int s = 0; s < e.getNumStrings(); ++s)
                    total += std::abs (layer.bend.getLiveCents (s));

                return (juce::uint32) juce::roundToInt (total / 5.0);
            }

            case TechniqueSlot::numSlots:
            default:
                return 0;
        }
    }

    juce::String conflictFor (LuthierAudioProcessor& p, TechniqueSlot slot)
    {
        if (! isArmed (p, slot))
            return {};

        const int n = p.getEngine().getNumStrings();

        // Slide first: it preempts (technique-cascade.md 3.4), so it is named first.
        const TechniqueSlot order[] = { TechniqueSlot::slide, TechniqueSlot::scrape, TechniqueSlot::slap,
                                        TechniqueSlot::tap, TechniqueSlot::mute, TechniqueSlot::bend };

        for (auto other : order)
        {
            if (other == slot || ! isArmed (p, other))
                continue;

            const auto message = CascadeResolver::conflictMessage (get (slot).cascade, get (other).cascade,
                                                                   stringMask (p, other), stringMask (p, slot), n);

            if (message.isNotEmpty())
                return message;
        }

        return {};
    }
}

//==============================================================================
namespace TechniqueUndo
{
    namespace
    {
        struct MergeWindow
        {
            juce::String key;
            juce::uint32 at = 0;
            int steps = -1;
        };

        MergeWindow& window()
        {
            static MergeWindow w;
            return w;
        }

        /** True when this edit continues the last one (same key, within 200 ms, nothing pushed since). */
        bool continues (LuthierAudioProcessor& p, const juce::String& key)
        {
            auto& w = window();
            const auto now = juce::Time::getMillisecondCounter();
            const bool same = w.key == key && now - w.at < (juce::uint32) kMergeMs && w.steps == p.getNumUndoSteps();
            w.key = key;
            w.at = now;
            return same;
        }

        void noteSteps (LuthierAudioProcessor& p) { window().steps = p.getNumUndoSteps(); }

        void write (LuthierAudioProcessor& p, const juce::String& id, float plain)
        {
            if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id)))
                param->setValueNotifyingHost (param->convertTo0to1 (plain));
        }
    }

    float getPlain (LuthierAudioProcessor& p, const juce::String& id)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getState().getParameter (id)))
            return param->convertFrom0to1 (param->getValue());

        return 0.0f;
    }

    void resetMergeWindow()
    {
        window() = MergeWindow {};
    }

    void setArmed (LuthierAudioProcessor& p, TechniqueSlot slot, bool armed)
    {
        // technique-arm: every arm and disarm is its own entry.
        const auto& info = TechniqueTable::get (slot);
        p.pushUndoState (juce::String (armed ? "Arm " : "Disarm ") + info.spokenName + " technique");
        write (p, info.armParameterId, armed ? 1.0f : 0.0f);
        window() = MergeWindow {};
    }

    void setParameter (LuthierAudioProcessor& p, const juce::String& parameterId, float plainValue)
    {
        // technique-param: one entry per parameter per 200 ms.
        if (! continues (p, "param:" + parameterId))
        {
            juce::String name = parameterId;

            if (auto* param = p.getState().getParameter (parameterId))
                name = param->getName (64);

            p.pushUndoState ("Change " + name);
        }

        write (p, parameterId, plainValue);
        noteSteps (p);
    }

    void paintLiveMuteStep (LuthierAudioProcessor& p, int step, MuteType type)
    {
        auto& mute = p.getEngine().getTechniqueLayer().mute;

        if (mute.getLiveStep (step) == type)
            return;

        // mute-grid-paint: one entry per cell per 200 ms.
        if (! continues (p, "live:" + juce::String (step)))
            p.pushUndoState ("Paint mute grid");

        mute.setLiveStep (step, type);
        noteSteps (p);
    }

    void paintPatternMuteStep (LuthierAudioProcessor& p, int step, MuteType type)
    {
        auto& rhythm = p.getEngine().getRhythmEngine();
        auto pattern = rhythm.getPattern();

        if (pattern.getMuteStep (step).type == type)
            return;

        if (! continues (p, "pattern:" + juce::String (step)))
            p.pushUndoState ("Paint mute row");

        auto m = pattern.getMuteStep (step);
        m.type = type;
        pattern.setMuteStep (step, m);
        rhythm.setPattern (pattern);
        noteSteps (p);
    }
}

//==============================================================================
TechniquePill::TechniquePill (LuthierAudioProcessor& p, TechniqueSlot s)
    : processor (p), slot (s)
{
    setWantsKeyboardFocus (true);
    setTitle (TechniqueTable::get (slot).spokenName + juce::String (" technique"));
    setDescription ("Click or Space to arm. Hold for its controls. Right-click or Enter opens its sub-tab.");
    refresh();
    motion.startTimerHz (*this, 30);
}

TechniquePill::~TechniquePill()
{
    motion.stopTimer();
}

juce::String TechniquePill::getLearnParameterId() const
{
    return TechniqueTable::get (slot).armParameterId;
}

juce::String TechniquePill::getAccessibleName() const
{
    return juce::String (TechniqueTable::get (slot).spokenName) + " technique, " + (armed ? "armed" : "not armed");
}

void TechniquePill::toggle()
{
    TechniqueUndo::setArmed (processor, slot, ! TechniqueTable::isArmed (processor, slot));
    refresh();

    if (auto* handler = getAccessibilityHandler())
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
}

void TechniquePill::refresh()
{
    const bool nowArmed = TechniqueTable::isArmed (processor, slot);
    const auto nowConflict = TechniqueTable::conflictFor (processor, slot);
    const auto fire = TechniqueTable::fireCount (processor, slot);

    bool changed = nowArmed != armed || nowConflict != conflict;

    if (fire != lastFire)
    {
        lastFire = fire;

        if (armed)
        {
            // gui-techniques-updates 9: under reduced motion the dot is a state, not a pulse.
            flash = 1.0f;
            changed = true;
        }
    }

    armed = nowArmed;
    conflict = nowConflict;

    const auto tip = conflict.isNotEmpty() ? conflict : juce::String (TechniqueTable::get (slot).summary);

    if (getTooltip() != tip)
        setTooltip (tip);

    setTitle (getAccessibleName());

    if (changed)
        repaint();
}

void TechniquePill::timerCallback()
{
    refresh();

    if (flash > 0.0f)
    {
        const bool reduced = AccessibilitySettings::get().isReducedMotion();
        flash = reduced ? 0.0f : juce::jmax (0.0f, flash - 0.12f);
        repaint();
    }

    // Press and hold: the popover.
    if (pressed && ! holdFired && juce::Time::getMillisecondCounter() - pressedAt >= (juce::uint32) kHoldMs)
    {
        holdFired = true;

        if (onHold != nullptr)
            onHold (slot);
    }
}

void TechniquePill::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (1.5f);
    const float radius = area.getHeight() * 0.5f;

    if (armed)
    {
        g.setColour (Palette::accent);
        g.fillRoundedRectangle (area, radius);
    }

    g.setColour (armed ? Palette::accentBright : (hasKeyboardFocus (false) ? Palette::textPrimary : Palette::edgeBright));
    g.drawRoundedRectangle (area, radius, hasKeyboardFocus (false) ? 2.0f : 1.2f);

    auto text = area.reduced (radius * 0.6f, 0.0f);

    // Not colour alone (9): a check when armed.
    if (armed)
    {
        auto tick = text.removeFromLeft (10.0f).withSizeKeepingCentre (8.0f, 8.0f);
        juce::Path p;
        p.startNewSubPath (tick.getX(), tick.getCentreY());
        p.lineTo (tick.getX() + 3.0f, tick.getBottom());
        p.lineTo (tick.getRight(), tick.getY());
        g.setColour (Palette::backgroundDeep);
        g.strokePath (p, juce::PathStrokeType (1.6f));
    }

    g.setColour (armed ? Palette::backgroundDeep : Palette::textMuted);
    g.setFont (Fonts::ui (11.0f, true));
    g.drawText (TechniqueTable::get (slot).name, text, juce::Justification::centred, false);

    // The firing dot, beside the pill (2: "a small state indicator dot next to each armed pill").
    if (armed)
    {
        const auto dot = juce::Rectangle<float> (area.getRight() - radius - 3.0f, area.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.setColour (Palette::backgroundDeep.interpolatedWith (Palette::success, 0.35f + 0.65f * flash));
        g.fillEllipse (dot.expanded (flash * 1.5f));
    }

    // technique-cascade.md 6: the red slash.
    if (conflict.isNotEmpty())
    {
        g.setColour (Palette::clip);
        g.drawLine (area.getX() + radius * 0.5f, area.getBottom() - 2.0f,
                    area.getRight() - radius * 0.5f, area.getY() + 2.0f, 2.0f);
    }
}

void TechniquePill::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onOpenSubTab != nullptr)
            onOpenSubTab (slot);

        return;
    }

    pressed = true;
    holdFired = false;
    pressedAt = juce::Time::getMillisecondCounter();
}

void TechniquePill::mouseUp (const juce::MouseEvent& e)
{
    if (! pressed)
        return;

    pressed = false;

    // A hold opened the popover; the release does not also toggle.
    if (! holdFired && ! e.mods.isPopupMenu() && getLocalBounds().contains (e.getPosition()))
        toggle();
}

bool TechniquePill::keyPressed (const juce::KeyPress& key)
{
    // gui-techniques-updates 9: Space arms / disarms, Enter opens the sub-tab.
    if (key == juce::KeyPress::spaceKey)
    {
        toggle();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        if (onOpenSubTab != nullptr)
            onOpenSubTab (slot);

        return true;
    }

    return false;
}

std::unique_ptr<juce::AccessibilityHandler> TechniquePill::createAccessibilityHandler()
{
    juce::AccessibilityActions actions;
    actions.addAction (juce::AccessibilityActionType::press, [this] { toggle(); });
    actions.addAction (juce::AccessibilityActionType::toggle, [this] { toggle(); });
    actions.addAction (juce::AccessibilityActionType::showMenu, [this]
    {
        if (onOpenSubTab != nullptr)
            onOpenSubTab (slot);
    });

    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::toggleButton, std::move (actions));
}

} // namespace luthier
