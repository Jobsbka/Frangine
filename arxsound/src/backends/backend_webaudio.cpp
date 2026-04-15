// src/backends/backend_webaudio.cpp
#include "../include/arxsound_backend.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <cstring>
#include <cmath>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

namespace arxsound {

// ============================================================================
// WebAudio internal structures
// ============================================================================

struct webaudio_context_state {
#if defined(__EMSCRIPTEN__)
    bool audio_context_created = false;
#endif
    std::mutex mutex;
};

struct webaudio_device_state {
    // Device info
    AS_device_type type{AS_device_type::PLAYBACK};
    
    // Audio format
    AS_format format{AS_format::F32};
    uint32_t channels{2};
    uint32_t sample_rate{48000};
    
    // Callbacks
    AS_device_data_proc data_callback{nullptr};
    AS_device_notification_proc notification_callback{nullptr};
    void* user_data{nullptr};
    
    // State
    std::atomic<bool> running{false};
    std::atomic<bool> started{false};
    
    // Volume
    float master_volume{1.0f};
    
    // Buffer (for emulated callback)
    std::vector<float> buffer;
    uint32_t buffer_size{0};
    
#if defined(__EMSCRIPTEN__)
    // JavaScript interop
    int32_t audio_node_id{-1};
#endif
};

// ============================================================================
// JavaScript interop (Emscripten)
// ============================================================================

#if defined(__EMSCRIPTEN__)

extern "C" {

// Create AudioContext and ScriptProcessorNode
EMSCRIPTEN_KEEPALIVE
int webaudio_create_device(int sample_rate, int channels, int buffer_size) {
    static const char* script = R"(
    (function(sampleRate, channels, bufferSize) {
        if (!window.miniaudio) {
            window.miniaudio = { devices: {}, nextId: 0 };
        }
        
        var audioContext = new (window.AudioContext || window.webkitAudioContext)({
            sampleRate: sampleRate
        });
        
        var scriptProcessor = audioContext.createScriptProcessor(bufferSize, channels, channels);
        var deviceId = window.miniaudio.nextId++;
        
        scriptProcessor.onaudioprocess = function(e) {
            var outputBuffer = e.outputBuffer;
            var deviceId = scriptProcessor.deviceId;
            
            // Call C callback
            if (Module.ccall) {
                Module.ccall('webaudio_audio_callback', null, 
                    ['number', 'number', 'number'],
                    [deviceId, outputBuffer.length, outputBuffer.numberOfChannels]);
            }
        };
        
        scriptProcessor.deviceId = deviceId;
        scriptProcessor.connect(audioContext.destination);
        
        window.miniaudio.devices[deviceId] = {
            context: audioContext,
            processor: scriptProcessor
        };
        
        return deviceId;
    })(sampleRate, channels, bufferSize);
    )";
    
    // Note: In real implementation, this would be proper JS interop
    // This is simplified for demonstration
    (void)script;
    (void)sample_rate;
    (void)channels;
    (void)buffer_size;
    return 0;
}

// Start audio playback
EMSCRIPTEN_KEEPALIVE
void webaudio_start_device(int device_id) {
    static const char* script = R"(
    (function(deviceId) {
        if (window.miniaudio && window.miniaudio.devices[deviceId]) {
            var device = window.miniaudio.devices[deviceId];
            if (device.context.state === 'suspended') {
                device.context.resume();
            }
        }
    })(deviceId);
    )";
    (void)script;
    (void)device_id;
}

// Stop audio playback
EMSCRIPTEN_KEEPALIVE
void webaudio_stop_device(int device_id) {
    static const char* script = R"(
    (function(deviceId) {
        if (window.miniaudio && window.miniaudio.devices[deviceId]) {
            var device = window.miniaudio.devices[deviceId];
            if (device.context.state === 'running') {
                device.context.suspend();
            }
        }
    })(deviceId);
    )";
    (void)script;
    (void)device_id;
}

