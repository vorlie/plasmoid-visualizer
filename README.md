# Plasmoid Visualizer

A high-performance C++ Audio Visualizer with a standalone OpenGL application and a native KDE Plasma 6 widget.

## Prerequisites

- **C++ Compiler** (GCC 11+ or Clang)
- **CMake** (3.16+)
- **Qt 6** (Core, Quick, Qml)
- **KDE Plasma 6** (for the widget)
- **Development Libraries**:
  - `libglfw3-dev`
  - `libglew-dev`
  - `libgl1-mesa-dev`

---

## 1. Standalone Application

The standalone app uses OpenGL and ImGui for a feature-rich visualization experience.

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

## 2. KDE Plasma 6 Widget

The widget integrates directly into your KDE Plasma desktop.

### Build the Plugin
```bash
# Create build directory for the plasma plugin
mkdir -p build_plasma && cd build_plasma

# Configure and build
cmake ../plasma_widget
make -j$(nproc)

# Copy the compiled plugin to the widget's UI folder
cp libvisualizerv2plugin.so ../plasma_widget/contents/ui/
```

### Install/Upgrade the Widget
```bash
# Go back to the root directory
cd ..

# Install the widget (first time)
kpackagetool6 --type Plasma/Applet --install plasma_widget

# OR Upgrade the widget (if already installed)
kpackagetool6 --type Plasma/Applet --upgrade plasma_widget

# Refresh Plasma cache
kbuildsycoca6
```

---

### Usage
1. Right-click desktop -> **Enter Edit Mode**.
2. Click **Add Widgets**.
3. Search for **"Plasmoid Visualizer"** and drag it to your desktop.
4. If it doesn't appear or shows an error, restart Plasma:
   ```bash
   plasmashell --replace &
   ```

---

## Features
- **Modular Standalone Editor**: A powerful OpenGL application with a window-based UI for deep customization.
- **Multi-Layer System**: Stack multiple visualizer layers with independent settings (Gain, Falloff, Frequency Range).
- **Customizable Aesthetics**: Per-layer color selection and bar height scaling.
- **High-Resolution Analysis**: Powered by a C++ backend with an 8192-sample FFT window for precise frequency detection.
- **Logarithmic Mapping**: Bars are naturally distributed across the spectrum.
- **Trap Nation Style Smoothing**: Custom temporal smoothing and spatial neighbor-averaging.
- **Real-Time System Capture**: Listen directly to system audio or microphone with minimal latency.
- **Plasma 6 Native Widget**: A streamlined, high-performance widget for your desktop or panel.

---

## Credits & Licenses

This project wouldn't be possible without these amazing open-source libraries:

- **[miniaudio](https://github.com/mackron/miniaudio)**: Single-header audio playback and capture library (Public Domain / MIT).
- **[KissFFT](https://github.com/mborgerding/kissfft)**: A mixed-radix Fast Fourier Transform generator (BSD 3-Clause).
- **[Dear ImGui](https://github.com/ocornut/imgui)**: Bloat-free Graphical User interface for C++ (MIT License).
- **[GLFW](https://www.glfw.org/)**: Open Source, multi-platform library for OpenGL (zlib/libpng license).
- **[GLEW](http://glew.sourceforge.net/)**: The OpenGL Extension Wrangler Library (Modified BSD / MIT).

---

## License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---
Check out the [RELEASE_NOTES.md](RELEASE_NOTES.md) for the latest updates!

---
Made with ❤️ and a lot of coffee. Enjoy the music!