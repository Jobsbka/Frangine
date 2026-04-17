#include "../include/arxrender_context.hpp"
#include "../include/arxrender_backend.hpp"
#include "../include/arxrender_log.hpp"
#include <mutex>
#include <cstring>
#include <algorithm>

namespace arxrender {
struct AR_context::impl {
    AR_context_config config{};
    AR_backend active_backend = AR_backend::NULL_BACKEND;
    AR_backend_callbacks callbacks{};
    std::vector<AR_backend_info> available_backends;
    bool initialized = false;
    std::mutex mutex;

    AR_result select_backend(const AR_backend* priority_list, uint32_t count) {
        std::vector<AR_backend> priority;
        if (priority_list && count > 0) priority.assign(priority_list, priority_list + count);
        else {
#if defined(_WIN32)
            priority = {AR_backend::VULKAN, AR_backend::OPENGL};
#elif defined(__APPLE__)
            priority = {AR_backend::METAL, AR_backend::OPENGL};
#elif defined(__linux__)
            priority = {AR_backend::VULKAN, AR_backend::OPENGL};
#else
            priority = {AR_backend::OPENGL, AR_backend::NULL_BACKEND};
#endif
            priority.push_back(AR_backend::NULL_BACKEND);
        }
        for (AR_backend target : priority) {
            for (const auto& info : available_backends) {
                if (info.backend == target && info.is_available()) {
                    active_backend = target;
                    callbacks = *info.callbacks;
                    return AR_result::SUCCESS;
                }
            }
        }
        return AR_result::NO_BACKEND;
    }
};

AR_context::AR_context() : p_impl(std::make_unique<impl>()) {}
AR_context::~AR_context() = default;

AR_result AR_context::init(const AR_backend* priority_list, uint32_t count, const AR_context_config* config) {
    if (!p_impl) return AR_result::ERROR;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    if (p_impl->initialized) return AR_result::INVALID_OPERATION;

    if (config) p_impl->config = *config;
    else p_impl->config = {nullptr, nullptr, nullptr, nullptr, false};
    if (!p_impl->config.log) p_impl->config.log = AR_log::default_log();

    {
        auto& global = get_global_state();
        std::lock_guard<std::mutex> glock(global.backend_mutex);
        p_impl->available_backends = global.registered_backends;
    }

    AR_result res = p_impl->select_backend(priority_list, count);
    if (res != AR_result::SUCCESS) return res;

    if (p_impl->callbacks.onContextInit) {
        res = p_impl->callbacks.onContextInit(this, &p_impl->config);
    }
    if (res != AR_result::SUCCESS) return res;
    p_impl->initialized = true;
    return AR_result::SUCCESS;
}

void AR_context::uninit() {
    if (!p_impl) return;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    if (!p_impl->initialized) return;
    if (p_impl->callbacks.onContextUninit) p_impl->callbacks.onContextUninit(this);
    p_impl->initialized = false;
}

AR_backend AR_context::active_backend() const { return p_impl ? p_impl->active_backend : AR_backend::NULL_BACKEND; }
AR_log* AR_context::log() const { return p_impl ? p_impl->config.log : nullptr; }
const AR_backend_callbacks* AR_context::callbacks() const { return p_impl ? &p_impl->callbacks : nullptr; }

const char* AR_context::backend_name(AR_backend backend) {
    auto& global = get_global_state();
    std::lock_guard<std::mutex> lock(global.backend_mutex);
    for (const auto& info : global.registered_backends) if (info.backend == backend) return info.name;
    switch (backend) {
        case AR_backend::NULL_BACKEND: return "Null";
        case AR_backend::OPENGL: return "OpenGL";
        case AR_backend::VULKAN: return "Vulkan";
        case AR_backend::METAL: return "Metal";
        default: return "Unknown";
    }
}

AR_result AR_context::backend_from_name(const char* name, AR_backend* out) {
    if (!name || !out) return AR_result::INVALID_ARGS;
    auto& global = get_global_state();
    std::lock_guard<std::mutex> lock(global.backend_mutex);
    for (const auto& info : global.registered_backends) {
        if (std::strcmp(name, info.name) == 0) { *out = info.backend; return AR_result::SUCCESS; }
    }
    return AR_result::NO_BACKEND;
}
} // namespace arxrender