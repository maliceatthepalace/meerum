#include "Widgets.h"

using namespace juce;

Knob::Knob (APVTS& state, const String& paramID, const String& caption)
    : captionText (caption),
      attachment (state, paramID, slider)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setVelocityBasedMode (false);
    slider.setDoubleClickReturnValue (true, (double) state.getParameter (paramID)->convertFrom0to1 (
                                                state.getParameter (paramID)->getDefaultValue()));
    slider.onHover = [this] (bool h) { hovering = h; updateCaption(); };
    slider.onDragStart = [this] { updateCaption(); };
    slider.onDragEnd = [this] { updateCaption(); };
    slider.onValueChange = [this] { if (hovering || slider.isMouseButtonDown()) updateCaption(); };
    addAndMakeVisible (slider);

    label.setJustificationType (Justification::centred);
    label.setFont (Theme::font (10.5f));
    label.setInterceptsMouseClicks (false, false);
    label.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (label);
    updateCaption();
}

void Knob::updateCaption()
{
    const bool showValue = hovering || slider.isMouseButtonDown();
    label.setText (showValue ? slider.getTextFromValue (slider.getValue()) : captionText, dontSendNotification);
    label.setColour (Label::textColourId, showValue ? Theme::accent : Theme::textDim);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromBottom (14));
    slider.setBounds (r);
}

Toggle::Toggle (APVTS& state, const String& paramID, const String& text)
    : attachment (state, paramID, *this)
{
    setButtonText (text);
}

Choice::Choice (APVTS& state, const String& paramID)
{
    if (auto* p = dynamic_cast<AudioParameterChoice*> (state.getParameter (paramID)))
        addItemList (p->choices, 1);
    setJustificationType (Justification::centredLeft);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramID, *this);
}

HSlider::HSlider (APVTS& state, const String& paramID)
    : attachment (state, paramID, *this)
{
    setSliderStyle (Slider::LinearHorizontal);
    setTextBoxStyle (Slider::TextBoxRight, false, 52, 18);
    setDoubleClickReturnValue (true, 0.0);
}

Panel::Panel (String t) : title (std::move (t)) {}

void Panel::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (r, 6.0f);

    auto header = r.withHeight ((float) headerHeight);
    Path hp;
    hp.addRoundedRectangle (header.getX(), header.getY(), header.getWidth(), header.getHeight(), 6.0f, 6.0f,
                            true, true, false, false);
    g.setColour (Theme::panelHeader);
    g.fillPath (hp);

    g.setColour (Theme::outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    g.setColour (Theme::text);
    g.setFont (Theme::font (12.0f, true));
    g.drawText (title, header.reduced (10.0f, 0.0f), Justification::centredLeft, false);
}

Rectangle<int> Panel::getHeaderArea() const
{
    GlyphArrangement glyphs;
    glyphs.addLineOfText (Theme::font (12.0f, true), title, 0.0f, 0.0f);
    const int titleWidth = (int) glyphs.getBoundingBox (0, -1, true).getWidth() + 18;
    return getLocalBounds().removeFromTop (headerHeight).withTrimmedLeft (titleWidth).reduced (4, 3);
}

Rectangle<int> Panel::getContentArea() const
{
    return getLocalBounds().withTrimmedTop (headerHeight).reduced (6);
}

void layoutRow (Rectangle<int> area, std::initializer_list<Component*> items, int gap)
{
    const int n = (int) items.size();
    if (n == 0) return;
    const int w = (area.getWidth() - gap * (n - 1)) / n;
    for (auto* c : items)
    {
        c->setBounds (area.removeFromLeft (w));
        area.removeFromLeft (gap);
    }
}
