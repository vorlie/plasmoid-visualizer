# Plasmoid Visualizer 🎨🔊

A high-performance C++ Audio Visualizer suite featuring a powerful standalone OpenGL editor and a native KDE Plasma 6 widget.

## Highlights 🚀
- **Multi-Layer Engine**: Stack different visualizers (Bars, XY, Particles) with independent frequency analysis.
- **High-Fidelity Oscilloscope**: Real-time XY trace with bloom, analog-style phosphor decay, and graticule grids.
- **Config Persistence**: Automatically saves your setup to `~/.config/PlasmoidVisualizerStd/config.toml`.
- **Oscilloscope Music Editor**: Write custom mathematical expressions `X(t)` and `Y(t)` to generate visuals.
- **FBO Rendering**: Isolated effect accumulation for professional visuals without UI ghosting.

---

## 1. Standalone Application 🖥️
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

## 2. KDE Plasma 6 Widget 🐧
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
Licensed under the **MIT License**. Check out [RELEASE_NOTES.md](RELEASE_NOTES.md) for the latest v1.4 changelog.

---
Made with ❤️ for the Linux Audio community. Enjoy!