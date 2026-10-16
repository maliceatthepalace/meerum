#include "Pages.h"
#include "../PluginProcessor.h"

using namespace juce;

namespace
{
    Label& styleCaption (Label& l, const String& text)
    {
        l.setText (text, dontSendNotification);
        l.setFont (Theme::font (10.5f));
        l.setColour (Label::textColourId, Theme::textDim);
        l.setJustificationType (Justification::centredLeft);
        return l;
    }
}

// ============================================================================
TabBar::TabBar (const StringArray& names)
{
    for (int i = 0; i < names.size(); ++i)
    {
        auto b = std::make_unique<TextButton> (names[i]);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);
        b->onClick = [this, i] { if (buttons[(size_t) i]->getToggleState()) setSelected (i); };
        addAndMakeVisible (*b);
        buttons.push_back (std::move (b));
    }
    buttons.front()->setToggleState (true, dontSendNotification);
}

void TabBar::resized()
{
    auto r = getLocalBounds();
    const int w = (r.getWidth() - 3 * ((int) buttons.size() - 1)) / (int) buttons.size();
    for (auto& b : buttons)
    {
        b->setBounds (r.removeFromLeft (w));
        r.removeFromLeft (3);
    }
}

void TabBar::setSelected (int index)
{
    selected = index;
    buttons[(size_t) index]->setToggleState (true, dontSendNotification);
    if (onChange) onChange (index);
}

KnobSet::KnobSet (APVTS& state, Component& parent, std::initializer_list<std::pair<String, String>> specs)
{
    for (auto& [id, caption] : specs)
    {
        knobs.push_back (std::make_unique<Knob> (state, id, caption));
        parent.addAndMakeVisible (*knobs.back());
    }
}

// ============================================================================
OscPanel::OscPanel (MeerumProcessor& p, int oscIndex)
    : Panel (oscIndex == 0 ? "OSC A" : "OSC B"),
      processor (p),
      osc (oscIndex),
      power (p.apvts, Params::osc (oscIndex, "on")),
      table (p.apvts, Params::osc (oscIndex, "table")),
      display (p, oscIndex),
      warpMode (p.apvts, Params::osc (oscIndex, "warp")),
      knobs (p.apvts, *this, {
          { Params::osc (oscIndex, "pos"),     "WT POS" },
          { Params::osc (oscIndex, "oct"),     "OCT" },
          { Params::osc (oscIndex, "semi"),    "SEMI" },
          { Params::osc (oscIndex, "fine"),    "FINE" },
          { Params::osc (oscIndex, "unison"),  "UNISON" },
          { Params::osc (oscIndex, "detune"),  "DETUNE" },
          { Params::osc (oscIndex, "width"),   "WIDTH" },
          { Params::osc (oscIndex, "pan"),     "PAN" },
          { Params::osc (oscIndex, "level"),   "LEVEL" },
          { Params::osc (oscIndex, "phase"),   "PHASE" },
          { Params::osc (oscIndex, "rand"),    "RAND" },
          { Params::osc (oscIndex, "warpAmt"), "WARP" } })
{
    addAndMakeVisible (power);
    addAndMakeVisible (table);
    addAndMakeVisible (display);
    addAndMakeVisible (warpMode);

    loadButton.setTooltip ("Load a wavetable from a WAV file (2048 samples per frame)");
    loadButton.onClick = [this] { chooseWavetable(); };
    addAndMakeVisible (loadButton);

    display.onFileDropped = [this] (const File& f) { loadWavetable (f); };
}

void OscPanel::chooseWavetable()
{
    chooser = std::make_unique<FileChooser> ("Load wavetable", File::getSpecialLocation (File::userDocumentsDirectory),
                                             "*.wav;*.aif;*.aiff;*.flac");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile())
                                  loadWavetable (file);
                          });
}

void OscPanel::loadWavetable (const File& file)
{
    String error;
    if (! processor.loadCustomWavetable (osc, file, error))
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Could not load wavetable",
                                          error.isNotEmpty() ? error : "Unsupported file: " + file.getFileName());
}

