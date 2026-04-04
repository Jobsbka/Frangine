#include "VulkanRenderer.hpp"
#include "core/utils/Logger.hpp"
#include <set>
#include <cstring>
#include <stdexcept>
#include <algorithm>

// Debug messenger callback
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        FRABGINE_LOG_ERROR("Vulkan Validation: {}", pCallbackData->pMessage);
    } else {
        FRABGINE_LOG_INFO("Vulkan Info: {}", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

namespace frabgine::graphics {

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer() {
    shutdown();
}

bool VulkanRenderer::initialize(void* windowHandle, uint32_t width, uint32_t height, const VulkanConfig& config) {
    config_ = config;
    
    FRABGINE_LOG_INFO("Initializing Vulkan Renderer...");
    
    if (!createInstance(config)) {
        FRABGINE_LOG_ERROR("Failed to create Vulkan instance");
        return false;
    }
    
    if (config.enableValidationLayers && !setupDebugMessenger(config)) {
        FRABGINE_LOG_ERROR("Failed to setup debug messenger");
        return false;
    }
    
    if (!selectPhysicalDevice()) {
        FRABGINE_LOG_ERROR("Failed to select physical device");
        return false;
    }
    
    if (!createLogicalDevice()) {
        FRABGINE_LOG_ERROR("Failed to create logical device");
        return false;
    }
    
    if (!createSwapchain(windowHandle, width, height)) {
        FRABGINE_LOG_ERROR("Failed to create swapchain");
        return false;
    }
    
    if (!createImageViews()) {
        FRABGINE_LOG_ERROR("Failed to create image views");
        return false;
    }
    
    if (!createRenderPass()) {
        FRABGINE_LOG_ERROR("Failed to create render pass");
        return false;
    }
    
    if (!createFramebuffers()) {
        FRABGINE_LOG_ERROR("Failed to create framebuffers");
        return false;
    }
    
    if (!createCommandPool()) {
        FRABGINE_LOG_ERROR("Failed to create command pool");
        return false;
    }
    
    if (!createSyncObjects()) {
        FRABGINE_LOG_ERROR("Failed to create sync objects");
        return false;
    }
    
    initialized_ = true;
    FRABGINE_LOG_INFO("Vulkan Renderer initialized successfully");
    return true;
}

void VulkanRenderer::shutdown() {
    if (!initialized_) return;
    
    vkDeviceWaitIdle(device_);
    
    cleanupSwapchain();
    
    for (size_t i = 0; i < config_.maxFramesInFlight; i++) {
        vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
        vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
        vkDestroyFence(device_, inFlightFences_[i], nullptr);
    }
    
    vkDestroyCommandPool(device_, commandPool_, nullptr);
    
    if (config_.enableValidationLayers && config_.enableDebugMessenger) {
        cleanupDebugMessenger();
    }
    
    vkDestroyDevice(device_, nullptr);
    vkDestroyInstance(instance_, nullptr);
    
    initialized_ = false;
    FRABGINE_LOG_INFO("Vulkan Renderer shut down");
}

void VulkanRenderer::beginFrame() {
    if (!initialized_) return;
    
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);
    
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX, 
        imageAvailableSemaphores_[currentFrame_], 
        VK_NULL_HANDLE, &imageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain(swapchainExtent_.width, swapchainExtent_.height);
        return;
    } else if (result != VK_SUCCESS && result != VK_TIMEOUT) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }
    
    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
    vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
}

void VulkanRenderer::endFrame() {
    if (!initialized_) return;
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
    
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentFrame_]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = {swapchain_};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &currentFrame_;
    
    VkResult result = vkQueuePresentKHR(presentQueue_, &presentInfo);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain(swapchainExtent_.width, swapchainExtent_.height);
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }
    
    currentFrame_ = (currentFrame_ + 1) % config_.maxFramesInFlight;
}

void VulkanRenderer::submitCommandBuffer(VkCommandBuffer cmdBuffer) {
    // Для простоты используем тот же command buffer из пула
    // В реальной реализации нужно управлять отдельными command buffers
}

