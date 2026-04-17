// src/backends/backend_vulkan.cpp
#include "../include/arxrender_backend.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <array>
#include <iostream>

namespace arxrender {

// ============================================================================
// Внутренние структуры состояния
// ============================================================================
struct VkState_Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
    std::vector<VkPhysicalDevice> physicalDevices;
    bool debugEnabled = false;
    std::mutex mutex;
};

struct VkState_Device {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    std::mutex mutex;
};

struct VkState_Surface {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkImageView> imageViews;
    std::vector<VkImage> images;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    uint32_t width = 0, height = 0;
    uint32_t imageIndex = 0;
    std::mutex mutex;
};

struct VkState_Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
};

struct VkState_Texture {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    uint32_t width = 1, height = 1;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct VkState_Shader {
    VkShaderModule module = VK_NULL_HANDLE;
    AR_shader_stage stage = AR_shader_stage::VERTEX;
};

struct VkState_Pipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    AR_primitive_topology topology = AR_primitive_topology::TRIANGLE_LIST;
    VkRenderPass renderPass = VK_NULL_HANDLE;
};

struct VkState_Material {
    VkState_Pipeline* pipeline = nullptr;
    std::unordered_map<std::string, VkState_Texture*> textures;
    std::unordered_map<std::string, std::any> uniforms;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
};

struct VkState_CmdBuffer {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkDeviceState* deviceState = nullptr;
    bool recording = false;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
};

// ============================================================================
// Вспомогательные функции
// ============================================================================
static VkFormat ar_to_vk_format(AR_format fmt) {
    switch (fmt) {
        case AR_format::R8_UNORM: return VK_FORMAT_R8_UNORM;
        case AR_format::RG8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case AR_format::RGB8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
        case AR_format::RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case AR_format::R16_UNORM: return VK_FORMAT_R16_UNORM;
        case AR_format::RG16_UNORM: return VK_FORMAT_R16G16_UNORM;
        case AR_format::RGBA16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
        case AR_format::R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
        case AR_format::RG32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case AR_format::RGB32_FLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case AR_format::RGBA32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case AR_format::R32_UINT: return VK_FORMAT_R32_UINT;
        case AR_format::RG32_UINT: return VK_FORMAT_R32G32_UINT;
        case AR_format::RGBA32_UINT: return VK_FORMAT_R32G32B32A32_UINT;
        case AR_format::DEPTH32: return VK_FORMAT_D32_SFLOAT;
        case AR_format::DEPTH24_STENCIL8: return VK_FORMAT_D24_UNORM_S8_UINT;
        case AR_format::BC1_RGB: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case AR_format::BC3_RGBA: return VK_FORMAT_BC3_UNORM_BLOCK;
        default: return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

static VkPrimitiveTopology ar_to_vk_topology(AR_primitive_topology t) {
    switch (t) {
        case AR_primitive_topology::POINT_LIST: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case AR_primitive_topology::LINE_LIST: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case AR_primitive_topology::LINE_STRIP: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case AR_primitive_topology::TRIANGLE_LIST: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case AR_primitive_topology::TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case AR_primitive_topology::TRIANGLE_FAN: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

static VkShaderStageFlagBits ar_to_vk_stage(AR_shader_stage s) {
    switch (s) {
        case AR_shader_stage::VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
        case AR_shader_stage::FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case AR_shader_stage::COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
        case AR_shader_stage::GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case AR_shader_stage::TESSELLATION_CTRL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case AR_shader_stage::TESSELLATION_EVAL: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        default: return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

static VkCompareOp ar_to_vk_compare(AR_compare_op op) {
    switch (op) {
        case AR_compare_op::NEVER: return VK_COMPARE_OP_NEVER;
        case AR_compare_op::LESS: return VK_COMPARE_OP_LESS;
        case AR_compare_op::EQUAL: return VK_COMPARE_OP_EQUAL;
        case AR_compare_op::LESS_EQUAL: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case AR_compare_op::GREATER: return VK_COMPARE_OP_GREATER;
        case AR_compare_op::NOT_EQUAL: return VK_COMPARE_OP_NOT_EQUAL;
        case AR_compare_op::GREATER_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case AR_compare_op::ALWAYS: return VK_COMPARE_OP_ALWAYS;
        default: return VK_COMPARE_OP_LESS;
    }
}

static VkCullModeFlags ar_to_vk_cull(AR_cull_mode mode) {
    switch (mode) {
        case AR_cull_mode::NONE: return VK_CULL_MODE_NONE;
        case AR_cull_mode::FRONT: return VK_CULL_MODE_FRONT_BIT;
        case AR_cull_mode::BACK: return VK_CULL_MODE_BACK_BIT;
        default: return VK_CULL_MODE_BACK_BIT;
    }
}

static VkFrontFace ar_to_vk_front(AR_front_face face) {
    return (face == AR_front_face::CW) ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

static VkPolygonMode ar_to_vk_polygon(AR_polygon_mode mode) {
    switch (mode) {
        case AR_polygon_mode::FILL: return VK_POLYGON_MODE_FILL;
        case AR_polygon_mode::LINE: return VK_POLYGON_MODE_LINE;
        case AR_polygon_mode::POINT: return VK_POLYGON_MODE_POINT;
        default: return VK_POLYGON_MODE_FILL;
    }
}

static VkBlendFactor ar_to_vk_blend(AR_blend_factor f) {
    switch (f) {
        case AR_blend_factor::ZERO: return VK_BLEND_FACTOR_ZERO;
        case AR_blend_factor::ONE: return VK_BLEND_FACTOR_ONE;
        case AR_blend_factor::SRC_COLOR: return VK_BLEND_FACTOR_SRC_COLOR;
        case AR_blend_factor::ONE_MINUS_SRC_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case AR_blend_factor::DST_COLOR: return VK_BLEND_FACTOR_DST_COLOR;
        case AR_blend_factor::ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case AR_blend_factor::SRC_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
        case AR_blend_factor::ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case AR_blend_factor::DST_ALPHA: return VK_BLEND_FACTOR_DST_ALPHA;
        case AR_blend_factor::ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        default: return VK_BLEND_FACTOR_ONE;
    }
}

static VkBlendOp ar_to_vk_blend_op(AR_blend_op op) {
    switch (op) {
        case AR_blend_op::ADD: return VK_BLEND_OP_ADD;
        case AR_blend_op::SUBTRACT: return VK_BLEND_OP_SUBTRACT;
        case AR_blend_op::REVERSE_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case AR_blend_op::MIN: return VK_BLEND_OP_MIN;
        case AR_blend_op::MAX: return VK_BLEND_OP_MAX;
        default: return VK_BLEND_OP_ADD;
    }
}

static uint32_t find_memory_type(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    (void)messageType;
    (void)pUserData;
    
    const char* severity = "UNKNOWN";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) severity = "VERBOSE";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) severity = "INFO";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) severity = "WARNING";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) severity = "ERROR";
    
    std::cerr << "[Vulkan/" << severity << "] " << pCallbackData->pMessage << std::endl;
    
    return VK_FALSE;
}

static VkResult create_debug_utils_messenger(VkInstance instance, 
    PFN_vkCreateDebugUtilsMessengerEXT* pCreateFunc,
    PFN_vkDestroyDebugUtilsMessengerEXT* pDestroyFunc,
    VkDebugUtilsMessengerEXT* pMessenger) {
    
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = 
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = vk_debug_callback;
    
    *pCreateFunc = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    *pDestroyFunc = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    
    if (!*pCreateFunc || !*pDestroyFunc) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    
    return (*pCreateFunc)(instance, &createInfo, nullptr, pMessenger);
}

static void destroy_debug_utils_messenger(VkInstance instance, 
    PFN_vkDestroyDebugUtilsMessengerEXT destroyFunc,
    VkDebugUtilsMessengerEXT messenger) {
    if (destroyFunc) {
        destroyFunc(instance, messenger, nullptr);
    }
}

// ============================================================================
// Platform-specific WSI extensions
// ============================================================================
#if defined(_WIN32)
static const char* get_platform_surface_extension() { return VK_KHR_WIN32_SURFACE_EXTENSION_NAME; }

static VkResult create_platform_surface(VkInstance instance, void* nativeWindow, VkSurfaceKHR* surface) {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(nullptr);
    createInfo.hwnd = static_cast<HWND>(nativeWindow);
    return vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, surface);
}

#elif defined(__linux__) && !defined(__ANDROID__)
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
static bool has_wayland_display() {
    return getenv("WAYLAND_DISPLAY") != nullptr;
}
#endif

static const char* get_platform_surface_extension() {
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    if (has_wayland_display()) return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
#endif
    return VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
}

static VkResult create_platform_surface(VkInstance instance, void* nativeWindow, VkSurfaceKHR* surface) {
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    if (has_wayland_display()) {
        VkWaylandSurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        createInfo.display = nullptr; // Should be provided by app
        createInfo.surface = nativeWindow;
        return vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, surface);
    }
#endif
    VkXlibSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.dpy = nullptr; // Should be provided by app
    createInfo.window = reinterpret_cast<Window>(nativeWindow);
    return vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, surface);
}

