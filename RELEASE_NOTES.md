# Plasmoid Visualizer v1.3

## Oscilloscope Visualizer Modes
**New visualization modes for audio analysis:**
- **Waveform Mode** - Display raw audio as a centered time-domain waveform
- **Oscilloscope XY Mode** - Map stereo channels to X/Y coordinates for Lissajous figures

**Stabilization Features:**
- Zero-crossing trigger detection for both Waveform and XY modes
- Smooth trigger interpolation to eliminate jitter
- Adjustable time scale slider (0.2x - 2.0x zoom) for detailed waveform inspection
## Video Export
**Offline rendering:**
- Export visualizations directly to MP4 video files
- FFMPEG integration with stdin piping for efficient processing
- Configurable resolution (custom width/height)
- Adjustable framerate (30/60 FPS)
- Perfect audio sync using precise sample-rate calculations
- Automatic vertical flip correction for OpenGL coordinate system
- Progress indication during rendering

## CRT Oscilloscope Aesthetics
**Authentic oscilloscope visual effects:**
- Multi-pass glow rendering (3-layer halo effect)
- Phosphor persistence/decay simulation
- Square aspect ratio enforcement (1:1) for XY mode
- Adjustable glow intensity and decay rates

## Multi-Channel Test Tone Generation
**Enhanced test tone capabilities:**
- **Stereo Mode** - Independent L/R channel frequency control
- Separate frequency sliders for Left and Right channels (20Hz - 2kHz)
- Perfect for creating classic Lissajous patterns (1:2, 2:3, 3:4 ratios)
- All waveform types available in stereo (Sine, Square, Sawtooth, Triangle, Noise)

##  XY Transformation Controls
**Advanced Lissajous pattern manipulation:**

- **Rotation** - 0-360° continuous rotation slider
- **Flip Horizontal** - Mirror pattern on X-axis
- **Flip Vertical** - Mirror pattern on Y-axis
- Combinable transformations for viewing patterns from any angle

## Technical Improvements
- Audio Engine Enhancements
- Robust device initialization with `resetDevice()` helper
- Fixed double-initialization crash when switching audio modes
- Added `readAudioFrames()` for random-access audio reading (video export)
- Implemented stereo buffer capture (`getStereoBuffer()`)
- Independent phase tracking for Left/Right test tone channels
- Sample-rate aware rendering for non-44.1kHz files

## UI/UX Improvements
- Cached device enumeration to prevent per-frame overhead
- Manual "Refresh Devices" button
- Dynamic UI controls (Time Scale, Rotation, Flip) visible only for relevant modes
- Stereo mode checkbox with conditional L/R frequency sliders
- Render Video dialog with progress tracking

## Code Quality
- Refactored main loop logic into reusable `updateVisualizer()` and `renderFrame()` helpers
- Consistent transformation pipeline: Gain → Aspect → Rotation → Flip → Clamp
- Exception handling with graceful degradation
- Clean separation between live rendering and offline video export paths

## Visual Demonstrations
The Oscilloscope XY mode with CRT glow effects produces stunning Lissajous figures:

- Circle (L=440Hz, R=440Hz, 0° phase)
- Figure-8 (L=440Hz, R=880Hz, 1:2 ratio)
- Trefoil (L=440Hz, R=660Hz, 2:3 ratio)
- Use Rotation + Flip controls to view these patterns from any perspective

# Plasmoid Visualizer v1.2 - Pro Editor & Multi-Layering

This update transforms the standalone application into a professional-grade visualizer editor with advanced layering and a modular interface.

## Pro Editor Enhancements
- **Modular Window UI**: The single control panel has been split into dedicated, closable windows:
    - **Audio Settings**: Device selection and file management.
    - **Layer Manager**: Add, remove, and toggle visibility for multiple layers.
    - **Layer Editor**: Deep customization for the selected layer.
    - **Debug Info**: Real-time performance and audio metrics.
- **Main Menu Bar**: Easily toggle editor windows from the top menu.

## Multi-Layer Visualizer System
- **Stackable Layers**: Create complex visualizer templates by stacking multiple layers.
- **Per-Layer Customization**:
    - **Independent Gain & Falloff**: Tune each layer's sensitivity and decay.
    - **Custom Colors**: Full RGBA color control for every layer.
    - **Frequency Range Selection**: Focus layers on specific bands (e.g., a "Bass" layer and a "Highs" layer).
    - **Individual Scaling**: Adjust bar heights per layer.

## Stability & Compatibility
- **Widget-Safe Refactor**: The core engine was refactored to support these advanced features while maintaining 100% backward compatibility with the Plasma widget.
- **Performance Optimized**: Efficient rendering of multiple layers using OpenGL blending.

---
# Plasmoid Visualizer v1.1 - Refined

This update focuses on polishing the user experience.

## Streamlined Widget Experience
- **Focused on Live Mode**: Removed the file playback and path input from the Plasma widget. This simplifies the UI and focuses on the widget's primary purpose: real-time system audio visualization.
- **Cleaner UI**: The widget now only shows the essential controls (Device Selection and Refresh), leaving more room for the beautiful spectrum bars.

---
# Plasmoid Visualizer v1.0 - Initial Release

Welcome to the first official release of **Plasmoid Visualizer**!

This project brings high-performance, aesthetically pleasing audio visualization to your Linux desktop, both as a standalone power-tool and a native KDE Plasma 6 widget.

## Key Features

### Stunning Visuals
- **High-Resolution FFT**: Powered by KissFFT with an 8192-sample window for precise frequency detection.
- **Logarithmic Mapping**: Bars are distributed naturally across the frequency spectrum, ensuring great visibility for both bass and treble.
- **Smooth Motion**: Custom temporal smoothing (attack/decay) and spatial neighbor-averaging for that iconic "Trap Nation" look.
- **256 Dynamic Bars**: A dense, vibrant spectrum that reacts instantly to your music.

### Dual-Mode Experience
- **Standalone App**: A full-featured OpenGL application with a detailed ImGui control panel. Perfect for deep configuration and testing.
- **Plasma 6 Widget**: A native Plasmoid built from the ground up for the latest KDE Plasma desktop. It lives on your desktop or panel and "just works."

### Real-Time Audio
- **System Capture**: Listen directly to your system audio, microphone, or any other input device.
- **Device Selection**: Easily switch between audio inputs via the built-in dropdown menu.
- **Low Latency**: Optimized C++ backend using `miniaudio` for minimal delay between sound and sight.

## Pre-Configured Defaults
We've tuned the initial experience to look great out of the box:
- **Gain**: 0.100
- **Falloff**: 0.500
- **Bar Height**: 0.100
- **Default Device**: Automatically attempts to select your primary headset/output.

## Installation
Check out the [README.md](README.md) for detailed build and installation instructions for both the standalone app and the Plasma widget.

---
Made with ❤️ and a lot of coffee. Enjoy the music!
