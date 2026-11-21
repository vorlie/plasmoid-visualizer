#pragma once

#include <GL/glew.h>
#include <utility>

namespace gl {

struct BufferOps {
    static GLuint create() noexcept {
        GLuint id = 0;
        glGenBuffers(1, &id);
        return id;
    }
    static void destroy(GLuint id) noexcept {
        if (id != 0) glDeleteBuffers(1, &id);
    }
};

struct VertexArrayOps {
    static GLuint create() noexcept {
        GLuint id = 0;
        glGenVertexArrays(1, &id);
        return id;
    }
    static void destroy(GLuint id) noexcept {
        if (id != 0) glDeleteVertexArrays(1, &id);
    }
};

struct TextureOps {
    static GLuint create() noexcept {
        GLuint id = 0;
        glGenTextures(1, &id);
        return id;
    }
    static void destroy(GLuint id) noexcept {
        if (id != 0) glDeleteTextures(1, &id);
    }
};

struct FramebufferOps {
    static GLuint create() noexcept {
        GLuint id = 0;
        glGenFramebuffers(1, &id);
        return id;
    }
    static void destroy(GLuint id) noexcept {
        if (id != 0) glDeleteFramebuffers(1, &id);
    }
};

struct RenderbufferOps {
    static GLuint create() noexcept {
        GLuint id = 0;
        glGenRenderbuffers(1, &id);
        return id;
    }
    static void destroy(GLuint id) noexcept {
        if (id != 0) glDeleteRenderbuffers(1, &id);
    }
};

template <typename Ops>
class Handle {
public:
    Handle() noexcept = default;
    explicit Handle(GLuint id) noexcept : m_id(id) {}
    ~Handle() noexcept { reset(); }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept : m_id(std::exchange(other.m_id, 0)) {}
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset();
            m_id = std::exchange(other.m_id, 0);
        }
        return *this;
    }

    static Handle create() noexcept {
        return Handle(Ops::create());
    }

    GLuint ensureCreated() noexcept {
        if (m_id == 0) {
            m_id = Ops::create();
        }
        return m_id;
    }

    void reset(GLuint next = 0) noexcept {
        if (m_id != 0) {
            Ops::destroy(m_id);
        }
        m_id = next;
    }

    [[nodiscard]] GLuint get() const noexcept { return m_id; }
    [[nodiscard]] bool valid() const noexcept { return m_id != 0; }
    explicit operator bool() const noexcept { return m_id != 0; }

private:
    GLuint m_id = 0;
};

using Buffer = Handle<BufferOps>;
using VertexArray = Handle<VertexArrayOps>;
using Texture = Handle<TextureOps>;
using FramebufferHandle = Handle<FramebufferOps>;
using Renderbuffer = Handle<RenderbufferOps>;

} // namespace gl
