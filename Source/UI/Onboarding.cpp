#include "Onboarding.h"
#include "UiPreferences.h"
#include "../Accessibility/Accessibility.h"

#include <atomic>

namespace luthier
{

namespace
{
    std::atomic<bool> launchRecorded { false };
    std::atomic<bool> automatic { true };
    std::function<juce::Time()> testClock;

    juce::Time now()
    {
        return testClock != nullptr ? testClock() : juce::Time::getCurrentTime();
    }

    /** Whole days from an ISO 8601 date written by this file to now; a large
        number when there is none, so a missing date never keeps a hint alive. */
    double daysSince (const juce::String& iso)
    {
        if (iso.isEmpty())
            return 1.0e9;

        const auto then = juce::Time::fromISO8601 (iso);
        return (double) (now().toMilliseconds() - then.toMilliseconds()) / (1000.0 * 60.0 * 60.0 * 24.0);
    }

    juce::String tabKey (const juce::String& tabName)
    {
        return Onboarding::kTabOpenedPrefix + tabName.toUpperCase().replaceCharacter (' ', '_');
    }
}

//==============================================================================
juce::String Onboarding::getCurrentVersion()
{
   #ifdef JucePlugin_VersionString
    return JucePlugin_VersionString;
   #else
    return "1.0.0";
   #endif
}

void Onboarding::recordLaunch (const juce::String& version)
{
    if (launchRecorded.exchange (true))
        return;

    auto& preferences = UiPreferences::get();

    preferences.setInt (kLaunchCountKey, preferences.getInt (kLaunchCountKey, 0) + 1);

    if (preferences.getString (kFirstLaunchKey, {}).isEmpty())
        preferences.setString (kFirstLaunchKey, now().toISO8601 (true));

    const auto versionKey = juce::String (kVersionLaunchPrefix) + version;

    if (preferences.getString (versionKey, {}).isEmpty())
        preferences.setString (versionKey, now().toISO8601 (true));
}

int Onboarding::getLaunchCount()
{
    return UiPreferences::get().getInt (kLaunchCountKey, 0);
}

bool Onboarding::isDiscoveryWeek()
{
    const int launches = getLaunchCount();

    // Not yet counted (a test, a host that built no editor): nothing is due.
    if (launches <= 0)
        return false;

    return launches <= kDiscoveryLaunches
        && daysSince (UiPreferences::get().getString (kFirstLaunchKey, {})) < (double) kDiscoveryDays;
}

bool Onboarding::isHintDue (const juce::String& key)
{
    return isDiscoveryWeek() && ! hasSeen (key);
}

void Onboarding::markSeen (const juce::String& key)
{
    if (! hasSeen (key))
        UiPreferences::get().setBool (kSeenPrefix + key, true);
}

bool Onboarding::hasSeen (const juce::String& key)
{
    return UiPreferences::get().getBool (kSeenPrefix + key, false);
}

void Onboarding::markTabOpened (const juce::String& tabName)
{
    if (! wasTabOpened (tabName))
        UiPreferences::get().setBool (tabKey (tabName), true);
}

bool Onboarding::wasTabOpened (const juce::String& tabName)
{
    return UiPreferences::get().getBool (tabKey (tabName), false);
}

bool Onboarding::isTabDotDue (const juce::String& tabName)
{
    return isDiscoveryWeek() && ! wasTabOpened (tabName);
}

//==============================================================================
const std::vector<Onboarding::NewFeature>& Onboarding::getNewFeatures()
{
    /*  gui-integration 20: "Every new feature added after 1.0". This build is
        1.0.0, so nothing is new yet; a later version adds a line here with its
        entry point's key, e.g. { "tab_TECHNIQUES", "1.1.0" }. */
    static const std::vector<NewFeature> features;
    return features;
}

bool Onboarding::isNewDotDue (const juce::String& featureKey, const juce::String& currentVersion)
{
    for (const auto& f : getNewFeatures())
    {
        if (featureKey != f.key)
            continue;

        // Only while the version that introduced it is the one running, for a week.
        if (currentVersion != f.introducedIn || hasSeen ("new_" + featureKey))
            return false;

        const auto first = UiPreferences::get().getString (juce::String (kVersionLaunchPrefix) + currentVersion, {});
        return daysSince (first) < (double) kNewDotDays;
    }

    return false;
}

//==============================================================================
Onboarding::Welcome Onboarding::getWelcomeDue (const juce::String& version)
{
    auto& preferences = UiPreferences::get();
    const auto welcomed = preferences.getString (kWelcomeVersionKey, {});

    // 13: "Welcome banner returns once with Version X.Y.Z installed."
    if (welcomed.isNotEmpty() && welcomed != version)
        return Welcome::upgrade;

    // 2 and 11: the tour offer, until answered, refused, or offered three times.
    if (preferences.getBool (kWelcomeDontAskKey, false) || preferences.getBool (kWelcomeAnsweredKey, false))
        return Welcome::none;

    if (preferences.getInt (kWelcomeShowsKey, 0) >= kMaxTourOffers)
        return Welcome::none;

    return Welcome::tourOffer;
}

void Onboarding::noteWelcomeShown (Welcome kind, const juce::String& version)
{
    auto& preferences = UiPreferences::get();

    if (kind == Welcome::tourOffer)
        preferences.setInt (kWelcomeShowsKey, preferences.getInt (kWelcomeShowsKey, 0) + 1);

    if (kind != Welcome::none)
        preferences.setString (kWelcomeVersionKey, version);
}

void Onboarding::answerWelcome (Answer answer)
{
    auto& preferences = UiPreferences::get();

    switch (answer)
    {
        case Answer::yes:       preferences.setBool (kWelcomeAnsweredKey, true); break;
        case Answer::never:     preferences.setBool (kWelcomeDontAskKey, true); break;
        case Answer::later:     // counted when shown: "up to 3 times total"
        case Answer::dismissed: break;
    }
}

void Onboarding::setClockForTesting (std::function<juce::Time()> clock)
{
    testClock = std::move (clock);
}

void Onboarding::resetProcessStateForTesting()
{
    launchRecorded = false;
}

void Onboarding::setAutomaticForTesting (bool isAutomatic) noexcept
{
    automatic = isAutomatic;
}

bool Onboarding::isAutomatic() noexcept
{
    return automatic.load();
}

//==============================================================================
std::vector<Onboarding::TourStep> Onboarding::getTourSteps (bool includeTechniques)
{
    // onboarding 3, in its order and its words.
    std::vector<TourStep> steps =
    {
        { "play",      "Play something",      "Hit a key on your MIDI keyboard. The output LED should light." },
        { "preset",    "Try a preset",        "Click to browse. Try '" + juce::String (kTourSuggestedPreset) + "'. Hover to hear, Enter to load." },   // preset-browser-previews 13
        { "mode",      "Switch to Advanced",  "Show every knob. You'll be here often." },
        { "guitar",    "Meet your guitar",    "Change the instrument, tuning, and character here." },
        { "rig",       "The rig",             "Pedals, amp, cabinet, room." },
        { "workspace", "The workspace",       "Everything else lives here: modulation, rhythm, tunes, tone match, practice, and more." },
        { "workshop",  "The Workshop",        "Every part of the guitar is a real part. Swap pickups, change strings, drop a "
                                              "different bridge. Try it. Escape to close." },
        { "tune",      "Sketch a tune",       "Type a chord progression, hit play. Add a melody in one click if you want to." },
        { "snapshots", "Snapshots",           "Save a moment, recall with one press." },
        { "practice",  "Practice",            "Metronome, looper, backing tracks, ear training. Slide it up when you need it." },
        { "slide",     "Slide Mode",          "Turn this on for bottleneck, lap steel or dobro. The fretboard becomes continuous." },
        { "options",   "Options",             "Audio, MIDI, appearance, accessibility, updates (File -> Options). Nothing here changes how it sounds." }
    };

    // The TECHNIQUES tab (gui-techniques-updates) joins after the workspace
    // stop when the build has it.
    if (includeTechniques)
        steps.insert (steps.begin() + 6, { "techniques", "Techniques",
                                           "Slap, tap, scrape, mute, bend: every playing technique has its own "
                                           "controls in this tab." });

    return steps;
}

bool Onboarding::isVisibleWithin (const juce::Component& c, const juce::Component* root)
{
    for (auto* p = &c; p != nullptr; p = p->getParentComponent())
    {
        // The root's own visibility is its host's business (a window not yet on screen).
        if (p == root)
            return true;

        if (! p->isVisible())
            return false;
    }

    // Not under root at all.
    return root == nullptr;
}

juce::Component* Onboarding::findComponent (juce::Component& root, const std::function<bool (juce::Component&)>& match)
{
    if (match (root))
        return &root;

    for (auto* child : root.getChildren())
        if (auto* found = findComponent (*child, match))
            return found;

    return nullptr;
}

juce::Button* Onboarding::findButton (juce::Component& root, const juce::String& text)
{
    juce::Button* hidden = nullptr;

    auto* showing = findComponent (root, [&text, &hidden, &root] (juce::Component& c)
    {
        auto* b = dynamic_cast<juce::Button*> (&c);

        if (b == nullptr || ! b->getButtonText().equalsIgnoreCase (text))
            return false;

        if (isVisibleWithin (c, &root))
            return true;

        if (hidden == nullptr)
            hidden = b;

        return false;
    });

    return showing != nullptr ? dynamic_cast<juce::Button*> (showing) : hidden;
}

//==============================================================================
// Welcome banner
//==============================================================================
WelcomeBanner::WelcomeBanner()
{
    for (auto* b : { &first, &second, &third })
        addChildComponent (b);

    AccessibleSetup::configureDescriptive (*this, "Welcome", "Welcome banner");
    setVisible (false);
}

void WelcomeBanner::showFor (Onboarding::Welcome newKind, const juce::String& version)
{
    kind = newKind;

    for (auto* b : { &first, &second, &third })
    {
        b->setVisible (false);
        b->onClick = nullptr;
    }

    if (kind == Onboarding::Welcome::tourOffer)
    {
        // onboarding 2, word for word.
        message = "Welcome to Luthier. Take the 2-minute tour?";

        first.setButtonText ("Yes");
        second.setButtonText ("Maybe later");
        third.setButtonText ("Don't ask again");

        first.onClick = [this]
        {
            Onboarding::answerWelcome (Onboarding::Answer::yes);
            close();

            if (onStartTour != nullptr)
                onStartTour();
        };

        second.onClick = [this] { Onboarding::answerWelcome (Onboarding::Answer::later); close(); };
        third.onClick  = [this] { Onboarding::answerWelcome (Onboarding::Answer::never); close(); };

        for (auto* b : { &first, &second, &third })
            b->setVisible (true);
    }
    else if (kind == Onboarding::Welcome::upgrade)
    {
        // onboarding 13: "Version X.Y.Z installed. What's new?" - the changelog one click away.
        message = "Version " + version + " installed. What's new?";

        first.setButtonText ("What's new");
        second.setButtonText ("Dismiss");

        first.onClick = [this]
        {
            close();

            if (onWhatsNew != nullptr)
                onWhatsNew();
        };

        second.onClick = [this] { Onboarding::answerWelcome (Onboarding::Answer::dismissed); close(); };

        first.setVisible (true);
        second.setVisible (true);
    }

    for (auto* b : { &first, &second, &third })
        AccessibleSetup::configureButton (*b, b->getButtonText());

    setHelpText (message);

    const bool show = kind != Onboarding::Welcome::none;

    if (show != isVisible())
    {
        setVisible (show);

        if (onVisibilityChanged != nullptr)
            onVisibilityChanged();
    }

    resized();
    repaint();
}

void WelcomeBanner::close()
{
    // The button's own onClick is running: hide after it returns.
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<WelcomeBanner> (this)]
    {
        if (safe != nullptr)
            safe->showFor (Onboarding::Welcome::none, {});
    });

    setVisible (false);

    if (onVisibilityChanged != nullptr)
        onVisibilityChanged();
}

