#include "Displays.h"
#include "../PluginProcessor.h"
#include <complex>

using namespace juce;

namespace
{
    void paintWell (Graphics& g, Rectangle<float> r)
    {
        g.setColour (Theme::well);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Theme::outline);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }

    void paintGrid (Graphics& g, Rectangle<float> r, int columns)
    {
        g.setColour (Theme::outline.withAlpha (0.35f));
        for (int i = 1; i < columns; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) columns;
            g.drawVerticalLine (roundToInt (x), r.getY(), r.getBottom());
        }
        g.drawHorizontalLine (roundToInt (r.getCentreY()), r.getX(), r.getRight());
    }
}

// ============================================================================
WavetableDisplay::WavetableDisplay (MeerumProcessor& p, int oscIndex)
    : processor (p), osc (oscIndex)
{
    timerCallback(); // initial state, so the first paint is already correct
    startTimerHz (30);
}

Point<float> WavetableDisplay::Geometry::origin (float depth) const
{
    return { area.getX() + depth * dx, area.getBottom() - amplitude - 4.0f - depth * dy };
}

WavetableDisplay::Geometry WavetableDisplay::geometry() const
{
    Geometry geo;
    geo.area = getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedTop (12.0f);
    geo.dx = geo.area.getWidth() * 0.16f;
    geo.dy = geo.area.getHeight() * 0.38f;
    geo.waveWidth = geo.area.getWidth() - geo.dx;
    geo.amplitude = (geo.area.getHeight() - geo.dy) * 0.45f;
    return geo;
}

Path WavetableDisplay::framePath (const Geometry& geo, float framePos, float depth) const
{
    Path p;
    if (table == nullptr || table->getNumFrames() == 0)
        return p;

    constexpr int points = 160;
    const auto& raw = table->getRawFrames();
    const int frames = table->getNumFrames();
    const int f0 = jlimit (0, frames - 1, (int) framePos);
    const int f1 = jmin (f0 + 1, frames - 1);
    const float frac = framePos - (float) f0;
    const float* a = raw.data() + (size_t) f0 * Wavetable::tableSize;
    const float* b = raw.data() + (size_t) f1 * Wavetable::tableSize;

    const auto o = geo.origin (depth);
    for (int i = 0; i <= points; ++i)
    {
        const int idx = jmin (Wavetable::tableSize - 1, i * Wavetable::tableSize / points);
        const float v = jlimit (-1.0f, 1.0f, a[idx] + frac * (b[idx] - a[idx]));
        const float x = o.x + geo.waveWidth * (float) i / (float) points;
        const float y = o.y - v * geo.amplitude;
        if (i == 0) p.startNewSubPath (x, y);
        else        p.lineTo (x, y);
    }
    return p;
}

void WavetableDisplay::rebuildStack()
{
    stack.clear();
    cachedTable = table.get();
    if (table == nullptr || table->getRawFrames().empty())
        return;

    const auto geo = geometry();
    const int frames = table->getNumFrames();
    const int layers = jmin (frames, 22);
    for (int l = layers - 1; l >= 0; --l)
    {
        const float t = layers > 1 ? (float) l / (float) (layers - 1) : 0.0f;
        stack.addPath (framePath (geo, t * (float) (frames - 1), t));
    }
}

void WavetableDisplay::timerCallback()
{
    auto current = processor.getOscTable (osc);
    if (current != table)
    {
        table = std::move (current);
        rebuildStack();
        repaint();
    }

    const float pos = processor.getDisplayedPosition (osc);
    if (std::abs (pos - position) > 1.0e-4f)
    {
        position = pos;
        repaint();
    }
}

