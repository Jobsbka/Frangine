// src/backends/backend_null.cpp
#include "../include/arxsound_backend.hpp"
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <cstring>
#include <cmath>

namespace arxsound {

// ============================================================================
// Null backend internal structures
// ============================================================================
struct null_backend_device_state {
    std::atomic<bool> running{false};
    std::thread worker_thread;
    
    // Параметры устройства
    AS_device_type type{AS_device_type::PLAYBACK};
    uint32_t sample_rate{48000};
    uint32_t playback_channels{2};
    uint32_t capture_channels{2};
    AS_format playback_format{AS_format::F32};
    AS_format capture_format{AS_format::F32};
    
    // Callbacks
    AS_device_data_proc data_callback{nullptr};
    AS_device_notification_proc notification_callback{nullptr};
    void* user_data{nullptr};
    
    // Буферизация
    uint32_t period_frames{256};
    uint32_t period_count{4};
    
    // Счётчики для эмуляции
    std::atomic<uint64_t> frames_played{0};
    std::atomic<uint64_t> frames_captured{0};
    
    // Для duplex: синхронизация
    std::mutex duplex_mutex;
};

// ============================================================================
// Worker loop implementation
// ============================================================================
static void null_backend_worker(null_backend_device_state* state) {
    if (!state) return;
    
    const size_t sample_bytes_playback = AS_bytes_per_sample(state->playback_format);
    const size_t sample_bytes_capture = AS_bytes_per_sample(state->capture_format);
    
    if (sample_bytes_playback == 0) return;
    
    // Выделяем буферы
    std::vector<uint8_t> playback_buffer;
    std::vector<uint8_t> capture_buffer;
    
    if (state->type == AS_device_type::PLAYBACK || state->type == AS_device_type::DUPLEX) {
        size_t pb_size = sample_bytes_playback * state->playback_channels * state->period_frames;
        playback_buffer.resize(pb_size);
    }
    if (state->type == AS_device_type::CAPTURE || state->type == AS_device_type::DUPLEX) {
        size_t cap_size = sample_bytes_capture * state->capture_channels * state->period_frames;
        capture_buffer.resize(cap_size);
        // Инициализируем тишиной
        memset(capture_buffer.data(), 0, cap_size);
    }
    
    // Главный цикл
    while (state->running.load(std::memory_order_acquire)) {
        // Вызываем callback если есть
        if (state->data_callback) {
            void* output_ptr = playback_buffer.empty() ? nullptr : playback_buffer.data();
            const void* input_ptr = capture_buffer.empty() ? nullptr : capture_buffer.data();
            
            state->data_callback(
                nullptr,  // device not passed to null backend
                output_ptr,
                input_ptr,
                state->period_frames
            );
        }
        
        // Обновляем счётчики
        if (state->type == AS_device_type::PLAYBACK || state->type == AS_device_type::DUPLEX) {
            state->frames_played.fetch_add(state->period_frames, std::memory_order_relaxed);
        }
        if (state->type == AS_device_type::CAPTURE || state->type == AS_device_type::DUPLEX) {
            state->frames_captured.fetch_add(state->period_frames, std::memory_order_relaxed);
        }
        
        // Эмуляция задержки на основе sample_rate
        // Период в миллисекундах = (период в фреймах * 1000) / sample_rate
        const uint32_t period_ms = (state->period_frames * 1000) / state->sample_rate;
        
        // Используем более точный сон если возможно
        auto sleep_duration = std::chrono::milliseconds(period_ms);
        auto sleep_ns = (state->period_frames * 1000000000ULL) / state->sample_rate;
        auto remainder_ns = sleep_ns - (period_ms * 1000000ULL);
        
        std::this_thread::sleep_for(sleep_duration);
        if (remainder_ns > 0) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(remainder_ns));
        }
    }
}

// ============================================================================
// Backend callbacks implementation
// ============================================================================

// --- Context callbacks ---

static AS_result null_on_context_init(
    AS_context* context,
    const AS_context_config* config,
    AS_backend_callbacks* callbacks
) {
    (void)context;
    (void)config;
    (void)callbacks;
    // Null backend не требует инициализации
    return AS_result::SUCCESS;
}

static AS_result null_on_context_uninit(AS_context* context) {
    (void)context;
    return AS_result::SUCCESS;
}

