# SkoomaTuner

A minimal VST3 tuner plugin.

<table><tr>
<td><img src="screenshots/dark-theme-needle.png" width="200"></td>
<td><img src="screenshots/dark-theme-strobe.png" width="200"></td>
<td><img src="screenshots/light-theme-needle.png" width="200"></td>
<td><img src="screenshots/light-theme-strobe.png" width="200"></td>
</tr></table>

Needle and strobe modes, dark and light themes. Spring-damped needle physics.

## Install

Download the VST3 for your platform from the [Releases](https://github.com/skoomabwoy/SkoomaTuner/releases) page.

- **Linux / Windows**: Extract the `.zip` and copy the `.vst3` bundle to your VST3 folder.
- **macOS**: Open the `.dmg`, drag `SkoomaTuner.vst3` into the `VST3 Plug-Ins` folder, then run `xattr -cr ~/Library/Audio/Plug-Ins/VST3/SkoomaTuner.vst3` in Terminal.

<details>
<summary>Alternatively, you can build from source</summary>

CMake 3.22+, C++17 compiler. All dependencies (JUCE, FFTW3) are fetched automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Copy `build/SkoomaTuner_artefacts/Release/VST3/SkoomaTuner.vst3/` to your VST3 folder.

</details>

## Credits

Pitch detection from [StompTuner](https://github.com/brummer10/StompTuner) by Hermann Meyer (brummer10), derived from [Guitarix](https://guitarix.org/). NSDF algorithm based on work by Philip McLeod (Tartini). Icons: [Font Awesome Free](https://fontawesome.com/) (SIL OFL 1.1).

## License

GPL-3.0
