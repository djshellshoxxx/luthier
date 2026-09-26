#include "JamPanel.h"
#include "Theme.h"
#include "UiPreferences.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Jam/JamEdition.h"
#include "../Jam/JamMidiExport.h"

namespace luthier
{

namespace
{
    constexpr int kGap = Metrics::gridHalf;
    constexpr int kGroupHeader = 20;
    constexpr int kHeaderRow = Metrics::buttonHeight;
    constexpr int kMessageHeight = 46;
    constexpr int kPairWidth = 720;   // below this the paired groups stack (8.1)

    int knobW() { return LuthierKnob::preferredWidthFor (LuthierKnob::Size::Small); }
    int knobH() { return LuthierKnob::preferredHeightFor (LuthierKnob::Size::Small); }
    constexpr int kChoiceH = LuthierChoice::labelHeight + 24;

    JamMidiExportOptions exportOptions (LuthierAudioProcessor& processor, int bars, bool luthierProfile)
    {
        const auto cfg = processor.getRouting().getMidiOutConfig();
        JamMidiExportOptions options;
        options.bars = bars;
        options.luthierProfile = luthierProfile;
        options.drumChannel = cfg.jamDrumChannel;
        options.bassChannel = cfg.jamBassChannel;
        return options;
    }
}

//==============================================================================
struct JamPanel::Group
{
    struct Item
    {
        juce::Component* component = nullptr;
        int width = 0, height = 0;
        juce::Rectangle<int>* store = nullptr;   ///< an area painted by the panel
        bool fullWidth = false;
    };

    juce::String title;
    std::vector<Item> items;
    int pairWith = -1;   ///< index of the group beside it, when paired
    juce::Rectangle<int> bounds;

    /** Flows the items into `width`; returns the height used. */
    int layout (juce::Rectangle<int> area, bool apply)
    {
        const int innerW = juce::jmax (1, area.getWidth() - 2 * kGap);
        int x = 0, y = kGroupHeader, rowH = 0;

        for (auto& item : items)
        {
            const int w = item.fullWidth ? innerW : juce::jmin (item.width, innerW);

            if (x > 0 && x + w > innerW)
            {
                x = 0;
                y += rowH + kGap;
                rowH = 0;
            }

            const juce::Rectangle<int> r (area.getX() + kGap + x, area.getY() + y, w, item.height);

            if (apply)
            {
                if (item.component != nullptr)
                    item.component->setBounds (r);

                if (item.store != nullptr)
                    *item.store = r;
            }

            x += w + kGap;
            rowH = juce::jmax (rowH, item.height);
        }

        const int height = y + rowH + kGap;

        if (apply)
            bounds = area.withHeight (height);

        return height;
    }
};

//==============================================================================
class JamPanel::DragOut : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    explicit DragOut (JamPanel& p) : panel (p)
    {
        setTooltip ("Drag the band's last bars out as a MIDI file: drums on channel 10, bass on 11. "
                    "Generic profile; hold Alt while dragging for the Luthier profile.");
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        AccessibleSetup::configureDescriptive (*this, "Drag Jam MIDI", "Drag the band's last bars out as a MIDI file.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        const float dashes[] = { 4.0f, 3.0f };
        juce::Path outline, dashed;
        outline.addRoundedRectangle (area, Metrics::controlCorner);
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);

        g.setColour (Palette::panelSunken);
        g.fillRoundedRectangle (area, Metrics::controlCorner);
        g.setColour (isEnabled() ? Palette::accent : Palette::textDisabled);
        g.fillPath (dashed);
        g.setFont (Fonts::ui (11.0f, true));
        g.drawFittedText ("DRAG MIDI", getLocalBounds(), juce::Justification::centred, 1);
    }

    void mouseDown (const juce::MouseEvent&) override { dragStarted = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStarted || e.getDistanceFromDragStart() < 6)
            return;

        dragStarted = true;
        const auto options = exportOptions (panel.processor, panel.getDragBars(), e.mods.isAltDown());
        const auto file = JamMidiExport::write (panel.processor.getJam().getCapture(), options);

