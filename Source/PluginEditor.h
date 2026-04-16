/*
    skoomaTuner - VST3 Tuner Plugin
    License: GPL-3.0
*/

#pragma once

#include "PluginProcessor.h"

class SkoomaTunerEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit SkoomaTunerEditor(SkoomaTunerProcessor&);
    ~SkoomaTunerEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    SkoomaTunerProcessor& processor;

    juce::Font monoFont;
    std::unique_ptr<juce::Drawable> iconMeter, iconTheme;
    juce::ComponentBoundsConstrainer constrainer;

    float displayFreq = 0.0f;
    float displayCents = 0.0f;
    int displayNoteIndex = 0;
    int displayOctave = 4;
    float smoothedCents = 0.0f;
    float needleVelocity = 0.0f;
    float strobePhase = 0.0f;

    static constexpr const char* noteNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkoomaTunerEditor)
};
