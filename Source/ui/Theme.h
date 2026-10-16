#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Theme
{
    // The active palette. Mutable so the user can switch dark/light mode and the accent colour
    // at runtime; only touched on the message thread, shared by all open editors.
    inline juce::Colour background, panel, panelHeader, outline, well, text, textDim;
    inline juce::Colour accent, accentDim, accentAlt, onAccent; // onAccent = text drawn on top of accent
    inline juce::Colour blackKey;

    const juce::Colour defaultAccent { 0xff3ddc84 }; // Meerum green

    struct Settings
    {
        bool light = false;
        juce::Colour accent = defaultAccent;
    };

    // Accent swatches offered in the theme panel.
    inline const std::vector<std::pair<juce::String, juce::Colour>>& accentPresets()
    {
        static const std::vector<std::pair<juce::String, juce::Colour>> presets {
            { "Green", defaultAccent },          { "Cyan", juce::Colour (0xff3fd0e0) },
            { "Blue", juce::Colour (0xff4c8dff) }, { "Purple", juce::Colour (0xff9b7cff) },
            { "Pink", juce::Colour (0xffff5fa2) }, { "Red", juce::Colour (0xffff5a5a) },
            { "Orange", juce::Colour (0xffff9f43) }, { "Yellow", juce::Colour (0xffffd84d) } };
        return presets;
    }

    // Recomputes the palette and bumps generation() so open editors refresh. Does not save.
    void apply (const Settings&);
    const Settings& current();

    // Loads the saved user settings once per process (first editor) and applies them.
    void initialise();
    // Applies and stores the settings in the user's application-data folder.
    void setAndSave (const Settings&);

    inline std::atomic<int>& generation()
    {
        static std::atomic<int> g { 0 };
        return g;
    }

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }
}

class MeerumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MeerumLookAndFeel();

    // Re-reads the Theme palette into the LookAndFeel colour IDs.
    void refreshColours();

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
