#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Single source of truth for every automatable parameter: IDs, ranges and choice lists.
// Both the DSP (via ParamCache) and the UI (via APVTS attachments) refer to these.
namespace Params
{
    constexpr int numOscs     = 2;   // wavetable oscillators A, B
    constexpr int numEnvs     = 3;   // ENV 1 is hard-wired to amplitude
    constexpr int numLfos     = 3;
    constexpr int numModSlots = 8;
    constexpr int maxVoices   = 16;
    constexpr int maxUnison   = 16;

    // ---- ID helpers -------------------------------------------------------
    inline juce::String osc (int i, const char* name)   { return juce::String (i == 0 ? "oscA_" : "oscB_") + name; }
    inline juce::String env (int i, const char* name)   { return "env" + juce::String (i + 1) + "_" + name; }
    inline juce::String lfo (int i, const char* name)   { return "lfo" + juce::String (i + 1) + "_" + name; }
    inline juce::String mod (int i, const char* name)   { return "mod" + juce::String (i + 1) + "_" + name; }

    // ---- choice lists -----------------------------------------------------
    inline const juce::StringArray& tableNames()
    {
        // The last entry selects the user-imported WAV of that oscillator.
        static const juce::StringArray s { "Basic Shapes", "Pulse Width", "Harmonic Sweep", "Vowels",
                                           "Sync Saw", "FM Bells", "Wavefold", "Digital Steps", "Custom (WAV)" };
        return s;
    }
    inline int customTableIndex() { return tableNames().size() - 1; }

    inline const juce::StringArray& warpModes()
    {
        static const juce::StringArray s { "Off", "Sync", "Bend +", "Bend -", "Mirror", "PWM", "Quantize" };
        return s;
    }
    inline const juce::StringArray& subShapes()     { static const juce::StringArray s { "Sine", "Triangle", "Saw", "Square" }; return s; }
    inline const juce::StringArray& noiseTypes()    { static const juce::StringArray s { "White", "Pink" }; return s; }
    inline const juce::StringArray& filterTypes()   { static const juce::StringArray s { "LP 12", "LP 24", "HP 12", "HP 24", "BP 12", "Notch" }; return s; }
    inline const juce::StringArray& lfoShapes()     { static const juce::StringArray s { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Rnd" }; return s; }
    inline const juce::StringArray& lfoModes()      { static const juce::StringArray s { "Retrig", "Free", "One-shot" }; return s; }
    inline const juce::StringArray& distTypes()     { static const juce::StringArray s { "Soft", "Hard", "Fold", "Crush" }; return s; }

    // Tempo divisions, expressed in quarter-note beats per cycle (see syncBeats()).
    inline const juce::StringArray& syncDivs()
    {
        static const juce::StringArray s { "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32",
                                           "1/4 T", "1/8 T", "1/16 T", "1/4 D", "1/8 D", "1/16 D" };
        return s;
    }
    inline double syncBeats (int index)
    {
        static const double b[] { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                  2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75, 0.375 };
        return b[juce::jlimit (0, (int) std::size (b) - 1, index)];
    }

    enum ModSource { srcNone, srcEnv1, srcEnv2, srcEnv3, srcLfo1, srcLfo2, srcLfo3,
                     srcVelocity, srcNote, srcModWheel, srcAftertouch, srcRandom, numModSources };
    inline const juce::StringArray& modSources()
    {
        static const juce::StringArray s { "None", "Env 1", "Env 2", "Env 3", "LFO 1", "LFO 2", "LFO 3",
                                           "Velocity", "Note", "Mod Wheel", "Aftertouch", "Random" };
        return s;
    }

    // Per-oscillator destinations are laid out as a block so dstOscBase(i) + offset works.
    enum ModDest { dstNone,
                   dstAPos, dstAPitch, dstALevel, dstAPan, dstADetune, dstAWarp,
                   dstBPos, dstBPitch, dstBLevel, dstBPan, dstBDetune, dstBWarp,
                   dstSubLevel, dstNoiseLevel,
                   dstCutoff, dstResonance, dstDrive, dstFilterMix,
                   dstLfo1Rate, dstLfo2Rate, dstLfo3Rate, dstAmp, numModDests };
    constexpr int oscDestStride = dstBPos - dstAPos;
    constexpr int dstOscBase (int osc) { return dstAPos + osc * oscDestStride; }
    enum OscDestOffset { oPos, oPitch, oLevel, oPan, oDetune, oWarp };

    inline const juce::StringArray& modDests()
    {
        static const juce::StringArray s { "None",
            "A WT Pos", "A Pitch", "A Level", "A Pan", "A Detune", "A Warp",
            "B WT Pos", "B Pitch", "B Level", "B Pan", "B Detune", "B Warp",
            "Sub Level", "Noise Level",
            "Filter Cutoff", "Filter Res", "Filter Drive", "Filter Mix",
            "LFO 1 Rate", "LFO 2 Rate", "LFO 3 Rate", "Amp" };
        return s;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- cached raw pointers for lock-free reads on the audio thread ----
    struct ParamCache
    {
        using P = std::atomic<float>*;

        struct Osc  { P on, table, pos, oct, semi, fine, unison, detune, width, pan, level, phase, rand, warp, warpAmt; };
        struct Env  { P a, h, d, s, r; };
        struct Lfo  { P shape, rate, sync, div, mode; };
        struct Mod  { P src, dst, amt; };

        Osc osc[numOscs];
        P subOn, subShape, subOct, subLevel, subPan;
        P noiseOn, noiseType, noiseLevel, noisePan;
        P fltOn, fltType, fltCutoff, fltRes, fltDrive, fltKey, fltMix;
        P fltRoute[4]; // oscA, oscB, sub, noise
        Env env[numEnvs];
        Lfo lfo[numLfos];
        Mod mod[numModSlots];
        P master, voices, legato, glide, bendRange;
        P distOn, distType, distDrive, distMix;
        P choOn, choRate, choDepth, choMix;
        P dlyOn, dlySync, dlyTime, dlyDiv, dlyFb, dlyMix, dlyPing;
        P revOn, revSize, revDamp, revWidth, revMix;

        void init (juce::AudioProcessorValueTreeState& s);
    };

    inline float get (std::atomic<float>* p)        { return p->load (std::memory_order_relaxed); }
    inline int   geti (std::atomic<float>* p)       { return (int) std::lround (p->load (std::memory_order_relaxed)); }
    inline bool  getb (std::atomic<float>* p)       { return p->load (std::memory_order_relaxed) > 0.5f; }
}
