#pragma once

#include "GlResources.hpp"

class FullscreenQuad {
public:
    FullscreenQuad() = default;
    ~FullscreenQuad() = default;

    FullscreenQuad(const FullscreenQuad&) = delete;
    FullscreenQuad& operator=(const FullscreenQuad&) = delete;
    FullscreenQuad(FullscreenQuad&&) noexcept = default;
    FullscreenQuad& operator=(FullscreenQuad&&) noexcept = default;

    void initialize() {
        if (m_vao.valid()) return;
        constexpr float kVertices[] = {
            -1.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f
        };
        m_vao = gl::VertexArray::create();
        m_vbo = gl::Buffer::create();
        glBindVertexArray(m_vao.get());
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo.get());
        glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
            reinterpret_cast<void*>(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    void bind() const noexcept {
        glBindVertexArray(m_vao.get());
    }

    void draw() const noexcept {
        glBindVertexArray(m_vao.get());
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    [[nodiscard]] bool ready() const noexcept { return m_vao.valid(); }

private:
    gl::VertexArray m_vao;
    gl::Buffer m_vbo;
};
