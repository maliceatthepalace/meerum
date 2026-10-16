#include "Wavetable.h"
#include <complex>

namespace
{
    using Complex = std::complex<double>;

    // Small in-place radix-2 FFT. JUCE's dsp::FFT picks a platform engine whose scaling
    // differs between backends, so we use our own to get identical tables everywhere.
    void fft (std::vector<Complex>& x, bool inverse)
    {
        const size_t n = x.size();

        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap (x[i], x[j]);
        }

        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double angle = juce::MathConstants<double>::twoPi / (double) len * (inverse ? 1.0 : -1.0);
            const Complex wlen (std::cos (angle), std::sin (angle));

            for (size_t i = 0; i < n; i += len)
            {
                Complex w (1.0);
                for (size_t k = 0; k < len / 2; ++k)
                {
                    const Complex u = x[i + k];
                    const Complex v = x[i + k + len / 2] * w;
                    x[i + k] = u + v;
                    x[i + k + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }

        if (inverse)
            for (auto& c : x)
                c /= (double) n;
    }

    std::vector<float> resampleCycle (const std::vector<float>& in)
    {
        constexpr int n = Wavetable::tableSize;
        std::vector<float> out ((size_t) n, 0.0f);

        if (in.empty())
            return out;

        if ((int) in.size() == n)
            return in;

        const double step = (double) in.size() / n;
        for (int i = 0; i < n; ++i)
        {
            const double pos = i * step;
            const size_t i0 = (size_t) pos % in.size();
            const size_t i1 = (i0 + 1) % in.size();
            const double frac = pos - std::floor (pos);
            out[(size_t) i] = (float) (in[i0] + frac * (in[i1] - in[i0]));
        }
        return out;
    }
}

Wavetable::Wavetable (const std::vector<std::vector<float>>& frames, juce::String tableName)
    : name (std::move (tableName))
{
    numFrames = juce::jlimit (1, maxFrames, (int) frames.size());

    constexpr int stride = tableSize + 1;
    data.assign ((size_t) numFrames * numLevels * stride, 0.0f);
    raw.assign ((size_t) numFrames * tableSize, 0.0f);

    std::vector<Complex> spectrum ((size_t) tableSize), work ((size_t) tableSize);

    for (int f = 0; f < numFrames; ++f)
    {
        const auto cycle = resampleCycle (f < (int) frames.size() ? frames[(size_t) f] : std::vector<float>());

        for (int i = 0; i < tableSize; ++i)
            spectrum[(size_t) i] = Complex (cycle[(size_t) i], 0.0);
        fft (spectrum, false);

        for (int level = 0; level < numLevels; ++level)
        {
            const int maxHarmonic = juce::jmin (tableSize / 2 - 1, (tableSize / 2) >> level);

            std::fill (work.begin(), work.end(), Complex());
            for (int k = 1; k <= maxHarmonic; ++k)
            {
                work[(size_t) k] = spectrum[(size_t) k];
                work[(size_t) (tableSize - k)] = spectrum[(size_t) (tableSize - k)];
            }
            fft (work, true);

            float* dest = data.data() + ((size_t) f * numLevels + (size_t) level) * stride;
            for (int i = 0; i < tableSize; ++i)
                dest[i] = (float) work[(size_t) i].real();
            dest[tableSize] = dest[0];
        }
    }

    // Normalise the whole table (not per frame) so the morph keeps its relative dynamics.
    float peak = 0.0f;
    for (int f = 0; f < numFrames; ++f)
    {
        const float* level0 = getFrame (f, 0);
        for (int i = 0; i < tableSize; ++i)
            peak = juce::jmax (peak, std::abs (level0[i]));
    }

    if (peak > 1.0e-6f)
        for (auto& s : data)
            s /= peak;

    for (int f = 0; f < numFrames; ++f)
        std::copy (getFrame (f, 0), getFrame (f, 0) + tableSize, raw.begin() + (ptrdiff_t) f * tableSize);
}

std::shared_ptr<Wavetable> Wavetable::fromAudioFile (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Could not read audio file: " + file.getFileName();
        return nullptr;
    }

    const auto maxSamples = (juce::int64) maxFrames * tableSize * 4;
    const int length = (int) juce::jmin (reader->lengthInSamples, maxSamples);
    if (length < 8)
    {
        error = "Audio file is too short.";
        return nullptr;
    }

    juce::AudioBuffer<float> buffer ((int) reader->numChannels, length);
    reader->read (&buffer, 0, length, 0, true, true);

    std::vector<float> mono ((size_t) length, 0.0f);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < length; ++i)
            mono[(size_t) i] += buffer.getSample (ch, i) / (float) buffer.getNumChannels();

    std::vector<std::vector<float>> frames;

    if (length % tableSize != 0 && length <= tableSize * 2)
    {
        frames.push_back (mono); // a single cycle of arbitrary length
    }
    else
    {
        const int available = length / tableSize;
        const int count = juce::jmin (available, maxFrames);

        for (int f = 0; f < count; ++f)
        {
            // Pick evenly spaced frames when the file holds more than we keep.
            const int src = count == available ? f : juce::roundToInt ((double) f * (available - 1) / juce::jmax (1, count - 1));
            const auto start = mono.begin() + (ptrdiff_t) src * tableSize;
            frames.emplace_back (start, start + tableSize);
        }
    }

    return std::make_shared<Wavetable> (frames, file.getFileNameWithoutExtension());
}

