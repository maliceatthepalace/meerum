#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Theme
{
    const juce::Colour background   { 0xff101318 };
    const juce::Colour panel        { 0xff1a1f27 };
    const juce::Colour panelHeader  { 0xff232a35 };
    const juce::Colour outline      { 0xff2f3743 };
    const juce::Colour well         { 0xff0b0e12 };
    const juce::Colour accent       { 0xff3fd0e0 };
    const juce::Colour accentDim    { 0xff1d6d78 };
    const juce::Colour text         { 0xffd8dee7 };
    const juce::Colour textDim      { 0xff8791a0 };

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }
}

class MeerumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MeerumLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos,
                           float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override              { return Theme::font (12.0f); }
    juce::Font getPopupMenuFont() override                              { return Theme::font (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override      { return Theme::font (12.0f, true); }
    juce::Font getSliderPopupFont (juce::Slider&) override              { return Theme::font (12.0f); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};
