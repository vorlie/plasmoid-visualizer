# Fonts

Place optional bundled application fonts in this directory.

- If `AdwaitaMono-Regular.ttf` is present here, the visualizer uses it as the default overlay and UI font.
- Otherwise, `AssetPaths::defaultFont()` falls back to:
  - **Windows**: `YuGothM.ttc`, `meiryo.ttc`, `msgothic.ttc`, then `consola.ttf` in `%WINDIR%\Fonts`
  - **Linux**: `/usr/share/fonts/Adwaita/AdwaitaMono-Regular.ttf`, then `/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf`
