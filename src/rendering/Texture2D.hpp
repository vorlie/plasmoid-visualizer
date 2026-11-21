#pragma once

#include "GlResources.hpp"
#include <filesystem>

class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D() = default;

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&&) noexcept = default;
    Texture2D& operator=(Texture2D&&) noexcept = default;

    bool loadRgba(const std::filesystem::path& path, bool flipVertically);
    void reset() noexcept;

    [[nodiscard]] GLuint id() const noexcept { return m_id.get(); }
    [[nodiscard]] bool loaded() const noexcept { return m_id.valid(); }
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }

private:
    gl::Texture m_id;
    int m_width = 0;
    int m_height = 0;
};
