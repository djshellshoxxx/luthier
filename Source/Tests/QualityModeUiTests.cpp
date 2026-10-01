/*  cpu-quality-modes.md 12: the GUI tests, CQ-21 to CQ-27.

    CQ-23 and CQ-24 need real paints, so they put the editor on the desktop
    (xvfb in CI) and pump the message loop by hand, running the processor a
    block at a time between messages so strings ring as they would in a host.
*/

#include "TestFramework.h"
#include "QualityTestSupport.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/AnimationPolicy.h"
#include "../UI/QualityBadge.h"
#include "../UI/QualityOptions.h"
#include "../UI/OptionsPages.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/Widgets.h"
#include "../UI/GuitarBodyComponent.h"
#include "../Workshop/WorkshopBench.h"

#include <regex>
#include <set>

#if JUCE_LINUX
 #include <ctime>
 #include <dlfcn.h>
#endif

#if JUCE_MAC
 // macOS has no juce::detail::dispatchNextMessageOnSystemQueue free function
 // (that symbol exists only in JUCE's Linux and Windows messaging back-ends);
 // there the system queue is drained through CFRunLoop, exactly as JUCE's own
 // macOS runDispatchLoopUntil does.
 #include <CoreFoundation/CoreFoundation.h>
#else
 namespace juce::detail { bool dispatchNextMessageOnSystemQueue (bool returnIfNoPendingMessages); }
#endif

using namespace luthier;
using namespace luthier::tests;

namespace
{
   #if JUCE_LINUX
    /*  JUCE looks up the window-manager atoms once, when it connects, with
        "only if they exist"; on a bare Xvfb (no window manager) they do not,
        and setting a window's type then fails with BadAtom. Interning them on
        a connection of our own before JUCE connects - static initialisation,
        before main - makes them exist. The connection stays open so the server
        keeps them. */
    struct PreInternWindowManagerAtoms
    {
        PreInternWindowManagerAtoms()
        {
            if (std::getenv ("DISPLAY") == nullptr)
                return;

            auto* lib = dlopen ("libX11.so.6", RTLD_NOW | RTLD_LOCAL);

            if (lib == nullptr)
                return;

            using OpenFn = void* (*) (const char*);
            using InternFn = unsigned long (*) (void*, const char*, int);
            auto open = (OpenFn) dlsym (lib, "XOpenDisplay");
            auto intern = (InternFn) dlsym (lib, "XInternAtom");

            if (open == nullptr || intern == nullptr)
                return;

            if (auto* display = open (nullptr))
                for (const char* name : { "WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW", "WM_CHANGE_STATE", "WM_STATE",
                                          "_NET_WM_PING", "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_NORMAL",
                                          "_NET_WM_WINDOW_TYPE_COMBO", "_NET_WM_STATE", "_NET_WM_STATE_HIDDEN",
                                          "_NET_WM_STATE_FULLSCREEN", "_NET_WM_ALLOWED_ACTIONS", "_NET_WM_ACTION_MOVE",
                                          "_NET_WM_ACTION_RESIZE", "_NET_WM_ACTION_FULLSCREEN", "_NET_WM_ACTION_MINIMIZE",
                                          "_NET_WM_ACTION_CLOSE", "_NET_WM_MOVERESIZE", "_NET_FRAME_EXTENTS", "_NET_WORKAREA",
                                          "_MOTIF_WM_HINTS", "_WIN_HINTS", "KWM_WIN_DECORATION", "_KDE_NET_WM_WINDOW_TYPE_OVERRIDE" })
                    intern (display, name, 0);
        }
    };

    const PreInternWindowManagerAtoms preInternWindowManagerAtoms;
   #endif

    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    template <typename T>
    T* findChild (juce::Component& root)
    {
        if (auto* t = dynamic_cast<T*> (&root))
            return t;

        for (auto* child : root.getChildren())
            if (auto* t = findChild<T> (*child))
                return t;

        return nullptr;
    }

    double threadCpuSeconds()
    {
       #if JUCE_LINUX
        timespec ts {};
        clock_gettime (CLOCK_THREAD_CPUTIME_ID, &ts);
        return (double) ts.tv_sec + (double) ts.tv_nsec * 1.0e-9;
       #else
        return juce::Time::getMillisecondCounterHiRes() * 0.001;
       #endif
    }