void WelcomeBanner::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (Palette::panelRaised);
    g.fillRoundedRectangle (bounds, Metrics::panelCorner);
    g.setColour (Palette::accentDim);
    g.drawRoundedRectangle (bounds, Metrics::panelCorner, 1.0f);

    g.setColour (Palette::accent);
    g.fillRect (getLocalBounds().removeFromLeft (3).reduced (0, 4));

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (12.5f, true));
    g.drawFittedText (message, textBounds, juce::Justification::centredLeft, 1);
}

void WelcomeBanner::resized()
{
    auto area = getLocalBounds().reduced (Metrics::grid, 3);

    for (auto* b : { &third, &second, &first })
    {
        if (! b->isVisible())
            continue;

        const int width = juce::jmax (60, Fonts::ui (12.0f).getStringWidth (b->getButtonText()) + 24);
        b->setBounds (area.removeFromRight (width));
        area.removeFromRight (Metrics::gridHalf);
    }

    textBounds = area.withTrimmedLeft (Metrics::gridHalf);
}

//==============================================================================
// Tour
//==============================================================================
TourOverlay::Callout::Callout()
{
    for (auto* b : { &backButton, &nextButton, &skipButton })
        addAndMakeVisible (b);

    AccessibleSetup::configureButton (backButton, "Back", "The previous stop of the tour.");
    AccessibleSetup::configureButton (nextButton, "Next", "The next stop of the tour.");
    AccessibleSetup::configureButton (skipButton, "Skip", "Ends the tour.");
}

