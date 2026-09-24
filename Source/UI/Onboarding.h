#pragma once

/*  Onboarding after the first-run defaults (onboarding.md 2, 3, 4, 11, 13;
    gui-integration.md 20). TUNE-HELP-ONBOARDING workstream.

    FirstRun.h applies the OS-following defaults and knows what a first session
    is. This file is everything the person sees afterwards:

      STATE (2, 4, 11, 13) - the counters behind the banner and the hints, all
          in UiPreferences (so "Restore first-run experience" re-arms all of
          them with one reset):
            - launches, and the date of the first one: the discovery week is
              "7 launches or 7 calendar days, whichever first" (4);
            - the welcome banner: which version was last welcomed, how many
              times the tour was offered ("up to 3 times total"), and whether
              it was answered or refused for good (2, 11);
            - "seen" flags for every first-sight pulse and the tabs opened (4);
            - the first launch of each version, which starts the one-week
              "NEW" dot on anything that version introduced (13, gui 20).

      WELCOME BANNER (2, 13) - non-modal, under the header, with the spec's
          three answers, or the upgrade line "Version X.Y.Z installed. What's
          new?" once per version.

      TOUR (3) - twelve stops, one callout each with Next / Back / Skip and
          Escape to end, the arrow on the control it names. TourOverlay does
          not know the editor: a stop names a target id and the host resolves
          it to a rectangle (and gets a chance to prepare the window for it -
          switch to Advanced, open a tab). A stop whose target cannot be found
          is shown centred, never pointing at nothing. The optional TECHNIQUES
          stop (gui-techniques-updates) is included only when the host says
          that tab exists.

      DISCOVERY LAYER (4, gui 20) - one transparent component over the window
          that paints the first-week hints: a pulse on first sight (the ?
          icons, the wrench, the TUNE tab, the Slide glyph), a soft dot on
          every column-4 tab not yet opened, and the NEW dot. It never takes
          a click, so it cannot get between a person and a control.

      DISCOVERY TOOLTIP (4) - "Fresh sound in one click." on the first
          Randomise hover of the week.

    Everything here is message thread.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

#include <functional>
#include <map>
#include <vector>

namespace luthier
{

namespace Onboarding
{
    //==========================================================================
    // UiPreferences keys.
    inline constexpr const char* kLaunchCountKey       = "onboarding_launch_count";
    inline constexpr const char* kFirstLaunchKey       = "onboarding_first_launch";
    inline constexpr const char* kWelcomeVersionKey    = "welcome_version";
    inline constexpr const char* kWelcomeShowsKey      = "welcome_tour_offers";
    inline constexpr const char* kWelcomeAnsweredKey   = "welcome_tour_answered";
    inline constexpr const char* kWelcomeDontAskKey    = "welcome_dont_ask";
    inline constexpr const char* kSeenPrefix           = "discovery_seen_";
    inline constexpr const char* kTabOpenedPrefix      = "discovery_tab_opened_";
    inline constexpr const char* kVersionLaunchPrefix  = "version_first_launch_";

    /** onboarding 2: "up to 3 times total". */
    inline constexpr int kMaxTourOffers = 3;

    /** onboarding 4: "7 launches or 7 calendar days, whichever first". */
    inline constexpr int kDiscoveryLaunches = 7;
    inline constexpr int kDiscoveryDays = 7;

    /** gui-integration 20: the NEW dot lasts one week. */
    inline constexpr int kNewDotDays = 7;

    /** onboarding 1's "Factory / Rock / Modern Overdrive" on the Les Paul-style
        guitar: after the trademark sweep (TODO 2g) the factory bank's rock
        overdrive on the single-cut is "Single-Cut Crunch". */
    inline constexpr const char* kFirstRunPreset = "Single-Cut Crunch";

    /** onboarding 3 stop 2 names "Acoustic Fingerstyle"; the bank's is this. */
    inline constexpr const char* kTourSuggestedPreset = "Fingerstyle Folk";

    /** The version this build is. */
    juce::String getCurrentVersion();

    //==========================================================================
    /** Counts this launch, once per process (a second editor is not a launch),
        and dates the first launch and this version's first launch. The editor
        calls it on construction. */
    void recordLaunch (const juce::String& version);

    int getLaunchCount();

    /** onboarding 4: within the first week. */
    bool isDiscoveryWeek();

    /** A first-sight hint: owed in the discovery week until seen. */
    bool isHintDue (const juce::String& key);
    void markSeen (const juce::String& key);
    bool hasSeen (const juce::String& key);

    /** onboarding 4: "Every unused Column 4 tab shows a soft accent dot until first opened." */
    void markTabOpened (const juce::String& tabName);
    bool wasTabOpened (const juce::String& tabName);
    bool isTabDotDue (const juce::String& tabName);

    //==========================================================================
    /** gui-integration 20 / onboarding 13: a feature introduced after 1.0,
        and the version that brought it. The list lives in Onboarding.cpp; a
        feature's entry point asks isNewDotDue with its key. */
    struct NewFeature
    {
        const char* key;
        const char* introducedIn;
    };

    const std::vector<NewFeature>& getNewFeatures();

    /** True for a week after the first launch of the version that introduced
        `featureKey`, until the entry point is used (markSeen ("new_" + key)). */
    bool isNewDotDue (const juce::String& featureKey, const juce::String& currentVersion);

    //==========================================================================
    enum class Welcome { none, tourOffer, upgrade };

    /** What the banner should say at this launch (2, 11, 13). */
    Welcome getWelcomeDue (const juce::String& version);

    /** The banner went up. */
    void noteWelcomeShown (Welcome kind, const juce::String& version);

    enum class Answer { yes, later, never, dismissed };
    void answerWelcome (Answer answer);

    //==========================================================================
    /** Tests: where "now" comes from, and forgetting that this process has
        already counted its launch. */
    void setClockForTesting (std::function<juce::Time()> clock);
    void resetProcessStateForTesting();

    /** The test runner turns the editor's automatic part off (the launch count
        and the welcome banner), as it does FirstRun: an editor built by an
        unrelated test must not count launches in the real ui.json. */
    void setAutomaticForTesting (bool automatic) noexcept;
    bool isAutomatic() noexcept;

    //==========================================================================
    /** One stop of the tour (onboarding 3). */
    struct TourStep
    {
        juce::String id;       ///< Also the target the host resolves.
        juce::String title;
        juce::String text;
    };

    /** The twelve stops in the spec's order, with the Techniques stop after
        "The workspace" when `includeTechniques`. */
    std::vector<TourStep> getTourSteps (bool includeTechniques);

    /** onboarding 3: "At the end: You're ready. Have fun." */
    inline constexpr const char* kTourEndText = "You're ready. Have fun.";

    /** True when `c` and every parent up to `root` are visible: on screen as far
        as the window is concerned, whether or not the window itself is. */
    bool isVisibleWithin (const juce::Component& c, const juce::Component* root);

    /** Finds the first descendant (or root itself) that satisfies `match`. */
    juce::Component* findComponent (juce::Component& root, const std::function<bool (juce::Component&)>& match);

    /** A button under `root` whose text is `text`, case-insensitive; showing ones first. */
    juce::Button* findButton (juce::Component& root, const juce::String& text);
}

