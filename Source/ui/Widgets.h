#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

using APVTS = juce::AudioProcessorValueTreeState;

// Rotary knob with a caption underneath; the caption shows the value while hovered or dragged.
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramID, const juce::String& caption);

    void resized() override;
    juce::Slider& getSlider() noexcept { return slider; }

private:
    struct HoverSlider : juce::Slider
    {
        std::function<void (bool)> onHover;
        void mouseEnter (const juce::MouseEvent& e) override { Slider::mouseEnter (e); if (onHover) onHover (true); }
        void mouseExit (const juce::MouseEvent& e) override  { Slider::mouseExit (e);  if (onHover) onHover (false); }
    };

    void updateCaption();

    HoverSlider slider;
    juce::Label label;
    juce::String captionText;
    bool hovering = false;
    APVTS::SliderAttachment attachment;
};

// Toggle bound to a bool parameter. Empty text draws as a power LED.
class Toggle : public juce::ToggleButton
{
public:
    Toggle (APVTS& state, const juce::String& paramID, const juce::String& text = {});

private:
    APVTS::ButtonAttachment attachment;
};

// ComboBox bound to a choice parameter.
class Choice : public juce::ComboBox
{
public:
    Choice (APVTS& state, const juce::String& paramID);

private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// Horizontal slider without text box (mod matrix amounts).
class HSlider : public juce::Slider
{
public:
    HSlider (APVTS& state, const juce::String& paramID);

private:
    APVTS::SliderAttachment attachment;
};

// Rounded panel with a header strip. Subclasses lay out children in getContentArea()
// and can place header widgets in getHeaderArea().
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String title);

    void paint (juce::Graphics&) override;

    static constexpr int headerHeight = 24;

    juce::Rectangle<int> getHeaderArea() const;   // right of the title text
    juce::Rectangle<int> getContentArea() const;

protected:
    juce::String title;
};

// Lays out a row of components with equal widths.
void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap = 4);
