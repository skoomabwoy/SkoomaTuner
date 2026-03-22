# SkoomaTuner

A minimal VST3 tuner plugin for Linux, built with JUCE.

## Credits

The pitch detection engine is extracted from [StompTuner](https://github.com/brummer10/StompTuner) by **Hermann Meyer (brummer10)**, which itself derives from the [Guitarix](https://guitarix.org/) project (Hermann Meyer, James Warden, Andreas Degert). The algorithm is NSDF (Normalized Square Difference Function), based on work by Philip McLeod (Tartini).

The resampler is [zita-resampler](https://kokkinizita.linuxaudio.org/linuxaudio/) by Fons Adriaensen.

## Building

Requires: CMake 3.22+, FFTW3 (`fftw` package), C++17 compiler.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The VST3 bundle is at `build/SkoomaTuner_artefacts/Release/VST3/SkoomaTuner.vst3/`.

Copy to `~/.vst3/` to install.

## License

GPL-3.0 — required by the combination of JUCE (GPLv3), zita-resampler (GPL-3.0), and Guitarix pitch detection code (GPL-2.0-or-later).
