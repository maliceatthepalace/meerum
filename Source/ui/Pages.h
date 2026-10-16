#pragma once

#include "Displays.h"

class MeerumProcessor;

// Row of mutually exclusive tab buttons.
class TabBar : public juce::Component
{
public:
    explicit TabBar (const juce::StringArray& names);
    void resized() override;
    void setSelected (int index);
    int getSelected() const noexcept { return selected; }

    std::function<void (int)> onChange;

private:
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    int selected = 0;
};

// Owns a list of knobs, created from (paramID, caption) pairs.
struct KnobSet
{
    KnobSet (APVTS& state, juce::Component& parent, std::initializer_list<std::pair<juce::String, juce::String>> specs);
    Knob& operator[] (size_t i) { return *knobs[i]; }
    std::vector<std::unique_ptr<Knob>> knobs;
};

// ---------------------------------------------------------------------------
class OscPanel : public Panel
{
public:
    OscPanel (MeerumProcessor& processor, int oscIndex);
    void resized() override;

private:
    void loadWavetable (const juce::File& file);
    void chooseWavetable();

    MeerumProcessor& processor;
    const int osc;
    Toggle power;
    Choice table;
    juce::TextButton loadButton { "WAV" };
    WavetableDisplay display;
    Choice warpMode;
    KnobSet knobs;
    std::unique_ptr<juce::FileChooser> chooser;
};

class SubPanel : public Panel
{
public:
    explicit SubPanel (APVTS& state);
    void resized() override;

private:
    Toggle power;
    Choice shape;
    KnobSet knobs;
};

class NoisePanel : public Panel
{
public:
    explicit NoisePanel (APVTS& state);
    void resized() override;

private:
    Toggle power;
    Choice type;
    KnobSet knobs;
};

class FilterPanel : public Panel
{
public:
    explicit FilterPanel (APVTS& state);
    void resized() override;

private:
    Toggle power;
    Choice type;
    FilterCurve curve;
    KnobSet knobs;
    Toggle routeA, routeB, routeSub, routeNoise;
};

class EnvSection : public Panel
{
public:
    explicit EnvSection (APVTS& state);
    void resized() override;

private:
    struct Page : juce::Component
    {
        Page (APVTS& state, int index);
        void resized() override;
        EnvelopeGraph graph;
        KnobSet knobs;
    };

    TabBar tabs { { "AMP", "ENV 2", "ENV 3" } };
    std::vector<std::unique_ptr<Page>> pages;
};

class LfoSection : public Panel
{
public:
    explicit LfoSection (APVTS& state);
    void resized() override;

private:
    struct Page : juce::Component, private juce::Timer
    {
        Page (APVTS& state, int index);
        void resized() override;
        void timerCallback() override;

        LfoPreview preview;
        Choice shape, mode, division;
        Toggle sync;
        Knob rate;
        juce::Label shapeLabel, modeLabel;
        std::atomic<float>* syncParam;
    };

    TabBar tabs { { "LFO 1", "LFO 2", "LFO 3" } };
    std::vector<std::unique_ptr<Page>> pages;
};

// ---------------------------------------------------------------------------
class OscPage : public juce::Component
{
public:
    explicit OscPage (MeerumProcessor& processor);
    void resized() override;

private:
    SubPanel sub;
    NoisePanel noise;
    OscPanel oscA, oscB;
    FilterPanel filter;
    EnvSection envs;
    LfoSection lfos;
};

class FxPage : public juce::Component, private juce::Timer
{
public:
    explicit FxPage (APVTS& state);
    void resized() override;

private:
    void timerCallback() override;

    struct FxPanel : Panel
    {
        FxPanel (APVTS& state, const juce::String& title, const juce::String& powerID);
        Toggle power;
    };

    FxPanel dist, chorus, delay, reverb;
    Choice distType, delayDiv;
    Toggle delaySync, delayPing;
    KnobSet distKnobs, chorusKnobs, delayKnobs, reverbKnobs;
    std::atomic<float>* delaySyncParam;
};

class MatrixPage : public Panel
{
public:
    explicit MatrixPage (APVTS& state);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        Row (APVTS& state, int slot);
        juce::Label index;
        Choice source, destination;
        HSlider amount;
    };

    std::vector<std::unique_ptr<Row>> rows;
};
