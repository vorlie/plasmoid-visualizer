# 🚀 Plasmoid Visualizer v1.0 - Initial Release

Welcome to the first official release of **Plasmoid Visualizer**! 🎵✨

This project brings high-performance, aesthetically pleasing audio visualization to your Linux desktop, both as a standalone power-tool and a native KDE Plasma 6 widget.

## ✨ Key Features

### 🎨 Stunning Visuals
- **High-Resolution FFT**: Powered by KissFFT with an 8192-sample window for precise frequency detection.
- **Logarithmic Mapping**: Bars are distributed naturally across the frequency spectrum, ensuring great visibility for both bass and treble.
- **Smooth Motion**: Custom temporal smoothing (attack/decay) and spatial neighbor-averaging for that iconic "Trap Nation" look.
- **256 Dynamic Bars**: A dense, vibrant spectrum that reacts instantly to your music.

### 🛠️ Dual-Mode Experience
- **Standalone App**: A full-featured OpenGL application with a detailed ImGui control panel. Perfect for deep configuration and testing.
- **Plasma 6 Widget**: A native Plasmoid built from the ground up for the latest KDE Plasma desktop. It lives on your desktop or panel and "just works."

### 🎙️ Real-Time Audio
- **System Capture**: Listen directly to your system audio, microphone, or any other input device.
- **Device Selection**: Easily switch between audio inputs via the built-in dropdown menu.
- **Low Latency**: Optimized C++ backend using `miniaudio` for minimal delay between sound and sight.

## 🔧 Pre-Configured Defaults
We've tuned the initial experience to look great out of the box:
- **Gain**: 0.100
- **Falloff**: 0.500
- **Bar Height**: 0.100
- **Default Device**: Automatically attempts to select your primary headset/output.

## 📦 Installation
Check out the [README.md](README.md) for detailed build and installation instructions for both the standalone app and the Plasma widget.

---
Made with ❤️ and a lot of coffee. Enjoy the music!
