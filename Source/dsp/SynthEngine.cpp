#include "SynthEngine.h"

void SynthEngine::prepare (double sr)
{
    sampleRate = sr;
    for (auto& v : voices)
        v.prepare (sr);
    reset();
}

void SynthEngine::reset()
{
    for (auto& v : voices)
        v.kill();
    numHeld = 0;
    sustainDown = false;
    lastNote = -1.0;
    lastStarted = nullptr;
}

void SynthEngine::takeSnapshot (const Params::ParamCache& p, const Wavetable* const* tables, double bpm)
{
    using namespace Params;
    auto& s = snap;
    s.sampleRate = sampleRate;

    for (int i = 0; i < numOscs; ++i)
    {
        const auto& src = p.osc[i];
        auto& o = s.osc[i];
        o.on = getb (src.on);
        o.table = tables[i];
        o.oct = geti (src.oct);           o.semi = geti (src.semi);       o.fine = get (src.fine);
        o.unison = geti (src.unison);     o.detune = get (src.detune);    o.width = get (src.width);
        o.pan = get (src.pan);            o.level = get (src.level);      o.pos = get (src.pos);
        o.phase = get (src.phase);        o.rand = get (src.rand);
        o.warp = geti (src.warp);         o.warpAmt = get (src.warpAmt);
    }

    s.subOn = getb (p.subOn);      s.subShape = geti (p.subShape);   s.subOct = geti (p.subOct);
    s.subLevel = get (p.subLevel); s.subPan = get (p.subPan);
    s.noiseOn = getb (p.noiseOn);  s.noiseType = geti (p.noiseType);
    s.noiseLevel = get (p.noiseLevel); s.noisePan = get (p.noisePan);

    s.fltOn = getb (p.fltOn);      s.fltType = geti (p.fltType);     s.cutoff = get (p.fltCutoff);
    s.res = get (p.fltRes);        s.drive = get (p.fltDrive);       s.keytrack = get (p.fltKey);
    s.fltMix = get (p.fltMix);
    for (int i = 0; i < 4; ++i)
        s.route[i] = getb (p.fltRoute[i]);

    for (int i = 0; i < numEnvs; ++i)
        s.env[i] = { get (p.env[i].a), get (p.env[i].h), get (p.env[i].d), get (p.env[i].s), get (p.env[i].r) };

    for (int i = 0; i < numLfos; ++i)
    {
        const auto& l = p.lfo[i];
        const double hz = getb (l.sync) ? (bpm / 60.0) / syncBeats (geti (l.div)) : (double) get (l.rate);
        s.lfo[i] = { geti (l.shape), geti (l.mode), hz };
    }

    s.numSlots = 0;
    for (int i = 0; i < numModSlots; ++i)
    {
        const int src = geti (p.mod[i].src), dst = geti (p.mod[i].dst);
        const float amt = get (p.mod[i].amt);
        if (src > srcNone && src < numModSources && dst > dstNone && dst < numModDests && amt != 0.0f)
            s.slots[s.numSlots++] = { src, dst, amt };
    }

    s.glide = get (p.glide);
    bendRange = geti (p.bendRange);
    s.bendSemis = pitchBend * (float) bendRange;
    s.modWheel = modWheel;
    s.aftertouch = aftertouch;

    polyphony = geti (p.voices);
    legato = getb (p.legato);
}

void SynthEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi,
                           const Params::ParamCache& params, const Wavetable* const* tables, double bpm)
{
    buffer.clear();
    takeSnapshot (params, tables, bpm);

    // Voices above a lowered polyphony limit fade out naturally.
    for (int i = activeVoiceLimit(); i < Params::maxVoices; ++i)
        if (voices[i].isGateOn())
            voices[i].release();

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    float scratchR[Voice::controlBlock * 16];
    const int numSamples = buffer.getNumSamples();

    auto renderRange = [&] (int start, int end)
    {
        for (int pos = start; pos < end;)
        {
            const int n = juce::jmin (end - pos, (int) std::size (scratchR));
            float* r = outR != nullptr ? outR + pos : scratchR;
            if (outR == nullptr)
                std::fill (scratchR, scratchR + n, 0.0f);

            for (auto& v : voices)
                if (v.isActive())
                    v.render (outL + pos, r, 0, n, snap);
            pos += n;
        }
    };

    int position = 0;
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderRange (position, eventPos);
        handleMidi (metadata.getMessage());
        position = eventPos;
    }
    renderRange (position, numSamples);

    for (int i = 0; i < Params::numLfos; ++i)
    {
        freeLfoPhase[i] += snap.lfo[i].hz * numSamples / sampleRate;
        freeLfoPhase[i] -= std::floor (freeLfoPhase[i]);
    }

    if (lastStarted != nullptr && lastStarted->isActive())
        for (int i = 0; i < Params::numOscs; ++i)
            displayedPosition[i].store (lastStarted->displayedPos[i]);
    else
        for (int i = 0; i < Params::numOscs; ++i)
            displayedPosition[i].store (snap.osc[i].pos);
}

