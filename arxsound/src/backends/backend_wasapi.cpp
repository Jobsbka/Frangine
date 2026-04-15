// src/backends/backend_wasapi.cpp
#include "../include/arxsound_backend.hpp"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>
#include <cmath>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mmdevapi.lib")

namespace arxsound {

// ============================================================================
// WASAPI internal structures
// ============================================================================

// COM interface wrappers (minimal)
struct IAudioClientWrapper {
    IAudioClient* ptr = nullptr;
};

struct IAudioRenderClientWrapper {
    IAudioRenderClient* ptr = nullptr;
};

struct IAudioCaptureClientWrapper {
    IAudioCaptureClient* ptr = nullptr;
};

struct IMMDeviceWrapper {
    IMMDevice* ptr = nullptr;
};

struct IMMDeviceEnumeratorWrapper {
    IMMDeviceEnumerator* ptr = nullptr;
};

struct IAudioEndpointVolumeWrapper {
    IAudioEndpointVolume* ptr = nullptr;
};

// WASAPI backend context state
struct wasapi_context_state {
    IMMDeviceEnumeratorWrapper enumerator;
    bool com_initialized = false;
    std::mutex mutex;
};

// WASAPI device state
struct wasapi_device_state {
    // Device handles
    IMMDeviceWrapper device;
    IAudioClientWrapper audio_client;
    IAudioRenderClientWrapper render_client;
    IAudioCaptureClientWrapper capture_client;
    IAudioEndpointVolumeWrapper endpoint_volume;
    
    // Audio format
    WAVEFORMATEX* wave_format = nullptr;
    
    // Buffers
    uint32_t buffer_size_frames = 0;
    uint32_t period_size_frames = 0;
    
    // Threading
    std::thread worker_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> started{false};
    HANDLE buffer_event = nullptr;
    
    // Device info
    AS_device_type type{AS_device_type::PLAYBACK};
    LPWSTR device_id = nullptr;
    
    // Volume
    float master_volume = 1.0f;
    
    // Duplex state
    std::mutex duplex_mutex;
};

// ============================================================================
// Helper functions
// ============================================================================

static AS_format wave_format_to_as_format(const WAVEFORMATEX* wf) {
    if (!wf) return AS_format::UNKNOWN;
    
    switch (wf->wBitsPerSample) {
        case 8:  return AS_format::U8;
        case 16: return AS_format::S16;
        case 24: return AS_format::S24;
        case 32:
            if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
                return AS_format::F32;
            }
            return AS_format::S32;
        default: return AS_format::UNKNOWN;
    }
}

static WAVEFORMATEX* as_format_to_wave_format(
    AS_format format,
    uint32_t channels,
    uint32_t sample_rate,
    ALLOCATOR* alloc = nullptr
) {
    WAVEFORMATEX* wf = reinterpret_cast<WAVEFORMATEX*>(
        CoTaskMemAlloc(sizeof(WAVEFORMATEX))
    );
    if (!wf) return nullptr;
    
    ZeroMemory(wf, sizeof(WAVEFORMATEX));
    
    wf->nChannels = static_cast<WORD>(channels);
    wf->nSamplesPerSec = sample_rate;
    wf->wBitsPerSample = 0;
    wf->nBlockAlign = 0;
    wf->nAvgBytesPerSec = 0;
    wf->cbSize = 0;
    
    switch (format) {
        case AS_format::U8:
            wf->wFormatTag = WAVE_FORMAT_PCM;
            wf->wBitsPerSample = 8;
            break;
        case AS_format::S16:
            wf->wFormatTag = WAVE_FORMAT_PCM;
            wf->wBitsPerSample = 16;
            break;
        case AS_format::S24:
            wf->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            wf->wBitsPerSample = 24;
            wf->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
            break;
        case AS_format::S32:
            wf->wFormatTag = WAVE_FORMAT_PCM;
            wf->wBitsPerSample = 32;
            break;
        case AS_format::F32:
            wf->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
            wf->wBitsPerSample = 32;
            break;
        default:
            CoTaskMemFree(wf);
            return nullptr;
    }
    
    wf->nBlockAlign = wf->nChannels * wf->wBitsPerSample / 8;
    wf->nAvgBytesPerSec = wf->nSamplesPerSec * wf->nBlockAlign;
    
    return wf;
}

