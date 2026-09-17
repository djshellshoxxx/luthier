#include "Overlays.h"
#include "../PluginProcessor.h"

namespace luthier
{

//==============================================================================
//  OverlayPanel
//==============================================================================
OverlayPanel::OverlayPanel (const juce::String& t)
    : title (t)
{
    addAndMakeVisible (closeButton);
    closeButton.setTooltip ("Close (Escape)");
    closeButton.onClick = [this] { if (onDismiss) onDismiss(); };

    setWantsKeyboardFocus (true);

    // Clicks must not fall through to the scrim behind, or clicking inside the
    // panel would dismiss it.
    setInterceptsMouseClicks (true, true);
}

OverlayPanel::~OverlayPanel() = default;

juce::Rectangle<int> OverlayPanel::getContentBounds() const
{
    return getLocalBounds().withTrimmedTop (titleBarHeight).reduced (Metrics::windowPadding);
}

void OverlayPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::DropShadow (juce::Colour (0xcc000000), 24, { 0, 6 }).drawForRectangle (g, getLocalBounds());

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, Metrics::windowCorner);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), Metrics::windowCorner, 1.0f);

    auto titleBar = getLocalBounds().removeFromTop (titleBarHeight);

    LuthierLookAndFeel::drawSectionHeader (g, titleBar.reduced (Metrics::windowPadding, 0), title);
    LuthierLookAndFeel::drawSeparator (g, { Metrics::windowPadding, titleBarHeight - 1,
                                            getWidth() - Metrics::windowPadding * 2, 1 });
}

void OverlayPanel::resized()
{
    closeButton.setBounds (getWidth() - Metrics::windowPadding - 72,
                           (titleBarHeight - Metrics::buttonHeight) / 2, 72, Metrics::buttonHeight);

    layoutContent (getContentBounds());
}

bool OverlayPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onDismiss)
            onDismiss();

        return true;
    }

    return false;
}

//==============================================================================
//  OverlayHost
//==============================================================================
OverlayHost::OverlayHost()
{
    setInterceptsMouseClicks (true, true);
    setVisible (false);
}

OverlayHost::~OverlayHost() = default;

void OverlayHost::show (OverlayPanel* panel)
{
    if (panel == nullptr)
        return;

    // Only one overlay at a time, structurally.
    if (current != nullptr && current != panel)
        dismiss();

    current = panel;
    current->onDismiss = [this] { dismiss(); };

    addAndMakeVisible (current);
    setVisible (true);
    toFront (false);
    resized();

    current->overlayShown();
    current->grabKeyboardFocus();
}

void OverlayHost::dismiss()
{
    if (current == nullptr)
    {
        setVisible (false);
        return;
    }

    auto* panel = current;
    current = nullptr;

    panel->overlayHidden();
    removeChildComponent (panel);

    setVisible (false);

    if (auto* parent = getParentComponent())
        parent->grabKeyboardFocus();
}

void OverlayHost::paint (juce::Graphics& g)
{
    // The scrim: dark enough to focus attention, light enough that the plugin is
    // still visibly there behind it.
    g.fillAll (juce::Colour (0xb0000000));
}

void OverlayHost::resized()
{
    if (current == nullptr)
        return;

    const auto preferred = current->getPreferredSize();

    const int w = juce::jmin (preferred.x, getWidth() - Metrics::windowPadding * 2);
    const int h = juce::jmin (preferred.y, getHeight() - Metrics::windowPadding * 2);

    current->setBounds (juce::Rectangle<int> (w, h).withCentre (getLocalBounds().getCentre()));
}

void OverlayHost::mouseDown (const juce::MouseEvent& e)
{
    // A click on the scrim, outside the panel, dismisses.
    if (current == nullptr || ! current->getBounds().contains (e.getPosition()))
        dismiss();
}

//==============================================================================
//  HelpPanel
//==============================================================================
namespace
{
    struct HelpSection { const char* title; const char* body; };

