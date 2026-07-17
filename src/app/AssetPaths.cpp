#include "AssetPaths.hpp"

#include <cstdlib>

namespace {
std::filesystem::path bundled(const std::filesystem::path& relativePath) {
    return std::filesystem::path("assets") / relativePath;
}
}

std::filesystem::path AssetPaths::shader(std::string_view fileName) {
    return bundled(std::filesystem::path("shaders") / fileName);
}

std::filesystem::path AssetPaths::defaultFont() {
    const auto bundledFont = bundled("fonts/AdwaitaMono-Regular.ttf");
    if (std::filesystem::exists(bundledFont)) {
        return bundledFont;
    }

#ifdef _WIN32
    if (const char* windowsDirectory = std::getenv("WINDIR")) {
        const auto consola = std::filesystem::path(windowsDirectory) / "Fonts" / "consola.ttf";
        if (std::filesystem::exists(consola)) {
            return consola;
        }
    }
#else
    const std::filesystem::path linuxFonts[] = {
        "/usr/share/fonts/Adwaita/AdwaitaMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
    };
    for (const auto& font : linuxFonts) {
        if (std::filesystem::exists(font)) {
            return font;
        }
    }
#endif

    return {};
}