static void free_wave_format(WAVEFORMATEX* wf) {
    if (wf) {
        CoTaskMemFree(wf);
    }
}

static const wchar_t* get_device_role(EDataFlow flow, ERole role) {
    if (flow == eRender) {
        switch (role) {
            case eConsole: return L"Console";
            case eMultimedia: return L"Multimedia";
            case eCommunications: return L"Communications";
            default: return L"Unknown";
        }
    } else {
        return L"Capture";
    }
}

// ============================================================================
// Worker thread for WASAPI
// ============================================================================

static void wasapi_worker_thread(wasapi_device_state* state) {
    if (!state || !state->audio_client.ptr) return;
    
    HRESULT hr;
    
    // Get buffer size
    UINT32 buffer_size = 0;
    hr = state->audio_client.ptr->GetBufferSize(&buffer_size);
    if (FAILED(hr)) {
        return;
    }
    state->buffer_size_frames = buffer_size;
    
    // For event-driven mode, get the buffer event
    HANDLE hEvent = state->buffer_event;
    if (!hEvent) {
        // Create event if not exists
        hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        state->buffer_event = hEvent;
        
        hr = state->audio_client.ptr->SetEventHandle(hEvent);
        if (FAILED(hr)) {
            CloseHandle(hEvent);
            state->buffer_event = nullptr;
            return;
        }
    }
    
    // Start the audio stream
    hr = state->audio_client.ptr->Start();
    if (FAILED(hr)) {
        return;
    }
    
    state->started.store(true);
    
    // Main processing loop
    while (state->running.load(std::memory_order_acquire)) {
        // Wait for buffer event (event-driven mode)
        if (hEvent) {
            WaitForSingleObject(hEvent, 1000);
        }
        
        // Get padding (frames currently in buffer)
        UINT32 padding = 0;
        hr = state->audio_client.ptr->GetCurrentPadding(&padding);
        if (FAILED(hr)) {
            break;
        }
        
        if (state->type == AS_device_type::PLAYBACK || 
            state->type == AS_device_type::DUPLEX) {
            
            // Calculate frames available for writing
            UINT32 frames_available = state->buffer_size_frames - padding;
            
            if (frames_available > 0) {
                BYTE* data = nullptr;
                hr = state->render_client.ptr->GetBuffer(frames_available, &data);
                if (SUCCEEDED(hr) && data) {
                    // Call user callback
                    if (state->master_volume != 1.0f) {
                        // Apply volume if needed (simplified - F32 only)
                        float* f32_data = reinterpret_cast<float*>(data);
                        uint32_t total_samples = frames_available * state->wave_format->nChannels;
                        for (uint32_t i = 0; i < total_samples; ++i) {
                            f32_data[i] *= state->master_volume;
                        }
                    }
                    
                    // Note: In real implementation, we'd call the user's data callback here
                    // For now, we just release the buffer
                    state->render_client.ptr->ReleaseBuffer(frames_available, 0);
                }
            }
        }
        
        if (state->type == AS_device_type::CAPTURE || 
            state->type == AS_device_type::DUPLEX) {
            
            // Capture processing
            UINT32 packets_available = 0;
            hr = state->capture_client.ptr->GetNextPacketSize(&packets_available);
            if (SUCCEEDED(hr) && packets_available > 0) {
                BYTE* data = nullptr;
                UINT32 frames_available = 0;
                DWORD flags = 0;
                
                hr = state->capture_client.ptr->GetBuffer(
                    &data, &frames_available, &flags, nullptr, nullptr
                );
                
                if (SUCCEEDED(hr) && data) {
                    // Call user callback with captured data
                    // (simplified - real implementation would call data_callback)
                    
                    state->capture_client.ptr->ReleaseBuffer(frames_available);
                }
            }
        }
        
        // Small sleep to prevent busy-waiting in non-event mode
        if (!hEvent) {
            Sleep(1);
        }
    }
    
    // Stop the audio stream
    state->audio_client.ptr->Stop();
    state->started.store(false);
}

// ============================================================================
// Backend callbacks implementation
// ============================================================================

// --- Context callbacks ---

