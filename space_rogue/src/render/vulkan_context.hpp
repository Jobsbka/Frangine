#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

// Forward declaration (чтобы не включать mesh.hpp в заголовок)
class Mesh;

struct UniformBufferObject {
    float model[16];
    float view[16];
    float proj[16];
};

class VulkanContext {
public:
    bool init(GLFWwindow* window);
    void cleanup();
    void drawFrame();

    // Доступ к device и command pool
    VkDevice getDevice() const { return device; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkCommandPool getCommandPool() const { return commandPool; }

    // Обновить командные буферы для отрисовки меша
    void updateCommandBuffers(const Mesh& mesh);
    
    // Создать uniform буфер и descriptor set
    void createUniformBuffer();
    void updateUniformBuffer(const UniformBufferObject& ubo);
    void createDescriptorPool();
    void createDescriptorSets(const Mesh& mesh);
    
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
    VkDescriptorSet getDescriptorSet() const { return descriptorSet; }

private:
    GLFWwindow* window = nullptr;

    VkInstance instance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    
    // Uniform buffer и descriptor sets
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;

    // Вспомогательные приватные методы
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();   // только аллокация, без записи
    void createSyncObjects();
};