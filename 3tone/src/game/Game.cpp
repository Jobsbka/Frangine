#include "Game.hpp"
#include "../render/graphics_device.hpp"
#include <iostream>
namespace arxglue::game {

Game::Game(const std::string& title, int width, int height)
    : m_width(width), m_height(height)
{
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create window");
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);

    auto& device = render::GraphicsDevice::instance();
    device.initialize(m_window);
}

Game::~Game() {
    if (m_window) {
        render::GraphicsDevice::instance().shutdown();
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

int Game::run() {
    onInit();
    m_lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(m_window)) {
        auto now = std::chrono::high_resolution_clock::now();
        m_deltaTime = std::chrono::duration<float>(now - m_lastTime).count();
        m_lastTime = now;

        glfwPollEvents();

        onUpdate(m_deltaTime);
        onRender();

        render::GraphicsDevice::instance().swapBuffers();
    }

    onShutdown();
    return 0;
}

void Game::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* self = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (self) self->onKey(key, scancode, action, mods);
}

void Game::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* self = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (self) self->onMouseButton(button, action, mods);
}

void Game::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* self = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (self) self->onMouseMove(xpos, ypos);
}

void Game::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_width = width;
        self->m_height = height;
        self->onResize(width, height);
    }
}

} // namespace arxglue::game