void TourOverlay::Callout::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (Palette::shadow);
    g.fillRoundedRectangle (bounds.translated (2.0f, 3.0f), Metrics::panelCorner * 2.0f);

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, Metrics::panelCorner * 2.0f);
    g.setColour (Palette::accent);
    g.drawRoundedRectangle (bounds, Metrics::panelCorner * 2.0f, 1.5f);

    auto area = textArea;

    g.setColour (Palette::accent);
    g.setFont (Fonts::ui (14.0f, true));
    g.drawText (title, area.removeFromTop (20), juce::Justification::centredLeft);

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::mono (10.0f));
    g.drawText (counter, textArea.withHeight (20), juce::Justification::centredRight);

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (12.5f));
    g.drawFittedText (text, area, juce::Justification::topLeft, 4);
}

void TourOverlay::Callout::resized()
{
    auto area = getLocalBounds().reduced (12, 10);
    auto buttons = area.removeFromBottom (Metrics::buttonHeight - 4);

    skipButton.setBounds (buttons.removeFromLeft (64));
    nextButton.setBounds (buttons.removeFromRight (72));
    buttons.removeFromRight (Metrics::gridHalf);
    backButton.setBounds (buttons.removeFromRight (64));

    area.removeFromBottom (Metrics::gridHalf);
    textArea = area;
}