        if (file.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
    }

private:
    JamPanel& panel;
    bool dragStarted = false;
};

//==============================================================================
JamPanel::JamPanel (LuthierAudioProcessor& p)
    : processor (p)
{
    // --- header -----------------------------------------------------------------------
    armed.attachTo (processor, ParamIDs::jamEnabled, "Arms the Jam band: it starts with your first chord.");
    play.attachTo (processor, ParamIDs::jamPlay, "Starts or stops the band now. Not undoable (transport).");
    fill.attachTo (processor, ParamIDs::jamFillNow, "A fill into the next bar.");

    // START on a band that is not armed arms it too (8.2's first press).
    play.getButton().onClick = [this]
    {
        if (play.getButton().getToggleState())
            if (auto* e = processor.getState().getParameter (ParamIDs::jamEnabled))
                if (e->getValue() < 0.5f)
                    e->setValueNotifyingHost (1.0f);
    };

    for (auto* label : { &statusLabel, &chordLabel })
    {
        label->setFont (Fonts::mono (12.0f));
        label->setColour (juce::Label::textColourId, Palette::textPrimary);
        addAndMakeVisible (*label);
    }

    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setTooltip ("The band's state, style, variation and intensity (with the dynamics' change).");
    chordLabel.setTooltip ("The chord the band is on, the next one and where it comes from, the tempo and the bar.");
    messageLabel.setFont (Fonts::ui (11.0f));
    messageLabel.setColour (juce::Label::textColourId, Palette::warning);
    messageLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (messageLabel);

    AccessibleSetup::configureDescriptive (statusLabel, "Band status", "-");
    AccessibleSetup::configureDescriptive (chordLabel, "Band chords", "-");
    AccessibleSetup::configureDescriptive (messageLabel, "Band notices", {});

    // --- STYLE ----------------------------------------------------------------------
    style.attachTo (processor, ParamIDs::jamStyle, "The band's style. User plays the style file you pick.");
    variation.attachTo (processor, ParamIDs::jamVariation, "Groove A or B of the style.");

    loadStyle.setTooltip ("Pick a .luthierjam style file (the style becomes User).");
    loadStyle.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Pick a Jam style", JamStyleLibrary::getUserFolder(), "*.luthierjam");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe = juce::Component::SafePointer<JamPanel> (this)] (const juce::FileChooser& fc)
                              {
                                  if (safe != nullptr && fc.getResult().existsAsFile())
                                  {
                                      safe->processor.loadJamStyleFile (fc.getResult());
                                      safe->refresh();
                                  }
                              });
    };
    AccessibleSetup::configureButton (loadStyle, "Load a user style", "Opens a file chooser for a .luthierjam style");

    linkKit.setTooltip ("Also switch the guitar's rhythm genre kit to the style's (never its rig preset).");
    linkKit.onClick = [this] { processor.setJamRhythmKitLinked (linkKit.getToggleState()); };
    AccessibleSetup::configureButton (linkKit, "Link guitar rhythm kit");

    // --- FEEL -----------------------------------------------------------------------
    intensity.attachTo (processor, ParamIDs::jamIntensity, "Intensity");
    intensityLabel.setFont (Fonts::label());
    intensityLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    fillEvery.attachTo (processor, ParamIDs::jamFillEvery, "A fill every so many bars (Off for none).");
    swing.attachTo (processor, ParamIDs::jamSwing, "Swing: 0 is the style's own; right swings harder, left straighter.");
    humanise.attachTo (processor, ParamIDs::jamHumanise, "How human the timing and velocities are.");
    dynamicsFollow.attachTo (processor, ParamIDs::jamDynamicsFollow, "The band plays up or down with how hard you play.");

    // --- FOLLOW ---------------------------------------------------------------------
    source.attachTo (processor, ParamIDs::jamChordSource, "Where the chords come from: Auto, your playing, or the tune.");
    follow.attachTo (processor, ParamIDs::jamFollow, "How quickly the band follows a chord change.");
    predict.attachTo (processor, ParamIDs::jamPredict, "Anticipate chord changes when your progression repeats.");

    // --- START/STOP -----------------------------------------------------------------
    startMode.attachTo (processor, ParamIDs::jamStartMode, "How the band starts.");

    for (int i = 0; i <= 2; ++i)
        countIn.getComboBox().addItem (juce::String (i), i + 1);

    countIn.attachTo (processor, ParamIDs::jamCountInBars, "Count-in bars (the band's sticks).");
    stopOnSilence.attachTo (processor, ParamIDs::jamStopOnSilence, "Stop when you stop playing.");

    for (int i = 1; i <= 8; ++i)
        silenceBars.getComboBox().addItem (juce::String (i), i);

    silenceBars.attachTo (processor, ParamIDs::jamSilenceBars, "Bars of silence before the band stops.");
    ending.attachTo (processor, ParamIDs::jamEnding, "Play an ending when the band stops.");

    // 11: a user preference (default on), not a parameter.
    metronomeQuiet.setTooltip ("The metronome and the tune's click go quiet while the band's drums are heard; the visual beat keeps running.");
    metronomeQuiet.setToggleState (UiPreferences::get().getBool (kMetronomePreference, true), juce::dontSendNotification);
    processor.setJamSilencesMetronome (metronomeQuiet.getToggleState());
    metronomeQuiet.onClick = [this]
    {
        UiPreferences::get().setBool (kMetronomePreference, metronomeQuiet.getToggleState());
        processor.setJamSilencesMetronome (metronomeQuiet.getToggleState());
    };
    AccessibleSetup::configureButton (metronomeQuiet, "Metronome goes quiet while the band plays");

    // --- KIT ------------------------------------------------------------------------
    kit.attachTo (processor, ParamIDs::jamKit, "The drum kit.");
    kitAuto.attachTo (processor, ParamIDs::jamKitAuto, "Use the style's own kit.");
    tuning.attachTo (processor, ParamIDs::jamKitTuning, "Kit tuning in semitones.");
    damping.attachTo (processor, ParamIDs::jamKitDamping, "Kit damping: more is shorter and drier.");
    room.attachTo (processor, ParamIDs::jamKitRoom, "The kit's room.");
    width.attachTo (processor, ParamIDs::jamKitWidth, "The kit's stereo width.");
    perspective.attachTo (processor, ParamIDs::jamKitPerspective, "Hear the kit from the audience or the drum stool.");

    // --- BASS -----------------------------------------------------------------------
    bassVoice.attachTo (processor, ParamIDs::jamBassVoice, "The bass: Auto picks the style's.");
    bassTone.attachTo (processor, ParamIDs::jamBassTone, "Bass tone: dark to bright.");
    bassNote.setFont (Fonts::ui (11.0f));
    bassNote.setColour (juce::Label::textColourId, Palette::textMuted);
    AccessibleSetup::configureDescriptive (bassNote, "Bass status", {});

    // --- MIXER ----------------------------------------------------------------------
    volume.attachTo (processor, ParamIDs::jamVolume, "The band's volume.");
    balance.attachTo (processor, ParamIDs::jamBalance, "Drums against bass: left drums only, right bass only.");
    drumsPan.attachTo (processor, ParamIDs::jamDrumsPan, "Drums pan.");
    bassPan.attachTo (processor, ParamIDs::jamBassPan, "Bass pan.");
    drumsMute.attachTo (processor, ParamIDs::jamDrumsMute, "Mute the drums.");
    bassMute.attachTo (processor, ParamIDs::jamBassMute, "Mute the bass.");
    output.attachTo (processor, ParamIDs::jamOutput, "Main, Separate (Aux 9 Jam Drums and Aux 10 Jam Bass) or both.");

    // --- LANES ----------------------------------------------------------------------
    for (int i = 0; i < (int) std::size (JamMidiExport::kDragChoices); ++i)
    {
        const int bars = JamMidiExport::kDragChoices[i];
        dragBars.addItem (bars > 0 ? "Last " + juce::String (bars) + " bars" : juce::String ("All"), i + 1);
    }

    dragBars.setSelectedId (2, juce::dontSendNotification);   // 8 bars
    dragBars.setTooltip ("How many bars the drag-out and Export MIDI take.");
    AccessibleSetup::configureComboBox (dragBars, "Jam MIDI length");

    dragOut = std::make_unique<DragOut> (*this);

    exportButton.setTooltip ("Save the band's last bars as a MIDI file (Type 1: Jam Drums and Jam Bass).");
    exportButton.onClick = [this]
    {
        const auto options = exportOptions (processor, getDragBars(), false);
        chooser = std::make_unique<juce::FileChooser> ("Export the band as MIDI",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                           .getChildFile (JamMidiExport::suggestedName (options)),
                                                       "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe = juce::Component::SafePointer<JamPanel> (this)] (const juce::FileChooser& fc)
                              {
                                  if (safe != nullptr && fc.getResult() != juce::File())
                                      safe->exportTo (fc.getResult(), false);
                              });
    };
    AccessibleSetup::configureButton (exportButton, "Export Jam MIDI");

    // Everything in, in the sketch's order.
    for (auto* c : std::initializer_list<juce::Component*> {
             &armed, &play, &fill, &statusLabel, &chordLabel, &messageLabel,
             &style, &variation, &loadStyle, &linkKit,
             &intensityLabel, &intensity, &fillEvery, &swing, &humanise, &dynamicsFollow,
             &source, &follow, &predict,
             &startMode, &countIn, &stopOnSilence, &silenceBars, &ending, &metronomeQuiet,
             &kit, &kitAuto, &tuning, &damping, &room, &width, &perspective,
             &bassVoice, &bassTone, &bassNote,
             &volume, &balance, &drumsPan, &drumsMute, &bassPan, &bassMute, &output,
             &lanes, &dragBars, dragOut.get(), &exportButton })
        addAndMakeVisible (*c);

    // 12: Tab follows the sketch.
    {
        int order = 1;

        for (auto* c : getFocusOrder())
        {
            c->setWantsKeyboardFocus (true);

            // The wrapper that is the panel's child carries the order.
            auto* child = c;

            while (child->getParentComponent() != nullptr && child->getParentComponent() != this)
                child = child->getParentComponent();

            child->setExplicitFocusOrder (order++);
        }
    }

    // jam-mode 15: the Free build's locks (none in this Pro build).
    if (JamEdition::kIsFree)
    {
        auto lockItems = [] (juce::ComboBox& box, JamEdition::Item item)
        {
            for (int i = 0; i < box.getNumItems(); ++i)
                box.setItemEnabled (box.getItemId (i), ! JamEdition::isLocked (item, i));
        };

        lockItems (style.getComboBox(), JamEdition::Item::style);
        lockItems (kit.getComboBox(), JamEdition::Item::kit);
        lockItems (bassVoice.getComboBox(), JamEdition::Item::bassVoice);
        lockItems (output.getComboBox(), JamEdition::Item::output);
        tuning.setEnabled (false);
        damping.setEnabled (false);
        loadStyle.setEnabled (false);
        exportButton.setEnabled (false);
        dragOut->setEnabled (false);
    }

    buildGroups();
    setSize (900, getPreferredHeight());
    refresh();
    motion.startTimerHz (*this, 30);   // 8.3
}

