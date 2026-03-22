# StompTuner → JUCE Port: Pitch Detection Extraction Guide

This document captures everything needed to create a clean JUCE tuner plugin
reusing the pitch detection engine from StompTuner (by brummer / Hermann Meyer).

## Goal

Build a VST3 tuner plugin for Linux with a clean, modern UI (needle tuner style),
reusing StompTuner's accurate pitch detection. No guitar pedal aesthetics — minimal,
readable, studio-friendly look.

Target format: **VST3 only, Linux only**.

---

## Source Project Overview

- **Repo**: `/home/skooma/Projects/StompTuner`
- **Framework**: DPF (DISTRHO Plugin Framework) + Cairo for UI
- **License**: GPL-2.0-or-later
- **Pitch detection origin**: Guitarix project (Hermann Meyer, James Warden, Andreas Degert)
- **Algorithm**: NSDF (Normalized Square Difference Function), derived from Tartini by Philip McLeod
- **Frequency range**: 24–999 Hz
- **Reference pitch**: configurable 432–452 Hz

---

## Files to Extract

All paths relative to `/home/skooma/Projects/StompTuner/plugins/`.

### Core Pitch Detection

| File | Lines | Purpose |
|------|-------|---------|
| `StompTuner/pitch_tracker.h` | 115 | PitchTracker class + PitchTrackerWorker (background thread) |
| `StompTuner/pitch_tracker.cpp` | 396 | FFT-based NSDF pitch detection algorithm |
| `StompTuner/tuner.hpp` | 51 | Thin wrapper class around PitchTracker |
| `StompTuner/tuner.cc` | 49 | Wrapper implementation |

### Audio Preprocessing

| File | Lines | Purpose |
|------|-------|---------|
| `StompTuner/low_high_cut.h` | 47 | Bandpass filter header (Faust-generated) |
| `StompTuner/low_high_cut.cc` | 90 | Bandpass filter implementation, no dependencies |

### Resampler (vendored library, copy entire directory)

| Directory | ~Lines | Purpose |
|-----------|--------|---------|
| `zita-resampler-1.1.0/` | ~600 | Zita resampler by Fons Adriaensen. Files: `resampler.cc`, `resampler-table.cc`, `gx_resampler.cc`, plus headers in `zita-resampler/` subdir |

**Total: ~1,350 lines of portable C++ with no framework dependencies.**

---

## External Dependency

**FFTW3** (single-precision float variant):
- Install: `sudo pacman -S fftw` (Arch/CachyOS)
- Header: `<fftw3.h>`
- Linker flag: `-lfftw3f`
- Used by: `pitch_tracker.cpp` for FFT/IFFT plans

No other external dependencies. The resampler and filter are self-contained.

---

## Audio Processing Chain

```
Audio Input
    │
    ▼
Bandpass Filter (low_high_cut::Dsp)
    │  - Filters to useful frequency range
    │  - init(sample_rate), compute(count, input, output)
    │
    ▼
Tuner (tuner class → PitchTracker)
    │  - Downsamples to internal 20.5kHz (via zita-resampler, factor 2)
    │  - Accumulates samples in circular buffer (FFT_SIZE = 2048)
    │  - Every ~100ms triggers FFT analysis on worker thread
    │  - NSDF autocorrelation → parabolic interpolation → frequency estimate
    │  - Fires callback when frequency changes
    │
    ▼
Detected Frequency (float, Hz)
```

---

## PitchTracker API

### Construction
```cpp
// Takes a std::function<void()> callback, fired when frequency changes
PitchTracker tracker([&]() { onNewFrequency(); });
```

### Initialization
```cpp
tracker.init(sampleRate);  // Call once with DAW sample rate
```

### Feeding Audio (called every processBlock)
```cpp
tracker.add(frameCount, floatBuffer);  // mono float* input
```

### Reading Results
```cpp
float freq = tracker.get_estimated_freq();  // Hz (0.0 if no signal, 24-999 range)
float note = tracker.get_estimated_note();  // 12 * log2(freq * 2.272727e-03), 1000.0 if silent
```

### Optional Configuration
```cpp
tracker.set_threshold(value);              // Signal detection threshold (linear, not dB)
tracker.set_fast_note_detection(true);     // 5x threshold, 10x faster updates
```

### Reset
```cpp
tracker.reset();  // Clear buffers, reset state
```

### Tuner Wrapper (simpler interface)
```cpp
tuner* t = new tuner([&]() { handleFreqChange(); });
t->init(sampleRate);
t->feed_tuner(frameCount, buffer);
float freq = t->get_freq();
float note = t->get_note();

// Static helpers:
tuner::set_threshold_level(*t, -60.0f);  // Takes dB, converts internally
tuner::set_fast_note(t, true);
```

---

## Threading Model

