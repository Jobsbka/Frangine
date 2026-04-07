#include "shader.hpp"
#include <stdexcept>
#include <vector>
#include <iostream>

namespace arxglue::render {

Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength);
        glGetProgramInfoLog(m_program, logLength, nullptr, log.data());
        throw std::runtime_error("Shader linking failed: " + std::string(log.data()));
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader() {
    if (m_program) {
        glDeleteProgram(m_program);
    }
}

Shader::Shader(Shader&& other) noexcept : m_program(other.m_program) {
    other.m_program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_program) glDeleteProgram(m_program);
        m_program = other.m_program;
        other.m_program = 0;
    }
    return *this;
}

void Shader::bind() const {
    glUseProgram(m_program);
}

void Shader::unbind() const {
    glUseProgram(0);
}

GLuint Shader::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(logLength);
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        throw std::runtime_error("Shader compilation failed: " + std::string(log.data()));
    }
    return shader;
}

GLint Shader::getUniformLocation(const std::string& name) const {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) {
        return it->second;
    }
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    m_uniformCache[name] = loc;
    return loc;
}

void Shader::setUniform1i(const std::string& name, int value) {
    GLint loc = getUniformLocation(name);
    if (loc != -1) glUniform1i(loc, value);
}

void Shader::setUniform1f(const std::string& name, float value) {
    GLint loc = getUniformLocation(name);
    if (loc != -1) glUniform1f(loc, value);
}

void Shader::setUniformMat4(const std::string& name, const float* mat4) {
    GLint loc = getUniformLocation(name);
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, mat4);
}

void Shader::setUniform3fv(const std::string& name, const float* vec3) {
    GLint loc = getUniformLocation(name);
    if (loc != -1) glUniform3fv(loc, 1, vec3);
}

} // namespace arxglue::render