void WavetableDisplay::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    paintWell (g, bounds);

    if (cachedTable != table.get())
        rebuildStack();

    if (table == nullptr)
    {
        g.setColour (Theme::textDim);
        g.drawText ("no wavetable", bounds, Justification::centred);
        return;
    }

    g.setColour (Theme::textDim.withAlpha (0.35f));
    g.strokePath (stack, PathStrokeType (1.0f));

    const auto geo = geometry();
    const float pos = jlimit (0.0f, 1.0f, position);
    const auto active = framePath (geo, pos * (float) (table->getNumFrames() - 1), pos);
    g.setColour (Theme::accent.withAlpha (0.25f));
    g.strokePath (active, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Theme::accent);
    g.strokePath (active, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setFont (Theme::font (11.0f));
    g.setColour (Theme::text);
    const auto textRow = bounds.reduced (8.0f, 4.0f).removeFromTop (14.0f);
    g.drawText (table->getName(), textRow, Justification::centredLeft, true);
    g.setColour (Theme::textDim);
    const int frame = roundToInt (pos * (float) (table->getNumFrames() - 1)) + 1;
    g.drawText ("Frame " + String (frame) + " / " + String (table->getNumFrames()), textRow, Justification::centredRight);

    if (dragHover)
    {
        g.setColour (Theme::accent.withAlpha (0.15f));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (Theme::accent);
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 2.0f);
        g.drawText ("Drop .wav to load", bounds, Justification::centred);
    }
}

bool WavetableDisplay::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac"))
            return true;
    return false;
}

void WavetableDisplay::filesDropped (const StringArray& files, int, int)
{
    dragHover = false;
    repaint();
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac"))
        {
            if (onFileDropped) onFileDropped (File (f));
            return;
        }
}

// ============================================================================
EnvelopeGraph::EnvelopeGraph (APVTS& state, int envIndex)
{
    const char* names[] { "a", "h", "d", "s", "r" };
    for (int i = 0; i < 5; ++i)
        params[(size_t) i] = state.getRawParameterValue (Params::env (envIndex, names[i]));
    timerCallback();
    startTimerHz (20);
}

void EnvelopeGraph::timerCallback()
{
    bool changed = false;
    for (size_t i = 0; i < 5; ++i)
    {
        const float v = params[i]->load();
        changed |= ! juce::exactlyEqual (v, last[i]);
        last[i] = v;
    }
    if (changed) repaint();
}

void EnvelopeGraph::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    paintWell (g, bounds);
    const auto r = bounds.reduced (8.0f, 8.0f);
    paintGrid (g, r, 8);

    const float a = last[0], h = last[1], d = last[2], s = last[3], rel = last[4];
    // Compress long times so short segments stay readable.
    auto seg = [] (float t) { return std::sqrt (jmax (0.0f, t)) + 0.04f; };
    const float sustainWidth = 0.6f;
    const float total = seg (a) + seg (h) + seg (d) + sustainWidth + seg (rel);
    const float scale = r.getWidth() / total;

    const float x0 = r.getX();
    const float xA = x0 + seg (a) * scale;
    const float xH = xA + seg (h) * scale;
    const float xD = xH + seg (d) * scale;
    const float xS = xD + sustainWidth * scale;
    const float xR = xS + seg (rel) * scale;
    const float top = r.getY(), bottom = r.getBottom();
    const float yS = bottom - s * r.getHeight();

    Path p;
    p.startNewSubPath (x0, bottom);
    p.lineTo (xA, top);
    p.lineTo (xH, top);
    p.quadraticTo (xH + (xD - xH) * 0.15f, yS, xD, yS);
    p.lineTo (xS, yS);
    p.quadraticTo (xS + (xR - xS) * 0.15f, bottom, xR, bottom);

    Path fill (p);
    fill.lineTo (x0, bottom);
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (Theme::accent.withAlpha (0.3f), 0.0f, top,
                                       Theme::accent.withAlpha (0.02f), 0.0f, bottom, false));
    g.fillPath (fill);
    g.setColour (Theme::accent);
    g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setColour (Theme::text);
    for (auto pt : { Point<float> (xA, top), Point<float> (xH, top), Point<float> (xD, yS), Point<float> (xS, yS) })
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (pt));
}

// ============================================================================
LfoPreview::LfoPreview (APVTS& state, int lfoIndex)
    : shape (state.getRawParameterValue (Params::lfo (lfoIndex, "shape")))
{
    timerCallback();
    startTimerHz (20);
}