TourOverlay::TourOverlay()
{
    addAndMakeVisible (callout);
    setVisible (false);
    setWantsKeyboardFocus (true);
    setAlwaysOnTop (true);

    callout.nextButton.onClick = [this] { next(); };
    callout.backButton.onClick = [this] { back(); };
    callout.skipButton.onClick = [this] { skip(); };

    AccessibleSetup::configureDescriptive (*this, "Tour", "The guided tour. Escape ends it.");
}

void TourOverlay::start (std::vector<Onboarding::TourStep> newSteps)
{
    steps = std::move (newSteps);
    running = ! steps.empty();

    if (! running)
        return;

    setVisible (true);
    toFront (true);
    show (0);
}

void TourOverlay::next()
{
    if (! running)
        return;

    if (isShowingEnd())
        end (true);
    else
        show (stepIndex + 1);
}

void TourOverlay::back()
{
    if (running && stepIndex > 0)
        show (stepIndex - 1);
}

void TourOverlay::skip()
{
    if (running)
        end (false);
}

void TourOverlay::end (bool completed)
{
    running = false;
    setVisible (false);

    if (onEnded != nullptr)
        onEnded (completed);
}

const Onboarding::TourStep* TourOverlay::getCurrentStep() const noexcept
{
    return running && juce::isPositiveAndBelow (stepIndex, (int) steps.size()) ? &steps[(size_t) stepIndex] : nullptr;
}

