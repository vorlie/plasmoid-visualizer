#include "ShaderProgram.hpp"
#include "Utf8Paths.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
std::string loadSource(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open shader file: " + Utf8Paths::toUtf8(path));
    }

    std::ostringstream source;
    source << file.rdbuf();
    return source.str();
}

GLuint compile(GLenum type, const std::string& source, const std::filesystem::path& path) {
    GLuint shader = glCreateShader(type);
    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<size_t>(std::max(logLength, 1)), '\0');
        GLsizei written = 0;
        glGetShaderInfoLog(shader, logLength, &written, log.data());
        log.resize(static_cast<size_t>(std::max(written, 0)));
        glDeleteShader(shader);
        throw std::runtime_error("Failed to compile " + Utf8Paths::toUtf8(path) + ":\n" + log);
    }

    return shader;
}
}

ShaderProgram::~ShaderProgram() {
    reset();
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : m_id(std::exchange(other.m_id, 0)),
      m_vertexPath(std::move(other.m_vertexPath)),
      m_fragmentPath(std::move(other.m_fragmentPath)),
      m_vertexMtime(other.m_vertexMtime),
      m_fragmentMtime(other.m_fragmentMtime),
      m_hasTimestamps(std::exchange(other.m_hasTimestamps, false)) {}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        reset();
        m_id = std::exchange(other.m_id, 0);
        m_vertexPath = std::move(other.m_vertexPath);
        m_fragmentPath = std::move(other.m_fragmentPath);
        m_vertexMtime = other.m_vertexMtime;
        m_fragmentMtime = other.m_fragmentMtime;
        m_hasTimestamps = std::exchange(other.m_hasTimestamps, false);
    }
    return *this;
}

void ShaderProgram::load(
    const std::filesystem::path& vertexPath,
    const std::filesystem::path& fragmentPath
) {
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint program = 0;

    try {
        vertexShader = compile(GL_VERTEX_SHADER, loadSource(vertexPath), vertexPath);
        fragmentShader = compile(GL_FRAGMENT_SHADER, loadSource(fragmentPath), fragmentPath);

        program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint success = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (success == GL_FALSE) {
            GLint logLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
            std::string log(static_cast<size_t>(std::max(logLength, 1)), '\0');
            GLsizei written = 0;
            glGetProgramInfoLog(program, logLength, &written, log.data());
            log.resize(static_cast<size_t>(std::max(written, 0)));
            throw std::runtime_error("Failed to link shader program:\n" + log);
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        reset();
        m_id = program;
        m_vertexPath = vertexPath;
        m_fragmentPath = fragmentPath;
        std::error_code vEc, fEc;
        const auto vTime = std::filesystem::last_write_time(vertexPath, vEc);
        const auto fTime = std::filesystem::last_write_time(fragmentPath, fEc);
        if (!vEc && !fEc) {
            m_vertexMtime = vTime;
            m_fragmentMtime = fTime;
            m_hasTimestamps = true;
        }
    } catch (...) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        throw;
    }
}

bool ShaderProgram::reloadIfChanged(bool force, std::string* errorOut) {
    if (m_vertexPath.empty() || m_fragmentPath.empty()) return false;

    std::error_code vEc, fEc;
    const auto vTime = std::filesystem::last_write_time(m_vertexPath, vEc);
    const auto fTime = std::filesystem::last_write_time(m_fragmentPath, fEc);
    if (vEc || fEc) {
        // Editor may be mid-save / atomic-renaming; try again on the next poll.
        return false;
    }
    if (!force && m_hasTimestamps && vTime == m_vertexMtime && fTime == m_fragmentMtime) {
        return false;
    }

    // Record observed timestamps before compiling so syntax errors aren't spammed every poll.
    m_vertexMtime = vTime;
    m_fragmentMtime = fTime;
    m_hasTimestamps = true;

    try {
        load(m_vertexPath, m_fragmentPath);
        return true;
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = e.what();
        return false;
    }
}

void ShaderProgram::reset() noexcept {
    if (m_id != 0) {
        glDeleteProgram(m_id);
        m_id = 0;
    }
}
