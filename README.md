# Plasmoid Visualizer 🎨🔊

A high-performance C++ Audio Visualizer suite featuring a powerful standalone OpenGL editor and a native KDE Plasma 6 widget.

[![Watch the Preview](https://img.shields.io/badge/Watch-Preview_Video-red?style=for-the-badge&logo=youtube)](https://youtu.be/zlsPb7s_U0g)



## Highlights 🚀
- **Multi-Layer Engine**: Stack different visualizers (Bars, XY, Particles) with independent frequency analysis.
- Smooth spline curves with area fill
- Real-time audio analysis with logarithmic mapping
- Comprehensive Debug and System Monitoring
- Technical FAQ for architectural details

## 🗺️ Documentation
- **[TECHNICAL_FAQ.md](TECHNICAL_FAQ.md)**: Deep dive into the architecture, optimizations, and visual science of the project.
- **Config Persistence**: Automatically saves your setup to `~/.config/PlasmoidVisualizerStd/config.toml`.
  - or `C:\Users\<username>\AppData\Roaming\PlasmoidVisualizerStd\config.toml` on Windows
- **Oscilloscope Music Editor**: Write custom mathematical expressions `X(t)` and `Y(t)` to generate visuals.
- **FBO Rendering**: Isolated effect accumulation for professional visuals without UI ghosting.

### Project layout

- `src/app`: application entry point, state, and runtime asset lookup
- `src/audio`: audio input, analysis, and oscilloscope music generation
- `src/config`: configuration serialization and application mapping
- `src/rendering`: OpenGL resources, visualization geometry, and frame rendering
- `src/ui`: UI coordination and individual panel implementations
- `src/export`: offline video rendering
- `src/platform`: operating-system-specific statistics
- `assets`: shaders, images, and optional bundled fonts
- `third_party`: vendored source dependencies
- `tests`: standalone unit tests

---

## 1. Windows (Standalone) 🪟
The standalone editor runs natively on Windows 10/11 using OpenGL 3.3 and WASAPI.

### Prerequisites
- **Visual Studio 2022 or newer** (tested on VS 2026) with "Desktop development with C++"
- **CMake 3.20+**
- **Git**
- **Dependencies** (Extract to project root):
  - [GLEW 2.3.1](https://github.com/nigels-com/glew/releases/tag/glew-2.3.1)
  - [GLFW 3.4 (64-bit Windows Binary)](https://github.com/glfw/glfw/releases/tag/3.4)
  - [GLM 1.0.3](https://github.com/g-truc/glm/releases/tag/1.0.3)

### Build and Run
```cmd
:: Clone the repository
git clone https://github.com/vorlie/plasmoid-visualizer.git
cd plasmoid-visualizer

:: Generate Project Files
cmake -B build

:: Build Release Configuration
cmake --build build --config Release

:: Run the application
.\build\Release\PlasmoidVisualizer.exe
```

### Installer (Optional)
Run the build script without arguments for an interactive menu:
```cmd
.\build_installer.bat
```

It can also be used directly from a terminal:

```cmd
.\build_installer.bat debug
.\build_installer.bat build Release
.\build_installer.bat test Debug
.\build_installer.bat package Release
.\build_installer.bat installer
```

The `installer` option requires **Inno Setup 6**. Use
`build_installer.bat help` for all available commands. The script uses an
existing `build-vs17` configuration when present, otherwise it uses `build`.

---

## 2. Linux (Standalone) 🐧
The standalone editor is the core of the project, built for deep customization and real-time tweaks.

### Build and Run
```bash
# Create build directory
mkdir -p build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Run the application
./PlasmoidVisualizer
```

---

## 3. KDE Plasma 6 Widget 🐧
The native widget brings high-performance visualization directly to your desktop or panel.

### Installation
```bash
# Build the plugin
mkdir -p build_plasma && cd build_plasma
cmake ../plasma_widget
make -j$(nproc)
cp libvisualizerv2plugin.so ../plasma_widget/contents/ui/

# Install the widget
cd ..
kpackagetool6 --type Plasma/Applet --install plasma_widget
```

---

## Features ✨

### 🎭 Visual Synthesis
- **Layer Manager**: Add unlimited visualization layers. Mixing different shapes creates unique visuals.
- **Advanced Shapes**: Bars, Lines, Dots, Waveform, and **Oscilloscope XY**.
- **Particle System**: Reactive physics with beat detection and customizable energy.

### 📺 Oscilloscope XY Mode
- **Analog Aesthetics**: True-to-life trace persistence and CRT glow.
- **Grid System**: 10x10 graticule grid that follows the trace aspect ratio perfectly.
- **Transformations**: Real-time rotation, flipping, and scaling.

---

## 🎨 Pro Tips: Oscilloscope Mastery

To get the most stunning, laser-sharp visuals from the Oscilloscope XY mode, follow these best practices:

- **Sample Rate is King**: Use **96kHz or 192kHz** audio files when possible. Higher sample rates provide more points per frame, resulting in silky-smooth curves instead of jagged lines.
- **Lossless Only**: Avoid low-bitrate MP3s. Compression artifacts often appear as "fuzz" or "noise" on the oscilloscope trace. For the cleanest visuals, stick to **FLAC or WAV**.
- **Phase is Everything**: Oscilloscope XY mode relies on the difference between the Left and Right channels. Pure mono audio will only appear as a diagonal line. Experiment with **stereo imaging effects** and **chorus** to add depth and volume to your patterns.
- **The 1:1 Rule**: Our visualizer automatically enforces a square aspect ratio. Use the **Rotation** and **Trace Thickness** controls to fine-tune the "phosphor" look for your specific screen resolution.

---

### 📂 Management & Persistence
- **Playlist & Scanning**: Specify a music folder and browse your library instantly.
- **XDG Standards**: Configuration is stored properly in standard Linux user directories.
- **Video Export**: Export your music as high-quality MP4 using FFMPEG integration.

---

## Credits & Licenses 💎

This project is built upon excellence:
- **[toml++](https://marzer.github.io/tomlplusplus/)**: Modern TOML parsing for C++.
- **[miniaudio](https://github.com/mackron/miniaudio)**: Audio engine with low-latency capture.
- **[KissFFT](https://github.com/mborgerding/kissfft)**: Spectral analysis engine.
- **[Dear ImGui](https://github.com/ocornut/imgui)**: Immediate-node UI for the editor.
- **[GLFW](https://www.glfw.org/)** / **[GLEW](http://glew.sourceforge.net/)**: OpenGL context and extensions.

---

## License
Licensed under the **MIT [License](LICENSE)**.

---
Made with ❤️ for the Linux Audio community. Enjoy!
