#include "Framebuffer.hpp"

#include <iostream>

void Framebuffer::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (m_width == width && m_height == height && ready()) return;

    reset();
    m_width = width;
    m_height = height;

    m_id = gl::FramebufferHandle::create();
    glBindFramebuffer(GL_FRAMEBUFFER, m_id.get());

    m_texture = gl::Texture::create();
    glBindTexture(GL_TEXTURE_2D, m_texture.get());
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
        GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture.get(), 0);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer is not complete (status=0x"
                  << std::hex << status << std::dec << ")" << std::endl;
        unbind();
        reset();
        return;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    unbind();
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_id.get());
}

void Framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::reset() noexcept {
    m_id.reset();
    m_texture.reset();
    m_width = 0;
    m_height = 0;
}
