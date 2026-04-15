// src/backends/backend_aaudio.cpp
#include "../include/arxsound_backend.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <cstring>
#include <cmath>

#if defined(__ANDROID__)
#include <aaudio/AAudio.h>
#endif

namespace arxsound {

// ============================================================================
// AAudio internal structures
// ============================================================================

#if defined(__ANDROID__)

// Function pointer types for AAudio (runtime linking)
typedef int32_t (*AAudio_createStreamBuilder_proc)(AAudioStreamBuilder** builder);
typedef int32_t (*AAudioStreamBuilder_delete_proc)(AAudioStreamBuilder* builder);
typedef int32_t (*AAudioStreamBuilder_setFormat_proc)(AAudioStreamBuilder* builder, AAudioFormat format);
typedef int32_t (*AAudioStreamBuilder_setChannelCount_proc)(AAudioStreamBuilder* builder, int32_t channelCount);
typedef int32_t (*AAudioStreamBuilder_setSampleRate_proc)(AAudioStreamBuilder* builder, int32_t sampleRate);
typedef int32_t (*AAudioStreamBuilder_setBufferCapacityInFrames_proc)(AAudioStreamBuilder* builder, int32_t numFrames);
typedef int32_t (*AAudioStreamBuilder_setPerformanceMode_proc)(AAudioStreamBuilder* builder, AAudioPerformanceMode mode);
typedef int32_t (*AAudioStreamBuilder_setSharingMode_proc)(AAudioStreamBuilder* builder, AAudioSharingMode sharingMode);
typedef int32_t (*AAudioStreamBuilder_setDirection_proc)(AAudioStreamBuilder* builder, AAudioDirection direction);
typedef int32_t (*AAudioStreamBuilder_setDataCallback_proc)(AAudioStreamBuilder* builder, AAudioStream_dataCallback callback, void* userData);
typedef int32_t (*AAudioStreamBuilder_setErrorCallback_proc)(AAudioStreamBuilder* builder, AAudioStream_errorCallback callback, void* userData);
typedef int32_t (*AAudioStreamBuilder_openStream_proc)(AAudioStreamBuilder* builder, AAudioStream** stream);
typedef int32_t (*AAudioStream_requestStart_proc)(AAudioStream* stream);
typedef int32_t (*AAudioStream_requestStop_proc)(AAudioStream* stream);
typedef int32_t (*AAudioStream_close_proc)(AAudioStream* stream);
typedef int32_t (*AAudioStream_getFormat_proc)(AAudioStream* stream);
typedef int32_t (*AAudioStream_getChannelCount_proc)(AAudioStream* stream);
typedef int32_t (*AAudioStream_getSampleRate_proc)(AAudioStream* stream);
typedef int32_t (*AAudioStream_getFramesPerBurst_proc)(AAudioStream* stream);
typedef int64_t (*AAudioStream_getFramesRead_proc)(AAudioStream* stream);
typedef int64_t (*AAudioStream_getFramesWritten_proc)(AAudioStream* stream);

struct aaudio_library {
    void* handle = nullptr;
    AAudio_createStreamBuilder_proc AAudio_createStreamBuilder = nullptr;
    AAudioStreamBuilder_delete_proc AAudioStreamBuilder_delete = nullptr;
    AAudioStreamBuilder_setFormat_proc AAudioStreamBuilder_setFormat = nullptr;
    AAudioStreamBuilder_setChannelCount_proc AAudioStreamBuilder_setChannelCount = nullptr;
    AAudioStreamBuilder_setSampleRate_proc AAudioStreamBuilder_setSampleRate = nullptr;
    AAudioStreamBuilder_setBufferCapacityInFrames_proc AAudioStreamBuilder_setBufferCapacityInFrames = nullptr;
    AAudioStreamBuilder_setPerformanceMode_proc AAudioStreamBuilder_setPerformanceMode = nullptr;
    AAudioStreamBuilder_setSharingMode_proc AAudioStreamBuilder_setSharingMode = nullptr;
    AAudioStreamBuilder_setDirection_proc AAudioStreamBuilder_setDirection = nullptr;
    AAudioStreamBuilder_setDataCallback_proc AAudioStreamBuilder_setDataCallback = nullptr;
    AAudioStreamBuilder_setErrorCallback_proc AAudioStreamBuilder_setErrorCallback = nullptr;
    AAudioStreamBuilder_openStream_proc AAudioStreamBuilder_openStream = nullptr;
    AAudioStream_requestStart_proc AAudioStream_requestStart = nullptr;
    AAudioStream_requestStop_proc AAudioStream_requestStop = nullptr;
    AAudioStream_close_proc AAudioStream_close = nullptr;
    AAudioStream_getFormat_proc AAudioStream_getFormat = nullptr;
    AAudioStream_getChannelCount_proc AAudioStream_getChannelCount = nullptr;
    AAudioStream_getSampleRate_proc AAudioStream_getSampleRate = nullptr;
    AAudioStream_getFramesPerBurst_proc AAudioStream_getFramesPerBurst = nullptr;
    AAudioStream_getFramesRead_proc AAudioStream_getFramesRead = nullptr;
    AAudioStream_getFramesWritten_proc AAudioStream_getFramesWritten = nullptr;
    