    /** Pumps the message loop for `seconds`, running the processor a block at
        a time in between. Returns the thread CPU seconds spent dispatching. */
    double pump (LuthierAudioProcessor& p, double seconds, bool audio = true)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels()), kBlock);
        juce::MidiBuffer midi;
        const double end = juce::Time::getMillisecondCounterHiRes() + seconds * 1000.0;
        double dispatching = 0.0;

        while (juce::Time::getMillisecondCounterHiRes() < end)
        {
            if (audio)
            {
                buffer.clear();
                midi.clear();
                p.processBlock (buffer, midi);
            }

            const double t0 = threadCpuSeconds();

            for (int i = 0; i < 50; ++i)
            {
               #if JUCE_MAC
                if (CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0, true) != kCFRunLoopRunHandledSource)
                    break;
               #else
                if (! juce::detail::dispatchNextMessageOnSystemQueue (true))
                    break;
               #endif
            }

            dispatching += threadCpuSeconds() - t0;
            juce::Thread::sleep (2);
        }

        return dispatching;
    }

    void ringChord (LuthierAudioProcessor& p)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels()), kBlock);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);   // sustain pedal

        for (int n : { 40, 45, 50, 55, 59, 64 })
            midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);

        buffer.clear();
        p.processBlock (buffer, midi);
    }

    /** The window a host would give the editor. It times every paint pass:
        the root's paint() opens each pass (the editor is not opaque, so it is
        never skipped) and its paintOverChildren() closes it - the editor's
        paint time, without the message thread's other work (CQ-23). */
    struct PaintTimedHost : juce::Component
    {
        void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); started = threadCpuSeconds(); }
        void paintOverChildren (juce::Graphics&) override { seconds += threadCpuSeconds() - started; ++passes; }
        void resized() override { if (auto* c = getChildComponent (0)) c->setBounds (getLocalBounds()); }

        double started = 0.0, seconds = 0.0;
        int passes = 0;
    };

    struct EditorOnDesktop
    {
        explicit EditorOnDesktop (LuthierAudioProcessor& p, bool timed = false)
        {
            editor.reset (dynamic_cast<LuthierAudioProcessorEditor*> (p.createEditor()));
            editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

            if (timed)
            {
                host = std::make_unique<PaintTimedHost>();
                host->setOpaque (true);
                host->addAndMakeVisible (*editor);
                host->setSize (editor->getWidth(), editor->getHeight());
                host->addToDesktop (0);
                host->setVisible (true);
            }
            else
            {
                editor->addToDesktop (0);
                editor->setVisible (true);
            }
        }

        ~EditorOnDesktop()
        {
            if (host != nullptr)
                host->removeFromDesktop();
            else if (editor != nullptr)
                editor->removeFromDesktop();

            editor.reset();
            host.reset();
        }

        std::unique_ptr<PaintTimedHost> host;
        std::unique_ptr<LuthierAudioProcessorEditor> editor;
    };

    juce::KeyPress shortcutFor (const char* actionId)
    {
        if (const auto* binding = AccessibilitySettings::get().findShortcut (actionId))
            return binding->key;

        return {};
    }

    void visitAdvancedTabs (LuthierAudioProcessorEditor& editor, LuthierAudioProcessor& p)
    {
        if (! p.getUiState().advancedMode)
            editor.keyPressed (shortcutFor ("toggleAdvanced"));

        if (auto* advanced = findChild<AdvancedPanel> (editor))
        {
            for (int t = 0; t < advanced->getNumWorkspaceTabs(); ++t)
            {
                advanced->setWorkspaceTab (t);
                pump (p, 0.05, false);
            }

            advanced->setWorkspaceTab (0);
        }
    }
}

//==============================================================================
// CQ-21 Policy truth table
//==============================================================================
LUTHIER_TEST (CpuQualityUi, CQ21_policyTruthTable)
{
    for (bool reduced : { false, true })
    {
        for (int l = 0; l < 3; ++l)
        {
            for (int relief = 0; relief <= 2; ++relief)
            {
                const auto level = (QualityLevel) l;
                const auto st = AnimationPolicy::compute (reduced, level, relief);

                // Table 6.
                MotionLevel expectMotion = MotionLevel::Full;
                bool expectStepped = false;

                if (level == QualityLevel::Low || relief >= 2)       { expectMotion = MotionLevel::Off; expectStepped = true; }
                else if (reduced)                                     { expectMotion = MotionLevel::Off; }
                else if (level == QualityLevel::Medium || relief == 1) { expectMotion = MotionLevel::Limited; }

                const auto label = juce::String ("reduced ") + (reduced ? "on" : "off") + ", " + qualityLevelKey (level)
                                   + ", relief " + juce::String (relief);
                CHECK_MSG (st.motion == expectMotion, label + ": motion");
                CHECK_MSG (st.readoutStepped == expectStepped, label + ": stepped");

                const int deco = AnimationPolicy::frameRateFor (st, AnimationPolicy::Decorative, 120);
                const int trans = AnimationPolicy::frameRateFor (st, AnimationPolicy::Transition, 60);
                const int live = AnimationPolicy::frameRateFor (st, AnimationPolicy::LiveReadout, 30);

                CHECK_MSG (deco == (expectMotion == MotionLevel::Off ? 0 : expectMotion == MotionLevel::Limited ? 30 : 60), label + ": decorative Hz");
                CHECK_MSG (trans == (expectMotion == MotionLevel::Off ? 0 : 60), label + ": transition Hz");
                CHECK_MSG (live == (expectStepped ? 10 : 30), label + ": readout Hz");

                // The live singleton agrees, and getAnimationMs is transitionMs.
                AccessibilitySettings::get().setReducedMotion (reduced);
                auto& policy = AnimationPolicy::get();
                policy.refreshReducedMotion();
                policy.setSource (&policy, level, relief);

                CHECK (policy.getMotion() == expectMotion);
                CHECK (policy.transitionMs (80) == (expectMotion == MotionLevel::Off ? 0 : 80));
                CHECK (AccessibilitySettings::get().getAnimationMs (80) == policy.transitionMs (80));
                CHECK (policy.mayAnimate (AnimationPolicy::LiveReadout));
                CHECK (policy.mayAnimate (AnimationPolicy::Decorative) == (expectMotion != MotionLevel::Off));
                CHECK ((policy.getStringsStyle() == AnimationPolicy::StringsStyle::LowStyle) == (expectMotion == MotionLevel::Limited));
            }
        }
    }

    AccessibilitySettings::get().setReducedMotion (false);
    AnimationPolicy::get().refreshReducedMotion();
    AnimationPolicy::get().removeSource (&AnimationPolicy::get());
}

