#include "Parameters.h"

namespace Params
{
namespace
{
    using APF = juce::AudioParameterFloat;
    using API = juce::AudioParameterInt;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;
    using Range = juce::NormalisableRange<float>;

    Range skewed (float lo, float hi, float centre)
    {
        Range r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::AudioParameterFloatAttributes withSuffix (const juce::String& suffix, int decimals = 2)
    {
        return juce::AudioParameterFloatAttributes()
            .withLabel (suffix)
            .withStringFromValueFunction ([decimals, suffix] (float v, int) { return juce::String (v, decimals) + suffix; });
    }

    juce::AudioParameterFloatAttributes percent()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
    }

    juce::AudioParameterFloatAttributes seconds()
    {
        return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
        });
    }

    juce::AudioParameterFloatAttributes hertz()
    {
        return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            return v < 1000.0f ? juce::String (v, v < 10.0f ? 2 : 0) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz";
        });
    }

    template <typename T, typename... Args>
    void add (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& id,
              const juce::String& name, Args&&... args)
    {
        layout.add (std::make_unique<T> (juce::ParameterID { id, 1 }, name, std::forward<Args> (args)...));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    for (int i = 0; i < numOscs; ++i)
    {
        const juce::String n = i == 0 ? "Osc A " : "Osc B ";
        add<APB> (l, osc (i, "on"),      n + "On",          i == 0);
        add<APC> (l, osc (i, "table"),   n + "Wavetable",   tableNames(), i == 0 ? 0 : 4);
        add<APF> (l, osc (i, "pos"),     n + "WT Pos",      Range (0.0f, 1.0f), 0.0f, percent());
        add<API> (l, osc (i, "oct"),     n + "Octave",      -4, 4, 0);
        add<API> (l, osc (i, "semi"),    n + "Semi",        -12, 12, 0);
        add<APF> (l, osc (i, "fine"),    n + "Fine",        Range (-100.0f, 100.0f), 0.0f, withSuffix (" ct", 0));
        add<API> (l, osc (i, "unison"),  n + "Unison",      1, maxUnison, 1);
        add<APF> (l, osc (i, "detune"),  n + "Detune",      Range (0.0f, 1.0f), 0.25f, percent());
        add<APF> (l, osc (i, "width"),   n + "Stereo Width",Range (0.0f, 1.0f), 0.75f, percent());
        add<APF> (l, osc (i, "pan"),     n + "Pan",         Range (-1.0f, 1.0f), 0.0f, percent());
        add<APF> (l, osc (i, "level"),   n + "Level",       Range (0.0f, 1.0f), 0.75f, percent());
        add<APF> (l, osc (i, "phase"),   n + "Phase",       Range (0.0f, 1.0f), 0.0f, percent());
        add<APF> (l, osc (i, "rand"),    n + "Phase Rand",  Range (0.0f, 1.0f), 1.0f, percent());
        add<APC> (l, osc (i, "warp"),    n + "Warp Mode",   warpModes(), 0);
        add<APF> (l, osc (i, "warpAmt"), n + "Warp",        Range (0.0f, 1.0f), 0.0f, percent());
    }

    add<APB> (l, "sub_on",    "Sub On",     false);
    add<APC> (l, "sub_shape", "Sub Shape",  subShapes(), 0);
    add<API> (l, "sub_oct",   "Sub Octave", -3, 1, -1);
    add<APF> (l, "sub_level", "Sub Level",  Range (0.0f, 1.0f), 0.75f, percent());
    add<APF> (l, "sub_pan",   "Sub Pan",    Range (-1.0f, 1.0f), 0.0f, percent());

    add<APB> (l, "noise_on",    "Noise On",    false);
    add<APC> (l, "noise_type",  "Noise Type",  noiseTypes(), 0);
    add<APF> (l, "noise_level", "Noise Level", Range (0.0f, 1.0f), 0.3f, percent());
    add<APF> (l, "noise_pan",   "Noise Pan",   Range (-1.0f, 1.0f), 0.0f, percent());

    add<APB> (l, "flt_on",     "Filter On",        false);
    add<APC> (l, "flt_type",   "Filter Type",      filterTypes(), 1);
    add<APF> (l, "flt_cutoff", "Filter Cutoff",    skewed (20.0f, 20000.0f, 1000.0f), 20000.0f, hertz());
    add<APF> (l, "flt_res",    "Filter Resonance", Range (0.0f, 1.0f), 0.1f, percent());
    add<APF> (l, "flt_drive",  "Filter Drive",     Range (0.0f, 1.0f), 0.0f, percent());
    add<APF> (l, "flt_key",    "Filter Keytrack",  Range (0.0f, 1.0f), 0.0f, percent());
    add<APF> (l, "flt_mix",    "Filter Mix",       Range (0.0f, 1.0f), 1.0f, percent());
    add<APB> (l, "flt_oscA",   "Filter Osc A",     true);
    add<APB> (l, "flt_oscB",   "Filter Osc B",     true);
    add<APB> (l, "flt_sub",    "Filter Sub",       true);
    add<APB> (l, "flt_noise",  "Filter Noise",     true);

    for (int i = 0; i < numEnvs; ++i)
    {
        const juce::String n = "Env " + juce::String (i + 1) + " ";
        add<APF> (l, env (i, "a"), n + "Attack",  skewed (0.0f, 10.0f, 0.5f), 0.005f, seconds());
        add<APF> (l, env (i, "h"), n + "Hold",    skewed (0.0f, 5.0f, 0.5f),  0.0f,   seconds());
        add<APF> (l, env (i, "d"), n + "Decay",   skewed (0.001f, 10.0f, 1.0f), 1.0f, seconds());
        add<APF> (l, env (i, "s"), n + "Sustain", Range (0.0f, 1.0f), i == 0 ? 1.0f : 0.0f, percent());
        add<APF> (l, env (i, "r"), n + "Release", skewed (0.001f, 15.0f, 1.0f), 0.015f + (i == 0 ? 0.0f : 0.3f), seconds());
    }

    for (int i = 0; i < numLfos; ++i)
    {
        const juce::String n = "LFO " + juce::String (i + 1) + " ";
        add<APC> (l, lfo (i, "shape"), n + "Shape", lfoShapes(), 0);
        add<APF> (l, lfo (i, "rate"),  n + "Rate",  skewed (0.01f, 40.0f, 2.0f), 2.0f, hertz());
        add<APB> (l, lfo (i, "sync"),  n + "Sync",  false);
        add<APC> (l, lfo (i, "div"),   n + "Division", syncDivs(), 4);
        add<APC> (l, lfo (i, "mode"),  n + "Mode",  lfoModes(), 0);
    }

    for (int i = 0; i < numModSlots; ++i)
    {
        const juce::String n = "Mod " + juce::String (i + 1) + " ";
        add<APC> (l, mod (i, "src"), n + "Source",      modSources(), 0);
        add<APC> (l, mod (i, "dst"), n + "Destination", modDests(), 0);
        add<APF> (l, mod (i, "amt"), n + "Amount",      Range (-1.0f, 1.0f), 0.0f, percent());
    }

    add<APF> (l, "master",    "Master Volume", Range (0.0f, 1.0f), 0.7f, percent());
    add<API> (l, "voices",    "Voices",        1, maxVoices, 8);
    add<APB> (l, "legato",    "Legato",        false);
    add<APF> (l, "glide",     "Glide",         skewed (0.0f, 2.0f, 0.2f), 0.0f, seconds());
    add<API> (l, "bendRange", "Pitch Bend Range", 0, 24, 2);

    add<APB> (l, "dist_on",    "Distortion On",    false);
    add<APC> (l, "dist_type",  "Distortion Type",  distTypes(), 0);
    add<APF> (l, "dist_drive", "Distortion Drive", Range (0.0f, 1.0f), 0.3f, percent());
    add<APF> (l, "dist_mix",   "Distortion Mix",   Range (0.0f, 1.0f), 1.0f, percent());

    add<APB> (l, "cho_on",    "Chorus On",    false);
    add<APF> (l, "cho_rate",  "Chorus Rate",  skewed (0.05f, 5.0f, 0.8f), 0.6f, hertz());
    add<APF> (l, "cho_depth", "Chorus Depth", Range (0.0f, 1.0f), 0.4f, percent());
    add<APF> (l, "cho_mix",   "Chorus Mix",   Range (0.0f, 1.0f), 0.5f, percent());

    add<APB> (l, "dly_on",   "Delay On",       false);
    add<APB> (l, "dly_sync", "Delay Sync",     true);
    add<APF> (l, "dly_time", "Delay Time",     skewed (0.01f, 2.0f, 0.35f), 0.35f, seconds());
    add<APC> (l, "dly_div",  "Delay Division", syncDivs(), 13);
    add<APF> (l, "dly_fb",   "Delay Feedback", Range (0.0f, 0.95f), 0.4f, percent());
    add<APF> (l, "dly_mix",  "Delay Mix",      Range (0.0f, 1.0f), 0.25f, percent());
    add<APB> (l, "dly_ping", "Delay Ping-Pong",true);

    add<APB> (l, "rev_on",    "Reverb On",    false);
    add<APF> (l, "rev_size",  "Reverb Size",  Range (0.0f, 1.0f), 0.6f, percent());
    add<APF> (l, "rev_damp",  "Reverb Damping", Range (0.0f, 1.0f), 0.5f, percent());
    add<APF> (l, "rev_width", "Reverb Width", Range (0.0f, 1.0f), 1.0f, percent());
    add<APF> (l, "rev_mix",   "Reverb Mix",   Range (0.0f, 1.0f), 0.25f, percent());

    return l;
}

