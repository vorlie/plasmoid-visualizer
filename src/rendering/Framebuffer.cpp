#include "Framebuffer.hpp"

#include <iostream>

Framebuffer::~Framebuffer() {
    reset();
}

void Framebuffer::resize(int width, int height) {
    if (m_width == width && m_height == height && ready()) return;

    reset();
    m_width = width;
    m_height = height;

    glGenFramebuffers(1, &m_id);
    glBindFramebuffer(GL_FRAMEBUFFER, m_id);

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
        GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer is not complete" << std::endl;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    unbind();
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_id);
}

void Framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::reset() noexcept {
    if (m_id != 0) glDeleteFramebuffers(1, &m_id);
    if (m_texture != 0) glDeleteTextures(1, &m_texture);
    m_id = 0;
    m_texture = 0;
    m_width = 0;
    m_height = 0;
}
