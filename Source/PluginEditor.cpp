/*
    SkoomaTuner - VST3 Tuner Plugin
    License: GPL-3.0
*/

#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

namespace {

struct Theme {
    juce::Colour background;
    juce::Colour tickMinor, tickMajor, tickCenter;
    juce::Colour tuneGreen, tuneOrange, tuneRed, tuneNone;
    juce::Colour labelText, freqText;
    juce::Colour strobeBandBg, strobeBorder;
    juce::Colour toggleBg, toggleBorder, toggleIcon;
};

const Theme darkTheme = {
    juce::Colour(0xff1a1a2e),   // background
    juce::Colour(0xff555555),   // tickMinor
    juce::Colour(0xffcccccc),   // tickMajor
    juce::Colour(0xff00ff88),   // tickCenter
    juce::Colour(0xff00ff88),   // tuneGreen
    juce::Colour(0xffffaa00),   // tuneOrange
    juce::Colour(0xffff4444),   // tuneRed
    juce::Colour(0xff444444),   // tuneNone
    juce::Colour(0xff888888),   // labelText
    juce::Colour(0xffaaaaaa),   // freqText
    juce::Colour(0xff0d0d1a),   // strobeBandBg
    juce::Colour(0xff333344),   // strobeBorder
    juce::Colour(0xff2a2a3e),   // toggleBg
    juce::Colour(0xff444455),   // toggleBorder
    juce::Colour(0xff999999),   // toggleIcon
};

const Theme lightTheme = {
    juce::Colour(0xfff2f2f7),   // background
    juce::Colour(0xffcccccc),   // tickMinor
    juce::Colour(0xff555555),   // tickMajor
    juce::Colour(0xff00aa55),   // tickCenter
    juce::Colour(0xff00aa55),   // tuneGreen
    juce::Colour(0xffdd8800),   // tuneOrange
    juce::Colour(0xffdd2222),   // tuneRed
    juce::Colour(0xffcccccc),   // tuneNone
    juce::Colour(0xff888888),   // labelText
    juce::Colour(0xff666666),   // freqText
    juce::Colour(0xffe6e6ee),   // strobeBandBg
    juce::Colour(0xffccccdd),   // strobeBorder
    juce::Colour(0xffe0e0ea),   // toggleBg
    juce::Colour(0xffbbbbcc),   // toggleBorder
    juce::Colour(0xff777777),   // toggleIcon
};

} // anonymous namespace

SkoomaTunerEditor::SkoomaTunerEditor(SkoomaTunerProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    auto typeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::JetBrainsMonoBoldsubset_ttf,
        BinaryData::JetBrainsMonoBoldsubset_ttfSize);
    monoFont = juce::Font(juce::FontOptions(typeface));

    auto iconTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::fasolidsubset_ttf,
        BinaryData::fasolidsubset_ttfSize);
    iconFont = juce::Font(juce::FontOptions(iconTypeface));

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

    float targetCents = 0.0f;

    if (freq > 0.0f)
    {
        float midiNote = 12.0f * std::log2(freq / refFreq) + 69.0f;
        int nearestMidi = static_cast<int>(std::round(midiNote));
        float cents = (midiNote - static_cast<float>(nearestMidi)) * 100.0f;

        displayNoteIndex = ((nearestMidi % 12) + 12) % 12;
        displayOctave = (nearestMidi / 12) - 1;
        displayCents = cents;
        targetCents = cents;
    }

    // Spring-damper needle physics (slightly underdamped)
    constexpr float dt = 1.0f / 30.0f;
    constexpr float springK = 150.0f;
    constexpr float damping = 20.0f;
    float springForce = -springK * (smoothedCents - targetCents) - damping * needleVelocity;
    needleVelocity += springForce * dt;
    smoothedCents += needleVelocity * dt;

    // Strobe phase accumulation
    if (freq > 0.0f)
        strobePhase += displayCents * 0.12f;

    repaint();
}

