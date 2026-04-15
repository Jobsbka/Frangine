#pragma once

#include "arxsound_types.hpp"
#include "arxsound_context.hpp"
#include <atomic>
#include <mutex>

namespace arxsound {

// ============================================================================
// Конфигурация устройства
// ============================================================================
struct AS_device_config {
    AS_device_type type = AS_device_type::PLAYBACK;
    
    // Общие параметры
    uint32_t sample_rate = 0;  // 0 = native
    AS_performance_profile performance_profile = AS_performance_profile::CONSERVATIVE;
    
    // Callbacks
    AS_device_data_proc data_callback = nullptr;
    AS_device_notification_proc notification_callback = nullptr;
    void* user_data = nullptr;
    
    // Конфигурация playback
    struct {
        AS_format format = AS_format::UNKNOWN;  // 0 = native
        uint32_t channels = 0;                   // 0 = native
        AS_channel* channel_map = nullptr;       // nullptr = default
        const void* device_id = nullptr;         // из AS_device_info::id
        AS_share_mode share_mode = AS_share_mode::SHARED;
    } playback;
    
    // Конфигурация capture
    struct {
        AS_format format = AS_format::UNKNOWN;
        uint32_t channels = 0;
        AS_channel* channel_map = nullptr;
        const void* device_id = nullptr;
        AS_share_mode share_mode = AS_share_mode::SHARED;
    } capture;
    
    // Период/буфер (подсказки)
    uint32_t period_size_in_frames = 0;    // 0 = backend default
    uint32_t period_count = 0;              // 0 = backend default
    
    // Флаги
    struct {
        AS_bool32 no_pre_slilced_callback : 1;  // не вызывать callback до start()
        AS_bool32 no_clip : 1;                   // отключить клиппинг
        AS_bool32 no_fixed_sized_callback : 1;   // разрешить переменный frame_count
    } flags;
};

constexpr AS_device_config AS_device_config_init(AS_device_type type) {
    AS_device_config cfg{};
    cfg.type = type;
    return cfg;
}

// ============================================================================
// Дескрипторы устройства (internal)
// ============================================================================
struct AS_device_descriptor {
    AS_format format;
    uint32_t channels;
    uint32_t sample_rate;
    AS_channel channel_map[256];  // макс. 256 каналов
    AS_share_mode share_mode;
    uint32_t period_size_in_frames;
    uint32_t period_count;
    const void* device_id;  // platform-specific
};

// ============================================================================
// Основное устройство
// ============================================================================
class AS_device {
public:
    AS_device();
    ~AS_device();
    
    // Инициализация
    AS_result init(AS_context* context, const AS_device_config* config);
    void uninit();
    
    // Управление состоянием
    AS_result start();
    AS_result stop();
    
    // Состояние
    AS_device_state state() const;
    AS_bool32 is_started() const;
    
    // Параметры (после init - отражают реальные значения)
    uint32_t sample_rate() const;
    
    struct stream_info {
        AS_format format;
        uint32_t channels;
        AS_device_descriptor descriptor;
    };
    
    stream_info playback_info() const;
    stream_info capture_info() const;
    
    // Громкость (мастер, 0.0..1.0)
    AS_result set_master_volume(float volume);
    AS_result get_master_volume(float* volume) const;
    
    // Контекст и конфиг
    AS_context* context() const;
    const AS_device_config* config() const;
    
    // Получение логов
    AS_log* log() const;
    
private:
    // Внутренняя реализация
    struct impl;
    std::unique_ptr<impl> p_impl;
    
    // Запрет копирования
    AS_device(const AS_device&) = delete;
    AS_device& operator=(const AS_device&) = delete;
};

// ============================================================================
// Обработчик data callback (для backend implementation)
// ============================================================================
AS_bool32 AS_device_handle_backend_data_callback(
    AS_device* device,
    void* output,
    const void* input,
    uint32_t frame_count
);

} // namespace arxsound