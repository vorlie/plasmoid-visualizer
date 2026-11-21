#pragma once

#include <optional>
#include <string>
#include <vector>

struct BenchmarkOptions {
    bool enabled = false;
    int layers = 1;
    bool vsync = true;
    int targetFps = 60;
    double durationSeconds = 2.5;
    double warmupSeconds = 0.5;
};

struct ExportCliOptions {
    bool enabled = false;
    std::string inputAudioPath;
    std::string outputVideoPath;
    std::string presetNameOrPath;
    std::string lyricsPath;
    std::optional<std::string> artist;
    std::optional<std::string> title;
    std::string backgroundPath;
    std::optional<int> width;
    std::optional<int> height;
    std::optional<int> fps;
    std::optional<int> codecIdx;
    std::optional<int> crf;
    bool disableOverlay = false;
};

struct CliOptions {
    bool showHelp = false;
    BenchmarkOptions benchmark;
    ExportCliOptions exportJob;
    std::string shaderDirOverride;
    bool watchShaders = false;
    std::string errorMessage;
};

namespace CliParser {
CliOptions parse(int argc, const char* const* argv);
CliOptions parse(const std::vector<std::string>& args);
std::string usageText();
} // namespace CliParser
