#include "CliOptions.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace {
std::string lowercaseCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool parseIntStrict(const std::string& text, int& out) {
    if (text.empty()) return false;
    char* endPtr = nullptr;
    const long val = std::strtol(text.c_str(), &endPtr, 10);
    if (endPtr == text.c_str() || *endPtr != '\0') return false;
    out = static_cast<int>(val);
    return true;
}

bool parseDoubleStrict(const std::string& text, double& out) {
    if (text.empty()) return false;
    char* endPtr = nullptr;
    const double val = std::strtod(text.c_str(), &endPtr);
    if (endPtr == text.c_str() || *endPtr != '\0') return false;
    out = val;
    return true;
}

bool parseSizeSpec(const std::string& text, int& width, int& height) {
    const size_t sep = text.find_first_of("xX");
    if (sep == std::string::npos || sep == 0 || sep + 1 >= text.size()) return false;
    int w = 0;
    int h = 0;
    if (!parseIntStrict(text.substr(0, sep), w) || !parseIntStrict(text.substr(sep + 1), h)) {
        return false;
    }
    if (w < 64 || h < 64 || w > 7680 || h > 4320 || (w % 2) != 0 || (h % 2) != 0) {
        return false;
    }
    width = w;
    height = h;
    return true;
}

bool parseCodecName(const std::string& text, int& codecIdx) {
    const std::string lower = lowercaseCopy(text);
    if (lower == "x264" || lower == "libx264" || lower == "cpu") {
        codecIdx = 0;
        return true;
    }
    if (lower == "nvenc" || lower == "h264_nvenc" || lower == "nvidia") {
        codecIdx = 1;
        return true;
    }
    if (lower == "vaapi" || lower == "h264_vaapi") {
        codecIdx = 2;
        return true;
    }
    if (lower == "amf" || lower == "h264_amf" || lower == "amd") {
        codecIdx = 3;
        return true;
    }
    return false;
}
} // namespace

CliOptions CliParser::parse(int argc, const char* const* argv) {
    std::vector<std::string> args;
    if (argc > 1 && argv != nullptr) {
        args.reserve(static_cast<size_t>(argc - 1));
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i] ? argv[i] : "");
        }
    }
    return parse(args);
}

