# Corrode

A JUCE audio effect inspired by the way Ableton Live's Erosion works: a very short delay whose time is modulated by filtered noise or a sine wave, adding rough, digital-sounding artefacts. It is a personal project, built for use in Bitwig Studio but written as a standard VST3 (and standalone app), so it should load in any VST3 host.

Corrode is not affiliated with or endorsed by Ableton. "Erosion" is Ableton's name, so this project uses its own name. The sound is being matched by measuring Erosion's output, not by looking at its code.

## Status

Early work in progress.

- The effect runs and sounds usable, but it is not yet a close match for Erosion.
- Several mappings are provisional guesses and are marked as such in the code: the depth ceiling (`maxModulationMs`), how Width maps to filter bandwidth, how Amount maps to depth, and the noise level estimate.
- Parameter defaults are placeholders.
- Tested so far on Windows 11 with Bitwig Studio, VST3 only.

## How it works

The input goes into a short stereo delay line (about 10 ms of buffer). The read position is moved by a control signal, which changes the delay time from sample to sample.

- **Noise:** white noise through a band-pass filter, with the same modulation on both channels.
- **Wide Noise:** the same, with independent noise per channel for a wider stereo image.
- **Sine:** a sine wave at the Frequency setting. Width has no effect in this mode.

The delay is read at the nearest whole sample, with no interpolation. This is deliberate: the stepping adds the harsh, digital character. At Amount 0 the delay is zero and the signal passes through unchanged.

## Parameters

| Parameter | Range                   | Notes                                                         |
| --------- | ----------------------- | ------------------------------------------------------------- |
| Mode      | Noise, Wide Noise, Sine |                                                               |
| Frequency | 300 Hz to 18 kHz        | Log scale. Centre of the noise band, or the sine rate.        |
| Width     | 0.10 to 2.50            | Provisionally read as bandwidth in octaves. Noise modes only. |
| Amount    | 0 to 200                | Modulation depth. Provisionally linear.                       |

The editor has a draggable X/Y pad (Frequency across, Amount up), three mode buttons and three knobs. The pad's grid is log-spaced like an EQ display, and in the noise modes it draws the noise band as a bell curve.

## Building

### Requirements

- Windows with Visual Studio 2022 or newer (the "Desktop development with C++" workload)
- CMake 3.22 or newer
- Git

### Get JUCE

Clone JUCE once, outside this project, and point CMake at it. This avoids downloading JUCE into every build folder.

```powershell
git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE E:\plugin-dev\JUCE
```

### Configure and build

From the project folder, using the generator that matches your Visual Studio (for example `"Visual Studio 17 2022"` or `"Visual Studio 18 2026"`):

```powershell
cmake -B build -G "Visual Studio 18 2026" -DFETCHCONTENT_SOURCE_DIR_JUCE=E:/plugin-dev/JUCE
cmake --build build --config Release
```

If you leave out `FETCHCONTENT_SOURCE_DIR_JUCE`, CMake downloads JUCE itself, which is slower and needs network access during configure.

Re-run the configure command whenever you add a source file. For code-only changes, the build command is enough.

### Output

```
build\Corrode_artefacts\Release\VST3\Corrode.vst3
build\Corrode_artefacts\Release\Standalone\Corrode.exe
```

The standalone app is a quick way to look at the interface without a host. To use the VST3, either copy `Corrode.vst3` into your VST3 folder (usually `C:\Program Files\Common Files\VST3`) or add the build folder as a plug-in location in your host, then rescan.

Close your host before rebuilding, otherwise it may hold the old `.vst3` open and the linker will fail.

## Project layout

```
CMakeLists.txt
Source/
  PluginProcessor.h     processor class and tuning constants
  PluginProcessor.cpp   parameters, DSP, state, plugin factory
  PluginEditor.h        editor and X/Y pad
  PluginEditor.cpp      interface drawing and parameter bindings
```

## Tuning

The values most likely to need adjusting while matching Erosion:

| Value                       | Where                        | What it controls                         |
| --------------------------- | ---------------------------- | ---------------------------------------- |
| `maxModulationMs`           | `PluginProcessor.h`          | Delay swing at Amount 200                |
| Amount scaling (`/ 200.0f`) | `PluginProcessor.cpp`        | Whether Amount is linear                 |
| Bandwidth to Q conversion   | `PluginProcessor.cpp`        | How Width shapes the noise band          |
| `noiseGain` (the `0.5f`)    | `PluginProcessor.cpp`        | How hard the noise drives the delay      |
| `std::lround` read          | `PluginProcessor.cpp`        | No interpolation, the source of the grit |
| Parameter defaults          | top of `PluginProcessor.cpp` | Starting values                          |

## Plan

1. Record Ableton's parameter defaults and the pad's frequency and amount tapers.
2. Render test signals (sines at several frequencies, plus saw and square) through both Erosion and Corrode, then compare by measurement.
3. Adjust depth, Amount curve, Width mapping and interpolation to match.
4. Refine the interface.
5. Validate with pluginval, install to the standard VST3 folder, and settle on a final name.

## Licence

Not yet decided. Note that JUCE is dual-licensed (AGPLv3 or a commercial licence), so check its terms before distributing any build.