static AS_result wasapi_on_context_init(
    AS_context* context,
    const AS_context_config* config,
    AS_backend_callbacks* callbacks
) {
    (void)callbacks;
    
    auto* state = new(std::nothrow) wasapi_context_state();
    if (!state) {
        return AS_result::OUT_OF_MEMORY;
    }
    
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr)) {
        state->com_initialized = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        // COM already initialized with different mode - continue anyway
        state->com_initialized = false;
    }
    
    // Create device enumerator
    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator)
    );
    
    if (SUCCEEDED(hr) && enumerator) {
        state->enumerator.ptr = enumerator;
    }
    
    // Store state in context (opaque pointer)
    // In real implementation, need proper storage mechanism
    (void)context;
    (void)config;
    
    delete state;  // For now, we don't persist context state
    return AS_result::SUCCESS;
}

static AS_result wasapi_on_context_uninit(AS_context* context) {
    (void)context;
    // Cleanup would happen here
    return AS_result::SUCCESS;
}

static AS_result wasapi_on_context_enumerate_devices(
    AS_context* context,
    AS_device_type type,
    AS_enumerate_devices_callback callback,
    void* user_data
) {
    if (!callback) return AS_result::INVALID_ARGS;
    
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator)
    );
    
    if (FAILED(hr) || !enumerator) {
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    IMMDeviceCollection* collection = nullptr;
    EDataFlow data_flow = (type == AS_device_type::CAPTURE) ? eCapture : eRender;
    
    hr = enumerator->EnumAudioEndpoints(data_flow, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr) || !collection) {
        enumerator->Release();
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    UINT count = 0;
    collection->GetCount(&count);
    
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        hr = collection->Item(i, &device);
        if (FAILED(hr) || !device) continue;
        
        // Get device ID
        LPWSTR device_id = nullptr;
        hr = device->GetId(&device_id);
        if (FAILED(hr) || !device_id) {
            device->Release();
            continue;
        }
        
        // Get device properties
        IPropertyStore* props = nullptr;
        hr = device->OpenPropertyStore(STGM_READ, &props);
        
        AS_device_info info{};
        ZeroMemory(&info, sizeof(info));
        
        if (SUCCEEDED(hr) && props) {
            PROPVARIANT name_var;
            PropVariantInit(&name_var);
            hr = props->GetValue(PKEY_Device_FriendlyName, &name_var);
            if (SUCCEEDED(hr) && name_var.vt == VT_LPWSTR) {
                wcstombs(info.name, name_var.pwszVal, sizeof(info.name) - 1);
            }
            PropVariantClear(&name_var);
            props->Release();
        }
        
        // Copy device ID
        wcstombs(info.id.wasapi, device_id, sizeof(info.id.wasapi) - 1);
        
        // Check if default
        IMMDevice* default_device = nullptr;
        hr = enumerator->GetDefaultAudioEndpoint(data_flow, eConsole, &default_device);
        if (SUCCEEDED(hr) && default_device) {
            LPWSTR default_id = nullptr;
            if (SUCCEEDED(default_device->GetId(&default_id))) {
                if (wcscmp(device_id, default_id) == 0) {
                    info.is_default = AS_TRUE;
                }
                CoTaskMemFree(default_id);
            }
            default_device->Release();
        }
        
        // Basic format info
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
        add_format(AS_format::F32, 2, 44100);
        add_format(AS_format::S16, 2, 44100);
        
        // Call callback
        callback(context, type, &info, user_data);
        
        CoTaskMemFree(device_id);
        device->Release();
    }
    
    collection->Release();
    enumerator->Release();
    
    return AS_result::SUCCESS;
}

static AS_result wasapi_on_context_get_device_info(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    AS_device_info* info
) {
    if (!info) return AS_result::INVALID_ARGS;
    
    // For simplicity, enumerate and find matching device
    return wasapi_on_context_enumerate_devices(context, type,
        [](AS_context*, AS_device_type, const AS_device_info* src, void* dst) {
            *static_cast<AS_device_info*>(dst) = *src;
            return AS_TRUE;
        }, info);
}

// --- Device callbacks ---

