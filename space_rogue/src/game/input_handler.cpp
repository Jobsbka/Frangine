#include "input_handler.hpp"
#include <cstring>  // для memset

InputHandler::InputHandler() {
    std::memset(keyPresses, 0, sizeof(keyPresses));
}

void InputHandler::setKey(int key, bool pressed) {
    // Запоминаем нажатие для edge detection
    if (pressed && !keyPresses[key]) {
        keyPresses[key] = true;
    }
    
    // Обновляем состояние
    switch (key) {
        case 87:    // W
        case 265:   // GLFW_KEY_UP
            state.forward = pressed;
            break;
        case 83:    // S
        case 264:   // GLFW_KEY_DOWN
            state.backward = pressed;
            break;
        case 65:    // A
        case 263:   // GLFW_KEY_LEFT
            state.left = pressed;
            break;
        case 68:    // D
        case 262:   // GLFW_KEY_RIGHT
            state.right = pressed;
            break;
        case 32:    // Space
        case 341:   // Left Ctrl
            state.fire = pressed;
            break;
        case 81:    // Q
        case 266:   // Page Up
            state.up = pressed;
            break;
        case 69:    // E
        case 267:   // Page Down
            state.down = pressed;
            break;
    }
}

bool InputHandler::isKeyPressed(int key) const {
    return keyPresses[key];
}

void InputHandler::resetKeyPresses() {
    std::memset(keyPresses, 0, sizeof(keyPresses));
}
