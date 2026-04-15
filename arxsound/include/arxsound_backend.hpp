#pragma once

#include "arxsound_types.hpp"
#include "arxsound_context.hpp"
#include "arxsound_device.hpp"

namespace arxsound {

// ============================================================================
// Backend callbacks (для реализации платформенных бэкендов)
// ============================================================================
struct AS_backend_callbacks {
    // Контекст
    AS_result (*on_context_init)(AS_context* context, const AS_context_config* config, AS_backend_callbacks* callbacks);
    AS_result (*on_context_uninit)(AS_context* context);
    AS_result (*on_context_enumerate_devices)(AS_context* context, AS_device_type type, AS_enumerate_devices_callback callback, void* user_data);
    AS_result (*on_context_get_device_info)(AS_context* context, AS_device_type type, const void* device_id, AS_device_info* info);
    
    // Устройство
    AS_result (*on_device_init)(AS_context* context, AS_device_type type, const void* device_id, const AS_device_config* config, AS_device* device);
    AS_result (*on_device_uninit)(AS_device* device);
    AS_result (*on_device_start)(AS_device* device);
    AS_result (*on_device_stop)(AS_device* device);
    
    // Data I/O (для blocking backends)
    AS_result (*on_device_read)(AS_device* device, void* frames, uint32_t frame_count, uint32_t* frames_read);
    AS_result (*on_device_write)(AS_device* device, const void* frames, uint32_t frame_count, uint32_t* frames_written);
    
    // Data loop (для callback-based backends - опционально)
    void (*on_device_data_loop)(AS_device* device);
    void (*on_device_data_loop_wakeup)(AS_device* device);
};

// ============================================================================
// Runtime linking helpers (для динамической загрузки библиотек)
// ============================================================================
using AS_handle = void*;  // HMODULE / void*

AS_handle AS_dlopen(AS_log* log, const char* path);
void* AS_dlsym(AS_log* log, AS_handle handle, const char* symbol);
void AS_dlclose(AS_log* log, AS_handle handle);

// ============================================================================
// Helper для backend registration
// ============================================================================
struct AS_backend_info {
    AS_backend backend;
    const char* name;
    AS_bool32 (*is_available)();
    AS_backend_callbacks callbacks;
};

// Регистрация бэкенда (вызывается при старте библиотеки)
AS_result AS_register_backend(const AS_backend_info* info);

} // namespace arxsound