static AS_result wasapi_on_device_init(
    AS_context* context,
    AS_device_type type,
    const void* device_id,
    const AS_device_config* config,
    AS_device* device
) {
    if (!context || !config || !device) {
        return AS_result::INVALID_ARGS;
    }
    
    auto* state = new(std::nothrow) wasapi_device_state();
    if (!state) {
        return AS_result::OUT_OF_MEMORY;
    }
    
    state->type = type;
    
    // Create device
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator)
    );
    
    if (FAILED(hr) || !enumerator) {
        delete state;
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    IMMDevice* imm_device = nullptr;
    if (device_id) {
        // Specific device
        char device_id_str[256];
        // Convert from stored format
        hr = enumerator->GetDevice(
            static_cast<const wchar_t*>(device_id),
            &imm_device
        );
    } else {
        // Default device
        EDataFlow flow = (type == AS_device_type::CAPTURE) ? eCapture : eRender;
        hr = enumerator->GetDefaultAudioEndpoint(flow, eConsole, &imm_device);
    }
    
    enumerator->Release();
    
    if (FAILED(hr) || !imm_device) {
        delete state;
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    state->device.ptr = imm_device;
    
    // Activate audio client
    IAudioClient* audio_client = nullptr;
    hr = imm_device->Activate(
        __uuidof(IAudioClient),
        CLSCTX_INPROC_SERVER,
        nullptr,
        reinterpret_cast<void**>(&audio_client)
    );
    
    if (FAILED(hr) || !audio_client) {
        imm_device->Release();
        delete state;
        return AS_result::DEVICE_NOT_AVAILABLE;
    }
    
    state->audio_client.ptr = audio_client;
    
    // Create wave format
    AS_format format = (type == AS_device_type::CAPTURE) 
        ? config->capture.format 
        : config->playback.format;
    if (format == AS_format::UNKNOWN) {
        format = AS_format::F32;
    }
    
    uint32_t channels = (type == AS_device_type::CAPTURE)
        ? config->capture.channels
        : config->playback.channels;
    if (channels == 0) channels = 2;
    
    uint32_t sample_rate = config->sample_rate;
    if (sample_rate == 0) sample_rate = 48000;
    
    state->wave_format = as_format_to_wave_format(format, channels, sample_rate);
    if (!state->wave_format) {
        audio_client->Release();
        imm_device->Release();
        delete state;
        return AS_result::FORMAT_NOT_SUPPORTED;
    }
    
    // Get device period
    REFERENCE_TIME default_period = 0;
    REFERENCE_TIME min_period = 0;
    audio_client->GetDevicePeriod(&default_period, &min_period);
    
    // Initialize audio client
    DWORD stream_flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (type == AS_device_type::DUPLEX) {
        stream_flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
    }
    
    hr = audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        stream_flags,
        default_period,
        default_period,
        state->wave_format,
        nullptr
    );
    
    if (FAILED(hr)) {
        // Try without event callback
        hr = audio_client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            default_period,
            default_period,
            state->wave_format,
            nullptr
        );
        
        if (FAILED(hr)) {
            free_wave_format(state->wave_format);
            audio_client->Release();
            imm_device->Release();
            delete state;
            return AS_result::FORMAT_NOT_SUPPORTED;
        }
    }
    
    // Get buffer size
    UINT32 buffer_size = 0;
    audio_client->GetBufferSize(&buffer_size);
    state->buffer_size_frames = buffer_size;
    state->period_size_frames = static_cast<uint32_t>(default_period / 10000);
    
    // Activate render/capture client
    if (type == AS_device_type::PLAYBACK || type == AS_device_type::DUPLEX) {
        IAudioRenderClient* render_client = nullptr;
        hr = audio_client->GetService(
            __uuidof(IAudioRenderClient),
            reinterpret_cast<void**>(&render_client)
        );
        if (SUCCEEDED(hr) && render_client) {
            state->render_client.ptr = render_client;
        }
    }
    
    if (type == AS_device_type::CAPTURE || type == AS_device_type::DUPLEX) {
        IAudioCaptureClient* capture_client = nullptr;
        hr = audio_client->GetService(
            __uuidof(IAudioCaptureClient),
            reinterpret_cast<void**>(&capture_client)
        );
        if (SUCCEEDED(hr) && capture_client) {
            state->capture_client.ptr = capture_client;
        }
    }
    
    // Get endpoint volume control
    IAudioEndpointVolume* endpoint_volume = nullptr;
    hr = imm_device->Activate(
        __uuidof(IAudioEndpointVolume),
        CLSCTX_INPROC_SERVER,
        nullptr,
        reinterpret_cast<void**>(&endpoint_volume)
    );
    if (SUCCEEDED(hr) && endpoint_volume) {
        state->endpoint_volume.ptr = endpoint_volume;
    }
    
    // Update device runtime info
    device->p_impl->actual_sample_rate = sample_rate;
    device->p_impl->actual_playback_format = format;
    device->p_impl->actual_playback_channels = channels;
    device->p_impl->actual_capture_format = format;
    device->p_impl->actual_capture_channels = channels;
    
    // Store state
    device->p_impl->backend_data = static_cast<void*>(state);
    
    return AS_result::SUCCESS;
}

