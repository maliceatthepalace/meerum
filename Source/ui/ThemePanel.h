#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "Theme.h"

// Pop-up panel for the user's look: dark/light mode, accent swatches and a free colour picker.
// Changes apply live (via Theme::apply) and are saved to the user settings shortly after.
class ThemePanel : public juce::Component,
                   private juce::ChangeListener,
                   private juce::Timer
{
public:
    ThemePanel();
    ~ThemePanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Swatch : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Swatch (const juce::String& name, juce::Colour c, std::function<void()> onPick);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override  { if (onPick) onPick(); }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }
        juce::Colour colour;
    private:
        std::function<void()> onPick;
    };

    void update (const Theme::Settings& s, bool saveNow);
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    juce::TextButton darkButton { "DARK" }, lightButton { "LIGHT" }, resetButton { "RESET" };
    std::vector<std::unique_ptr<Swatch>> swatches;
    juce::ColourSelector picker { juce::ColourSelector::showColourspace };
};
