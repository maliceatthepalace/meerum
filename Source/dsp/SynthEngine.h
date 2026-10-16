#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Voice.h"

// Voice allocation, MIDI handling and parameter snapshotting.
class SynthEngine
{
public:
    void prepare (double sampleRate);
    void reset();

    // Renders voices (replacing the buffer's content) for the given MIDI.
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi,
                  const Params::ParamCache& params, const Wavetable* const* tables, double bpm);

    std::atomic<float> displayedPosition[Params::numOscs] {};

private:
    void takeSnapshot (const Params::ParamCache& p, const Wavetable* const* tables, double bpm);
    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void releaseSustained();
    Voice* findVoiceToUse();
    int activeVoiceLimit() const noexcept { return juce::jlimit (1, Params::maxVoices, polyphony); }

    double sampleRate = 44100.0;
    Voice voices[Params::maxVoices];
    SynthSnapshot snap;
    juce::uint64 voiceCounter = 0;
    int polyphony = 8, bendRange = 2;
    bool legato = false;

    // Mono mode note stack, most recent last.
    int heldNotes[128] {};
    int numHeld = 0;

    bool sustainDown = false;
    float modWheel = 0.0f, aftertouch = 0.0f, pitchBend = 0.0f; // pitchBend in -1..1
    double lastNote = -1.0;
    double freeLfoPhase[Params::numLfos] {};
    Voice* lastStarted = nullptr;
};