#elif defined(__APPLE__)
static const char* get_platform_surface_extension() { return VK_EXT_METAL_SURFACE_EXTENSION_NAME; }

static VkResult create_platform_surface(VkInstance instance, void* nativeWindow, VkSurfaceKHR* surface) {
    VkMetalSurfaceCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    createInfo.pLayer = nativeWindow; // CAMetalLayer*
    return vkCreateMetalSurfaceEXT(instance, &createInfo, nullptr, surface);
}

#elif defined(__ANDROID__)
static const char* get_platform_surface_extension() { return VK_KHR_ANDROID_SURFACE_EXTENSION_NAME; }

static VkResult create_platform_surface(VkInstance instance, void* nativeWindow, VkSurfaceKHR* surface) {
    VkAndroidSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.window = reinterpret_cast<ANativeWindow*>(nativeWindow);
    return vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, surface);
}

#else
static const char* get_platform_surface_extension() { return nullptr; }
static VkResult create_platform_surface(VkInstance, void*, VkSurfaceKHR*) { return VK_ERROR_INITIALIZATION_FAILED; }
#endif

// ============================================================================
// Backend callbacks implementation
// ============================================================================

// --- Context callbacks ---
static AR_result vk_onContextInit(AR_context* context, const AR_context_config* config) {
    if (!context || !config) return AR_result::INVALID_ARGS;
    
    auto* ctxState = new VkState_Context{};
    if (!ctxState) return AR_result::OUT_OF_MEMORY;
    
    ctxState->debugEnabled = config->debug_layer;
    
    // Enable validation layers if requested
    std::vector<const char*> validationLayers;
    if (ctxState->debugEnabled) {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        
        for (const char* layerName : {"VK_LAYER_KHRONOS_validation"}) {
            for (const auto& layer : availableLayers) {
                if (strcmp(layer.layerName, layerName) == 0) {
                    validationLayers.push_back(layerName);
                    break;
                }
            }
        }
    }
    
    // Collect extensions
    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME};
    const char* platformExt = get_platform_surface_extension();
    if (platformExt) {
        extensions.push_back(platformExt);
    }
    if (ctxState->debugEnabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    // Create instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "ArxRender";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "ArxGlue";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    if (vkCreateInstance(&createInfo, nullptr, &ctxState->instance) != VK_SUCCESS) {
        delete ctxState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Setup debug messenger if enabled
    if (ctxState->debugEnabled) {
        VkResult result = create_debug_utils_messenger(
            ctxState->instance,
            &ctxState->vkCreateDebugUtilsMessengerEXT,
            &ctxState->vkDestroyDebugUtilsMessengerEXT,
            &ctxState->debugMessenger);
        if (result != VK_SUCCESS) {
            vkDestroyInstance(ctxState->instance, nullptr);
            delete ctxState;
            return AR_result::ERROR_GENERIC;
        }
    }
    
    // Enumerate physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(ctxState->instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        if (ctxState->debugMessenger) {
            destroy_debug_utils_messenger(ctxState->instance, 
                ctxState->vkDestroyDebugUtilsMessengerEXT, ctxState->debugMessenger);
        }
        vkDestroyInstance(ctxState->instance, nullptr);
        delete ctxState;
        return AR_result::NO_BACKEND;
    }
    
    ctxState->physicalDevices.resize(deviceCount);
    vkEnumeratePhysicalDevices(ctxState->instance, &deviceCount, ctxState->physicalDevices.data());
    
    context->p_impl->backend_data = static_cast<void*>(ctxState);
    return AR_result::SUCCESS;
}

