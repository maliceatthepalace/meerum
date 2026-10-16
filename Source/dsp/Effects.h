#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Parameters.h"

// Global stereo effects, in series: distortion -> chorus -> delay -> reverb.
class FxChain
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Params::ParamCache& p, double bpm);

private:
    void processDistortion (juce::AudioBuffer<float>& buffer, const Params::ParamCache& p);
    void processDelay (juce::AudioBuffer<float>& buffer, const Params::ParamCache& p, double bpm);

    double sampleRate = 44100.0;

    // Distortion
    juce::SmoothedValue<float> distDrive, distMix;
    float crushHeld[2] {};
    float crushCounter = 0.0f;

    juce::dsp::Chorus<float> chorus;

    // Delay
    juce::AudioBuffer<float> delayBuffer;
    int delayWrite = 0;
    juce::SmoothedValue<float> delaySamples, delayMix;
    float delayTone[2] {};
    bool delayWasOn = false;

    juce::Reverb reverb;
    juce::SmoothedValue<float> reverbMix;
};
