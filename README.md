# skoomaTuner

<table><tr>
<td><img src="screenshots/dark-theme-needle.png" width="200"></td>
<td><img src="screenshots/dark-theme-strobe.png" width="200"></td>
<td><img src="screenshots/light-theme-needle.png" width="200"></td>
<td><img src="screenshots/light-theme-strobe.png" width="200"></td>
</tr></table>

Needle and strobe modes, dark and light themes. Spring-damped needle physics.

Part of a series of small, minimal VST3 plugins:
- [skoomaLoud](https://github.com/skoomabwoy/skoomaLoud) (loudness meter)
- [skoomaFilter](https://github.com/skoomabwoy/skoomaFilter) (LP filter)
- [skoomaRoute](https://github.com/skoomabwoy/skoomaRoute) (stereo routing)

## Install

Download the VST3 for your platform from the [Releases](https://github.com/skoomabwoy/skoomaTuner/releases) page.

- **Linux**: Extract the `.zip` and copy `skoomaTuner.vst3` to `~/.vst3/` or `/usr/lib/vst3/`.
- **Windows**: Extract the `.zip` and copy `skoomaTuner.vst3` to `C:\Program Files\Common Files\VST3\`.
- **macOS**: Open the `.dmg`, drag `skoomaTuner.vst3` into the `VST3 Plug-Ins` folder, then run `xattr -cr ~/Library/Audio/Plug-Ins/VST3/skoomaTuner.vst3` in Terminal. This is needed because the plugin is unsigned (we don't want to pay $99/year for a free plugin).

<details>
<summary>Alternatively, you can build from source</summary>

CMake 3.22+, C++17 compiler. All dependencies (JUCE, FFTW3) are fetched automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Copy `build/SkoomaTuner_artefacts/Release/VST3/skoomaTuner.vst3/` to your VST3 folder.

</details>

## Credits

- Pitch detection from [StompTuner](https://github.com/brummer10/StompTuner) by Hermann Meyer (brummer10)
- NSDF algorithm based on work by Philip McLeod (Tartini)
- Icons: [Boxicons](https://github.com/box-icons/boxicons-core) (MIT)
- Font: [JetBrains Mono](https://www.jetbrains.com/lp/mono/) (SIL OFL 1.1)

## License

GPL-3.0