    bool loaded = false;
};

static aaudio_library g_aaudio_lib;

static bool load_aaudio_library(AS_log* log) {
    if (g_aaudio_lib.loaded) {
        return true;
    }
    
    const char* lib_names[] = {
        "libaaudio.so",
        "libaaudio.so.1"
    };
    
    for (size_t i = 0; i < sizeof(lib_names) / sizeof(lib_names[0]); ++i) {
        g_aaudio_lib.handle = AS_dlopen(log, lib_names[i]);
        if (g_aaudio_lib.handle) {
            break;
        }
    }
    
    if (!g_aaudio_lib.handle) {
        return false;
    }
    
    #define LOAD_AAUDIO_SYMBOL(name) \
        g_aaudio_lib.name = reinterpret_cast<decltype(g_aaudio_lib.name)>( \
            AS_dlsym(log, g_aaudio_lib.handle, #name))
    
    LOAD_AAUDIO_SYMBOL(AAudio_createStreamBuilder);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_delete);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_setFormat);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_setChannelCount);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_setSampleRate);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_setBufferCapacityInFrames);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_setPerformanceMode);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_setSharingMode);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_setDirection);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_setDataCallback);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_setErrorCallback);
    LOAD_AAUDIO_SYMBOL(AAudioStreamBuilder_openStream);
    LOAD_AAUDIO_SYMBOL(AAudioStream_requestStart);
    LOAD_AAUDIO_SYMBOL(AAudioStream_requestStop);
    LOAD_AAUDIO_SYMBOL(AAudioStream_close);
    LOAD_AAUDIO_SYMBOL(AAudioStream_getFormat);
    LOAD_AAUDIO_SYMBOL(AAudioStream_getChannelCount);
    LOAD_AAUDIO_SYMBOL(AAudioStream_getSampleRate);
    LOAD_AAUDIO_SYMBOL(AAudioStream_getFramesPerBurst);
    LOAD_AAUDIO_SYMBOL(AAudioStream_getFramesRead);
    LOAD_AAUDIO_SYMBOL(AAudioStream_getFramesWritten);
    
    #undef LOAD_AAUDIO_SYMBOL
    
    // Check critical symbols
    if (!g_aaudio_lib.AAudio_createStreamBuilder || 
        !g_aaudio_lib.AAudioStreamBuilder_openStream ||
        !g_aaudio_lib.AAudioStream_requestStart) {
        AS_dlclose(log, g_aaudio_lib.handle);
        g_aaudio_lib.handle = nullptr;
        return false;
    }
    
    g_aaudio_lib.loaded = true;
    return true;
}

#endif

struct aaudio_context_state {
#if defined(__ANDROID__)
    aaudio_library* lib = nullptr;
#endif
    std::mutex mutex;
};

struct aaudio_device_state {
#if defined(__ANDROID__)
    AAudioStreamBuilder* stream_builder = nullptr;
    AAudioStream* stream = nullptr;
#endif
    
    // Audio format
    AS_format format{AS_format::F32};
    uint32_t channels{2};
    uint32_t sample_rate{48000};
    
    // Device info
    AS_device_type type{AS_device_type::PLAYBACK};
    
    // Callbacks
    AS_device_data_proc data_callback{nullptr};
    AS_device_notification_proc notification_callback{nullptr};
    void* user_data{nullptr};
    
    // State
    std::atomic<bool> running{false};
    std::atomic<bool> started{false};
    
    // Volume
    float master_volume{1.0f};
    
    // Buffer info
    uint32_t burst_size{0};
    uint32_t buffer_capacity{0};
    