//==============================================================================
// CQ-22 Registry completeness
//==============================================================================
namespace
{
    /** The class a source line belongs to: the nearest function definition
        above it at column 0, `...Owner::name (`. */
    juce::String owningClass (const juce::StringArray& lines, int index)
    {
        static const std::regex definition (R"(^(?:[\w:<>&*~\s]+?\s+)?((?:\w+::)+)~?\w+\s*\()");

        for (int j = index; j >= 0; --j)
        {
            const auto line = lines[j].toStdString();

            if (line.empty() || line[0] == ' ' || line[0] == '\t')
                continue;

            std::smatch m;

            if (std::regex_search (line, m, definition))
            {
                auto qualified = juce::String (m[1].str()).trimCharactersAtEnd (":");
                return qualified.fromLastOccurrenceOf ("::", false, false);
            }
        }

        return {};
    }

    /** The body of `class Name` in `text`, or empty. */
    juce::String classBody (const juce::String& text, const juce::String& name)
    {
        const std::regex decl ("(^|\\n)\\s*(class|struct)\\s+(\\w+\\s+)?" + name.toStdString() + "\\b[^;{]*\\{");
        const auto s = text.toStdString();
        std::smatch m;

        if (! std::regex_search (s, m, decl))
            return {};

        size_t i = (size_t) (m.position (0) + m.length (0));
        int depth = 1;
        const size_t start = i;

        while (i < s.size() && depth > 0)
        {
            depth += s[i] == '{' ? 1 : s[i] == '}' ? -1 : 0;
            ++i;
        }

        return juce::String (s.substr (start, i - start));
    }
}

LUTHIER_TEST (CpuQualityUi, CQ22_everyTimerDrivenUiClassIsRegisteredOrAllowListed)
{
    const auto root = QualityTestSupport::sourceRoot();
    juce::Array<juce::File> sources;

    for (const auto& e : juce::RangedDirectoryIterator (root.getChildFile ("UI"), true, "*.cpp;*.h", juce::File::findFiles))
        sources.add (e.getFile());

    sources.add (root.getChildFile ("PluginEditor.cpp"));
    sources.add (root.getChildFile ("PluginEditor.h"));

    // Every header and source text, to find a class's declaration wherever it is.
    juce::StringArray texts;

    for (const auto& f : sources)
        texts.add (f.loadFileAsString());

    std::set<juce::String> allowed;

    for (const auto& [name, reason] : AnimationPolicy::getPollOnlyAllowList())
    {
        CHECK_MSG (juce::String (reason).length() > 10, juce::String ("allow-list entry without a reason: ") + name);
        allowed.insert (name);
    }

    const std::regex animates (R"(startTimer|startTimerHz|VBlankAttachment|ComponentAnimator|juce::Animator)");
    int hits = 0;
    std::set<juce::String> checked;

    for (int f = 0; f < sources.size(); ++f)
    {
        if (! sources[f].hasFileExtension ("cpp"))
            continue;

        const auto lines = juce::StringArray::fromLines (texts[f]);

        for (int i = 0; i < lines.size(); ++i)
        {
            const auto trimmed = lines[i].trimStart();

            if (trimmed.startsWith ("//") || trimmed.startsWith ("*") || ! std::regex_search (lines[i].toStdString(), animates))
                continue;

            ++hits;
            const auto cls = owningClass (lines, i);

            if (cls.isEmpty() || checked.count (cls) > 0)
                continue;

            checked.insert (cls);

            if (allowed.count (cls) > 0)
                continue;

            bool registered = false;

            for (const auto& text : texts)
                registered = registered || classBody (text, cls).contains ("AnimationPolicy::Registration");

            CHECK_MSG (registered, sources[f].getFileName() + ":" + juce::String (i + 1) + " - " + cls
                       + " animates or polls on a timer without an AnimationPolicy::Registration "
                         "or a poll-only allow-list entry (cpu-quality-modes 6)");
        }
    }

    CHECK (hits > 40);
    std::cout << "    " << hits << " timer sites in " << (int) checked.size() << " classes" << std::endl;
}

