#include "input_manager.hpp"
#include "../core/context.hpp"
#include <iostream>

namespace arxglue::ui {

InputManager& InputManager::instance() {
    static InputManager instance;
    return instance;
}

void InputManager::update(GLFWwindow* window) {
    static const int monitoredKeys[] = {
        GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D,
        GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT,
        GLFW_KEY_SPACE, GLFW_KEY_ENTER, GLFW_KEY_ESCAPE
    };
    for (int key : monitoredKeys) {
        m_keys[key] = (glfwGetKey(window, key) == GLFW_PRESS);
    }

    m_mouseButtons[GLFW_MOUSE_BUTTON_LEFT]  = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    m_mouseButtons[GLFW_MOUSE_BUTTON_RIGHT] = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

    glfwGetCursorPos(window, &m_mouseX, &m_mouseY);

    static int frameCounter = 0;
    if (frameCounter++ % 60 == 0) {
        std::cout << "[InputManager] Mouse: (" << m_mouseX << ", " << m_mouseY << ") Left: " 
                  << (m_mouseButtons[GLFW_MOUSE_BUTTON_LEFT] ? "down" : "up") << std::endl;
    }
}

void InputManager::writeToContext(arxglue::Context& ctx) const {
    ctx.setState("input.mouseX", static_cast<float>(m_mouseX));
    ctx.setState("input.mouseY", static_cast<float>(m_mouseY));
    ctx.setState("input.mouseLeft", m_mouseButtons.count(GLFW_MOUSE_BUTTON_LEFT) ? m_mouseButtons.at(GLFW_MOUSE_BUTTON_LEFT) : false);
    ctx.setState("input.mouseRight", m_mouseButtons.count(GLFW_MOUSE_BUTTON_RIGHT) ? m_mouseButtons.at(GLFW_MOUSE_BUTTON_RIGHT) : false);

    ctx.setState("input.keyW", m_keys.count(GLFW_KEY_W) ? m_keys.at(GLFW_KEY_W) : false);
    ctx.setState("input.keyA", m_keys.count(GLFW_KEY_A) ? m_keys.at(GLFW_KEY_A) : false);
    ctx.setState("input.keyS", m_keys.count(GLFW_KEY_S) ? m_keys.at(GLFW_KEY_S) : false);
    ctx.setState("input.keyD", m_keys.count(GLFW_KEY_D) ? m_keys.at(GLFW_KEY_D) : false);
    ctx.setState("input.keyUp", m_keys.count(GLFW_KEY_UP) ? m_keys.at(GLFW_KEY_UP) : false);
    ctx.setState("input.keyDown", m_keys.count(GLFW_KEY_DOWN) ? m_keys.at(GLFW_KEY_DOWN) : false);
    ctx.setState("input.keyLeft", m_keys.count(GLFW_KEY_LEFT) ? m_keys.at(GLFW_KEY_LEFT) : false);
    ctx.setState("input.keyRight", m_keys.count(GLFW_KEY_RIGHT) ? m_keys.at(GLFW_KEY_RIGHT) : false);
    ctx.setState("input.keySpace", m_keys.count(GLFW_KEY_SPACE) ? m_keys.at(GLFW_KEY_SPACE) : false);
    ctx.setState("input.keyEnter", m_keys.count(GLFW_KEY_ENTER) ? m_keys.at(GLFW_KEY_ENTER) : false);
    ctx.setState("input.keyEscape", m_keys.count(GLFW_KEY_ESCAPE) ? m_keys.at(GLFW_KEY_ESCAPE) : false);
}

bool InputManager::isKeyPressed(int key) const {
    auto it = m_keys.find(key);
    return it != m_keys.end() && it->second;
}

bool InputManager::isMouseButtonPressed(int button) const {
    auto it = m_mouseButtons.find(button);
    return it != m_mouseButtons.end() && it->second;
}

std::array<float, 2> InputManager::getMousePosition() const {
    return { static_cast<float>(m_mouseX), static_cast<float>(m_mouseY) };
}

} // namespace arxglue::ui