    const HelpSection kHelpSections[] =
    {
        { "Getting Started",
          "Luthier synthesises a guitar from physics. There are no samples anywhere in it:\n"
          "every note is a vibrating string model, coupled through a bridge, coloured by a\n"
          "body, sensed by a pickup and pushed through an amp, a cabinet and a room.\n"
          "\n"
          "The quickest way in:\n"
          "\n"
          "  1. Pick an instrument from the selector at the top left.\n"
          "  2. Pick a Style from the dropdown at the bottom of Easy mode.\n"
          "  3. Press AUDITION to hear it without touching a keyboard.\n"
          "  4. Play. Six macro knobs cover most of what you will want to change.\n"
          "\n"
          "When you want to go deeper, switch to Advanced with the button at the top right.\n"
          "Nothing is hidden there that is not also reachable from Easy mode - Advanced just\n"
          "stops summarising.\n"
          "\n"
          "Because the model is physical, techniques you never set up still work. Bend into a\n"
          "slide into a vibrato into a harmonic and it behaves correctly, because the engine\n"
          "knows what a string is rather than looking for a recording of that combination." },

        { "The Interface",
          "HEADER (always visible)\n"
          "  Output LED       top-left corner. Dark when silent, brightening to white as the\n"
          "                   level approaches 0 dBFS, red while the signal is over.\n"
          "  Instrument       loads a complete guitar: body, woods, pickups, strings, tuning\n"
          "                   and its usual amp and cabinet.\n"
          "  Tuning           open-string tuning. Per-string tunings live in Advanced.\n"
          "  Preset           name, with prev/next arrows. Click the name to browse.\n"
          "  File             save, open, import, export, options, randomise, reset.\n"
          "  A / B            two comparison slots. A>B copies the current one across.\n"
          "  Undo / Redo      64 steps.\n"
          "  Panic            stops every string immediately.\n"
          "  ?                this window.\n"
          "  Advanced         switches modes.\n"
          "\n"
          "EASY MODE\n"
          "  Top band         the instrument, drawn from the settings actually in use, and an\n"
          "                   interactive fretboard. Click a fret to hear that note; how high\n"
          "                   in the lane you click sets how hard it is picked. Right-click a\n"
          "                   string for mute, capo and scale overlays.\n"
          "  Middle band      six macros. Each has a dice (randomise just this one) and a\n"
          "                   padlock (exclude it from Randomise).\n"
          "  Bottom band      style, playing mode, the chord readout, AUDITION and export.\n"
          "\n"
          "ADVANCED MODE\n"
          "  Column 1         one row per string: pitch, computed tension, gauge, mute.\n"
          "                   Tension turns amber when it leaves the playable range.\n"
          "  Column 2         everything about the selected string, plus the neck and the\n"
          "                   temperament.\n"
          "  Column 3         body, pickups and the playing hand, including string noise.\n"
          "  Column 4         bridge, cable, both pedalboards, amp, cabinet, room,\n"
          "                   performance, humanisation and master." },

        { "Controls",
          "Every control behaves the same way:\n"
          "\n"
          "  Left-drag        adjust. Vertical or horizontal, whichever you start with.\n"
          "  Shift-drag       coarse.\n"
          "  Ctrl-drag        ultra-fine (Cmd on macOS).\n"
          "  Double-click     reset to default.\n"
          "  Hover            the value replaces the label; a tooltip follows after a moment.\n"
          "  Right-click      Enter value, Reset, Copy, Paste, MIDI Learn, Lock, Randomise.\n"
          "\n"
          "MIDI LEARN\n"
          "  Right-click a control, choose MIDI Learn, then move a knob or pedal on your\n"
          "  controller. A small teal dot appears on any control that has a CC mapped.\n"
          "  Right-click again to clear it. Sustain and sostenuto are skipped while learning,\n"
          "  so an accidental pedal press cannot steal the mapping.\n"
          "\n"
          "LOCKS\n"
          "  A locked control is left alone by Randomise. The padlock under the macro knobs\n"
          "  toggles it; so does the right-click menu, on any control." },

        { "Playing Techniques",
          "Technique is inferred from what you play, in this order:\n"
          "\n"
          "  Palm mute        CC 67. Drops the string's damping filter and shortens the decay.\n"
          "  Pinch harmonic   CC 72.\n"
          "  Natural harmonic CC 73. Land on a node (12th, 7th, 5th fret) and it chimes.\n"
          "  Tap              CC 74 (when MPE is off).\n"
          "  Slide guitar     CC 75, or the toggle in Advanced. Every note becomes a slide.\n"
          "  Muted picking    CC 71.\n"
          "  Slide            two notes on one string closer together than the legato window,\n"
          "                   or CC 65 held.\n"
          "  Hammer-on        a higher note on a ringing string, below the legato velocity.\n"
          "  Pull-off         the same, going down.\n"
          "  Pluck            everything else.\n"
          "\n"
          "  Pitch bend       bends the string by changing its tension.\n"
          "  Mod wheel        vibrato depth.\n"
          "  Aftertouch       vibrato depth (or bend - your choice, in Options).\n"
          "  Breath / CC 2    whammy bar.\n"
          "  Sustain          lets every string ring.\n"
          "  Sostenuto        holds only what was already down.\n"
          "\n"
          "A legato move never re-picks the string: the vibration carries through and only\n"
          "the pitch changes, which is what makes a slide sound like one note rather than two." },

        { "Playing Modes",
          "MONO / LEAD\n"
          "  Every note goes to one string, chosen to keep the hand near where it already is.\n"
          "  Overlapping notes become hammer-ons, pull-offs or slides. Best for solos.\n"
          "\n"
          "POLY / CHORD\n"
          "  Chords are voiced across the strings by a search that only returns fingerings a\n"
          "  hand could actually make: one note per string, a reachable fret span, and pitch\n"
          "  order following string order. If a voicing is impossible, the nearest playable\n"
          "  one is used rather than something absurd.\n"
          "  Chords are strummed, not triggered simultaneously - the strum speed and direction\n"
          "  are yours to set, and the strum varies slightly every time.\n"
          "  This mode carries a small latency (the chord window, 2 ms by default) so that a\n"
          "  chord split across a buffer boundary still voices as a chord. It is reported to\n"
          "  your host.\n"
          "\n"
          "GUITAR CONTROLLER\n"
          "  MIDI channel 1 is the high E, channel 2 the B, and so on. For hex pickups such as\n"
          "  the Roland GK or Fishman TriplePlay. Per-string bend and pressure work natively.\n"
          "  Turn MPE on in Advanced for expressive controllers (Seaboard, LinnStrument, Osmose)." },

        { "Presets",
          "Presets are plain JSON with the extension .luthierpreset, so they are readable,\n"
          "diffable and safe to keep in version control.\n"
          "\n"
          "WHERE THEY LIVE\n"
          "  Factory   beside the plugin in Resources/Presets/Factory, or, if the plugin\n"
          "            cannot write there, in Documents/Luthier/Presets/Factory.\n"
          "  User      Documents/Luthier/Presets/User\n"
          "  Renders   Documents/Luthier/Renders\n"
          "\n"
          "  Options has a button that opens each of these folders for you.\n"
          "\n"
          "IF PRESETS DO NOT APPEAR\n"
          "  1. Open Options and press Rescan presets.\n"
          "  2. Check the preset is in one of the folders listed there, and that its extension\n"
          "     is exactly .luthierpreset.\n"
          "  3. Presets in sub-folders are grouped by the sub-folder's name, which is where\n"
          "     their category comes from. A preset loose in the root gets the category User.\n"
          "  4. Add any other folder with Add a preset folder.\n"
          "\n"
          "A factory preset is never overwritten. Saving one makes a copy in your user folder,\n"
          "so you cannot lose the original." },

        { "Export",
          "AUDIO\n"
          "  File > Export audio, or the Export button in Easy mode. You can render the\n"
          "  audition phrase or a MIDI file you choose. Format, bit depth, sample rate, tail\n"
          "  length and normalisation are all yours to set.\n"
          "  The render runs on its own thread through a second, offline copy of the plugin\n"
          "  loaded with your exact settings, so what you get is what you heard - including\n"
          "  the reverb tail, which is why there is a tail-length control.\n"
          "  When it finishes you are told where the file went, how long it is and at what\n"
          "  quality.\n"
          "\n"
          "MIDI\n"
          "  Luthier keeps the last sixty seconds of MIDI you played. File > Save last MIDI\n"
          "  take writes it out, even though you never pressed record. Useful exactly when you\n"
          "  play something good by accident." },

        { "Troubleshooting",
          "THE PLUGIN DOES NOT APPEAR IN MY HOST\n"
          "  Windows VST3 lives in:\n"
          "      C:\\Program Files\\Common Files\\VST3\\Luthier.vst3\n"
          "  macOS VST3 and AU live in:\n"
          "      ~/Library/Audio/Plug-Ins/VST3/Luthier.vst3\n"
          "      ~/Library/Audio/Plug-Ins/Components/Luthier.component\n"
          "  Copy the bundle there by hand if the installer could not, then rescan in your\n"
          "  host. Most hosts cache the plugin list; some need the cache cleared as well.\n"
          "\n"
          "TO UNINSTALL BY HAND\n"
          "  Delete the bundle from the folder above. Your presets and renders live in\n"
          "  Documents/Luthier and are left alone, so remove that folder too if you want\n"
          "  everything gone.\n"
          "\n"
          "NO SOUND\n"
          "  - Check the track is receiving MIDI: the dot beside the logo lights up.\n"
          "  - Check the amp is not on Standby, and that Master is up.\n"
          "  - If every pickup is switched off, an electric guitar is silent by design. The\n"
          "    switch position is shown on the illustration.\n"
          "  - Press Panic, in case a note is stuck.\n"
          "\n"
          "CRACKLES OR HIGH CPU\n"
          "  - Lower Oversampling in Options. 2x is usually indistinguishable from 4x.\n"
          "  - Raise your host's buffer size.\n"
          "  - Switch the body from Convolution to Modal, or vice versa; which is cheaper\n"
          "    depends on your machine.\n"
          "\n"
          "IT CRASHES\n"
          "  Open Help > Debug, tick \"Create log file on crash\", reproduce the crash, then\n"
          "  send BOTH the crash log and an exported troubleshooting file to support, with a\n"
          "  description of what you were doing. Both are written to\n"
          "  Documents/Luthier/Diagnostics." },

        { "Keyboard Shortcuts",
          "  Escape           close the overlay that is open\n"
          "  Space            start or stop the audition phrase\n"
          "  Tab              switch between Easy and Advanced\n"
          "  Ctrl/Cmd + Z     undo\n"
          "  Ctrl/Cmd + Shift + Z, or Ctrl/Cmd + Y   redo\n"
          "  Ctrl/Cmd + S     save the current preset\n"
          "  Ctrl/Cmd + Shift + S   save as\n"
          "  Ctrl/Cmd + R     randomise\n"
          "  Ctrl/Cmd + E     export audio\n"
          "  Ctrl/Cmd + P     preset browser\n"
          "  Ctrl/Cmd + ,     options\n"
          "  Ctrl/Cmd + D     debug tools\n"
          "  F1               this window\n"
          "  [ and ]          previous and next preset\n"
          "  0                panic\n"
          "\n"
          "On any control:\n"
          "  Double-click     reset to default\n"
          "  Shift-drag       coarse\n"
          "  Ctrl/Cmd-drag    ultra-fine\n"
          "  Right-click      the full control menu" },

        { "About and Licence",
          "Luthier " JucePlugin_VersionString "\n"
          "A physically-modelled guitar. No samples.\n"
          "\n"
          "(c) Luthier Audio. All rights reserved.\n"
          "\n"
          "Built with JUCE (juce.com), used under the terms of the JUCE licence that applies\n"
          "to this build. JUCE is (c) Raw Material Software Limited.\n"
          "\n"
          "LICENCE\n"
          "  This copy of Luthier is licensed, not sold. You may install and use it on the\n"
          "  machines you personally work on. You may not redistribute the plugin binary, nor\n"
          "  reverse engineer it, except where that right cannot be excluded by law.\n"
          "  Presets you create are yours, and audio you render with it is yours, with no\n"
          "  further obligation.\n"
          "  The plugin is provided as-is, without warranty of any kind.\n"
          "\n"
          "Full third-party notices are in THIRD_PARTY_LICENCES.txt beside the plugin.\n"
          "\n"
          "LINKS\n"
          "  Homepage   https://luthieraudio.example/luthier\n"
          "  Source     https://github.com/luthieraudio/luthier\n"
          "  Support    support@luthieraudio.example\n"
          "\n"
          "The buttons below open these. If a link does not open, the addresses above can be\n"
          "copied by hand." }
    };

    constexpr int kNumHelpSections = (int) (sizeof (kHelpSections) / sizeof (kHelpSections[0]));
}

int HelpPanel::SectionListModel::getNumRows() { return kNumHelpSections; }

void HelpPanel::SectionListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                    int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, kNumHelpSections))
        return;

    if (selected)
    {
        g.setColour (Palette::accent.withAlpha (0.16f));
        g.fillRect (0, 0, width, height);

        g.setColour (Palette::accent);
        g.fillRect (0, 0, 2, height);
    }

    g.setColour (selected ? Palette::accent : Palette::textMuted);
    g.setFont (Fonts::ui (12.0f, selected));
    g.drawText (kHelpSections[row].title, 12, 0, width - 16, height,
                juce::Justification::centredLeft, true);
}

void HelpPanel::SectionListModel::selectedRowsChanged (int lastRow)
{
    owner.showSection (lastRow);
}

HelpPanel::HelpPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Help"), processor (p)
{
    addAndMakeVisible (sectionList);
    sectionList.setModel (&listModel);
    sectionList.setRowHeight (26);
    sectionList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);

    addAndMakeVisible (body);
    body.setMultiLine (true);
    body.setReadOnly (true);
    body.setScrollbarsShown (true);
    body.setCaretVisible (false);
    body.setFont (Fonts::mono (12.0f));
    body.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);

    addAndMakeVisible (debugButton);
    debugButton.setTooltip ("Live internals, crash logging and the destructive reset");
    debugButton.onClick = [this] { if (onOpenDebug) onOpenDebug(); };

    addAndMakeVisible (githubButton);
    githubButton.onClick = [] { juce::URL ("https://github.com/luthieraudio/luthier").launchInDefaultBrowser(); };

    addAndMakeVisible (homepageButton);
    homepageButton.onClick = [] { juce::URL ("https://luthieraudio.example/luthier").launchInDefaultBrowser(); };

    addAndMakeVisible (supportButton);
    supportButton.onClick = []
    {
        juce::URL ("mailto:support@luthieraudio.example?subject=Luthier%20"
                   JucePlugin_VersionString).launchInDefaultBrowser();
    };

    sectionList.selectRow (0);
    showSection (0);
}

