/*
    SkoomaTuner - VST3 Tuner Plugin
    Based on pitch detection from StompTuner (Hermann Meyer et al.)
    License: GPL-3.0
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

SkoomaTunerProcessor::SkoomaTunerProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::mono(), true)
                     .withOutput("Output", juce::AudioChannelSet::mono(), true))
{
}

void SkoomaTunerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    low_high_cut::Dsp::init_static(static_cast<uint32_t>(sampleRate), &lowHighCut);

    workBuffer.resize(static_cast<size_t>(samplesPerBlock));

    pitchTracker = std::make_unique<PitchTracker>([this]() {
        currentFreq.store(pitchTracker->get_estimated_freq(), std::memory_order_release);
    });
    pitchTracker->init(static_cast<unsigned int>(sampleRate));
}

void SkoomaTunerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!pitchTracker)
        return;

    auto* channelData = buffer.getReadPointer(0);
    int numSamples = buffer.getNumSamples();

    if (static_cast<size_t>(numSamples) > workBuffer.size())
        workBuffer.resize(static_cast<size_t>(numSamples));

    std::memcpy(workBuffer.data(), channelData, static_cast<size_t>(numSamples) * sizeof(float));
    low_high_cut::Dsp::compute_static(numSamples, workBuffer.data(), workBuffer.data(), &lowHighCut);
    pitchTracker->add(numSamples, workBuffer.data());
}

void SkoomaTunerProcessor::releaseResources()
{
    pitchTracker.reset();
}

juce::AudioProcessorEditor* SkoomaTunerProcessor::createEditor()
{
    return new SkoomaTunerEditor(*this);
}

void SkoomaTunerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    float ref = referenceFreq.load();
    uint8_t strobe = strobeMode.load() ? 1 : 0;
    uint8_t dark = darkMode.load() ? 1 : 0;
    destData.append(&ref, sizeof(float));
    destData.append(&strobe, sizeof(uint8_t));
    destData.append(&dark, sizeof(uint8_t));
}

void SkoomaTunerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto* bytes = static_cast<const char*>(data);

    if (sizeInBytes >= static_cast<int>(sizeof(float)))
    {
        float ref;
        std::memcpy(&ref, bytes, sizeof(float));
        if (ref >= 432.0f && ref <= 452.0f)
            referenceFreq.store(ref);
    }

    if (sizeInBytes >= static_cast<int>(sizeof(float) + 2))
    {
        strobeMode.store(bytes[sizeof(float)] != 0);
        darkMode.store(bytes[sizeof(float) + 1] != 0);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SkoomaTunerProcessor();
}
