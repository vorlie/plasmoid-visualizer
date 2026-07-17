#include "ShaderProgram.hpp"

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
        throw std::runtime_error("Failed to open shader file: " + path.string());
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
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Failed to compile " + path.string() + ":\n" + log);
    }

    return shader;
}
}

ShaderProgram::~ShaderProgram() {
    reset();
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : m_id(std::exchange(other.m_id, 0)) {}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        reset();
        m_id = std::exchange(other.m_id, 0);
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
            glGetProgramInfoLog(program, logLength, nullptr, log.data());
            throw std::runtime_error("Failed to link shader program:\n" + log);
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        reset();
        m_id = program;
    } catch (...) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        throw;
    }
}

void ShaderProgram::reset() noexcept {
    if (m_id != 0) {
        glDeleteProgram(m_id);
        m_id = 0;
    }
}
