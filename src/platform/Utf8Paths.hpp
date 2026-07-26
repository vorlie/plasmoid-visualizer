#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Utf8Paths {
std::filesystem::path fromUtf8(std::string_view utf8);
std::string toUtf8(const std::filesystem::path& path);

#ifdef _WIN32
std::wstring toWide(std::string_view utf8);
#endif
}