void HelpPanel::showSection (int index)
{
    if (! juce::isPositiveAndBelow (index, kNumHelpSections))
        return;

    body.setText (kHelpSections[index].body, false);
    body.moveCaretToTop (false);
}

void HelpPanel::layoutContent (juce::Rectangle<int> content)
{
    auto footer = content.removeFromBottom (Metrics::buttonHeight);

    debugButton.setBounds (footer.removeFromLeft (150));
    footer.removeFromLeft (Metrics::grid);

    supportButton.setBounds (footer.removeFromRight (120));
    footer.removeFromRight (Metrics::gridHalf);
    homepageButton.setBounds (footer.removeFromRight (100));
    footer.removeFromRight (Metrics::gridHalf);
    githubButton.setBounds (footer.removeFromRight (90));

    content.removeFromBottom (Metrics::grid);

    sectionList.setBounds (content.removeFromLeft (188));
    content.removeFromLeft (Metrics::grid);
    body.setBounds (content);
}

//==============================================================================
//  DebugPanel
//==============================================================================
DebugPanel::DebugPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Debug Tools"), processor (p)
{
    addAndMakeVisible (explanation);
    explanation.setFont (Fonts::ui (11.5f));
    explanation.setColour (juce::Label::textColourId, Palette::textMuted);
    explanation.setJustificationType (juce::Justification::topLeft);
    explanation.setText (
        "Two different files, for two different jobs.\n\n"
        "The TROUBLESHOOTING FILE is a one-off snapshot: your settings, your audio and MIDI "
        "configuration, your host, the version, and a short self-test. It is what to send "
        "first for anything that is merely not working as expected. It is safe to share and "
        "contains no audio.\n\n"
        "The CRASH LOG is only useful if Luthier is actually crashing. Tick the box below, "
        "reproduce the crash, and a timestamped log is written with a copy of the "
        "troubleshooting report at the top. It is off every time the plugin loads, on purpose.\n\n"
        "If Luthier is hard-crashing, send BOTH files to support with a description of what "
        "you were doing. Both are written to Documents/Luthier/Diagnostics.",
        juce::dontSendNotification);

    addAndMakeVisible (crashLogToggle);
    crashLogToggle.setTooltip ("Off on every load. Turning it on also turns on the live data stream.");
    crashLogToggle.onClick = [this]
    {
        processor.getDiagnostics().setCrashLogEnabled (crashLogToggle.getToggleState());
        refreshState();
    };

    addAndMakeVisible (troubleshootButton);
    troubleshootButton.setTooltip ("Write a troubleshooting file you can send to support");
    troubleshootButton.onClick = [this]
    {
        processor.getPresetManager().captureExtraState();

        const auto file = processor.getDiagnostics().writeTroubleshootingReport (
            juce::JSON::toString (processor.getPresetManager().toVar(), false),
            processor.getEngine().getValidator().getSummary());

        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions()
                .withIconType (file != juce::File() ? juce::MessageBoxIconType::InfoIcon
                                                    : juce::MessageBoxIconType::WarningIcon)
                .withTitle (file != juce::File() ? "Troubleshooting file written" : "Could not write the file")
                .withMessage (file != juce::File()
                                ? "Written to\n" + file.getFullPathName()
                                  + "\n\nSend this to support@luthieraudio.example with a description "
                                    "of the problem."
                                : "The diagnostics folder could not be written to. Check the folder "
                                  "permissions for Documents/Luthier.")
                .withButton ("OK"),
            nullptr);
    };

    addAndMakeVisible (openFolderButton);
    openFolderButton.onClick = [] { Diagnostics::getDiagnosticsFolder().revealToUser(); };

    addAndMakeVisible (hardResetButton);
    hardResetButton.setColour (juce::TextButton::textColourOffId, Palette::clip);
    hardResetButton.setTooltip ("Destructive: resets every setting, clears the MIDI map, deletes "
                                "diagnostic files and cached data, and reinstalls the factory bank.");
    hardResetButton.onClick = [this]
    {
        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle ("Reset everything?")
                .withMessage ("This resets every setting, clears your MIDI mappings, deletes "
                              "diagnostic files and cached data, and reinstalls the factory "
                              "preset bank.\n\nYour own saved presets are NOT deleted.\n\n"
                              "This cannot be undone.")
                .withButton ("Reset everything")
                .withButton ("Cancel"),
            [this] (int result)
            {
                if (result == 1)
                {
                    processor.hardResetAndClearCaches();
                    refreshState();
                }
            });
    };

    addAndMakeVisible (clearButton);
    clearButton.onClick = [this]
    {
        processor.getDiagnostics().reset();
        streamView.clear();
        lastStreamCount = 0;
    };

    auto setupView = [this] (juce::TextEditor& view)
    {
        addAndMakeVisible (view);
        view.setMultiLine (true);
        view.setReadOnly (true);
        view.setScrollbarsShown (true);
        view.setCaretVisible (false);
        view.setFont (Fonts::mono (10.5f));
        view.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    };

    setupView (stateView);
    setupView (streamView);
}

DebugPanel::~DebugPanel()
{
    stopTimer();
}

void DebugPanel::overlayShown()
{
    processor.getDiagnostics().setEnabled (true);
    crashLogToggle.setToggleState (processor.getDiagnostics().isCrashLogEnabled(),
                                   juce::dontSendNotification);
    refreshState();
    startTimerHz (8);
}

void DebugPanel::overlayHidden()
{
    stopTimer();
}

void DebugPanel::refreshState()
{
    auto& engine = processor.getEngine();
    const auto& validator = engine.getValidator();

    juce::String text;

    text << "ENGINE\n"
         << "  Sample rate        " << juce::String (engine.getSampleRate(), 1) << " Hz\n"
         << "  Reported latency   " << processor.getLatencySamples() << " samples\n"
         << "  CPU (this plugin)  " << juce::String (engine.getCpuEstimate(), 1) << " %\n"
         << "  Oversampling       " << engine.getOversamplingFactor() << "x\n"
         << "  Host tempo         " << juce::String (processor.getHostTempo(), 1) << " BPM\n"
         << "\nINSTRUMENT\n"
         << "  Guitar             " << engine.getGuitarSpec().name << "\n"
         << "  Strings            " << engine.getNumStrings() << "\n"
         << "  Scale length       " << juce::String (engine.getGuitarSpec().scaleLengthMm, 1) << " mm\n"
         << "  Fretless           " << (engine.isFretless() ? "yes" : "no") << "\n"
         << "  Frozen             " << (engine.isFrozen() ? "yes" : "no") << "\n"
         << "\nSTRINGS\n";

    for (int s = 0; s < engine.getNumStrings(); ++s)
    {
        const auto& spec = engine.getStringSpec (s);

        text << "  " << juce::String (s + 1).paddedLeft (' ', 2) << "  "
             << TuningEngine::describeFrequency (engine.getStringFrequency (s)).paddedRight (' ', 12)
             << "fret " << juce::String (engine.getStringFret (s), 2).paddedLeft (' ', 6) << "   "
             << "level " << juce::String (engine.getStringLevel (s), 5).paddedLeft (' ', 8) << "   "
             << "tension " << juce::String (spec.tensionNewtons, 1).paddedLeft (' ', 6) << " N"
             << (StringMaterials::isTensionPlayable (spec.tensionNewtons) ? "" : "  <-- out of range")
             << "\n";
    }

    text << "\nSIGNAL\n"
         << "  Peak               " << juce::String (engine.getMasterBus().getPeakDb(), 2) << " dBFS\n"
         << "  LUFS (short)       " << juce::String (engine.getMasterBus().getLufs(), 1) << "\n"
         << "  Limiter reduction  " << juce::String (engine.getMasterBus().getGainReductionDb(), 2) << " dB\n"
         << "  Body mode          " << (int) engine.getBodyEngine().getMode()
         << "  (" << engine.getBodyEngine().getNumModes() << " modes)\n"
         << "  Pickups silent     " << (engine.getPickupEngine().isSilent() ? "YES" : "no") << "\n"
         << "\nVALIDATOR\n  " << validator.getSummary() << "\n";

    ValidationRecord records[12];
    const int count = validator.getRecentRecords (records, 12);

    for (int i = 0; i < count; ++i)
    {
        text << "  " << juce::String (getValidationCheckName (records[i].check)).paddedRight (' ', 20)
             << " string " << records[i].stringIndex
             << "  " << juce::String (records[i].offendingValue, 4)
             << " -> " << juce::String (records[i].correctedValue, 4)
             << (records[i].rejected ? "   (rejected)" : "") << "\n";
    }

    const auto selfTest = Diagnostics::runSelfTest();

    text << "\nSELF TEST\n";

    for (const auto& line : selfTest.passed)   text << "  [ok]      " << line << "\n";
    for (const auto& line : selfTest.warnings) text << "  [warning] " << line << "\n";
    for (const auto& line : selfTest.failed)   text << "  [FAILED]  " << line << "\n";

    const auto caret = stateView.getCaretPosition();
    stateView.setText (text, false);
    stateView.setCaretPosition (caret);
}

