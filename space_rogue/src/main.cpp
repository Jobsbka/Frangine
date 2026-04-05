#include <GLFW/glfw3.h>
#include "render/vulkan_context.hpp"
#include <iostream>

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Space Rogue", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    VulkanContext vk;
    if (!vk.init(window)) {
        std::cerr << "Failed to initialize Vulkan" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        vk.drawFrame();
    }
    
    vk.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}