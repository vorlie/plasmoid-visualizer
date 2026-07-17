#pragma once

#include <GL/glew.h>

class Framebuffer {
public:
    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void resize(int width, int height);
    void bind() const;
    static void unbind();
    void reset() noexcept;

    [[nodiscard]] bool ready() const noexcept { return m_id != 0; }
    [[nodiscard]] GLuint texture() const noexcept { return m_texture; }
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }

private:
    GLuint m_id = 0;
    GLuint m_texture = 0;
    int m_width = 0;
    int m_height = 0;
};