void OscPanel::resized()
{
    auto header = getHeaderArea();
    power.setBounds (header.removeFromLeft (20));
    header.removeFromLeft (4);
    loadButton.setBounds (header.removeFromRight (40));
    header.removeFromRight (4);
    table.setBounds (header);

    auto r = getContentArea();
    constexpr int knobRow = 54;
    auto row2 = r.removeFromBottom (knobRow);
    r.removeFromBottom (2);
    auto row1 = r.removeFromBottom (knobRow);
    r.removeFromBottom (6);
    display.setBounds (r);

    layoutRow (row1, { &knobs[0], &knobs[1], &knobs[2], &knobs[3], &knobs[4], &knobs[5] }, 2);

    // Last column of row 2 stacks the warp mode selector over the warp amount.
    const int colWidth = (row2.getWidth() - 2 * 5) / 6;
    auto warpColumn = row2.removeFromRight (colWidth);
    row2.removeFromRight (2);
    warpMode.setBounds (warpColumn.removeFromTop (18));
    knobs[11].setBounds (warpColumn);
    layoutRow (row2, { &knobs[6], &knobs[7], &knobs[8], &knobs[9], &knobs[10] }, 2);
}

// ============================================================================
SubPanel::SubPanel (APVTS& s)
    : Panel ("SUB"), power (s, "sub_on"), shape (s, "sub_shape"),
      knobs (s, *this, { { "sub_oct", "OCT" }, { "sub_level", "LEVEL" }, { "sub_pan", "PAN" } })
{
    addAndMakeVisible (power);
    addAndMakeVisible (shape);
}

void SubPanel::resized()
{
    auto header = getHeaderArea();
    power.setBounds (header.removeFromLeft (20));
    header.removeFromLeft (4);
    shape.setBounds (header);
    layoutRow (getContentArea().withSizeKeepingCentre (getContentArea().getWidth(), 58),
               { &knobs[0], &knobs[1], &knobs[2] }, 2);
}

NoisePanel::NoisePanel (APVTS& s)
    : Panel ("NOISE"), power (s, "noise_on"), type (s, "noise_type"),
      knobs (s, *this, { { "noise_level", "LEVEL" }, { "noise_pan", "PAN" } })
{
    addAndMakeVisible (power);
    addAndMakeVisible (type);
}

void NoisePanel::resized()
{
    auto header = getHeaderArea();
    power.setBounds (header.removeFromLeft (20));
    header.removeFromLeft (4);
    type.setBounds (header);
    layoutRow (getContentArea().withSizeKeepingCentre (getContentArea().getWidth() * 2 / 3, 58),
               { &knobs[0], &knobs[1] }, 2);
}

// ============================================================================
FilterPanel::FilterPanel (APVTS& s)
    : Panel ("FILTER"), power (s, "flt_on"), type (s, "flt_type"), curve (s),
      knobs (s, *this, { { "flt_cutoff", "CUTOFF" }, { "flt_res", "RES" }, { "flt_drive", "DRIVE" },
                         { "flt_key", "KEY" }, { "flt_mix", "MIX" } }),
      routeA (s, "flt_oscA", "A"), routeB (s, "flt_oscB", "B"),
      routeSub (s, "flt_sub", "S"), routeNoise (s, "flt_noise", "N")
{
    for (auto* c : std::initializer_list<Component*> { &power, &type, &curve, &routeA, &routeB, &routeSub, &routeNoise })
        addAndMakeVisible (c);

    routeA.setTooltip ("Route OSC A through the filter");
    routeB.setTooltip ("Route OSC B through the filter");
    routeSub.setTooltip ("Route SUB through the filter");
    routeNoise.setTooltip ("Route NOISE through the filter");
}

void FilterPanel::resized()
{
    auto header = getHeaderArea();
    power.setBounds (header.removeFromLeft (20));
    header.removeFromLeft (4);
    type.setBounds (header);

    auto r = getContentArea();
    auto routes = r.removeFromBottom (20);
    r.removeFromBottom (4);
    auto row2 = r.removeFromBottom (58);
    auto row1 = r.removeFromBottom (58);
    r.removeFromBottom (6);
    curve.setBounds (r);

    layoutRow (row1, { &knobs[0], &knobs[1], &knobs[2] }, 2);
    layoutRow (row2.withSizeKeepingCentre (row2.getWidth() * 2 / 3, row2.getHeight()), { &knobs[3], &knobs[4] }, 2);
    layoutRow (routes, { &routeA, &routeB, &routeSub, &routeNoise }, 4);
}

