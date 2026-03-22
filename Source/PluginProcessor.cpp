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

    pitchDsp = std::make_unique<tuner>([this]() {
        currentFreq.store(pitchDsp->get_freq(), std::memory_order_release);
    });
    pitchDsp->init(static_cast<unsigned int>(sampleRate));
    tuner::set_fast_note(*pitchDsp, true);
}

void SkoomaTunerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!pitchDsp)
        return;

    auto* channelData = buffer.getReadPointer(0);
    int numSamples = buffer.getNumSamples();

    if (static_cast<size_t>(numSamples) > workBuffer.size())
        workBuffer.resize(static_cast<size_t>(numSamples));

    std::memcpy(workBuffer.data(), channelData, static_cast<size_t>(numSamples) * sizeof(float));
    low_high_cut::Dsp::compute_static(numSamples, workBuffer.data(), workBuffer.data(), &lowHighCut);
    pitchDsp->feed_tuner(numSamples, workBuffer.data());
}

void SkoomaTunerProcessor::releaseResources()
{
    pitchDsp.reset();
}

juce::AudioProcessorEditor* SkoomaTunerProcessor::createEditor()
{
    return new SkoomaTunerEditor(*this);
}

void SkoomaTunerProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    float ref = referenceFreq.load();
    destData.append(&ref, sizeof(float));
}

void SkoomaTunerProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (sizeInBytes >= static_cast<int>(sizeof(float)))
    {
        float ref;
        std::memcpy(&ref, data, sizeof(float));
        if (ref >= 432.0f && ref <= 452.0f)
            referenceFreq.store(ref);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SkoomaTunerProcessor();
}
