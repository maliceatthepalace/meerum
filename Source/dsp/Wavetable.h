#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

// A morphable wavetable: N single-cycle frames of tableSize samples, each stored as
// numLevels band-limited mip levels (level L keeps harmonics 1 .. 1024 >> L), so playback
// at any pitch can pick a level whose highest harmonic stays below Nyquist.
class Wavetable
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numLevels = 11;
    static constexpr int maxFrames = 256;

    // Each inner vector is one cycle of any length (resampled to tableSize).
    Wavetable (const std::vector<std::vector<float>>& frames, juce::String name);

    // Serum-style WAV: consecutive 2048-sample frames; anything else is treated as one cycle
    // (short files) or chopped into 2048-sample frames. Returns nullptr and sets error on failure.
    static std::shared_ptr<Wavetable> fromAudioFile (const juce::File& file, juce::String& error);
    static std::shared_ptr<Wavetable> fromRawFrames (const juce::MemoryBlock& block, juce::String name);

    int getNumFrames() const noexcept               { return numFrames; }
    const juce::String& getName() const noexcept    { return name; }

    // tableSize + 1 samples (the last duplicates the first, for interpolation without wrapping).
    const float* getFrame (int frame, int level) const noexcept
    {
        return data.data() + ((size_t) frame * numLevels + (size_t) level) * (tableSize + 1);
    }

    // The level-0 frames concatenated, as used for state serialisation and display.
    const std::vector<float>& getRawFrames() const noexcept { return raw; }

    // Mip level for a phase increment (cycles per sample).
    static int levelForIncrement (double inc) noexcept
    {
        const double maxHarmonic = 0.5 / juce::jmax (inc, 1.0e-9);
        int level = 0;
        while (level < numLevels - 1 && (double) (1024 >> level) > maxHarmonic)
            ++level;
        return level;
    }

    // Interpolated read: framePos in [0, numFrames - 1], phase in [0, 1).
    float read (float framePos, float phase, int level) const noexcept
    {
        const int f0 = juce::jlimit (0, numFrames - 1, (int) framePos);
        const int f1 = juce::jmin (f0 + 1, numFrames - 1);
        const float ff = framePos - (float) f0;

        const float idx = phase * (float) tableSize;
        const int i0 = juce::jlimit (0, tableSize - 1, (int) idx);
        const float fi = idx - (float) i0;

        const float* a = getFrame (f0, level);
        const float sa = a[i0] + fi * (a[i0 + 1] - a[i0]);
        if (f1 == f0 || ff <= 0.0f)
            return sa;

        const float* b = getFrame (f1, level);
        const float sb = b[i0] + fi * (b[i0 + 1] - b[i0]);
        return sa + ff * (sb - sa);
    }

private:
    int numFrames = 0;
    juce::String name;
    std::vector<float> data;
    std::vector<float> raw;
};

namespace WavetableBank
{
    // Factory tables, in the order of Params::tableNames() (excluding "Custom").
    const std::vector<std::shared_ptr<const Wavetable>>& builtIns();
}