// ============================================================================
EnvSection::Page::Page (APVTS& s, int index)
    : graph (s, index),
      knobs (s, *this, { { Params::env (index, "a"), "ATTACK" }, { Params::env (index, "h"), "HOLD" },
                         { Params::env (index, "d"), "DECAY" },  { Params::env (index, "s"), "SUSTAIN" },
                         { Params::env (index, "r"), "RELEASE" } })
{
    addAndMakeVisible (graph);
}

void EnvSection::Page::resized()
{
    auto r = getLocalBounds();
    layoutRow (r.removeFromBottom (58), { &knobs[0], &knobs[1], &knobs[2], &knobs[3], &knobs[4] }, 4);
    r.removeFromBottom (6);
    graph.setBounds (r);
}

EnvSection::EnvSection (APVTS& s) : Panel ("ENVELOPES")
{
    for (int i = 0; i < Params::numEnvs; ++i)
    {
        pages.push_back (std::make_unique<Page> (s, i));
        addChildComponent (*pages.back());
    }
    pages.front()->setVisible (true);

    tabs.onChange = [this] (int index)
    {
        for (size_t i = 0; i < pages.size(); ++i)
            pages[i]->setVisible ((int) i == index);
    };
    addAndMakeVisible (tabs);
}

void EnvSection::resized()
{
    tabs.setBounds (getHeaderArea().removeFromRight (210));
    for (auto& p : pages)
        p->setBounds (getContentArea());
}

// ============================================================================
LfoSection::Page::Page (APVTS& s, int index)
    : preview (s, index),
      shape (s, Params::lfo (index, "shape")),
      mode (s, Params::lfo (index, "mode")),
      division (s, Params::lfo (index, "div")),
      sync (s, Params::lfo (index, "sync"), "SYNC"),
      rate (s, Params::lfo (index, "rate"), "RATE"),
      syncParam (s.getRawParameterValue (Params::lfo (index, "sync")))
{
    for (auto* c : std::initializer_list<Component*> { &preview, &shape, &mode, &division, &sync, &rate,
                                                       &shapeLabel, &modeLabel })
        addAndMakeVisible (c);

    lookAndFeelChanged();
    timerCallback();
    startTimerHz (10);
}

void LfoSection::Page::lookAndFeelChanged()
{
    styleCaption (shapeLabel, "SHAPE");
    styleCaption (modeLabel, "MODE");
}

void LfoSection::Page::timerCallback()
{
    const bool synced = Params::getb (syncParam);
    rate.setVisible (! synced);
    division.setVisible (synced);
}

void LfoSection::Page::resized()
{
    auto r = getLocalBounds();
    auto controls = r.removeFromRight (170);
    r.removeFromRight (8);
    preview.setBounds (r);

    auto left = controls.removeFromLeft (100);
    controls.removeFromLeft (6);
    shapeLabel.setBounds (left.removeFromTop (14));
    shape.setBounds (left.removeFromTop (22));
    left.removeFromTop (6);
    modeLabel.setBounds (left.removeFromTop (14));
    mode.setBounds (left.removeFromTop (22));
    left.removeFromTop (8);
    sync.setBounds (left.removeFromTop (22));

    auto right = controls;
    rate.setBounds (right.removeFromTop (64));
    division.setBounds (rate.getBounds().withSizeKeepingCentre (right.getWidth(), 22));
}

LfoSection::LfoSection (APVTS& s) : Panel ("LFO")
{
    for (int i = 0; i < Params::numLfos; ++i)
    {
        pages.push_back (std::make_unique<Page> (s, i));
        addChildComponent (*pages.back());
    }
    pages.front()->setVisible (true);

    tabs.onChange = [this] (int index)
    {
        for (size_t i = 0; i < pages.size(); ++i)
            pages[i]->setVisible ((int) i == index);
    };
    addAndMakeVisible (tabs);
}

void LfoSection::resized()
{
    tabs.setBounds (getHeaderArea().removeFromRight (210));
    for (auto& p : pages)
        p->setBounds (getContentArea());
}