- `PitchTrackerWorker` spawns a background thread on construction
- The worker thread sleeps on a condition variable
- `add()` accumulates samples; when enough time has passed (~100ms), it copies
  the buffer and wakes the worker via `cv.notify_one()`
- Worker runs `run()` (the FFT analysis), then calls the `new_freq` callback
- `busy` atomic flag prevents overlapping analysis runs
- **This is real-time safe on the audio thread** — `add()` never blocks

---

## How to Wire Into JUCE

### PluginProcessor.h
```cpp
#include "low_high_cut.h"
#include "tuner.hpp"
#include <atomic>

class TunerProcessor : public juce::AudioProcessor {
    low_high_cut::Dsp lowHighCut;
    std::unique_ptr<tuner> dsp;
    std::atomic<float> currentFreq{0.0f};
    float refFreq = 440.0f;
    // ...
};
```

### PluginProcessor.cpp
```cpp
void TunerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    lowHighCut.init_static(static_cast<uint32_t>(sampleRate), &lowHighCut);
    dsp = std::make_unique<tuner>([this]() {
        currentFreq.store(dsp->get_freq(), std::memory_order_release);
    });
    dsp->init(static_cast<unsigned int>(sampleRate));
}

void TunerProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer&) {
    auto* channelData = buffer.getWritePointer(0);
    int numSamples = buffer.getNumSamples();

    // Work on a copy so we don't modify the pass-through audio
    std::vector<float> workBuffer(channelData, channelData + numSamples);
    lowHighCut.compute_static(numSamples, workBuffer.data(), workBuffer.data(), &lowHighCut);
    dsp->feed_tuner(numSamples, workBuffer.data());
}

void TunerProcessor::releaseResources() {
    dsp.reset();
}
```

### PluginEditor.cpp (UI concept)
```cpp
void TunerEditor::paint(juce::Graphics& g) {
    float freq = processor.currentFreq.load(std::memory_order_acquire);

    // Convert frequency to note + cents (same math as StompTuner)
    // refFreq = 440.0 (or user-configurable)
    float noteFloat = 12.0f * log2f(freq / refFreq) + 69.0f;  // MIDI note
    int nearestNote = std::round(noteFloat);
    float cents = (noteFloat - nearestNote) * 100.0f;

    // Draw your needle tuner:
    // - Background arc/gauge
    // - Needle rotated by cents value
    // - Note name text
    // - Frequency text
    // - Cents offset text
}
```

---

## Note/Frequency Math

From the original `CairoTunerWidget.hpp`, the note detection works like this:

```cpp
// Convert frequency to note number relative to reference pitch
// refFreq typically 440.0
float noteNum = 12.0f * log2f(freq / refFreq) + 4.0f;
// +4 because internal numbering starts at E (open low E guitar string)

// For standard MIDI note numbering (A4 = 69):
float midiNote = 12.0f * log2f(freq / 440.0f) + 69.0f;
int nearestMidi = std::round(midiNote);
float cents = (midiNote - nearestMidi) * 100.0f;  // -50 to +50

// Note names:
const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char* noteName = noteNames[((nearestMidi % 12) + 12) % 12];
int octave = (nearestMidi / 12) - 1;
```

---

## Parameters for the JUCE Plugin

Minimal set (matching StompTuner):

| Parameter | Type | Range | Default | Notes |
|-----------|------|-------|---------|-------|
| Bypass | bool | 0/1 | 0 | Standard plugin bypass |
| Frequency | float (output) | 0–1000 Hz | — | Read-only, from pitch tracker |
| Reference Freq | float | 432–452 Hz | 440 | User-adjustable tuning reference |

---

## JUCE Project Setup Notes

1. **Plugin format**: VST3 only
2. **Audio config**: 1 input, 1 output (mono analyzer, passes audio through)
3. **Include paths**: Add the directory containing the extracted files + zita-resampler headers
4. **Linker flags**: `-lfftw3f -lpthread`
5. **C++ standard**: C++11 minimum (for std::thread, std::atomic, std::function)
6. **Build**: Can use Projucer or CMake — CMake may be easier for linking FFTW3

---

## Licensing

- Pitch detection code: **GPL-2.0-or-later** (Guitarix / Hermann Meyer et al.)
- Zita-resampler: **GPL-3.0** (Fons Adriaensen)
- JUCE: **GPLv3** (free tier) or commercial
- **Result**: Fork must be **GPL-3.0** (to satisfy all three)

---

## What NOT to Copy

Everything in `plugins/CairoWidgets/` — that's the old UI.
Everything in `plugins/Utils/` — DPF-specific UI utilities.
`leder.c`, `scratch.c` — leather/scratch texture PNG data for the pedal look.
`UIStompTuner.hpp/.cpp` — DPF UI controller.
`DistrhoPluginInfo.h` — DPF plugin metadata.
The `dpf/` submodule — not needed with JUCE.