//==============================================================================
/** onboarding 2 and 13: the welcome banner under the header. */
class WelcomeBanner : public juce::Component
{
public:
    WelcomeBanner();

    static constexpr int preferredHeight = 32;

    /** Shows the banner for `kind` (none hides it). */
    void showFor (Onboarding::Welcome kind, const juce::String& version);
    Onboarding::Welcome getKind() const noexcept { return kind; }

    juce::String getMessage() const { return message; }

    /** The buttons, left to right as shown. */
    juce::TextButton& getFirstButton() noexcept  { return first; }
    juce::TextButton& getSecondButton() noexcept { return second; }
    juce::TextButton& getThirdButton() noexcept  { return third; }

    std::function<void()> onStartTour;
    std::function<void()> onWhatsNew;

    /** Called when the banner appears or goes, so the window can re-lay out. */
    std::function<void()> onVisibilityChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void close();

    Onboarding::Welcome kind = Onboarding::Welcome::none;
    juce::String message;
    juce::TextButton first, second, third;
    juce::Rectangle<int> textBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WelcomeBanner)
};

//==============================================================================
/** onboarding 3: the tour. Laid over the whole window; only its callout takes
    clicks, so the person can do what a stop suggests while it is up. */
class TourOverlay : public juce::Component
{
public:
    TourOverlay();

    /** Where a stop's target is, in this component's coordinates; empty when
        it cannot be found (the callout is then centred). */
    std::function<juce::Rectangle<int> (const juce::String& stepId)> findTarget;

    /** Called before a stop is shown, so the host can make its target visible. */
    std::function<void (const juce::String& stepId)> prepareStep;

    /** Called when the tour ends: finished, skipped or Escaped. */
    std::function<void (bool completed)> onEnded;

    void start (std::vector<Onboarding::TourStep> steps);
    void next();
    void back();
    void skip();

