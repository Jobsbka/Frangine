#pragma once
#include "arxrender.hpp"
#include <mutex>
#include <memory>
#include <cstdarg>

namespace arxrender {

enum class AR_log_level : int32_t { DEBUG, INFO, WARNING, ERROR };
struct AR_log_message {
    AR_log_level level;
    const char* message;
    size_t length;
    void* user_data;
};

using AR_log_callback = void(*)(const AR_log_message&);
struct AR_log_config {
    AR_log_level min_level = AR_log_level::INFO;
    AR_log_callback callback = nullptr;
    void* user_data = nullptr;
    void* alloc_user_data = nullptr;
    void* (*on_alloc)(size_t, void*) = nullptr;
    void* (*on_realloc)(void*, size_t, void*) = nullptr;
    void (*on_free)(void*, void*) = nullptr;
};

class AR_log {
public:
    AR_API AR_log();
    AR_API ~AR_log();
    AR_API AR_result init(const AR_log_config* config = nullptr);
    AR_API void uninit();
    AR_API void post(AR_log_level level, const char* format, ...);
    AR_API void post_v(AR_log_level level, const char* format, va_list args);
    AR_API AR_log_level min_level() const;
    AR_API void set_min_level(AR_log_level level);
    AR_API static AR_log* default_log();

private:
    struct impl;
    std::unique_ptr<impl> p_impl;
};

} // namespace arxrender