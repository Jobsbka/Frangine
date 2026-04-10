// src/render/graphics_device.cpp
#include "graphics_device.hpp"
#include "texture.hpp"
#include "mesh.hpp"
#include "shader.hpp"
#include "material.hpp"
#include "render_target.hpp"
#include <glad/glad.h>
#include <stdexcept>
#include <iostream>

namespace arxglue::render {

GraphicsDevice& GraphicsDevice::instance() {
    static GraphicsDevice device;
    return device;
}

bool GraphicsDevice::initialize(GLFWwindow* window) {
    if (m_initialized) return true;
    if (!window) return false;
    m_window = window;
    
    glfwMakeContextCurrent(m_window);
    if (glfwGetCurrentContext() != m_window) {
        std::cerr << "[GraphicsDevice] Failed to make context current" << std::endl;
        return false;
    }

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // Очистка возможных ошибок после инициализации
    while (glGetError() != GL_NO_ERROR) {}

    std::cout << "[GraphicsDevice] OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "[GraphicsDevice] GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_initialized = true;
    return true;
}

void GraphicsDevice::shutdown() {
    m_window = nullptr;
    m_initialized = false;
}

void GraphicsDevice::makeCurrent() {
    if (m_window) {
        glfwMakeContextCurrent(m_window);
        GLFWwindow* cur = glfwGetCurrentContext();
        if (cur != m_window) {
            std::cerr << "[GraphicsDevice] makeCurrent failed: current=" << cur << " expected=" << m_window << std::endl;
        }
    }
}

void GraphicsDevice::clear(float r, float g, float b, float a) {
    static int count = 0;
    if (count++ % 60 == 0) {
        std::cout << "[GraphicsDevice] clear color (" << r << "," << g << "," << b << ")" << std::endl;
    }
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GraphicsDevice::swapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

void GraphicsDevice::setViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

std::shared_ptr<Texture> GraphicsDevice::createTexture(int width, int height, int channels, const void* data) {
    makeCurrent();
    return std::make_shared<Texture>(width, height, channels, data);
}

std::shared_ptr<Texture> GraphicsDevice::createRenderTargetTexture(int width, int height) {
    makeCurrent();
    return std::make_shared<Texture>(width, height, 4, nullptr, true);
}

std::shared_ptr<Mesh> GraphicsDevice::createMesh(const std::vector<float>& vertices,
                                                 const std::vector<uint32_t>& indices,
                                                 const std::vector<int>& attribSizes) {
    makeCurrent();
    return std::make_shared<Mesh>(vertices, indices, attribSizes);
}

std::shared_ptr<Shader> GraphicsDevice::createShader(const std::string& vertexSrc,
                                                     const std::string& fragmentSrc) {
    makeCurrent();
    GLFWwindow* cur = glfwGetCurrentContext();
    std::cout << "[GraphicsDevice] Creating shader... current context: " << cur << " (expected " << m_window << ")" << std::endl;
    if (cur != m_window) {
        std::cerr << "[GraphicsDevice] Context mismatch! Re-setting..." << std::endl;
        glfwMakeContextCurrent(m_window);
    }
    auto shader = std::make_shared<Shader>(vertexSrc, fragmentSrc);
    std::cout << "[GraphicsDevice] Shader created successfully." << std::endl;
    return shader;
}

std::shared_ptr<Material> GraphicsDevice::createMaterial(std::shared_ptr<Shader> shader) {
    return std::make_shared<Material>(shader);
}

std::shared_ptr<RenderTarget> GraphicsDevice::createRenderTarget(int width, int height) {
    makeCurrent();
    return std::make_shared<RenderTarget>(width, height);
}

} // namespace arxglue::render