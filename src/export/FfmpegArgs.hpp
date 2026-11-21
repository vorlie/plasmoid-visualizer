#pragma once

#include "PersistentConfig.hpp"
#include <string>
#include <vector>

namespace FfmpegArgs {
std::vector<std::string> buildExportArgs(
    const VideoRenderSettings& settings,
    const std::string& inputAudioPath,
    bool allowVaapi =
#ifdef _WIN32
        false
#else
        true
#endif
);
} // namespace FfmpegArgs