CliOptions CliParser::parse(const std::vector<std::string>& args) {
    CliOptions opts;
    std::optional<int> parsedFps;

    auto requireValue = [&](size_t& i, const std::string& flag, std::string& out) -> bool {
        if (i + 1 >= args.size()) {
            opts.errorMessage = "Missing value for " + flag;
            return false;
        }
        out = args[++i];
        return true;
    };

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "-h" || arg == "--help") {
            opts.showHelp = true;
            return opts;
        } else if (arg == "--benchmark") {
            opts.benchmark.enabled = true;
        } else if (arg == "--layers") {
            std::string val;
            int n = 1;
            if (!requireValue(i, arg, val) || !parseIntStrict(val, n)) {
                if (opts.errorMessage.empty()) opts.errorMessage = "Invalid integer for --layers: " + val;
                return opts;
            }
            opts.benchmark.layers = std::clamp(n, 1, 16);
        } else if (arg == "--vsync") {
            std::string val;
            int n = 1;
            if (!requireValue(i, arg, val) || !parseIntStrict(val, n)) {
                if (opts.errorMessage.empty()) opts.errorMessage = "Invalid integer for --vsync: " + val;
                return opts;
            }
            opts.benchmark.vsync = (n != 0);
        } else if (arg == "--seconds") {
            std::string val;
            double secs = 2.5;
            if (!requireValue(i, arg, val) || !parseDoubleStrict(val, secs)) {
                if (opts.errorMessage.empty()) opts.errorMessage = "Invalid number for --seconds: " + val;
                return opts;
            }
            opts.benchmark.durationSeconds = std::clamp(secs, 0.5, 30.0);
        } else if (arg == "--fps") {
            std::string val;
            int fpsVal = 60;
            if (!requireValue(i, arg, val) || !parseIntStrict(val, fpsVal)) {
                if (opts.errorMessage.empty()) opts.errorMessage = "Invalid integer for --fps: " + val;
                return opts;
            }
            parsedFps = fpsVal;
        } else if (arg == "--export") {
            if (i + 2 >= args.size()) {
                opts.errorMessage = "--export requires <input_audio> <output_video>";
                return opts;
            }
            opts.exportJob.enabled = true;
            opts.exportJob.inputAudioPath = args[++i];
            opts.exportJob.outputVideoPath = args[++i];
            if (opts.exportJob.inputAudioPath.empty() || opts.exportJob.outputVideoPath.empty()) {
                opts.errorMessage = "--export input and output paths cannot be empty";
                return opts;
            }
        } else if (arg == "--preset") {
            if (!requireValue(i, arg, opts.exportJob.presetNameOrPath)) return opts;
        } else if (arg == "--lyrics") {
            if (!requireValue(i, arg, opts.exportJob.lyricsPath)) return opts;
        } else if (arg == "--artist") {
            std::string val;
            if (!requireValue(i, arg, val)) return opts;
            opts.exportJob.artist = val;
        } else if (arg == "--title") {
            std::string val;
            if (!requireValue(i, arg, val)) return opts;
            opts.exportJob.title = val;
        } else if (arg == "--background") {
            if (!requireValue(i, arg, opts.exportJob.backgroundPath)) return opts;
        } else if (arg == "--size") {
            std::string val;
            int w = 0, h = 0;
            if (!requireValue(i, arg, val) || !parseSizeSpec(val, w, h)) {
                if (opts.errorMessage.empty()) {
                    opts.errorMessage = "Invalid --size '" + val + "' (expected even WxH between 64x64 and 7680x4320)";
                }
                return opts;
            }
            opts.exportJob.width = w;
            opts.exportJob.height = h;
        } else if (arg == "--codec") {
            std::string val;
            int codecIdx = 0;
            if (!requireValue(i, arg, val) || !parseCodecName(val, codecIdx)) {
                if (opts.errorMessage.empty()) {
                    opts.errorMessage = "Unknown --codec '" + val + "' (expected x264, nvenc, vaapi, or amf)";
                }
                return opts;
            }
            opts.exportJob.codecIdx = codecIdx;
        } else if (arg == "--crf") {
            std::string val;
            int crfVal = 16;
            if (!requireValue(i, arg, val) || !parseIntStrict(val, crfVal) || crfVal < 0 || crfVal > 51) {
                if (opts.errorMessage.empty()) {
                    opts.errorMessage = "Invalid --crf '" + val + "' (expected integer 0..51)";
                }
                return opts;
            }
            opts.exportJob.crf = crfVal;
        } else if (arg == "--no-overlay") {
            opts.exportJob.disableOverlay = true;
        } else if (arg == "--shader-dir") {
            if (!requireValue(i, arg, opts.shaderDirOverride)) return opts;
        } else if (arg == "--watch-shaders") {
            opts.watchShaders = true;
        } else {
            opts.errorMessage = "Unknown option: " + arg;
            return opts;
        }
    }

    if (opts.benchmark.enabled && opts.exportJob.enabled) {
        opts.errorMessage = "Cannot combine --benchmark and --export";
        return opts;
    }

    if (parsedFps.has_value()) {
        if (opts.exportJob.enabled) {
            if (*parsedFps < 1 || *parsedFps > 240) {
                opts.errorMessage = "Invalid --fps for --export (must be 1..240)";
                return opts;
            }
            opts.exportJob.fps = *parsedFps;
        } else {
            opts.benchmark.targetFps = std::clamp(*parsedFps, 0, 1000);
        }
    }

    return opts;
}

std::string CliParser::usageText() {
    return
        "Plasmoid Visualizer\n"
        "\n"
        "Usage:\n"
        "  PlasmoidVisualizer [options]\n"
        "  PlasmoidVisualizer --export <input_audio> <output_video> [export options]\n"
        "  PlasmoidVisualizer --benchmark [--layers N] [--vsync 0|1] [--fps N] [--seconds S]\n"
        "\n"
        "Export options:\n"
        "  --preset <name_or_path>        Load visual preset (built-in/user name or .toml path)\n"
        "  --lyrics <file.lrc>            Load LRC lyrics file into Media Overlay\n"
        "  --artist <text>                Override Media Overlay artist text\n"
        "  --title <text>                 Override Media Overlay title text\n"
        "  --background <file>            Set Media Overlay still/GIF/video background\n"
        "  --size <WxH>                   Output resolution (even dimensions, e.g. 1920x1080)\n"
        "  --fps <1..240>                 Output video frame rate\n"
        "  --codec <x264|nvenc|vaapi|amf> Video encoder\n"
        "  --crf <0..51>                  Encoder quality (CRF/QP)\n"
        "  --no-overlay                   Disable Media Overlay for this render\n"
        "\n"
        "General / developer options:\n"
        "  --shader-dir <dir>             Load GLSL shaders from a custom directory\n"
        "  --watch-shaders                Enable live GLSL shader hot-reload in GUI mode\n"
        "  -h, --help                     Show this help message\n";
}
