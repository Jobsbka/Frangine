// src/render/graphics_device.cpp
#include "graphics_device.hpp"
#include "texture.hpp"
#include "mesh.hpp"
#include "shader.hpp"
#include "material.hpp"
#include "render_target.hpp"
#include <glad/glad.h>
#include <stdexcept>

namespace arxglue::render {

GraphicsDevice& GraphicsDevice::instance() {
    static GraphicsDevice device;
    return device;
}

bool GraphicsDevice::initialize(GLFWwindow* window) {
    if (m_initialized) return true;
    if (!window) return false;
    m_window = window;
    makeCurrent();

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Failed to initialize GLAD");
    }

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
    }
}

void GraphicsDevice::clear(float r, float g, float b, float a) {
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
    return std::make_shared<Texture>(width, height, channels, data);
}

std::shared_ptr<Texture> GraphicsDevice::createRenderTargetTexture(int width, int height) {
    return std::make_shared<Texture>(width, height, 4, nullptr, true);
}

std::shared_ptr<Mesh> GraphicsDevice::createMesh(const std::vector<float>& vertices,
                                                 const std::vector<uint32_t>& indices,
                                                 const std::vector<int>& attribSizes) {
    return std::make_shared<Mesh>(vertices, indices, attribSizes);
}

std::shared_ptr<Shader> GraphicsDevice::createShader(const std::string& vertexSrc,
                                                     const std::string& fragmentSrc) {
    return std::make_shared<Shader>(vertexSrc, fragmentSrc);
}

std::shared_ptr<Material> GraphicsDevice::createMaterial(std::shared_ptr<Shader> shader) {
    return std::make_shared<Material>(shader);
}

std::shared_ptr<RenderTarget> GraphicsDevice::createRenderTarget(int width, int height) {
    return std::make_shared<RenderTarget>(width, height);
}

} // namespace arxglue::render