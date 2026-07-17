#include "Texture2D.hpp"

#include "stb_image.h"
#include <iostream>

Texture2D::~Texture2D() {
    reset();
}

bool Texture2D::loadRgba(const std::filesystem::path& path, bool flipVertically) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(flipVertically);
    unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load image: " << path.string() << std::endl;
        return false;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);

    reset();
    m_id = texture;
    m_width = width;
    m_height = height;
    return true;
}

void Texture2D::reset() noexcept {
    if (m_id != 0) glDeleteTextures(1, &m_id);
    m_id = 0;
    m_width = 0;
    m_height = 0;
}
