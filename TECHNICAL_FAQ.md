# Technical FAQ

## General Architecture

### What technologies are used in Plasmoid Visualizer?
- **Language**: C++17
- **Graphics**: OpenGL 3.3 Core Profile (GLFW + GLEW)
- **UI**: Dear ImGui
- **Audio Engine**: `miniaudio` (file playback, low-latency capture/WASAPI loopback, test tone, and oscilloscope music synthesis)
- **DSP & Math**: `KissFFT` (8192-point real-to-complex FFT), `tinyexpr` (XY/Z expression evaluation), and Catmull-Rom spline interpolation
- **Config**: `toml++` (atomic TOML save/load)

### How does the audio-to-render data path work?
1. The real-time `miniaudio` audio callback writes stereo + optional Z samples into preallocated, lock-free atomic SPSC ring buffers (`65,536` frames) with zero mutex locks or heap allocations.
2. Once per render frame, `RenderManager` pulls the latest audio window, caches at most one 8192-point Hanning-windowed FFT per active audio channel (`Mixed`, `Left`, `Right`), and computes delta-time-independent attack/falloff log-binned bar magnitudes.
3. Focused renderers (`SpectrumRenderer`, `XYTraceRenderer`, `QuadRenderer`, `TextRenderer`) build vertices into reusable scratch buffers and stream them through append-only ring VBOs (`glBufferSubData` with occasional orphaning).
4. GPU post-processing (phosphor persistence, half-res separable HDR bloom, and Gaussian lyrics-band blur) runs only when enabled by active layers.

### How does the multi-layered rendering work?
Layers are stored in `PersistentConfig::layers` and rendered in stack order. Persistent XY oscilloscope layers draw into the HDR persistence FBO (with optional fast/slow phosphor decay), followed by direct layers (`Bars`, `Lines`, `Dots`, `Waveform`, `Curve`, and triggered `Oscilloscope XY`), optional HDR bloom, and the always-on-top native OpenGL `MediaOverlayLayer`.

## Audio Analysis

### Why use 192 kHz audio for Oscilloscope XY?
At 44.1 kHz, fast geometric XY shapes have relatively few samples per screen refresh. At 96 kHz or 192 kHz, the beam traces far more coordinate points per frame, producing smooth curves without polygonal corners.

### Why process audio as 32-bit float?
Decoding and conditioning in 32-bit floating-point prevents quantization loss across AC-coupling, biquad bandwidth filtering, gain scaling, and 2D rotation stages.

### Does it support beat detection?
Yes. `AnalysisEngine::detectBeat` tracks low-frequency (20–150 Hz) energy against a frame-rate-independent exponential moving average and triggers Zen-Kun background pulse and shake effects with a cooldown timer.

## Visuals & Rendering

### How is the analog oscilloscope look achieved?
- **Beam Conditioning**: Per-axis AC/DC coupling, low-pass bandwidth filter, auto-gain, and jump blanking.
- **Dwell & Z-Modulation**: Velocity-derived brightness plus optional explicit Z-channel modulation (for Oscilloscope Music).
- **Soft Ribbon Beam**: Miter-clamped screen-space ribbons rendered in two additive passes (soft outer halo + bright core) with a Gaussian falloff fragment shader.
- **Dual-Decay Phosphor & Bloom**: Fast persistence FBO + optional slow afterglow FBO + 8-pass half-resolution separable HDR bloom.

### How is antialiasing handled?
1. **Hardware MSAA**: 4x Multisample Anti-Aliasing requested on the GLFW window.
2. **Smooth Lines**: `GL_LINE_SMOOTH` with `GL_NICEST` hint and driver-clamped `glLineWidth`.
3. **Shader AA**: Procedural bars, dots, and XY beams use `fwidth()` and `smoothstep()` in GLSL 330 core shaders for resolution-independent edges.

## Performance & System Monitoring

### How does frame pacing work when VSync is off?
When VSync is disabled and `Target FPS > 0`, the main loop sleeps (`std::this_thread::sleep_for`, using 1 ms timer resolution on Windows via `timeBeginPeriod(1)`) until ~1.5 ms before the deadline, then yield-spins only the final sub-millisecond window for tight frame pacing at low idle CPU usage.

### How are CPU, RAM, and GPU metrics collected?
- **Windows**: Win32 `GetProcessTimes`, `GetSystemTimes`, `GetProcessMemoryInfo`, and `GL_RENDERER` (+ `GL_NVX_gpu_memory_info` for dedicated VRAM on NVIDIA GPUs).
- **Linux**: `/proc/self/stat`, `/proc/stat`, and `/proc/self/status` for CPU and RSS RAM. Dedicated GPU VRAM currently reports `N/A (Linux)`.

## Video Export & Troubleshooting

### How does "Render to Video" work?
`VideoRenderManager` opens its own dedicated stereo `ma_decoder` (never touching live playback state), pauses the live preview pass while presenting the rendered offline frame in the window, reads pixels asynchronously through double-buffered OpenGL Pixel Buffer Objects (`GL_PIXEL_PACK_BUFFER`), and streams raw RGB24 frames from a background writer thread directly to `ffmpeg` via `CreateProcessW` (Windows) or `posix_spawn` (Linux) with explicit argument vectors (no shell invocation). Audio is stream-copied when container-compatible (e.g., MP3/AAC into MP4, or into MKV) and encoded to 320 kbps AAC when exporting PCM/WAV/FLAC/OGG into MP4.

You must have `ffmpeg` installed and available in your system `PATH`.

### Why can't I see visuals in "Live Capture" mode?
Select the appropriate input/loopback device in **Audio Settings** and click **Refresh** if you plugged in a new audio device. On Windows, loopback capture uses WASAPI; on Linux (PipeWire / PulseAudio), select a `Monitor of ...` source to visualize system output.

## Platform Support

- **Windows 10/11 (x64)**: Primary platform (MSVC / VS 2022+, WASAPI, OpenGL 3.3 Core).
- **Linux (x86_64)**: Standalone app built and unit-tested in GitHub Actions CI (GCC, OpenGL 3.3 Core, PipeWire/PulseAudio via `miniaudio`).