void SynthEngine::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
    {
        pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        snap.bendSemis = pitchBend * (float) bendRange;
    }
    else if (m.isChannelPressure())
        snap.aftertouch = aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())
        snap.aftertouch = aftertouch = (float) m.getAfterTouchValue() / 127.0f;
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (cc == 1)
            snap.modWheel = modWheel = (float) m.getControllerValue() / 127.0f;
        else if (cc == 64)
        {
            sustainDown = m.getControllerValue() >= 64;
            if (! sustainDown)
                releaseSustained();
        }
        else if (cc == 120 || cc == 123)
        {
            for (auto& v : voices)
                cc == 120 ? v.kill() : v.release();
            numHeld = 0;
        }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices)
            v.release();
        numHeld = 0;
    }
}

void SynthEngine::noteOn (int note, float velocity)
{
    const bool mono = activeVoiceLimit() == 1;

    if (mono)
    {
        // Keep a stack of held notes so releasing returns to the previous one.
        for (int i = 0; i < numHeld; ++i)
            if (heldNotes[i] == note)
            {
                std::copy (heldNotes + i + 1, heldNotes + numHeld, heldNotes + i);
                --numHeld;
                break;
            }
        if (numHeld < (int) std::size (heldNotes))
            heldNotes[numHeld++] = note;

        auto& v = voices[0];
        if (legato && v.isGateOn())
            v.glideTo (note);
        else
            v.start (note, velocity, v.isActive() ? v.getCurrentNote() : lastNote, snap, freeLfoPhase);

        v.age = ++voiceCounter;
        lastStarted = &v;
    }
    else
    {
        Voice* v = nullptr;
        for (int i = 0; i < activeVoiceLimit(); ++i)
            if (voices[i].isActive() && voices[i].getNote() == note)
                v = &voices[i];

        if (v == nullptr)
            v = findVoiceToUse();

        // In poly mode, glide only when playing legato (another key still held).
        bool anyHeld = false;
        for (int i = 0; i < activeVoiceLimit(); ++i)
            anyHeld = anyHeld || (voices[i].isGateOn() && &voices[i] != v);

        v->start (note, velocity, anyHeld ? lastNote : -1.0, snap, freeLfoPhase);
        v->age = ++voiceCounter;
        lastStarted = v;
    }

    lastNote = note;
}

void SynthEngine::noteOff (int note)
{
    if (activeVoiceLimit() == 1)
    {
        for (int i = 0; i < numHeld; ++i)
            if (heldNotes[i] == note)
            {
                std::copy (heldNotes + i + 1, heldNotes + numHeld, heldNotes + i);
                --numHeld;
                break;
            }

        auto& v = voices[0];
        if (v.getNote() != note || ! v.isGateOn())
            return;

        if (numHeld > 0)
        {
            v.glideTo (heldNotes[numHeld - 1]);
            lastNote = heldNotes[numHeld - 1];
        }
        else if (sustainDown)
            v.sustained = true;
        else
            v.release();
        return;
    }

    for (auto& v : voices)
        if (v.isGateOn() && v.getNote() == note)
        {
            if (sustainDown)
                v.sustained = true;
            else
                v.release();
        }
}

void SynthEngine::releaseSustained()
{
    for (auto& v : voices)
        if (v.sustained && v.isGateOn())
        {
            v.sustained = false;
            v.release();
        }
}

Voice* SynthEngine::findVoiceToUse()
{
    const int limit = activeVoiceLimit();

    for (int i = 0; i < limit; ++i)
        if (! voices[i].isActive())
            return &voices[i];

    // Steal the oldest released voice, otherwise the oldest held one.
    Voice* oldestReleased = nullptr;
    Voice* oldest = nullptr;
    for (int i = 0; i < limit; ++i)
    {
        auto& v = voices[i];
        if (! v.isGateOn() && (oldestReleased == nullptr || v.age < oldestReleased->age))
            oldestReleased = &v;
        if (oldest == nullptr || v.age < oldest->age)
            oldest = &v;
    }
    return oldestReleased != nullptr ? oldestReleased : oldest;
}