JamPanel::~JamPanel()
{
    motion.stopTimer();
}

std::vector<juce::Component*> JamPanel::getFocusOrder() const
{
    auto* self = const_cast<JamPanel*> (this);

    return { &self->armed.getButton(), &self->play.getButton(), &self->fill.getButton(),
             &self->style.getComboBox(), &self->variation.getComboBox(), &self->loadStyle, &self->linkKit,
             &self->intensity, &self->fillEvery.getComboBox(), &self->swing.getSlider(), &self->humanise.getSlider(),
             &self->dynamicsFollow.getButton(),
             &self->source.getComboBox(), &self->follow.getComboBox(), &self->predict.getButton(),
             &self->startMode.getComboBox(), &self->countIn.getComboBox(), &self->stopOnSilence.getButton(),
             &self->silenceBars.getComboBox(), &self->ending.getButton(), &self->metronomeQuiet,
             &self->kit.getComboBox(), &self->kitAuto.getButton(), &self->tuning.getSlider(), &self->damping.getSlider(),
             &self->room.getSlider(), &self->width.getSlider(), &self->perspective.getComboBox(),
             &self->bassVoice.getComboBox(), &self->bassTone.getSlider(),
             &self->volume.getSlider(), &self->balance.getSlider(), &self->drumsPan.getSlider(), &self->drumsMute.getButton(),
             &self->bassPan.getSlider(), &self->bassMute.getButton(), &self->output.getComboBox(),
             &self->lanes, &self->dragBars, &self->exportButton };
}