std::shared_ptr<Wavetable> Wavetable::fromRawFrames (const juce::MemoryBlock& block, juce::String tableName)
{
    const int numSamples = (int) (block.getSize() / sizeof (float));
    const int count = juce::jmin (maxFrames, numSamples / tableSize);
    if (count < 1)
        return nullptr;

    auto* samples = static_cast<const float*> (block.getData());
    std::vector<std::vector<float>> frames;
    for (int f = 0; f < count; ++f)
        frames.emplace_back (samples + f * tableSize, samples + (f + 1) * tableSize);

    return std::make_shared<Wavetable> (frames, std::move (tableName));
}

//==============================================================================
namespace WavetableBank
{
namespace
{
    constexpr int builtInFrames = 64;
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // Builds a table by evaluating fn (morph 0..1, phase 0..1) for every frame and sample.
    template <typename Fn>
    std::shared_ptr<const Wavetable> generate (const juce::String& name, Fn&& fn)
    {
        std::vector<std::vector<float>> frames ((size_t) builtInFrames);
        for (int f = 0; f < builtInFrames; ++f)
        {
            const double morph = (double) f / (builtInFrames - 1);
            auto& frame = frames[(size_t) f];
            frame.resize ((size_t) Wavetable::tableSize);
            for (int i = 0; i < Wavetable::tableSize; ++i)
                frame[(size_t) i] = (float) fn (morph, (double) i / Wavetable::tableSize);
        }
        return std::make_shared<const Wavetable> (frames, name);
    }

    double additive (double p, int maxHarmonic, const std::function<double (int)>& amp)
    {
        double s = 0.0;
        for (int k = 1; k <= maxHarmonic; ++k)
            s += amp (k) * std::sin (twoPi * k * p);
        return s;
    }

    double saw (double p)      { return 2.0 * p - 1.0; }
    double square (double p)   { return p < 0.5 ? 1.0 : -1.0; }
    double triangle (double p) { return 1.0 - 4.0 * std::abs (p - 0.5); }

    std::vector<std::shared_ptr<const Wavetable>> buildAll()
    {
        std::vector<std::shared_ptr<const Wavetable>> tables;

        // Sine -> triangle -> saw -> square.
        tables.push_back (generate ("Basic Shapes", [] (double m, double p)
        {
            const double sine = std::sin (twoPi * p);
            const double tri = triangle (std::fmod (p + 0.25, 1.0));
            const double seg = m * 3.0;
            if (seg < 1.0) return juce::jmap (seg, sine, tri);
            if (seg < 2.0) return juce::jmap (seg - 1.0, tri, saw (p));
            return juce::jmap (seg - 2.0, saw (p), square (p));
        }));

        tables.push_back (generate ("Pulse Width", [] (double m, double p)
        {
            const double width = 0.5 - 0.47 * m;
            return p < width ? 1.0 : -1.0;
        }));

        // Adds harmonics one by one, with the newest fading in.
        tables.push_back (generate ("Harmonic Sweep", [] (double m, double p)
        {
            const double count = 1.0 + m * 47.0;
            const int whole = (int) count;
            return additive (p, whole + 1, [count, whole] (int k)
            {
                const double a = 1.0 / k;
                return k <= whole ? a : a * (count - whole);
            });
        }));

        // Formant pairs morphing through A - E - I - O - U.
        tables.push_back (generate ("Vowels", [] (double m, double p)
        {
            static const double f1[] { 800, 400, 300, 500, 330 };
            static const double f2[] { 1150, 1600, 2300, 900, 800 };
            const double pos = m * 4.0;
            const int v = juce::jmin (3, (int) pos);
            const double t = pos - v;
            const double a = juce::jmap (t, f1[v], f1[v + 1]);
            const double b = juce::jmap (t, f2[v], f2[v + 1]);
            return additive (p, 40, [a, b] (int k)
            {
                const double f = k * 110.0; // formants are placed relative to a 110 Hz fundamental
                return std::exp (-std::pow ((f - a) / 120.0, 2.0))
                     + 0.7 * std::exp (-std::pow ((f - b) / 160.0, 2.0))
                     + 0.08 / k;
            });
        }));

        // Hard-synced saw, slave ratio 1 -> 8.
        tables.push_back (generate ("Sync Saw", [] (double m, double p)
        {
            const double ratio = 1.0 + m * 7.0;
            return saw (std::fmod (p * ratio, 1.0)) * (1.0 - 0.5 * p); // fade hides the sync edge
        }));

        // Two-operator FM with rising index.
        tables.push_back (generate ("FM Bells", [] (double m, double p)
        {
            const double index = m * 6.0;
            return std::sin (twoPi * p + index * std::sin (twoPi * 3.0 * p));
        }));

        tables.push_back (generate ("Wavefold", [] (double m, double p)
        {
            const double gain = 1.0 + m * 9.0;
            return std::sin (gain * std::sin (twoPi * p));
        }));

        tables.push_back (generate ("Digital Steps", [] (double m, double p)
        {
            const double steps = juce::jmax (2.0, std::round (32.0 - m * 30.0));
            const double s = std::sin (twoPi * p) * 0.5 + 0.5 * saw (p) * m;
            return std::round (s * steps) / steps;
        }));

        return tables;
    }
}

const std::vector<std::shared_ptr<const Wavetable>>& builtIns()
{
    static const auto tables = buildAll();
    return tables;
}
}
