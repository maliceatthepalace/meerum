#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// AHDSR envelope. Attack is linear from the current level (so retriggering never clicks);
// decay and release are exponential and reach -60 dB after the set time.
class Envelope
{
public:
    void setSampleRate (double sr) noexcept { sampleRate = sr; }

    void setParameters (float attack, float hold, float decay, float sustainLevel, float release) noexcept
    {
        attackStep = attack > 0.0005f ? (float) (1.0 / (attack * sampleRate)) : 1.0f;
        holdSamples = (int) (hold * sampleRate);
        decayCoef = coefFor (decay);
        releaseCoef = coefFor (release);
        sustain = sustainLevel;
    }

    void noteOn() noexcept   { stage = Stage::attack; holdCounter = 0; }
    void noteOff() noexcept  { if (stage != Stage::idle) stage = Stage::release; }
    void reset() noexcept    { stage = Stage::idle; level = 0.0f; }

    bool isActive() const noexcept   { return stage != Stage::idle; }
    float getLevel() const noexcept  { return level; }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::idle:
                break;

            case Stage::attack:
                level += attackStep;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::hold;
                }
                break;

            case Stage::hold:
                if (++holdCounter >= holdSamples)
                    stage = Stage::decay;
                break;

            case Stage::decay:
                level = sustain + (level - sustain) * decayCoef;
                if (std::abs (level - sustain) < 1.0e-4f)
                    stage = Stage::sustain;
                break;

            case Stage::sustain:
                level += (sustain - level) * 0.002f; // follow knob changes without zipper
                break;

            case Stage::release:
                level *= releaseCoef;
                if (level < 1.0e-5f)
                    reset();
                break;
        }
        return level;
    }

private:
    enum class Stage { idle, attack, hold, decay, sustain, release };

    float coefFor (float seconds) const noexcept
    {
        return (float) std::exp (-6.9 / juce::jmax (1.0, seconds * sampleRate));
    }

    double sampleRate = 44100.0;
    Stage stage = Stage::idle;
    float level = 0.0f, attackStep = 1.0f, decayCoef = 0.0f, releaseCoef = 0.0f, sustain = 1.0f;
    int holdSamples = 0, holdCounter = 0;
};

//==============================================================================
// Bipolar LFO (-1..1), advanced at control rate.
class Lfo
{
public:
    enum Shape { sine, triangle, sawUp, sawDown, square, sampleHold, smoothRandom };

    void setPhase (double p) noexcept   { phase = p - std::floor (p); finished = false; }
    double getPhase() const noexcept    { return phase; }
    void seed (juce::int64 s) noexcept  { rng.setSeed (s); held = randomValue(); target = randomValue(); }

    float value (int shape) const noexcept
    {
        const auto p = (float) phase;
        switch (shape)
        {
            case sine:         return std::sin (juce::MathConstants<float>::twoPi * p);
            case triangle:     return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
            case sawUp:        return 2.0f * p - 1.0f;
            case sawDown:      return 1.0f - 2.0f * p;
            case square:       return p < 0.5f ? 1.0f : -1.0f;
            case sampleHold:   return held;
            case smoothRandom: return held + (target - held) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * p));
            default:           return 0.0f;
        }
    }

    void advance (int numSamples, double hz, double sampleRate, bool oneShot) noexcept
    {
        if (finished)
            return;

        phase += hz * numSamples / sampleRate;
        if (phase >= 1.0)
        {
            if (oneShot)
            {
                phase = 0.9999;
                finished = true;
                return;
            }
            phase -= std::floor (phase);
            held = target;
            target = randomValue();
        }
    }

private:
    float randomValue() noexcept { return rng.nextFloat() * 2.0f - 1.0f; }

    double phase = 0.0;
    bool finished = false;
    float held = 0.0f, target = 0.0f;
    juce::Random rng;
};

//==============================================================================
// Stereo topology-preserving-transform state-variable filter (Cytomic/Simper),
// two cascaded stages for 24 dB slopes.
class SvfFilter
{
public:
    enum Type { lp12, lp24, hp12, hp24, bp12, notch };

    void reset() noexcept
    {
        for (auto& s : state)
            s = {};
    }

    void setCoefficients (float cutoffHz, float resonance, double sampleRate) noexcept
    {
        const double fc = juce::jlimit (20.0, sampleRate * 0.45, (double) cutoffHz);
        const double g = std::tan (juce::MathConstants<double>::pi * fc / sampleRate);
        k = (float) (2.0 - 1.96 * juce::jlimit (0.0f, 1.0f, resonance));
        a1 = (float) (1.0 / (1.0 + g * (g + k)));
        a2 = (float) g * a1;
        a3 = (float) g * a2;
    }

    float process (float x, int channel, int type) noexcept
    {
        const bool twoStages = type == lp24 || type == hp24;
        float y = tick (x, state[(size_t) channel * 2], type);
        if (twoStages)
            y = tick (y, state[(size_t) channel * 2 + 1], type);
        return y;
    }

private:
    struct State { float ic1 = 0.0f, ic2 = 0.0f; };

    float tick (float v0, State& s, int type) noexcept
    {
        const float v3 = v0 - s.ic2;
        const float v1 = a1 * s.ic1 + a2 * v3;
        const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;

        switch (type)
        {
            case lp12: case lp24: return v2;
            case hp12: case hp24: return v0 - k * v1 - v2;
            case bp12:            return k * v1; // unity gain at the centre frequency
            case notch:           return v0 - k * v1;
            default:              return v2;
        }
    }

    float k = 2.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    State state[4];
};