LUTHIER_TEST (CpuQualityUi, CQ22_builtEditorsRegisterEveryTableClassThatExists)
{
    QualityTestSupport::ScopedTempSettings temp;
    LuthierAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);

    std::set<juce::String> seen;

    {
        EditorOnDesktop shown (p);
        visitAdvancedTabs (*shown.editor, p);

        for (const auto& r : AnimationPolicy::get().getRegistry())
            seen.insert (r.name);

        shown.editor->keyPressed (shortcutFor ("toggleAdvanced"));   // Easy
        pump (p, 0.05, false);

        for (const auto& r : AnimationPolicy::get().getRegistry())
            seen.insert (r.name);
    }

    // Section 6's table, by class; a class not in the build yet is skipped
    // (animated strings, the piano-roll chord display: other workstreams).
    const char* table[] = { "StringAnimator", "GuitarBodyComponent", "FretboardComponent", "LevelMeter", "OutputLed",
                            "FeedbackLed", "AmpFacePanel", "DataStreamDisplay", "NoiseEventStrip", "BuzzHeatmap",
                            "ModSourceCard", "TunePanel", "RhythmPanel", "LiveStrip", "TapPad", "BenchIllustration",
                            "QualityBadge", "HeaderBar", "ChordNameDisplay", "PianoRollKeyFade" };

    const auto root = QualityTestSupport::sourceRoot().getChildFile ("UI");
    juce::String allUi;

    for (const auto& e : juce::RangedDirectoryIterator (root, true, "*.h", juce::File::findFiles))
        allUi << e.getFile().loadFileAsString();

    int required = 0;

    // "Exists in the build": declared, and used by something other than its
    // own declaration and definition (DataStreamDisplay is declared but no
    // panel holds one yet).
    juce::StringArray uses;

    for (const auto& e : juce::RangedDirectoryIterator (root, true, "*.cpp;*.h", juce::File::findFiles))
        uses.add (e.getFile().getFileName() + "\n" + e.getFile().loadFileAsString());

    uses.add (QualityTestSupport::sourceRoot().getChildFile ("PluginEditor.h").loadFileAsString());

    for (const auto* name : table)
    {
        if (! std::regex_search (allUi.toStdString(), std::regex (std::string ("class\\s+(\\w+\\s+)?") + name + "\\b")))
            continue;

        int usedElsewhere = 0;

        for (const auto& text : uses)
            if (text.contains (name) && ! text.contains (juce::String ("class ") + name) && ! text.contains (juce::String (name) + "::" + name))
                ++usedElsewhere;

        if (usedElsewhere == 0)
            continue;

        ++required;
        CHECK_MSG (seen.count (name) > 0, juce::String (name) + " exists but no built editor registered it");
    }

    CHECK_MSG (required >= 10, "only " + juce::String (required) + " table classes found in the build");
}

//==============================================================================
// CQ-23 Low means no animation repaints, CQ-24 transitions at Low
//==============================================================================
LUTHIER_TEST (CpuQualityUi, CQ23_lowMeansNoAnimationRepaints)
{
    QualityTestSupport::ScopedTempSettings temp;
    AccessibilitySettings::get().setReducedMotion (false);
    AnimationPolicy::get().refreshReducedMotion();

    LuthierAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);

    int stateChanges = 0;

    auto measure = [&p, &stateChanges] (QualityLevel level, bool advanced, int& decorativePaints, int& worstReadoutPaints,
                                        int& runningDecorativeTimers, double& cpu, int& busiestDecorative)
    {
        EditorOnDesktop shown (p, true);

        if (advanced)
            visitAdvancedTabs (*shown.editor, p);

        p.getQualityController().forceLevelForTesting ((int) level);
        shown.editor->getQualityLink().pumpForTesting();
        ringChord (p);
        pump (p, 0.3);

        // The chord name appearing or going (piano-roll-chord-display 4: it
        // holds 1.2 s) is a change of state, one repaint each, not animation.
        std::function<int (juce::Component&)> nameChanges = [&nameChanges] (juce::Component& c)
        {
            int n = 0;

            if (auto* body = dynamic_cast<GuitarBodyComponent*> (&c))
                n += body->getStaticChordNameChanges();

            for (auto* child : c.getChildren())
                n += nameChanges (*child);

            return n;
        };

        const int namesBefore = nameChanges (*shown.editor);
        AnimationPolicy::get().resetPaintCounts();
        const double paintBefore = shown.host->seconds;
        const double dispatching = pump (p, 2.0);
        stateChanges = nameChanges (*shown.editor) - namesBefore;
        cpu = shown.host->seconds - paintBefore;

        // Timing noise: a second window, and the lower paint time. The paint
        // counts below are the first window's.
        const auto counts = AnimationPolicy::get().getRegistry();
        ringChord (p);
        pump (p, 0.3);
        const double secondBefore = shown.host->seconds;
        pump (p, 2.0);
        cpu = juce::jmin (cpu, shown.host->seconds - secondBefore);

        std::cout << "    " << (advanced ? "Advanced" : "Easy") << " at " << qualityLevelKey (level) << ": paint "
                  << juce::String (cpu * 1000.0, 1) << " ms of " << juce::String (dispatching * 1000.0, 1)
                  << " ms dispatching" << std::endl;

        decorativePaints = worstReadoutPaints = runningDecorativeTimers = busiestDecorative = 0;

        for (const auto& r : counts)
        {
            if (r.motionClass == AnimationPolicy::LiveReadout)
            {
                worstReadoutPaints = juce::jmax (worstReadoutPaints, r.paints);
            }
            else
            {
                decorativePaints += r.paints;
                busiestDecorative = juce::jmax (busiestDecorative, r.paints);
                runningDecorativeTimers += r.timerRunning ? 1 : 0;

                if (level == QualityLevel::Low && (r.paints > 0 || r.timerRunning))
                    std::cout << "    at Low: " << r.name << " painted " << r.paints
                              << (r.timerRunning ? ", timer running" : "") << std::endl;
            }

            if (std::getenv ("CQ23_VERBOSE") != nullptr && r.paints > 0)
                std::cout << "    " << qualityLevelKey (level) << " " << r.name << " " << r.paints << std::endl;

            if (level == QualityLevel::Low && r.motionClass == AnimationPolicy::LiveReadout && r.paints > 21)
                std::cout << "    at Low: readout " << r.name << " painted " << r.paints << std::endl;
        }

        p.getQualityController().forceLevelForTesting (-1);
    };

    for (bool advanced : { true, false })
    {
        int decoLow = 0, readLow = 0, timersLow = 0, busiestLow = 0;
        int decoHigh = 0, readHigh = 0, timersHigh = 0, busiestHigh = 0;
        double cpuLow = 0.0, cpuHigh = 0.0;

        measure (QualityLevel::High, advanced, decoHigh, readHigh, timersHigh, cpuHigh, busiestHigh);
        measure (QualityLevel::Low, advanced, decoLow, readLow, timersLow, cpuLow, busiestLow);

        const juce::String mode = advanced ? "Advanced" : "Easy";
        std::cout << "    " << mode << ": High busiest decorative " << busiestHigh << " paints, "
                  << juce::String (cpuHigh * 1000.0, 1) << " ms; Low decorative " << decoLow
                  << " paints, worst readout " << readLow << ", " << juce::String (cpuLow * 1000.0, 1) << " ms" << std::endl;

        CHECK_MSG (stateChanges <= 2, mode + ": the chord name changed " + juce::String (stateChanges) + " times in 2 s");
        CHECK_MSG (decoLow <= stateChanges, mode + ": Decorative and Transition registrants painted " + juce::String (decoLow)
                                            + " times at Low, for " + juce::String (stateChanges) + " chord-name changes");
        CHECK_MSG (timersLow == 0, mode + ": " + juce::String (timersLow) + " Decorative / Transition timers still running at Low");
        CHECK_MSG (readLow <= 21, mode + ": a live readout painted " + juce::String (readLow) + " times in 2 s at Low");
        CHECK_MSG (busiestHigh > 30, mode + ": the control failed - nothing decorative animated at High (" + juce::String (busiestHigh) + ")");

       #if JUCE_LINUX
        CHECK_MSG (cpuLow <= 0.4 * cpuHigh, mode + ": editor paint time at Low " + juce::String (cpuLow * 1000.0, 1)
                   + " ms is not 60 % below High's " + juce::String (cpuHigh * 1000.0, 1) + " ms");
       #endif
    }
}