void DebugPanel::timerCallback()
{
    refreshState();

    auto& diagnostics = processor.getDiagnostics();
    const int total = diagnostics.getTotalRecords();

    if (total == lastStreamCount)
        return;

    const int newRecords = juce::jmin (total - lastStreamCount, Diagnostics::kRingSize);
    lastStreamCount = total;

    std::vector<Diagnostics::Record> records ((size_t) juce::jmax (1, newRecords));
    const int count = diagnostics.getRecords (records.data(), newRecords);

    juce::String appended;

    for (int i = 0; i < count; ++i)
        appended << Diagnostics::formatRecord (records[(size_t) i], processor.getEngine().getSampleRate()) << "\n";

    streamView.moveCaretToEnd (false);
    streamView.insertTextAtCaret (appended);

    // Keep the view bounded, or a long session eats memory in the UI.
    if (streamView.getTotalNumChars() > 200000)
        streamView.setText (streamView.getText().fromLastOccurrenceOf ("\n", false, false)
                                                .paddedLeft (' ', 0), false);
}

void DebugPanel::layoutContent (juce::Rectangle<int> content)
{
    auto left = content.removeFromLeft (content.getWidth() / 2);
    content.removeFromLeft (Metrics::grid);

    auto buttons = left.removeFromBottom (Metrics::buttonHeight * 2 + Metrics::gridHalf);

    auto topRow = buttons.removeFromTop (Metrics::buttonHeight);
    troubleshootButton.setBounds (topRow.removeFromLeft (200));
    topRow.removeFromLeft (Metrics::gridHalf);
    openFolderButton.setBounds (topRow.removeFromLeft (170));

    buttons.removeFromTop (Metrics::gridHalf);
    hardResetButton.setBounds (buttons.removeFromLeft (300));

    left.removeFromBottom (Metrics::grid);
    crashLogToggle.setBounds (left.removeFromBottom (Metrics::buttonHeight));
    left.removeFromBottom (Metrics::gridHalf);

    explanation.setBounds (left.removeFromBottom (juce::jmin (190, left.getHeight() / 2)));
    left.removeFromBottom (Metrics::grid);

    stateView.setBounds (left);

    auto streamHeader = content.removeFromBottom (Metrics::buttonHeight);
    clearButton.setBounds (streamHeader.removeFromLeft (120));

    content.removeFromBottom (Metrics::gridHalf);
    streamView.setBounds (content);
}

//==============================================================================
//  OptionsPanel
//==============================================================================
int OptionsPanel::FolderListModel::getNumRows()
{
    return owner.processor.getPresetManager().getSearchFolders().size();
}

void OptionsPanel::FolderListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                      int width, int height, bool selected)
{
    const auto folders = owner.processor.getPresetManager().getSearchFolders();

    if (! juce::isPositiveAndBelow (row, folders.size()))
        return;

    if (selected)
    {
        g.setColour (Palette::accent.withAlpha (0.14f));
        g.fillRect (0, 0, width, height);
    }

    g.setColour (Palette::textMuted);
    g.setFont (Fonts::mono (11.0f));
    g.drawText (folders[row].getFullPathName(), 8, 0, width - 12, height,
                juce::Justification::centredLeft, true);
}

OptionsPanel::OptionsPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Options"), processor (p)
{
    addAndMakeVisible (tooltipsToggle);
    tooltipsToggle.setTooltip ("Turn the hover tooltips on or off");
    tooltipsToggle.onClick = [this]
    {
        processor.getUiState().tooltipsEnabled = tooltipsToggle.getToggleState();
    };

    addAndMakeVisible (driftToggle);
    driftToggle.onClick = [this]
    {
        if (auto* param = processor.getState().getParameter (ParamIDs::tuningDrift))
            param->setValueNotifyingHost (driftToggle.getToggleState() ? 1.0f : 0.0f);
    };

    addAndMakeVisible (oversampling);
    oversampling.attachTo (processor, ParamIDs::oversample,
                           "Oversampling for the amp and the drive pedals. 4x is the default; "
                           "2x sounds very close and costs noticeably less.");

    addAndMakeVisible (chordWindow);
    chordWindow.attachTo (processor, ParamIDs::chordWindow,
                          "How long Poly mode waits to collect a chord. Longer catches chords "
                          "split across buffers; shorter has less latency.");

    addAndMakeVisible (openUserFolder);
    openUserFolder.onClick = [] { PresetManager::getUserPresetFolder().revealToUser(); };

    addAndMakeVisible (openRenderFolder);
    openRenderFolder.onClick = [] { PresetManager::getRenderFolder().revealToUser(); };

    addAndMakeVisible (openFactoryFolder);
    openFactoryFolder.onClick = [] { PresetManager::getFactoryPresetFolder().revealToUser(); };

    addAndMakeVisible (addFolderButton);
    addFolderButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Choose a folder to scan for presets",
                                                            PresetManager::getUserPresetFolder());

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories,
                              [this, chooser] (const juce::FileChooser& fc)
        {
            const auto folder = fc.getResult();

            if (folder.isDirectory())
            {
                processor.getPresetManager().addSearchFolder (folder);
                folderList.updateContent();
            }
        });
    };

    addAndMakeVisible (rescanButton);
    rescanButton.onClick = [this]
    {
        processor.getPresetManager().refresh();
        folderList.updateContent();
    };

    addAndMakeVisible (audioSettingsButton);
    audioSettingsButton.setTooltip ("Standalone only: choose the audio device and MIDI inputs");
    audioSettingsButton.onClick = [this]
    {
        // In a plugin the host owns the devices; in the standalone build the
        // wrapper does. Either way this plugin does not, so the honest thing is to
        // say where the setting actually lives rather than offer a dead button.
        const bool standalone =
            (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::InfoIcon)
                .withTitle (standalone ? "Audio and MIDI settings"
                                       : "Audio and MIDI are handled by your host")
                .withMessage (standalone
                                ? "Use the Options button in the standalone window's own toolbar "
                                  "to choose the audio device, the sample rate, the buffer size "
                                  "and which MIDI inputs are active.\n\nThose settings belong to "
                                  "the wrapper rather than to the plugin, so they are remembered "
                                  "separately from your presets."
                                : "When Luthier runs as a plugin, your host chooses the audio "
                                  "device, the sample rate, the buffer size and which MIDI inputs "
                                  "reach the track.\n\nChange them in your host's audio "
                                  "preferences. The standalone version has its own device "
                                  "settings in its toolbar.")
                .withButton ("OK"),
            nullptr);
    };

    addAndMakeVisible (presetPathLabel);
    presetPathLabel.setFont (Fonts::ui (11.0f));
    presetPathLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    presetPathLabel.setJustificationType (juce::Justification::topLeft);

    addAndMakeVisible (audioNote);
    audioNote.setFont (Fonts::ui (11.0f));
    audioNote.setColour (juce::Label::textColourId, Palette::textMuted);
    audioNote.setJustificationType (juce::Justification::topLeft);
    audioNote.setText ("Presets are plain JSON files with the extension .luthierpreset. The "
                       "folder a preset sits in becomes its category. If a preset does not "
                       "appear, press Rescan; if it still does not, check that it is in one of "
                       "the folders listed here and that its extension is exactly right.",
                       juce::dontSendNotification);

    addAndMakeVisible (folderList);
    folderList.setModel (&folderModel);
    folderList.setRowHeight (22);
    folderList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
}

void OptionsPanel::overlayShown()
{
    tooltipsToggle.setToggleState (processor.getUiState().tooltipsEnabled, juce::dontSendNotification);

    if (auto* param = processor.getState().getParameter (ParamIDs::tuningDrift))
        driftToggle.setToggleState (param->getValue() > 0.5f, juce::dontSendNotification);

    presetPathLabel.setText (
        "User presets   " + PresetManager::getUserPresetFolder().getFullPathName() + "\n"
        "Factory        " + PresetManager::getFactoryPresetFolder().getFullPathName() + "\n"
        "Renders        " + PresetManager::getRenderFolder().getFullPathName() + "\n"
        "Diagnostics    " + Diagnostics::getDiagnosticsFolder().getFullPathName(),
        juce::dontSendNotification);

    folderList.updateContent();
}

