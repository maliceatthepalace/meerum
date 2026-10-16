// Offline smoke test: renders every factory preset to WAV and checks the output is sane
// (no NaN/Inf, audible, not clipping hard), plus state round-trip and a CPU benchmark.
// Usage: MeerumRenderTest [output-dir]

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        if (! ok)
        {
            ++failures;
            std::cout << "  FAIL: " << what << std::endl;
        }
    }

    // Renders `seconds` of audio, sending note-ons at the start and note-offs at `releaseAt`.
    juce::AudioBuffer<float> render (MeerumProcessor& proc, const std::vector<int>& notes, double seconds, double releaseAt)
    {
        const int total = (int) (seconds * sampleRate);
        const int releaseSample = (int) (releaseAt * sampleRate);
        juce::AudioBuffer<float> out (2, total), block (2, blockSize);

        for (int pos = 0; pos < total; pos += blockSize)
        {
            const int n = juce::jmin (blockSize, total - pos);
            block.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;

            for (int note : notes)
            {
                if (pos == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
                if (releaseSample >= pos && releaseSample < pos + n)
                    midi.addEvent (juce::MidiMessage::noteOff (1, note), releaseSample - pos);
            }

            proc.processBlock (block, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, pos, block, ch, 0, n);
        }
        return out;
    }

    void writeWav (const juce::AudioBuffer<float>& buffer, const juce::File& file)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (file),
                                                                              sampleRate, 2, 24, {}, 0));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }

    void setParam (MeerumProcessor& proc, const juce::String& id, float value)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File outDir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                       : juce::File::getCurrentWorkingDirectory().getChildFile ("test-renders");
    outDir.createDirectory();

    MeerumProcessor proc;
    proc.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    // ---- every factory preset ----
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        proc.prepareToPlay (sampleRate, blockSize); // clears tails between presets
        const auto name = proc.getProgramName (i);
        const bool mono = proc.apvts.getRawParameterValue ("voices")->load() < 1.5f;
        const std::vector<int> notes = mono ? std::vector<int> { 36 } : std::vector<int> { 48, 55, 60, 64 };

        const auto buffer = render (proc, notes, 4.0, 2.0);
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < buffer.getNumSamples(); ++s)
                finite = finite && std::isfinite (buffer.getSample (ch, s));

        const float peak = buffer.getMagnitude (0, buffer.getNumSamples());
        const float rms = buffer.getRMSLevel (0, 0, buffer.getNumSamples());
        std::cout << juce::String (name).paddedRight (' ', 16) << " peak " << juce::String (peak, 3)
                  << "  rms " << juce::String (rms, 4) << std::endl;

        check (finite, name + ": output contains NaN/Inf");
        check (peak > 0.01f, name + ": output is (almost) silent");
        check (peak < 1.5f, name + ": output clips heavily");

        writeWav (buffer, outDir.getChildFile (juce::File::createLegalFileName (name) + ".wav"));
    }

    // ---- every built-in table at a few positions, with unison and warp ----
    proc.setCurrentProgram (0);
    for (int t = 0; t < Params::customTableIndex(); ++t)
        for (float pos : { 0.0f, 0.5f, 1.0f })
        {
            setParam (proc, Params::osc (0, "table"), (float) t);
            setParam (proc, Params::osc (0, "pos"), pos);
            setParam (proc, Params::osc (0, "unison"), 4);
            setParam (proc, Params::osc (0, "warp"), (float) (t % 7));
            setParam (proc, Params::osc (0, "warpAmt"), 0.5f);
            proc.prepareToPlay (sampleRate, blockSize);
            const auto buffer = render (proc, { 40, 96 }, 0.5, 0.4);
            const float peak = buffer.getMagnitude (0, buffer.getNumSamples());
            check (std::isfinite (peak) && peak > 0.001f && peak < 1.5f,
                   "table " + Params::tableNames()[t] + " pos " + juce::String (pos) + " peak " + juce::String (peak));
        }

    // ---- custom wavetable import + state round trip ----
    {
        const auto wavFile = outDir.getChildFile ("custom-table-input.wav");
        juce::AudioBuffer<float> table (1, Wavetable::tableSize * 4);
        for (int f = 0; f < 4; ++f)
            for (int i = 0; i < Wavetable::tableSize; ++i)
                table.setSample (0, f * Wavetable::tableSize + i,
                                 std::sin (juce::MathConstants<float>::twoPi * (float) (f + 1) * (float) i / Wavetable::tableSize));
        wavFile.deleteFile();
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (wavFile), sampleRate, 1, 24, {}, 0));
            writer->writeFromAudioSampleBuffer (table, 0, table.getNumSamples());
        }

        juce::String error;
        check (proc.loadCustomWavetable (1, wavFile, error), "custom table import: " + error);
        check (proc.getOscTable (1)->getNumFrames() == 4, "custom table should have 4 frames");

        setParam (proc, "flt_cutoff", 1234.0f);
        juce::MemoryBlock state;
        proc.getStateInformation (state);

        proc.setCurrentProgram (1);
        proc.setStateInformation (state.getData(), (int) state.getSize());
        check (std::abs (proc.apvts.getRawParameterValue ("flt_cutoff")->load() - 1234.0f) < 1.0f, "state restores cutoff");
        check (proc.getOscTable (1)->getNumFrames() == 4, "state restores the custom wavetable");
        wavFile.deleteFile();
    }

    // ---- CPU: 8 notes of a 7-voice unison supersaw, both oscillators ----
    {
        proc.setCurrentProgram (1);
        setParam (proc, Params::osc (1, "unison"), 7);
        proc.prepareToPlay (sampleRate, blockSize);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        render (proc, { 48, 52, 55, 59, 60, 64, 67, 71 }, 10.0, 9.0);
        const double elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        std::cout << "CPU: 10 s of 8-note, 2x7-unison audio rendered in " << juce::String (elapsed, 3)
                  << " s (" << juce::String (elapsed / 10.0 * 100.0, 1) << " % of one core)" << std::endl;
    }

    // ---- editor screenshots of every page (for visual review) ----
    {
        proc.setCurrentProgram (1);
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        auto snapshot = [&] (const juce::String& name)
        {
            juce::PNGImageFormat png;
            const auto file = outDir.getChildFile ("ui-" + name + ".png");
            file.deleteFile();
            juce::FileOutputStream stream (file);
            png.writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f), stream);
        };

        std::function<juce::Button* (juce::Component&, const juce::String&)> findButton;
        findButton = [&findButton] (juce::Component& c, const juce::String& text) -> juce::Button*
        {
            if (auto* b = dynamic_cast<juce::Button*> (&c); b != nullptr && b->getButtonText() == text)
                return b;
            for (auto* child : c.getChildren())
                if (auto* found = findButton (*child, text))
                    return found;
            return nullptr;
        };

        snapshot ("osc");
        for (const juce::String page : { "FX", "MATRIX" })
            if (auto* b = findButton (*editor, page))
            {
                b->setToggleState (true, juce::dontSendNotification); // what a real click does first
                if (b->onClick)
                    b->onClick();
                snapshot (page.toLowerCase());
            }
    }

    std::cout << (failures == 0 ? "ALL CHECKS PASSED" : juce::String (failures) + " CHECK(S) FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
