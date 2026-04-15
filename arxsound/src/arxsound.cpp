// src/arxsound.cpp
#include "../include/arxsound.hpp"
#include <atomic>
#include <mutex>
#include <memory>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <algorithm>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <mmdeviceapi.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
    #include <dlfcn.h>
    #include <pthread.h>
#endif

namespace arxsound {

// ============================================================================
// Глобальное состояние
// ============================================================================
struct GlobalState {
    std::atomic<bool> initialized{false};
    AS_log default_log;
    std::mutex init_mutex;
    std::vector<AS_backend_info> registered_backends;
    std::mutex backend_mutex;
};

static GlobalState& get_global_state() {
    static GlobalState state;
    return state;
}

// ============================================================================
// Вспомогательные функции
// ============================================================================
static AS_result copy_allocation_callbacks(
    AS_allocation_callbacks* dst,
    const AS_allocation_callbacks* src
) {
    if (!dst || !src) return AS_result::INVALID_ARGS;
    *dst = *src;
    return AS_result::SUCCESS;
}

static void* default_alloc(size_t size, void* user_data) {
    (void)user_data;
    return ::operator new(size);
}

static void* default_realloc(void* ptr, size_t size, void* user_data) {
    (void)user_data;
    return ::operator realloc(ptr, size);
}

static void default_free(void* ptr, void* user_data) {
    (void)user_data;
    ::operator delete(ptr);
}

static AS_allocation_callbacks get_default_allocators() {
    AS_allocation_callbacks cb{};
    cb.user_data = nullptr;
    cb.on_alloc = default_alloc;
    cb.on_realloc = default_realloc;
    cb.on_free = default_free;
    return cb;
}

// ============================================================================
// AS_log implementation
// ============================================================================
struct AS_log::impl {
    AS_log_config config;
    std::mutex mutex;
    bool initialized = false;
    
    void post_internal(AS_log_level level, const char* format, va_list args) {
        if (level < config.min_level) return;
        
        std::lock_guard<std::mutex> lock(mutex);
        
        char buffer[2048];
        vsnprintf(buffer, sizeof(buffer), format, args);
        
        AS_log_message msg{};
        msg.level = level;
        msg.message = buffer;
        msg.message_length = strlen(buffer);
        msg.user_data = config.user_data;
        
        if (config.callback) {
            config.callback(msg);
        } else {
            // Fallback to stderr
            const char* level_str = nullptr;
            switch (level) {
                case AS_log_level::DEBUG: level_str = "DEBUG"; break;
                case AS_log_level::INFO: level_str = "INFO"; break;
                case AS_log_level::WARNING: level_str = "WARNING"; break;
                case AS_log_level::ERROR: level_str = "ERROR"; break;
            }
            fprintf(stderr, "[ArxSound/%s] %s\n", level_str, buffer);
        }
    }
};

AS_log::AS_log() : p_impl(std::make_unique<impl>()) {}
AS_log::~AS_log() = default;

AS_result AS_log::init(const AS_log_config* config) {
    if (!p_impl) return AS_result::ERROR;
    
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    
    if (p_impl->initialized) {
        return AS_result::INVALID_OPERATION;
    }
    
    if (config) {
        p_impl->config = *config;
    } else {
        p_impl->config = AS_log_config_init();
    }
    
    // Убедимся, что аллокаторы установлены
    if (!p_impl->config.allocation_callbacks.on_alloc) {
        p_impl->config.allocation_callbacks = get_default_allocators();
    }
    
    p_impl->initialized = true;
    return AS_result::SUCCESS;
}

void AS_log::uninit() {
    if (!p_impl) return;
    
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    p_impl->initialized = false;
}

void AS_log::post(AS_log_level level, const char* format, ...) {
    if (!p_impl || !p_impl->initialized) return;
    
    va_list args;
    va_start(args, format);
    p_impl->post_internal(level, format, args);
    va_end(args);
}

void AS_log::post_v(AS_log_level level, const char* format, va_list args) {
    if (!p_impl || !p_impl->initialized) return;
    p_impl->post_internal(level, format, args);
}

AS_log_level AS_log::min_level() const {
    if (!p_impl) return AS_log_level::INFO;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    return p_impl->config.min_level;
}

void AS_log::set_min_level(AS_log_level level) {
    if (!p_impl) return;
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    p_impl->config.min_level = level;
}

AS_log* AS_log::default_log() {
    auto& global = get_global_state();
    std::lock_guard<std::mutex> lock(global.init_mutex);
    
    if (!global.initialized.load()) {
        global.default_log.init();
        global.initialized.store(true);
    }
    return &global.default_log;
}

// ============================================================================
// AS_context::impl
// ============================================================================
struct AS_context::impl {
    AS_context_config config;
    AS_backend active_backend{AS_backend::NULL_BACKEND};
    AS_backend_callbacks backend_callbacks{};
    std::vector<AS_backend_info> available_backends;
    bool initialized = false;
    std::mutex mutex;
    
