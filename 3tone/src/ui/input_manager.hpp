// src/ui/input_manager.hpp
#pragma once

#include <GLFW/glfw3.h>
#include <array>
#include <unordered_map>

namespace arxglue {
    struct Context;
}

namespace arxglue::ui {

class InputManager {
public:
    static InputManager& instance();

    void update(GLFWwindow* window);
    void writeToContext(arxglue::Context& ctx) const;

    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;
    std::array<float, 2> getMousePosition() const;

private:
    InputManager() = default;
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    std::unordered_map<int, bool> m_keys;
    std::unordered_map<int, bool> m_mouseButtons;
    double m_mouseX = 0.0;
    double m_mouseY = 0.0;
};

} // namespace arxglue::ui