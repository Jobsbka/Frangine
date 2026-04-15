#pragma once

#include "arxsound_types.hpp"
#include "arxsound_log.hpp"
#include <vector>
#include <string>
#include <memory>

namespace arxsound {

// ============================================================================
// Информация об устройстве
// ============================================================================
struct AS_device_info {
    // Идентификатор устройства (platform-specific)
    union {
        char null_backend[64];
#if defined(_WIN32)
        wchar_t wasapi[256];
        GUID dsound;
        uint32_t winmm;
#endif
#if defined(__linux__)
        char alsa[64];
        char pulse[64];
        char jack[64];
#endif
#if defined(__APPLE__)
        char coreaudio[256];
#endif
#if defined(__ANDROID__)
        int32_t aaudio;
        void* opensl;
#endif
#if defined(__EMSCRIPTEN__)
        int32_t webaudio;
#endif
        char custom[64];
    } id;
    
    char name[256];           // Человекочитаемое имя
    AS_bool32 is_default;     // Устройство по умолчанию
    
    // Нативные форматы устройства
    struct native_format {
        AS_format format;
        uint32_t channels;
        uint32_t sample_rate;
    };
    
    static constexpr size_t MAX_NATIVE_FORMATS = 32;
    native_format native_formats[MAX_NATIVE_FORMATS];
    uint32_t native_format_count;
    
    // Минимальные/максимальные параметры
    uint32_t min_sample_rate;
    uint32_t max_sample_rate;
    uint32_t min_channels;
    uint32_t max_channels;
    
    // Поддерживаемые share modes
    AS_bool32 supports_shared;
    AS_bool32 supports_exclusive;
};

// ============================================================================
// Конфигурация контекста
// ============================================================================
struct AS_context_config {
    AS_log* log = nullptr;
    AS_thread_priority thread_priority = AS_thread_priority::NORMAL;
    size_t thread_stack_size = 0;  // 0 = default
    void* user_data = nullptr;
    AS_allocation_callbacks allocation_callbacks = AS_allocation_callbacks_init_default();
    
    // Backend-specific config (opaque pointers)
    struct {
#if defined(_WIN32)
        void* wasapi_hwnd;  // HWND для WASAPI
        void* dsound_hwnd;  // HWND для DirectSound
#endif
    } backend;
};

constexpr AS_context_config AS_context_config_init() {
    return AS_context_config{};
}

// ============================================================================
// Callbacks для backend enumeration
// ============================================================================
using AS_enumerate_devices_callback = AS_bool32 (*)(
    void* context,
    AS_device_type type,
    const AS_device_info* info,
    void* user_data
);

// ============================================================================
// Основной контекст
// ============================================================================
class AS_context {
public:
    AS_context();
    ~AS_context();
    
    // Инициализация
    AS_result init(
        const AS_backend* backends = nullptr,
        uint32_t backend_count = 0,
        const AS_context_config* config = nullptr
    );
    
    void uninit();
    
    // Enumeration устройств
    AS_result enumerate_devices(
        AS_device_type type,
        AS_enumerate_devices_callback callback,
        void* user_data
    ) const;
    
    // Получение информации об устройстве
    AS_result get_device_info(
        AS_device_type type,
        const void* device_id,  // из AS_device_info::id
        AS_device_info* info
    ) const;
    
    // Утилиты
    AS_log* log() const;
    const AS_allocation_callbacks& allocation_callbacks() const;
    AS_backend active_backend() const;
    
    // Статические хелперы
    static const char* backend_name(AS_backend backend);
    static AS_result backend_from_name(const char* name, AS_backend* out);
    
private:
    // Внутренняя реализация (pImpl)
    struct impl;
    std::unique_ptr<impl> p_impl;
};

} // namespace arxsound