// ============================================================================
OscPage::OscPage (MeerumProcessor& p)
    : sub (p.apvts), noise (p.apvts), oscA (p, 0), oscB (p, 1), filter (p.apvts), envs (p.apvts), lfos (p.apvts)
{
    for (auto* c : std::initializer_list<Component*> { &sub, &noise, &oscA, &oscB, &filter, &envs, &lfos })
        addAndMakeVisible (c);
}

void OscPage::resized()
{
    constexpr int gap = 6;
    auto r = getLocalBounds();
    auto top = r.removeFromTop (r.getHeight() * 58 / 100);
    r.removeFromTop (gap);

    auto left = top.removeFromLeft (164);
    top.removeFromLeft (gap);
    sub.setBounds (left.removeFromTop ((left.getHeight() - gap) / 2));
    left.removeFromTop (gap);
    noise.setBounds (left);

    filter.setBounds (top.removeFromRight (206));
    top.removeFromRight (gap);
    oscA.setBounds (top.removeFromLeft ((top.getWidth() - gap) / 2));
    top.removeFromLeft (gap);
    oscB.setBounds (top);

    envs.setBounds (r.removeFromLeft ((r.getWidth() - gap) / 2));
    r.removeFromLeft (gap);
    lfos.setBounds (r);
}

// ============================================================================
FxPage::FxPanel::FxPanel (APVTS& s, const String& t, const String& powerID)
    : Panel (t), power (s, powerID)
{
    addAndMakeVisible (power);
}

FxPage::FxPage (APVTS& s)
    : dist (s, "DISTORTION", "dist_on"), chorus (s, "CHORUS", "cho_on"),
      delay (s, "DELAY", "dly_on"), reverb (s, "REVERB", "rev_on"),
      distType (s, "dist_type"), delayDiv (s, "dly_div"),
      delaySync (s, "dly_sync", "SYNC"), delayPing (s, "dly_ping", "PING-PONG"),
      distKnobs (s, dist, { { "dist_drive", "DRIVE" }, { "dist_mix", "MIX" } }),
      chorusKnobs (s, chorus, { { "cho_rate", "RATE" }, { "cho_depth", "DEPTH" }, { "cho_mix", "MIX" } }),
      delayKnobs (s, delay, { { "dly_time", "TIME" }, { "dly_fb", "FEEDBACK" }, { "dly_mix", "MIX" } }),
      reverbKnobs (s, reverb, { { "rev_size", "SIZE" }, { "rev_damp", "DAMPING" }, { "rev_width", "WIDTH" },
                                { "rev_mix", "MIX" } }),
      delaySyncParam (s.getRawParameterValue ("dly_sync"))
{
    for (auto* c : std::initializer_list<Component*> { &dist, &chorus, &delay, &reverb })
        addAndMakeVisible (c);

    dist.addAndMakeVisible (distType);
    delay.addAndMakeVisible (delaySync);
    delay.addAndMakeVisible (delayPing);
    delay.addChildComponent (delayDiv);

    timerCallback();
    startTimerHz (10);
}

void FxPage::timerCallback()
{
    const bool synced = Params::getb (delaySyncParam);
    delayKnobs[0].setVisible (! synced);
    delayDiv.setVisible (synced);
}

