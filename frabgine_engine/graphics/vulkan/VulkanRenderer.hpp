#pragma once

#include "core/memory/SmartPointers.hpp"
#include "core/math/Vector.hpp"
#include "core/math/Matrix.hpp"
#include <string>
#include <vector>
#include <optional>

#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifdef __linux__
    #define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.h>

namespace frabgine::graphics {

// Конфигурация Vulkan устройства
struct VulkanConfig {
    bool enableValidationLayers = true;
    bool enableDebugMessenger = true;
    uint32_t maxFramesInFlight = 2;
};

// Основной класс Vulkan рендерера
class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    // Инициализация и очистка
    bool initialize(void* windowHandle, uint32_t width, uint32_t height, const VulkanConfig& config = {});
    void shutdown();

    // Рендеринг
    void beginFrame();
    void endFrame();
    void submitCommandBuffer(VkCommandBuffer cmdBuffer);

    // Управление ресурсами
    VkDevice getDevice() const { return device_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    VkInstance getInstance() const { return instance_; }
    VkQueue getGraphicsQueue() const { return graphicsQueue_; }
    VkQueue getPresentQueue() const { return presentQueue_; }
    VkRenderPass getRenderPass() const { return renderPass_; }
    VkSwapchain getSwapchain() const { return swapchain_; }

    // Recreate swapchain при изменении размера окна
    void recreateSwapchain(uint32_t width, uint32_t height);

    // Получение текущих размеров
    uint32_t getWidth() const { return swapchainExtent_.width; }
    uint32_t getHeight() const { return swapchainExtent_.height; }

private:
    // Инициализация компонентов Vulkan
    bool createInstance(const VulkanConfig& config);
    bool setupDebugMessenger(const VulkanConfig& config);
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain(void* windowHandle, uint32_t width, uint32_t height);
    bool createImageViews();
    bool createRenderPass();
    bool createFramebuffers();
    bool createCommandPool();
    bool createSyncObjects();

    // Очистка
    void cleanupSwapchain();
    void cleanupDebugMessenger();

    // Утилиты
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    uint32_t findQueueFamilyIndex(VkPhysicalDevice device, VkQueueFlags flags);

    // Основные объекты Vulkan
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkFormat swapchainImageFormat_;
    VkExtent2D swapchainExtent_{};

    // Render pass и framebuffer
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    // Command buffer
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    // Queues
    uint32_t graphicsQueueFamily_ = UINT32_MAX;
    uint32_t presentQueueFamily_ = UINT32_MAX;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    // Sync objects
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    uint32_t currentFrame_ = 0;

    // Конфигурация
    VulkanConfig config_;
    bool initialized_ = false;
};

} // namespace frabgine::graphics
