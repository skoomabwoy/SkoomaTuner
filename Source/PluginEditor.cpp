/*
    SkoomaTuner - VST3 Tuner Plugin
    License: GPL-3.0
*/

#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

SkoomaTunerEditor::SkoomaTunerEditor(SkoomaTunerProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    auto typeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::JetBrainsMonoBoldsubset_ttf,
        BinaryData::JetBrainsMonoBoldsubset_ttfSize);
    monoFont = juce::Font(juce::FontOptions(typeface));

    constrainer.setFixedAspectRatio(1.0);
    constrainer.setMinimumSize(200, 200);
    constrainer.setMaximumSize(800, 800);
    setConstrainer(&constrainer);
    setResizable(true, true);
    setSize(300, 300);

    startTimerHz(30);
}

SkoomaTunerEditor::~SkoomaTunerEditor()
{
    stopTimer();
}

void SkoomaTunerEditor::timerCallback()
{
    float freq = processor.currentFreq.load(std::memory_order_acquire);
    float refFreq = processor.referenceFreq.load(std::memory_order_acquire);

    displayFreq = freq;

    if (freq > 0.0f)
    {
        float midiNote = 12.0f * std::log2(freq / refFreq) + 69.0f;
        int nearestMidi = static_cast<int>(std::round(midiNote));
        float cents = (midiNote - static_cast<float>(nearestMidi)) * 100.0f;

        displayNoteIndex = ((nearestMidi % 12) + 12) % 12;
        displayOctave = (nearestMidi / 12) - 1;
        displayCents = cents;

        smoothedCents += (cents - smoothedCents) * 0.3f;
    }
    else
    {
        smoothedCents *= 0.85f;
    }

    repaint();
}

void SkoomaTunerEditor::paint(juce::Graphics& g)
{
    float w = static_cast<float>(getWidth());
    float scale = w / 300.0f;

    g.fillAll(juce::Colour(0xff1a1a2e));

    // --- Gauge ---
    float cx = w * 0.5f;
    float cy = w * 0.48f;
    float radius = w * 0.38f;

    float arcStart = juce::MathConstants<float>::pi * 0.75f;
    float arcSpan = juce::MathConstants<float>::pi * 1.5f;

    for (int i = -50; i <= 50; i += 5)
    {
        float frac = (static_cast<float>(i) + 50.0f) / 100.0f;
        float angle = arcStart + frac * arcSpan;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        bool isMajor = (i % 10 == 0);
        bool isCenter = (i == 0);

        float innerR = radius * (isCenter ? 0.72f : isMajor ? 0.78f : 0.85f);

        float x1 = cx + innerR * cosA;
        float y1 = cy + innerR * sinA;
        float x2 = cx + radius * cosA;
        float y2 = cy + radius * sinA;

        if (isCenter)
        {
            g.setColour(juce::Colour(0xff00ff88));
            g.drawLine(x1, y1, x2, y2, 2.5f * scale);
        }
        else if (isMajor)
        {
            g.setColour(juce::Colour(0xffcccccc));
            g.drawLine(x1, y1, x2, y2, 1.5f * scale);
        }
        else
        {
            g.setColour(juce::Colour(0xff555555));
            g.drawLine(x1, y1, x2, y2, 1.0f * scale);
        }
    }

    // Cent labels: -50 and +50
    g.setFont(monoFont.withHeight(11.0f * scale));
    g.setColour(juce::Colour(0xff888888));
    for (int val : { -50, 50 })
    {
        float frac = (static_cast<float>(val) + 50.0f) / 100.0f;
        float angle = arcStart + frac * arcSpan;
        float labelR = radius * 0.65f;
        float lx = cx + labelR * std::cos(angle);
        float ly = cy + labelR * std::sin(angle);

        juce::String labelText = (val > 0) ? ("+" + juce::String(val)) : juce::String(val);
        float lw = 36.0f * scale;
        float lh = 14.0f * scale;
        g.drawText(labelText, juce::Rectangle<float>(lx - lw * 0.5f, ly - lh * 0.5f, lw, lh),
                   juce::Justification::centred, false);
    }

    // --- Needle ---
    float needleCents = std::clamp(smoothedCents, -50.0f, 50.0f);
    float needleFrac = (needleCents + 50.0f) / 100.0f;
    float needleAngle = arcStart + needleFrac * arcSpan;

    float nx = cx + radius * 0.92f * std::cos(needleAngle);
    float ny = cy + radius * 0.92f * std::sin(needleAngle);

    float absC = std::abs(smoothedCents);
    juce::Colour needleColour;
    if (displayFreq <= 0.0f)
        needleColour = juce::Colour(0xff444444);
    else if (absC < 3.0f)
        needleColour = juce::Colour(0xff00ff88);
    else if (absC < 15.0f)
        needleColour = juce::Colour(0xffffaa00);
    else
        needleColour = juce::Colour(0xffff4444);

    g.setColour(needleColour);
    g.drawLine(cx, cy, nx, ny, 2.0f * scale);

    float dotR = 5.0f * scale;
    g.fillEllipse(cx - dotR, cy - dotR, dotR * 2, dotR * 2);

    // --- Text displays (only when signal present) ---
    if (displayFreq > 0.0f)
    {
        float noteY = cy + 20.0f * scale;
        float noteH = 48.0f * scale;

        g.setColour(needleColour);
        g.setFont(monoFont.withHeight(noteH));
        juce::String noteStr = juce::String(noteNames[displayNoteIndex]) + juce::String(displayOctave);
        g.drawText(noteStr, juce::Rectangle<float>(0, noteY, w, noteH * 1.1f),
                   juce::Justification::centred, false);

        float freqY = noteY + noteH * 1.15f;
        float freqH = 16.0f * scale;
        g.setColour(juce::Colour(0xffaaaaaa));
        g.setFont(monoFont.withHeight(freqH));
        g.drawText(juce::String(displayFreq, 1) + " Hz",
                   juce::Rectangle<float>(0, freqY, w, freqH * 1.3f),
                   juce::Justification::centred, false);

        float centsY = freqY + freqH * 1.4f;
        float centsH = 14.0f * scale;
        int centsInt = static_cast<int>(std::round(displayCents));
        g.setColour(needleColour);
        g.setFont(monoFont.withHeight(centsH));
        g.drawText((centsInt >= 0 ? "+" : "") + juce::String(centsInt) + " cents",
                   juce::Rectangle<float>(0, centsY, w, centsH * 1.3f),
                   juce::Justification::centred, false);
    }
}

void SkoomaTunerEditor::resized()
{
}
