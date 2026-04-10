// src/render/shader.cpp
#include "shader.hpp"
#include "graphics_device.hpp"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <sstream>
#include <GLFW/glfw3.h>

namespace arxglue::render {

Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    std::cout << "[Shader] Constructor called" << std::endl;
    std::string errorLog;

    std::cout << "[Shader] Compiling vertex shader..." << std::endl;
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSrc, errorLog);
    if (vs == 0) {
        throw std::runtime_error("Vertex shader compilation failed:\n" + errorLog);
    }
    std::cout << "[Shader] Vertex shader compiled (id=" << vs << ")" << std::endl;

    std::cout << "[Shader] Compiling fragment shader..." << std::endl;
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSrc, errorLog);
    if (fs == 0) {
        glDeleteShader(vs);
        throw std::runtime_error("Fragment shader compilation failed:\n" + errorLog);
    }
    std::cout << "[Shader] Fragment shader compiled (id=" << fs << ")" << std::endl;

    std::cout << "[Shader] Creating program..." << std::endl;
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
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(m_program);
        m_program = 0;
        throw std::runtime_error("Shader linking failed: " + std::string(log.data()));
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    std::cout << "[Shader] Program created successfully (id=" << m_program << ")" << std::endl;
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

GLuint Shader::compileShader(GLenum type, const std::string& source, std::string& outError) {
    std::cout << "[Shader] compileShader called for " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << std::endl;

    // Принудительно делаем контекст текущим
    auto& device = GraphicsDevice::instance();
    GLFWwindow* win = device.getWindow();
    if (win) {
        glfwMakeContextCurrent(win);
    } else {
        outError = "GraphicsDevice has no window";
        std::cerr << outError << std::endl;
        return 0;
    }

    // Очищаем предыдущие ошибки (однократно)
    while (glGetError() != GL_NO_ERROR) {}

    GLFWwindow* currentCtx = glfwGetCurrentContext();
    std::cout << "[Shader] Current GLFW context: " << currentCtx << " (expected " << win << ")" << std::endl;

    GLuint shader = glCreateShader(type);
    GLenum err = glGetError();
    if (shader == 0) {
        outError = "glCreateShader returned 0, OpenGL error: " + std::to_string(err);
        std::cerr << outError << std::endl;
        return 0;
    }
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
        outError = std::string(log.data());
        std::cerr << "[Shader] Compile error in "
                  << (type == GL_VERTEX_SHADER ? "vertex" : "fragment")
                  << " shader:\n" << outError << std::endl;
        glDeleteShader(shader);
        return 0;
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

void Shader::setUniform4fv(const std::string& name, const float* vec4) {
    GLint loc = getUniformLocation(name);
    if (loc != -1) glUniform4fv(loc, 1, vec4);
}

} // namespace arxglue::render