#include "FfmpegArgs.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool hasPair(const std::vector<std::string>& args, const std::string& flag, const std::string& value) {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag && args[i + 1] == value) return true;
    }
    return false;
}
} // namespace

int main() {
    VideoRenderSettings s;
    s.width = 1920;
    s.height = 1080;
    s.fps = 60;
    s.codecIdx = 0;
    s.crf = 18;
    s.outputPath = "out/my render & clip.mp4";

    // 1. WAV input into MP4 must transcode audio to AAC (never -c:a copy).
    const std::string hostileInput = "C:/Music/song; rm -rf / & $(whoami) \"quoted\".wav";
    const auto wavToMp4 = FfmpegArgs::buildExportArgs(s, hostileInput, /*allowVaapi=*/false);
    if (!hasPair(wavToMp4, "-c:a", "aac") || !hasPair(wavToMp4, "-b:a", "320k")) {
        std::cerr << "WAV -> MP4 must select AAC 320k audio encoding\n";
        return 1;
    }
    if (!hasPair(wavToMp4, "-i", hostileInput) || wavToMp4.back() != s.outputPath) {
        std::cerr << "Paths must be passed verbatim as single argv elements\n";
        return 1;
    }

    // 2. MP3 / M4A into MP4 should stream-copy audio without lossy re-encode.
    const auto mp3ToMp4 = FfmpegArgs::buildExportArgs(s, "track.MP3", /*allowVaapi=*/false);
    if (!hasPair(mp3ToMp4, "-c:a", "copy")) {
        std::cerr << "MP3 -> MP4 should use -c:a copy\n";
        return 1;
    }

    // 3. WAV into MKV can safely stream-copy audio.
    s.outputPath = "out/lossless.mkv";
    const auto wavToMkv = FfmpegArgs::buildExportArgs(s, "track.wav", /*allowVaapi=*/false);
    if (!hasPair(wavToMkv, "-c:a", "copy")) {
        std::cerr << "WAV -> MKV should use -c:a copy\n";
        return 1;
    }

    // 4. VAAPI codec selection on Windows (allowVaapi=false) must fall back to libx264 + non-hwupload vf.
    s.codecIdx = 2;
    s.outputPath = "out/vaapi_fallback.mp4";
    const auto winVaapi = FfmpegArgs::buildExportArgs(s, "track.flac", /*allowVaapi=*/false);
    if (!hasPair(winVaapi, "-c:v", "libx264") || hasPair(winVaapi, "-c:v", "h264_vaapi")) {
        std::cerr << "VAAPI disabled should fall back to libx264\n";
        return 1;
    }
    if (std::find(winVaapi.begin(), winVaapi.end(), "-vaapi_device") != winVaapi.end()) {
        std::cerr << "VAAPI disabled should not pass -vaapi_device\n";
        return 1;
    }

    // 5. VAAPI enabled (Linux) should use h264_vaapi + hwupload filter.
    const auto linuxVaapi = FfmpegArgs::buildExportArgs(s, "track.flac", /*allowVaapi=*/true);
    if (!hasPair(linuxVaapi, "-c:v", "h264_vaapi") ||
        !hasPair(linuxVaapi, "-vf", "vflip,format=nv12,hwupload")) {
        std::cerr << "VAAPI enabled should configure h264_vaapi and hwupload filter\n";
        return 1;
    }

    return 0;
}