static AS_result null_on_context_enumerate_devices(
    AS_context* context,
    AS_device_type type,
    AS_enumerate_devices_callback callback,
    void* user_data
) {
    if (!callback) return AS_result::INVALID_ARGS;
    
    AS_device_info info{};
    
    // Заполняем базовую информацию
    const char* type_str = (type == AS_device_type::PLAYBACK) ? "Playback" : "Capture";
    snprintf(info.name, sizeof(info.name), "Null %s Device", type_str);
    info.is_default = AS_TRUE;
    
    // Идентификатор устройства (для null — просто строка)
    snprintf(info.id.null_backend, sizeof(info.id.null_backend), "null_%s_0", 
             (type == AS_device_type::PLAYBACK) ? "playback" : "capture");
    
    // Поддерживаемые параметры
    info.min_sample_rate = 8000;
    info.max_sample_rate = 192000;
    info.min_channels = 1;
    info.max_channels = 8;
    info.supports_shared = AS_TRUE;
    info.supports_exclusive = AS_FALSE;
    
    // Нативные форматы (предпочитаем F32)
    info.native_format_count = 0;
    
    auto add_format = [&](AS_format fmt, uint32_t ch, uint32_t sr) {
        if (info.native_format_count < AS_device_info::MAX_NATIVE_FORMATS) {
            info.native_formats[info.native_format_count++] = {fmt, ch, sr};
        }
    };
    
    // Добавляем популярные комбинации
    add_format(AS_format::F32, 1, 48000);
    add_format(AS_format::F32, 2, 48000);
    add_format(AS_format::F32, 2, 44100);
    add_format(AS_format::S16, 2, 44100);
    add_format(AS_format::S16, 2, 48000);
    
    // Вызываем callback пользователя
    return callback(context, type, &info, user_data) ? AS_result::SUCCESS : AS_result::ERROR;
}

static AS_result null_on_context_get_device_info(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    AS_device_info* info
) {
    (void)device_id;
    // Возвращаем те же данные что и при enumeration
    return null_on_context_enumerate_devices(context, type,
        [](AS_context*, AS_device_type, const AS_device_info* src, void* dst) {
            *static_cast<AS_device_info*>(dst) = *src;
            return AS_TRUE;
        }, info);
}

// --- Device callbacks ---

static AS_result null_on_device_init(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    const AS_device_config* config,
    AS_device* device
) {
    if (!context || !config || !device) {
        return AS_result::INVALID_ARGS;
    }
    
    // Создаём состояние устройства
    auto* state = new(std::nothrow) null_backend_device_state();
    if (!state) {
        return AS_result::OUT_OF_MEMORY;
    }
    
    state->type = type;
    state->sample_rate = config->sample_rate ? config->sample_rate : 48000;
    state->playback_channels = config->playback.channels ? config->playback.channels : 2;
    state->capture_channels = config->capture.channels ? config->capture.channels : 2;
    state->playback_format = (config->playback.format != AS_format::UNKNOWN) 
        ? config->playback.format : AS_format::F32;
    state->capture_format = (config->capture.format != AS_format::UNKNOWN)
        ? config->capture.format : AS_format::F32;
    
    state->data_callback = config->data_callback;
    state->notification_callback = config->notification_callback;
    state->user_data = config->user_data;
    
    state->period_frames = config->period_size_in_frames 
        ? config->period_size_in_frames : 256;
    state->period_count = config->period_count ? config->period_count : 4;
    
    // Ограничиваем period_frames разумными значениями
    if (state->period_frames < 64) state->period_frames = 64;
    if (state->period_frames > 8192) state->period_frames = 8192;
    
    // Сохраняем state в opaque pointer устройства
    // В реальной реализации нужен механизм для хранения backend-specific data
    // Здесь используем reinterpret_cast для простоты
    device->p_impl->backend_data = static_cast<void*>(state);
    
    // Обновляем runtime параметры устройства
    device->p_impl->actual_sample_rate = state->sample_rate;
    device->p_impl->actual_playback_format = state->playback_format;
    device->p_impl->actual_playback_channels = state->playback_channels;
    device->p_impl->actual_capture_format = state->capture_format;
    device->p_impl->actual_capture_channels = state->capture_channels;
    
    // Копируем дескрипторы
    device->p_impl->playback_descriptor = device->p_impl->config.playback.device_id 
        ? *static_cast<const AS_device_descriptor*>(device->p_impl->config.playback.device_id)
        : AS_device_descriptor{};
    device->p_impl->capture_descriptor = device->p_impl->config.capture.device_id
        ? *static_cast<const AS_device_descriptor*>(device->p_impl->config.capture.device_id)
        : AS_device_descriptor{};
    
    return AS_result::SUCCESS;
}