    AS_result select_backend(const AS_backend* backends, uint32_t backend_count) {
        // Если список пуст — используем дефолтный порядок
        std::vector<AS_backend> priority;
        if (backends && backend_count > 0) {
            priority.assign(backends, backends + backend_count);
        } else {
            // Дефолтный порядок по платформе
        #if defined(_WIN32)
            priority = {AS_backend::WASAPI, AS_backend::DSOUND, AS_backend::WINMM};
        #elif defined(__APPLE__)
            priority = {AS_backend::COREAUDIO};
        #elif defined(__ANDROID__)
            priority = {AS_backend::AAUDIO, AS_backend::OPENSL};
        #elif defined(__linux__)
            priority = {AS_backend::ALSA, AS_backend::PULSEAUDIO, AS_backend::JACK};
        #elif defined(__EMSCRIPTEN__)
            priority = {AS_backend::WEBAUDIO};
        #endif
            priority.push_back(AS_backend::NULL_BACKEND);
        }
        
        // Проверяем доступность
        for (AS_backend backend : priority) {
            for (const auto& info : available_backends) {
                if (info.backend == backend && info.is_available()) {
                    active_backend = backend;
                    backend_callbacks = info.callbacks;
                    return AS_result::SUCCESS;
                }
            }
        }
        return AS_result::NO_BACKEND;
    }
};

AS_context::AS_context() : p_impl(std::make_unique<impl>()) {}
AS_context::~AS_context() = default;

AS_result AS_context::init(
    const AS_backend* backends,
    uint32_t backend_count,
    const AS_context_config* config
) {
    if (!p_impl) return AS_result::ERROR;
    
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    
    if (p_impl->initialized) {
        return AS_result::INVALID_OPERATION;
    }
    
    // Копируем конфиг
    if (config) {
        p_impl->config = *config;
    } else {
        p_impl->config = AS_context_config_init();
    }
    
    // Устанавливаем дефолтный лог если не указан
    if (!p_impl->config.log) {
        p_impl->config.log = AS_log::default_log();
    }
    
    // Устанавливаем дефолтные аллокаторы если не указаны
    if (!p_impl->config.allocation_callbacks.on_alloc) {
        p_impl->config.allocation_callbacks = get_default_allocators();
    }
    
    // Получаем список зарегистрированных бэкендов
    {
        auto& global = get_global_state();
        std::lock_guard<std::mutex> global_lock(global.backend_mutex);
        p_impl->available_backends = global.registered_backends;
    }
    
    // Выбираем и инициализируем бэкенд
    AS_result res = p_impl->select_backend(backends, backend_count);
    if (!AS_result_is_success(res)) {
        return res;
    }
    
    // Вызываем callback инициализации бэкенда
    if (p_impl->backend_callbacks.onContextInit) {
        res = p_impl->backend_callbacks.onContextInit(this, &p_impl->config, &p_impl->backend_callbacks);
        if (!AS_result_is_success(res)) {
            return res;
        }
    }
    
    p_impl->initialized = true;
    return AS_result::SUCCESS;
}

void AS_context::uninit() {
    if (!p_impl) return;
    
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    
    if (!p_impl->initialized) return;
    
    // Вызываем callback деинициализации бэкенда
    if (p_impl->backend_callbacks.onContextUninit) {
        p_impl->backend_callbacks.onContextUninit(this);
    }
    
    p_impl->initialized = false;
}

AS_result AS_context::enumerate_devices(
    AS_device_type type,
    AS_enumerate_devices_callback callback,
    void* user_data
) const {
    if (!p_impl || !p_impl->initialized) return AS_result::INVALID_OPERATION;
    if (!callback) return AS_result::INVALID_ARGS;
    
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    
    if (p_impl->backend_callbacks.onContextEnumerateDevices) {
        return p_impl->backend_callbacks.onContextEnumerateDevices(
            const_cast<AS_context*>(this), type, callback, user_data);
    }
    return AS_result::NOT_IMPLEMENTED;
}

AS_result AS_context::get_device_info(
    AS_device_type type,
    const void* device_id,
    AS_device_info* info
) const {
    if (!p_impl || !p_impl->initialized) return AS_result::INVALID_OPERATION;
    if (!info) return AS_result::INVALID_ARGS;
    
    std::lock_guard<std::mutex> lock(p_impl->mutex);
    
    if (p_impl->backend_callbacks.onContextGetDeviceInfo) {
        return p_impl->backend_callbacks.onContextGetDeviceInfo(
            const_cast<AS_context*>(this), type, device_id, info);
    }
    return AS_result::NOT_IMPLEMENTED;
}

AS_log* AS_context::log() const {
    if (!p_impl) return nullptr;
    return p_impl->config.log;
}

const AS_allocation_callbacks& AS_context::allocation_callbacks() const {
    static AS_allocation_callbacks default_cb = get_default_allocators();
    if (!p_impl) return default_cb;
    return p_impl->config.allocation_callbacks;
}

AS_backend AS_context::active_backend() const {
    if (!p_impl) return AS_backend::NULL_BACKEND;
    return p_impl->active_backend;
}

const char* AS_context::backend_name(AS_backend backend) {
    auto& global = get_global_state();
    std::lock_guard<std::mutex> lock(global.backend_mutex);
    
    for (const auto& info : global.registered_backends) {
        if (info.backend == backend) {
            return info.name;
        }
    }
    
    // Fallback для известных бэкендов
    switch (backend) {
        case AS_backend::NULL_BACKEND: return "Null";
    #if defined(_WIN32)
        case AS_backend::WASAPI: return "WASAPI";
        case AS_backend::DSOUND: return "DirectSound";
        case AS_backend::WINMM: return "WinMM";
    #endif
    #if defined(__linux__)
        case AS_backend::ALSA: return "ALSA";
        case AS_backend::PULSEAUDIO: return "PulseAudio";
        case AS_backend::JACK: return "JACK";
    #endif
    #if defined(__APPLE__)
        case AS_backend::COREAUDIO: return "CoreAudio";
    #endif
    #if defined(__ANDROID__)
        case AS_backend::AAUDIO: return "AAudio";
        case AS_backend::OPENSL: return "OpenSL";
    #endif
    #if defined(__EMSCRIPTEN__)
        case AS_backend::WEBAUDIO: return "WebAudio";
    #endif
        default: return "Unknown";
    }
}

AS_result AS_context::backend_from_name(const char* name, AS_backend* out) {
    if (!name || !out) return AS_result::INVALID_ARGS;
    
    auto& global = get_global_state();
    std::lock_guard<std::mutex> lock(global.backend_mutex);
    
    for (const auto& info : global.registered_backends) {
        if (strcmp(name, info.name) == 0) {
            *out = info.backend;
            return AS_result::SUCCESS;
        }
    }
    return AS_result::NO_BACKEND;
}

// ============================================================================
// AS_device::impl
// ============================================================================
struct AS_device::impl {
    AS_context* context = nullptr;
    AS_device_config config{};
    AS_device_state state{AS_device_state::UNINITIALIZED};
    AS_device_descriptor playback_descriptor{};
    AS_device_descriptor capture_descriptor{};
    