void OptionsPanel::layoutContent (juce::Rectangle<int> content)
{
    auto row = content.removeFromTop (Metrics::buttonHeight);
    tooltipsToggle.setBounds (row.removeFromLeft (240));
    driftToggle.setBounds (row.removeFromLeft (280));

    content.removeFromTop (Metrics::grid);

    auto controlRow = content.removeFromTop (LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));
    oversampling.setBounds (controlRow.removeFromLeft (200).withHeight (40));
    controlRow.removeFromLeft (Metrics::grid);
    chordWindow.setBounds (controlRow.removeFromLeft (LuthierKnob::preferredWidthFor (LuthierKnob::Size::Normal)));

    controlRow.removeFromLeft (Metrics::grid);
    audioSettingsButton.setBounds (controlRow.removeFromLeft (200).withHeight (Metrics::buttonHeight));

    content.removeFromTop (Metrics::grid);

    presetPathLabel.setBounds (content.removeFromTop (72));
    content.removeFromTop (Metrics::gridHalf);

    auto folderButtons = content.removeFromTop (Metrics::buttonHeight);
    openUserFolder.setBounds (folderButtons.removeFromLeft (170));
    folderButtons.removeFromLeft (Metrics::gridHalf);
    openFactoryFolder.setBounds (folderButtons.removeFromLeft (180));
    folderButtons.removeFromLeft (Metrics::gridHalf);
    openRenderFolder.setBounds (folderButtons.removeFromLeft (150));

    content.removeFromTop (Metrics::gridHalf);

    auto scanButtons = content.removeFromTop (Metrics::buttonHeight);
    addFolderButton.setBounds (scanButtons.removeFromLeft (180));
    scanButtons.removeFromLeft (Metrics::gridHalf);
    rescanButton.setBounds (scanButtons.removeFromLeft (140));

    content.removeFromTop (Metrics::grid);

    audioNote.setBounds (content.removeFromBottom (64));
    content.removeFromBottom (Metrics::gridHalf);

    folderList.setBounds (content);
}

//==============================================================================
//  ExportPanel
//==============================================================================
ExportPanel::ExportPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Export Audio"), processor (p)
{
    destinationFolder = PresetManager::getRenderFolder();

    auto addLabelled = [this] (juce::Component& c, const juce::String& tooltip)
    {
        addAndMakeVisible (c);

        if (auto* combo = dynamic_cast<juce::ComboBox*> (&c))
            combo->setTooltip (tooltip);
    };

    addLabelled (sourceBox, "What to render");
    sourceBox.addItem ("Audition phrase", 1);
    sourceBox.addItem ("MIDI file...", 2);
    sourceBox.setSelectedId (1);
    sourceBox.onChange = [this]
    {
        if (sourceBox.getSelectedId() != 2)
        {
            updateEstimate();
            return;
        }

        auto chooser = std::make_shared<juce::FileChooser> ("Choose a MIDI file to render",
                                                             PresetManager::getRenderFolder(), "*.mid;*.midi");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                              [this, chooser] (const juce::FileChooser& fc)
        {
            importedMidiFile = fc.getResult();

            if (importedMidiFile == juce::File())
                sourceBox.setSelectedId (1, juce::dontSendNotification);

            updateEstimate();
        });
    };

    addLabelled (phraseBox, "Which phrase to render");

    for (int i = 0; i < (int) AuditionPhrase::Type::NumTypes; ++i)
        phraseBox.addItem (AuditionPhrase::getName ((AuditionPhrase::Type) i), i + 1);

    phraseBox.setSelectedItemIndex (0);
    phraseBox.onChange = [this] { updateEstimate(); };

    addLabelled (formatBox, "File format");
    formatBox.addItem ("WAV", 1);
    formatBox.addItem ("AIFF", 2);
    formatBox.addItem ("FLAC", 3);
    formatBox.setSelectedId (1);
    formatBox.onChange = [this]
    {
        const auto format = (AudioExporter::Format) (formatBox.getSelectedId() - 1);
        const auto depths = AudioExporter::getSupportedBitDepths (format);

        const int previous = bitDepthBox.getSelectedId();
        bitDepthBox.clear (juce::dontSendNotification);

        for (int d : depths)
            bitDepthBox.addItem (juce::String (d) + "-bit", d);

        bitDepthBox.setSelectedId (depths.contains (previous) ? previous : depths.getLast(),
                                   juce::dontSendNotification);
        updateEstimate();
    };

    addLabelled (bitDepthBox, "Bit depth");
    bitDepthBox.addItem ("16-bit", 16);
    bitDepthBox.addItem ("24-bit", 24);
    bitDepthBox.addItem ("32-bit", 32);
    bitDepthBox.setSelectedId (24);
    bitDepthBox.onChange = [this] { updateEstimate(); };

    addLabelled (sampleRateBox, "Sample rate");

    for (int rate : { 44100, 48000, 88200, 96000, 176400, 192000 })
        sampleRateBox.addItem (juce::String (rate / 1000.0, 1) + " kHz", rate);

    sampleRateBox.setSelectedId (48000);
    sampleRateBox.onChange = [this] { updateEstimate(); };

    addAndMakeVisible (tailSlider);
    tailSlider.setRange (0.0, 20.0, 0.1);
    tailSlider.setValue (4.0);
    tailSlider.setTextValueSuffix (" s tail");
    tailSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tailSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 20);
    tailSlider.setTooltip ("Extra time recorded after the last note, so the reverb and the "
                           "ringing strings are not cut off");
    tailSlider.onValueChange = [this] { updateEstimate(); };

    addAndMakeVisible (normaliseToggle);
    normaliseToggle.setTooltip ("Scale the whole render so its loudest peak hits the target");
    normaliseToggle.onClick = [this] { normaliseTarget.setEnabled (normaliseToggle.getToggleState()); };

    addAndMakeVisible (normaliseTarget);
    normaliseTarget.setRange (-12.0, 0.0, 0.1);
    normaliseTarget.setValue (-1.0);
    normaliseTarget.setTextValueSuffix (" dBFS");
    normaliseTarget.setSliderStyle (juce::Slider::LinearHorizontal);
    normaliseTarget.setTextBoxStyle (juce::Slider::TextBoxRight, false, 90, 20);
    normaliseTarget.setEnabled (false);

    addAndMakeVisible (fileNameEditor);
    fileNameEditor.setText ("Luthier Render", false);
    fileNameEditor.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);

    addAndMakeVisible (chooseFolderButton);
    chooseFolderButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Choose where to save the render",
                                                             destinationFolder);

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories,
                              [this, chooser] (const juce::FileChooser& fc)
        {
            if (fc.getResult().isDirectory())
            {
                destinationFolder = fc.getResult();
                updateEstimate();
            }
        });
    };

    addAndMakeVisible (exportButton);
    exportButton.setColour (juce::TextButton::textColourOffId, Palette::accent);
    exportButton.onClick = [this] { startExport(); };

    addAndMakeVisible (cancelButton);
    cancelButton.setEnabled (false);
    cancelButton.onClick = [this] { processor.getExporter().cancelExport(); };

    addAndMakeVisible (statusLabel);
    statusLabel.setFont (Fonts::ui (11.5f));
    statusLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    statusLabel.setJustificationType (juce::Justification::topLeft);

    addAndMakeVisible (estimateLabel);
    estimateLabel.setFont (Fonts::mono (11.0f));
    estimateLabel.setColour (juce::Label::textColourId, Palette::textMuted);

    addAndMakeVisible (progressBar);
    progressBar.setVisible (false);

    startTimerHz (10);
}

ExportPanel::~ExportPanel()
{
    stopTimer();
}

void ExportPanel::overlayShown()
{
    phraseBox.setSelectedItemIndex ((int) processor.getUiState().auditionType,
                                    juce::dontSendNotification);
    updateEstimate();
}

void ExportPanel::updateEstimate()
{
    const double rate = (double) juce::jmax (8000, sampleRateBox.getSelectedId());
    const int depth = juce::jmax (16, bitDepthBox.getSelectedId());

    double musicSeconds = 4.0;

    if (sourceBox.getSelectedId() == 2 && importedMidiFile.existsAsFile())
    {
        juce::FileInputStream stream (importedMidiFile);
        juce::MidiFile file;

        if (stream.openedOk() && file.readFrom (stream))
        {
            file.convertTimestampTicksToSeconds();
            musicSeconds = juce::jmax (0.5, file.getLastTimestamp());
        }
    }
    else
    {
        musicSeconds = AuditionPhrase::getDurationSeconds (
            (AuditionPhrase::Type) juce::jmax (0, phraseBox.getSelectedItemIndex()),
            processor.getHostTempo());
    }

    const double total = musicSeconds + tailSlider.getValue();
    const double bytes = total * rate * (depth / 8.0) * 2.0;

    estimateLabel.setText (juce::String (total, 1) + " s   ~"
                           + juce::File::descriptionOfSizeInBytes ((juce::int64) bytes)
                           + "   ->   " + destinationFolder.getFullPathName(),
                           juce::dontSendNotification);
}