LUTHIER_TEST (CpuQualityUi, CQ24_transitionsAreInstantAtLow)
{
    QualityTestSupport::ScopedTempSettings temp;
    LuthierAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);
    EditorOnDesktop shown (p);
    visitAdvancedTabs (*shown.editor, p);

    p.getQualityController().forceLevelForTesting ((int) QualityLevel::Low);
    shown.editor->getQualityLink().pumpForTesting();
    pump (p, 0.2);

    CHECK (AnimationPolicy::get().transitionMs (80) == 0);
    CHECK (! AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition));

    // MIDI Learn armed: a steady outline, nothing animating for 2 s.
    shown.editor->keyPressed (shortcutFor ("midiLearnArm"));
    pump (p, 0.2);   // the arm's own paint: the outline appearing is the state change
    AnimationPolicy::get().resetPaintCounts();
    pump (p, 2.0);

    int animated = 0;

    for (const auto& r : AnimationPolicy::get().getRegistry())
        if (r.motionClass != AnimationPolicy::LiveReadout)
        {
            animated += r.paints;

            if (r.paints > 0)
                std::cout << "    MIDI Learn at Low: " << r.name << " painted " << r.paints << std::endl;
        }

    CHECK_MSG (animated == 0, "something decorative repainted while MIDI Learn was armed at Low");
    shown.editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));

    // A notification appears in one frame: visible, full size, at once.
    shown.editor->getNotifications().post ({ "cq24", "One frame", Notification::Level::info });
    pump (p, 0.05, false);
    CHECK (shown.editor->getNotifications().isVisible());
    CHECK (shown.editor->getNotifications().getHeight() > 0);

    p.getQualityController().forceLevelForTesting (-1);
}

