#pragma once

#include <GL/glew.h>
#include <filesystem>

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    void load(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
    void reset() noexcept;

    [[nodiscard]] GLuint id() const noexcept { return m_id; }
    operator GLuint() const noexcept { return m_id; }

private:
    GLuint m_id = 0;
};