void ExportPanel::startExport()
{
    if (processor.getExporter().isExporting())
        return;

    AudioExporter::Options options;
    options.format = (AudioExporter::Format) juce::jmax (0, formatBox.getSelectedId() - 1);
    options.bitDepth = juce::jmax (16, bitDepthBox.getSelectedId());
    options.sampleRate = (double) juce::jmax (8000, sampleRateBox.getSelectedId());
    options.tailSeconds = tailSlider.getValue();
    options.normalise = normaliseToggle.getToggleState();
    options.normaliseTargetDb = normaliseTarget.getValue();
    options.tempoBpm = processor.getHostTempo();
    options.numChannels = 2;

    auto name = fileNameEditor.getText().trim();

    if (name.isEmpty())
        name = "Luthier Render";

    options.outputFile = destinationFolder.getNonexistentChildFile (
        juce::File::createLegalFileName (name),
        AudioExporter::getExtension (options.format));

    // ---- what to render -------------------------------------------------------
    juce::MidiMessageSequence sequence;

    if (sourceBox.getSelectedId() == 2 && importedMidiFile.existsAsFile())
    {
        juce::FileInputStream stream (importedMidiFile);
        juce::MidiFile file;

        if (stream.openedOk() && file.readFrom (stream))
        {
            file.convertTimestampTicksToSeconds();

            for (int t = 0; t < file.getNumTracks(); ++t)
                sequence.addSequence (*file.getTrack (t), 0.0);

            sequence.updateMatchedPairs();
            sequence.sort();
        }
    }

    if (sequence.getNumEvents() == 0)
        sequence = AuditionPhrase::build (
            (AuditionPhrase::Type) juce::jmax (0, phraseBox.getSelectedItemIndex()),
            processor.getHostTempo());

    if (sequence.getNumEvents() == 0)
    {
        statusLabel.setText ("There is nothing to render.", juce::dontSendNotification);
        return;
    }

    const auto state = processor.captureStateBlock();

    statusLabel.setText ("Rendering...", juce::dontSendNotification);
    progressBar.setVisible (true);
    exportButton.setEnabled (false);
    cancelButton.setEnabled (true);

    processor.getExporter().startExport (
        options, sequence, state,
        [] { return LuthierAudioProcessor::createOfflineInstance(); },
        [this] (const AudioExporter::Result& result)
        {
            progressBar.setVisible (false);
            exportButton.setEnabled (true);
            cancelButton.setEnabled (false);

            statusLabel.setText (result.message, juce::dontSendNotification);

            // On success the user is told everything the brief asks for: that it
            // worked, where it went, how long it is and at what quality.
            juce::NativeMessageBox::showAsync (
                juce::MessageBoxOptions()
                    .withIconType (result.success ? juce::MessageBoxIconType::InfoIcon
                                                  : juce::MessageBoxIconType::WarningIcon)
                    .withTitle (result.success ? "Export finished" : "Export failed")
                    .withMessage (result.success
                                    ? "Saved\n  " + result.file.getFileName()
                                      + "\n\nLocation\n  " + result.file.getParentDirectory().getFullPathName()
                                      + "\n\nLength\n  " + juce::String (result.lengthSeconds, 2) + " seconds"
                                      + "\n\nQuality\n  " + result.qualityDescription
                                      + "\n\nPeak\n  " + juce::String (result.peakDb, 2) + " dBFS"
                                    : result.message)
                    .withButton ("OK"),
                nullptr);
        });
}

void ExportPanel::timerCallback()
{
    progress = processor.getExporter().getProgress();

    if (processor.getExporter().isExporting())
        statusLabel.setText ("Rendering... " + juce::String (juce::roundToInt (progress * 100.0)) + "%",
                             juce::dontSendNotification);
}

void ExportPanel::layoutContent (juce::Rectangle<int> content)
{
    auto row = [&content] (int height)
    {
        auto r = content.removeFromTop (height);
        content.removeFromTop (Metrics::gridHalf);
        return r;
    };

    auto sourceRow = row (26);
    sourceBox.setBounds (sourceRow.removeFromLeft (170));
    sourceRow.removeFromLeft (Metrics::gridHalf);
    phraseBox.setBounds (sourceRow);

    auto formatRow = row (26);
    formatBox.setBounds (formatRow.removeFromLeft (110));
    formatRow.removeFromLeft (Metrics::gridHalf);
    bitDepthBox.setBounds (formatRow.removeFromLeft (110));
    formatRow.removeFromLeft (Metrics::gridHalf);
    sampleRateBox.setBounds (formatRow.removeFromLeft (120));

    tailSlider.setBounds (row (24));

    auto normRow = row (24);
    normaliseToggle.setBounds (normRow.removeFromLeft (110));
    normaliseTarget.setBounds (normRow);

    auto fileRow = row (26);
    chooseFolderButton.setBounds (fileRow.removeFromRight (100));
    fileRow.removeFromRight (Metrics::gridHalf);
    fileNameEditor.setBounds (fileRow);

    estimateLabel.setBounds (row (20));

    auto buttons = content.removeFromBottom (Metrics::buttonHeight);
    exportButton.setBounds (buttons.removeFromRight (110));
    buttons.removeFromRight (Metrics::gridHalf);
    cancelButton.setBounds (buttons.removeFromRight (100));

    content.removeFromBottom (Metrics::gridHalf);
    progressBar.setBounds (content.removeFromBottom (18));
    content.removeFromBottom (Metrics::gridHalf);

    statusLabel.setBounds (content);
}

//==============================================================================
//  PresetBrowserPanel
//==============================================================================
int PresetBrowserPanel::PresetListModel::getNumRows()
{
    return owner.visibleIndices.size();
}

void PresetBrowserPanel::PresetListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                            int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, owner.visibleIndices.size()))
        return;

    const auto* info = owner.processor.getPresetManager().getPreset (owner.visibleIndices[row]);

    if (info == nullptr)
        return;

    if (selected)
    {
        g.setColour (Palette::accent.withAlpha (0.16f));
        g.fillRect (0, 0, width, height);

        g.setColour (Palette::accent);
        g.fillRect (0, 0, 2, height);
    }

    g.setColour (selected ? Palette::accent : Palette::textPrimary);
    g.setFont (Fonts::ui (12.5f, selected));
    g.drawText (info->name, 12, 0, width - 110, height, juce::Justification::centredLeft, true);

    g.setColour (info->isFactory ? Palette::textDisabled : Palette::secondary);
    g.setFont (Fonts::ui (10.0f));
    g.drawText (info->isFactory ? info->category : info->category + "  (user)",
                width - 106, 0, 98, height, juce::Justification::centredRight, true);
}

void PresetBrowserPanel::PresetListModel::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    owner.loadSelected();
}

void PresetBrowserPanel::PresetListModel::selectedRowsChanged (int lastRow)
{
    if (! juce::isPositiveAndBelow (lastRow, owner.visibleIndices.size()))
    {
        owner.description.setText ({}, juce::dontSendNotification);
        return;
    }

    const auto* info = owner.processor.getPresetManager().getPreset (owner.visibleIndices[lastRow]);

    if (info == nullptr)
        return;

    juce::String text = info->description;

    if (info->tags.size() > 0)
        text += (text.isEmpty() ? "" : "\n\n") + juce::String ("Tags: ") + info->tags.joinIntoString (", ");

    text += juce::String (text.isEmpty() ? "" : "\n\n") + info->file.getFullPathName();

    owner.description.setText (text, juce::dontSendNotification);
    owner.deleteButton.setEnabled (! info->isFactory);
}

PresetBrowserPanel::PresetBrowserPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Presets"), processor (p)
{
    addAndMakeVisible (searchBox);
    searchBox.setTextToShowWhenEmpty ("Search by name or tag...", Palette::textDisabled);
    searchBox.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    searchBox.onTextChange = [this] { rebuildList(); };

    addAndMakeVisible (categoryBox);
    categoryBox.onChange = [this] { rebuildList(); };

    addAndMakeVisible (list);
    list.setModel (&listModel);
    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);

    addAndMakeVisible (description);
    description.setFont (Fonts::ui (11.5f));
    description.setColour (juce::Label::textColourId, Palette::textMuted);
    description.setJustificationType (juce::Justification::topLeft);

    addAndMakeVisible (loadButton);
    loadButton.setColour (juce::TextButton::textColourOffId, Palette::accent);
    loadButton.onClick = [this] { loadSelected(); };

    addAndMakeVisible (deleteButton);
    deleteButton.setColour (juce::TextButton::textColourOffId, Palette::clip);
    deleteButton.setEnabled (false);
    deleteButton.onClick = [this]
    {
        const int row = list.getSelectedRow();

        if (! juce::isPositiveAndBelow (row, visibleIndices.size()))
            return;

        const int index = visibleIndices[row];
        const auto* info = processor.getPresetManager().getPreset (index);

        if (info == nullptr || info->isFactory)
            return;

        const auto name = info->name;

        juce::NativeMessageBox::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle ("Delete this preset?")
                .withMessage ("\"" + name + "\" will be deleted from disk. This cannot be undone.")
                .withButton ("Delete")
                .withButton ("Cancel"),
            [this, index] (int result)
            {
                if (result == 1)
                {
                    processor.getPresetManager().deletePreset (index);
                    rebuildList();
                }
            });
    };

    addAndMakeVisible (saveAsButton);
    saveAsButton.onClick = [this]
    {
        // Handing straight over to Save As keeps the one-overlay-at-a-time rule.
        if (saveAsPanelRequested)
            saveAsPanelRequested();
        else if (onDismiss)
            onDismiss();
    };

    processor.getPresetManager().addChangeListener (this);
}

PresetBrowserPanel::~PresetBrowserPanel()
{
    processor.getPresetManager().removeChangeListener (this);
}

void PresetBrowserPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuildList();
}

void PresetBrowserPanel::overlayShown()
{
    auto& manager = processor.getPresetManager();

    const auto previous = categoryBox.getText();

    categoryBox.clear (juce::dontSendNotification);
    categoryBox.addItem ("All categories", 1);

    int id = 2;

    for (const auto& category : manager.getCategories())
        categoryBox.addItem (category, id++);

    categoryBox.setText (previous.isNotEmpty() ? previous : "All categories",
                         juce::dontSendNotification);

    if (categoryBox.getSelectedId() == 0)
        categoryBox.setSelectedId (1, juce::dontSendNotification);

    rebuildList();
}

void PresetBrowserPanel::rebuildList()
{
    auto& manager = processor.getPresetManager();

    const auto query = searchBox.getText().trim().toLowerCase();
    const auto category = (categoryBox.getSelectedId() <= 1) ? juce::String()
                                                             : categoryBox.getText();

    visibleIndices.clear();

    for (int i = 0; i < manager.getNumPresets(); ++i)
    {
        const auto* info = manager.getPreset (i);

        if (info == nullptr)
            continue;

        if (category.isNotEmpty() && info->category != category)
            continue;

        if (query.isNotEmpty())
        {
            const bool matches = info->name.toLowerCase().contains (query)
                                 || info->description.toLowerCase().contains (query)
                                 || info->tags.joinIntoString (" ").toLowerCase().contains (query);

            if (! matches)
                continue;
        }

        visibleIndices.add (i);
    }

    list.updateContent();
    list.repaint();

    const int current = manager.getCurrentPresetIndex();
    const int row = visibleIndices.indexOf (current);

    if (row >= 0)
        list.selectRow (row, false, true);
}

void PresetBrowserPanel::loadSelected()
{
    const int row = list.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, visibleIndices.size()))
        return;

    processor.pushUndoState ("Load preset");
    processor.getPresetManager().loadPreset (visibleIndices[row]);
    processor.getParameterBridge().applyAllNow();
}

void PresetBrowserPanel::layoutContent (juce::Rectangle<int> content)
{
    auto top = content.removeFromTop (26);
    categoryBox.setBounds (top.removeFromRight (180));
    top.removeFromRight (Metrics::gridHalf);
    searchBox.setBounds (top);

    content.removeFromTop (Metrics::grid);

    auto buttons = content.removeFromBottom (Metrics::buttonHeight);
    loadButton.setBounds (buttons.removeFromRight (100));
    buttons.removeFromRight (Metrics::gridHalf);
    deleteButton.setBounds (buttons.removeFromRight (100));
    buttons.removeFromLeft (0);
    saveAsButton.setBounds (buttons.removeFromLeft (120));

    content.removeFromBottom (Metrics::grid);

    description.setBounds (content.removeFromBottom (80));
    content.removeFromBottom (Metrics::gridHalf);

    list.setBounds (content);
}

//==============================================================================
//  SaveAsPanel
//==============================================================================
SaveAsPanel::SaveAsPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Save Preset As"), processor (p)
{
    auto setupEditor = [this] (juce::TextEditor& e, const juce::String& placeholder, bool multiline)
    {
        addAndMakeVisible (e);
        e.setMultiLine (multiline);
        e.setReturnKeyStartsNewLine (multiline);
        e.setTextToShowWhenEmpty (placeholder, Palette::textDisabled);
        e.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    };

    setupEditor (nameEditor, "Preset name", false);
    setupEditor (descriptionEditor, "What this sound is for (optional)", true);
    setupEditor (tagsEditor, "Tags, separated by commas (optional)", false);

    addAndMakeVisible (categoryBox);
    categoryBox.setEditableText (true);
    categoryBox.setTooltip ("The folder the preset is saved into, and the category it is "
                            "listed under. Type a new one to create it.");

    addAndMakeVisible (saveButton);
    saveButton.setColour (juce::TextButton::textColourOffId, Palette::accent);
    saveButton.onClick = [this]
    {
        const auto name = nameEditor.getText().trim();

        if (name.isEmpty())
        {
            status.setText ("Give the preset a name first.", juce::dontSendNotification);
            return;
        }

        auto category = categoryBox.getText().trim();

        if (category.isEmpty())
            category = "User";

        const auto tags = juce::StringArray::fromTokens (tagsEditor.getText(), ",", "");

        if (processor.getPresetManager().saveAs (name, category, descriptionEditor.getText(), tags))
        {
            status.setText ("Saved.", juce::dontSendNotification);

            if (onDismiss)
                onDismiss();
        }
        else
        {
            status.setText ("Could not save. Check that Documents/Luthier/Presets/User is writable.",
                            juce::dontSendNotification);
        }
    };

    addAndMakeVisible (status);
    status.setFont (Fonts::ui (11.5f));
    status.setColour (juce::Label::textColourId, Palette::warning);
}

void SaveAsPanel::overlayShown()
{
    auto& manager = processor.getPresetManager();

    nameEditor.setText (manager.getCurrentPresetName(), false);
    nameEditor.selectAll();
    nameEditor.grabKeyboardFocus();

    categoryBox.clear (juce::dontSendNotification);

    int id = 1;
    for (const auto& category : manager.getCategories())
        categoryBox.addItem (category, id++);

    if (id == 1)
        categoryBox.addItem ("User", 1);

    categoryBox.setText ("User", juce::dontSendNotification);
    status.setText ({}, juce::dontSendNotification);
}

void SaveAsPanel::layoutContent (juce::Rectangle<int> content)
{
    nameEditor.setBounds (content.removeFromTop (28));
    content.removeFromTop (Metrics::gridHalf);

    categoryBox.setBounds (content.removeFromTop (26));
    content.removeFromTop (Metrics::gridHalf);

    tagsEditor.setBounds (content.removeFromTop (26));
    content.removeFromTop (Metrics::gridHalf);

    auto buttons = content.removeFromBottom (Metrics::buttonHeight);
    saveButton.setBounds (buttons.removeFromRight (110));

    content.removeFromBottom (Metrics::gridHalf);
    status.setBounds (content.removeFromBottom (20));
    content.removeFromBottom (Metrics::gridHalf);

    descriptionEditor.setBounds (content);
}

//==============================================================================
//  ChordAndTabPanel
//==============================================================================
int ChordAndTabPanel::ChordListModel::getNumRows()
{
    return owner.matches.size();
}

void ChordAndTabPanel::ChordListModel::paintListBoxItem (int row, juce::Graphics& g,
                                                         int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, owner.matches.size()))
        return;

    const auto& chord = ChordVoicer::getLibraryChord (owner.matches[row]);

    if (selected)
    {
        g.setColour (Palette::accent.withAlpha (0.16f));
        g.fillRect (0, 0, width, height);
    }

    g.setColour (selected ? Palette::accent : Palette::textPrimary);
    g.setFont (Fonts::ui (12.5f, selected));
    g.drawText (chord.name, 10, 0, width - 14, height, juce::Justification::centredLeft, true);
}

void ChordAndTabPanel::ChordListModel::selectedRowsChanged (int lastRow)
{
    if (juce::isPositiveAndBelow (lastRow, owner.matches.size()))
    {
        owner.selectedChord = owner.matches[lastRow];
        owner.diagram.repaint();
    }
}

void ChordAndTabPanel::DiagramComponent::paint (juce::Graphics& g)
{
    const auto& chord = ChordVoicer::getLibraryChord (owner.selectedChord);

    auto bounds = getLocalBounds().reduced (Metrics::grid);

    g.setColour (Palette::textPrimary);
    g.setFont (Fonts::ui (18.0f, true));
    g.drawText (chord.name, bounds.removeFromTop (28), juce::Justification::centred, false);

    bounds.reduce (Metrics::grid, Metrics::grid);

    // Find the window of frets to show.
    int lowest = 99, highest = 0;

    for (int s = 0; s < 6; ++s)
    {
        if (chord.frets[s] > 0)
        {
            lowest = juce::jmin (lowest, chord.frets[s]);
            highest = juce::jmax (highest, chord.frets[s]);
        }
    }

    const int firstFret = (lowest > 4 && lowest != 99) ? lowest : 1;
    const int numFrets = juce::jmax (4, juce::jmin (5, highest - firstFret + 1));

    const float stringSpacing = (float) bounds.getWidth() / 5.0f;
    const float fretSpacing = (float) bounds.getHeight() / (float) (numFrets + 1);

    // Strings run left to right with the low E on the left, as a chord chart is
    // conventionally drawn - which is the mirror of the fretboard above, on purpose.
    for (int s = 0; s < 6; ++s)
    {
        const float x = (float) bounds.getX() + stringSpacing * (float) s;
        g.setColour (Palette::textDisabled);
        g.drawVerticalLine ((int) x, (float) bounds.getY() + fretSpacing,
                            (float) bounds.getBottom());
    }

    for (int f = 0; f <= numFrets; ++f)
    {
        const float y = (float) bounds.getY() + fretSpacing * (float) (f + 1);

        g.setColour (f == 0 && firstFret == 1 ? Palette::textPrimary : Palette::textDisabled);
        g.fillRect ((float) bounds.getX(), y, (float) bounds.getWidth(),
                    (f == 0 && firstFret == 1) ? 3.0f : 1.0f);
    }

    if (firstFret > 1)
    {
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::mono (11.0f));
        g.drawText (juce::String (firstFret) + "fr",
                    bounds.getX() - 34, (int) ((float) bounds.getY() + fretSpacing), 32, 16,
                    juce::Justification::centredRight, false);
    }

    // Dots. Library chords index string 0 as the high E, so the chart is drawn in
    // reverse to put the low E on the left.
    for (int s = 0; s < 6; ++s)
    {
        const int display = 5 - s;
        const float x = (float) bounds.getX() + stringSpacing * (float) display;
        const int fret = chord.frets[s];

        if (fret < 0)
        {
            g.setColour (Palette::textDisabled);
            g.setFont (Fonts::ui (13.0f));
            g.drawText ("x", (int) x - 8, bounds.getY() - 2, 16, 16, juce::Justification::centred, false);
            continue;
        }

        if (fret == 0)
        {
            g.setColour (Palette::textMuted);
            g.drawEllipse (x - 5.0f, (float) bounds.getY() + 2.0f, 10.0f, 10.0f, 1.2f);
            continue;
        }

        const float y = (float) bounds.getY() + fretSpacing * ((float) (fret - firstFret) + 1.5f);

        g.setColour (Palette::accent);
        g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);

        if (chord.fingers[s] > 0)
        {
            g.setColour (Palette::backgroundDeep);
            g.setFont (Fonts::ui (10.0f, true));
            g.drawText (juce::String (chord.fingers[s]), (int) x - 8, (int) y - 8, 16, 16,
                        juce::Justification::centred, false);
        }
    }
}