static AR_result vk_onContextUninit(AR_context* context) {
    if (!context || !context->p_impl) return AR_result::INVALID_ARGS;
    
    auto* ctxState = static_cast<VkState_Context*>(context->p_impl->backend_data);
    if (!ctxState) return AR_result::SUCCESS;
    
    if (ctxState->debugMessenger) {
        destroy_debug_utils_messenger(ctxState->instance, 
            ctxState->vkDestroyDebugUtilsMessengerEXT, ctxState->debugMessenger);
    }
    
    vkDestroyInstance(ctxState->instance, nullptr);
    delete ctxState;
    context->p_impl->backend_data = nullptr;
    
    return AR_result::SUCCESS;
}

static AR_result vk_onContextEnumerateDevices(AR_context*, AR_device_type, 
    AR_enumerate_devices_callback, void*) {
    return AR_result::NOT_IMPLEMENTED;
}

static AR_result vk_onContextGetDeviceInfo(AR_context*, AR_device_type, 
    const void*, AR_device_info*) {
    return AR_result::NOT_IMPLEMENTED;
}

// --- Device callbacks ---
static AR_result vk_onDeviceInit(AR_context* context, AR_device_type, 
    const void*, const AR_device_config* config, AR_device* device) {
    
    if (!context || !context->p_impl || !config || !device) {
        return AR_result::INVALID_ARGS;
    }
    
    auto* ctxState = static_cast<VkState_Context*>(context->p_impl->backend_data);
    if (!ctxState || ctxState->physicalDevices.empty()) {
        return AR_result::NO_BACKEND;
    }
    
    auto* devState = new VkState_Device{};
    if (!devState) return AR_result::OUT_OF_MEMORY;
    
    // Select physical device (first suitable one)
    for (VkPhysicalDevice physicalDevice : ctxState->physicalDevices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(physicalDevice, &features);
        
        // Check queue family support
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        
        bool foundGraphics = false;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                devState->graphicsQueueFamily = i;
                foundGraphics = true;
                break;
            }
        }
        
        if (foundGraphics) {
            devState->physicalDevice = physicalDevice;
            break;
        }
    }
    
    if (devState->physicalDevice == VK_NULL_HANDLE) {
        delete devState;
        return AR_result::NO_BACKEND;
    }
    
    // Create logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = devState->graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    
    std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    
    if (vkCreateDevice(devState->physicalDevice, &deviceCreateInfo, nullptr, &devState->logicalDevice) != VK_SUCCESS) {
        delete devState;
        return AR_result::ERROR_GENERIC;
    }
    
    vkGetDeviceQueue(devState->logicalDevice, devState->graphicsQueueFamily, 0, &devState->graphicsQueue);
    
    // Create command pool
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = devState->graphicsQueueFamily;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    if (vkCreateCommandPool(devState->logicalDevice, &cmdPoolInfo, nullptr, &devState->commandPool) != VK_SUCCESS) {
        vkDestroyDevice(devState->logicalDevice, nullptr);
        delete devState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 2> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024}
    }};
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1024;
    
    if (vkCreateDescriptorPool(devState->logicalDevice, &poolInfo, nullptr, &devState->descriptorPool) != VK_SUCCESS) {
        vkDestroyCommandPool(devState->logicalDevice, devState->commandPool, nullptr);
        vkDestroyDevice(devState->logicalDevice, nullptr);
        delete devState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Create pipeline cache
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vkCreatePipelineCache(devState->logicalDevice, &cacheInfo, nullptr, &devState->pipelineCache);
    
    device->p_impl->backend_data = static_cast<void*>(devState);
    return AR_result::SUCCESS;
}

static AR_result vk_onDeviceUninit(AR_device* device) {
    if (!device || !device->p_impl) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<VkState_Device*>(device->p_impl->backend_data);
    if (!devState) return AR_result::SUCCESS;
    
    // Cleanup pipeline cache
    if (devState->pipelineCache) {
        vkDestroyPipelineCache(devState->logicalDevice, devState->pipelineCache, nullptr);
    }
    
    // Cleanup descriptor pool
    if (devState->descriptorPool) {
        vkDestroyDescriptorPool(devState->logicalDevice, devState->descriptorPool, nullptr);
    }
    
    // Cleanup command pool
    if (devState->commandPool) {
        vkDestroyCommandPool(devState->logicalDevice, devState->commandPool, nullptr);
    }
    
    // Destroy logical device
    if (devState->logicalDevice) {
        vkDestroyDevice(devState->logicalDevice, nullptr);
    }
    
    delete devState;
    device->p_impl->backend_data = nullptr;
    
    return AR_result::SUCCESS;
}

static AR_result vk_onDeviceStart(AR_device*) { return AR_result::SUCCESS; }
static AR_result vk_onDeviceStop(AR_device*) { return AR_result::SUCCESS; }