static AS_result null_on_device_uninit(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    // Останавливаем если нужно
    if (device->p_impl->started.load()) {
        null_on_device_stop(device);
    }
    
    // Освобождаем состояние
    auto* state = static_cast<null_backend_device_state*>(device->p_impl->backend_data);
    if (state) {
        delete state;
        device->p_impl->backend_data = nullptr;
    }
    
    return AS_result::SUCCESS;
}

static AS_result null_on_device_start(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<null_backend_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::INVALID_OPERATION;
    
    if (state->running.load()) {
        return AS_result::INVALID_OPERATION;  // Уже запущено
    }
    
    // Сбрасываем счётчики
    state->frames_played.store(0, std::memory_order_relaxed);
    state->frames_captured.store(0, std::memory_order_relaxed);
    
    // Запускаем воркер
    state->running.store(true, std::memory_order_release);
    
    try {
        state->worker_thread = std::thread(null_backend_worker, state);
    } catch (...) {
        state->running.store(false, std::memory_order_release);
        return AS_result::ERROR;
    }
    
    // Уведомляем о старте если есть callback
    if (state->notification_callback) {
        state->notification_callback(
            device,
            static_cast<int>(AS_device_notification_type::STARTED),
            state->user_data
        );
    }
    
    return AS_result::SUCCESS;
}

static AS_result null_on_device_stop(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<null_backend_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::INVALID_OPERATION;
    
    if (!state->running.load()) {
        return AS_result::SUCCESS;  // Уже остановлено
    }
    
    // Останавливаем воркер
    state->running.store(false, std::memory_order_release);
    
    if (state->worker_thread.joinable()) {
        state->worker_thread.join();
    }
    
    // Уведомляем об остановке если есть callback
    if (state->notification_callback) {
        state->notification_callback(
            device,
            static_cast<int>(AS_device_notification_type::STOPPED),
            state->user_data
        );
    }
    
    return AS_result::SUCCESS;
}

// Опциональные read/write для blocking backends (null не использует)
static AS_result null_on_device_read(
    AS_device* device,
    void* frames,
    uint32_t frame_count,
    uint32_t* frames_read
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_read) *frames_read = 0;
    return AS_result::NOT_IMPLEMENTED;  // Null backend callback-based
}

static AS_result null_on_device_write(
    AS_device* device,
    const void* frames,
    uint32_t frame_count,
    uint32_t* frames_written
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_written) *frames_written = 0;
    return AS_result::NOT_IMPLEMENTED;  // Null backend callback-based
}

// ============================================================================
// Backend registration
// ============================================================================
static AS_bool32 null_is_available() {
    // Null backend всегда доступен — не требует внешних зависимостей
    return AS_TRUE;
}

static AS_backend_callbacks g_null_callbacks = {
    null_on_context_init,
    null_on_context_uninit,
    null_on_context_enumerate_devices,
    null_on_context_get_device_info,
    null_on_device_init,
    null_on_device_uninit,
    null_on_device_start,
    null_on_device_stop,
    null_on_device_read,
    null_on_device_write,
    nullptr,  // on_device_data_loop
    nullptr   // on_device_data_loop_wakeup
};

static AS_backend_info g_null_info = {
    AS_backend::NULL_BACKEND,
    "Null",
    null_is_available,
    g_null_callbacks
};

// Публичная функция регистрации (может вызываться при старте библиотеки)
AS_result AS_register_null_backend() {
    return AS_register_backend(&g_null_info);
}

// Авто-регистрация при загрузке (если используется статическая линковка)
#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void auto_register_null_backend() {
    AS_register_null_backend();
}
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void __cdecl auto_register_null_backend_msvc() {
    AS_register_null_backend();
}
__declspec(allocate(".CRT$XCU")) void (*__auto_register_null)(void) = auto_register_null_backend_msvc;
#endif

} // namespace arxsound