ChordAndTabPanel::ChordAndTabPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Chords and Tab"), processor (p)
{
    lastNotes.fill (-1);

    addAndMakeVisible (searchBox);
    searchBox.setTextToShowWhenEmpty ("Search chords...", Palette::textDisabled);
    searchBox.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);
    searchBox.onTextChange = [this]
    {
        int found[256];
        const int count = ChordVoicer::searchLibrary (searchBox.getText(), found, 256);

        matches.clear();

        for (int i = 0; i < count; ++i)
            matches.add (found[i]);

        chordList.updateContent();

        if (matches.size() > 0)
            chordList.selectRow (0);
    };

    addAndMakeVisible (chordList);
    chordList.setModel (&listModel);
    chordList.setRowHeight (24);
    chordList.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);

    addAndMakeVisible (diagram);

    addAndMakeVisible (tabView);
    tabView.setMultiLine (true);
    tabView.setReadOnly (true);
    tabView.setScrollbarsShown (true);
    tabView.setCaretVisible (false);
    tabView.setFont (Fonts::mono (12.0f));
    tabView.setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);

    addAndMakeVisible (recordTabToggle);
    recordTabToggle.setTooltip ("Write what you play into the tab below, in real time");
    recordTabToggle.setToggleState (true, juce::dontSendNotification);

    addAndMakeVisible (clearTabButton);
    clearTabButton.onClick = [this]
    {
        tabLines.clear();
        tabView.clear();
        lastNotes.fill (-1);
    };

    addAndMakeVisible (exportTabButton);
    exportTabButton.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Export the tab",
            PresetManager::getRenderFolder().getChildFile ("Luthier Tab.txt"), "*.txt");

        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, chooser] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();

            if (file != juce::File())
                file.replaceWithText (tabView.getText());
        });
    };

    searchBox.onTextChange();
    startTimerHz (12);
}

ChordAndTabPanel::~ChordAndTabPanel()
{
    stopTimer();
}

void ChordAndTabPanel::overlayShown()
{
    searchBox.onTextChange();
}

void ChordAndTabPanel::captureTabColumn()
{
    auto& engine = processor.getEngine();
    const int numStrings = engine.getNumStrings();

    bool anyNew = false;
    int column[kMaxStrings];

    for (int s = 0; s < numStrings; ++s)
    {
        const int note = engine.getStringMidiNote (s);
        column[s] = -1;

        if (note >= 0 && note != lastNotes[(size_t) s])
        {
            column[s] = (int) std::round (engine.getStringFret (s));
            anyNew = true;
        }

        lastNotes[(size_t) s] = note;
    }

    if (! anyNew)
        return;

    // Tab is written with the high E on top, which is the convention.
    while (tabLines.size() < numStrings)
        tabLines.add ("|");

    for (int s = 0; s < numStrings; ++s)
    {
        const auto cell = (column[s] >= 0) ? juce::String (column[s]) : juce::String ("-");
        tabLines.set (s, tabLines[s] + cell.paddedRight ('-', 3));
    }

    // Wrap at a sensible width so the tab stays readable.
    if (tabLines[0].length() > 76)
    {
        juce::String block;

        for (const auto& line : tabLines)
            block << line << "\n";

        block << "\n";

        tabView.moveCaretToEnd (false);
        tabView.insertTextAtCaret (block);

        for (int s = 0; s < tabLines.size(); ++s)
            tabLines.set (s, "|");
    }
}

void ChordAndTabPanel::timerCallback()
{
    if (recordTabToggle.getToggleState())
        captureTabColumn();
}

void ChordAndTabPanel::layoutContent (juce::Rectangle<int> content)
{
    auto left = content.removeFromLeft (juce::jmax (200, content.getWidth() / 3));
    content.removeFromLeft (Metrics::grid);

    searchBox.setBounds (left.removeFromTop (26));
    left.removeFromTop (Metrics::gridHalf);
    chordList.setBounds (left);

    auto diagramArea = content.removeFromTop (juce::jmax (180, content.getHeight() / 2));
    diagram.setBounds (diagramArea);

    content.removeFromTop (Metrics::grid);

    auto tabButtons = content.removeFromTop (Metrics::buttonHeight);
    recordTabToggle.setBounds (tabButtons.removeFromLeft (170));
    exportTabButton.setBounds (tabButtons.removeFromRight (110));
    tabButtons.removeFromRight (Metrics::gridHalf);
    clearTabButton.setBounds (tabButtons.removeFromRight (100));

    content.removeFromTop (Metrics::gridHalf);
    tabView.setBounds (content);
}

//==============================================================================
//  SecretPanel
//==============================================================================
SecretPanel::SecretPanel (LuthierAudioProcessor& p)
    : OverlayPanel ("Wolf"), processor (p)
{
    addAndMakeVisible (blurb);
    blurb.setFont (Fonts::ui (11.5f));
    blurb.setColour (juce::Label::textColourId, Palette::secondary);
    blurb.setJustificationType (juce::Justification::topLeft);
    blurb.setText ("You found it.\n\n"
                   "A wolf tone is what happens when a note lands exactly on a strong body "
                   "resonance and the two feed each other until the instrument howls. Luthiers "
                   "spend real effort designing it out. This puts it back, and then some: a "
                   "dispersive feedback network that smears transients into a rising chirp and "
                   "blooms sustained notes into a howl.\n\n"
                   "It sits right at the end of the chain, after the room. Regeneration is "
                   "capped below unity, so it will misbehave but it will not run away.",
                   juce::dontSendNotification);

    const juce::String secretTip = "Hidden effect - Wolf. Found by clicking the notch in the "
                                   "top-left corner of the window.";

    addAndMakeVisible (enableToggle);
    enableToggle.attachTo (processor, ParamIDs::secretOn, secretTip + " Engages the effect.");

    addAndMakeVisible (rateKnob);
    rateKnob.attachTo (processor, ParamIDs::secretRate,
                       secretTip + " How fast the network's delay is swept.");
    rateKnob.setAccentColour (Palette::secondary);

    addAndMakeVisible (depthKnob);
    depthKnob.attachTo (processor, ParamIDs::secretDepth,
                        secretTip + " How far it sweeps, and how much dispersion goes with it.");
    depthKnob.setAccentColour (Palette::secondary);

    addAndMakeVisible (feedbackKnob);
    feedbackKnob.attachTo (processor, ParamIDs::secretFeedback,
                           secretTip + " How much of the output goes back in. Capped below unity.");
    feedbackKnob.setAccentColour (Palette::secondary);

    addAndMakeVisible (mixKnob);
    mixKnob.attachTo (processor, ParamIDs::secretMix, secretTip + " Wet/dry balance.");
    mixKnob.setAccentColour (Palette::secondary);
}

void SecretPanel::layoutContent (juce::Rectangle<int> content)
{
    enableToggle.setBounds (content.removeFromTop (Metrics::buttonHeight).removeFromLeft (120));
    content.removeFromTop (Metrics::grid);

    auto knobRow = content.removeFromTop (LuthierKnob::preferredHeightFor (LuthierKnob::Size::Normal));
    const int cell = knobRow.getWidth() / 4;

    rateKnob.setBounds (knobRow.removeFromLeft (cell));
    depthKnob.setBounds (knobRow.removeFromLeft (cell));
    feedbackKnob.setBounds (knobRow.removeFromLeft (cell));
    mixKnob.setBounds (knobRow);

    content.removeFromTop (Metrics::grid);
    blurb.setBounds (content);
}

} // namespace luthier