// --- Surface callbacks ---
static AR_result vk_onSurfaceInit(AR_context* context, const AR_surface_config* config, AR_surface* surface) {
    if (!context || !context->p_impl || !config || !surface) {
        return AR_result::INVALID_ARGS;
    }
    
    auto* ctxState = static_cast<VkState_Context*>(context->p_impl->backend_data);
    if (!ctxState) return AR_result::INVALID_OPERATION;
    
    auto* surfState = new VkState_Surface{};
    if (!surfState) return AR_result::OUT_OF_MEMORY;
    
    // Create surface
    if (create_platform_surface(ctxState->instance, config->native_window_handle, &surfState->surface) != VK_SUCCESS) {
        delete surfState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Get surface capabilities and formats
    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctxState->physicalDevices[0], surfState->surface, &surfaceCaps);
    
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctxState->physicalDevices[0], surfState->surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctxState->physicalDevices[0], surfState->surface, &formatCount, surfaceFormats.data());
    
    // Select format
    surfState->format = VK_FORMAT_B8G8R8A8_UNORM;
    surfState->colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    for (const auto& fmt : surfaceFormats) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM || fmt.format == VK_FORMAT_R8G8B8A8_UNORM) {
            surfState->format = fmt.format;
            surfState->colorSpace = fmt.colorSpace;
            break;
        }
    }
    
    // Select present mode
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctxState->physicalDevices[0], surfState->surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctxState->physicalDevices[0], surfState->surface, &presentModeCount, presentModes.data());
    
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }
    
    // Determine swapchain extent
    VkExtent2D extent = surfaceCaps.currentExtent;
    if (surfaceCaps.currentExtent.width == UINT32_MAX) {
        extent.width = std::clamp(config->width, surfaceCaps.minImageExtent.width, surfaceCaps.maxImageExtent.width);
        extent.height = std::clamp(config->height, surfaceCaps.minImageExtent.height, surfaceCaps.maxImageExtent.height);
    }
    
    surfState->width = extent.width;
    surfState->height = extent.height;
    
    // Create swapchain
    uint32_t imageCount = config->swapchain_image_count;
    if (imageCount < surfaceCaps.minImageCount) imageCount = surfaceCaps.minImageCount;
    if (surfaceCaps.maxImageCount > 0 && imageCount > surfaceCaps.maxImageCount) {
        imageCount = surfaceCaps.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surfState->surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfState->format;
    swapchainInfo.imageColorSpace = surfState->colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.preTransform = surfaceCaps.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
    
    auto* devState = static_cast<VkState_Device*>(surface->p_impl->context->p_impl->backend_data);
    if (vkCreateSwapchainKHR(devState->logicalDevice, &swapchainInfo, nullptr, &surfState->swapchain) != VK_SUCCESS) {
        vkDestroySurfaceKHR(ctxState->instance, surfState->surface, nullptr);
        delete surfState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Get swapchain images
    vkGetSwapchainImagesKHR(devState->logicalDevice, surfState->swapchain, &imageCount, nullptr);
    surfState->images.resize(imageCount);
    vkGetSwapchainImagesKHR(devState->logicalDevice, surfState->swapchain, &imageCount, surfState->images.data());
    
    // Create image views
    surfState->imageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = surfState->images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfState->format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(devState->logicalDevice, &viewInfo, nullptr, &surfState->imageViews[i]) != VK_SUCCESS) {
            // Cleanup on failure
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyImageView(devState->logicalDevice, surfState->imageViews[j], nullptr);
            }
            vkDestroySwapchainKHR(devState->logicalDevice, surfState->swapchain, nullptr);
            vkDestroySurfaceKHR(ctxState->instance, surfState->surface, nullptr);
            delete surfState;
            return AR_result::ERROR_GENERIC;
        }
    }
    
    // Create render pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = surfState->format;
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
    
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    if (vkCreateRenderPass(devState->logicalDevice, &renderPassInfo, nullptr, &surfState->renderPass) != VK_SUCCESS) {
        for (auto& view : surfState->imageViews) {
            vkDestroyImageView(devState->logicalDevice, view, nullptr);
        }
        vkDestroySwapchainKHR(devState->logicalDevice, surfState->swapchain, nullptr);
        vkDestroySurfaceKHR(ctxState->instance, surfState->surface, nullptr);
        delete surfState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Create framebuffers
    surfState->framebuffers.resize(surfState->imageViews.size());
    for (size_t i = 0; i < surfState->imageViews.size(); i++) {
        VkImageView attachments[] = {surfState->imageViews[i]};
        
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = surfState->renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = surfState->width;
        fbInfo.height = surfState->height;
        fbInfo.layers = 1;
        
        if (vkCreateFramebuffer(devState->logicalDevice, &fbInfo, nullptr, &surfState->framebuffers[i]) != VK_SUCCESS) {
            for (size_t j = 0; j < i; j++) {
                vkDestroyFramebuffer(devState->logicalDevice, surfState->framebuffers[j], nullptr);
            }
            vkDestroyRenderPass(devState->logicalDevice, surfState->renderPass, nullptr);
            for (auto& view : surfState->imageViews) {
                vkDestroyImageView(devState->logicalDevice, view, nullptr);
            }
            vkDestroySwapchainKHR(devState->logicalDevice, surfState->swapchain, nullptr);
            vkDestroySurfaceKHR(ctxState->instance, surfState->surface, nullptr);
            delete surfState;
            return AR_result::ERROR_GENERIC;
        }
    }
    
    surface->p_impl->backend_data = static_cast<void*>(surfState);
    return AR_result::SUCCESS;
}

static AR_result vk_onSurfaceUninit(AR_surface* surface) {
    if (!surface || !surface->p_impl) return AR_result::INVALID_ARGS;
    
    auto* surfState = static_cast<VkState_Surface*>(surface->p_impl->backend_data);
    if (!surfState) return AR_result::SUCCESS;
    
    auto* devState = static_cast<VkState_Device*>(surface->p_impl->context->p_impl->backend_data);
    auto* ctxState = static_cast<VkState_Context*>(surface->p_impl->context->p_impl->backend_data);
    
    // Wait for device idle before cleanup
    vkDeviceWaitIdle(devState->logicalDevice);
    
    // Cleanup framebuffers
    for (auto fb : surfState->framebuffers) {
        vkDestroyFramebuffer(devState->logicalDevice, fb, nullptr);
    }
    
    // Cleanup render pass
    if (surfState->renderPass) {
        vkDestroyRenderPass(devState->logicalDevice, surfState->renderPass, nullptr);
    }
    
    // Cleanup image views
    for (auto view : surfState->imageViews) {
        vkDestroyImageView(devState->logicalDevice, view, nullptr);
    }
    
    // Cleanup swapchain
    if (surfState->swapchain) {
        vkDestroySwapchainKHR(devState->logicalDevice, surfState->swapchain, nullptr);
    }
    
    // Cleanup surface
    if (surfState->surface) {
        vkDestroySurfaceKHR(ctxState->instance, surfState->surface, nullptr);
    }
    
    delete surfState;
    surface->p_impl->backend_data = nullptr;
    
    return AR_result::SUCCESS;
}

static AR_result vk_onSurfaceResize(AR_surface* surface, uint32_t width, uint32_t height) {
    if (!surface || !surface->p_impl) return AR_result::INVALID_ARGS;
    
    auto* surfState = static_cast<VkState_Surface*>(surface->p_impl->backend_data);
    if (!surfState) return AR_result::INVALID_OPERATION;
    
    // Mark for recreation on next present
    surfState->width = width;
    surfState->height = height;
    
    return AR_result::SUCCESS;
}