//==============================================================================
// CQ-25 Reduced motion independence
//==============================================================================
LUTHIER_TEST (CpuQualityUi, CQ25_reducedMotionAndLowStayIndependent)
{
    QualityTestSupport::ScopedTempSettings temp;
    LuthierAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);
    EditorOnDesktop shown (p);

    AccessibilitySettings::get().setReducedMotion (true);
    AnimationPolicy::get().refreshReducedMotion();
    shown.editor->getQualityLink().pumpForTesting();

    CHECK (! AnimationPolicy::get().mayAnimate (AnimationPolicy::Decorative));
    CHECK (! AnimationPolicy::get().isReadoutStepped());

    // LiveReadout keeps its requested rate under reduced motion at High.
    bool meterAt30 = false;

    for (const auto& r : AnimationPolicy::get().getRegistry())
        if (r.name == "LevelMeter" || r.name == "OutputLed")
            meterAt30 = meterAt30 || r.appliedHz == 30;

    CHECK (meterAt30);

    AccessibilitySettings::get().setReducedMotion (false);
    AnimationPolicy::get().refreshReducedMotion();

    // Low never flips the Reduced motion preference.
    PerformanceSettings::get().setQuality (QualityChoice::Low);
    shown.editor->getQualityLink().pumpForTesting();
    CHECK (AnimationPolicy::get().getMotion() == MotionLevel::Off);
    CHECK (! AccessibilitySettings::get().isReducedMotion());

    AppearancePage page (p);
    page.refresh();
    auto* toggle = findChild<juce::ToggleButton> (page);
    CHECK (toggle != nullptr);

    bool anyReducedChecked = false;

    for (auto* c : page.getChildren())
        if (auto* t = dynamic_cast<juce::ToggleButton*> (c))
            if (t->getButtonText() == "Reduced motion")
                anyReducedChecked = t->getToggleState();

    CHECK (! anyReducedChecked);
}

//==============================================================================
// CQ-26 UI
//==============================================================================
LUTHIER_TEST (CpuQualityUi, CQ26_audioPageBadgeAndAppearanceNote)
{
    QualityTestSupport::ScopedTempSettings temp;
    LuthierAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);

    for (bool advanced : { false, true })
    {
        EditorOnDesktop shown (p);

        if (advanced)
            shown.editor->keyPressed (shortcutFor ("toggleAdvanced"));

        // The badge in all six states.
        struct State { QualityChoice choice; QualityLevel autoLevel; const char* expect; };
        const State states[] = { { QualityChoice::High, QualityLevel::High, "HIGH" },
                                 { QualityChoice::Medium, QualityLevel::High, "MED" },
                                 { QualityChoice::Low, QualityLevel::High, "LOW" },
                                 { QualityChoice::Auto, QualityLevel::High, "AUTO\xc2\xb7H" },
                                 { QualityChoice::Auto, QualityLevel::Medium, "AUTO\xc2\xb7M" },
                                 { QualityChoice::Auto, QualityLevel::Low, "AUTO\xc2\xb7L" } };

        for (const auto& st : states)
        {
            PerformanceSettings::get().setAutoLastLevel (st.autoLevel);
            PerformanceSettings::get().setQuality (st.choice);
            p.getQualityController().restartAutoForTesting();
            shown.editor->getQualityBadge().refresh();
            CHECK_MSG (shown.editor->getQualityBadge().getLabelText() == juce::String::fromUTF8 (st.expect),
                       "badge shows " + shown.editor->getQualityBadge().getLabelText());
        }

        if (! advanced)
            CHECK (shown.editor->getQualityBadge().getShareText() == "-");   // not playing yet

        // Clicking the badge opens AUDIO with focus in the group.
        PerformanceSettings::get().setQuality (QualityChoice::High);

        // A click on the badge comes with the window focused; under xvfb an
        // earlier test's window may still hold it.
        shown.editor->toFront (true);
        pump (p, 0.3, false);   // the window manager's FocusIn lands before the click
        shown.editor->getQualityBadge().onOpen();
        pump (p, 0.05, false);

        auto* options = findChild<QualityOptions> (*shown.editor);
        CHECK (options != nullptr && options->isShowing());

        if (options != nullptr)
        {
            bool focused = false;

            for (int i = 0; i < 4; ++i)
                focused = focused || options->getPill (QualityOptions::pillChoice (i)).hasKeyboardFocus (false);

            if (! focused)
            {
                auto* f = juce::Component::getCurrentlyFocusedComponent();
                std::cout << "    focus: " << (f ? typeid (*f).name() : "none") << " peerFocused "
                          << (shown.editor->getPeer() && shown.editor->getPeer()->isFocused()) << " advanced " << (int) advanced << std::endl;
            }

            CHECK_MSG (focused, "the badge did not put focus in the CPU quality group");

            // Four pills in one radio group, the override combo, two toggles, the status line.
            for (int i = 0; i < 4; ++i)
                CHECK (options->getPill (QualityOptions::pillChoice (i)).getRadioGroupId() != 0);

            CHECK (options->getOverrideBox().getNumItems() == 5);
            CHECK (options->getOfflineToggle().isShowing() && options->getNotifyToggle().isShowing());
            CHECK (options->getStatusText().isNotEmpty());

            // The oversampling note shows only while capped.
            p.getQualityController().forceLevelForTesting ((int) QualityLevel::High);
            QualityTestSupport::setChoice (p, ParamIDs::oversample, 2);   // 4x
            p.getParameterBridge().applyAllNow();
            pump (p, 0.02);
            options->refresh();
            CHECK (! options->getOversamplingNote().isVisible());

            p.getQualityController().forceLevelForTesting ((int) QualityLevel::Medium);
            pump (p, 0.02);
            options->refresh();
            CHECK (options->getOversamplingNote().isVisible());
            CHECK (options->getOversamplingNote().getText().contains ("2x"));
            p.getQualityController().forceLevelForTesting (-1);

            // The disclosure.
            CHECK (! options->isDisclosureOpen());
            options->setDisclosureOpen (true);
            CHECK (options->isDisclosureOpen());
            options->setDisclosureOpen (false);
        }
    }

    // The Low note on AppearancePage.
    PerformanceSettings::get().setQuality (QualityChoice::Low);
    AppearancePage page (p);
    page.setSize (700, 400);
    page.refresh();

    bool noteShown = false;

    for (auto* c : page.getChildren())
        if (auto* l = dynamic_cast<juce::Label*> (c))
            noteShown = noteShown || (l->isVisible() && l->getText().contains ("Animations are off"));

    CHECK (noteShown);
}

