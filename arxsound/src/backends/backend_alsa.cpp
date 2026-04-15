// src/backends/backend_alsa.cpp
#include "../include/arxsound_backend.hpp"
#include <alsa/asoundlib.h>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>
#include <cmath>

namespace arxsound {

// ============================================================================
// ALSA internal structures
// ============================================================================

struct alsa_context_state {
    snd_ctl_t* ctl = nullptr;
    std::mutex mutex;
};

struct alsa_device_state {
    // PCM handles
    snd_pcm_t* pcm_handle = nullptr;
    
    // Audio format
    snd_pcm_format_t alsa_format;
    uint32_t channels = 2;
    uint32_t sample_rate = 48000;
    
    // Buffer info
    snd_pcm_uframes_t buffer_size = 0;
    snd_pcm_uframes_t period_size = 0;
    
    // Threading
    std::thread worker_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> started{false};
    
    // Device info
    AS_device_type type{AS_device_type::PLAYBACK};
    char device_name[256];
    
    // Callbacks
    AS_device_data_proc data_callback = nullptr;
    void* user_data = nullptr;
    
    // Volume
    float master_volume = 1.0f;
    
    // Poll descriptors
    struct pollfd* poll_fds = nullptr;
    unsigned int poll_fd_count = 0;
};

// ============================================================================
// Helper functions
// ============================================================================

static AS_format alsa_format_to_as_format(snd_pcm_format_t fmt) {
    switch (fmt) {
        case SND_PCM_FORMAT_U8:      return AS_format::U8;
        case SND_PCM_FORMAT_S16_LE:  return AS_format::S16;
        case SND_PCM_FORMAT_S24_LE:  return AS_format::S24;
        case SND_PCM_FORMAT_S32_LE:  return AS_format::S32;
        case SND_PCM_FORMAT_FLOAT_LE: return AS_format::F32;
        default: return AS_format::UNKNOWN;
    }
}

static snd_pcm_format_t as_format_to_alsa_format(AS_format format) {
    switch (format) {
        case AS_format::U8:      return SND_PCM_FORMAT_U8;
        case AS_format::S16:     return SND_PCM_FORMAT_S16_LE;
        case AS_format::S24:     return SND_PCM_FORMAT_S24_LE;
        case AS_format::S32:     return SND_PCM_FORMAT_S32_LE;
        case AS_format::F32:     return SND_PCM_FORMAT_FLOAT_LE;
        default: return SND_PCM_FORMAT_UNKNOWN;
    }
}

static const char* get_alsa_device_name(AS_device_type type) {
    return (type == AS_device_type::CAPTURE) ? "default" : "default";
}

static snd_pcm_stream_t get_alsa_stream_type(AS_device_type type) {
    return (type == AS_device_type::CAPTURE) 
        ? SND_PCM_STREAM_CAPTURE 
        : SND_PCM_STREAM_PLAYBACK;
}

// ============================================================================
// Worker thread for ALSA
// ============================================================================

static void alsa_worker_thread(alsa_device_state* state) {
    if (!state || !state->pcm_handle) return;
    
    snd_pcm_sframes_t frames_to_write = static_cast<snd_pcm_sframes_t>(state->period_size);
    std::vector<uint8_t> buffer;
    
    // Calculate buffer size
    size_t bytes_per_frame = 0;
    switch (state->alsa_format) {
        case SND_PCM_FORMAT_U8: bytes_per_frame = 1; break;
        case SND_PCM_FORMAT_S16_LE: bytes_per_frame = 2; break;
        case SND_PCM_FORMAT_S24_LE: bytes_per_frame = 3; break;
        case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_FLOAT_LE: bytes_per_frame = 4; break;
        default: bytes_per_frame = 4;
    }
    bytes_per_frame *= state->channels;
    
    buffer.resize(state->period_size * bytes_per_frame);
    
    // Prepare PCM
    snd_pcm_prepare(state->pcm_handle);
    state->started.store(true);
    
    // Main processing loop
    while (state->running.load(std::memory_order_acquire)) {
        // Wait for poll events
        if (state->poll_fds && state->poll_fd_count > 0) {
            int err = poll(state->poll_fds, state->poll_fd_count, 1000);
            if (err < 0) {
                continue;
            }
            
            // Check for errors
            unsigned short revents = 0;
            snd_pcm_poll_descriptors_revents(
                state->pcm_handle, 
                state->poll_fds, 
                state->poll_fd_count, 
                &revents
            );
            
            if (revents & (POLLERR | POLLNVAL)) {
                snd_pcm_prepare(state->pcm_handle);
                continue;
            }
            
            if (!(revents & POLLOUT)) {
                continue;
            }
        }
        
        if (state->type == AS_device_type::PLAYBACK || 
            state->type == AS_device_type::DUPLEX) {
            
            // Fill buffer with audio data
            if (state->data_callback) {
                state->data_callback(
                    nullptr,  // device
                    buffer.data(),
                    nullptr,  // input
                    static_cast<uint32_t>(state->period_size)
                );
            } else {
                // Silence
                memset(buffer.data(), 0, buffer.size());
            }
            
            // Write to device
            snd_pcm_sframes_t written = snd_pcm_writei(
                state->pcm_handle, 
                buffer.data(), 
                frames_to_write
            );
            
            if (written < 0) {
                if (written == -EPIPE) {
                    // Underrun
                    snd_pcm_prepare(state->pcm_handle);
                } else if (written == -EAGAIN) {
                    continue;
                }
            }
        }
        
        if (state->type == AS_device_type::CAPTURE || 
            state->type == AS_device_type::DUPLEX) {
            
            // Read from device
            snd_pcm_sframes_t available = snd_pcm_avail(state->pcm_handle);
            if (available > 0) {
                snd_pcm_sframes_t read = snd_pcm_readi(
                    state->pcm_handle,
                    buffer.data(),
                    static_cast<snd_pcm_uframes_t>(available)
                );
                
                if (read < 0) {
                    if (read == -EPIPE) {
                        // Overrun
                        snd_pcm_prepare(state->pcm_handle);
                    }
                } else if (read > 0 && state->data_callback) {
                    state->data_callback(
                        nullptr,
                        nullptr,  // output
                        buffer.data(),
                        static_cast<uint32_t>(read)
                    );
                }
            }
        }
    }
    
    snd_pcm_drop(state->pcm_handle);
    state->started.store(false);
}

// ============================================================================
// Backend callbacks implementation
// ============================================================================

// --- Context callbacks ---

static AS_result alsa_on_context_init(
    AS_context* context,
    const AS_context_config* config,
    AS_backend_callbacks* callbacks
) {
    (void)context;
    (void)config;
    (void)callbacks;
    
    // ALSA doesn't require special context initialization
    return AS_result::SUCCESS;
}

static AS_result alsa_on_context_uninit(AS_context* context) {
    (void)context;
    return AS_result::SUCCESS;
}

static AS_result alsa_on_context_enumerate_devices(
    AS_context* context,
    AS_device_type type,
    AS_enumerate_devices_callback callback,
    void* user_data
) {
    if (!callback) return AS_result::INVALID_ARGS;
    
    void** hints = nullptr;
    snd_config_t* config = nullptr;
    
    // Get default config
    if (snd_config_update_free_global() < 0) {
        snd_config_update_free_global();
    }
    
    const char* device_name = get_alsa_device_name(type);
    snd_pcm_stream_t stream = get_alsa_stream_type(type);
    
    // Enumerate devices
    AS_device_info info{};
    ZeroMemory(&info, sizeof(info));
    
    // Default device
    snprintf(info.name, sizeof(info.name), "ALSA %s Device", 
             (type == AS_device_type::PLAYBACK) ? "Playback" : "Capture");
    strcpy(info.id.alsa, device_name);
    info.is_default = AS_TRUE;
    info.min_sample_rate = 8000;
    info.max_sample_rate = 192000;
    info.min_channels = 1;
    info.max_channels = 8;
    info.supports_shared = AS_TRUE;
    info.supports_exclusive = AS_FALSE;
    
    // Native formats
    info.native_format_count = 0;
    auto add_format = [&](AS_format fmt, uint32_t ch, uint32_t sr) {
        if (info.native_format_count < AS_device_info::MAX_NATIVE_FORMATS) {
            info.native_formats[info.native_format_count++] = {fmt, ch, sr};
        }
    };
    add_format(AS_format::S16, 2, 48000);
    add_format(AS_format::S16, 2, 44100);
    add_format(AS_format::F32, 2, 48000);
    
    callback(context, type, &info, user_data);
    
    // Try to enumerate card devices
    int card = -1;
    while (snd_card_next(&card) >= 0 && card >= 0) {
        char card_name[256];
        snd_ctl_card_info_t* card_info;
        snd_ctl_card_info_alloca(&card_info);
        
        char ctl_name[32];
        snprintf(ctl_name, sizeof(ctl_name), "hw:%d", card);
        
        snd_ctl_t* ctl;
        if (snd_ctl_open(&ctl, ctl_name, 0) >= 0) {
            if (snd_ctl_card_info(ctl, card_info) >= 0) {
                AS_device_info card_info_struct{};
                ZeroMemory(&card_info_struct, sizeof(card_info_struct));
                
                snprintf(card_info_struct.name, sizeof(card_info_struct.name),
                        "ALSA %s (hw:%d)", 
                        snd_ctl_card_info_get_name(card_info), card);
                snprintf(card_info_struct.id.alsa, sizeof(card_info_struct.id.alsa),
                        "hw:%d", card);
                card_info_struct.is_default = AS_FALSE;
                card_info_struct.min_sample_rate = 8000;
                card_info_struct.max_sample_rate = 192000;
                card_info_struct.min_channels = 1;
                card_info_struct.max_channels = 8;
                card_info_struct.supports_shared = AS_TRUE;
                card_info_struct.supports_exclusive = AS_TRUE;
                
                callback(context, type, &card_info_struct, user_data);
            }
            snd_ctl_close(ctl);
        }
    }
    
    return AS_result::SUCCESS;
}

static AS_result alsa_on_context_get_device_info(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    AS_device_info* info
) {
    if (!info) return AS_result::INVALID_ARGS;
    
    // For simplicity, return default info
    return alsa_on_context_enumerate_devices(context, type,
        [](AS_context*, AS_device_type, const AS_device_info* src, void* dst) {
            *static_cast<AS_device_info*>(dst) = *src;
            return AS_TRUE;
        }, info);
}

// --- Device callbacks ---

static AS_result alsa_on_device_init(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    const AS_device_config* config,
    AS_device* device
) {
    if (!context || !config || !device) {
        return AS_result::INVALID_ARGS;
    }
    
    auto* state = new(std::nothrow) alsa_device_state();
    if (!state) {
        return AS_result::OUT_OF_MEMORY;
    }
    
    state->type = type;
    state->data_callback = config->data_callback;
    state->user_data = config->user_data;
    
    // Determine device name
    const char* device_name = "default";
    if (device_id) {
        // Use provided device ID
        device_name = static_cast<const char*>(device_id);
    }
    strncpy(state->device_name, device_name, sizeof(state->device_name) - 1);
    
    // Determine format and parameters
    AS_format format = (type == AS_device_type::CAPTURE)
        ? config->capture.format
        : config->playback.format;
    if (format == AS_format::UNKNOWN) {
        format = AS_format::S16;
    }
    
    uint32_t channels = (type == AS_device_type::CAPTURE)
        ? config->capture.channels
        : config->playback.channels;
    if (channels == 0) channels = 2;
    
    uint32_t sample_rate = config->sample_rate;
    if (sample_rate == 0) sample_rate = 48000;
    
    state->alsa_format = as_format_to_alsa_format(format);
    state->channels = channels;
    state->sample_rate = sample_rate;
    
    // Open PCM device
    snd_pcm_stream_t stream = get_alsa_stream_type(type);
    int err = snd_pcm_open(
        &state->pcm_handle,
        state->device_name,
        stream,
        SND_PCM_NONBLOCK
    );
    
    if (err < 0 || !state->pcm_handle) {
        delete state;
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    // Set hardware parameters
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    
    err = snd_pcm_hw_params_any(state->pcm_handle, hw_params);
    if (err < 0) {
        snd_pcm_close(state->pcm_handle);
        delete state;
        return AS_result::ERROR;
    }
    
    err = snd_pcm_hw_params_set_access(
        state->pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED
    );
    if (err < 0) {
        snd_pcm_close(state->pcm_handle);
        delete state;
        return AS_result::ERROR;
    }
    
    err = snd_pcm_hw_params_set_format(state->pcm_handle, hw_params, state->alsa_format);
    if (err < 0) {
        // Try fallback format
        state->alsa_format = SND_PCM_FORMAT_S16_LE;
        err = snd_pcm_hw_params_set_format(state->pcm_handle, hw_params, state->alsa_format);
        if (err < 0) {
            snd_pcm_close(state->pcm_handle);
            delete state;
            return AS_result::FORMAT_NOT_SUPPORTED;
        }
    }
    
    err = snd_pcm_hw_params_set_channels(state->pcm_handle, hw_params, channels);
    if (err < 0) {
        snd_pcm_close(state->pcm_handle);
        delete state;
        return AS_result::CHANNELS_NOT_SUPPORTED;
    }
    
    unsigned int rate = sample_rate;
    err = snd_pcm_hw_params_set_rate_near(state->pcm_handle, hw_params, &rate, 0);
    if (err < 0) {
        snd_pcm_close(state->pcm_handle);
        delete state;
        return AS_result::SAMPLE_RATE_NOT_SUPPORTED;
    }
    state->sample_rate = rate;
    
    // Set buffer size
    snd_pcm_uframes_t buffer_size = config->period_size_in_frames * 
                                    (config->period_count ? config->period_count : 4);
    if (buffer_size == 0) {
        buffer_size = 2048;  // Default
    }
    
    err = snd_pcm_hw_params_set_buffer_size_near(state->pcm_handle, hw_params, &buffer_size);
    if (err < 0) {
        snd_pcm_close(state->pcm_handle);
        delete state;
        return AS_result::ERROR;
    }
    state->buffer_size = buffer_size;
    
    // Set period size
    snd_pcm_uframes_t period_size = config->period_size_in_frames;
    if (period_size == 0) {
        period_size = 512;  // Default
    }
    
    err = snd_pcm_hw_params_set_period_size_near(state->pcm_handle, hw_params, &period_size, 0);
    if (err < 0) {
        snd_pcm_close(state->pcm_handle);
        delete state;
        return AS_result::ERROR;
    }
    state->period_size = period_size;
    
    // Apply hardware parameters
    err = snd_pcm_hw_params(state->pcm_handle, hw_params);
    if (err < 0) {
        snd_pcm_close(state->pcm_handle);
        delete state;
        return AS_result::ERROR;
    }
    
    // Get poll descriptors
    state->poll_fd_count = snd_pcm_poll_descriptors_count(state->pcm_handle);
    if (state->poll_fd_count > 0) {
        state->poll_fds = new(std::nothrow) struct pollfd[state->poll_fd_count];
        if (state->poll_fds) {
            snd_pcm_poll_descriptors(state->pcm_handle, state->poll_fds, state->poll_fd_count);
        }
    }
    
    // Update device runtime info
    device->p_impl->actual_sample_rate = state->sample_rate;
    device->p_impl->actual_playback_format = alsa_format_to_as_format(state->alsa_format);
    device->p_impl->actual_playback_channels = state->channels;
    device->p_impl->actual_capture_format = alsa_format_to_as_format(state->alsa_format);
    device->p_impl->actual_capture_channels = state->channels;
    
    // Store state
    device->p_impl->backend_data = static_cast<void*>(state);
    
    return AS_result::SUCCESS;
}

static AS_result alsa_on_device_uninit(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<alsa_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::SUCCESS;
    
    // Stop if running
    if (state->running.load()) {
        alsa_on_device_stop(device);
    }
    
    // Close PCM handle
    if (state->pcm_handle) {
        snd_pcm_close(state->pcm_handle);
    }
    
    // Free poll fds
    if (state->poll_fds) {
        delete[] state->poll_fds;
    }
    
    delete state;
    device->p_impl->backend_data = nullptr;
    
    return AS_result::SUCCESS;
}

static AS_result alsa_on_device_start(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<alsa_device_state*>(device->p_impl->backend_data);
    if (!state || !state->pcm_handle) {
        return AS_result::INVALID_OPERATION;
    }
    
    if (state->running.load()) {
        return AS_result::SUCCESS;
    }
    
    state->running.store(true);
    
    // Start worker thread
    try {
        state->worker_thread = std::thread(alsa_worker_thread, state);
    } catch (...) {
        state->running.store(false);
        return AS_result::ERROR;
    }
    
    return AS_result::SUCCESS;
}

static AS_result alsa_on_device_stop(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<alsa_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::SUCCESS;
    
    if (!state->running.load()) {
        return AS_result::SUCCESS;
    }
    
    state->running.store(false);
    
    // Wait for worker thread
    if (state->worker_thread.joinable()) {
        state->worker_thread.join();
    }
    
    return AS_result::SUCCESS;
}

static AS_result alsa_on_device_read(
    AS_device* device,
    void* frames,
    uint32_t frame_count,
    uint32_t* frames_read
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_read) *frames_read = 0;
    return AS_result::NOT_IMPLEMENTED;  // ALSA is callback-based in our implementation
}

static AS_result alsa_on_device_write(
    AS_device* device,
    const void* frames,
    uint32_t frame_count,
    uint32_t* frames_written
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_written) *frames_written = 0;
    return AS_result::NOT_IMPLEMENTED;  // ALSA is callback-based in our implementation
}

// ============================================================================
// Backend registration
// ============================================================================

static AS_bool32 alsa_is_available() {
    // Check if ALSA is available by trying to open default device
    snd_pcm_t* pcm = nullptr;
    int err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (err >= 0 && pcm) {
        snd_pcm_close(pcm);
        return AS_TRUE;
    }
    return AS_FALSE;
}

static AS_backend_callbacks g_alsa_callbacks = {
    alsa_on_context_init,
    alsa_on_context_uninit,
    alsa_on_context_enumerate_devices,
    alsa_on_context_get_device_info,
    alsa_on_device_init,
    alsa_on_device_uninit,
    alsa_on_device_start,
    alsa_on_device_stop,
    alsa_on_device_read,
    alsa_on_device_write,
    nullptr,  // on_device_data_loop
    nullptr   // on_device_data_loop_wakeup
};

static AS_backend_info g_alsa_info = {
    AS_backend::ALSA,
    "ALSA",
    alsa_is_available,
    g_alsa_callbacks
};

AS_result AS_register_alsa_backend() {
    return AS_register_backend(&g_alsa_info);
}

// Auto-register on load
#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void auto_register_alsa_backend() {
    AS_register_alsa_backend();
}
#endif

} // namespace arxsound