#pragma once

#include "Widgets.h"

class MeerumProcessor;
class Wavetable;

// Pseudo-3D stack of the oscillator's wavetable frames; the frame at the current
// (modulated) position is drawn in the accent colour. Accepts dropped .wav files.
class WavetableDisplay : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    WavetableDisplay (MeerumProcessor& processor, int oscIndex);

    void paint (juce::Graphics&) override;
    void resized() override                       { cachedTable = nullptr; }

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true;  repaint(); }
    void fileDragExit (const juce::StringArray&) override            { dragHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

    std::function<void (const juce::File&)> onFileDropped;

private:
    struct Geometry
    {
        juce::Rectangle<float> area;
        float dx, dy, waveWidth, amplitude;
        juce::Point<float> origin (float depth) const;  // left/centre point of a frame at depth 0..1
    };

    void timerCallback() override;
    void rebuildStack();
    Geometry geometry() const;
    juce::Path framePath (const Geometry&, float framePos, float depth) const;

    MeerumProcessor& processor;
    const int osc;

    std::shared_ptr<const Wavetable> table;
    const Wavetable* cachedTable = nullptr;
    juce::Path stack;
    float position = -1.0f;
    bool dragHover = false;
};

// AHDSR shape preview for one envelope.
class EnvelopeGraph : public juce::Component, private juce::Timer
{
public:
    EnvelopeGraph (APVTS& state, int envIndex);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::array<std::atomic<float>*, 5> params;
    std::array<float, 5> last {};
};

// One-cycle-and-a-bit preview of an LFO shape.
class LfoPreview : public juce::Component, private juce::Timer
{
public:
    LfoPreview (APVTS& state, int lfoIndex);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::atomic<float>* shape;
    int lastShape = -1;
};

// Magnitude response of the filter, on a log frequency axis.
class FilterCurve : public juce::Component, private juce::Timer
{
public:
    explicit FilterCurve (APVTS& state);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::atomic<float>* on;
    std::atomic<float>* type;
    std::atomic<float>* cutoff;
    std::atomic<float>* res;
    std::array<float, 4> last {};
};