void JamPanel::buildGroups()
{
    using Item = Group::Item;
    const int kw = knobW(), kh = knobH();

    auto add = [this] (const char* title, std::vector<Item> items, int pairWith = -1)
    {
        auto g = std::make_unique<Group>();
        g->title = title;
        g->items = std::move (items);
        g->pairWith = pairWith;
        groups.push_back (std::move (g));
    };

    add ("STYLE", { { &style, 150, kChoiceH }, { &variation, 70, kChoiceH },
                    { &loadStyle, 104, Metrics::rowHeight }, { &linkKit, 190, Metrics::rowHeight } }, 1);
    add ("FEEL", { { &intensityLabel, 60, 22 }, { &intensity, 120, 22 }, { &fillEvery, 96, kChoiceH },
                   { &swing, kw, kh }, { &humanise, kw, kh }, { &dynamicsFollow, 130, Metrics::rowHeight } }, 0);
    add ("FOLLOW", { { &source, 96, kChoiceH }, { &follow, 96, kChoiceH },
                     { &predict, 130, Metrics::rowHeight } }, 3);
    add ("START/STOP", { { &startMode, 128, kChoiceH }, { &countIn, 56, kChoiceH }, { &silenceBars, 56, kChoiceH },
                         { &stopOnSilence, 136, Metrics::rowHeight }, { &ending, 124, Metrics::rowHeight },
                         { &metronomeQuiet, 300, Metrics::rowHeight } }, 2);
    add ("KIT", { { &kit, 104, kChoiceH }, { &kitAuto, 80, Metrics::rowHeight }, { &tuning, kw, kh }, { &damping, kw, kh },
                  { &room, kw, kh }, { &width, kw, kh }, { &perspective, 104, kChoiceH } });
    add ("BASS", { { &bassVoice, 112, kChoiceH }, { &bassTone, kw, kh }, { &bassNote, 280, Metrics::rowHeight } });
    add ("MIXER", { { &volume, kw, kh }, { &balance, kw, kh }, { &drumsPan, kw, kh }, { &drumsMute, 70, Metrics::rowHeight },
                    { &bassPan, kw, kh }, { &bassMute, 70, Metrics::rowHeight }, { &output, 136, kChoiceH },
                    { nullptr, 44, 40, &meterBounds } });
    add ("LANES", { { &lanes, 0, JamLaneView::kPreferredHeight, nullptr, true },
                    { &dragBars, 120, Metrics::rowHeight }, { dragOut.get(), 110, Metrics::rowHeight },
                    { &exportButton, 104, Metrics::rowHeight } });
}

