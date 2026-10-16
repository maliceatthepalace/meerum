#pragma once

#include "Parameters.h"

// Factory presets, exposed to the host as programs. Each preset lists only the parameters
// that differ from their defaults; values are in real units (choice/int params use the index).
namespace FactoryPresets
{
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    inline const std::vector<Preset>& all()
    {
        using namespace Params;
        enum Table { basic, pulse, harmonic, vowels, syncSaw, fmBells, wavefold, digital };
        enum Filter { lp12, lp24, hp12, hp24, bp12, notch };

        auto slot = [] (int i, int src, int dst, float amt) -> std::vector<std::pair<juce::String, float>>
        {
            return { { mod (i, "src"), (float) src }, { mod (i, "dst"), (float) dst }, { mod (i, "amt"), amt } };
        };

        auto join = [] (std::initializer_list<std::vector<std::pair<juce::String, float>>> parts)
        {
            std::vector<std::pair<juce::String, float>> out;
            for (const auto& p : parts)
                out.insert (out.end(), p.begin(), p.end());
            return out;
        };

        static const std::vector<Preset> presets
        {
            { "Init", {} },

            { "Supersaw Lead", join ({
                { { osc (0, "table"), basic }, { osc (0, "pos"), 0.667f }, { osc (0, "unison"), 7 },
                  { osc (0, "detune"), 0.3f }, { osc (0, "width"), 0.9f },
                  { osc (1, "on"), 1 }, { osc (1, "table"), basic }, { osc (1, "pos"), 0.667f }, { osc (1, "oct"), 1 },
                  { osc (1, "unison"), 3 }, { osc (1, "detune"), 0.2f }, { osc (1, "level"), 0.35f },
                  { "flt_on", 1 }, { "flt_type", lp24 }, { "flt_cutoff", 4500 }, { "flt_res", 0.2f },
                  { env (0, "a"), 0.005f }, { env (0, "s"), 0.8f }, { env (0, "r"), 0.3f },
                  { env (1, "d"), 0.6f }, { env (1, "s"), 0.3f },
                  { "glide", 0.04f },
                  { "dly_on", 1 }, { "dly_mix", 0.18f }, { "rev_on", 1 }, { "rev_mix", 0.2f } },
                slot (0, srcEnv2, dstCutoff, 0.25f) }) },

            { "Reese Bass", join ({
                { { osc (0, "table"), basic }, { osc (0, "pos"), 0.667f }, { osc (0, "oct"), -1 }, { osc (0, "unison"), 2 },
                  { osc (0, "detune"), 0.18f }, { osc (0, "width"), 0.4f },
                  { osc (1, "on"), 1 }, { osc (1, "table"), basic }, { osc (1, "pos"), 0.667f }, { osc (1, "oct"), -1 },
                  { osc (1, "fine"), 14 }, { osc (1, "unison"), 2 }, { osc (1, "detune"), 0.12f },
                  { "flt_on", 1 }, { "flt_type", lp24 }, { "flt_cutoff", 700 }, { "flt_res", 0.3f }, { "flt_drive", 0.4f },
                  { env (0, "r"), 0.08f },
                  { "voices", 1 }, { "legato", 1 }, { "glide", 0.06f },
                  { "dist_on", 1 }, { "dist_drive", 0.2f }, { "dist_mix", 0.6f },
                  { lfo (0, "rate"), 0.3f } },
                slot (0, srcLfo1, dstCutoff, 0.12f) }) },

            { "Pluck", join ({
                { { osc (0, "table"), harmonic }, { osc (0, "pos"), 0.7f }, { osc (0, "unison"), 3 }, { osc (0, "detune"), 0.12f },
                  { osc (1, "on"), 1 }, { osc (1, "table"), fmBells }, { osc (1, "pos"), 0.2f }, { osc (1, "oct"), 1 },
                  { osc (1, "level"), 0.35f },
                  { "flt_on", 1 }, { "flt_type", lp12 }, { "flt_cutoff", 700 }, { "flt_res", 0.25f }, { "flt_key", 0.5f },
                  { env (0, "a"), 0.001f }, { env (0, "d"), 0.45f }, { env (0, "s"), 0.0f }, { env (0, "r"), 0.35f },
                  { env (1, "a"), 0.0f }, { env (1, "d"), 0.25f }, { env (1, "s"), 0.0f },
                  { "dly_on", 1 }, { "dly_mix", 0.25f }, { "rev_on", 1 }, { "rev_mix", 0.25f } },
                slot (0, srcEnv2, dstCutoff, 0.45f),
                slot (1, srcVelocity, dstAmp, 0.3f) }) },

            { "Vowel Pad", join ({
                { { osc (0, "table"), vowels }, { osc (0, "pos"), 0.2f }, { osc (0, "unison"), 5 }, { osc (0, "detune"), 0.2f },
                  { osc (1, "on"), 1 }, { osc (1, "table"), basic }, { osc (1, "pos"), 0.0f }, { osc (1, "oct"), -1 },
                  { osc (1, "level"), 0.4f },
                  { "flt_on", 1 }, { "flt_type", lp12 }, { "flt_cutoff", 3500 },
                  { env (0, "a"), 1.2f }, { env (0, "d"), 2.0f }, { env (0, "s"), 0.8f }, { env (0, "r"), 2.5f },
                  { lfo (0, "shape"), 1 }, { lfo (0, "rate"), 0.12f }, { lfo (0, "mode"), 1 },
                  { "cho_on", 1 }, { "rev_on", 1 }, { "rev_size", 0.85f }, { "rev_mix", 0.4f } },
                slot (0, srcLfo1, dstAPos, 0.45f),
                slot (1, srcModWheel, dstCutoff, 0.3f) }) },

            { "Wobble Bass", join ({
                { { osc (0, "table"), syncSaw }, { osc (0, "pos"), 0.3f },
                  { osc (1, "on"), 1 }, { osc (1, "table"), basic }, { osc (1, "pos"), 1.0f }, { osc (1, "level"), 0.45f },
                  { "sub_on", 1 }, { "sub_level", 0.6f }, { "flt_sub", 0 },
                  { "flt_on", 1 }, { "flt_type", lp24 }, { "flt_cutoff", 250 }, { "flt_res", 0.45f }, { "flt_drive", 0.5f },
                  { env (0, "r"), 0.08f },
                  { lfo (0, "sync"), 1 }, { lfo (0, "div"), 5 },
                  { "voices", 1 }, { "glide", 0.03f },
                  { "dist_on", 1 }, { "dist_drive", 0.35f } },
                slot (0, srcLfo1, dstCutoff, 0.45f),
                slot (1, srcLfo1, dstAPos, 0.35f) }) },

            { "Bell Keys", join ({
                { { osc (0, "table"), fmBells }, { osc (0, "pos"), 0.35f }, { osc (0, "rand"), 0.0f },
                  { osc (1, "on"), 1 }, { osc (1, "table"), basic }, { osc (1, "pos"), 0.0f }, { osc (1, "oct"), 1 },
                  { osc (1, "level"), 0.3f },
                  { env (0, "a"), 0.002f }, { env (0, "d"), 1.5f }, { env (0, "s"), 0.2f }, { env (0, "r"), 1.2f },
                  { env (1, "d"), 1.2f },
                  { "cho_on", 1 }, { "cho_mix", 0.35f }, { "rev_on", 1 }, { "rev_mix", 0.3f } },
                slot (0, srcVelocity, dstAPos, 0.4f),
                slot (1, srcEnv2, dstAPos, 0.3f) }) },

            { "Noise Sweep", join ({
                { { osc (0, "on"), 0 },
                  { "noise_on", 1 }, { "noise_type", 1 }, { "noise_level", 0.9f },
                  { "flt_on", 1 }, { "flt_type", bp12 }, { "flt_cutoff", 300 }, { "flt_res", 0.65f },
                  { env (0, "a"), 0.5f }, { env (0, "r"), 2.0f },
                  { env (1, "a"), 3.0f }, { env (1, "d"), 2.0f }, { env (1, "s"), 1.0f },
                  { "rev_on", 1 }, { "rev_size", 0.9f }, { "rev_mix", 0.5f } },
                slot (0, srcEnv2, dstCutoff, 0.6f) }) },
        };

        return presets;
    }
}
