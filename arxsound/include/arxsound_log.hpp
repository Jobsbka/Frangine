#pragma once

#include "arxsound_types.hpp"
#include <string>
#include <functional>
#include <mutex>

namespace arxsound {

// ============================================================================
// Callback для логирования
// ============================================================================
struct AS_log_message {
    AS_log_level level;
    const char* message;
    size_t message_length;
    void* user_data;  // из конфига лога
};

using AS_log_callback = std::function<void(const AS_log_message&)>;

// ============================================================================
// Конфигурация лога
// ============================================================================
struct AS_log_config {
    AS_allocation_callbacks allocation_callbacks = AS_allocation_callbacks_init_default();
    AS_log_level min_level = AS_log_level::INFO;
    AS_log_callback callback = nullptr;
    void* user_data = nullptr;
};

constexpr AS_log_config AS_log_config_init() {
    return AS_log_config{};
}

// ============================================================================
// Объект лога
// ============================================================================
class AS_log {
public:
    AS_log();
    ~AS_log();
    
    AS_result init(const AS_log_config* config = nullptr);
    void uninit();
    
    // Постинг сообщения
    void post(AS_log_level level, const char* message, ...);
    void post_v(AS_log_level level, const char* format, va_list args);
    
    // Геттеры
    AS_log_level min_level() const;
    void set_min_level(AS_log_level level);
    
    // Статический дефолтный лог
    static AS_log* default_log();
    
private:
    mutable std::mutex m_mutex;
    AS_log_config m_config;
    bool m_initialized = false;
};

// ============================================================================
// Макросы для удобного логирования
// ============================================================================
#define AS_LOG_POST(log, level, ...) \
    do { \
        if (log) log->post(level, __VA_ARGS__); \
    } while(0)

#define AS_LOG_DEBUG(log, ...) AS_LOG_POST(log, AS_log_level::DEBUG, __VA_ARGS__)
#define AS_LOG_INFO(log, ...)  AS_LOG_POST(log, AS_log_level::INFO, __VA_ARGS__)
#define AS_LOG_WARNING(log, ...) AS_LOG_POST(log, AS_log_level::WARNING, __VA_ARGS__)
#define AS_LOG_ERROR(log, ...) AS_LOG_POST(log, AS_log_level::ERROR, __VA_ARGS__)

} // namespace arxsound