    bool isRunning() const noexcept { return running; }
    int getStepIndex() const noexcept { return stepIndex; }
    int getNumSteps() const noexcept { return (int) steps.size(); }

    /** True on the closing card ("You're ready. Have fun."). */
    bool isShowingEnd() const noexcept { return running && stepIndex >= (int) steps.size(); }

    const Onboarding::TourStep* getCurrentStep() const noexcept;

    juce::Rectangle<int> getTargetBounds() const noexcept   { return targetBounds; }
    juce::Rectangle<int> getCalloutBounds() const noexcept  { return callout.getBounds(); }
    juce::String getCalloutText() const;

    juce::TextButton& getNextButton() noexcept { return callout.nextButton; }
    juce::TextButton& getBackButton() noexcept { return callout.backButton; }
    juce::TextButton& getSkipButton() noexcept { return callout.skipButton; }

    /** Where a callout of `size` goes for `target` inside `area`: below it if
        there is room, else above, right, left; clamped inside `area`; centred
        when the target is empty. Pure, for the alignment test. */
    static juce::Rectangle<int> placeCallout (juce::Rectangle<int> target, juce::Rectangle<int> area,
                                              juce::Point<int> size);

    static constexpr int kCalloutWidth = 320;
    static constexpr int kCalloutHeight = 132;

    /** Re-resolves and re-places the current stop (the host's window moved). */
    void relayout();

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool hitTest (int x, int y) override;

private:
    void show (int index);
    void end (bool completed);

    struct Callout : public juce::Component
    {
        Callout();
        void paint (juce::Graphics&) override;
        void resized() override;

        juce::String title, text, counter;
        juce::TextButton backButton { "Back" }, nextButton { "Next" }, skipButton { "Skip" };
        juce::Rectangle<int> textArea;
    };

    std::vector<Onboarding::TourStep> steps;
    int stepIndex = 0;
    bool running = false;
    juce::Rectangle<int> targetBounds;
    Callout callout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TourOverlay)
};

//==============================================================================
/** onboarding 4 and gui-integration 20: the first-week hints, painted over the
    window. Never takes a click. */
class DiscoveryLayer : public juce::Component,
                       private juce::Timer
{
public:
    enum class Kind
    {
        pulse,   ///< A ring that pulses for a few seconds on first sight, then is marked seen.
        tabDot,  ///< A soft accent dot until the tab is opened.
        newDot   ///< "NEW", for a week after the version that introduced it.
    };

    /** A target: its hint key and the component it marks (nullptr when absent). */
    struct Target
    {
        juce::String key;
        juce::Component* component = nullptr;
    };

    DiscoveryLayer();
    ~DiscoveryLayer() override;

    /** Adds a source of targets. The provider is asked on every tick, so a
        target that comes and goes (a tab, a ? icon in a scrolled column) is
        found wherever it is now. */
    void addTargets (Kind kind, std::function<std::vector<Target>()> provider);

    /** How long a first-sight pulse lasts once seen. */
    static constexpr double kPulseMs = 4000.0;

    /** What is painted now, in this component's coordinates: for tests. */
    struct Mark
    {
        Kind kind;
        juce::String key;
        juce::Rectangle<int> bounds;
    };

    const std::vector<Mark>& getMarks() const noexcept { return marks; }

    /** One tick now (the timer's work); tests call it. */
    void update();

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { update(); }

    struct Source
    {
        Kind kind;
        std::function<std::vector<Target>()> provider;
    };

    std::vector<Source> sources;
    std::vector<Mark> marks;
    std::map<juce::String, double> firstSeenMs;   ///< pulse key -> when first on screen

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiscoveryLayer)
};

//==============================================================================
/** onboarding 4: "The randomize dice shows a tooltip on first Randomize hover:
    Fresh sound in one click." Attached as a mouse listener to the button; the
    button's own tooltip comes back after that one hover. */
class DiscoveryTooltip : private juce::MouseListener
{
public:
    static constexpr const char* kKey = "randomise_tooltip";
    static constexpr const char* kText = "Fresh sound in one click.";

    DiscoveryTooltip() = default;
    ~DiscoveryTooltip() override;

    void attachTo (juce::Button* button);
    juce::Button* getButton() const noexcept { return button.getComponent(); }

    /** What the mouse entering does; public for tests. */
    void entered();
    void exited();

private:
    void mouseEnter (const juce::MouseEvent&) override { entered(); }
    void mouseExit (const juce::MouseEvent&) override  { exited(); }

    juce::Component::SafePointer<juce::Button> button;
    juce::String originalTooltip;
    bool showing = false;
};

} // namespace luthier
