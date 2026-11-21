#pragma once

#include <filesystem>
#include <string_view>

namespace AssetPaths {
void setShaderDirectoryOverride(const std::filesystem::path& dir);
std::filesystem::path shaderDirectory();
std::filesystem::path shader(std::string_view fileName);
std::filesystem::path defaultFont();
std::filesystem::path presetsDirectory();
}
