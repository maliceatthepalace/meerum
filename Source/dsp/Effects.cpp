#include "Effects.h"

void FxChain::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;

    distDrive.reset (sr, 0.02);
    distMix.reset (sr, 0.02);

    chorus.prepare ({ sr, (juce::uint32) maxBlockSize, 2 });
    chorus.setCentreDelay (8.0f);
    chorus.setFeedback (0.15f);

    delayBuffer.setSize (2, (int) (sr * 4.0) + 2);
    delaySamples.reset (sr, 0.08);
    delayMix.reset (sr, 0.02);

    reverb.setSampleRate (sr);
    reverbMix.reset (sr, 0.02);

    reset();
}

void FxChain::reset()
{
    chorus.reset();
    delayBuffer.clear();
    delayWrite = 0;
    delayWasOn = false;
    delayTone[0] = delayTone[1] = 0.0f;
    reverb.reset();
    crushHeld[0] = crushHeld[1] = 0.0f;
    crushCounter = 0.0f;
}

void FxChain::process (juce::AudioBuffer<float>& buffer, const Params::ParamCache& p, double bpm)
{
    using namespace Params;
    jassert (buffer.getNumChannels() >= 2);

    if (getb (p.distOn))
        processDistortion (buffer, p);

    if (getb (p.choOn))
    {
        chorus.setRate (get (p.choRate));
        chorus.setDepth (get (p.choDepth));
        chorus.setMix (get (p.choMix) * 0.5f); // the JUCE chorus is fully wet at mix 1
        juce::dsp::AudioBlock<float> block (buffer);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

    const bool delayOn = getb (p.dlyOn);
    if (delayOn && ! delayWasOn)
    {
        delayBuffer.clear(); // don't replay echoes from before the delay was switched off
        delayTone[0] = delayTone[1] = 0.0f;
        delaySamples.setCurrentAndTargetValue (0.0f); // jump to the new time instead of sweeping
    }
    delayWasOn = delayOn;
    if (delayOn)
        processDelay (buffer, p, bpm);

    if (getb (p.revOn))
    {
        reverbMix.setTargetValue (get (p.revMix));
        juce::Reverb::Parameters rp;
        rp.roomSize = get (p.revSize);
        rp.damping = get (p.revDamp);
        rp.width = get (p.revWidth);
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        reverb.setParameters (rp);

        // Process wet into a copy, then crossfade with smoothing.
        const int n = buffer.getNumSamples();
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        constexpr int chunk = 256;
        float wetL[chunk], wetR[chunk];

        for (int start = 0; start < n; start += chunk)
        {
            const int m = juce::jmin (chunk, n - start);
            std::copy (l + start, l + start + m, wetL);
            std::copy (r + start, r + start + m, wetR);
            reverb.processStereo (wetL, wetR, m);

            for (int i = 0; i < m; ++i)
            {
                const float mix = reverbMix.getNextValue();
                l[start + i] += (wetL[i] - l[start + i]) * mix;
                r[start + i] += (wetR[i] - r[start + i]) * mix;
            }
        }
    }
}

void FxChain::processDistortion (juce::AudioBuffer<float>& buffer, const Params::ParamCache& p)
{
    using namespace Params;
    const int type = geti (p.distType);
    distDrive.setTargetValue (get (p.distDrive));
    distMix.setTargetValue (get (p.distMix));

    const int n = buffer.getNumSamples();
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float drive = distDrive.getNextValue();
        const float mix = distMix.getNextValue();
        const float gain = juce::Decibels::decibelsToGain (drive * 30.0f);
        float* samples[2] { l + i, r + i };

        if (type == 3) // Crush: sample-rate and bit-depth reduction
        {
            crushCounter += 1.0f;
            const bool sampleNow = crushCounter >= 1.0f + drive * 31.0f;
            if (sampleNow)
                crushCounter = 0.0f;
            const float steps = std::pow (2.0f, 16.0f - drive * 13.0f);

            for (int ch = 0; ch < 2; ++ch)
            {
                if (sampleNow)
                    crushHeld[ch] = std::round (*samples[ch] * steps) / steps;
                *samples[ch] += (crushHeld[ch] - *samples[ch]) * mix;
            }
            continue;
        }

        for (auto* s : samples)
        {
            const float x = *s * gain;
            float y;
            switch (type)
            {
                case 1:  y = juce::jlimit (-1.0f, 1.0f, x); break;
                case 2:  y = std::sin (x); break;
                default: y = std::tanh (x); break;
            }
            *s += (y - *s) * mix;
        }
    }
}

void FxChain::processDelay (juce::AudioBuffer<float>& buffer, const Params::ParamCache& p, double bpm)
{
    using namespace Params;
    const double seconds = getb (p.dlySync) ? 60.0 / juce::jmax (1.0, bpm) * syncBeats (geti (p.dlyDiv))
                                            : (double) get (p.dlyTime);
    const int size = delayBuffer.getNumSamples();
    const auto target = (float) juce::jlimit (1.0, (double) size - 2.0, seconds * sampleRate);
    if (delaySamples.getCurrentValue() < 1.0f)
        delaySamples.setCurrentAndTargetValue (target);
    else
        delaySamples.setTargetValue (target);
    delayMix.setTargetValue (get (p.dlyMix));
    const float feedback = get (p.dlyFb);
    const bool pingPong = getb (p.dlyPing);
    const float toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 6000.0f / (float) sampleRate);

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    auto* dl = delayBuffer.getWritePointer (0);
    auto* dr = delayBuffer.getWritePointer (1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float d = delaySamples.getNextValue();
        float readPos = (float) delayWrite - d;
        if (readPos < 0.0f)
            readPos += (float) size;
        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % size;
        const float frac = readPos - (float) i0;

        float wetL = dl[i0] + frac * (dl[i1] - dl[i0]);
        float wetR = dr[i0] + frac * (dr[i1] - dr[i0]);

        // Gentle low-pass in the feedback path so repeats darken.
        delayTone[0] += (wetL - delayTone[0]) * toneCoef;
        delayTone[1] += (wetR - delayTone[1]) * toneCoef;
        wetL = delayTone[0];
        wetR = delayTone[1];

        if (pingPong)
        {
            dl[delayWrite] = (l[i] + r[i]) * 0.5f + wetR * feedback;
            dr[delayWrite] = wetL * feedback;
        }
        else
        {
            dl[delayWrite] = l[i] + wetL * feedback;
            dr[delayWrite] = r[i] + wetR * feedback;
        }

        if (++delayWrite >= size)
            delayWrite = 0;

        const float mix = delayMix.getNextValue();
        l[i] += (wetL - l[i]) * mix;
        r[i] += (wetR - r[i]) * mix;
    }
}
