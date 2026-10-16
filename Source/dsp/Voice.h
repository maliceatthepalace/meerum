#pragma once

#include "../Parameters.h"
#include "Modulators.h"
#include "Wavetable.h"

// Plain-value copy of all parameters, taken once per audio block so voices never
// touch the atomics and all voices see a consistent state.
struct SynthSnapshot
{
    struct Osc
    {
        bool on = false;
        const Wavetable* table = nullptr;
        int oct = 0, semi = 0, unison = 1, warp = 0;
        float fine = 0, pos = 0, detune = 0, width = 0, pan = 0, level = 0, phase = 0, rand = 0, warpAmt = 0;
    };
    struct Env  { float a, h, d, s, r; };
    struct Lfo  { int shape, mode; double hz; };
    struct Slot { int src, dst; float amt; };

    Osc osc[Params::numOscs];
    bool subOn = false;   int subShape = 0, subOct = -1;  float subLevel = 0, subPan = 0;
    bool noiseOn = false; int noiseType = 0;              float noiseLevel = 0, noisePan = 0;
    bool fltOn = false;   int fltType = 0;
    float cutoff = 20000, res = 0, drive = 0, keytrack = 0, fltMix = 1;
    bool route[4] {};
    Env env[Params::numEnvs] {};
    Lfo lfo[Params::numLfos] {};
    Slot slots[Params::numModSlots] {};
    int numSlots = 0;
    float glide = 0, bendSemis = 0, modWheel = 0, aftertouch = 0;
    double sampleRate = 44100.0;
};

class Voice
{
public:
    static constexpr int controlBlock = 32; // modulation is evaluated every this many samples

    void prepare (double sampleRate);

    // fromNote < 0 means no glide.
    void start (int note, float velocity, double fromNote, const SynthSnapshot& s, const double* freeLfoPhases);
    void glideTo (int note)                       { midiNote = note; targetNote = note; }
    void release();
    void kill();

    // Adds into out[0..1] from startSample for numSamples.
    void render (float* outL, float* outR, int startSample, int numSamples, const SynthSnapshot& s);

    bool isActive() const noexcept                { return env[0].isActive(); }
    bool isGateOn() const noexcept                { return gate; }
    int getNote() const noexcept                  { return midiNote; }
    double getCurrentNote() const noexcept        { return glideNote; }

    juce::uint64 age = 0;       // start order, for voice stealing
    bool sustained = false;     // note-off arrived while the sustain pedal was down
    float displayedPos[Params::numOscs] {};

private:
    struct OscState
    {
        double phase[Params::maxUnison] {};
        float gainL[Params::maxUnison] {}, gainR[Params::maxUnison] {};
        float framePos = 0.0f;
        int lastUnison = 0;
    };

    void renderChunk (float* outL, float* outR, int numSamples, const SynthSnapshot& s);
    void renderOsc (int index, const float* mods, float* busL, float* busR, int n, const SynthSnapshot& s);

    double sampleRate = 44100.0;
    int midiNote = -1;
    double targetNote = 60.0, glideNote = 60.0;
    float velocity = 0.0f, noteRandom = 0.0f;
    bool gate = false;

    Envelope env[Params::numEnvs];
    Lfo lfo[Params::numLfos];
    OscState osc[Params::numOscs];
    double subPhase = 0.0;
    float pink[3] {};
    float prevSubL = 0.0f, prevSubR = 0.0f, prevNoiseL = 0.0f, prevNoiseR = 0.0f, prevAmpMod = 1.0f;
    SvfFilter filter;
    juce::Random rng;
};