juce::String TourOverlay::getCalloutText() const
{
    return callout.text;
}

void TourOverlay::show (int index)
{
    stepIndex = juce::jlimit (0, (int) steps.size(), index);

    if (const auto* step = getCurrentStep())
    {
        if (prepareStep != nullptr)
            prepareStep (step->id);

        callout.title = step->title;
        callout.text = step->text;
        callout.counter = juce::String (stepIndex + 1) + " / " + juce::String ((int) steps.size());
        callout.nextButton.setButtonText (stepIndex + 1 == (int) steps.size() ? "Finish" : "Next");
        callout.backButton.setEnabled (stepIndex > 0);
        callout.skipButton.setVisible (true);
    }
    else
    {
        // The closing card.
        callout.title = "That's the tour";
        callout.text = Onboarding::kTourEndText;
        callout.counter = {};
        callout.nextButton.setButtonText ("Done");
        callout.backButton.setEnabled (true);
        callout.skipButton.setVisible (false);
    }

    callout.setDescription (callout.title + ". " + callout.text);
    relayout();

    if (isShowing())
        callout.nextButton.grabKeyboardFocus();
}

void TourOverlay::relayout()
{
    if (! running)
        return;

    targetBounds = {};

    if (const auto* step = getCurrentStep(); step != nullptr && findTarget != nullptr)
        targetBounds = findTarget (step->id).getIntersection (getLocalBounds());

    callout.setBounds (placeCallout (targetBounds, getLocalBounds(), { kCalloutWidth, kCalloutHeight }));
    repaint();
}

juce::Rectangle<int> TourOverlay::placeCallout (juce::Rectangle<int> target, juce::Rectangle<int> area,
                                                juce::Point<int> size)
{
    const int margin = Metrics::grid;
    const int gap = 14;   // room for the arrow
    const auto inner = area.reduced (margin);
    const int w = juce::jmin (size.x, inner.getWidth());
    const int h = juce::jmin (size.y, inner.getHeight());

    if (target.isEmpty())
        return inner.withSizeKeepingCentre (w, h);

    auto clampInside = [&inner] (juce::Rectangle<int> r) { return r.constrainedWithin (inner); };
    const int cx = target.getCentreX() - w / 2;
    const int cy = target.getCentreY() - h / 2;

    // Below, above, right, left: the first that fits without covering the target.
    const juce::Rectangle<int> candidates[] =
    {
        { cx, target.getBottom() + gap, w, h },
        { cx, target.getY() - gap - h, w, h },
        { target.getRight() + gap, cy, w, h },
        { target.getX() - gap - w, cy, w, h }
    };

    for (const auto& c : candidates)
    {
        const auto placed = clampInside (c);

        if (! placed.intersects (target))
            return placed;
    }

    // A target that fills the window: the callout sits over its middle.
    return clampInside (juce::Rectangle<int> (cx, cy, w, h));
}