void VulkanRenderer::recreateSwapchain(uint32_t width, uint32_t height) {
    if (!initialized_) return;
    
    vkDeviceWaitIdle(device_);
    
    cleanupSwapchain();
    createSwapchain(nullptr, width, height); // windowHandle не нужен при ресоздании
    createImageViews();
    createRenderPass();
    createFramebuffers();
    
    FRABGINE_LOG_INFO("Swapchain recreated: {}x{}", width, height);
}

bool VulkanRenderer::createInstance(const VulkanConfig& config) {
    if (config.enableValidationLayers && !checkValidationLayerSupport()) {
        FRABGINE_LOG_ERROR("Validation layers requested but not available");
        return false;
    }
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Frabgine Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Frabgine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (config.enableValidationLayers && config.enableDebugMessenger) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(std::vector<const char*>{"VK_LAYER_KHRONOS_validation"}.size());
        createInfo.ppEnabledLayerNames = &std::vector<const char*>{"VK_LAYER_KHRONOS_validation"}[0];
        
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }
    
    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        FRABGINE_LOG_ERROR("Failed to create Vulkan instance");
        return false;
    }
    
    FRABGINE_LOG_INFO("Vulkan instance created");
    return true;
}

bool VulkanRenderer::setupDebugMessenger(const VulkanConfig& config) {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance_, "vkCreateDebugUtilsMessengerEXT");
    
    if (func != nullptr) {
        return func(instance_, &createInfo, nullptr, &debugMessenger_) == VK_SUCCESS;
    }
    
    return false;
}

bool VulkanRenderer::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, 0, &deviceCount);
    
    if (deviceCount == 0) {
        FRABGINE_LOG_ERROR("Failed to find GPUs with Vulkan support");
        return false;
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, deviceCount, devices.data());
    
    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice_ = device;
            
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice_, &deviceProperties);
            FRABGINE_LOG_INFO("Selected GPU: {}", deviceProperties.deviceName);
            
            return true;
        }
    }
    
    FRABGINE_LOG_ERROR("Failed to find a suitable GPU");
    return false;
}

bool VulkanRenderer::createLogicalDevice() {
    graphicsQueueFamily_ = findQueueFamilyIndex(physicalDevice_, VK_QUEUE_GRAPHICS_BIT);
    presentQueueFamily_ = findQueueFamilyIndex(physicalDevice_, VK_QUEUE_TRANSFER_BIT);
    
    if (graphicsQueueFamily_ == UINT32_MAX) {
        FRABGINE_LOG_ERROR("Failed to find graphics queue family");
        return false;
    }
    
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily_;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(std::vector<const char*>{VK_KHR_SWAPCHAIN_EXTENSION_NAME}.size());
    createInfo.ppEnabledExtensionNames = &std::vector<const char*>{VK_KHR_SWAPCHAIN_EXTENSION_NAME}[0];
    
    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        FRABGINE_LOG_ERROR("Failed to create logical device");
        return false;
    }
    
    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentQueueFamily_ != UINT32_MAX ? presentQueueFamily_ : graphicsQueueFamily_, 0, &presentQueue_);
    
    FRABGINE_LOG_INFO("Logical device created");
    return true;
}

bool VulkanRenderer::createSwapchain(void* windowHandle, uint32_t width, uint32_t height) {
    // Упрощенная реализация без привязки к окну (для headless или с Qt)
    // В полной версии нужно использовать surface от окна
    
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, VK_NULL_HANDLE, &capabilities);
    
    swapchainExtent_ = capabilities.currentExtent;
    if (swapchainExtent_.width == UINT32_MAX) {
        swapchainExtent_.width = width;
        swapchainExtent_.height = height;
    }
    
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = VK_NULL_HANDLE; // Нужно установить реальный surface
    createInfo.minImageCount = 2;
    createInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = swapchainExtent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    
    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        FRABGINE_LOG_ERROR("Failed to create swapchain");
        return false;
    }
    
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());
    
    swapchainImageFormat_ = VK_FORMAT_B8G8R8A8_SRGB;
    
    FRABGINE_LOG_INFO("Swapchain created: {}x{}", swapchainExtent_.width, swapchainExtent_.height);
    return true;
}