void SkoomaTunerEditor::paint(juce::Graphics& g)
{
    const bool dark = processor.darkMode.load();
    const bool strobe = processor.strobeMode.load();
    const auto& t = dark ? darkTheme : lightTheme;

    float w = static_cast<float>(getWidth());
    float scale = w / 300.0f;

    g.fillAll(t.background);

    float cx = w * 0.5f;
    float cy = w * 0.48f;
    float radius = w * 0.38f;

    // Tuning color
    float absC = std::abs(smoothedCents);
    juce::Colour tuneColour;
    if (displayFreq <= 0.0f)
        tuneColour = t.tuneNone;
    else if (absC < 3.0f)
        tuneColour = t.tuneGreen;
    else if (absC < 15.0f)
        tuneColour = t.tuneOrange;
    else
        tuneColour = t.tuneRed;

    if (!strobe)
    {
        // --- Gauge ---
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
                g.setColour(t.tickCenter);
                g.drawLine(x1, y1, x2, y2, 2.5f * scale);
            }
            else if (isMajor)
            {
                g.setColour(t.tickMajor);
                g.drawLine(x1, y1, x2, y2, 1.5f * scale);
            }
            else
            {
                g.setColour(t.tickMinor);
                g.drawLine(x1, y1, x2, y2, 1.0f * scale);
            }
        }

        // Cent labels: -50 and +50
        g.setFont(monoFont.withHeight(11.0f * scale));
        g.setColour(t.labelText);
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

        g.setColour(tuneColour);
        g.drawLine(cx, cy, nx, ny, 2.0f * scale);

        float dotR = 5.0f * scale;
        g.fillEllipse(cx - dotR, cy - dotR, dotR * 2, dotR * 2);
    }
    else
    {
        // --- Strobe ---
        float bandW = w * 0.75f;
        float bandH = w * 0.16f;
        float bandX = (w - bandW) * 0.5f;
        float bandY = cy - bandH * 0.5f;

        g.setColour(t.strobeBandBg);
        g.fillRoundedRectangle(bandX, bandY, bandW, bandH, 4.0f * scale);

        float barW = 14.0f * scale;
        float period = barW * 2.0f;
        float rawOffset = strobePhase * scale;
        float phaseOffset = std::fmod(std::fmod(rawOffset, period) + period, period);

        g.saveState();
        g.reduceClipRegion(juce::Rectangle<int>(
            static_cast<int>(bandX + 1), static_cast<int>(bandY + 1),
            static_cast<int>(bandW - 2), static_cast<int>(bandH - 2)));

        float inset = 3.0f * scale;
        for (float x = bandX - period + phaseOffset; x < bandX + bandW + period; x += period)
        {
            g.setColour(tuneColour.withAlpha(0.85f));
            g.fillRect(x, bandY + inset, barW, bandH - inset * 2.0f);
        }

        g.restoreState();

        // Center markers and border use tickMajor for contrast
        float mw = 4.0f * scale;
        g.setColour(t.tickMajor);

        juce::Path topMarker;
        topMarker.addTriangle(cx - mw, bandY, cx + mw, bandY, cx, bandY + mw * 1.5f);
        g.fillPath(topMarker);

        juce::Path botMarker;
        botMarker.addTriangle(cx - mw, bandY + bandH, cx + mw, bandY + bandH,
                              cx, bandY + bandH - mw * 1.5f);
        g.fillPath(botMarker);

        g.drawRoundedRectangle(bandX, bandY, bandW, bandH, 4.0f * scale, 1.0f * scale);
    }

    // --- Toggle: theme (top-left) ---
    float iconSize = 33.0f * scale;
    float iconPad = 8.0f * scale;

    float themeX = iconPad;
    float themeY = iconPad;
    g.setColour(t.toggleBg);
    g.fillRoundedRectangle(themeX, themeY, iconSize, iconSize, 3.0f * scale);
    g.setColour(t.toggleBorder);
    g.drawRoundedRectangle(themeX, themeY, iconSize, iconSize, 3.0f * scale, 1.0f * scale);

    g.setColour(t.toggleIcon);
    g.setFont(iconFont.withHeight(iconSize * 0.6f));
    g.drawText(juce::String::charToString(juce::juce_wchar(0xf042)),
               juce::Rectangle<float>(themeX, themeY, iconSize, iconSize),
               juce::Justification::centred, false);

    // --- Toggle: mode (top-right) ---
    float modeX = w - iconSize - iconPad;
    float modeY = iconPad;
    g.setColour(t.toggleBg);
    g.fillRoundedRectangle(modeX, modeY, iconSize, iconSize, 3.0f * scale);
    g.setColour(t.toggleBorder);
    g.drawRoundedRectangle(modeX, modeY, iconSize, iconSize, 3.0f * scale, 1.0f * scale);

    g.setColour(t.toggleIcon);
    g.setFont(iconFont.withHeight(iconSize * 0.6f));
    g.drawText(juce::String::charToString(juce::juce_wchar(strobe ? 0xf629 : 0xf13a)),
               juce::Rectangle<float>(modeX, modeY, iconSize, iconSize),
               juce::Justification::centred, false);

    // --- Text displays (only when signal present) ---
    if (displayFreq > 0.0f)
    {
        float noteY = cy + 20.0f * scale;
        float noteH = 48.0f * scale;

        g.setColour(tuneColour);
        g.setFont(monoFont.withHeight(noteH));
        juce::String noteStr = juce::String(noteNames[displayNoteIndex]) + juce::String(displayOctave);
        g.drawText(noteStr, juce::Rectangle<float>(0, noteY, w, noteH * 1.1f),
                   juce::Justification::centred, false);

        float freqY = noteY + noteH * 1.15f;
        float freqH = 16.0f * scale;
        g.setColour(t.freqText);
        g.setFont(monoFont.withHeight(freqH));
        g.drawText(juce::String(displayFreq, 1) + " Hz",
                   juce::Rectangle<float>(0, freqY, w, freqH * 1.3f),
                   juce::Justification::centred, false);

        float centsY = freqY + freqH * 1.4f;
        float centsH = 14.0f * scale;
        int centsInt = static_cast<int>(std::round(displayCents));
        g.setColour(tuneColour);
        g.setFont(monoFont.withHeight(centsH));
        g.drawText((centsInt >= 0 ? "+" : "") + juce::String(centsInt) + " cents",
                   juce::Rectangle<float>(0, centsY, w, centsH * 1.3f),
                   juce::Justification::centred, false);
    }
}

void SkoomaTunerEditor::resized()
{
}

void SkoomaTunerEditor::mouseDown(const juce::MouseEvent& e)
{
    float w = static_cast<float>(getWidth());
    float scale = w / 300.0f;
    float iconSize = 33.0f * scale;
    float iconPad = 8.0f * scale;

    // Mode toggle (top-right)
    juce::Rectangle<float> modeRect(w - iconSize - iconPad, iconPad, iconSize, iconSize);
    if (modeRect.contains(e.position))
    {
        processor.strobeMode.store(!processor.strobeMode.load());
        repaint();
        return;
    }

    // Theme toggle (top-left)
    juce::Rectangle<float> themeRect(iconPad, iconPad, iconSize, iconSize);
    if (themeRect.contains(e.position))
    {
        processor.darkMode.store(!processor.darkMode.load());
        repaint();
    }
}
