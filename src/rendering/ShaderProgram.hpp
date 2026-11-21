#pragma once

#include <GL/glew.h>
#include <filesystem>
#include <string>

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    void load(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);
    bool reloadIfChanged(bool force = false, std::string* errorOut = nullptr);
    void reset() noexcept;

    [[nodiscard]] GLuint id() const noexcept { return m_id; }
    operator GLuint() const noexcept { return m_id; }

private:
    GLuint m_id = 0;
    std::filesystem::path m_vertexPath;
    std::filesystem::path m_fragmentPath;
    std::filesystem::file_time_type m_vertexMtime{};
    std::filesystem::file_time_type m_fragmentMtime{};
    bool m_hasTimestamps = false;
};
