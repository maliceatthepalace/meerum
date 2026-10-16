#include "Theme.h"

using namespace juce;

namespace Theme
{
namespace
{
    Settings settings;
    bool initialised = false;

    PropertiesFile::Options fileOptions()
    {
        PropertiesFile::Options o;
        o.applicationName = "Meerum";
        o.folderName = "Meerum";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }
}

void apply (const Settings& s)
{
    settings = s;

    if (s.light)
    {
        background  = Colour (0xffe6e9ee);
        panel       = Colour (0xfff7f8fa);
        panelHeader = Colour (0xffdfe3e9);
        outline     = Colour (0xffc3c9d2);
        well        = Colour (0xffeef0f3);
        text        = Colour (0xff1c2129);
        textDim     = Colour (0xff5c6572);
        blackKey    = Colour (0xff2a2f37);
    }
    else
    {
        background  = Colour (0xff101318);
        panel       = Colour (0xff1a1f27);
        panelHeader = Colour (0xff232a35);
        outline     = Colour (0xff2f3743);
        well        = Colour (0xff0b0e12);
        text        = Colour (0xffd8dee7);
        textDim     = Colour (0xff8791a0);
        blackKey    = well;
    }

    // In light mode very bright accents (e.g. yellow) would vanish on white, so darken them a bit.
    accent    = s.light && s.accent.getPerceivedBrightness() > 0.75f ? s.accent.darker (0.35f) : s.accent;
    accentDim = accent.interpolatedWith (panel, 0.55f);
    accentAlt = accent.withRotatedHue (0.12f);
    onAccent  = accent.getPerceivedBrightness() > 0.6f ? Colour (0xff101318) : Colours::white;

    ++generation();
}

const Settings& current() { return settings; }

void initialise()
{
    if (initialised)
        return;
    initialised = true;

    PropertiesFile file (fileOptions());
    Settings s;
    s.light = file.getBoolValue ("lightMode", false);
    s.accent = Colour::fromString (file.getValue ("accent", defaultAccent.toString())).withAlpha (1.0f);
    apply (s);
}

void setAndSave (const Settings& s)
{
    apply (s);
    PropertiesFile file (fileOptions());
    file.setValue ("lightMode", s.light);
    file.setValue ("accent", s.accent.toString());
    file.saveIfNeeded();
}
}

MeerumLookAndFeel::MeerumLookAndFeel()
{
    Theme::initialise();
    refreshColours();
}

void MeerumLookAndFeel::refreshColours()
{
    setColour (ResizableWindow::backgroundColourId, Theme::background);
    setColour (ComboBox::backgroundColourId,        Theme::well);
    setColour (ComboBox::outlineColourId,           Theme::outline);
    setColour (ComboBox::textColourId,              Theme::text);
    setColour (ComboBox::arrowColourId,             Theme::accent);
    setColour (PopupMenu::backgroundColourId,       Theme::panel);
    setColour (PopupMenu::textColourId,             Theme::text);
    setColour (PopupMenu::headerTextColourId,       Theme::accent);
    setColour (PopupMenu::highlightedBackgroundColourId, Theme::accentDim);
    setColour (PopupMenu::highlightedTextColourId,  Theme::text);
    setColour (Label::textColourId,                 Theme::text);
    setColour (TextButton::buttonColourId,          Theme::panelHeader);
    setColour (TextButton::buttonOnColourId,        Theme::accent);
    setColour (TextButton::textColourOffId,         Theme::textDim);
    setColour (TextButton::textColourOnId,          Theme::onAccent);
    setColour (Slider::textBoxTextColourId,         Theme::text);
    setColour (Slider::textBoxOutlineColourId,      Colours::transparentBlack);
    setColour (TooltipWindow::backgroundColourId,   Theme::panelHeader);
    setColour (BubbleComponent::backgroundColourId, Theme::panelHeader);
    setColour (BubbleComponent::outlineColourId,    Theme::accentDim);
    setColour (AlertWindow::backgroundColourId,     Theme::panel);
    setColour (AlertWindow::textColourId,           Theme::text);
}

void MeerumLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, Slider& s)
{
    const auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float arcRadius = radius - 2.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Bipolar ranges (e.g. pan) draw their value arc from the centre.
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float fromAngle = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                                    : startAngle;

    Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::well);
    g.strokePath (track, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    const bool enabled = s.isEnabled();
    Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, fromAngle, angle, true);
    g.setColour (enabled ? Theme::accent : Theme::textDim);
    g.strokePath (value, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    const float knobRadius = arcRadius - 5.0f;
    g.setGradientFill (ColourGradient (Theme::panelHeader.brighter (0.25f), centre.x, centre.y - knobRadius,
                                       Theme::panelHeader.darker (0.4f), centre.x, centre.y + knobRadius, false));
    g.fillEllipse (Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre));
    g.setColour (Theme::outline);
    g.drawEllipse (Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre), 1.0f);

    const auto tip = centre.getPointOnCircumference (knobRadius - 2.0f, angle);
    const auto inner = centre.getPointOnCircumference (knobRadius * 0.35f, angle);
    g.setColour (enabled ? Theme::text : Theme::textDim);
    g.drawLine ({ inner, tip }, 2.0f);
}

void MeerumLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float,
                                          float, Slider::SliderStyle style, Slider& s)
{
    if (style != Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, s);
        return;
    }

    const auto r = Rectangle<int> (x, y, w, h).toFloat();
    const auto track = r.withSizeKeepingCentre (r.getWidth(), 6.0f);
    g.setColour (Theme::well);
    g.fillRoundedRectangle (track, 3.0f);

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroX = bipolar ? r.getX() + (float) s.valueToProportionOfLength (0.0) * r.getWidth() : r.getX();
    const float left = jmin (zeroX, sliderPos), right = jmax (zeroX, sliderPos);
    g.setColour (Theme::accent);
    g.fillRoundedRectangle (track.withLeft (left).withRight (right), 3.0f);

    if (bipolar)
    {
        g.setColour (Theme::textDim);
        g.fillRect (Rectangle<float> (1.0f, 12.0f).withCentre ({ zeroX, r.getCentreY() }));
    }

    g.setColour (Theme::text);
    g.fillEllipse (Rectangle<float> (12.0f, 12.0f).withCentre ({ sliderPos, r.getCentreY() }));
}

void MeerumLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    if (b.getButtonText().isEmpty())
    {
        // Power LED style.
        const auto led = Rectangle<float> (12.0f, 12.0f).withCentre (r.getCentre());
        g.setColour (on ? Theme::accent : Theme::well);
        g.fillEllipse (led);
        g.setColour (on ? Theme::accent.brighter (0.4f) : (highlighted ? Theme::textDim : Theme::outline));
        g.drawEllipse (led, 1.2f);
        return;
    }

    g.setColour (on ? Theme::accent : (highlighted ? Theme::panelHeader.brighter (0.15f) : Theme::well));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Theme::outline);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    g.setColour (on ? Theme::onAccent : Theme::textDim);
    g.setFont (Theme::font (11.0f, true));
    g.drawText (b.getButtonText(), r, Justification::centred, false);
}

void MeerumLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? Theme::accent : Theme::panelHeader;
    if (highlighted) c = c.brighter (0.1f);
    if (down)        c = c.darker (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Theme::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void MeerumLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? Theme::onAccent : Theme::text);
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
}

void MeerumLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto r = Rectangle<int> (w, h).toFloat().reduced (0.5f);
    g.setColour (Theme::well);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.hasKeyboardFocus (true) ? Theme::accentDim : Theme::outline);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    Path arrow;
    const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.isEnabled() ? Theme::accent : Theme::textDim);
    g.fillPath (arrow);
}

void MeerumLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

Label* MeerumLookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (Theme::font (11.0f));
    l->setColour (Label::textColourId, Theme::text);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    return l;
}
