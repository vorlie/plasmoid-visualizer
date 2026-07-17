#pragma once

#include <GL/glew.h>
#include <filesystem>

class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    bool loadRgba(const std::filesystem::path& path, bool flipVertically);
    void reset() noexcept;

    [[nodiscard]] GLuint id() const noexcept { return m_id; }
    [[nodiscard]] bool loaded() const noexcept { return m_id != 0; }
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }

private:
    GLuint m_id = 0;
    int m_width = 0;
    int m_height = 0;
};
