#include "OscMusicEditor.hpp"

#include <cmath>
#include <iostream>
#include <string>

int main() {
    OscMusicEditor editor;
    if (!editor.isValid()) {
        std::cerr << "Default OscMusicEditor expressions should be valid\n";
        return 1;
    }

    // 1. All built-in presets must validate and generate finite preview points.
    const auto& presets = editor.getPresets();
    if (presets.empty()) {
        std::cerr << "Expected built-in OscMusic presets\n";
        return 1;
    }
    for (size_t i = 0; i < presets.size(); ++i) {
        editor.loadPreset(static_cast<int>(i));
        if (!editor.isValid()) {
            std::cerr << "Preset '" << presets[i].name << "' failed validation: "
                      << editor.getErrorMessage() << "\n";
            return 1;
        }
        const auto pts = editor.getPreviewPoints(64, 0.01f);
        if (pts.size() != 128) {
            std::cerr << "Preset '" << presets[i].name << "' returned wrong preview size\n";
            return 1;
        }
        for (float v : pts) {
            if (!std::isfinite(v) || v < -1.0f || v > 1.0f) {
                std::cerr << "Preset '" << presets[i].name << "' produced out-of-range sample\n";
                return 1;
            }
        }
    }

    // 2. Custom expression + clamping check.
    editor.setBaseFrequency(220.0f);
    if (!editor.setExpressions("2 * sin_hz(f, t)", "cos(f * t * tau)", "0.5")) {
        std::cerr << "Valid custom expressions rejected: " << editor.getErrorMessage() << "\n";
        return 1;
    }
    const auto buf = editor.generateXYBuffer(0.05f, 48000);
    if (buf.stereo.size() != 4800 || buf.z.size() != 2400) {
        std::cerr << "Unexpected generated buffer size\n";
        return 1;
    }
    for (float s : buf.stereo) {
        if (!std::isfinite(s) || s < -1.0f || s > 1.0f) {
            std::cerr << "Stereo sample was not finite/clamped to [-1, 1]\n";
            return 1;
        }
    }
    for (float z : buf.z) {
        if (std::abs(z - 0.5f) > 1e-5f) {
            std::cerr << "Unexpected Z intensity sample\n";
            return 1;
        }
    }

    // 3. Invalid syntax and division-by-zero (Inf/NaN) must fail cleanly.
    if (editor.setExpressions("sin(((", "cos(t)", "1") || editor.isValid() || editor.getErrorMessage().empty()) {
        std::cerr << "Malformed syntax should fail validation\n";
        return 1;
    }
    if (!editor.generateXYBuffer(0.1f, 48000).stereo.empty()) {
        std::cerr << "Invalid editor should not generate samples\n";
        return 1;
    }
    if (editor.setExpressions("1 / 0", "0", "1") || editor.isValid()) {
        std::cerr << "Inf expression should fail validation\n";
        return 1;
    }

    // 4. Non-positive duration / sample rate returns empty safely.
    editor.setExpressions("sin(t)", "cos(t)", "1");
    if (!editor.generateXYBuffer(0.0f, 48000).stereo.empty() ||
        !editor.generateXYBuffer(1.0f, 0).stereo.empty()) {
        std::cerr << "Non-positive duration/sampleRate should return empty buffer\n";
        return 1;
    }

    return 0;
}
