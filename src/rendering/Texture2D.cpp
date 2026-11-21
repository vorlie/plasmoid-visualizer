#include "Texture2D.hpp"
#include "Utf8Paths.hpp"

#include "stb_image.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

bool Texture2D::loadRgba(const std::filesystem::path& path, bool flipVertically) {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::ifstream input(path, std::ios::binary);
    const std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    stbi_set_flip_vertically_on_load(flipVertically);
    unsigned char* data = bytes.empty()
        ? nullptr
        : stbi_load_from_memory(
            bytes.data(), static_cast<int>(bytes.size()),
            &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load image: " << Utf8Paths::toUtf8(path) << std::endl;
        return false;
    }

    auto texture = gl::Texture::create();
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);

    m_id = std::move(texture);
    m_width = width;
    m_height = height;
    return true;
}

void Texture2D::reset() noexcept {
    m_id.reset();
    m_width = 0;
    m_height = 0;
}