int JamPanel::layoutGroups (int panelWidth, bool apply)
{
    auto area = juce::Rectangle<int> (0, 0, panelWidth, 100000).reduced (kGap, 0);
    int y = kGap;

    // The header: buttons and status, the chord line, the notices.
    {
        auto row = juce::Rectangle<int> (area.getX(), y, area.getWidth(), kHeaderRow);

        if (apply)
        {
            armed.setBounds (row.removeFromLeft (84));
            row.removeFromLeft (kGap);
            play.setBounds (row.removeFromLeft (116));
            row.removeFromLeft (kGap);
            fill.setBounds (row.removeFromLeft (60));
            row.removeFromLeft (kGap);
            statusLabel.setBounds (row);
        }

        y += kHeaderRow + kGap;

        if (apply)
            chordLabel.setBounds (area.getX(), y, area.getWidth(), 18);

        y += 18 + kGap;

        if (apply)
            messageLabel.setBounds (area.getX(), y, area.getWidth(), kMessageHeight);

        y += kMessageHeight + kGap;
    }

    const bool paired = panelWidth >= kPairWidth;

    for (size_t i = 0; i < groups.size(); ++i)
    {
        auto& g = *groups[i];

        if (paired && g.pairWith >= 0)
        {
            if ((int) i > g.pairWith)
                continue;   // laid out with its partner

            auto& partner = *groups[(size_t) g.pairWith];
            const int half = (area.getWidth() - kGap) / 2;
            const int h1 = g.layout ({ area.getX(), y, half, 0 }, false);
            const int h2 = partner.layout ({ area.getX() + half + kGap, y, half, 0 }, false);
            const int h = juce::jmax (h1, h2);

            if (apply)
            {
                g.layout ({ area.getX(), y, half, 0 }, true);
                partner.layout ({ area.getX() + half + kGap, y, half, 0 }, true);
                g.bounds.setHeight (h);
                partner.bounds.setHeight (h);
            }

            y += h + kGap;
            continue;
        }

        const int h = g.layout ({ area.getX(), y, area.getWidth(), 0 }, apply);
        y += h + kGap;
    }

    return y;
}

