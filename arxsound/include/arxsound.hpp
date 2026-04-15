/**
 * ArxSound - LowLevel Audio Library
 * 
 * Кроссплатформенная библиотека для воспроизведения и записи аудио в реальном времени.
 * Построена на протоколе ArxGlue для интеграции с игровыми движками.
 * 
 * Лицензия: BSD
 */

#pragma once

#ifndef ARXSOUND_H
#define ARXSOUND_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Version
// ============================================================================
#define AS_VERSION_MAJOR 0
#define AS_VERSION_MINOR 1
#define AS_VERSION_REVISION 0
#define AS_VERSION_STRING "0.1.0"

// ============================================================================
// Public API includes
// ============================================================================
#include "arxsound_types.hpp"
#include "arxsound_log.hpp"
#include "arxsound_context.hpp"
#include "arxsound_device.hpp"
#include "arxsound_convert.hpp"

// ============================================================================
// Quick start helpers
// ============================================================================

/**
 * Инициализация устройства "одной строкой" для простых сценариев.
 * Создаёт внутренний контекст и устройство.
 */
AS_API AS_result AS_init_and_start_device(
    AS_device_type type,
    AS_format format,
    uint32_t channels,
    uint32_t sample_rate,
    AS_device_data_proc callback,
    void* user_data,
    AS_device** out_device
);

/**
 * Остановка и очистка устройства, созданного через AS_init_and_start_device.
 */
AS_API void AS_stop_and_uninit_device(AS_device* device);

/**
 * Получение строкового описания кода результата.
 */
AS_API const char* AS_result_string(AS_result result);

/**
 * Получение имени бэкенда.
 */
AS_API const char* AS_backend_name(AS_backend backend);

#ifdef __cplusplus
} // extern "C"

// ============================================================================
// C++ convenience wrappers
// ============================================================================
namespace arxsound {

// RAII wrapper для устройства
class device_guard {
public:
    device_guard() = default;
    ~device_guard() { if (m_device) m_device->uninit(); }
    
    // Move-only
    device_guard(device_guard&&) = default;
    device_guard& operator=(device_guard&&) = default;
    
    device_guard(const device_guard&) = delete;
    device_guard& operator=(const device_guard&) = delete;
    
    AS_device* get() { return m_device; }
    AS_device* release() { auto* d = m_device; m_device = nullptr; return d; }
    
    AS_result init(AS_context* ctx, const AS_device_config* cfg) {
        if (!m_device) m_device = std::make_unique<AS_device>();
        return m_device->init(ctx, cfg);
    }
    
private:
    std::unique_ptr<AS_device> m_device;
};

} // namespace arxsound

#endif // __cplusplus

#endif // ARXSOUND_H