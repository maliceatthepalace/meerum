#pragma once

#include "PluginProcessor.h"
#include "ui/Pages.h"

class MeerumEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MeerumEditor (MeerumProcessor&);
    ~MeerumEditor() override;

    void resized() override;

    static constexpr int baseWidth = 1060, baseHeight = 720;

private:
    // Everything is laid out at base size inside this component, which is then scaled.
    struct Content : juce::Component
    {
        void paint (juce::Graphics&) override;
    };

    struct PresetEntry
    {
        juce::String name;
        int program = -1;      // factory program index, or -1 for a user preset file
        juce::File file;
    };

    void timerCallback() override;
    void layoutContent();
    void refreshPresetList();
    void selectPreset (int index);
    void stepPreset (int delta);
    void savePresetAs();
    void showPage (int index);

    MeerumProcessor& meerumProcessor;
    MeerumLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { nullptr, 600 };
    Content content;

    // Top bar
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" };
    KnobSet globalKnobs;
    Toggle legato;
    std::vector<PresetEntry> presets;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastGeneration = -1;

    TabBar pageTabs { { "OSC", "FX", "MATRIX" } };
    OscPage oscPage;
    FxPage fxPage;
    MatrixPage matrixPage;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeerumEditor)
};