void LfoPreview::timerCallback()
{
    const int s = Params::geti (shape);
    if (s != lastShape)
    {
        lastShape = s;
        repaint();
    }
}

void LfoPreview::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    paintWell (g, bounds);
    const auto r = bounds.reduced (8.0f, 10.0f);
    paintGrid (g, r, 4);

    constexpr int cycles = 2, points = 240;
    Random rng (1234);
    float held = rng.nextFloat() * 2.0f - 1.0f, next = rng.nextFloat() * 2.0f - 1.0f;
    int lastCycle = 0;

    Path p;
    for (int i = 0; i <= points; ++i)
    {
        const float t = (float) cycles * (float) i / (float) points;
        const int cycle = (int) t;
        const float ph = t - (float) cycle;
        if (cycle != lastCycle)
        {
            lastCycle = cycle;
            held = next;
            next = rng.nextFloat() * 2.0f - 1.0f;
        }

        float v = 0.0f;
        switch (lastShape)
        {
            case 0:  v = std::sin (MathConstants<float>::twoPi * ph); break;
            case 1:  v = ph < 0.25f ? 4.0f * ph : (ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f); break;
            case 2:  v = 2.0f * ph - 1.0f; break;
            case 3:  v = 1.0f - 2.0f * ph; break;
            case 4:  v = ph < 0.5f ? 1.0f : -1.0f; break;
            case 5:  v = held; break;
            default: v = held + (next - held) * (0.5f - 0.5f * std::cos (MathConstants<float>::pi * ph)); break;
        }

        const float x = r.getX() + r.getWidth() * (float) i / (float) points;
        const float y = r.getCentreY() - v * r.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (x, y);
        else        p.lineTo (x, y);
    }

    g.setColour (Theme::accent);
    g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
}

// ============================================================================
FilterCurve::FilterCurve (APVTS& state)
    : on (state.getRawParameterValue ("flt_on")),
      type (state.getRawParameterValue ("flt_type")),
      cutoff (state.getRawParameterValue ("flt_cutoff")),
      res (state.getRawParameterValue ("flt_res"))
{
    timerCallback();
    startTimerHz (20);
}

void FilterCurve::timerCallback()
{
    const std::array<float, 4> now { on->load(), type->load(), cutoff->load(), res->load() };
    if (now != last)
    {
        last = now;
        repaint();
    }
}

void FilterCurve::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    paintWell (g, bounds);
    const auto r = bounds.reduced (4.0f, 6.0f);

    const bool enabled = last[0] > 0.5f;
    const int t = (int) std::lround (last[1]);
    const float fc = jmax (20.0f, last[2]);
    const float k = 2.0f - 1.96f * jlimit (0.0f, 1.0f, last[3]);  // damping, 1/Q
    const bool steep = t == 1 || t == 3;

    constexpr float minDb = -36.0f, maxDb = 18.0f;
    Path p;
    constexpr int points = 120;
    for (int i = 0; i <= points; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / (float) points);
        const float w = f / fc;
        // Analog SVF prototype: H(s) = N(s) / (s^2 + k s + 1), s = j w.
        const std::complex<float> s (0.0f, w);
        const auto den = s * s + k * s + 1.0f;
        std::complex<float> num;
        switch (t)
        {
            case 0: case 1: num = 1.0f;          break;
            case 2: case 3: num = s * s;         break;
            case 4:         num = k * s;         break;
            default:        num = s * s + 1.0f;  break;
        }
        float mag = std::abs (num / den);
        if (steep) mag *= mag;
        const float db = jlimit (minDb, maxDb, Decibels::gainToDecibels (mag, minDb));
        const float x = r.getX() + r.getWidth() * (float) i / (float) points;
        const float y = jmap (db, maxDb, minDb, r.getY(), r.getBottom());
        if (i == 0) p.startNewSubPath (x, y);
        else        p.lineTo (x, y);
    }

    const auto colour = enabled ? Theme::accent : Theme::textDim;
    Path fill (p);
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setColour (colour.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (p, PathStrokeType (1.6f));
}
