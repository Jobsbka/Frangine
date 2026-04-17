#include "../include/arxrender_log.hpp"
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace arxrender {

struct AR_log::impl {
    AR_log_config config{};
    std::mutex mutex;
    bool initialized = false;

    void post_internal(AR_log_level level, const char* format, va_list args) {
        if (level < config.min_level) return;
        std::lock_guard<std::mutex> lock(mutex);
        char buffer[2048];
        vsnprintf(buffer, sizeof(buffer), format, args);
        AR_log_message msg{level, buffer, strlen(buffer), config.user_data};
        if (config.callback) {
            config.callback(msg);
        } else {
            const char* lvl = "INFO";
            switch (level) {
                case AR_log_level::DEBUG: lvl = "DEBUG"; break;
                case AR_log_level::INFO: lvl = "INFO"; break;
                case AR_log_level::WARNING: lvl = "WARNING"; break;
                case AR_log_level::ERROR: lvl = "ERROR"; break;
            }
            std::fprintf(stderr, "[ArxRender/%s] %s\n", lvl, buffer);
        }
    }
};

AR_log::AR_log() : p_impl(std::make_unique<impl>()) {}
AR_log::~AR_log() = default;

AR_result AR_log::init(const AR_log_config* config) {
    if (!p_impl) return AR_result::ERROR;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    if (p_impl->initialized) return AR_result::INVALID_OPERATION;
    if (config) p_impl->config = *config;
    else {
        p_impl->config = {AR_log_level::INFO, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    }
    p_impl->initialized = true;
    return AR_result::SUCCESS;
}

void AR_log::uninit() {
    if (p_impl) {
        std::lock_guard<std::mutex> lock(p_impl->mutex);
        p_impl->initialized = false;
    }
}

void AR_log::post(AR_log_level level, const char* format, ...) {
    if (!p_impl || !p_impl->initialized) return;
    va_list args; va_start(args, format);
    p_impl->post_internal(level, format, args);
    va_end(args);
}

void AR_log::post_v(AR_log_level level, const char* format, va_list args) {
    if (p_impl && p_impl->initialized) p_impl->post_internal(level, format, args);
}

AR_log_level AR_log::min_level() const {
    return (p_impl && p_impl->initialized) ? p_impl->config.min_level : AR_log_level::INFO;
}

void AR_log::set_min_level(AR_log_level level) {
    if (p_impl) {
        std::lock_guard<std::mutex> lock(p_impl->mutex);
        p_impl->config.min_level = level;
    }
}

// Глобальный дефолтный лог
static std::unique_ptr<AR_log> g_default_log;
static std::mutex g_log_mutex;
AR_log* AR_log::default_log() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!g_default_log) {
        g_default_log = std::make_unique<AR_log>();
        g_default_log->init();
    }
    return g_default_log.get();
}

} // namespace arxrender