LUTHIER_TEST (CpuQualityUi, CQ26_masterOversamplingTooltipCarriesTheCap)
{
    // Section 5: Advanced column 3's Master oversampling control has the
    // AUDIO page's "Running at 2x while quality is Medium." as its tooltip.
    QualityTestSupport::ScopedTempSettings temp;
    LuthierAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);
    EditorOnDesktop shown (p);

    if (! p.getUiState().advancedMode)
        shown.editor->keyPressed (shortcutFor ("toggleAdvanced"));

    auto tooltip = [&shown]
    {
        juce::String found;
        std::function<void (juce::Component&)> visit = [&] (juce::Component& c)
        {
            if (auto* t = dynamic_cast<juce::SettableTooltipClient*> (&c))
                if (t->getTooltip().startsWith ("Oversampling for the nonlinear stages"))
                    found = t->getTooltip();

            for (auto* child : c.getChildren())
                visit (*child);
        };
        visit (*shown.editor);
        return found;
    };

    juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels()), kBlock);
    juce::MidiBuffer midi;

    for (auto level : { QualityLevel::Medium, QualityLevel::High })
    {
        p.getQualityController().forceLevelForTesting ((int) level);
        buffer.clear();
        p.processBlock (buffer, midi);
        shown.editor->getQualityLink().pumpForTesting();

        const auto note = QualityOptions::oversamplingNoteFor (p);
        CHECK_MSG (tooltip().isNotEmpty(), "no Master oversampling control found");
        CHECK_MSG (level == QualityLevel::High ? note.isEmpty() : note.contains ("Running at 2x"), "note: " + note);
        CHECK_MSG (level == QualityLevel::High ? ! tooltip().contains ("Running at") : tooltip().contains (note),
                   "tooltip: " + tooltip());
    }

    p.getQualityController().forceLevelForTesting (-1);
}

//==============================================================================
// Section 7 reaching the UI (ported from the superseded performance-budget 8
// ladder's CpuReliefUi tests: stream, audition, opt-out)
//==============================================================================
LUTHIER_TEST (CpuQualityUi, governorReliefSuspendsTheStreamAndFreezesTheAudition)
{
    QualityTestSupport::ScopedTempSettings temp;
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& controller = processor.getQualityController();
    controller.stopTimerForTesting();
    double now = 0.0;
    QualityController::LoadSnapshot load;
    controller.setClockForTesting ([&now] { return now; });
    controller.setLoadFeedForTesting ([&load] { return load; });

    auto tickFor = [&] (double ms)
    {
        for (double t = 0.0; t < ms; t += 100.0)
        {
            now += 100.0;
            controller.tick();
        }
    };

    // E1 then E2: 200 ms above 85 %, then 200 ms above 90 %.
    load.mean200ms = 0.95; load.mean2s = 0.95; load.measuredSeconds = 30.0;
    tickFor (800.0);
    CHECK (controller.getReliefLevel() == 2);

    // E1: the stream does no work, even with records arriving.
    DataStreamDisplay stream;
    stream.setSource (&processor);
    juce::Component parent;
    parent.addAndMakeVisible (stream);
    stream.setBounds (0, 0, 300, 100);
    const bool wasEnabled = DataStreamDisplay::isEnabledByUser();
    DataStreamDisplay::setEnabledByUser (true);
    processor.getDiagnostics().setCrashLogEnabled (true);
    processor.getDiagnostics().log (LogCategory::Engine, "governor test record");
    stream.update (1000.0);
    CHECK (! stream.isScrolling());
    CHECK (stream.getNumLinesKept() == 0);

    // E2: no new audition starts.
    auto& bench = processor.getBench();
    const auto candidates = processor.getPartLibrary().getParts (PartType::bridge);
    CHECK (! candidates.isEmpty());

    if (! candidates.isEmpty())
    {
        bench.beginAudition (GuitarSlot::bridge, candidates[0]);
        CHECK_MSG (! bench.isAuditioning(), "an audition started at E2");
    }

    // The load falls (2 s mean below 70 %): both work again.
    load.mean200ms = 0.3; load.mean2s = 0.3;
    tickFor (800.0);
    CHECK (controller.getReliefLevel() == 0);

    processor.getDiagnostics().log (LogCategory::Engine, "governor test record 2");
    stream.update (2000.0);
    CHECK (stream.getNumLinesKept() > 0);

    if (! candidates.isEmpty())
    {
        bench.beginAudition (GuitarSlot::bridge, candidates[0]);
        CHECK (bench.isAuditioning());
        bench.endAudition();
    }

    processor.getDiagnostics().setCrashLogEnabled (false);
    DataStreamDisplay::setEnabledByUser (wasEnabled);
    controller.setLoadFeedForTesting ({});
    controller.setClockForTesting ({});
}

