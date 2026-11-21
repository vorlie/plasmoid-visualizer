#include "FfmpegArgs.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <cctype>

namespace {
std::string lowercaseExtension(const std::string& utf8Path) {
    std::string ext = Utf8Paths::toUtf8(Utf8Paths::fromUtf8(utf8Path).extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

bool isMp4CopyableAudioExtension(const std::string& ext) {
    return ext == ".mp3" || ext == ".m4a" || ext == ".aac";
}
} // namespace

std::vector<std::string> FfmpegArgs::buildExportArgs(
    const VideoRenderSettings& s,
    const std::string& inputAudioPath,
    bool allowVaapi
) {
    const bool useVaapi = allowVaapi && (s.codecIdx == 2);

    std::vector<std::string> args = {
        "ffmpeg",
        "-y",
        "-hide_banner",
        "-loglevel", "error",
        "-f", "rawvideo",
        "-vcodec", "rawvideo",
        "-s", std::to_string(s.width) + "x" + std::to_string(s.height),
        "-pix_fmt", "rgb24",
        "-r", std::to_string(s.fps),
        "-i", "-",
        "-i", inputAudioPath
    };

    if (useVaapi) {
        args.push_back("-vaapi_device");
        args.push_back("/dev/dri/renderD128");
        args.push_back("-vf");
        args.push_back("vflip,format=nv12,hwupload");
    } else {
        args.push_back("-vf");
        args.push_back("vflip,colorspace=all=bt709:iall=bt709");
    }

    // Codecs: 0=libx264, 1=h264_nvenc, 2=h264_vaapi, 3=h264_amf
    switch (s.codecIdx) {
        case 1:
            args.insert(args.end(), {
                "-c:v", "h264_nvenc",
                "-rc", "vbr",
                "-cq", std::to_string(s.crf),
                "-preset", "slow",
                "-pix_fmt", "yuv420p"
            });
            break;
        case 2:
            if (useVaapi) {
                args.insert(args.end(), {
                    "-c:v", "h264_vaapi",
                    "-qp", std::to_string(s.crf)
                });
            } else {
                args.insert(args.end(), {
                    "-c:v", "libx264",
                    "-crf", std::to_string(s.crf),
                    "-preset", "veryslow",
                    "-pix_fmt", "yuv420p"
                });
            }
            break;
        case 3:
            args.insert(args.end(), {
                "-c:v", "h264_amf",
                "-rc", "cqp",
                "-qp_i", std::to_string(s.crf),
                "-qp_p", std::to_string(s.crf),
                "-quality", "quality",
                "-pix_fmt", "yuv420p"
            });
            break;
        default:
            args.insert(args.end(), {
                "-c:v", "libx264",
                "-crf", std::to_string(s.crf),
                "-preset", "veryslow",
                "-pix_fmt", "yuv420p"
            });
            break;
    }

    const std::string inputExt = lowercaseExtension(inputAudioPath);
    const std::string outputExt = lowercaseExtension(s.outputPath);
    const bool mp4Output = (outputExt == ".mp4" || outputExt == ".m4v" || outputExt == ".mov");
    if (!mp4Output || isMp4CopyableAudioExtension(inputExt)) {
        args.insert(args.end(), {"-c:a", "copy"});
    } else {
        // Re-encode WAV/PCM/FLAC/OGG sources to high-bitrate AAC for MP4 compatibility.
        args.insert(args.end(), {"-c:a", "aac", "-b:a", "320k"});
    }

    args.insert(args.end(), {"-shortest", s.outputPath});
    return args;
}
