#include "AssetPaths.hpp"

#include <cstdlib>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace {
std::filesystem::path& shaderOverrideStorage() {
    static std::filesystem::path overrideDir;
    return overrideDir;
}

std::filesystem::path executableDirectory() {
    static const std::filesystem::path cached = []() -> std::filesystem::path {
#ifdef _WIN32
        std::vector<wchar_t> buf(MAX_PATH, L'\0');
        for (;;) {
            const DWORD written = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
            if (written == 0) return {};
            if (written < buf.size() - 1) {
                return std::filesystem::path(buf.data(), buf.data() + written).parent_path();
            }
            if (buf.size() >= 32768) return {};
            buf.resize(buf.size() * 2, L'\0');
        }
#else
        std::vector<char> buf(4096, '\0');
        const ssize_t len = ::readlink("/proc/self/exe", buf.data(), buf.size() - 1);
        if (len > 0) {
            return std::filesystem::path(std::string(buf.data(), static_cast<size_t>(len))).parent_path();
        }
        return {};
#endif
    }();
    return cached;
}

std::filesystem::path bundled(const std::filesystem::path& relativePath) {
    const auto exeDir = executableDirectory();
    if (!exeDir.empty()) {
        const auto exeRelative = exeDir / "assets" / relativePath;
        std::error_code ec;
        if (std::filesystem::exists(exeRelative, ec) && !ec) {
            return exeRelative;
        }
    }
    return std::filesystem::path("assets") / relativePath;
}
} // namespace

void AssetPaths::setShaderDirectoryOverride(const std::filesystem::path& dir) {
    shaderOverrideStorage() = dir;
}

std::filesystem::path AssetPaths::shaderDirectory() {
    const auto& overrideDir = shaderOverrideStorage();
    if (!overrideDir.empty()) {
        return overrideDir;
    }
    return bundled("shaders");
}

std::filesystem::path AssetPaths::shader(std::string_view fileName) {
    return shaderDirectory() / fileName;
}

std::filesystem::path AssetPaths::presetsDirectory() {
    return bundled("presets");
}

std::filesystem::path AssetPaths::defaultFont() {
    std::error_code ec;
    const auto bundledFont = bundled("fonts/AdwaitaMono-Regular.ttf");
    if (std::filesystem::exists(bundledFont, ec) && !ec) {
        return bundledFont;
    }

#ifdef _WIN32
    if (const wchar_t* windowsDirectory = _wgetenv(L"WINDIR")) {
        const auto fonts = std::filesystem::path(windowsDirectory) / L"Fonts";
        const wchar_t* candidates[] = {L"YuGothM.ttc", L"meiryo.ttc", L"msgothic.ttc", L"consola.ttf"};
        for (const wchar_t* candidate : candidates) {
            const auto path = fonts / candidate;
            ec.clear();
            if (std::filesystem::exists(path, ec) && !ec) return path;
        }
    }
#else
    const std::filesystem::path linuxFonts[] = {
        "/usr/share/fonts/Adwaita/AdwaitaMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
    };
    for (const auto& font : linuxFonts) {
        ec.clear();
        if (std::filesystem::exists(font, ec) && !ec) {
            return font;
        }
    }
#endif

    return {};
}
