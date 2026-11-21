# Plasmoid Visualizer

A real-time, multi-layer C++17 / OpenGL 3.3 Core audio visualizer and offline video exporter for Windows and Linux.

[![Watch the Preview](https://img.shields.io/badge/Watch-Preview_Video-red?style=for-the-badge&logo=youtube)](https://youtu.be/zlsPb7s_U0g)

## Highlights

- **Multi-Layer Engine**: Stack independent layers (`Bars`, `Lines`, `Dots`, `Waveform`, `Curve`, and `Oscilloscope XY`) with per-layer frequency range, gain, attack/falloff, smoothing, channel selection, and color.
- **Analog-Style XY Oscilloscope**: Continuous or triggered acquisition, AC/DC coupling, bandwidth limiting, phosphor fast/slow afterglow, HDR bloom, graticule grid, and live signal measurements.
- **Media Overlay Layer**: Always-on-top metadata, dual spectra, synchronized LRC lyrics with Timeline & raw LRC editor, Gaussian-blurred lyric band, system font selection, and still image / animated GIF / looped video backgrounds.
- **Oscilloscope Music Editor**: Write custom `X(t)`, `Y(t)`, `Z(t)` expressions using `tinyexpr`, preview live, and synthesize up to 192 kHz stereo + Z buffers.
- **Shareable Visual Presets**: Save, load, import, and share self-contained visual presets (`assets/presets/*.toml` + user `<config_dir>/presets/*.toml`) without overwriting machine-local audio device, playlist, font, or video paths.
- **Offline & Headless CLI Video Export**: Deterministic frame-by-frame capture with dedicated audio decoder, double-buffered PBO readback, background writer thread, live GUI preview, and `--export` headless CLI batch rendering.
- **Live GLSL Shader Hot-Reload**: Edit `.vert`/`.frag` shaders while the visualizer runs (`F5`, `--watch-shaders`, `--shader-dir`, or `Debug Info`) with safe rollback on compile errors.
- **Automatic Config Persistence**: Saves settings on clean exit (and on demand via `File > Save Config`) to:
  - Windows: `%APPDATA%\PlasmoidVisualizerStd\config.toml` (with UI layout in `imgui.ini` alongside it)
  - Linux: `~/.config/PlasmoidVisualizerStd/config.toml`

## Documentation

- [TECHNICAL_FAQ.md](TECHNICAL_FAQ.md) — Architecture, audio/DSP pipeline, rendering passes, and platform notes.
- [CONTRIBUTING.md](CONTRIBUTING.md) — Building, testing, warning standards, and commit conventions.

### Project Layout

- `src/app`: application entry point, CLI option parser (`CliOptions`), runtime state (`AppState`), and executable-relative asset lookup (`AssetPaths`)
- `src/audio`: wait-free SPSC `AudioEngine` (`miniaudio`), FFT/log-binning `AnalysisEngine` (`KissFFT`), and `OscMusicEditor` (`tinyexpr`)
- `src/config`: `PersistentConfig`, atomic TOML `ConfigManager` + presets (`toml++`), and `ConfigLogic`
- `src/rendering`: OpenGL RAII handles (`GlResources`), hot-reloadable `ShaderProgram`, `RenderManager`, `Visualizer` facade (`QuadRenderer`, `TextRenderer`, `SpectrumRenderer`, `XYTraceRenderer`), `XYOscilloscopeEngine`, `BloomRenderer`, `GaussianBlurRenderer`, `OverlayPreset`, `LyricsTimeline`, and `AnimatedBackground`
- `src/export`: asynchronous PBO `VideoRenderManager` and `FfmpegArgs`
- `src/platform`: safe argument-vector `SubprocessPipe`, `SystemStats`, and `Utf8Paths`
- `src/ui`: Dear ImGui `UIManager`, `UIHelpers`, and individual panel modules (including `PresetsPanel`)
- `assets`: GLSL 330 core shaders, built-in visual presets (`assets/presets/*.toml`), and optional bundled fonts
- `third_party`: vendored source dependencies (`imgui`, `kissfft`, `miniaudio`, `stb`, `tinyexpr`, `toml++`)
- `tests`: standalone CTest unit test suite

---

## 1. Windows Build & Run

### Prerequisites

- **Visual Studio 2022** (or newer) with *Desktop development with C++*
- **CMake 3.16+**
- **Git**
- **FFmpeg** in `PATH` *(optional — required for Video Export and Looped Video backgrounds)*

By default, CMake fetches and builds **GLFW 3.4** and **GLEW 2.3.1** (static, matched Debug/Release CRTs) via `FetchContent`, so no manual dependency downloads or runtime DLL setup are required. To build offline using local binary directories instead, pass `-DPLASMOID_USE_FETCHCONTENT=OFF`.

### Build, Test & Run

```powershell
# Configure (Visual Studio 2022 x64)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build Release
cmake --build build --config Release --parallel

# Run unit tests
ctest --test-dir build -C Release --output-on-failure

# Run the application (assets are copied beside the executable automatically)
.\build\Release\PlasmoidVisualizer.exe
```

### CLI Reference (Headless Export, Shaders & Benchmarks)

Run `PlasmoidVisualizer --help` for the full list of options.

**Headless batch video export** (runs with a hidden OpenGL context, no GUI, exits `0` on success):

```powershell
.\build\Release\PlasmoidVisualizer.exe --export song.flac out.mp4 `
    --preset "Stylized CRT Music" `
    --lyrics song.lrc --artist "Artist" --title "Track" `
    --size 1920x1080 --fps 60 --codec x264 --crf 18
```

*(On displayless Linux servers without X11/Wayland, prefix with `xvfb-run -a`.)*

**Live GLSL shader iteration** (point directly at `assets/shaders` without rebuilding; `F5` forces an immediate reload, `Debug Info` displays status and compiler logs):

```powershell
.\build\Release\PlasmoidVisualizer.exe --shader-dir .\assets\shaders --watch-shaders
```

**Built-in visual presets** (`File > Presets...` or `--preset <name_or_path>`):
- Bundled in `assets/presets/`: *Analog Scope*, *Stylized CRT Music*, *Neon Dual Bars*, *Mirrored Spline Glow*
- User presets saved to `%APPDATA%\PlasmoidVisualizerStd\presets\` (Windows) or `~/.config/PlasmoidVisualizerStd/presets/` (Linux)

**Benchmark CLI (Optional):**

```powershell
.\build\Release\PlasmoidVisualizer.exe --benchmark --layers 5 --vsync 0 --fps 60 --seconds 2
```

### Installer (Optional)

```cmd
.\build_installer.bat
```

Direct commands: `build_installer.bat build Release`, `build_installer.bat test Debug`, `build_installer.bat package Release`, `build_installer.bat installer` (requires **Inno Setup 6**).

---

## 2. Linux Build & Run

### Prerequisites (Debian / Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libgl1-mesa-dev libglfw3-dev libglew-dev \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
# Optional (for video export & looped video backgrounds):
sudo apt-get install -y ffmpeg
```

*(If system `glfw3`/`GLEW` development packages are unavailable, CMake falls back to `FetchContent` automatically.)*

### Build, Test & Run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/PlasmoidVisualizer
```

---

## Pro Tips: Oscilloscope XY Mode

- **Sample Rate**: 96 kHz or 192 kHz files yield significantly smoother XY curves than 44.1 kHz.
- **Lossless Audio**: Prefer FLAC or WAV over low-bitrate lossy formats, whose compression artifacts appear as trace fuzz.
- **Stereo Separation**: XY mode plots Left (X) against Right (Y); pure mono audio draws a diagonal line.

## Credits & Licenses

- [toml++](https://marzer.github.io/tomlplusplus/)
- [miniaudio](https://github.com/mackron/miniaudio)
- [KissFFT](https://github.com/mborgerding/kissfft)
- [tinyexpr](https://github.com/codeplea/tinyexpr)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [stb](https://github.com/nothings/stb)
- [GLFW](https://www.glfw.org/) / [GLEW](http://glew.sourceforge.net/)

Licensed under the [MIT License](LICENSE).
