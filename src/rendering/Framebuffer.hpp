#pragma once

#include "GlResources.hpp"

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer() = default;

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&&) noexcept = default;
    Framebuffer& operator=(Framebuffer&&) noexcept = default;

    void resize(int width, int height);
    void bind() const;
    static void unbind();
    void reset() noexcept;

    [[nodiscard]] bool ready() const noexcept { return m_id.valid(); }
    [[nodiscard]] GLuint texture() const noexcept { return m_texture.get(); }
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }

private:
    gl::FramebufferHandle m_id;
    gl::Texture m_texture;
    int m_width = 0;
    int m_height = 0;
};
