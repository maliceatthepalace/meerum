#include "Voice.h"

namespace
{
    constexpr float voiceHeadroom = 0.35f; // ~-3 dB vs. 0.5, keeps 4-note unison chords below 0 dBFS

    double noteToHz (double note) noexcept { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

    // Phase distortion applied before the table lookup (Quantize is applied to the output instead).
    inline float warpPhase (float p, int mode, float amt) noexcept
    {
        switch (mode)
        {
            case 1: // Sync
            {
                const float q = p * (1.0f + amt * 7.0f);
                return q - std::floor (q);
            }
            case 2: return std::pow (p, 1.0f / (1.0f + amt * 3.0f)); // Bend +
            case 3: return std::pow (p, 1.0f + amt * 3.0f);          // Bend -
            case 4: // Mirror: blend towards reading the first half forwards then backwards
            {
                const float mirrored = p < 0.5f ? p : 1.0f - p;
                return p + (mirrored - p) * amt;
            }
            case 5: // PWM: squeeze the first half of the cycle
            {
                const float w = 0.5f - amt * 0.49f;
                return p < w ? p * 0.5f / w : 0.5f + (p - w) * 0.5f / (1.0f - w);
            }
            default: return p;
        }
    }

    inline float quantize (float x, float amt) noexcept
    {
        const float steps = juce::jmap (amt, 64.0f, 2.0f);
        return std::round (x * steps) / steps;
    }

    inline float polyBlep (double t, double dt) noexcept
    {
        if (t < dt)       { t /= dt;               return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt;    return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    inline void panGains (float pan, float& l, float& r) noexcept
    {
        const float angle = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        l = std::cos (angle) * juce::MathConstants<float>::sqrt2;
        r = std::sin (angle) * juce::MathConstants<float>::sqrt2;
    }

    inline float ramp (float from, float to, int i, int n) noexcept
    {
        return from + (to - from) * (float) (i + 1) / (float) n;
    }
}

void Voice::prepare (double sr)
{
    sampleRate = sr;
    for (auto& e : env)
        e.setSampleRate (sr);
    kill();
}

void Voice::start (int note, float vel, double fromNote, const SynthSnapshot& s, const double* freeLfoPhases)
{
    if (! isActive())
        filter.reset();

    midiNote = note;
    targetNote = note;
    glideNote = (fromNote >= 0.0 && s.glide > 0.0f) ? fromNote : (double) note;
    velocity = vel;
    noteRandom = rng.nextFloat() * 2.0f - 1.0f;
    gate = true;
    sustained = false;

    for (int i = 0; i < Params::numEnvs; ++i)
    {
        const auto& e = s.env[i];
        env[i].setParameters (e.a, e.h, e.d, e.s, e.r);
        env[i].noteOn();
    }

    for (int i = 0; i < Params::numLfos; ++i)
    {
        lfo[i].seed (rng.nextInt64());
        lfo[i].setPhase (s.lfo[i].mode == 1 ? freeLfoPhases[i] : 0.0);
    }

    for (int o = 0; o < Params::numOscs; ++o)
    {
        auto& st = osc[o];
        for (auto& ph : st.phase)
        {
            const double p = s.osc[o].phase + s.osc[o].rand * rng.nextDouble();
            ph = p - std::floor (p);
        }
        st.lastUnison = 0; // snap gains and frame position on the first chunk
    }

    subPhase = 0.0;
}

void Voice::release()
{
    gate = false;
    for (auto& e : env)
        e.noteOff();
}

void Voice::kill()
{
    gate = false;
    sustained = false;
    midiNote = -1;
    for (auto& e : env)
        e.reset();
}

void Voice::render (float* outL, float* outR, int startSample, int numSamples, const SynthSnapshot& s)
{
    while (numSamples > 0 && isActive())
    {
        const int n = juce::jmin (numSamples, controlBlock);
        renderChunk (outL + startSample, outR + startSample, n, s);
        startSample += n;
        numSamples -= n;
    }
}

void Voice::renderChunk (float* outL, float* outR, int n, const SynthSnapshot& s)
{
    using namespace Params;

    // ---- modulation sources (sampled at the start of the chunk) ----
    float src[numModSources] {};
    for (int i = 0; i < numEnvs; ++i)
        src[srcEnv1 + i] = env[i].getLevel();
    for (int i = 0; i < numLfos; ++i)
        src[srcLfo1 + i] = lfo[i].value (s.lfo[i].shape);
    src[srcVelocity]   = velocity;
    src[srcNote]       = (float) (glideNote - 60.0) / 60.0f;
    src[srcModWheel]   = s.modWheel;
    src[srcAftertouch] = s.aftertouch;
    src[srcRandom]     = noteRandom;

    float mods[numModDests] {};
    for (int i = 0; i < s.numSlots; ++i)
        mods[s.slots[i].dst] += s.slots[i].amt * src[s.slots[i].src];

    for (int i = 0; i < numLfos; ++i)
    {
        const double hz = s.lfo[i].hz * std::pow (2.0, mods[dstLfo1Rate + i] * 4.0f);
        lfo[i].advance (n, hz, sampleRate, s.lfo[i].mode == 2);
    }

    if (glideNote != targetNote)
    {
        if (s.glide <= 0.0f)
            glideNote = targetNote;
        else
        {
            const double coef = 1.0 - std::exp (-n / (s.glide / 3.0 * sampleRate));
            glideNote += (targetNote - glideNote) * coef;
            if (std::abs (targetNote - glideNote) < 0.001)
                glideNote = targetNote;
        }
    }

    // ---- envelopes (per sample) ----
    float ampEnv[controlBlock];
    for (int i = 0; i < numEnvs; ++i)
    {
        const auto& e = s.env[i];
        env[i].setParameters (e.a, e.h, e.d, e.s, e.r);
    }
    for (int i = 0; i < n; ++i)
    {
        ampEnv[i] = env[0].next();
        env[1].next();
        env[2].next();
    }

    // ---- sources into the filtered and dry buses ----
    float fltL[controlBlock] {}, fltR[controlBlock] {}, dryL[controlBlock] {}, dryR[controlBlock] {};
    auto busFor = [&s, &fltL, &fltR, &dryL, &dryR] (int route, float*& l, float*& r)
    {
        const bool toFilter = s.fltOn && s.route[route];
        l = toFilter ? fltL : dryL;
        r = toFilter ? fltR : dryR;
    };

    for (int o = 0; o < numOscs; ++o)
    {
        if (! s.osc[o].on || s.osc[o].table == nullptr)
            continue;
        float *l, *r;
        busFor (o, l, r);
        renderOsc (o, mods, l, r, n, s);
    }

    if (s.subOn)
    {
        float *l, *r;
        busFor (2, l, r);
        const double inc = noteToHz (glideNote + s.bendSemis + s.subOct * 12.0) / sampleRate;
        const float level = juce::jlimit (0.0f, 1.0f, s.subLevel + mods[dstSubLevel]);
        float gl, gr;
        panGains (s.subPan, gl, gr);
        gl *= level;
        gr *= level;

        for (int i = 0; i < n; ++i)
        {
            const double p = subPhase;
            float v;
            switch (s.subShape)
            {
                case 0:  v = std::sin (juce::MathConstants<float>::twoPi * (float) p); break;
                case 1:  v = 1.0f - 4.0f * std::abs ((float) p - 0.5f); break;
                case 2:  v = (float) (2.0 * p - 1.0) - polyBlep (p, inc); break;
                default:
                {
                    const double p2 = p + 0.5 - std::floor (p + 0.5);
                    v = (p < 0.5 ? 1.0f : -1.0f) + polyBlep (p, inc) - polyBlep (p2, inc);
                    break;
                }
            }
            l[i] += v * ramp (prevSubL, gl, i, n);
            r[i] += v * ramp (prevSubR, gr, i, n);
            subPhase += inc;
            if (subPhase >= 1.0)
                subPhase -= 1.0;
        }
        prevSubL = gl;
        prevSubR = gr;
    }

    if (s.noiseOn)
    {
        float *l, *r;
        busFor (3, l, r);
        const float level = juce::jlimit (0.0f, 1.0f, s.noiseLevel + mods[dstNoiseLevel]);
        float gl, gr;
        panGains (s.noisePan, gl, gr);
        gl *= level;
        gr *= level;

        for (int i = 0; i < n; ++i)
        {
            float v = rng.nextFloat() * 2.0f - 1.0f;
            if (s.noiseType == 1) // Paul Kellet's economy pink filter
            {
                pink[0] = 0.99765f * pink[0] + v * 0.0990460f;
                pink[1] = 0.96300f * pink[1] + v * 0.2965164f;
                pink[2] = 0.57000f * pink[2] + v * 1.0526913f;
                v = (pink[0] + pink[1] + pink[2] + v * 0.1848f) * 0.25f;
            }
            l[i] += v * ramp (prevNoiseL, gl, i, n);
            r[i] += v * ramp (prevNoiseR, gr, i, n);
        }
        prevNoiseL = gl;
        prevNoiseR = gr;
    }

    // ---- filter ----
    if (s.fltOn)
    {
        const float cutoff = s.cutoff
                           * std::pow (2.0f, s.keytrack * (float) (glideNote - 60.0) / 12.0f)
                           * std::pow (2.0f, mods[dstCutoff] * 10.0f);
        filter.setCoefficients (cutoff, juce::jlimit (0.0f, 1.0f, s.res + mods[dstResonance]), sampleRate);

        const float drive = juce::jlimit (0.0f, 1.0f, s.drive + mods[dstDrive]);
        const float driveGain = 1.0f + drive * 8.0f;
        const float driveNorm = 1.0f / std::sqrt (driveGain);
        const float mix = juce::jlimit (0.0f, 1.0f, s.fltMix + mods[dstFilterMix]);

        for (int i = 0; i < n; ++i)
        {
            float inL = fltL[i], inR = fltR[i];
            if (drive > 0.001f)
            {
                inL = std::tanh (inL * driveGain) * driveNorm;
                inR = std::tanh (inR * driveGain) * driveNorm;
            }
            dryL[i] += fltL[i] + (filter.process (inL, 0, s.fltType) - fltL[i]) * mix;
            dryR[i] += fltR[i] + (filter.process (inR, 1, s.fltType) - fltR[i]) * mix;
        }
    }

    // ---- amplifier ----
    const float ampMod = juce::jmax (0.0f, 1.0f + mods[dstAmp]);
    for (int i = 0; i < n; ++i)
    {
        const float g = ampEnv[i] * ramp (prevAmpMod, ampMod, i, n) * voiceHeadroom;
        outL[i] += dryL[i] * g;
        outR[i] += dryR[i] * g;
    }
    prevAmpMod = ampMod;
}

void Voice::renderOsc (int index, const float* mods, float* busL, float* busR, int n, const SynthSnapshot& s)
{
    using namespace Params;
    const auto& o = s.osc[index];
    auto& st = osc[index];
    const auto& table = *o.table;
    const int base = dstOscBase (index);

    const double pitch = glideNote + s.bendSemis + o.oct * 12.0 + o.semi + o.fine / 100.0 + mods[base + oPitch] * 24.0f;
    const float level  = juce::jlimit (0.0f, 1.0f, o.level + mods[base + oLevel]);
    const float pan    = juce::jlimit (-1.0f, 1.0f, o.pan + mods[base + oPan]);
    const float detune = juce::jlimit (0.0f, 1.0f, o.detune + mods[base + oDetune]);
    const float warp   = juce::jlimit (0.0f, 1.0f, o.warpAmt + mods[base + oWarp]);
    const float pos    = juce::jlimit (0.0f, 1.0f, o.pos + mods[base + oPos]);
    displayedPos[index] = pos;

    const float targetFrame = pos * (float) (table.getNumFrames() - 1);
    const int unison = juce::jlimit (1, maxUnison, o.unison);
    const bool snap = st.lastUnison != unison;
    if (snap)
        st.framePos = targetFrame;

    const double spread = 0.5 * detune + 1.5 * detune * detune * detune; // semitones
    const float unisonGain = 1.0f / std::sqrt ((float) unison);
    const double syncRatio = o.warp == 1 ? 1.0 + warp * 7.0 : 1.0;

    for (int u = 0; u < unison; ++u)
    {
        const float t = unison > 1 ? -1.0f + 2.0f * (float) u / (float) (unison - 1) : 0.0f;
        const double inc = noteToHz (pitch + t * spread) / sampleRate;
        const int mip = Wavetable::levelForIncrement (inc * syncRatio);

        float gl, gr;
        panGains (pan + t * o.width, gl, gr);
        gl *= level * unisonGain;
        gr *= level * unisonGain;
        if (snap)
        {
            st.gainL[u] = gl;
            st.gainR[u] = gr;
        }

        double phase = st.phase[u];
        for (int i = 0; i < n; ++i)
        {
            const float frame = ramp (st.framePos, targetFrame, i, n);
            float v = table.read (frame, warpPhase ((float) phase, o.warp, warp), mip);
            if (o.warp == 6 && warp > 0.0f)
                v = quantize (v, warp);

            busL[i] += v * ramp (st.gainL[u], gl, i, n);
            busR[i] += v * ramp (st.gainR[u], gr, i, n);

            phase += inc;
            if (phase >= 1.0)
                phase -= std::floor (phase);
        }
        st.phase[u] = phase;
        st.gainL[u] = gl;
        st.gainR[u] = gr;
    }

    st.framePos = targetFrame;
    st.lastUnison = unison;
}
