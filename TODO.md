# Plasmoid Visualizer - Editor TODO

This file tracks planned features, improvements, and ideas for the standalone visualizer editor.

## Visuals & Rendering
- [ ] **Custom Shaders**: Allow users to load custom GLSL fragment shaders for bars (e.g., gradients, glow, textures).
- [X] **Bar Shapes**: Add options for rounded bars, lines, or dots.
- [X] **Mirror Mode**: Option to mirror the spectrum from the center.
- [ ] **Circular Layout**: "Trap Nation" style circular visualizer mode.
- [ ] **Background Images**: Allow setting a background image or video behind the visualizer.
- [X] **Particles System**: Add floating particles/snow that react to the beat (Avee Player style).
- [ ] **Pulsing Logo**: A central image that scales/pulses with the audio energy.

## Layer Enhancements
- [ ] **Layer Presets**: Save and load layer configurations as JSON templates.
- [ ] **Layer Blending Modes**: Add Additive, Multiplicative, and Screen blending for overlapping layers.
- [ ] **Frequency Presets**: Quick buttons for "Sub-Bass", "Mids", "Highs" frequency ranges.
- [ ] **Layer Reordering**: Drag and drop layers to change their rendering order.

## Audio Analysis
- [ ] **Beat Detection**: Add a pulse effect or event trigger based on low-frequency peaks.
- [ ] **More FFT Windows**: Support for Blackman-Harris, Nutall, etc.
- [ ] **Logarithmic vs Linear Scaling**: Toggle between different frequency distribution modes.
- [ ] **Stereo Support**: Separate analysis for Left and Right channels.

## UI/UX
- [ ] **Theme Support**: Custom ImGui themes (colors, rounding, fonts).
- [ ] **Docking**: Enable ImGui docking to snap windows together.
- [ ] **Minimize to Tray**: Keep the editor running in the system tray.
- [ ] **Auto-Reload**: Automatically reload the last used template on startup.
- [ ] **Text Overlays**: Add customizable text (e.g., "Now Playing" info or custom labels).
- [ ] **Visualizer Presets**: A gallery of pre-made templates inspired by Avee Player/Trap Nation.

## Integration
- [ ] **Export to Widget**: A button to "Push to Widget" which updates the Plasma widget's configuration (requires a config sync mechanism).
- [ ] **Recording**: Export the visualizer output to a video file or GIF.

---
*Feel free to add more ideas!*