    // Error tracking
    std::atomic<int32_t> last_error{0};
};

// ============================================================================
// AAudio data callback
// ============================================================================

#if defined(__ANDROID__)
static AAudioDataCallbackResult aaudio_data_callback(
    AAudioStream* stream,
    void* user_data,
    void* audio_data,
    int32_t num_frames
) {
    auto* state = static_cast<aaudio_device_state*>(user_data);
    if (!state || !state->data_callback) {
        // Fill with silence
        memset(audio_data, 0, num_frames * state->channels * sizeof(float));
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
    
    // Call user callback
    state->data_callback(
        nullptr,  // device
        audio_data,
        nullptr,  // input (for playback)
        static_cast<uint32_t>(num_frames)
    );
    
    // Apply master volume
    if (state->master_volume != 1.0f) {
        float* f32_data = static_cast<float*>(audio_data);
        int32_t total_samples = num_frames * state->channels;
        for (int32_t i = 0; i < total_samples; ++i) {
            f32_data[i] *= state->master_volume;
        }
    }
    
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static AAudioErrorCallbackResult aaudio_error_callback(
    AAudioStream* stream,
    void* user_data,
    AAudioResult error
) {
    auto* state = static_cast<aaudio_device_state*>(user_data);
    if (state) {
        state->last_error.store(error, std::memory_order_release);
    }
    
    // Return CONTINUE to keep stream running, STOP to close it
    return (error == AAUDIO_ERROR_DISCONNECTED) 
        ? AAUDIO_CALLBACK_RESULT_STOP 
        : AAUDIO_CALLBACK_RESULT_CONTINUE;
}
#endif

// ============================================================================
// Backend callbacks implementation
// ============================================================================

// --- Context callbacks ---

static AS_result aaudio_on_context_init(
    AS_context* context,
    const AS_context_config* config,
    AS_backend_callbacks* callbacks
) {
    (void)context;
    (void)config;
    (void)callbacks;
    
#if defined(__ANDROID__)
    auto* state = new(std::nothrow) aaudio_context_state();
    if (!state) {
        return AS_result::OUT_OF_MEMORY;
    }
    
    AS_log* log = config ? config->log : nullptr;
    if (!load_aaudio_library(log)) {
        delete state;
        return AS_result::API_NOT_FOUND;
    }
    
    state->lib = &g_aaudio_lib;
    // Store state in context (simplified - real impl needs proper storage)
    delete state;
#endif
    
    return AS_result::SUCCESS;
}

static AS_result aaudio_on_context_uninit(AS_context* context) {
    (void)context;
    
#if defined(__ANDROID__)
    if (g_aaudio_lib.handle) {
        AS_dlclose(nullptr, g_aaudio_lib.handle);
        g_aaudio_lib.handle = nullptr;
        g_aaudio_lib.loaded = false;
    }
#endif
    
    return AS_result::SUCCESS;
}

static AS_result aaudio_on_context_enumerate_devices(
    AS_context* context,
    AS_device_type type,
    AS_enumerate_devices_callback callback,
    void* user_data
) {
    if (!callback) return AS_result::INVALID_ARGS;
    
#if defined(__ANDROID__)
    if (!g_aaudio_lib.loaded) {
        return AS_result::NO_BACKEND;
    }
#endif
    
    AS_device_info info{};
    memset(&info, 0, sizeof(info));
    
    const char* type_str = (type == AS_device_type::PLAYBACK) ? "Playback" : "Capture";
    snprintf(info.name, sizeof(info.name), "AAudio %s Device", type_str);
    strcpy(info.id.aaudio, "default");
    info.is_default = AS_TRUE;
    
    // AAudio capabilities
    info.min_sample_rate = 8000;
    info.max_sample_rate = 192000;
    info.min_channels = 1;
    info.max_channels = 8;
    info.supports_shared = AS_TRUE;
    info.supports_exclusive = AS_TRUE;
    
    // Native formats
    info.native_format_count = 0;
    auto add_format = [&](AS_format fmt, uint32_t ch, uint32_t sr) {
        if (info.native_format_count < AS_device_info::MAX_NATIVE_FORMATS) {
            info.native_formats[info.native_format_count++] = {fmt, ch, sr};
        }
    };
    add_format(AS_format::F32, 2, 48000);
    add_format(AS_format::F32, 1, 48000);
    add_format(AS_format::S16, 2, 48000);
    add_format(AS_format::S16, 2, 44100);
    
    callback(context, type, &info, user_data);
    
    return AS_result::SUCCESS;
}

static AS_result aaudio_on_context_get_device_info(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    AS_device_info* info
) {
    if (!info) return AS_result::INVALID_ARGS;
    
    return aaudio_on_context_enumerate_devices(context, type,
        [](AS_context*, AS_device_type, const AS_device_info* src, void* dst) {
            *static_cast<AS_device_info*>(dst) = *src;
            return AS_TRUE;
        }, info);
}

// --- Device callbacks ---

static AS_result aaudio_on_device_init(
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
    
#if !defined(__ANDROID__)
    return AS_result::NO_BACKEND;
#else
    if (!g_aaudio_lib.loaded) {
        return AS_result::API_NOT_FOUND;
    }
#endif
    
    auto* state = new(std::nothrow) aaudio_device_state();
    if (!state) {
        return AS_result::OUT_OF_MEMORY;
    }
    
    state->type = type;
    state->data_callback = config->data_callback;
    state->notification_callback = config->notification_callback;
    state->user_data = config->user_data;
    
    // Determine format and parameters
    state->format = (type == AS_device_type::CAPTURE)
        ? config->capture.format
        : config->playback.format;
    if (state->format == AS_format::UNKNOWN) {
        state->format = AS_format::F32;
    }
    
    state->channels = (type == AS_device_type::CAPTURE)
        ? config->capture.channels
        : config->playback.channels;
    if (state->channels == 0) state->channels = 2;
    
    state->sample_rate = config->sample_rate;
    if (state->sample_rate == 0) state->sample_rate = 48000;
    
#if defined(__ANDROID__)
    // Create stream builder
    AAudioStreamBuilder* builder = nullptr;
    int32_t result = g_aaudio_lib.AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || !builder) {
        delete state;
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    state->stream_builder = builder;
    
    // Set format
    AAudioFormat aaudio_format = (state->format == AS_format::F32)
        ? AAUDIO_FORMAT_PCM_FLOAT
        : AAUDIO_FORMAT_PCM_I16;
    g_aaudio_lib.AAudioStreamBuilder_setFormat(builder, aaudio_format);
    
    // Set channels
    g_aaudio_lib.AAudioStreamBuilder_setChannelCount(builder, static_cast<int32_t>(state->channels));
    
    // Set sample rate (0 = optimal)
    g_aaudio_lib.AAudioStreamBuilder_setSampleRate(builder, static_cast<int32_t>(state->sample_rate));
    
    // Set direction
    AAudioDirection direction = (type == AS_device_type::CAPTURE)
        ? AAUDIO_DIRECTION_INPUT
        : AAUDIO_DIRECTION_OUTPUT;
    g_aaudio_lib.AAudioStreamBuilder_setDirection(builder, direction);
    
    // Set performance mode (low latency)
    g_aaudio_lib.AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    
    // Set sharing mode
    AAudioSharingMode sharing_mode = (type == AS_device_type::CAPTURE || 
                                      config->playback.share_mode == AS_share_mode::EXCLUSIVE)
        ? AAUDIO_SHARING_MODE_EXCLUSIVE
        : AAUDIO_SHARING_MODE_SHARED;
    g_aaudio_lib.AAudioStreamBuilder_setSharingMode(builder, sharing_mode);
    
    // Set callbacks
    g_aaudio_lib.AAudioStreamBuilder_setDataCallback(builder, aaudio_data_callback, state);
    g_aaudio_lib.AAudioStreamBuilder_setErrorCallback(builder, aaudio_error_callback, state);
    
    // Set buffer capacity
    uint32_t buffer_capacity = config->period_size_in_frames * 
                               (config->period_count ? config->period_count : 4);
    if (buffer_capacity > 0) {
        g_aaudio_lib.AAudioStreamBuilder_setBufferCapacityInFrames(
            builder, static_cast<int32_t>(buffer_capacity));
    }
#endif
    
    // Update device runtime info
    device->p_impl->actual_sample_rate = state->sample_rate;
    device->p_impl->actual_playback_format = state->format;
    device->p_impl->actual_playback_channels = state->channels;
    device->p_impl->actual_capture_format = state->format;
    device->p_impl->actual_capture_channels = state->channels;
    
    // Store state
    device->p_impl->backend_data = static_cast<void*>(state);
    
    return AS_result::SUCCESS;
}

static AS_result aaudio_on_device_uninit(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<aaudio_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::SUCCESS;
    
    // Stop if running
    if (state->running.load()) {
        aaudio_on_device_stop(device);
    }
    
#if defined(__ANDROID__)
    // Close stream
    if (state->stream && g_aaudio_lib.AAudioStream_close) {
        g_aaudio_lib.AAudioStream_close(state->stream);
    }
    
    // Delete builder
    if (state->stream_builder && g_aaudio_lib.AAudioStreamBuilder_delete) {
        g_aaudio_lib.AAudioStreamBuilder_delete(state->stream_builder);
    }
#endif
    
    delete state;
    device->p_impl->backend_data = nullptr;
    
    return AS_result::SUCCESS;
}

static AS_result aaudio_on_device_start(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<aaudio_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::INVALID_OPERATION;
    
    if (state->running.load()) {
        return AS_result::SUCCESS;
    }
    
#if defined(__ANDROID__)
    if (!g_aaudio_lib.loaded || !state->stream_builder) {
        return AS_result::INVALID_OPERATION;
    }
    
    // Open stream
    AAudioStream* stream = nullptr;
    int32_t result = g_aaudio_lib.AAudioStreamBuilder_openStream(
        state->stream_builder, &stream);
    if (result != AAUDIO_OK || !stream) {
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    state->stream = stream;
    
    // Get actual parameters
    state->format = (g_aaudio_lib.AAudioStream_getFormat(stream) == AAUDIO_FORMAT_PCM_FLOAT)
        ? AS_format::F32 : AS_format::S16;
    state->channels = static_cast<uint32_t>(g_aaudio_lib.AAudioStream_getChannelCount(stream));
    state->sample_rate = static_cast<uint32_t>(g_audio_lib.AAudioStream_getSampleRate(stream));
    state->burst_size = static_cast<uint32_t>(g_aaudio_lib.AAudioStream_getFramesPerBurst(stream));
    
    // Start stream
    result = g_aaudio_lib.AAudioStream_requestStart(stream);
    if (result != AAUDIO_OK) {
        g_aaudio_lib.AAudioStream_close(stream);
        state->stream = nullptr;
        return AS_result::ERROR;
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

static AS_result aaudio_on_device_stop(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<aaudio_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::SUCCESS;
    
    if (!state->running.load()) {
        return AS_result::SUCCESS;
    }
    
#if defined(__ANDROID__)
    if (state->stream && g_aaudio_lib.AAudioStream_requestStop) {
        g_aaudio_lib.AAudioStream_requestStop(state->stream);
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

static AS_result aaudio_on_device_read(
    AS_device* device,
    void* frames,
    uint32_t frame_count,
    uint32_t* frames_read
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_read) *frames_read = 0;
    return AS_result::NOT_IMPLEMENTED;  // AAudio is callback-based
}

static AS_result aaudio_on_device_write(
    AS_device* device,
    const void* frames,
    uint32_t frame_count,
    uint32_t* frames_written
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_written) *frames_written = 0;
    return AS_result::NOT_IMPLEMENTED;  // AAudio is callback-based
}

// ============================================================================
// Backend registration
// ============================================================================

static AS_bool32 aaudio_is_available() {
#if defined(__ANDROID__)
    AS_log* log = AS_log::default_log();
    return load_aaudio_library(log) ? AS_TRUE : AS_FALSE;
#else
    return AS_FALSE;
#endif
}

static AS_backend_callbacks g_aaudio_callbacks = {
    aaudio_on_context_init,
    aaudio_on_context_uninit,
    aaudio_on_context_enumerate_devices,
    aaudio_on_context_get_device_info,
    aaudio_on_device_init,
    aaudio_on_device_uninit,
    aaudio_on_device_start,
    aaudio_on_device_stop,
    aaudio_on_device_read,
    aaudio_on_device_write,
    nullptr,  // on_device_data_loop
    nullptr   // on_device_data_loop_wakeup
};

static AS_backend_info g_aaudio_info = {
    AS_backend::AAUDIO,
    "AAudio",
    aaudio_is_available,
    g_aaudio_callbacks
};

AS_result AS_register_aaudio_backend() {
    return AS_register_backend(&g_aaudio_info);
}

#if defined(__ANDROID__) && (defined(__GNUC__) || defined(__clang__))
__attribute__((constructor))
static void auto_register_aaudio_backend() {
    AS_register_aaudio_backend();
}
#endif

} // namespace arxsound