int JamPanel::getPreferredHeightFor (int panelWidth) const
{
    return const_cast<JamPanel*> (this)->layoutGroups (juce::jmax (480, panelWidth), false);
}

int JamPanel::getPreferredHeight() const
{
    return getPreferredHeightFor (getWidth() > 0 ? getWidth() : 900);
}

void JamPanel::resized()
{
    layoutGroups (getWidth(), true);
}

void JamPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);

    for (const auto& group : groups)
    {
        g.setColour (Palette::panelRaised);
        g.fillRoundedRectangle (group->bounds.toFloat(), Metrics::panelCorner);
        LuthierLookAndFeel::drawSectionHeader (g, group->bounds.withHeight (kGroupHeader), group->title);
    }

    // The part meters, from the status's peaks (with the parts' initials as well as the bars).
    if (! meterBounds.isEmpty())
    {
        auto r = meterBounds.reduced (2);
        const auto& s = facts.status;
        const double peaks[] = { facts.haveStatus ? s.drumsPeak : 0.0, facts.haveStatus ? s.bassPeak : 0.0 };
        const char* names[] = { "D", "B" };
        const int w = r.getWidth() / 2;

        for (int i = 0; i < 2; ++i)
        {
            auto bar = r.removeFromLeft (w).reduced (2, 0);
            auto label = bar.removeFromBottom (10);
            const double db = juce::Decibels::gainToDecibels (peaks[i], -60.0);
            const float level = (float) juce::jlimit (0.0, 1.0, (db + 60.0) / 60.0);

            g.setColour (Palette::panelSunken);
            g.fillRect (bar);
            g.setColour (db > -1.0 ? Palette::clip : Palette::dataStream);
            g.fillRect (bar.withTrimmedTop ((int) ((1.0f - level) * (float) bar.getHeight())));
            g.setColour (Palette::textMuted);
            g.setFont (Fonts::ui (9.0f));
            g.drawText (names[i], label, juce::Justification::centred);
        }
    }
}

//==============================================================================
int JamPanel::getDragBars() const
{
    const int index = juce::jlimit (0, (int) std::size (JamMidiExport::kDragChoices) - 1, dragBars.getSelectedId() - 1);
    return JamMidiExport::kDragChoices[index];
}

juce::File JamPanel::exportTo (const juce::File& destination, bool luthierProfile) const
{
    return JamMidiExport::write (processor.getJam().getCapture(), exportOptions (processor, getDragBars(), luthierProfile),
                                 destination);
}

void JamPanel::showFacts (const JamUiFacts& forced)
{
    facts = forced;
    factsForced = true;
    refresh();
}

