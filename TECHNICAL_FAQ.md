# Technical FAQ

## General Architecture

### What technologies are used in the Plasmoid Visualizer?
The project is built using:
- **Language**: C++17
- **Graphics**: OpenGL 3.3 (Core Profile)
- **Audio Engine**: `miniaudio` (low-latency capture and playback)
- **Math/Analysis**: `kissfft` (Fast Fourier Transform), `tinyexpr` (expression parsing), and custom Catmull-Rom splines for smooth curves.
- **Config**: `toml++`

### How is CPU usage so low while processing high-res data?
The engine leverages several optimization strategies:
- **Zero-Copy Path**: We read raw PCM data via `miniaudio` and stream it directly into Vertex Buffer Objects (VBOs) for rendering.
- **GPU Workload**: By offloading heavy visual tasks like "Phosphor Bloom" and "Trace Decay" to custom GLSL shaders, the CPU is freed from pixel manipulation, focusing only on audio processing and vertex orchestration.
- **C++ Performance**: Manual memory management and tight loops ensure minimal overhead compared to interpreted or garbage-collected languages.

### How does the multi-layered rendering work?
The visualizer uses a layer-based system. Each layer has its own configuration (gain, falloff, frequency range, shape). Layers are rendered sequentially, allowing for complex stacked visuals (e.g., a waveform over a frequency spectrum).

## Audio Analysis

### Why use 192kHz audio for a visualizer?
In standard audio (44.1kHz), the "sampling resolution" is often too low to draw complex geometric shapes accurately in modes like Oscilloscope XY. At 192kHz, we have over 192,000 coordinate points per second. This allows the beam to render incredibly fine details—such as intricate mathematical "N-Sphere" structures—without lines appearing jagged or aliased.

### Why does the visualizer use 32-bit Float if the source is 16-bit?
Even with 16-bit source files, we "promote" the data to 32-bit floating-point containers. This prevents cumulative "rounding errors" during high-precision math stages like coordinate scaling, rotation, and bloom. This ensures the visual output remains razor-sharp regardless of gain settings.

### Does it support beat detection?
Yes. The `AnalysisEngine` monitors energy spikes in specific sub-bands. When the energy exceeds a dynamic threshold (adjustable via "Sensitivity"), a beat is detected, which can trigger visual events like pulsing or particle bursts.

## Visuals & Rendering

### How is the "Analog Oscilloscope" look achieved?
We simulate CRT behavior using several techniques:
- **Velocity Modulation**: The brightness of the trace is inversely proportional to its speed, mimicking phosphor excitation.
- **Circular Beam Head**: Fragment shaders use Signed Distance Fields (SDF) and `smoothstep` to render a soft, circular point at the leading edge.
- **Persistence (Glow)**: We use multiple rendering passes and alpha blending (often combined with a "Persistence" factor) to keep old trace data visible for a short duration, creating a realistic glow.

### How do you handle antialiasing without a high performance hit?
We combine three strategies:
1. **Hardware MSAA**: 4x Multisample Anti-Aliasing is requested via GLFW.
2. **OpenGL Smoothing**: `GL_LINE_SMOOTH` and `GL_POLYGON_SMOOTH` are enabled with `GL_NICEST` hints.
3. **Sub-pixel Shader AA**: Procedural shapes (Bars, Dots) use `fwidth()` and `smoothstep()` in the fragment shader to produce perfectly smooth edges regardless of resolution.

### What is the "Moving" effect seen in some frames?
This is typically **Temporal Aliasing**. Since the screen refresh rate (e.g., 144Hz or 180Hz) and the audio sampling signal (192kHz) aren't perfectly synchronized, you occasionally see the "ghost" or "phase shift" of the previous audio frame. This actually enhances the "analog phosphor" look, mimicking the behavior of real physical hardware oscilloscopes.

## Performance & System

### Is the CPU/RAM monitoring accurate?
On Linux, we parse `/proc/self/stat` and `/proc/self/status` for extremely low-overhead, accurate reporting.
- **Global CPU**: Shows usage relative to the total system capacity (KDE style).
- **Process CPU**: Shows usage where 100% equals 1 fully saturated core (htop style).

### Why does VRAM reporting differ from some tools?
We query physical dedicated VRAM directly via Linux `sysfs` (`/sys/class/drm/`). This avoids the common mistake of including "Shared System Memory" (GTT), which can make an 8GB card look like it has 16GB in some OpenGL-only tools.

## Troubleshooting

### Why can't I see any visuals in "Capture" mode?
Ensure you have selected the correct input device. On Linux, this is often a "Monitor" device (e.g., `Monitor of Internal Audio`) if you want to visualize system output.

### How do I render to video?
The "Render to Video" feature pipes raw frames directly into `ffmpeg`. You must have `ffmpeg` installed in your system PATH for this to function.

> [!WARNING]
> This feature is currently an **unpolished experimental tool**. Because it captures and processes audio frames differently than the real-time engine, you may notice that the resulting video's visualizations are "off" or less reactive compared to the live playback. This is a known limitation of the current frame-stitching implementation.
