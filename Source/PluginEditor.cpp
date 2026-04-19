/*
    skoomaTuner - VST3 Tuner Plugin
    License: GPL-3.0
*/

#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

namespace {

struct Theme {
    juce::Colour background;
    juce::Colour trackDim, trackBright, accent;
    juce::Colour meterLow, meterMid, meterHigh, meterOff;
    juce::Colour labelText, valueText;
    juce::Colour panelBg;
    juce::Colour toggleIcon;
};

const Theme darkTheme = {
    juce::Colour(0xff1a1a2e),   // background
    juce::Colour(0xff555555),   // trackDim
    juce::Colour(0xffcccccc),   // trackBright
    juce::Colour(0xff00ff88),   // accent
    juce::Colour(0xff00ff88),   // meterLow
    juce::Colour(0xffffaa00),   // meterMid
    juce::Colour(0xffff4444),   // meterHigh
    juce::Colour(0xff444444),   // meterOff
    juce::Colour(0xff888888),   // labelText
    juce::Colour(0xffaaaaaa),   // valueText
    juce::Colour(0xff0d0d1a),   // panelBg
    juce::Colour(0xff999999),   // toggleIcon
};

const Theme lightTheme = {
    juce::Colour(0xfff2f2f7),   // background
    juce::Colour(0xffcccccc),   // trackDim
    juce::Colour(0xff555555),   // trackBright
    juce::Colour(0xff00aa55),   // accent
    juce::Colour(0xff00aa55),   // meterLow
    juce::Colour(0xffdd8800),   // meterMid
    juce::Colour(0xffdd2222),   // meterHigh
    juce::Colour(0xffcccccc),   // meterOff
    juce::Colour(0xff888888),   // labelText
    juce::Colour(0xff666666),   // valueText
    juce::Colour(0xffe6e6ee),   // panelBg
    juce::Colour(0xff777777),   // toggleIcon
};

void drawIcon(juce::Graphics& g, const juce::Drawable* src,
              juce::Rectangle<float> rect, juce::Colour colour)
{
    if (src == nullptr) return;
    auto d = src->createCopy();
    d->replaceColour(juce::Colours::black, colour);
    d->drawWithin(g, rect, juce::RectanglePlacement::centred, 1.0f);
}

} // anonymous namespace

SkoomaTunerEditor::SkoomaTunerEditor(SkoomaTunerProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    auto typeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::JetBrainsMonoBold_ttf,
        BinaryData::JetBrainsMonoBold_ttfSize);
    monoFont = juce::Font(juce::FontOptions(typeface));

    iconMeter = juce::Drawable::createFromImageData(BinaryData::meter_svg, BinaryData::meter_svgSize);
    iconTheme = juce::Drawable::createFromImageData(BinaryData::theme_svg, BinaryData::theme_svgSize);

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
        tuneColour = t.meterOff;
    else if (absC < 3.0f)
        tuneColour = t.meterLow;
    else if (absC < 15.0f)
        tuneColour = t.meterMid;
    else
        tuneColour = t.meterHigh;

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
                g.setColour(t.accent);
                g.drawLine(x1, y1, x2, y2, 2.5f * scale);
            }
            else if (isMajor)
            {
                g.setColour(t.trackBright);
                g.drawLine(x1, y1, x2, y2, 1.5f * scale);
            }
            else
            {
                g.setColour(t.trackDim);
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

        g.setColour(t.panelBg);
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
        g.setColour(t.trackBright);

        juce::Path topMarker;
        topMarker.addTriangle(cx - mw, bandY, cx + mw, bandY, cx, bandY + mw * 1.5f);
        g.fillPath(topMarker);

        juce::Path botMarker;
        botMarker.addTriangle(cx - mw, bandY + bandH, cx + mw, bandY + bandH,
                              cx, bandY + bandH - mw * 1.5f);
        g.fillPath(botMarker);

        g.drawRoundedRectangle(bandX, bandY, bandW, bandH, 4.0f * scale, 1.0f * scale);
    }

    // --- Toggles (hover shows background only) ---
    float iconSize = 33.0f * scale;
    float iconPad = 8.0f * scale;

    juce::Point<float> mousePos;
    bool mouseHere = isMouseOver(false);
    if (mouseHere) mousePos = getMouseXYRelative().toFloat();

    juce::Rectangle<float> modeRect(iconPad, iconPad, iconSize, iconSize);
    if (mouseHere && modeRect.contains(mousePos)) {
        g.setColour(t.toggleIcon.withAlpha(0.15f));
        g.fillRoundedRectangle(modeRect, 3.0f * scale);
    }
    drawIcon(g, iconMeter.get(), modeRect.reduced(iconSize * 0.2f), t.toggleIcon);

    juce::Rectangle<float> themeRect(w - iconSize - iconPad, iconPad, iconSize, iconSize);
    if (mouseHere && themeRect.contains(mousePos)) {
        g.setColour(t.toggleIcon.withAlpha(0.15f));
        g.fillRoundedRectangle(themeRect, 3.0f * scale);
    }
    drawIcon(g, iconTheme.get(), themeRect.reduced(iconSize * 0.2f), t.toggleIcon);

    // --- Text displays (only when signal present) ---
    // Unified text band across Tuner/Loud/Image: main at 0.76w, secondary at 0.89w,
    // placed below the arc endpoints (y ≈ 0.75w) so the needle sweep can't overlap.
    if (displayFreq > 0.0f)
    {
        const float mainH  = 34.0f * scale;
        const float mainY  = 0.76f * w;
        const float labelH = 14.0f * scale;
        const float labelY = 0.89f * w;

        g.setColour(tuneColour);
        g.setFont(monoFont.withHeight(mainH));
        juce::String noteStr = juce::String(noteNames[displayNoteIndex]) + juce::String(displayOctave);
        g.drawText(noteStr, juce::Rectangle<float>(0, mainY, w, mainH),
                   juce::Justification::centred, false);

        int centsInt = static_cast<int>(std::round(displayCents));
        g.setColour(tuneColour);
        g.setFont(monoFont.withHeight(labelH));
        g.drawText((centsInt >= 0 ? "+" : "") + juce::String(centsInt) + " cents",
                   juce::Rectangle<float>(0, labelY, w, labelH),
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

    // Mode toggle (top-left)
    juce::Rectangle<float> modeRect(iconPad, iconPad, iconSize, iconSize);
    if (modeRect.contains(e.position))
    {
        processor.strobeMode.store(!processor.strobeMode.load());
        repaint();
        return;
    }

    // Theme toggle (top-right)
    juce::Rectangle<float> themeRect(w - iconSize - iconPad, iconPad, iconSize, iconSize);
    if (themeRect.contains(e.position))
    {
        processor.darkMode.store(!processor.darkMode.load());
        repaint();
    }
}
