#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"
#include "dsp/Wavetable.h"
#include "dsp/SynthEngine.h"
#include "dsp/Effects.h"

class MeerumProcessor : public juce::AudioProcessor
{
public:
    MeerumProcessor();
    ~MeerumProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return JucePlugin_Name; }
    bool acceptsMidi() const override                   { return true; }
    bool producesMidi() const override                  { return false; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 5.0; }

    // Factory presets are exposed to the host as programs.
    int getNumPrograms() override;
    int getCurrentProgram() override                    { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- used by the editor -------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // The table an oscillator currently plays (built-in, or its imported WAV).
    std::shared_ptr<const Wavetable> getOscTable (int osc) const;
    bool loadCustomWavetable (int osc, const juce::File& file, juce::String& error);
    juce::String getCustomTableName (int osc) const;

    // Modulated WT position (0..1) of the most recently triggered voice, for display.
    float getDisplayedPosition (int osc) const          { return engine.displayedPosition[osc].load(); }

    juce::File getPresetFolder() const;
    bool savePreset (const juce::File& file);
    bool loadPreset (const juce::File& file);
    juce::String getCurrentPresetName() const           { return currentPresetName; }

    // Incremented whenever a preset/program/state is loaded, so the UI can refresh.
    std::atomic<int> presetGeneration { 0 };

private:
    juce::ValueTree createStateTree();
    void restoreFromTree (const juce::ValueTree& tree);
    void setCustomTable (int osc, std::shared_ptr<Wavetable> table);

    Params::ParamCache params;
    SynthEngine engine;
    FxChain fx;

    mutable juce::SpinLock tableLock;
    std::shared_ptr<Wavetable> customTables[Params::numOscs];
    juce::CriticalSection retiredLock;
    std::vector<std::shared_ptr<Wavetable>> retiredTables;

    float lastMasterGain = 0.0f;

    int currentProgram = 0;
    juce::String currentPresetName { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeerumProcessor)
};