void FxPage::resized()
{
    constexpr int gap = 6;
    auto r = getLocalBounds();
    auto top = r.removeFromTop ((r.getHeight() - gap) / 2);
    r.removeFromTop (gap);

    dist.setBounds (top.removeFromLeft ((top.getWidth() - gap) / 2));
    top.removeFromLeft (gap);
    chorus.setBounds (top);
    delay.setBounds (r.removeFromLeft ((r.getWidth() - gap) / 2));
    r.removeFromLeft (gap);
    reverb.setBounds (r);

    for (auto* p : { &dist, &chorus, &delay, &reverb })
        p->power.setBounds (p->getHeaderArea().removeFromLeft (20));

    auto knobArea = [] (Panel& p, int count)
    {
        return p.getContentArea().withSizeKeepingCentre (jmin (p.getContentArea().getWidth(), count * 90), 84);
    };

    {
        auto c = dist.getContentArea();
        distType.setBounds (c.removeFromTop (24).withSizeKeepingCentre (160, 22));
        layoutRow (knobArea (dist, 2), { &distKnobs[0], &distKnobs[1] }, 8);
    }

    layoutRow (knobArea (chorus, 3), { &chorusKnobs[0], &chorusKnobs[1], &chorusKnobs[2] }, 8);

    {
        auto c = delay.getContentArea();
        auto toggles = c.removeFromTop (24).withSizeKeepingCentre (200, 22);
        delaySync.setBounds (toggles.removeFromLeft (70));
        toggles.removeFromLeft (8);
        delayPing.setBounds (toggles);
        layoutRow (knobArea (delay, 3), { &delayKnobs[0], &delayKnobs[1], &delayKnobs[2] }, 8);
        delayDiv.setBounds (delayKnobs[0].getBounds().withSizeKeepingCentre (delayKnobs[0].getWidth(), 22));
    }

    layoutRow (knobArea (reverb, 4), { &reverbKnobs[0], &reverbKnobs[1], &reverbKnobs[2], &reverbKnobs[3] }, 8);
}

// ============================================================================
MatrixPage::Row::Row (APVTS& s, int slot)
    : source (s, Params::mod (slot, "src")),
      destination (s, Params::mod (slot, "dst")),
      amount (s, Params::mod (slot, "amt"))
{
    index.setText (String (slot + 1), dontSendNotification);
    index.setFont (Theme::font (13.0f, true));
    index.setColour (Label::textColourId, Theme::accent);
    index.setJustificationType (Justification::centred);
}

MatrixPage::MatrixPage (APVTS& s) : Panel ("MOD MATRIX")
{
    for (int i = 0; i < Params::numModSlots; ++i)
    {
        rows.push_back (std::make_unique<Row> (s, i));
        auto& row = *rows.back();
        for (auto* c : std::initializer_list<Component*> { &row.index, &row.source, &row.destination, &row.amount })
            addAndMakeVisible (c);
    }
}

namespace
{
    struct MatrixColumns
    {
        Rectangle<int> index, source, destination, amount;
    };

    MatrixColumns splitColumns (Rectangle<int> row)
    {
        MatrixColumns c;
        c.index = row.removeFromLeft (40);
        row.removeFromLeft (8);
        c.source = row.removeFromLeft (220);
        row.removeFromLeft (16);
        c.destination = row.removeFromLeft (220);
        row.removeFromLeft (24);
        c.amount = row;
        return c;
    }
}

void MatrixPage::lookAndFeelChanged()
{
    for (auto& row : rows)
        row->index.setColour (Label::textColourId, Theme::accent);
}

void MatrixPage::paint (Graphics& g)
{
    Panel::paint (g);

    auto r = getContentArea().reduced (10, 4);
    const auto header = splitColumns (r.removeFromTop (20));
    g.setFont (Theme::font (11.0f, true));
    g.setColour (Theme::textDim);
    g.drawText ("#",           header.index,       Justification::centred);
    g.drawText ("SOURCE",      header.source,      Justification::centredLeft);
    g.drawText ("DESTINATION", header.destination, Justification::centredLeft);
    g.drawText ("AMOUNT",      header.amount,      Justification::centredLeft);

    // Zebra striping behind the rows.
    r.removeFromTop (6);
    const int rowHeight = r.getHeight() / Params::numModSlots;
    for (int i = 0; i < Params::numModSlots; ++i)
        if (i % 2 == 0)
        {
            g.setColour (Theme::panelHeader.withAlpha (0.5f));
            g.fillRoundedRectangle (r.withY (r.getY() + i * rowHeight).withHeight (rowHeight).toFloat(), 4.0f);
        }
}

void MatrixPage::resized()
{
    auto r = getContentArea().reduced (10, 4);
    r.removeFromTop (26);
    const int rowHeight = r.getHeight() / Params::numModSlots;

    for (auto& row : rows)
    {
        const auto c = splitColumns (r.removeFromTop (rowHeight).reduced (0, (rowHeight - 26) / 2));
        row->index.setBounds (c.index);
        row->source.setBounds (c.source);
        row->destination.setBounds (c.destination);
        row->amount.setBounds (c.amount.withTrimmedRight (8));
    }
}