void TourOverlay::paint (juce::Graphics& g)
{
    if (! running)
        return;

    // A soft dim around the target, so the eye goes where the arrow does.
    juce::Path dim;
    dim.addRectangle (getLocalBounds().toFloat());

    if (! targetBounds.isEmpty())
    {
        dim.addRoundedRectangle (targetBounds.toFloat().expanded (4.0f), Metrics::panelCorner * 2.0f);
        dim.setUsingNonZeroWinding (false);
    }

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (dim);

    if (targetBounds.isEmpty())
        return;

    g.setColour (Palette::accent);
    g.drawRoundedRectangle (targetBounds.toFloat().expanded (4.0f), Metrics::panelCorner * 2.0f, 2.0f);

    // The arrow: from the callout's nearest edge to the target.
    const auto c = callout.getBounds().toFloat();
    const auto t = targetBounds.toFloat();
    const auto tip = juce::Point<float> (juce::jlimit (t.getX(), t.getRight(), c.getCentreX()),
                                         juce::jlimit (t.getY(), t.getBottom(), c.getCentreY()));
    const auto start = juce::Point<float> (juce::jlimit (c.getX(), c.getRight(), tip.x),
                                           juce::jlimit (c.getY(), c.getBottom(), tip.y));

    if (start.getDistanceFrom (tip) > 4.0f)
    {
        juce::Path arrow;
        arrow.addArrow ({ start, tip }, 2.0f, 10.0f, 10.0f);
        g.fillPath (arrow);
    }
}

void TourOverlay::resized()
{
    relayout();
}

bool TourOverlay::keyPressed (const juce::KeyPress& key)
{
    // onboarding 3: "Escape ends the tour."
    if (key == juce::KeyPress::escapeKey)
    {
        skip();
        return true;
    }

    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::returnKey)
    {
        next();
        return true;
    }

    if (key == juce::KeyPress::leftKey)
    {
        back();
        return true;
    }

    return false;
}

bool TourOverlay::hitTest (int x, int y)
{
    // Only the callout takes clicks; the window under the dim stays usable.
    return running && callout.getBounds().contains (x, y);
}

//==============================================================================
// Discovery layer
//==============================================================================
DiscoveryLayer::DiscoveryLayer()
{
    setInterceptsMouseClicks (false, false);
    setAccessible (false);
    motion.startTimerHz (*this, 15);
}

DiscoveryLayer::~DiscoveryLayer()
{
    motion.stopTimer();
}

void DiscoveryLayer::addTargets (Kind kind, std::function<std::vector<Target>()> provider)
{
    sources.push_back ({ kind, std::move (provider) });
}

void DiscoveryLayer::update()
{
    std::vector<Mark> next;
    const double ms = juce::Time::getMillisecondCounterHiRes();
    const bool week = Onboarding::isDiscoveryWeek();
    const auto version = Onboarding::getCurrentVersion();

    if (getParentComponent() != nullptr)
    {
        for (const auto& source : sources)
        {
            if (source.provider == nullptr)
                continue;

            for (const auto& target : source.provider())
            {
                auto* c = target.component;

                if (c == nullptr || target.key.isEmpty() || ! Onboarding::isVisibleWithin (*c, getParentComponent()))
                    continue;

                auto bounds = getLocalArea (c, c->getLocalBounds());

                // Scrolled out of sight inside a viewport is not "sight".
                auto* p = c->getParentComponent();

                while (p != nullptr && p != getParentComponent())
                {
                    if (dynamic_cast<juce::Viewport*> (p) != nullptr)
                        bounds = bounds.getIntersection (getLocalArea (p, p->getLocalBounds()));

                    p = p->getParentComponent();
                }

                if (bounds.isEmpty())
                    continue;

                bool due = false;

                switch (source.kind)
                {
                    case Kind::pulse:
                    {
                        if (! week || Onboarding::hasSeen (target.key))
                            break;

                        // First sight starts the pulse; when it has run, it is seen.
                        auto [it, inserted] = firstSeenMs.try_emplace (target.key, ms);

                        if (ms - it->second >= kPulseMs)
                            Onboarding::markSeen (target.key);
                        else
                            due = true;

                        break;
                    }

                    case Kind::tabDot: due = week && ! Onboarding::wasTabOpened (target.key); break;
                    case Kind::newDot: due = Onboarding::isNewDotDue (target.key, version); break;
                }

                if (due)
                    next.push_back ({ source.kind, target.key, bounds });
            }
        }
    }

    const bool changed = next.size() != marks.size()
                      || ! std::equal (next.begin(), next.end(), marks.begin(), [] (const Mark& a, const Mark& b)
                         { return a.kind == b.kind && a.key == b.key && a.bounds == b.bounds; });

    // Only the marks' own areas (a pulse ring grows 7 px out, the NEW badge sits
    // 4 px above): this layer covers the whole window, and a full repaint here
    // repainted everything under it at the pulse rate.
    juce::Rectangle<int> dirty;
    const auto addMark = [&dirty] (const Mark& m, bool pulsesOnly)
    {
        if (! pulsesOnly || m.kind == Kind::pulse)
            dirty = dirty.isEmpty() ? m.bounds.expanded (10) : dirty.getUnion (m.bounds.expanded (10));
    };

    if (changed)
        for (const auto& m : marks)
            addMark (m, false);   // the old marks, to clear them

    marks = std::move (next);

    const bool pulsing = std::any_of (marks.begin(), marks.end(), [] (const Mark& m) { return m.kind == Kind::pulse; });
    const bool animating = pulsing && AnimationPolicy::get().mayAnimate (AnimationPolicy::Decorative);

    if (changed || animating)
        for (const auto& m : marks)
            addMark (m, ! changed);

    if (! dirty.isEmpty())
        repaint (dirty);
}

