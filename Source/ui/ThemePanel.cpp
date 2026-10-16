#include "ThemePanel.h"

using namespace juce;

namespace
{
    constexpr int panelWidth = 264, panelHeight = 318, pad = 12;
}

ThemePanel::Swatch::Swatch (const String& name, Colour c, std::function<void()> pick)
    : colour (c), onPick (std::move (pick))
{
    setTooltip (name);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void ThemePanel::Swatch::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (colour);
    g.fillEllipse (r);

    if (Theme::current().accent == colour)
    {
        g.setColour (Theme::text);
        g.drawEllipse (r.expanded (1.0f), 2.0f);
    }
    else if (isMouseOver())
    {
        g.setColour (Theme::textDim);
        g.drawEllipse (r, 1.0f);
    }
}

ThemePanel::ThemePanel()
{
    for (auto* b : { &darkButton, &lightButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);
        addAndMakeVisible (b);
    }
    darkButton.setToggleState (! Theme::current().light, dontSendNotification);
    lightButton.setToggleState (Theme::current().light, dontSendNotification);
    darkButton.onClick  = [this] { auto s = Theme::current(); s.light = false; update (s, true); };
    lightButton.onClick = [this] { auto s = Theme::current(); s.light = true;  update (s, true); };

    for (const auto& [name, colour] : Theme::accentPresets())
    {
        const auto c = colour;
        swatches.push_back (std::make_unique<Swatch> (name, c, [this, c]
        {
            auto s = Theme::current();
            s.accent = c;
            picker.setCurrentColour (c, dontSendNotification);
            update (s, true);
        }));
        addAndMakeVisible (*swatches.back());
    }

    picker.setCurrentColour (Theme::current().accent, dontSendNotification);
    picker.setColour (ColourSelector::backgroundColourId, Colours::transparentBlack);
    picker.addChangeListener (this);
    addAndMakeVisible (picker);

    resetButton.setTooltip ("Back to dark mode with the default green accent");
    resetButton.onClick = [this]
    {
        const Theme::Settings defaults;
        picker.setCurrentColour (defaults.accent, dontSendNotification);
        darkButton.setToggleState (true, dontSendNotification);
        update (defaults, true);
    };
    addAndMakeVisible (resetButton);

    setSize (panelWidth, panelHeight);
}

ThemePanel::~ThemePanel()
{
    picker.removeChangeListener (this);
    if (isTimerRunning())
        timerCallback(); // don't lose a pending save
}

void ThemePanel::update (const Theme::Settings& s, bool saveNow)
{
    if (saveNow)
        Theme::setAndSave (s);
    else
    {
        Theme::apply (s);
        startTimer (400); // dragging in the picker: save once it settles
    }

    for (auto& sw : swatches)
        sw->repaint();
    repaint();
}

void ThemePanel::changeListenerCallback (ChangeBroadcaster*)
{
    auto s = Theme::current();
    s.accent = picker.getCurrentColour().withAlpha (1.0f);
    update (s, false);
}

void ThemePanel::timerCallback()
{
    stopTimer();
    Theme::setAndSave (Theme::current());
}

void ThemePanel::paint (Graphics& g)
{
    g.setFont (Theme::font (11.0f, true));
    g.setColour (Theme::textDim);
    g.drawText ("MODE",   pad, pad,       200, 14, Justification::centredLeft);
    g.drawText ("ACCENT", pad, pad + 54,  200, 14, Justification::centredLeft);
    g.drawText ("CUSTOM", pad, pad + 132, 200, 14, Justification::centredLeft);
}

void ThemePanel::resized()
{
    auto r = getLocalBounds().reduced (pad);

    r.removeFromTop (18);
    auto modeRow = r.removeFromTop (26);
    darkButton.setBounds (modeRow.removeFromLeft (modeRow.getWidth() / 2 - 3));
    modeRow.removeFromLeft (6);
    lightButton.setBounds (modeRow);

    r.removeFromTop (28);
    const int size = r.getWidth() / (int) (swatches.size() / 2);
    for (size_t i = 0; i < swatches.size(); ++i)
    {
        const int row = (int) i / (int) (swatches.size() / 2), col = (int) i % (int) (swatches.size() / 2);
        swatches[i]->setBounds (r.getX() + col * size, r.getY() + row * 30, juce::jmin (size, 28), 28);
    }

    r.removeFromTop (76);
    resetButton.setBounds (r.removeFromBottom (24).removeFromRight (70));
    r.removeFromBottom (6);
    picker.setBounds (r);
}
