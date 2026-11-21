#pragma once

#include "GlResources.hpp"
#include <algorithm>
#include <cstddef>
#include <vector>

class StreamingVertexBuffer {
public:
    StreamingVertexBuffer() = default;
    ~StreamingVertexBuffer() = default;

    StreamingVertexBuffer(const StreamingVertexBuffer&) = delete;
    StreamingVertexBuffer& operator=(const StreamingVertexBuffer&) = delete;
    StreamingVertexBuffer(StreamingVertexBuffer&&) noexcept = default;
    StreamingVertexBuffer& operator=(StreamingVertexBuffer&&) noexcept = default;

    void initialize(size_t floatsPerVertex, size_t initialBytes) {
        m_floatsPerVertex = floatsPerVertex;
        m_capacityBytes = initialBytes;
        m_offsetBytes = 0;
        m_vao = gl::VertexArray::create();
        m_vbo = gl::Buffer::create();
        glBindVertexArray(m_vao.get());
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo.get());
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_capacityBytes), nullptr, GL_STREAM_DRAW);
    }

    // Call immediately after initialize() to attach attribute layout, then finishLayout().
    void finishLayout() const noexcept {
        glBindVertexArray(0);
    }

    GLint upload(const std::vector<float>& vertices) {
        const size_t vertexStride = m_floatsPerVertex * sizeof(float);
        const size_t bytes = vertices.size() * sizeof(float);
        glBindVertexArray(m_vao.get());
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo.get());
        if (m_offsetBytes + bytes > m_capacityBytes) {
            if (bytes > m_capacityBytes) {
                m_capacityBytes = std::max(bytes, std::max<size_t>(m_capacityBytes * 2, 64 * 1024));
            }
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_capacityBytes), nullptr, GL_STREAM_DRAW);
            m_offsetBytes = 0;
        }
        glBufferSubData(
            GL_ARRAY_BUFFER,
            static_cast<GLintptr>(m_offsetBytes),
            static_cast<GLsizeiptr>(bytes),
            vertices.data());
        const GLint firstVertex = static_cast<GLint>(m_offsetBytes / vertexStride);
        m_offsetBytes += bytes;
        return firstVertex;
    }

    [[nodiscard]] GLuint vao() const noexcept { return m_vao.get(); }

private:
    gl::VertexArray m_vao;
    gl::Buffer m_vbo;
    size_t m_floatsPerVertex = 1;
    size_t m_capacityBytes = 0;
    size_t m_offsetBytes = 0;
};