void DiscoveryLayer::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

    const bool still = ! AnimationPolicy::get().mayAnimate (AnimationPolicy::Decorative);   // cpu-quality-modes 6
    const double phase = std::fmod (juce::Time::getMillisecondCounterHiRes() / 900.0, 1.0);

    for (const auto& mark : marks)
    {
        const auto r = mark.bounds.toFloat();

        switch (mark.kind)
        {
            case Kind::pulse:
            {
                // A subtle ring; under reduced motion it holds still (accessibility 5).
                const float grow = still ? 2.0f : 2.0f + 5.0f * (float) phase;
                const float alpha = still ? 0.8f : 0.9f * (1.0f - (float) phase);

                g.setColour (Palette::accentBright.withAlpha (alpha));
                g.drawRoundedRectangle (r.expanded (grow), Metrics::panelCorner * 2.0f, 2.0f);
                break;
            }

            case Kind::tabDot:
            {
                const float d = 6.0f;
                g.setColour (Palette::accent.withAlpha (0.85f));
                g.fillEllipse (r.getRight() - d - 3.0f, r.getY() + 3.0f, d, d);
                break;
            }

            case Kind::newDot:
            {
                auto badge = juce::Rectangle<float> (26.0f, 12.0f).withPosition (r.getRight() - 28.0f, r.getY() - 4.0f);
                g.setColour (Palette::secondary);
                g.fillRoundedRectangle (badge, 5.0f);
                g.setColour (Palette::plateText);
                g.setFont (Fonts::ui (9.0f, true));
                g.drawText ("NEW", badge, juce::Justification::centred);
                break;
            }
        }
    }
}

//==============================================================================
// Discovery tooltip
//==============================================================================
DiscoveryTooltip::~DiscoveryTooltip()
{
    if (auto* b = button.getComponent())
    {
        b->removeMouseListener (this);

        if (showing)
            b->setTooltip (originalTooltip);
    }
}

void DiscoveryTooltip::attachTo (juce::Button* newButton)
{
    if (auto* old = button.getComponent())
        old->removeMouseListener (this);

    button = newButton;

    if (newButton != nullptr)
        newButton->addMouseListener (this, false);
}

void DiscoveryTooltip::entered()
{
    auto* b = button.getComponent();

    if (b == nullptr || showing || ! Onboarding::isHintDue (kKey))
        return;

    originalTooltip = b->getTooltip();
    b->setTooltip (kText);
    showing = true;

    // Once: the first Randomize hover of the week.
    Onboarding::markSeen (kKey);
}

void DiscoveryTooltip::exited()
{
    if (! showing)
        return;

    showing = false;

    if (auto* b = button.getComponent())
        b->setTooltip (originalTooltip);
}

} // namespace luthier
