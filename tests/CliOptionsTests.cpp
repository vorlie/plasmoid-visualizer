#include "CliOptions.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
    // 1. Full --export invocation with order-independent flags.
    {
        const CliOptions opts = CliParser::parse({
            "--fps", "60",
            "--export", "songs/demo.flac", "out/video.mp4",
            "--preset", "Analog Scope",
            "--lyrics", "songs/demo.lrc",
            "--artist", "Test Artist",
            "--title", "Test Track",
            "--background", "bg.gif",
            "--size", "1280x720",
            "--codec", "nvenc",
            "--crf", "18",
            "--no-overlay",
            "--shader-dir", "assets/shaders"
        });
        if (!opts.errorMessage.empty()) {
            std::cerr << "Unexpected parse error: " << opts.errorMessage << "\n";
            return 1;
        }
        if (!opts.exportJob.enabled ||
            opts.exportJob.inputAudioPath != "songs/demo.flac" ||
            opts.exportJob.outputVideoPath != "out/video.mp4" ||
            opts.exportJob.presetNameOrPath != "Analog Scope" ||
            opts.exportJob.lyricsPath != "songs/demo.lrc" ||
            opts.exportJob.artist != "Test Artist" ||
            opts.exportJob.title != "Test Track" ||
            opts.exportJob.backgroundPath != "bg.gif" ||
            opts.exportJob.width != 1280 ||
            opts.exportJob.height != 720 ||
            opts.exportJob.fps != 60 ||
            opts.exportJob.codecIdx != 1 ||
            opts.exportJob.crf != 18 ||
            !opts.exportJob.disableOverlay ||
            opts.shaderDirOverride != "assets/shaders") {
            std::cerr << "Parsed --export options did not match expected values\n";
            return 1;
        }
    }

    // 2. Benchmark options remain compatible.
    {
        const CliOptions opts = CliParser::parse({
            "--benchmark", "--layers", "5", "--vsync", "0", "--fps", "0", "--seconds", "2.0"
        });
        if (!opts.errorMessage.empty() || !opts.benchmark.enabled ||
            opts.benchmark.layers != 5 || opts.benchmark.vsync != false ||
            opts.benchmark.targetFps != 0) {
            std::cerr << "Benchmark CLI parsing failed\n";
            return 1;
        }
    }

    // 3. Help flag exits cleanly without error.
    {
        const CliOptions opts = CliParser::parse({"--help"});
        if (!opts.showHelp || !opts.errorMessage.empty() || CliParser::usageText().empty()) {
            std::cerr << "--help parsing failed\n";
            return 1;
        }
    }

    // 4. Error cases: odd size, missing export args, conflicting modes, unknown codec, invalid CRF.
    if (CliParser::parse({"--export", "in.wav"}).errorMessage.empty()) {
        std::cerr << "Missing --export output path should fail\n";
        return 1;
    }
    if (CliParser::parse({"--export", "in.wav", "out.mp4", "--size", "1921x1080"}).errorMessage.empty()) {
        std::cerr << "Odd --size width should fail\n";
        return 1;
    }
    if (CliParser::parse({"--export", "in.wav", "out.mp4", "--codec", "invalid"}).errorMessage.empty()) {
        std::cerr << "Unknown --codec should fail\n";
        return 1;
    }
    if (CliParser::parse({"--export", "in.wav", "out.mp4", "--crf", "99"}).errorMessage.empty()) {
        std::cerr << "Out-of-range --crf should fail\n";
        return 1;
    }
    if (CliParser::parse({"--benchmark", "--export", "in.wav", "out.mp4"}).errorMessage.empty()) {
        std::cerr << "Combining --benchmark and --export should fail\n";
        return 1;
    }
    if (CliParser::parse({"--unknown-flag"}).errorMessage.empty()) {
        std::cerr << "Unknown flag should fail\n";
        return 1;
    }

    return 0;
}
