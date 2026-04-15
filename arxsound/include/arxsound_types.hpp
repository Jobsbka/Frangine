#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace arxsound {

// ============================================================================
// Результат операций
// ============================================================================
enum class AS_result : int32_t {
    SUCCESS = 0,
    
    // Общие ошибки
    ERROR = -1,
    INVALID_ARGS = -2,
    INVALID_OPERATION = -3,
    
    // Бэкенд-специфичные
    NO_BACKEND = -4,
    DEVICE_NOT_AVAILABLE = -5,
    API_NOT_FOUND = -6,
    
    // Ресурсы
    OUT_OF_MEMORY = -7,
    NOT_IMPLEMENTED = -8,
    
    // Асинхронные состояния
    BUSY = -9,
    AT_END = -10,
    
    // Форматы
    FORMAT_NOT_SUPPORTED = -11,
    CHANNELS_NOT_SUPPORTED = -12,
    SAMPLE_RATE_NOT_SUPPORTED = -13
};

constexpr bool AS_result_is_success(AS_result r) { return r == AS_result::SUCCESS; }
constexpr bool AS_result_is_error(AS_result r) { return static_cast<int>(r) < 0; }

// ============================================================================
// Логические типы
// ============================================================================
using AS_bool32 = int32_t;
constexpr AS_bool32 AS_TRUE = 1;
constexpr AS_bool32 AS_FALSE = 0;

// ============================================================================
// Аудиоформаты
// ============================================================================
enum class AS_format : uint32_t {
    UNKNOWN = 0,
    U8 = 1,      // 8-bit unsigned
    S16 = 2,     // 16-bit signed
    S24 = 3,     // 24-bit signed (packed)
    S32 = 4,     // 32-bit signed
    F32 = 5      // 32-bit float [-1, 1]
};

constexpr uint32_t AS_bytes_per_sample(AS_format format) {
    switch (format) {
        case AS_format::U8:  return 1;
        case AS_format::S16: return 2;
        case AS_format::S24: return 3;
        case AS_format::S32:
        case AS_format::F32: return 4;
        default: return 0;
    }
}

// ============================================================================
// Типы устройств
// ============================================================================
enum class AS_device_type : uint32_t {
    PLAYBACK = 1,
    CAPTURE = 2,
    DUPLEX = 3,      // playback + capture синхронно
    LOOPBACK = 4     // захват вывода (platform-specific)
};

// ============================================================================
// Состояния устройства
// ============================================================================
enum class AS_device_state : uint32_t {
    UNINITIALIZED = 0,
    STOPPED = 1,
    STARTING = 2,
    STARTED = 3,
    STOPPING = 4
};

// ============================================================================
// Режимы разделения (share mode)
// ============================================================================
enum class AS_share_mode : uint32_t {
    SHARED = 0,   // разделяемый доступ (рекомендуется)
    EXCLUSIVE = 1 // эксклюзивный доступ (низкая задержка)
};

// ============================================================================
// Профили производительности
// ============================================================================
enum class AS_performance_profile : uint32_t {
    CONSERVATIVE = 0,  // стабильность > задержка
    LOW_LATENCY = 1,   // задержка > стабильность
};

// ============================================================================
// Каналы и канал-мапы
// ============================================================================
using AS_channel = uint8_t;

enum class AS_channel_position : uint8_t {
    UNKNOWN = 0,
    MONO = 1,
    FRONT_LEFT = 2,
    FRONT_RIGHT = 3,
    FRONT_CENTER = 4,
    LFE = 5,
    BACK_LEFT = 6,
    BACK_RIGHT = 7,
    FRONT_LEFT_CENTER = 8,
    FRONT_RIGHT_CENTER = 9,
    BACK_CENTER = 10,
    SIDE_LEFT = 11,
    SIDE_RIGHT = 12,
    TOP_CENTER = 13,
    TOP_FRONT_LEFT = 14,
    TOP_FRONT_CENTER = 15,
    TOP_FRONT_RIGHT = 16,
    TOP_BACK_LEFT = 17,
    TOP_BACK_CENTER = 18,
    TOP_BACK_RIGHT = 19
};

// Стандартные канал-мапы
enum class AS_standard_channel_map : uint32_t {
    DEFAULT = 0,
    WEBCOMP = 1,    // WebRTC-style
    ALSA = 2,
    PULSEAUDIO = 3,
    COREAUDIO = 4,
    WAV = 5
};

// ============================================================================
// Уровни логирования
// ============================================================================
enum class AS_log_level : uint32_t {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

// ============================================================================
// Бэкенды
// ============================================================================
enum class AS_backend : uint32_t {
    NULL_BACKEND = 0,
    
#if defined(_WIN32)
    WASAPI = 1,
    DSOUND = 2,
    WINMM = 3,
#endif

#if defined(__linux__) && !defined(__ANDROID__)
    ALSA = 10,
    PULSEAUDIO = 11,
    JACK = 12,
#endif

#if defined(__APPLE__)
    COREAUDIO = 20,
#endif

#if defined(__ANDROID__)
    AAUDIO = 30,
    OPENSL = 31,
#endif

#if defined(__EMSCRIPTEN__)
    WEBAUDIO = 40,
#endif

    CUSTOM = 250,
    COUNT = 251
};

// ============================================================================
// Приоритеты потоков
// ============================================================================
enum class AS_thread_priority : int32_t {
    IDLE = -5,
    LOWEST = -4,
    LOW = -3,
    NORMAL = 0,
    HIGH = 3,
    HIGHEST = 4,
    REALTIME = 5
};

// ============================================================================
// Callback-типы
// ============================================================================
struct AS_device;  // forward declaration

using AS_device_data_proc = void (*)(AS_device* device, void* output, const void* input, uint32_t frame_count);
using AS_device_notification_proc = void (*)(AS_device* device, int notification_type, void* user_data);

enum class AS_device_notification_type : int32_t {
    STARTED = 1,
    STOPPED = 2,
    REROUTED = 3,
    INTERRUPTION_BEGAN = 4,
    INTERRUPTION_ENDED = 5
};

// ============================================================================
// Аллокаторы памяти
// ============================================================================
struct AS_allocation_callbacks {
    void* user_data = nullptr;
    void* (*on_alloc)(size_t size, void* user_data) = nullptr;
    void* (*on_realloc)(void* ptr, size_t size, void* user_data) = nullptr;
    void  (*on_free)(void* ptr, void* user_data) = nullptr;
};

constexpr AS_allocation_callbacks AS_allocation_callbacks_init_default() {
    return AS_allocation_callbacks{
        nullptr,
        [](size_t sz, void*) { return ::operator new(sz); },
        [](void* p, size_t sz, void*) { return ::operator realloc(p, sz); },
        [](void* p, void*) { ::operator delete(p); }
    };
}

// ============================================================================
// Вспомогательные макросы для API
// ============================================================================
#ifndef AS_API
    #if defined(_WIN32) || defined(__CYGWIN__)
        #ifdef AS_BUILD_SHARED
            #define AS_API __declspec(dllexport)
        #else
            #define AS_API __declspec(dllimport)
        #endif
    #elif defined(__GNUC__) && __GNUC__ >= 4
        #define AS_API __attribute__((visibility("default")))
    #else
        #define AS_API
    #endif
#endif

#ifndef AS_INLINE
    #ifdef __cplusplus
        #define AS_INLINE inline
    #else
        #define AS_INLINE static inline
    #endif
#endif

} // namespace arxsound