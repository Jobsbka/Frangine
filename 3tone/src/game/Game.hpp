#pragma once
#include <string>
#include <memory>
#include <chrono>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace arxglue::game {

class Game {
public:
    Game(const std::string& title, int width = 800, int height = 600);
    virtual ~Game();

    // Запуск главного цикла
    int run();

protected:
    GLFWwindow* m_window;
    int m_width, m_height;
    std::chrono::high_resolution_clock::time_point m_lastTime;
    float m_deltaTime = 0.0f;

    // Виртуальные методы для переопределения
    virtual void onInit() {}
    virtual void onUpdate(float deltaTime) {}
    virtual void onRender() {}
    virtual void onShutdown() {}
    virtual void onKey(int key, int scancode, int action, int mods) {}
    virtual void onMouseButton(int button, int action, int mods) {}
    virtual void onMouseMove(double xpos, double ypos) {}
    virtual void onResize(int width, int height) {}

private:
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
};

} // namespace arxglue::game