void JamPanel::refresh()
{
    if (! factsForced)
    {
        auto& apvts = processor.getState();
        auto on = [&apvts] (const char* id) { auto* p = apvts.getParameter (id); return p != nullptr && p->getValue() > 0.5f; };

        JamUiFacts f;
        JamStatus status;
        const double now = juce::Time::getMillisecondCounterHiRes();

        if (processor.getJam().getStatusChannel().read (status))
        {
            if (status.sequence != lastSequence)
            {
                lastSequence = status.sequence;
                lastSequenceAt = now;
            }

            f.status = status;
            f.haveStatus = now - lastSequenceAt < 250.0;   // 8.3
        }

        f.enabled = on (ParamIDs::jamEnabled);

        if (auto* src = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::jamChordSource)))
            f.chordSource = src->getIndex();

        f.tunePlaying = processor.getTunePlayer().isPlaying();
        f.separateFallback = processor.isJamSeparateFallingBack();
        f.backingTrackPlaying = processor.getBackingTrack().isPlaying();
        f.styleWarning = processor.getJamStyleWarning();
        facts = f;
    }

    // The lanes and the stale playhead.
    lanes.setStatus (facts.status, facts.haveStatus);

    statusText = JamUiText::statusLine (facts);
    chordText = JamUiText::chordLine (facts);
    messages = JamUiText::messages (facts);

    if (statusLabel.getText() != statusText) { statusLabel.setText (statusText, juce::dontSendNotification); statusLabel.setDescription (statusText); }
    if (chordLabel.getText() != chordText)   { chordLabel.setText (chordText, juce::dontSendNotification); chordLabel.setDescription (chordText); }

    const auto messageText = messages.joinIntoString ("\n");

    if (messageLabel.getText() != messageText)
    {
        messageLabel.setText (messageText, juce::dontSendNotification);
        messageLabel.setDescription (messageText);
    }

    // 7 / 8.4: the bass group says when the bass is resting or playing the tune's line.
    const auto bassText = facts.haveStatus && facts.status.bassResting ? juce::String (JamUiText::kBassistResting)
                        : facts.haveStatus && facts.status.tuneBassPlaying ? juce::String ("Playing the tune's bass line")
                                                                           : juce::String();

    if (bassNote.getText() != bassText)
        bassNote.setText (bassText, juce::dontSendNotification);

    if (linkKit.getToggleState() != processor.isJamRhythmKitLinked())
        linkKit.setToggleState (processor.isJamRhythmKitLinked(), juce::dontSendNotification);

    if (metronomeQuiet.getToggleState() != processor.doesJamSilenceMetronome())
        metronomeQuiet.setToggleState (processor.doesJamSilenceMetronome(), juce::dontSendNotification);

    if (facts.haveStatus)
        announce (facts.status);

    repaint (meterBounds);
}

void JamPanel::announce (const JamStatus& s)
{
    // 12: the pill's state changes, and the style or intensity while playing.
    const bool changed = s.state != announcedState
                         || (s.state == JamState::playing && (s.style != announcedStyle || s.effectiveIntensity != announcedIntensity));

    if (changed)
    {
        announcedState = s.state;
        announcedStyle = s.style;
        announcedIntensity = s.effectiveIntensity;
        lastAnnouncement = JamUiText::stateAnnouncement (s);

        if (isShowing())
            juce::AccessibilityHandler::postAnnouncement (lastAnnouncement, juce::AccessibilityHandler::AnnouncementPriority::medium);
    }

    // At verbosity High, chord changes, at most once every 2 s.
    if (AccessibilitySettings::get().getVerbosity() == AccessibilitySettings::Verbosity::verbose && s.currentChord.isKnown())
    {
        const auto chord = s.currentChord.toString();
        const double now = juce::Time::getMillisecondCounterHiRes();

        if (chord != announcedChord && now - lastChordAnnouncementAt >= 2000.0)
        {
            announcedChord = chord;
            lastChordAnnouncementAt = now;
            lastAnnouncement = "Chord " + chord;

            if (isShowing())
                juce::AccessibilityHandler::postAnnouncement (lastAnnouncement, juce::AccessibilityHandler::AnnouncementPriority::low);
        }
    }
}

} // namespace luthier
