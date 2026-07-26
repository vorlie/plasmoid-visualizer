#include "Utf8Paths.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

int main() {
    const std::string japanese =
        "D:/Media/Music/TUYU ("
        "\xE3\x83\x84\xE3\x83\xA6"
        ")/01. "
        "\xE3\x81\x8F\xE3\x82\x89\xE3\x81\xB9\xE3\x82\x89\xE3\x82\x8C"
        "\xE3\x81\xA3\xE5\xAD\x90"
        ".flac";
    const auto native = Utf8Paths::fromUtf8(japanese);
    if (Utf8Paths::toUtf8(native) != japanese) {
        std::cerr << "UTF-8 filesystem round trip failed\n";
        return EXIT_FAILURE;
    }
#ifdef _WIN32
    if (Utf8Paths::toWide(japanese).empty()) {
        std::cerr << "UTF-8 to UTF-16 conversion failed\n";
        return EXIT_FAILURE;
    }
#endif
    return EXIT_SUCCESS;
}