static AR_result vk_onSurfacePresent(AR_surface* surface, AR_command_buffer* cmd) {
    if (!surface || !surface->p_impl || !cmd) return AR_result::INVALID_ARGS;
    
    auto* surfState = static_cast<VkState_Surface*>(surface->p_impl->backend_data);
    auto* devState = static_cast<VkState_Device*>(surface->p_impl->context->p_impl->backend_data);
    auto* cmdState = static_cast<VkState_CmdBuffer*>(cmd->p_impl->backend_data);
    
    if (!surfState || !devState || !cmdState) return AR_result::INVALID_OPERATION;
    
    // Acquire next image
    VkResult result = vkAcquireNextImageKHR(devState->logicalDevice, surfState->swapchain, 
        UINT64_MAX, surfState->imageAvailableSemaphore, VK_NULL_HANDLE, &surfState->imageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // Surface needs recreation - simplified handling
        return AR_result::SUCCESS;
    } else if (result != VK_SUCCESS && result != VK_TIMEOUT) {
        return AR_result::ERROR_GENERIC;
    }
    
    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = {surfState->imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdState->commandBuffer;
    
    VkSemaphore signalSemaphores[] = {surfState->renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    if (vkQueueSubmit(devState->graphicsQueue, 1, &submitInfo, cmdState->fence) != VK_SUCCESS) {
        return AR_result::ERROR_GENERIC;
    }
    
    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapchains[] = {surfState->swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &surfState->imageIndex;
    
    result = vkQueuePresentKHR(devState->graphicsQueue, &presentInfo);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return AR_result::SUCCESS;
    } else if (result != VK_SUCCESS) {
        return AR_result::ERROR_GENERIC;
    }
    
    // Reset fence for next frame
    vkResetFences(devState->logicalDevice, 1, &cmdState->fence);
    
    return AR_result::SUCCESS;
}

// --- Resource callbacks ---
static AR_result vk_onBufferCreate(AR_device* device, const AR_buffer_desc* desc, AR_buffer** out) {
    if (!device || !device->p_impl || !desc || !out) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<VkState_Device*>(device->p_impl->backend_data);
    if (!devState) return AR_result::INVALID_OPERATION;
    
    auto* bufState = new VkState_Buffer{};
    if (!bufState) return AR_result::OUT_OF_MEMORY;
    
    // Determine usage flags
    VkBufferUsageFlags usage = 0;
    if (desc->usage & AR_usage::TRANSFER_SRC) usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (desc->usage & AR_usage::TRANSFER_DST) usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (desc->usage & AR_usage::UNIFORM_BUFFER) usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (desc->usage & AR_usage::STORAGE_BUFFER) usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (desc->usage & AR_usage::VERTEX_BUFFER) usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (desc->usage & AR_usage::INDEX_BUFFER) usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    
    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc->size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(devState->logicalDevice, &bufferInfo, nullptr, &bufState->buffer) != VK_SUCCESS) {
        delete bufState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(devState->logicalDevice, bufState->buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(devState->physicalDevice, 
        memRequirements.memoryTypeBits, 
        desc->host_visible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
                          : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(devState->logicalDevice, &allocInfo, nullptr, &bufState->memory) != VK_SUCCESS) {
        vkDestroyBuffer(devState->logicalDevice, bufState->buffer, nullptr);
        delete bufState;
        return AR_result::ERROR_GENERIC;
    }
    
    vkBindBufferMemory(devState->logicalDevice, bufState->buffer, bufState->memory, 0);
    
    // Upload initial data if provided
    if (desc->initial_data && desc->host_visible) {
        void* data;
        vkMapMemory(devState->logicalDevice, bufState->memory, 0, desc->size, 0, &data);
        memcpy(data, desc->initial_data, desc->size);
        vkUnmapMemory(devState->logicalDevice, bufState->memory);
    } else if (desc->initial_data && !desc->host_visible) {
        // Staging buffer for device-local memory
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        
        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size = desc->size;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        vkCreateBuffer(devState->logicalDevice, &stagingInfo, nullptr, &stagingBuffer);
        
        VkMemoryRequirements stagingMemReqs;
        vkGetBufferMemoryRequirements(devState->logicalDevice, stagingBuffer, &stagingMemReqs);
        
        VkMemoryAllocateInfo stagingAlloc{};
        stagingAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        stagingAlloc.allocationSize = stagingMemReqs.size;
        stagingAlloc.memoryTypeIndex = find_memory_type(devState->physicalDevice,
            stagingMemReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        vkAllocateMemory(devState->logicalDevice, &stagingAlloc, nullptr, &stagingMemory);
        vkBindBufferMemory(devState->logicalDevice, stagingBuffer, stagingMemory, 0);
        
        void* stagingData;
        vkMapMemory(devState->logicalDevice, stagingMemory, 0, desc->size, 0, &stagingData);
        memcpy(stagingData, desc->initial_data, desc->size);
        vkUnmapMemory(devState->logicalDevice, stagingMemory);
        
        // Copy to destination buffer
        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = devState->commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        
        vkAllocateCommandBuffers(devState->logicalDevice, &cmdAllocInfo, &commandBuffer);
        
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        
        VkBufferCopy copyRegion{};
        copyRegion.size = desc->size;
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, bufState->buffer, 1, &copyRegion);
        
        vkEndCommandBuffer(commandBuffer);
        
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        vkQueueSubmit(devState->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(devState->graphicsQueue);
        
        vkFreeCommandBuffers(devState->logicalDevice, devState->commandPool, 1, &commandBuffer);
        vkDestroyBuffer(devState->logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(devState->logicalDevice, stagingMemory, nullptr);
    }
    
    bufState->size = desc->size;
    bufState->usage = usage;
    
    *out = reinterpret_cast<AR_buffer*>(bufState);
    return AR_result::SUCCESS;
}

static void vk_onBufferDestroy(AR_buffer* buffer) {
    if (!buffer) return;
    
    auto* bufState = reinterpret_cast<VkState_Buffer*>(buffer);
    if (bufState->memory) {
        // Need device handle - in real impl would store reference
        vkFreeMemory(VK_NULL_HANDLE, bufState->memory, nullptr);
    }
    if (bufState->buffer) {
        vkDestroyBuffer(VK_NULL_HANDLE, bufState->buffer, nullptr);
    }
    delete bufState;
}

static AR_result vk_onTextureCreate(AR_device* device, const AR_texture_desc* desc, AR_texture** out) {
    if (!device || !device->p_impl || !desc || !out) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<VkState_Device*>(device->p_impl->backend_data);
    if (!devState) return AR_result::INVALID_OPERATION;
    
    auto* texState = new VkState_Texture{};
    if (!texState) return AR_result::OUT_OF_MEMORY;
    
    texState->width = desc->width;
    texState->height = desc->height;
    texState->format = ar_to_vk_format(desc->format);
    
    // Create image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = desc->width;
    imageInfo.extent.height = desc->height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = desc->mip_levels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = texState->format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    
    if (vkCreateImage(devState->logicalDevice, &imageInfo, nullptr, &texState->image) != VK_SUCCESS) {
        delete texState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(devState->logicalDevice, texState->image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_type(devState->physicalDevice,
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    if (vkAllocateMemory(devState->logicalDevice, &allocInfo, nullptr, &texState->memory) != VK_SUCCESS) {
        vkDestroyImage(devState->logicalDevice, texState->image, nullptr);
        delete texState;
        return AR_result::ERROR_GENERIC;
    }
    
    vkBindImageMemory(devState->logicalDevice, texState->image, texState->memory, 0);
    
    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texState->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = texState->format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = desc->mip_levels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(devState->logicalDevice, &viewInfo, nullptr, &texState->view) != VK_SUCCESS) {
        vkFreeMemory(devState->logicalDevice, texState->memory, nullptr);
        vkDestroyImage(devState->logicalDevice, texState->image, nullptr);
        delete texState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(desc->mip_levels);
    
    if (vkCreateSampler(devState->logicalDevice, &samplerInfo, nullptr, &texState->sampler) != VK_SUCCESS) {
        vkDestroyImageView(devState->logicalDevice, texState->view, nullptr);
        vkFreeMemory(devState->logicalDevice, texState->memory, nullptr);
        vkDestroyImage(devState->logicalDevice, texState->image, nullptr);
        delete texState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Upload initial data if provided
    if (desc->initial_data) {
        // Create staging buffer
        size_t imageSize = desc->width * desc->height * 4; // Assume RGBA
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = imageSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        vkCreateBuffer(devState->logicalDevice, &bufferInfo, nullptr, &stagingBuffer);
        
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(devState->logicalDevice, stagingBuffer, &memReqs);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = find_memory_type(devState->physicalDevice,
            memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        vkAllocateMemory(devState->logicalDevice, &allocInfo, nullptr, &stagingMemory);
        vkBindBufferMemory(devState->logicalDevice, stagingBuffer, stagingMemory, 0);
        
        void* data;
        vkMapMemory(devState->logicalDevice, stagingMemory, 0, imageSize, 0, &data);
        memcpy(data, desc->initial_data, imageSize);
        vkUnmapMemory(devState->logicalDevice, stagingMemory);
        
        // Transition image layout and copy
        VkCommandBuffer commandBuffer;
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = devState->commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        
        vkAllocateCommandBuffers(devState->logicalDevice, &cmdAllocInfo, &commandBuffer);
        
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        
        // Transition to transfer dst
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texState->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = desc->mip_levels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        // Copy buffer to image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {desc->width, desc->height, 1};
        
        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texState->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        // Transition to shader read
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        
        vkEndCommandBuffer(commandBuffer);
        
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        vkQueueSubmit(devState->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(devState->graphicsQueue);
        
        vkFreeCommandBuffers(devState->logicalDevice, devState->commandPool, 1, &commandBuffer);
        vkDestroyBuffer(devState->logicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(devState->logicalDevice, stagingMemory, nullptr);
    }
    
    *out = reinterpret_cast<AR_texture*>(texState);
    return AR_result::SUCCESS;
}

static void vk_onTextureDestroy(AR_texture* texture) {
    if (!texture) return;
    
    auto* texState = reinterpret_cast<VkState_Texture*>(texture);
    if (texState->sampler) {
        vkDestroySampler(VK_NULL_HANDLE, texState->sampler, nullptr);
    }
    if (texState->view) {
        vkDestroyImageView(VK_NULL_HANDLE, texState->view, nullptr);
    }
    if (texState->memory) {
        vkFreeMemory(VK_NULL_HANDLE, texState->memory, nullptr);
    }
    if (texState->image) {
        vkDestroyImage(VK_NULL_HANDLE, texState->image, nullptr);
    }
    delete texState;
}

static AR_result vk_onShaderCreate(AR_device* device, AR_shader_stage stage, 
    const void* code, size_t size, const char*, AR_shader** out) {
    
    if (!device || !device->p_impl || !code || !out) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<VkState_Device*>(device->p_impl->backend_data);
    if (!devState) return AR_result::INVALID_OPERATION;
    
    auto* shState = new VkState_Shader{};
    if (!shState) return AR_result::OUT_OF_MEMORY;
    
    shState->stage = stage;
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = static_cast<const uint32_t*>(code);
    
    if (vkCreateShaderModule(devState->logicalDevice, &createInfo, nullptr, &shState->module) != VK_SUCCESS) {
        delete shState;
        return AR_result::PIPELINE_COMPILE_FAILED;
    }
    
    *out = reinterpret_cast<AR_shader*>(shState);
    return AR_result::SUCCESS;
}

static void vk_onShaderDestroy(AR_shader* shader) {
    if (!shader) return;
    
    auto* shState = reinterpret_cast<VkState_Shader*>(shader);
    if (shState->module) {
        vkDestroyShaderModule(VK_NULL_HANDLE, shState->module, nullptr);
    }
    delete shState;
}

static AR_result vk_onPipelineCreate(AR_device* device, const AR_pipeline_desc* desc, AR_pipeline** out) {
    if (!device || !device->p_impl || !desc || !out) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<VkState_Device*>(device->p_impl->backend_data);
    auto* surfState = static_cast<VkState_Surface*>(device->p_impl->context->p_impl->backend_data);
    if (!devState || !surfState) return AR_result::INVALID_OPERATION;
    
    auto* pipeState = new VkState_Pipeline{};
    if (!pipeState) return AR_result::OUT_OF_MEMORY;
    
    pipeState->topology = desc->topology;
    pipeState->renderPass = surfState->renderPass;
    
    // Shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    
    if (desc->vertex_shader) {
        auto* vs = reinterpret_cast<VkState_Shader*>(desc->vertex_shader);
        VkPipelineShaderStageCreateInfo vsInfo{};
        vsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vsInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vsInfo.module = vs->module;
        vsInfo.pName = "main";
        shaderStages.push_back(vsInfo);
    }
    
    if (desc->fragment_shader) {
        auto* fs = reinterpret_cast<VkState_Shader*>(desc->fragment_shader);
        VkPipelineShaderStageCreateInfo fsInfo{};
        fsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fsInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fsInfo.module = fs->module;
        fsInfo.pName = "main";
        shaderStages.push_back(fsInfo);
    }
    
    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = ar_to_vk_topology(desc->topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(surfState->width);
    viewport.height = static_cast<float>(surfState->height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {surfState->width, surfState->height};
    
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    
    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = ar_to_vk_polygon(desc->rasterizer.polygon_mode);
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = ar_to_vk_cull(desc->rasterizer.cull_mode);
    rasterizer.frontFace = ar_to_vk_front(desc->rasterizer.front_face);
    rasterizer.depthBiasEnable = desc->rasterizer.depth_bias ? VK_TRUE : VK_FALSE;
    rasterizer.depthBiasConstantFactor = desc->rasterizer.depth_bias_constant;
    rasterizer.depthBiasClamp = desc->rasterizer.depth_bias_clamp;
    rasterizer.depthBiasSlopeFactor = desc->rasterizer.depth_bias_slope;
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;
    
    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = desc->blend.enabled ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = ar_to_vk_blend(desc->blend.src_color);
    colorBlendAttachment.dstColorBlendFactor = ar_to_vk_blend(desc->blend.dst_color);
    colorBlendAttachment.colorBlendOp = ar_to_vk_blend_op(desc->blend.color_op);
    colorBlendAttachment.srcAlphaBlendFactor = ar_to_vk_blend(desc->blend.src_alpha);
    colorBlendAttachment.dstAlphaBlendFactor = ar_to_vk_blend(desc->blend.dst_alpha);
    colorBlendAttachment.alphaBlendOp = ar_to_vk_blend_op(desc->blend.alpha_op);
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;
    
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc->depth_stencil.depth_test ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc->depth_stencil.depth_write ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = ar_to_vk_compare(desc->depth_stencil.depth_compare);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = desc->depth_stencil.stencil_test ? VK_TRUE : VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};
    
    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();
    
    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    
    if (vkCreatePipelineLayout(devState->logicalDevice, &pipelineLayoutInfo, nullptr, &pipeState->layout) != VK_SUCCESS) {
        delete pipeState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipeState->layout;
    pipelineInfo.renderPass = surfState->renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    
    if (vkCreateGraphicsPipelines(devState->logicalDevice, devState->pipelineCache, 
        1, &pipelineInfo, nullptr, &pipeState->pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(devState->logicalDevice, pipeState->layout, nullptr);
        delete pipeState;
        return AR_result::PIPELINE_COMPILE_FAILED;
    }
    
    *out = reinterpret_cast<AR_pipeline*>(pipeState);
    return AR_result::SUCCESS;
}

static void vk_onPipelineDestroy(AR_pipeline* pipeline) {
    if (!pipeline) return;
    
    auto* pipeState = reinterpret_cast<VkState_Pipeline*>(pipeline);
    if (pipeState->pipeline) {
        vkDestroyPipeline(VK_NULL_HANDLE, pipeState->pipeline, nullptr);
    }
    if (pipeState->layout) {
        vkDestroyPipelineLayout(VK_NULL_HANDLE, pipeState->layout, nullptr);
    }
    delete pipeState;
}

// --- Command buffer callbacks ---
static AR_result vk_onCmdBufferCreate(AR_device* device, AR_command_buffer** out) {
    if (!device || !device->p_impl || !out) return AR_result::INVALID_ARGS;
    
    auto* devState = static_cast<VkState_Device*>(device->p_impl->backend_data);
    if (!devState) return AR_result::INVALID_OPERATION;
    
    auto* cmdState = new VkState_CmdBuffer{};
    if (!cmdState) return AR_result::OUT_OF_MEMORY;
    
    cmdState->deviceState = devState;
    
    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = devState->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    if (vkAllocateCommandBuffers(devState->logicalDevice, &allocInfo, &cmdState->commandBuffer) != VK_SUCCESS) {
        delete cmdState;
        return AR_result::ERROR_GENERIC;
    }
    
    // Create synchronization primitives
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(devState->logicalDevice, &fenceInfo, nullptr, &cmdState->fence);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(devState->logicalDevice, &semaphoreInfo, nullptr, &cmdState->imageAvailableSemaphore);
    vkCreateSemaphore(devState->logicalDevice, &semaphoreInfo, nullptr, &cmdState->renderFinishedSemaphore);
    
    *out = reinterpret_cast<AR_command_buffer*>(cmdState);
    return AR_result::SUCCESS;
}

static void vk_onCmdBufferDestroy(AR_command_buffer* cmd) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<VkState_CmdBuffer*>(cmd);
    auto* devState = cmdState->deviceState;
    
    if (cmdState->commandBuffer) {
        vkFreeCommandBuffers(devState->logicalDevice, devState->commandPool, 1, &cmdState->commandBuffer);
    }
    if (cmdState->fence) {
        vkDestroyFence(devState->logicalDevice, cmdState->fence, nullptr);
    }
    if (cmdState->renderFinishedSemaphore) {
        vkDestroySemaphore(devState->logicalDevice, cmdState->renderFinishedSemaphore, nullptr);
    }
    if (cmdState->imageAvailableSemaphore) {
        vkDestroySemaphore(devState->logicalDevice, cmdState->imageAvailableSemaphore, nullptr);
    }
    
    delete cmdState;
}

static AR_result vk_onCmdBegin(AR_command_buffer* cmd) {
    if (!cmd) return AR_result::INVALID_ARGS;
    
    auto* cmdState = reinterpret_cast<VkState_CmdBuffer*>(cmd);
    if (cmdState->recording) return AR_result::INVALID_OPERATION;
    
    // Wait for fence before reusing
    vkWaitFences(cmdState->deviceState->logicalDevice, 1, &cmdState->fence, VK_TRUE, UINT64_MAX);
    vkResetFences(cmdState->deviceState->logicalDevice, 1, &cmdState->fence);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    if (vkBeginCommandBuffer(cmdState->commandBuffer, &beginInfo) != VK_SUCCESS) {
        return AR_result::ERROR_GENERIC;
    }
    
    cmdState->recording = true;
    return AR_result::SUCCESS;
}

static AR_result vk_onCmdEnd(AR_command_buffer* cmd) {
    if (!cmd) return AR_result::INVALID_ARGS;
    
    auto* cmdState = reinterpret_cast<VkState_CmdBuffer*>(cmd);
    if (!cmdState->recording) return AR_result::INVALID_OPERATION;
    
    if (vkEndCommandBuffer(cmdState->commandBuffer) != VK_SUCCESS) {
        return AR_result::ERROR_GENERIC;
    }
    
    cmdState->recording = false;
    return AR_result::SUCCESS;
}

static AR_result vk_onCmdExecute(AR_command_buffer*) {
    // Execution is handled in surface present for Vulkan
    return AR_result::SUCCESS;
}

static void vk_onCmdClear(AR_command_buffer* cmd, float r, float g, float b, float a) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<VkState_CmdBuffer*>(cmd);
    if (!cmdState->recording) return;
    
    VkClearValue clearColor{};
    clearColor.color = {{r, g, b, a}};
    
    auto* surfState = static_cast<VkState_Surface*>(cmdState->deviceState->context->p_impl->backend_data);
    
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = surfState->renderPass;
    renderPassInfo.framebuffer = surfState->framebuffers[surfState->imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {surfState->width, surfState->height};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    
    vkCmdBeginRenderPass(cmdState->commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

static void vk_onCmdSetViewport(AR_command_buffer* cmd, float x, float y, float w, float h) {
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<VkState_CmdBuffer*>(cmd);
    if (!cmdState->recording) return;
    
    VkViewport viewport{};
    viewport.x = x;
    viewport.y = y;
    viewport.width = w;
    viewport.height = h;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    vkCmdSetViewport(cmdState->commandBuffer, 0, 1, &viewport);
}

static void vk_onCmdBindPipeline(AR_command_buffer* cmd, AR_pipeline* pipeline) {
    if (!cmd || !pipeline) return;
    
    auto* cmdState = reinterpret_cast<VkState_CmdBuffer*>(cmd);
    auto* pipeState = reinterpret_cast<VkState_Pipeline*>(pipeline);
    if (!cmdState->recording) return;
    
    vkCmdBindPipeline(cmdState->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeState->pipeline);
}

static void vk_onCmdBindMaterial(AR_command_buffer*, AR_material*) {
    // Descriptor set binding would go here
}

static void vk_onCmdDraw(AR_command_buffer* cmd, uint32_t vertexCount, uint32_t instanceCount, 
    uint32_t firstVertex, uint32_t firstInstance) {
    
    if (!cmd) return;
    
    auto* cmdState = reinterpret_cast<VkState_CmdBuffer*>(cmd);
    if (!cmdState->recording) return;
    
    vkCmdDraw(cmdState->commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

static void vk_onCmdDrawIndexed(AR_command_buffer* cmd, AR_buffer* indexBuffer, uint32_t indexCount, 
    uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    
    if (!cmd || !indexBuffer) return;
    
    auto* cmdState = reinterpret_cast<VkState_CmdBuffer*>(cmd);
    auto* idxState = reinterpret_cast<VkState_Buffer*>(indexBuffer);
    if (!cmdState->recording) return;
    
    vkCmdBindIndexBuffer(cmdState->commandBuffer, idxState->buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmdState->commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

// ============================================================================
// Backend registration
// ============================================================================
static AS_bool32 vk_is_available() {
    // Check if Vulkan loader is available
    VkInstance instance;
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result == VK_SUCCESS) {
        vkDestroyInstance(instance, nullptr);
        return AS_TRUE;
    }
    return AS_FALSE;
}

static AR_backend_callbacks g_vk_callbacks = {
    vk_onContextInit,
    vk_onContextUninit,
    vk_onContextEnumerateDevices,
    vk_onContextGetDeviceInfo,
    vk_onDeviceInit,
    vk_onDeviceUninit,
    vk_onDeviceStart,
    vk_onDeviceStop,
    nullptr,  // onDeviceRead
    nullptr,  // onDeviceWrite
    nullptr,  // onDeviceDataLoop
    nullptr,  // onDeviceDataLoopWakeup
    vk_onSurfaceInit,
    vk_onSurfaceUninit,
    vk_onSurfaceResize,
    vk_onSurfacePresent,
    vk_onBufferCreate,
    vk_onBufferDestroy,
    vk_onTextureCreate,
    vk_onTextureDestroy,
    vk_onShaderCreate,
    vk_onShaderDestroy,
    vk_onPipelineCreate,
    vk_onPipelineDestroy,
    vk_onCmdBufferCreate,
    vk_onCmdBufferDestroy,
    vk_onCmdBegin,
    vk_onCmdEnd,
    vk_onCmdExecute,
    vk_onCmdClear,
    vk_onCmdSetViewport,
    vk_onCmdBindPipeline,
    vk_onCmdBindMaterial,
    vk_onCmdDraw,
    vk_onCmdDrawIndexed
};

static AR_backend_info g_vk_info = {
    AR_backend::VULKAN,
    "Vulkan",
    vk_is_available,
    &g_vk_callbacks
};

AR_result AR_register_vulkan_backend() {
    return AR_register_backend(&g_vk_info);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void auto_register_vulkan() {
    AR_register_vulkan_backend();
}
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void __cdecl auto_register_vulkan_msvc() {
    AR_register_vulkan_backend();
}
__declspec(allocate(".CRT$XCU")) void (*__auto_register_vulkan)(void) = auto_register_vulkan_msvc;
#endif

} // namespace arxrender