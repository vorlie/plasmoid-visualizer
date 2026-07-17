#pragma once

#include <filesystem>
#include <string_view>

namespace AssetPaths {
std::filesystem::path shader(std::string_view fileName);
std::filesystem::path defaultFont();
}
