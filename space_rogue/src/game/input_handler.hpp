#pragma once
#include "../math/math.hpp"

// Состояние ввода (клавиши)
struct InputState {
    bool forward = false;   // W / Up
    bool backward = false;  // S / Down
    bool left = false;      // A / Left
    bool right = false;     // D / Right
    bool fire = false;      // Space / Ctrl
    bool up = false;        // Q / PageUp
    bool down = false;      // E / PageDown
};

// Класс для обработки ввода GLFW
class InputHandler {
public:
    InputHandler();
    
    // Вызывать из GLFW callbacks
    void setKey(int key, bool pressed);
    
    // Получить текущее состояние ввода
    const InputState& getState() const { return state; }
    
    // Проверить, была ли клавиша нажата в этом кадре (edge trigger)
    bool isKeyPressed(int key) const;
    
    // Сбросить состояние "нажат в этом кадре"
    void resetKeyPresses();
    
private:
    InputState state;
    // Флаги для edge detection (нажата ли клавиша в текущем кадре)
    bool keyPresses[512];  // простой массив для всех возможных ключей
};
