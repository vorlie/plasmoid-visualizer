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

## 3. Creating Release Assets

If you want to create a package for others to download, you can use the provided packaging script:

```bash
# Make the script executable
chmod +x package.sh

# Run the script
./package.sh
```

This will create a `release_assets/` directory containing:
- `plasmoid-visualizer.plasmoid`: The ready-to-install Plasma widget.
- `plasmoid-visualizer-standalone-linux.zip`: The standalone application binary.

---

## Features
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
- **Standalone**: File playback, Live Mode (System Audio), Test Tone, adjustable Gain/Falloff/Bar Height.
- **Widget**: Native Plasma 6 support, real-time capture, device selection, optimized defaults.
- **Visuals**: Logarithmic frequency mapping, spatial smoothing, 256 bars.

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
