/*
    SkoomaTuner - VST3 Tuner Plugin
    Based on pitch detection from StompTuner (Hermann Meyer et al.)
    License: GPL-3.0
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/low_high_cut.h"
#include "dsp/tuner.hpp"
#include <atomic>
#include <memory>
#include <vector>

class SkoomaTunerProcessor : public juce::AudioProcessor
{
public:
    SkoomaTunerProcessor();
    ~SkoomaTunerProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    std::atomic<float> currentFreq{0.0f};
    std::atomic<float> referenceFreq{440.0f};

private:
    low_high_cut::Dsp lowHighCut;
    std::unique_ptr<tuner> pitchDsp;
    std::vector<float> workBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkoomaTunerProcessor)
};
