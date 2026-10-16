#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

namespace
{
    const juce::Identifier stateType ("MeerumState");
    const juce::Identifier customTableType ("CustomTable");
    constexpr const char* presetExtension = ".mrpreset";
}

MeerumProcessor::MeerumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", Params::createLayout())
{
    params.init (apvts);
    WavetableBank::builtIns(); // build the factory tables now rather than on the audio thread
}

bool MeerumProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MeerumProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

std::shared_ptr<const Wavetable> MeerumProcessor::getOscTable (int osc) const
{
    const int index = Params::geti (params.osc[osc].table);
    if (index == Params::customTableIndex())
    {
        const juce::SpinLock::ScopedLockType lock (tableLock);
        if (customTables[osc] != nullptr)
            return customTables[osc];
    }

    const auto& tables = WavetableBank::builtIns();
    return tables[(size_t) juce::jlimit (0, (int) tables.size() - 1, index)];
}

void MeerumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto hostBpm = position->getBpm())
                bpm = *hostBpm;

    // Hold references for the whole block so a table swapped by the UI stays alive.
    std::shared_ptr<const Wavetable> held[Params::numOscs];
    const Wavetable* tables[Params::numOscs];
    for (int i = 0; i < Params::numOscs; ++i)
    {
        held[i] = getOscTable (i);
        tables[i] = held[i].get();
    }

    engine.process (buffer, midi, params, tables, bpm);
    fx.process (buffer, params, bpm);

    const float gain = Params::get (params.master);
    buffer.applyGainRamp (0, buffer.getNumSamples(), lastMasterGain, gain);
    lastMasterGain = gain;
}

juce::AudioProcessorEditor* MeerumProcessor::createEditor()
{
    return new MeerumEditor (*this);
}

//==============================================================================
bool MeerumProcessor::loadCustomWavetable (int osc, const juce::File& file, juce::String& error)
{
    auto table = Wavetable::fromAudioFile (file, error);
    if (table == nullptr)
        return false;

    setCustomTable (osc, std::move (table));

    auto* param = apvts.getParameter (Params::osc (osc, "table"));
    param->setValueNotifyingHost (param->convertTo0to1 ((float) Params::customTableIndex()));
    return true;
}

void MeerumProcessor::setCustomTable (int osc, std::shared_ptr<Wavetable> table)
{
    std::shared_ptr<Wavetable> old;
    {
        const juce::SpinLock::ScopedLockType lock (tableLock);
        old = std::exchange (customTables[osc], std::move (table));
    }

    // The audio thread may still be rendering with the old table. Keep it alive here and
    // free it on a later call, once nobody else references it, so the audio thread never
    // ends up deallocating it.
    const juce::ScopedLock lock (retiredLock);
    if (old != nullptr)
        retiredTables.push_back (std::move (old));
    retiredTables.erase (std::remove_if (retiredTables.begin(), retiredTables.end(),
                                         [] (const auto& t) { return t.use_count() == 1; }),
                         retiredTables.end());
}

juce::String MeerumProcessor::getCustomTableName (int osc) const
{
    const juce::SpinLock::ScopedLockType lock (tableLock);
    return customTables[osc] != nullptr ? customTables[osc]->getName() : juce::String();
}

//==============================================================================
juce::ValueTree MeerumProcessor::createStateTree()
{
    juce::ValueTree tree (stateType);
    tree.setProperty ("presetName", currentPresetName, nullptr);
    tree.appendChild (apvts.copyState(), nullptr);

    for (int i = 0; i < Params::numOscs; ++i)
    {
        std::shared_ptr<Wavetable> table;
        {
            const juce::SpinLock::ScopedLockType lock (tableLock);
            table = customTables[i];
        }
        if (table == nullptr)
            continue;

        const auto& raw = table->getRawFrames();
        juce::MemoryBlock block (raw.data(), raw.size() * sizeof (float));
        juce::ValueTree t (customTableType);
        t.setProperty ("osc", i, nullptr);
        t.setProperty ("name", table->getName(), nullptr);
        t.setProperty ("frames", block.toBase64Encoding(), nullptr);
        tree.appendChild (t, nullptr);
    }
    return tree;
}

void MeerumProcessor::restoreFromTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (stateType))
        return;

    auto paramState = tree.getChildWithName (apvts.state.getType());
    if (paramState.isValid())
        apvts.replaceState (paramState);

    // APVTS skips parameters whose stored value already matches, which can leave a stepped
    // parameter (bool/choice) holding an off-grid normalised value. Snap all to the grid.
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const float snapped = ranged->convertTo0to1 (ranged->convertFrom0to1 (ranged->getValue()));
            if (snapped != ranged->getValue())
                ranged->setValueNotifyingHost (snapped);
        }

    for (int i = 0; i < Params::numOscs; ++i)
        setCustomTable (i, nullptr);

    for (const auto& child : tree)
    {
        if (! child.hasType (customTableType))
            continue;

        juce::MemoryBlock block;
        const int osc = child["osc"];
        if (osc >= 0 && osc < Params::numOscs && block.fromBase64Encoding (child["frames"].toString()))
            setCustomTable (osc, Wavetable::fromRawFrames (block, child["name"].toString()));
    }

    currentPresetName = tree.getProperty ("presetName", "Init").toString();
    ++presetGeneration;
}

void MeerumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void MeerumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreFromTree (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
int MeerumProcessor::getNumPrograms()
{
    return (int) FactoryPresets::all().size();
}

const juce::String MeerumProcessor::getProgramName (int index)
{
    const auto& presets = FactoryPresets::all();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void MeerumProcessor::setCurrentProgram (int index)
{
    const auto& presets = FactoryPresets::all();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;
    const auto& preset = presets[(size_t) index];

    // Start from defaults so every preset is self-contained.
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    for (const auto& [id, value] : preset.values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));

    for (int i = 0; i < Params::numOscs; ++i)
        setCustomTable (i, nullptr);

    currentPresetName = preset.name;
    ++presetGeneration;
}

juce::File MeerumProcessor::getPresetFolder() const
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("Meerum").getChildFile ("Presets");
    folder.createDirectory();
    return folder;
}

bool MeerumProcessor::savePreset (const juce::File& target)
{
    const auto file = target.withFileExtension (presetExtension);
    currentPresetName = file.getFileNameWithoutExtension();
    ++presetGeneration;

    if (auto xml = createStateTree().createXml())
        return xml->writeTo (file);
    return false;
}

bool MeerumProcessor::loadPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (stateType))
        return false;

    restoreFromTree (tree);
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MeerumProcessor();
}