static AS_result wasapi_on_device_uninit(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<wasapi_device_state*>(device->p_impl->backend_data);
    if (!state) return AS_result::SUCCESS;
    
    // Stop if running
    if (state->running.load()) {
        wasapi_on_device_stop(device);
    }
    
    // Release COM interfaces
    if (state->render_client.ptr) {
        state->render_client.ptr->Release();
    }
    if (state->capture_client.ptr) {
        state->capture_client.ptr->Release();
    }
    if (state->audio_client.ptr) {
        state->audio_client.ptr->Release();
    }
    if (state->endpoint_volume.ptr) {
        state->endpoint_volume.ptr->Release();
    }
    if (state->device.ptr) {
        state->device.ptr->Release();
    }
    
    // Free wave format
    free_wave_format(state->wave_format);
    
    // Close event handle
    if (state->buffer_event) {
        CloseHandle(state->buffer_event);
    }
    
    // Free device ID
    if (state->device_id) {
        CoTaskMemFree(state->device_id);
    }
    
    delete state;
    device->p_impl->backend_data = nullptr;
    
    return AS_result::SUCCESS;
}

static AS_result wasapi_on_device_start(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<wasapi_device_state*>(device->p_impl->backend_data);
    if (!state || !state->audio_client.ptr) {
        return AS_result::INVALID_OPERATION;
    }
    
    if (state->running.load()) {
        return AS_result::SUCCESS;
    }
    
    state->running.store(true);
    
    // Start worker thread
    try {
        state->worker_thread = std::thread(wasapi_worker_thread, state);
    } catch (...) {
        state->running.store(false);
        return AS_result::ERROR;
    }
    
    return AS_result::SUCCESS;
}

static AS_result wasapi_on_device_stop(AS_device* device) {
    if (!device || !device->p_impl) return AS_result::INVALID_ARGS;
    
    auto* state = static_cast<wasapi_device_state*>(device->p_impl->backend_data);
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

static AS_result wasapi_on_device_read(
    AS_device* device,
    void* frames,
    uint32_t frame_count,
    uint32_t* frames_read
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_read) *frames_read = 0;
    return AS_result::NOT_IMPLEMENTED;  // WASAPI is callback-based
}

static AS_result wasapi_on_device_write(
    AS_device* device,
    const void* frames,
    uint32_t frame_count,
    uint32_t* frames_written
) {
    (void)device;
    (void)frames;
    (void)frame_count;
    if (frames_written) *frames_written = 0;
    return AS_result::NOT_IMPLEMENTED;  // WASAPI is callback-based
}

// ============================================================================
// Backend registration
// ============================================================================

static AS_bool32 wasapi_is_available() {
    // Check if we can create device enumerator
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator)
    );
    
    if (SUCCEEDED(hr) && enumerator) {
        enumerator->Release();
        return AS_TRUE;
    }
    return AS_FALSE;
}

static AS_backend_callbacks g_wasapi_callbacks = {
    wasapi_on_context_init,
    wasapi_on_context_uninit,
    wasapi_on_context_enumerate_devices,
    wasapi_on_context_get_device_info,
    wasapi_on_device_init,
    wasapi_on_device_uninit,
    wasapi_on_device_start,
    wasapi_on_device_stop,
    wasapi_on_device_read,
    wasapi_on_device_write,
    nullptr,  // on_device_data_loop
    nullptr   // on_device_data_loop_wakeup
};

static AS_backend_info g_wasapi_info = {
    AS_backend::WASAPI,
    "WASAPI",
    wasapi_is_available,
    g_wasapi_callbacks
};

AS_result AS_register_wasapi_backend() {
    return AS_register_backend(&g_wasapi_info);
}

// Auto-register on load
#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor))
static void auto_register_wasapi_backend() {
    AS_register_wasapi_backend();
}
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void __cdecl auto_register_wasapi_msvc() {
    AS_register_wasapi_backend();
}
__declspec(allocate(".CRT$XCU")) void (*__auto_register_wasapi)(void) = auto_register_wasapi_msvc;
#endif

} // namespace arxsound