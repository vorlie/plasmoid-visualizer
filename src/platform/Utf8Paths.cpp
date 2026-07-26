#include "Utf8Paths.hpp"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Utf8Paths {

std::filesystem::path fromUtf8(std::string_view utf8) {
    return std::filesystem::u8path(utf8.begin(), utf8.end());
}

std::string toUtf8(const std::filesystem::path& path) {
    return path.u8string();
}

#ifdef _WIN32
std::wstring toWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()),
        nullptr, 0);
    if (required <= 0) return {};
    std::wstring result(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()),
        result.data(), required);
    return result;
}
#endif

}