void ParamCache::init (juce::AudioProcessorValueTreeState& s)
{
    auto p = [&s] (const juce::String& id)
    {
        auto* raw = s.getRawParameterValue (id);
        jassert (raw != nullptr); // unknown parameter ID
        return raw;
    };

    for (int i = 0; i < numOscs; ++i)
    {
        auto& o = osc[i];
        o.on = p (Params::osc (i, "on"));         o.table = p (Params::osc (i, "table"));
        o.pos = p (Params::osc (i, "pos"));       o.oct = p (Params::osc (i, "oct"));
        o.semi = p (Params::osc (i, "semi"));     o.fine = p (Params::osc (i, "fine"));
        o.unison = p (Params::osc (i, "unison")); o.detune = p (Params::osc (i, "detune"));
        o.width = p (Params::osc (i, "width"));   o.pan = p (Params::osc (i, "pan"));
        o.level = p (Params::osc (i, "level"));   o.phase = p (Params::osc (i, "phase"));
        o.rand = p (Params::osc (i, "rand"));     o.warp = p (Params::osc (i, "warp"));
        o.warpAmt = p (Params::osc (i, "warpAmt"));
    }

    subOn = p ("sub_on"); subShape = p ("sub_shape"); subOct = p ("sub_oct"); subLevel = p ("sub_level"); subPan = p ("sub_pan");
    noiseOn = p ("noise_on"); noiseType = p ("noise_type"); noiseLevel = p ("noise_level"); noisePan = p ("noise_pan");

    fltOn = p ("flt_on"); fltType = p ("flt_type"); fltCutoff = p ("flt_cutoff"); fltRes = p ("flt_res");
    fltDrive = p ("flt_drive"); fltKey = p ("flt_key"); fltMix = p ("flt_mix");
    fltRoute[0] = p ("flt_oscA"); fltRoute[1] = p ("flt_oscB"); fltRoute[2] = p ("flt_sub"); fltRoute[3] = p ("flt_noise");

    for (int i = 0; i < numEnvs; ++i)
        env[i] = { p (Params::env (i, "a")), p (Params::env (i, "h")), p (Params::env (i, "d")),
                   p (Params::env (i, "s")), p (Params::env (i, "r")) };

    for (int i = 0; i < numLfos; ++i)
        lfo[i] = { p (Params::lfo (i, "shape")), p (Params::lfo (i, "rate")), p (Params::lfo (i, "sync")),
                   p (Params::lfo (i, "div")), p (Params::lfo (i, "mode")) };

    for (int i = 0; i < numModSlots; ++i)
        mod[i] = { p (Params::mod (i, "src")), p (Params::mod (i, "dst")), p (Params::mod (i, "amt")) };

    master = p ("master"); voices = p ("voices"); legato = p ("legato"); glide = p ("glide"); bendRange = p ("bendRange");

    distOn = p ("dist_on"); distType = p ("dist_type"); distDrive = p ("dist_drive"); distMix = p ("dist_mix");
    choOn = p ("cho_on"); choRate = p ("cho_rate"); choDepth = p ("cho_depth"); choMix = p ("cho_mix");
    dlyOn = p ("dly_on"); dlySync = p ("dly_sync"); dlyTime = p ("dly_time"); dlyDiv = p ("dly_div");
    dlyFb = p ("dly_fb"); dlyMix = p ("dly_mix"); dlyPing = p ("dly_ping");
    revOn = p ("rev_on"); revSize = p ("rev_size"); revDamp = p ("rev_damp"); revWidth = p ("rev_width"); revMix = p ("rev_mix");
}
}
