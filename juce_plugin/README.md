# Artifact Remover VST3 Plugin

A JUCE-based VST3 audio plugin that wraps the Artifact Remover library for real-time audio processing using a moving window framework.

## Building the Plugin

### Prerequisites
- JUCE framework (automatically downloaded via CMake)
- Artifact Remover library (from parent build)
- CMake 3.21+
- Visual Studio 2019+ (Windows) or Xcode (macOS)

### Build Instructions

#### Option 1: Build as part of parent project

Update the parent `CMakeLists.txt` to include the plugin subdirectory:

```cmake
# Add this to the main CMakeLists.txt
add_subdirectory(juce_plugin)
```

#### Option 2: Standalone build

From the `juce_plugin` directory:

```bash
mkdir build
cd build
cmake .. -A x64
cmake --build . --config Release
```

### Features

- **Single-channel audio processing** - Processes mono audio (one channel)
- **Moving window framework** - Processes audio in overlapping windows (50% overlap by default)
- **Real-time parameter control** via GUI:
  - **Window Size**: Range 128-2048 samples (default: 512)
  - **Lower Frequency**: Range 5-100 Hz (default: 50 Hz)
  - **Upper Frequency**: Range 100-500 Hz (default: 250 Hz)
  - **Factor**: Range 1-10 (default: 2.0)
  - **SVD Threshold**: Range 0-1 (default: 0, disabled)

- **MKL Acceleration**: Uses Intel MKL for optimized SVD computation (if available)
- **Acceptable Latency**: ~1-2ms processing latency

## Architecture

### PluginProcessor.h/cpp
- `ArtifactRemoverAudioProcessor` - Wraps the Remover class
- Manages parameter state via `AudioProcessorValueTreeState`
- Implements real-time audio processing loop
- Handles sample buffering and output

### PluginEditor.h/cpp
- `ArtifactRemoverEditor` - Simple GUI with sliders
- Parameter binding via `SliderParameterAttachment`
- Real-time value display

## Configuration

### Hankel Matrix Parameters
Currently hardcoded in `PluginProcessor.h`:
- `HANKEL_LENGTH = 512` - Hankel matrix dimension (samples)
- `TIME_DELAY = 10` - Time delay parameter

To adjust these values, modify the constants in `PluginProcessor.h` and rebuild.

### MKL Integration
The plugin uses the Artifact Remover library's MKL configuration. Ensure:
1. MKL is properly installed and configured in the parent project
2. Environment variable set: `MKL_THREADING_LAYER=sequential` (set automatically in demo)

## Audio Processing Flow

1. Incoming mono audio samples are accumulated into a buffer
2. When buffer is full (HANKEL_LENGTH samples), artifact removal is applied
3. Processed samples are returned with latency compensation
4. Process repeats for each block

## Notes

- **Single channel only**: The plugin processes mono audio. Stereo hosts will use the left channel or mono mixdown.
- **Latency**: Plugin reports ~HANKEL_LENGTH samples of latency for timing compensation
- **Parameter updates**: All parameter changes are applied in real-time
- **No MIDI**: Plugin does not accept or produce MIDI data

## Building for Different Platforms

### Windows
```bash
cmake .. -A x64 -G "Visual Studio 16 2019"
cmake --build . --config Release
```

### macOS
```bash
cmake .. -G Xcode
cmake --build . --config Release
```

The compiled plugin will be in:
- **VST3**: `build/ArtifactRemoverVST3_artefacts/Release/VST3/Artifact Remover.vst3`
- **Standalone**: `build/ArtifactRemoverVST3_artefacts/Release/Standalone/Artifact Remover`

## Troubleshooting

### Build fails to find JUCE
- Ensure internet connection for JUCE download
- JUCE is cached in `juce_plugin/JUCE` after first download

### Plugin crashes on audio processing
- Check that parent `artifact_remover` library is properly linked
- Verify `remove_artifact()` function signature matches parameter count

### Parameter changes don't affect output
- Verify `SliderParameterAttachment` is properly created
- Check `apvts` parameter IDs match between processor and attachments

## Future Improvements

- [ ] Stereo processing (requires dual-channel implementation)
- [ ] Preset system for common use cases
- [ ] Advanced GUI with waveform display
- [ ] A/B comparison functionality
- [ ] Parameter automation curves