bool VulkanRenderer::createImageViews() {
    swapchainImageViews_.resize(swapchainImages_.size());
    
    for (size_t i = 0; i < swapchainImages_.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainImageFormat_;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(device_, &createInfo, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) {
            FRABGINE_LOG_ERROR("Failed to create image view");
            return false;
        }
    }
    
    FRABGINE_LOG_INFO("Created {} image views", swapchainImageViews_.size());
    return true;
}

bool VulkanRenderer::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    
    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    
    if (vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        FRABGINE_LOG_ERROR("Failed to create render pass");
        return false;
    }
    
    FRABGINE_LOG_INFO("Render pass created");
    return true;
}

bool VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());
    
    for (size_t i = 0; i < swapchainImageViews_.size(); i++) {
        VkImageView attachments[] = {swapchainImageViews_[i]};
        
        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = renderPass_;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = attachments;
        createInfo.width = swapchainExtent_.width;
        createInfo.height = swapchainExtent_.height;
        createInfo.layers = 1;
        
        if (vkCreateFramebuffer(device_, &createInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            FRABGINE_LOG_ERROR("Failed to create framebuffer");
            return false;
        }
    }
    
    FRABGINE_LOG_INFO("Created {} framebuffers", framebuffers_.size());
    return true;
}

bool VulkanRenderer::createCommandPool() {
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = graphicsQueueFamily_;
    
    if (vkCreateCommandPool(device_, &createInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        FRABGINE_LOG_ERROR("Failed to create command pool");
        return false;
    }
    
    commandBuffers_.resize(config_.maxFramesInFlight);
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
    
    if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        FRABGINE_LOG_ERROR("Failed to allocate command buffers");
        return false;
    }
    
    FRABGINE_LOG_INFO("Command pool and buffers created");
    return true;
}

bool VulkanRenderer::createSyncObjects() {
    imageAvailableSemaphores_.resize(config_.maxFramesInFlight);
    renderFinishedSemaphores_.resize(config_.maxFramesInFlight);
    inFlightFences_.resize(config_.maxFramesInFlight);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (size_t i = 0; i < config_.maxFramesInFlight; i++) {
        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
            FRABGINE_LOG_ERROR("Failed to create sync objects");
            return false;
        }
    }
    
    FRABGINE_LOG_INFO("Sync objects created");
    return true;
}

void VulkanRenderer::cleanupSwapchain() {
    for (auto framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    framebuffers_.clear();
    
    for (auto imageView : swapchainImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    swapchainImageViews_.clear();
    
    vkDestroyRenderPass(device_, renderPass_, nullptr);
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
}

void VulkanRenderer::cleanupDebugMessenger() {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance_, "vkDestroyDebugUtilsMessengerEXT");
    
    if (func != nullptr) {
        func(instance_, debugMessenger_, nullptr);
    }
}

bool VulkanRenderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    
    for (const auto& layerProperties : availableLayers) {
        if (strcmp(layerProperties.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            return true;
        }
    }
    
    return false;
}

std::vector<const char*> VulkanRenderer::getRequiredExtensions() {
    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
    #ifdef _WIN32
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    #elif defined(__linux__)
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
    #endif
    };
    
    return extensions;
}

bool VulkanRenderer::isDeviceSuitable(VkPhysicalDevice device) {
    if (!checkDeviceExtensionSupport(device)) {
        return false;
    }
    
    // Проверка queue families и других характеристик
    return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    
    std::set<std::string> requiredExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    
    return requiredExtensions.empty();
}

uint32_t VulkanRenderer::findQueueFamilyIndex(VkPhysicalDevice device, VkQueueFlags flags) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & flags) {
            return i;
        }
    }
    
    return UINT32_MAX;
}

} // namespace frabgine::graphics