    // Runtime параметры (после инициализации)
    uint32_t actual_sample_rate = 0;
    AS_format actual_playback_format{AS_format::UNKNOWN};
    uint32_t actual_playback_channels = 0;
    AS_format actual_capture_format{AS_format::UNKNOWN};
    uint32_t actual_capture_channels = 0;
    
    float master_volume = 1.0f;
    
    // Backend-specific opaque pointer
    void* backend_data = nullptr;
    
    std::atomic<bool> started{false};
    std::mutex state_mutex;
    
    bool initialized = false;
};

AS_device::AS_device() : p_impl(std::make_unique<impl>()) {}
AS_device::~AS_device() = default;

AS_result AS_device::init(AS_context* context, const AS_device_config* config) {
    if (!p_impl || !context || !config) return AS_result::INVALID_ARGS;
    
    std::lock_guard<std::mutex> lock(p_impl->state_mutex);
    
    if (p_impl->initialized) {
        return AS_result::INVALID_OPERATION;
    }
    
    p_impl->context = context;
    p_impl->config = *config;
    
    // Валидация конфига
    if (config->type == AS_device_type::PLAYBACK || config->type == AS_device_type::DUPLEX) {
        if (config->playback.channels == 0 || config->playback.channels > 256) {
            return AS_result::INVALID_ARGS;
        }
    }
    if (config->type == AS_device_type::CAPTURE || config->type == AS_device_type::DUPLEX) {
        if (config->capture.channels == 0 || config->capture.channels > 256) {
            return AS_result::INVALID_ARGS;
        }
    }
    if (!config->data_callback) {
        return AS_result::INVALID_ARGS;
    }
    
    // Копируем дескрипторы из конфига
    if (config->type == AS_device_type::PLAYBACK || config->type == AS_device_type::DUPLEX) {
        p_impl->playback_descriptor.format = config->playback.format;
        p_impl->playback_descriptor.channels = config->playback.channels;
        p_impl->playback_descriptor.sample_rate = config->sample_rate;
        p_impl->playback_descriptor.share_mode = config->playback.share_mode;
        p_impl->playback_descriptor.device_id = config->playback.device_id;
        if (config->playback.channel_map) {
            memcpy(p_impl->playback_descriptor.channel_map, 
                   config->playback.channel_map,
                   config->playback.channels * sizeof(AS_channel));
        }
    }
    if (config->type == AS_device_type::CAPTURE || config->type == AS_device_type::DUPLEX) {
        p_impl->capture_descriptor.format = config->capture.format;
        p_impl->capture_descriptor.channels = config->capture.channels;
        p_impl->capture_descriptor.sample_rate = config->sample_rate;
        p_impl->capture_descriptor.share_mode = config->capture.share_mode;
        p_impl->capture_descriptor.device_id = config->capture.device_id;
        if (config->capture.channel_map) {
            memcpy(p_impl->capture_descriptor.channel_map,
                   config->capture.channel_map,
                   config->capture.channels * sizeof(AS_channel));
        }
    }
    
    // Вызываем backend init
    auto* ctx_impl = context->p_impl.get();
    if (!ctx_impl || !ctx_impl->backend_callbacks.onDeviceInit) {
        return AS_result::NOT_IMPLEMENTED;
    }
    
    AS_result res = ctx_impl->backend_callbacks.onDeviceInit(
        context,
        config->type,
        (config->type == AS_device_type::CAPTURE || config->type == AS_device_type::DUPLEX) 
            ? config->capture.device_id 
            : config->playback.device_id,
        config,
        this
    );
    
    if (!AS_result_is_success(res)) {
        return res;
    }
    
    p_impl->state = AS_device_state::STOPPED;
    p_impl->initialized = true;
    
    return AS_result::SUCCESS;
}

void AS_device::uninit() {
    if (!p_impl) return;
    
    std::lock_guard<std::mutex> lock(p_impl->state_mutex);
    
    if (!p_impl->initialized) return;
    
    // Останавливаем если запущено
    if (p_impl->started.load()) {
        stop();
    }
    
    // Вызываем backend uninit
    if (p_impl->context && p_impl->context->p_impl) {
        auto& callbacks = p_impl->context->p_impl->backend_callbacks;
        if (callbacks.onDeviceUninit) {
            callbacks.onDeviceUninit(this);
        }
    }
    
    p_impl->initialized = false;
    p_impl->state = AS_device_state::UNINITIALIZED;
    p_impl->backend_data = nullptr;
}

AS_result AS_device::start() {
    if (!p_impl) return AS_result::INVALID_OPERATION;
    
    std::lock_guard<std::mutex> lock(p_impl->state_mutex);
    
    if (!p_impl->initialized) return AS_result::INVALID_OPERATION;
    if (p_impl->state != AS_device_state::STOPPED) return AS_result::INVALID_OPERATION;
    
    p_impl->state = AS_device_state::STARTING;
    
    // Вызываем backend start
    if (p_impl->context && p_impl->context->p_impl) {
        auto& callbacks = p_impl->context->p_impl->backend_callbacks;
        if (callbacks.onDeviceStart) {
            AS_result res = callbacks.onDeviceStart(this);
            if (!AS_result_is_success(res)) {
                p_impl->state = AS_device_state::STOPPED;
                return res;
            }
        }
    }
    
    p_impl->started.store(true);
    p_impl->state = AS_device_state::STARTED;
    
    return AS_result::SUCCESS;
}

AS_result AS_device::stop() {
    if (!p_impl) return AS_result::INVALID_OPERATION;
    
    std::lock_guard<std::mutex> lock(p_impl->state_mutex);
    
    if (!p_impl->initialized) return AS_result::INVALID_OPERATION;
    if (p_impl->state != AS_device_state::STARTED) return AS_result::INVALID_OPERATION;
    
    p_impl->state = AS_device_state::STOPPING;
    p_impl->started.store(false);
    
    // Вызываем backend stop
    if (p_impl->context && p_impl->context->p_impl) {
        auto& callbacks = p_impl->context->p_impl->backend_callbacks;
        if (callbacks.onDeviceStop) {
            callbacks.onDeviceStop(this);
        }
    }
    
    p_impl->state = AS_device_state::STOPPED;
    
    return AS_result::SUCCESS;
}

AS_device_state AS_device::state() const {
    if (!p_impl) return AS_device_state::UNINITIALIZED;
    std::lock_guard<std::mutex> lock(p_impl->state_mutex);
    return p_impl->state;
}

AS_bool32 AS_device::is_started() const {
    if (!p_impl) return AS_FALSE;
    return p_impl->started.load() ? AS_TRUE : AS_FALSE;
}

uint32_t AS_device::sample_rate() const {
    if (!p_impl) return 0;
    return p_impl->actual_sample_rate;
}

AS_device::stream_info AS_device::playback_info() const {
    stream_info info{};
    if (p_impl) {
        info.format = p_impl->actual_playback_format;
        info.channels = p_impl->actual_playback_channels;
        info.descriptor = p_impl->playback_descriptor;
    }
    return info;
}

AS_device::stream_info AS_device::capture_info() const {
    stream_info info{};
    if (p_impl) {
        info.format = p_impl->actual_capture_format;
        info.channels = p_impl->actual_capture_channels;
        info.descriptor = p_impl->capture_descriptor;
    }
    return info;
}

AS_result AS_device::set_master_volume(float volume) {
    if (!p_impl) return AS_result::INVALID_OPERATION;
    if (volume < 0.0f || volume > 1.0f) return AS_result::INVALID_ARGS;
    
    p_impl->master_volume = volume;
    return AS_result::SUCCESS;
}

AS_result AS_device::get_master_volume(float* volume) const {
    if (!p_impl || !volume) return AS_result::INVALID_ARGS;
    *volume = p_impl->master_volume;
    return AS_result::SUCCESS;
}

AS_context* AS_device::context() const {
    if (!p_impl) return nullptr;
    return p_impl->context;
}

const AS_device_config* AS_device::config() const {
    if (!p_impl) return nullptr;
    return &p_impl->config;
}

AS_log* AS_device::log() const {
    if (!p_impl || !p_impl->context) return nullptr;
    return p_impl->context->log();
}

// ============================================================================
// Обработчик data callback (для backend implementation)
// ============================================================================
AS_bool32 AS_device_handle_backend_data_callback(
    AS_device* device,
    void* output,
    const void* input,
    uint32_t frame_count
) {
    if (!device || !device->p_impl) return AS_FALSE;
    
    auto* impl = device->p_impl.get();
    
    // Проверяем состояние
    if (impl->state != AS_device_state::STARTED) {
        // Заполняем тишиной если playback
        if (output && impl->actual_playback_channels > 0) {
            size_t sample_bytes = AS_bytes_per_sample(impl->actual_playback_format);
            size_t total_bytes = sample_bytes * impl->actual_playback_channels * frame_count;
            memset(output, 0, total_bytes);
        }
        return AS_TRUE;
    }
    
    // Применяем мастер-громкость
    if (impl->master_volume != 1.0f && output && impl->actual_playback_format == AS_format::F32) {
        float* f32_out = static_cast<float*>(output);
        uint32_t total_samples = frame_count * impl->actual_playback_channels;
        for (uint32_t i = 0; i < total_samples; ++i) {
            f32_out[i] *= impl->master_volume;
        }
    }
    
    // Вызываем пользовательский callback
    if (impl->config.data_callback) {
        impl->config.data_callback(device, output, input, frame_count);
    }
    
    return AS_TRUE;
}

// ============================================================================
// Quick start helpers
// ============================================================================
AS_API AS_result AS_init_and_start_device(
    AS_device_type type,
    AS_format format,
    uint32_t channels,
    uint32_t sample_rate,
    AS_device_data_proc callback,
    void* user_data,
    AS_device** out_device
) {
    if (!out_device || !callback) {
        return AS_result::INVALID_ARGS;
    }
    
    // Инициализация контекста
    AS_context_config ctx_cfg = AS_context_config_init();
    ctx_cfg.log = AS_log::default_log();
    
    AS_context context;
    AS_result res = context.init(nullptr, 0, &ctx_cfg);
    if (!AS_result_is_success(res)) {
        return res;
    }
    
    // Конфигурация устройства
    AS_device_config dev_cfg = AS_device_config_init(type);
    dev_cfg.playback.format = format;
    dev_cfg.playback.channels = channels;
    dev_cfg.sample_rate = sample_rate;
    dev_cfg.data_callback = callback;
    dev_cfg.user_data = user_data;
    
    // Создание устройства
    *out_device = new(std::nothrow) AS_device();
    if (!*out_device) {
        context.uninit();
        return AS_result::OUT_OF_MEMORY;
    }
    
    res = (*out_device)->init(&context, &dev_cfg);
    if (!AS_result_is_success(res)) {
        delete *out_device;
        *out_device = nullptr;
        context.uninit();
        return res;
    }
    
    // Запуск
    res = (*out_device)->start();
    if (!AS_result_is_success(res)) {
        (*out_device)->uninit();
        delete *out_device;
        *out_device = nullptr;
        context.uninit();
        return res;
    }
    
    return AS_result::SUCCESS;
}

AS_API void AS_stop_and_uninit_device(AS_device* device) {
    if (!device) return;
    
    device->stop();
    device->uninit();
    delete device;
}

AS_API const char* AS_result_string(AS_result result) {
    switch (result) {
        case AS_result::SUCCESS: return "SUCCESS";
        case AS_result::ERROR: return "ERROR";
        case AS_result::INVALID_ARGS: return "INVALID_ARGS";
        case AS_result::INVALID_OPERATION: return "INVALID_OPERATION";
        case AS_result::NO_BACKEND: return "NO_BACKEND";
        case AS_result::DEVICE_NOT_AVAILABLE: return "DEVICE_NOT_AVAILABLE";
        case AS_result::API_NOT_FOUND: return "API_NOT_FOUND";
        case AS_result::OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case AS_result::NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case AS_result::BUSY: return "BUSY";
        case AS_result::AT_END: return "AT_END";
        case AS_result::FORMAT_NOT_SUPPORTED: return "FORMAT_NOT_SUPPORTED";
        case AS_result::CHANNELS_NOT_SUPPORTED: return "CHANNELS_NOT_SUPPORTED";
        case AS_result::SAMPLE_RATE_NOT_SUPPORTED: return "SAMPLE_RATE_NOT_SUPPORTED";
        default: return "UNKNOWN_RESULT";
    }
}

// ============================================================================
// Backend registration
// ============================================================================
AS_result AS_register_backend(const AS_backend_info* info) {
    if (!info) return AS_result::INVALID_ARGS;
    
    auto& global = get_global_state();
    std::lock_guard<std::mutex> lock(global.backend_mutex);
    
    // Проверяем дубликаты
    for (const auto& existing : global.registered_backends) {
        if (existing.backend == info->backend) {
            return AS_result::INVALID_OPERATION;
        }
    }
    
    global.registered_backends.push_back(*info);
    return AS_result::SUCCESS;
}

// ============================================================================
// Runtime linking helpers
// ============================================================================
#if defined(_WIN32)
AS_handle AS_dlopen(AS_log* log, const char* path) {
    (void)log;
    return reinterpret_cast<AS_handle>(LoadLibraryA(path));
}

void* AS_dlsym(AS_log* log, AS_handle handle, const char* symbol) {
    (void)log;
    if (!handle) return nullptr;
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), symbol));
}

void AS_dlclose(AS_log* log, AS_handle handle) {
    (void)log;
    if (handle) {
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
    }
}
#else
AS_handle AS_dlopen(AS_log* log, const char* path) {
    void* handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (!handle && log) {
        log->post(AS_log_level::ERROR, "dlopen(%s) failed: %s", path, dlerror());
    }
    return handle;
}

void* AS_dlsym(AS_log* log, AS_handle handle, const char* symbol) {
    if (!handle) return nullptr;
    void* sym = dlsym(handle, symbol);
    if (!sym && log) {
        log->post(AS_log_level::WARNING, "dlsym(%s) failed: %s", symbol, dlerror());
    }
    return sym;
}

void AS_dlclose(AS_log* log, AS_handle handle) {
    if (handle) {
        int res = dlclose(handle);
        if (res != 0 && log) {
            log->post(AS_log_level::WARNING, "dlclose failed: %s", dlerror());
        }
    }
}
#endif

} // namespace arxsound