// Destroy device
EMSCRIPTEN_KEEPALIVE
void webaudio_destroy_device(int device_id) {
    static const char* script = R"(
    (function(deviceId) {
        if (window.miniaudio && window.miniaudio.devices[deviceId]) {
            var device = window.miniaudio.devices[deviceId];
            device.processor.disconnect();
            device.context.close();
            delete window.miniaudio.devices[deviceId];
        }
    })(deviceId);
    )";
    (void)script;
    (void)device_id;
}

} // extern "C"

// C callback from JavaScript
extern "C" void webaudio_audio_callback(int device_id, int frame_count, int channels) {
    // In real implementation, this would find the device state and call data_callback
    (void)device_id;
    (void)frame_count;
    (void)channels;
}

#endif

// ============================================================================
// Backend callbacks implementation
// ============================================================================

// --- Context callbacks ---

static AS_result webaudio_on_context_init(
    AS_context* context,
    const AS_context_config* config,
    AS_backend_callbacks* callbacks
) {
    (void)context;
    (void)config;
    (void)callbacks;
    
#if defined(__EMSCRIPTEN__)
    auto* state = new(std::nothrow) webaudio_context_state();
    if (!state) {
        return AS_result::OUT_OF_MEMORY;
    }
    
    // WebAudio doesn't require special initialization
    state->audio_context_created = false;
    delete state;
#endif
    
    return AS_result::SUCCESS;
}

static AS_result webaudio_on_context_uninit(AS_context* context) {
    (void)context;
    return AS_result::SUCCESS;
}