LUTHIER_TEST (CpuQualityUi, theDiagnosticsOptOutIsSavedAndReachesE3)
{
    QualityTestSupport::ScopedTempSettings temp;
    LuthierAudioProcessor processor;

    // Section 5: one toggle, the old relief 7 opt-out renamed.
    DiagnosticsPage page (processor);
    page.refresh();
    juce::ToggleButton* toggle = nullptr;
    int dropToggles = 0;

    for (auto* child : page.getChildren())
        if (auto* t = dynamic_cast<juce::ToggleButton*> (child); t != nullptr && t->getButtonText().containsIgnoreCase ("drop"))
        {
            toggle = t;
            ++dropToggles;
        }

    CHECK_MSG (dropToggles == 1, juce::String (dropToggles) + " string-drop toggles on DIAGNOSTICS");
    CHECK (toggle != nullptr);

    if (toggle != nullptr)
    {
        CHECK (toggle->getToggleState());   // default on
        toggle->setToggleState (false, juce::sendNotificationSync);
        CHECK (! PerformanceSettings::get().isEmergencyStringDrop());
        toggle->setToggleState (true, juce::sendNotificationSync);
        CHECK (PerformanceSettings::get().isEmergencyStringDrop());
    }
}

//==============================================================================
// CQ-27 Accessibility
//==============================================================================
LUTHIER_TEST (CpuQualityUi, CQ27_groupKeysNamesOrderAnnouncementsAndShortcut)
{
    QualityTestSupport::ScopedTempSettings temp;
    PerformanceSettings::get().setQuality (QualityChoice::High);
    LuthierAudioProcessor p;
    p.prepareToPlay (kSr, kBlock);
    EditorOnDesktop shown (p);

    shown.editor->openQualityOptions();
    pump (p, 0.05, false);
    auto* options = findChild<QualityOptions> (*shown.editor);
    CHECK (options != nullptr);

    if (options != nullptr)
    {
        // A named group of radio buttons, each with a description.
        CHECK (options->getGroup().getTitle() == "CPU quality");

        if (auto* h = options->getGroup().getAccessibilityHandler())
            CHECK (h->getRole() == juce::AccessibilityRole::group);

        for (int i = 0; i < 4; ++i)
        {
            auto& pill = options->getPill (QualityOptions::pillChoice (i));
            CHECK (pill.getTitle().isNotEmpty());
            CHECK (pill.getDescription().isNotEmpty());

            if (auto* h = pill.getAccessibilityHandler())
                CHECK (h->getRole() == juce::AccessibilityRole::radioButton);
        }

        // Arrow keys cycle the group: High -> Medium -> Low -> Auto -> High.
        const QualityChoice order[] = { QualityChoice::Medium, QualityChoice::Low, QualityChoice::Auto, QualityChoice::High };

        for (auto expect : order)
        {
            options->getGroup().keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
            CHECK (PerformanceSettings::get().getQuality() == expect);
        }
    }

    // The badge is a focusable button, last in the footer's tab order.
    auto& badge = shown.editor->getQualityBadge();
    CHECK (badge.getWantsKeyboardFocus());

    if (auto* h = badge.getAccessibilityHandler())
        CHECK (h->getRole() == juce::AccessibilityRole::button);

    for (auto* c : shown.editor->getChildren())
        if (c != &badge && c->getBottom() >= shown.editor->getHeight() - Metrics::footerHeight && c->isVisible())
            CHECK_MSG (c->getExplicitFocusOrder() < badge.getExplicitFocusOrder(), "a footer control comes after the badge");

    // Announcements are rate-limited to one per 5 s.
    auto& link = shown.editor->getQualityLink();
    const int before = link.getAnnouncementCount();
    PerformanceSettings::get().setQuality (QualityChoice::Auto);
    p.getQualityController().stopTimerForTesting();

    double now = 0.0;
    QualityController::LoadSnapshot load;
    load.mean2s = 0.9;
    p.getQualityController().setClockForTesting ([&] { return now; });
    p.getQualityController().setLoadFeedForTesting ([&] { return load; });
    PerformanceSettings::get().setAutoLastLevel (QualityLevel::High);
    p.getQualityController().restartAutoForTesting();

    for (; now < 6000.0; now += 100.0)
    {
        p.getQualityController().tick();
        link.pumpForTesting();
    }

    CHECK (p.getQualityController().getAutoLevel() == QualityLevel::Low);   // two notices...
    CHECK (link.getAnnouncementCount() - before == 1);                      // ...one announcement

    // "Cycle CPU quality" can be bound, and then works.
    PerformanceSettings::get().setQuality (QualityChoice::High);
    const juce::KeyPress key ('q', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, 0);
    CHECK (AccessibilitySettings::get().rebind ("cycleCpuQuality", key));
    CHECK (shown.editor->keyPressed (key));
    CHECK (PerformanceSettings::get().getQuality() == QualityChoice::Medium);
    AccessibilitySettings::get().resetShortcut ("cycleCpuQuality");
}