static AS_result webaudio_on_context_enumerate_devices(
    AS_context* context,
    AS_device_type type,
    AS_enumerate_devices_callback callback,
    void* user_data
) {
    if (!callback) return AS_result::INVALID_ARGS;
    
    // WebAudio only supports playback (no capture in browsers)
    if (type == AS_device_type::CAPTURE) {
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    AS_device_info info{};
    memset(&info, 0, sizeof(info));
    
    snprintf(info.name, sizeof(info.name), "WebAudio Output");
    strcpy(info.id.webaudio, "default");
    info.is_default = AS_TRUE;
    
    // WebAudio capabilities
    info.min_sample_rate = 8000;
    info.max_sample_rate = 96000;  // Browser limit
    info.min_channels = 1;
    info.max_channels = 8;
    info.supports_shared = AS_TRUE;
    info.supports_exclusive = AS_FALSE;
    
    // Native formats (WebAudio uses F32)
    info.native_format_count = 0;
    info.native_formats[0] = {AS_format::F32, 2, 48000};
    info.native_formats[1] = {AS_format::F32, 2, 44100};
    info.native_format_count = 2;
    
    callback(context, type, &info, user_data);
    
    return AS_result::SUCCESS;
}

static AS_result webaudio_on_context_get_device_info(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    AS_device_info* info
) {
    if (!info) return AS_result::INVALID_ARGS;
    
    return webaudio_on_context_enumerate_devices(context, type,
        [](AS_context*, AS_device_type, const AS_device_info* src, void* dst) {
            *static_cast<AS_device_info*>(dst) = *src;
            return AS_TRUE;
        }, info);
}

// --- Device callbacks ---

static AS_result webaudio_on_device_init(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    const AS_device_config* config,
    AS_device* device
) {
    (void)context;
    (void)device_id;
    
    if (!config || !device) {
        return AS_result::INVALID_ARGS;
    }
    
    // WebAudio only supports playback
    if (type == AS_device_type::CAPTURE || type == AS_device_type::DUPLEX) {
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    auto* state = new(std::nothrow) webaudio_device_state();
    if (!state) {
        return AS_result::OUT_OF_MEMORY;
    }
    
    state->type = type;
    state->data_callback = config->data_callback;
    state->notification_callback = config->notification_callback;
    state->user_data = config->user_data;
    
    // WebAudio always uses F32
    state->format = AS_format::F32;
    state->channels = config->playback.channels ? config->playback.channels : 2;
    state->sample_rate = config->sample_rate ? config->sample_rate : 48000;
    
    // Buffer size
    uint32_t buffer_size = config->period_size_in_frames;
    if (buffer_size == 0) buffer_size = 2048;  // WebAudio default
    state->buffer_size = buffer_size;
    state->buffer.resize(buffer_size * state->channels);
    
#if defined(__EMSCRIPTEN__)
    // Create WebAudio device via JS interop
    state->audio_node_id = webaudio_create_device(
        static_cast<int>(state->sample_rate),
        static_cast<int>(state->channels),
        static_cast<int>(buffer_size)
    );
    
    if (state->audio_node_id < 0) {
        delete state;
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
#endif
    
    // Update device runtime info
    device->p_impl->actual_sample_rate = state->sample_rate;
    device->p_impl->actual_playback_format = state->format;
    device->p_impl->actual_playback_channels = state->channels;
    
    // Store state
    device->p_impl->backend_data = static_cast<void*>(state);
    
    return AS_result::SUCCESS;
}

static AS_result webaudio_on_device_uninit(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<webaudio_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::SUCCESS;
    
    // Stop if running
    if (state->running.load()) {
        webaudio_on_device_stop(device);
    }
    
#if defined(__EMSCRIPTEN__)
    if (state->audio_node_id >= 0) {
        webaudio_destroy_device(state->audio_node_id);
    }
#endif
    
    delete state;
    device->p_impl->backend_data = nullptr;
    
    return AS_result::SUCCESS;
}

static AS_result webaudio_on_device_start(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<webaudio_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::INVALID_OPERATION;
    
    if (state->running.load()) {
        return AS_result::SUCCESS;
    }
    
#if defined(__EMSCRIPTEN__)
    if (state->audio_node_id >= 0) {
        webaudio_start_device(state->audio_node_id);
    }
#endif
    
    state->running.store(true);
    state->started.store(true);
    
    // Notify callback
    if (state->notification_callback) {
        state->notification_callback(
            device,
            static_cast<int>(AS_device_notification_type::STARTED),
            state->user_data
        );
    }
    
    return AS_result::SUCCESS;
}

static AS_result webaudio_on_device_stop(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<webaudio_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::SUCCESS;
    
    if (!state->running.load()) {
        return AS_result::SUCCESS;
    }
    
#if defined(__EMSCRIPTEN__)
    if (state->audio_node_id >= 0) {
        webaudio_stop_device(state->audio_node_id);
    }
#endif
    
    state->running.store(false);
    state->started.store(false);
    
    // Notify callback
    if (state->notification_callback) {
        state->notification_callback(
            device,
            static_cast<int>(AS_device_notification_type::STOPPED),
            state->user_data
        );
    }
    
    return AS_result::SUCCESS;
}

static AS_result webaudio_on_device_read(
    AS_device* device,
    void* frames,
    uint32_t frame_count,
    uint32_t* frames_read
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_read) *frames_read = 0;
    return AS_result::NOT_IMPLEMENTED;
}

static AS_result webaudio_on_device_write(
    AS_device* device,
    const void* frames,
    uint32_t frame_count,
    uint32_t* frames_written
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_written) *frames_written = 0;
    return AS_result::NOT_IMPLEMENTED;
}

// ============================================================================
// Backend registration
// ============================================================================

static AS_bool32 webaudio_is_available() {
#if defined(__EMSCRIPTEN__)
    return AS_TRUE;
#else
    return AS_FALSE;
#endif
}

static AS_backend_callbacks g_webaudio_callbacks = {
    webaudio_on_context_init,
    webaudio_on_context_uninit,
    webaudio_on_context_enumerate_devices,
    webaudio_on_context_get_device_info,
    webaudio_on_device_init,
    webaudio_on_device_uninit,
    webaudio_on_device_start,
    webaudio_on_device_stop,
    webaudio_on_device_read,
    webaudio_on_device_write,
    nullptr,  // on_device_data_loop
    nullptr   // on_device_data_loop_wakeup
};

static AS_backend_info g_webaudio_info = {
    AS_backend::WEBAUDIO,
    "WebAudio",
    webaudio_is_available,
    g_webaudio_callbacks
};

AS_result AS_register_webaudio_backend() {
    return AS_register_backend(&g_webaudio_info);
}

#if defined(__EMSCRIPTEN__) && (defined(__GNUC__) || defined(__clang__))
__attribute__((constructor))
static void auto_register_webaudio_backend() {
    AS_register_webaudio_backend();
}
#endif

} // namespace arxsound